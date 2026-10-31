#!/usr/bin/env python3
"""Mod downloads for an OpenSWBF2 dedicated server: Source's FastDL, served by the server itself.

  modserver.py serve            the HTTP service; entrypoint.sh starts it next to the game
  modserver.py apply-pending    puts uploaded mods in place; entrypoint.sh runs it before each game start

Players' launchers read GET /mods.json (every mod folder, with each file's size and SHA-256) and fetch
what they lack from GET /mods/<folder>/<path>, which takes ranges, so downloads resume. Reading needs
the join password when the server has one (or the admin password). Admins upload mods the server
lacks: they wait in /config/addons/.pending and load when the game restarts, at once or when nobody
is on. Programs are never listed, served or taken, and neither is the unofficial 1.3 patch.
"""
import argparse
import hashlib
import hmac
import http.server
import json
import os
import re
import shutil
import signal
import sys
import threading
import time
import urllib.parse
import uuid
from collections import defaultdict, deque

PROGRAMS = (".exe", ".dll", ".bat", ".cmd", ".com", ".scr", ".msi", ".ps1", ".vbs")
PATCH_ADDON = "aaa-v1.3patch"
ADDME = "addme.script"
FOLDER_NAME = re.compile(r"^[A-Za-z0-9_][A-Za-z0-9_.-]{0,63}$")
# Names a Windows launcher can write: no reserved characters, no trailing dot or space, no devices
BAD_CHARS = re.compile(r'[<>:"/\\|?*\x00-\x1f]')
DEVICES = re.compile(r"^(con|prn|aux|nul|com[0-9]|lpt[0-9])(\..*)?$", re.IGNORECASE)
SHA256 = re.compile(r"^[0-9a-f]{64}$")
MAX_URL = 1024
MAX_PATH = 400
MAX_FILE = 2 << 30
MAX_MOD = 8 << 30
MAX_FILES = 50000
MAX_JSON = 16 << 20
SPARE = 1 << 30
CHUNK = 64 << 10


def log(text):
    print(f"[openswbf2] mods: {text}", flush=True)


def is_program(path):
    return path.lower().endswith(PROGRAMS)


def safe_segment(name):
    return (
        0 < len(name) <= 255
        and name not in (".", "..")
        and not BAD_CHARS.search(name)
        and not name.endswith((".", " "))
        and not DEVICES.match(name)
    )


def safe_relative(path):
    """A file path inside a mod folder, with forward slashes, that every launcher can write."""
    return isinstance(path, str) and 0 < len(path) <= MAX_PATH and all(safe_segment(p) for p in path.split("/"))


def safe_folder(name):
    return isinstance(name, str) and bool(FOLDER_NAME.match(name)) and not DEVICES.match(name)


def split_url(raw):
    """The decoded segments of a request path, or None when one of them could leave its folder."""
    if len(raw) > MAX_URL:
        return None
    path = raw.split("?", 1)[0]
    if not path.startswith("/"):
        return None
    parts = []
    for segment in path[1:].split("/"):
        try:
            name = urllib.parse.unquote(segment, errors="strict")
        except UnicodeDecodeError:
            return None
        if not safe_segment(name):
            return None
        parts.append(name)
    return parts


def query(raw):
    return urllib.parse.parse_qs(raw.split("?", 1)[1]) if "?" in raw else {}


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as file:
        while True:
            block = file.read(1 << 20)
            if not block:
                return digest.hexdigest()
            digest.update(block)


def write_json(path, data):
    temp = f"{path}.{uuid.uuid4().hex}.tmp"
    with open(temp, "w", encoding="utf-8") as file:
        json.dump(data, file)
    os.replace(temp, path)


def read_json(path, default):
    try:
        with open(path, encoding="utf-8") as file:
            return json.load(file)
    except (OSError, ValueError):
        return default


def tree_size(path):
    return sum(os.path.getsize(os.path.join(d, f)) for d, _, files in os.walk(path) for f in files)


def walk_files(base):
    """(relative path with /, full path) of every file under base, without following links."""
    for dirpath, dirnames, filenames in os.walk(base):
        dirnames.sort()
        for name in sorted(filenames):
            full = os.path.join(dirpath, name)
            yield os.path.relpath(full, base).replace(os.sep, "/"), full


class Refused(Exception):
    def __init__(self, status, error, **extra):
        super().__init__(error)
        self.status = status
        self.body = {"error": error, **extra}


class FileEntry:
    __slots__ = ("path", "size", "sha256", "real")

    def __init__(self, path, size, sha256, real):
        self.path, self.size, self.sha256, self.real = path, size, sha256, real


class Folder:
    def __init__(self, name, source, files):
        self.name, self.source, self.files = name, source, files
        self.index = {f.path: f for f in files}
        self.size = sum(f.size for f in files)

    def describe(self):
        return {
            "name": self.name,
            "size": self.size,
            "source": self.source,
            "files": [{"path": f.path, "size": f.size, "sha256": f.sha256} for f in self.files],
        }


class Library:
    """The mod folders the game loads (the first root wins a name) and their files' SHA-256,
    cached by path, size and modification time so each file is hashed once."""

    def __init__(self, roots, cache_file):
        self.roots = roots
        self.cache_file = cache_file
        self.lock = threading.Lock()
        self.cache = read_json(cache_file, {})
        self.folders = []
        self.scanned = None
        self.noted = set()

    def note(self, text):
        if text not in self.noted:
            self.noted.add(text)
            log(text)

    def current(self, max_age=5.0):
        with self.lock:
            fresh = self.scanned is not None and time.monotonic() - self.scanned < max_age
        return self.folders if fresh else self.scan()

    def lookup(self, folder, path):
        for entry in self.current():
            if entry.name == folder:
                return entry.index.get(path)
        return None

    def scan(self):
        with self.lock:
            cache, folders, taken = {}, [], set()
            for root, source in self.roots:
                try:
                    names = sorted(os.listdir(root))
                except OSError:
                    continue
                for name in names:
                    path = os.path.join(root, name)
                    if name.startswith(".") or not os.path.isdir(path):
                        continue
                    if name.lower() in taken:
                        self.note(f"{source} mod {name} is left out: the server's own {name} has that name")
                        continue
                    taken.add(name.lower())
                    if name.lower() == PATCH_ADDON:
                        continue
                    if not safe_folder(name):
                        self.note(f"{name!r} is left out: mod folder names are letters, digits, _ . -")
                        continue
                    folder = self.scan_folder(name, path, source, cache)
                    if folder:
                        folders.append(folder)
            if cache != self.cache:
                self.cache = cache
                try:
                    write_json(self.cache_file, cache)
                except OSError as error:
                    self.note(f"can't save the hash cache: {error}")
            self.folders = folders
            self.scanned = time.monotonic()
            return folders

    def scan_folder(self, name, path, source, cache):
        base = os.path.realpath(path)
        try:
            top = os.listdir(base)
        except OSError:
            return None
        if not any(n.lower() == ADDME and os.path.isfile(os.path.join(base, n)) for n in top):
            self.note(f"{name} is left out: it has no {ADDME}")
            return None
        files, seen = [], set()
        for rel, full in walk_files(base):
            if is_program(rel):
                self.note(f"{name}/{rel} is left out: programs are never shared")
                continue
            if not safe_relative(rel):
                self.note(f"{name}/{rel} is left out: Windows can't use that name")
                continue
            real = os.path.realpath(full)
            if not real.startswith(base + os.sep) or not os.path.isfile(real):
                self.note(f"{name}/{rel} is left out: it links outside its folder")
                continue
            if rel.lower() in seen:
                self.note(f"{name}/{rel} is left out: another file differs only in case")
                continue
            seen.add(rel.lower())
            try:
                stat = os.stat(real)
                files.append(FileEntry(rel, stat.st_size, self.hash(real, stat, cache), real))
            except OSError:
                continue
        return Folder(name, source, files)

    def hash(self, real, stat, cache):
        known = self.cache.get(real)
        if known and known[0] == stat.st_size and known[1] == stat.st_mtime_ns:
            cache[real] = known
            return known[2]
        digest = sha256_file(real)
        cache[real] = [stat.st_size, stat.st_mtime_ns, digest]
        return digest


class Gate:
    """Who a request is: the admin password, the join password (or no password on a server without
    one), or nobody. Five wrong passwords in ten seconds lock that address out for thirty."""

    FAILS, WINDOW, LOCKOUT = 5, 10.0, 30.0

    def __init__(self, join_password, admin_password):
        self.join = self.digest(join_password) if join_password else None
        self.admin = self.digest(admin_password) if admin_password else None
        self.fails = defaultdict(deque)
        self.locked = {}
        self.lock = threading.Lock()

    @staticmethod
    def digest(text):
        # Comparing digests takes the same time whatever the length of the guess
        return hashlib.sha256(text.encode("utf-8", "replace")).digest()

    def role(self, header, address):
        """'admin', 'player', None (wrong or missing password) or 'locked'."""
        now = time.monotonic()
        with self.lock:
            if self.locked.get(address, 0) > now:
                return "locked"
        scheme, _, value = (header or "").partition(" ")
        if scheme.lower() != "bearer" or not value.strip():
            return "player" if self.join is None else None
        given = self.digest(value.strip())
        is_admin = self.admin is not None and hmac.compare_digest(given, self.admin)
        is_join = self.join is not None and hmac.compare_digest(given, self.join)
        if is_admin:
            return "admin"
        if is_join:
            return "player"
        with self.lock:
            fails = self.fails[address]
            fails.append(now)
            while fails and now - fails[0] > self.WINDOW:
                fails.popleft()
            if len(fails) >= self.FAILS:
                fails.clear()
                self.locked[address] = now + self.LOCKOUT
                log(f"{address}: too many wrong passwords, locked out for {int(self.LOCKOUT)} s")
        return None


class Bucket:
    """One download speed limit shared by everyone, in bytes a second (0: none)."""

    def __init__(self, rate):
        self.rate = rate
        self.burst = max(rate / 4, CHUNK)
        self.tokens = self.burst
        self.last = time.monotonic()
        self.lock = threading.Lock()

    def take(self, count):
        if self.rate <= 0:
            return
        with self.lock:
            now = time.monotonic()
            self.tokens = min(self.burst, self.tokens + (now - self.last) * self.rate)
            self.last = now
            self.tokens -= count
            wait = -self.tokens / self.rate if self.tokens < 0 else 0
        if wait > 0:
            time.sleep(wait)


class Restarter:
    """Restarts the game (entrypoint.sh does it on SIGUSR1) at once, or once nobody is on."""

    def __init__(self, restart, players, interval=15.0, gap=60.0):
        self.restart = restart
        self.players = players
        self.interval = interval
        self.gap = gap
        self.when = None
        self.last = None
        self.lock = threading.Lock()
        self.wake = threading.Event()
        self.thread = None

    @property
    def state(self):
        return "when-empty" if self.when == "empty" else "none"

    def now(self):
        with self.lock:
            now = time.monotonic()
            if self.last is not None and now - self.last < self.gap:
                raise Refused(429, "the server restarted less than a minute ago")
            self.last = now
            self.when = None
        log("restarting the game to load mod changes")
        self.restart()

    def when_empty(self):
        with self.lock:
            self.when = "empty"
            if self.thread is None or not self.thread.is_alive():
                self.thread = threading.Thread(target=self.watch, daemon=True)
                self.thread.start()
        self.wake.set()
        log("the game restarts once nobody is on")

    def cancel(self):
        with self.lock:
            self.when = None
        self.wake.set()

    def watch(self):
        while True:
            self.wake.wait(self.interval)
            self.wake.clear()
            with self.lock:
                if self.when != "empty":
                    return
            if self.players() == 0:
                try:
                    self.now()
                except Refused:
                    continue
                return


def rcon_players(port, password):
    """Everyone connected to the game, from rcon /state, or None when it doesn't answer."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import rcon as rconlib

    local = port + 10001 if port + 10001 <= 65535 else 0
    client = rconlib.Rcon("127.0.0.1", port, 3.0, local)
    try:
        client.hello()
        client.join()
        client.line("/admin /login " + password)
        lines = client.line("/admin /state")
    except OSError:
        return None
    finally:
        client.sock.close()
    for line in lines:
        for token in line.split():
            if token.startswith("players="):
                try:
                    return int(token[len("players="):].split("/")[-1])
                except ValueError:
                    return None
    return None


class Uploads:
    """Admin uploads: files go to .staging/<folder> one by one, checked against their SHA-256; a
    commit checks the whole mod and moves it to .pending, which apply_pending puts in place."""

    def __init__(self, root, server_root):
        self.root = root
        self.server_root = server_root
        self.staging = os.path.join(root, ".staging")
        self.pending = os.path.join(root, ".pending")
        self.locks = defaultdict(threading.Lock)

    def check_folder(self, folder):
        if not safe_folder(folder):
            raise Refused(400, "mod folder names are letters, digits, _ . -")
        if folder.lower() == PATCH_ADDON:
            raise Refused(403, "the unofficial 1.3 patch is never taken: OpenSWBF2 has its support built in")
        if folder.lower() in self.names(self.server_root):
            raise Refused(409, f"{folder} is one of the server's own mods (the read-only addons folder)")
        for other in self.names(self.root, keep_case=True) | self.names(self.pending, keep_case=True):
            if other.lower() == folder.lower() and other != folder:
                raise Refused(409, f"the server already has {other}; mod names differ by more than case")

    @staticmethod
    def names(path, keep_case=False):
        try:
            return {n if keep_case else n.lower() for n in os.listdir(path) if os.path.isdir(os.path.join(path, n))}
        except OSError:
            return set()

    @staticmethod
    def check_list(body):
        files = body.get("files") if isinstance(body, dict) else None
        if not isinstance(files, list) or not files or len(files) > MAX_FILES:
            raise Refused(400, "the request needs the mod's file list")
        wanted, total, addme = {}, 0, False
        for item in files:
            if not isinstance(item, dict):
                raise Refused(400, "a file entry is not an object")
            path, size, sha = item.get("path"), item.get("size"), item.get("sha256")
            if not safe_relative(path):
                raise Refused(400, f"unsafe path: {path!r}")
            if is_program(path):
                raise Refused(403, f"{path} is a program; programs are never taken")
            if not isinstance(size, int) or isinstance(size, bool) or not 0 <= size <= MAX_FILE:
                raise Refused(413 if isinstance(size, int) and size > MAX_FILE else 400, f"bad size for {path}")
            if not isinstance(sha, str) or not SHA256.match(sha.lower()):
                raise Refused(400, f"bad SHA-256 for {path}")
            if path.lower() in {p.lower() for p in wanted}:
                raise Refused(400, f"{path} is listed twice")
            wanted[path] = (size, sha.lower())
            total += size
            addme = addme or path.lower() == ADDME
        if not addme:
            raise Refused(400, f"a mod folder needs an {ADDME}")
        if total > MAX_MOD:
            raise Refused(413, "a mod can be 8 GB at most")
        return wanted, total

    def record_file(self, folder):
        return os.path.join(self.staging, folder + ".json")

    def staged(self, folder, wanted):
        """Files already staged that match the list; others are removed."""
        base = os.path.join(self.staging, folder)
        record = read_json(self.record_file(folder), {})
        have, kept = [], {}
        if os.path.isdir(base):
            for rel, full in list(walk_files(base)):
                want = wanted.get(rel)
                if want and os.path.getsize(full) == want[0] and record.get(rel) == want[1]:
                    have.append(rel)
                    kept[rel] = want[1]
                else:
                    os.remove(full)
        return have, kept

    def begin(self, folder, body):
        self.check_folder(folder)
        wanted, total = self.check_list(body)
        with self.locks[folder]:
            os.makedirs(self.staging, exist_ok=True)
            have, kept = self.staged(folder, wanted)
            write_json(self.record_file(folder), kept)
            needed = total - sum(wanted[p][0] for p in have)
            if shutil.disk_usage(self.root).free < needed + SPARE:
                raise Refused(507, "the server hasn't enough free disk space for that mod")
        return {"have": have}

    def put(self, folder, path, length, sha, stream):
        self.check_folder(folder)
        if not safe_relative(path):
            raise Refused(400, "unsafe path")
        if is_program(path):
            raise Refused(403, f"{path} is a program; programs are never taken")
        if length is None:
            raise Refused(411, "Content-Length is needed")
        if length > MAX_FILE:
            raise Refused(413, "a file can be 2 GB at most")
        sha = (sha or "").lower()
        if not SHA256.match(sha):
            raise Refused(400, "an X-Sha256 header is needed")
        os.makedirs(self.staging, exist_ok=True)
        if shutil.disk_usage(self.root).free < length + SPARE:
            raise Refused(507, "the server hasn't enough free disk space")
        temp = os.path.join(self.staging, f".{uuid.uuid4().hex}.part")
        digest, left = hashlib.sha256(), length
        try:
            with open(temp, "wb") as file:
                while left > 0:
                    block = stream.read(min(CHUNK, left))
                    if not block:
                        raise Refused(400, "the upload ended early")
                    digest.update(block)
                    file.write(block)
                    left -= len(block)
            if digest.hexdigest() != sha:
                raise Refused(400, f"{path} arrived damaged (SHA-256 differs); send it again")
            target = os.path.join(self.staging, folder, *path.split("/"))
            with self.locks[folder]:
                os.makedirs(os.path.dirname(target), exist_ok=True)
                os.replace(temp, target)
                record = read_json(self.record_file(folder), {})
                record[path] = sha
                write_json(self.record_file(folder), record)
        finally:
            if os.path.exists(temp):
                os.remove(temp)
        return {"ok": True}

    def commit(self, folder, body):
        self.check_folder(folder)
        wanted, _ = self.check_list(body)
        with self.locks[folder]:
            have, _ = self.staged(folder, wanted)
            missing = sorted(set(wanted) - set(have))
            if missing:
                raise Refused(409, "some files haven't arrived", missing=missing[:100])
            os.makedirs(self.pending, exist_ok=True)
            target = os.path.join(self.pending, folder)
            if os.path.isdir(target):
                shutil.rmtree(target)
            os.replace(os.path.join(self.staging, folder), target)
            for leftover in (self.record_file(folder), os.path.join(self.pending, folder + ".remove")):
                if os.path.exists(leftover):
                    os.remove(leftover)
        log(f"{folder} uploaded ({len(wanted)} files): it loads when the game restarts")
        return {"pending": True}

    def remove(self, folder):
        self.check_folder(folder)
        with self.locks[folder]:
            live = os.path.isdir(os.path.join(self.root, folder))
            pending = os.path.join(self.pending, folder)
            if not live and not os.path.isdir(pending):
                raise Refused(404, f"{folder} isn't an uploaded mod")
            if os.path.isdir(pending):
                shutil.rmtree(pending)
            staging = os.path.join(self.staging, folder)
            if os.path.isdir(staging):
                shutil.rmtree(staging)
            if os.path.exists(self.record_file(folder)):
                os.remove(self.record_file(folder))
            if live:
                os.makedirs(self.pending, exist_ok=True)
                open(os.path.join(self.pending, folder + ".remove"), "w").close()
        log(f"{folder} removed" + (": it unloads when the game restarts" if live else ""))
        return {"pending": live}

    def describe_pending(self):
        try:
            names = sorted(os.listdir(self.pending))
        except OSError:
            return []
        changes = []
        for name in names:
            change = pending_change(self.pending, name)
            if change:
                folder, removal = change
                path = os.path.join(self.pending, name)
                changes.append({"name": folder, "action": "remove"} if removal
                               else {"name": folder, "action": "add", "size": tree_size(path)})
        return changes


def pending_change(pending, name):
    """(folder, is a removal) for an entry of .pending: a folder to add, or a <folder>.remove file."""
    path = os.path.join(pending, name)
    if name.endswith(".remove") and os.path.isfile(path):
        folder, removal = name[: -len(".remove")], True
    elif os.path.isdir(path):
        folder, removal = name, False
    else:
        return None
    return (folder, removal) if safe_folder(folder) else None


def apply_pending(root):
    """Puts uploads in place and removes the mods marked for removal. Run while the game is stopped."""
    pending = os.path.join(root, ".pending")
    trash = os.path.join(root, ".trash")
    try:
        names = sorted(os.listdir(pending))
    except OSError:
        return []
    done = []
    for name in names:
        change = pending_change(pending, name)
        if not change:
            continue
        folder, removal = change
        path = os.path.join(pending, name)
        live = os.path.join(root, folder)
        if os.path.isdir(live):
            os.makedirs(trash, exist_ok=True)
            os.replace(live, os.path.join(trash, f"{folder}.{uuid.uuid4().hex}"))
        if removal:
            os.remove(path)
            done.append(f"removed {folder}")
        else:
            os.replace(path, live)
            done.append(f"added {folder}")
    if os.path.isdir(trash):
        shutil.rmtree(trash, ignore_errors=True)
    return done


class ModService:
    def __init__(self, addons, config, join_password="", admin_password="", server_name="", version="",
                 uploads=True, rate=0, restart=None, players=None):
        self.uploads_root = os.path.join(config, "addons")
        os.makedirs(self.uploads_root, exist_ok=True)
        self.library = Library([(addons, "server"), (self.uploads_root, "uploaded")],
                               os.path.join(config, "mod-hashes.json"))
        self.gate = Gate(join_password, admin_password)
        self.bucket = Bucket(rate)
        self.restarter = Restarter(restart or (lambda: None), players or (lambda: None))
        self.uploads = Uploads(self.uploads_root, addons)
        self.server_name = server_name
        self.version = version
        self.uploads_on = uploads

    def manifest(self, admin):
        reply = {
            "schema": 1,
            "server": self.server_name,
            "openswbf2": self.version,
            "uploads": self.uploads_on,
            "restart": self.restarter.state,
            "folders": [folder.describe() for folder in self.library.scan()],
        }
        if admin:
            reply["pending"] = self.uploads.describe_pending()
        return reply


class Server(http.server.ThreadingHTTPServer):
    daemon_threads = True
    request_queue_size = 64
    MAX_CONNECTIONS, MAX_PER_ADDRESS = 32, 4

    def __init__(self, address, service):
        self.service = service
        self.connections = defaultdict(int)
        self.accepted = {}
        self.count_lock = threading.Lock()
        super().__init__(address, Handler)

    def verify_request(self, request, client_address):
        with self.count_lock:
            address = client_address[0]
            if sum(self.connections.values()) >= self.MAX_CONNECTIONS or self.connections[address] >= self.MAX_PER_ADDRESS:
                return False
            self.connections[address] += 1
            self.accepted[id(request)] = address
        return True

    def shutdown_request(self, request):
        with self.count_lock:
            address = self.accepted.pop(id(request), None)
            if address is not None:
                self.connections[address] -= 1
                if self.connections[address] <= 0:
                    del self.connections[address]
        super().shutdown_request(request)


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    server_version = "OpenSWBF2-mods/1"
    sys_version = ""
    timeout = 30

    def log_message(self, format, *args):
        pass

    def do_GET(self):
        self.route("GET")

    def do_HEAD(self):
        self.route("HEAD")

    def do_PUT(self):
        self.route("PUT")

    def do_POST(self):
        self.route("POST")

    def do_DELETE(self):
        self.route("DELETE")

    @property
    def service(self):
        return self.server.service

    def route(self, method):
        self.body_read = False
        try:
            parts = split_url(self.path)
            if parts is None:
                raise Refused(400, "bad path")
            if parts == ["mods.json"] and method in ("GET", "HEAD"):
                role = self.who()
                self.send_json(200, self.service.manifest(role == "admin"), head=method == "HEAD")
            elif len(parts) >= 3 and parts[0] == "mods" and method in ("GET", "HEAD"):
                self.who()
                self.send_file(parts[1], "/".join(parts[2:]), head=method == "HEAD")
            elif len(parts) >= 2 and parts[0] == "upload" and method in ("PUT", "POST", "DELETE"):
                self.who(admin=True)
                if not self.service.uploads_on:
                    raise Refused(403, "uploads are switched off on this server (MOD_UPLOADS=0)")
                self.upload(method, parts[1], parts[2:])
            elif parts == ["admin", "restart"] and method in ("POST", "DELETE"):
                self.who(admin=True)
                self.restart(method)
            else:
                raise Refused(404, "not found")
        except Refused as refused:
            self.send_json(refused.status, refused.body)
        except (BrokenPipeError, ConnectionResetError):
            self.close_connection = True
        except OSError as error:
            log(f"{self.client_address[0]} {method} {self.path}: {error}")
            self.send_json(500, {"error": "the server couldn't do that"})

    def who(self, admin=False):
        role = self.service.gate.role(self.headers.get("Authorization"), self.client_address[0])
        if role == "locked":
            raise Refused(429, "too many wrong passwords; wait 30 seconds")
        if role is None:
            raise Refused(401, "password")
        if admin and role != "admin":
            raise Refused(403, "that needs the admin password")
        return role

    def length(self):
        value = self.headers.get("Content-Length")
        if value is None:
            return None
        try:
            length = int(value)
        except ValueError:
            raise Refused(400, "bad Content-Length")
        if length < 0:
            raise Refused(400, "bad Content-Length")
        return length

    def read_json(self):
        length = self.length()
        if length is None:
            raise Refused(411, "Content-Length is needed")
        if length > MAX_JSON:
            raise Refused(413, "that list is too long")
        data = self.rfile.read(length)
        self.body_read = True
        try:
            return json.loads(data)
        except ValueError:
            raise Refused(400, "the body isn't JSON")

    def upload(self, method, folder, rest):
        uploads = self.service.uploads
        if method == "PUT" and rest:
            length = self.length()
            reply = uploads.put(folder, "/".join(rest), length, self.headers.get("X-Sha256"), self.rfile)
            self.body_read = True
        elif method == "POST" and rest == ["begin"]:
            reply = uploads.begin(folder, self.read_json())
        elif method == "POST" and rest == ["commit"]:
            reply = uploads.commit(folder, self.read_json())
        elif method == "DELETE" and not rest:
            reply = uploads.remove(folder)
        else:
            raise Refused(404, "not found")
        self.send_json(200, reply)

    def restart(self, method):
        restarter = self.service.restarter
        if method == "DELETE":
            restarter.cancel()
        else:
            when = query(self.path).get("when", ["now"])[0]
            if when == "now":
                restarter.now()
            elif when == "empty":
                restarter.when_empty()
            else:
                raise Refused(400, "when is now or empty")
        self.send_json(200, {"restart": restarter.state})

    def send_json(self, status, body, head=False):
        data = json.dumps(body).encode("utf-8")
        if status >= 400 and not self.body_read and (self.headers.get("Content-Length") or "0") != "0":
            self.close_connection = True  # the unread body would be taken for the next request
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        if status == 401:
            self.send_header("WWW-Authenticate", 'Bearer realm="mods"')
        if self.close_connection:
            self.send_header("Connection", "close")
        self.end_headers()
        if not head:
            self.wfile.write(data)

    def send_file(self, folder, path, head):
        entry = self.service.library.lookup(folder, path)
        if entry is None:
            raise Refused(404, "not found")
        try:
            file = open(entry.real, "rb")
        except OSError:
            raise Refused(404, "not found")
        with file:
            size = os.fstat(file.fileno()).st_size
            if size != entry.size:
                raise Refused(503, "that file just changed; read mods.json again")
            start, end = 0, size - 1
            status = 200
            ranges = self.headers.get("Range")
            if_range = self.headers.get("If-Range")
            if ranges and (if_range is None or if_range.strip('"') == entry.sha256):
                wanted = self.parse_range(ranges, size)
                if wanted == "unsatisfiable":
                    self.send_response(416)
                    self.send_header("Content-Range", f"bytes */{size}")
                    self.send_header("Content-Length", "0")
                    self.end_headers()
                    return
                if wanted:
                    start, end = wanted
                    status = 206
            length = max(0, end - start + 1)
            self.send_response(status)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(length))
            self.send_header("Accept-Ranges", "bytes")
            self.send_header("ETag", f'"{entry.sha256}"')
            if status == 206:
                self.send_header("Content-Range", f"bytes {start}-{end}/{size}")
            self.end_headers()
            if head:
                return
            file.seek(start)
            left = length
            try:
                while left > 0:
                    block = file.read(min(CHUNK, left))
                    if not block:
                        break
                    self.service.bucket.take(len(block))
                    self.wfile.write(block)
                    left -= len(block)
            except OSError:
                pass  # the player went away or stalled; the launcher resumes from what it has
            if left > 0:
                self.close_connection = True

    @staticmethod
    def parse_range(header, size):
        """(start, end) of a single byte range, 'unsatisfiable', or None to send the whole file."""
        unit, _, spec = header.partition("=")
        if unit.strip().lower() != "bytes" or "," in spec:
            return None
        first, dash, last = spec.strip().partition("-")
        if not dash:
            return None
        try:
            if first == "":
                if not last.isdigit():
                    return None
                count = int(last)
                if count == 0:
                    return "unsatisfiable"
                return max(0, size - count), size - 1
            if not first.isdigit() or (last and not last.isdigit()):
                return None
            start = int(first)
            end = min(int(last), size - 1) if last else size - 1
        except ValueError:
            return None
        if start >= size:
            return "unsatisfiable"
        if end < start:
            return None
        return start, end


def env_int(name, default):
    try:
        return int(os.environ.get(name) or default)
    except ValueError:
        return default


def main():
    parser = argparse.ArgumentParser(description="Mod downloads for an OpenSWBF2 dedicated server.")
    commands = parser.add_subparsers(dest="command", required=True)
    serve = commands.add_parser("serve", help="run the HTTP service")
    serve.add_argument("--host", default="0.0.0.0")
    serve.add_argument("--port", type=int, default=env_int("MOD_PORT", env_int("PORT", 3658)))
    serve.add_argument("--addons", default="/addons")
    serve.add_argument("--config", default="/config")
    apply = commands.add_parser("apply-pending", help="put uploaded mods in place (the game must be stopped)")
    apply.add_argument("--config", default="/config")
    args = parser.parse_args()

    if args.command == "apply-pending":
        for change in apply_pending(os.path.join(args.config, "addons")):
            log(change)
        return 0

    game_port = env_int("PORT", 3658)
    admin_password = os.environ.get("ADMIN_PASSWORD", "")
    entrypoint = env_int("ENTRYPOINT_PID", 0)

    def restart():
        if entrypoint > 0:
            os.kill(entrypoint, signal.SIGUSR1)

    service = ModService(
        args.addons,
        args.config,
        join_password=os.environ.get("JOIN_PASSWORD", ""),
        admin_password=admin_password,
        server_name=os.environ.get("SERVER_NAME", ""),
        version=os.environ.get("OPENSWBF2_VERSION", ""),
        uploads=os.environ.get("MOD_UPLOADS", "1") == "1",
        rate=env_int("MOD_RATE", 50) * 125000,
        restart=restart,
        players=lambda: rcon_players(game_port, admin_password),
    )
    server = Server((args.host, args.port), service)
    signal.signal(signal.SIGTERM, lambda *_: sys.exit(0))
    folders = service.library.scan()
    log(f"sharing {len(folders)} mods on TCP {args.port}: {' '.join(f.name for f in folders) or 'none yet'}")
    try:
        server.serve_forever()
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
