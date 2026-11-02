#!/usr/bin/env python3
"""Mod downloads for an OpenSWBF2 dedicated server: Source's FastDL, served by the server itself.

  modserver.py serve            the HTTP service; entrypoint.sh starts it next to the game
  modserver.py apply-pending    puts uploaded mods in place; entrypoint.sh runs it before each game start
  modserver.py settings         writes the next game start's settings; entrypoint.sh runs it before each

Players' launchers read GET /mods.json (every mod folder, with each file's size and SHA-256) and fetch
what they lack from GET /mods/<folder>/<path>, which takes ranges, so downloads resume. Reading needs
the join password when the server has one (or the admin password). Admins upload mods the server
lacks: they wait in /config/addons/.pending and load when the game restarts, at once or when nobody
is on. Programs are never listed, served or taken, and neither is the unofficial 1.3 patch.

It also runs the launchers' lobby: players who connect from the launcher's Multiplayer page wait in
it (POST /lobby/join, GET /status) with their game closed, and Start (POST /admin/match?do=start)
opens all their games into the server's held lobby, then starts the match once they're in.

It keeps the server's settings (PUT /admin/settings): the playlist, game options, name, join
password, player limit, tick rate and lobby. The launcher's admin screen sets them, and controls the
game over rcon itself. A new server starts with the defaults. GET /missions.json is the server's own
map list, so the admin picks maps the server can load.
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
import socket
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
# A refused request's body up to this size is read and dropped, so the connection stays usable;
# past it the reply goes out and the connection closes after a short drain
DRAIN = 1 << 20
LINGER = 2.0


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

    def set_join(self, join_password):
        """An admin changed the join password: it counts from the next request."""
        self.join = self.digest(join_password) if join_password else None

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


def reply_lines(payload):
    """The text lines of an rcon reply payload: the key, then [1, length, text] per line."""
    lines, i = [], 1
    while i + 1 < len(payload) and payload[i] == 1:
        length = payload[i + 1]
        lines.append(payload[i + 2 : i + 2 + length].decode("ascii", "replace"))
        i += 2 + length
    return lines


def parse_state(lines):
    """rcon /state: {phase, map, next, players (everyone connected), teams (those on a team: a
    player still loading the map has none yet)}."""
    values = {}
    for line in lines:
        for token in line.split():
            key, eq, value = token.partition("=")
            if eq:
                values[key] = value
    try:
        team1, team2, players = (int(n) for n in values.get("players", "").split("/"))
    except ValueError:
        team1 = team2 = players = 0
    phase = values.get("state", "")
    return {
        "phase": phase if phase in ("shell", "lobby", "warmup", "playing", "ended") else "offline",
        "map": values.get("map", ""),
        "next": values.get("next", ""),
        "players": players,
        "teams": team1 + team2,
    }


def parse_playerinfo(lines):
    """rcon /playerinfo: (names, the slot of the next page or None). Rows are
    "p <slot> <team> <alive> <score> <kills> <deaths> <ping> <name>"; a headless client has no name."""
    names, more = [], None
    for line in lines:
        parts = line.split(" ", 8)
        if parts[0] == "p" and len(parts) >= 8:
            name = parts[8].strip() if len(parts) > 8 else ""
            names.append(name or f"Player {parts[1]}")
        elif parts[0] == "more" and len(parts) > 1 and parts[1].isdigit():
            more = int(parts[1])
    return names, more


class RconGame:
    """The game over rcon, through one admin session kept open: the server takes 5 logins in 10 s
    and keeps 3 sessions, and forgets one after 60 s of silence. Replies are told from the lines the
    server pushes on its own by their "#<command>" first line."""

    def __init__(self, port, password, timeout=3.0):
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        import rcon as rconlib

        self.rconlib = rconlib
        self.port = port
        self.password = password
        self.timeout = timeout
        self.local = port + 10001 if port + 10001 <= 65535 else 0
        self.client = None
        self.lock = threading.Lock()

    def close(self):
        if self.client is not None:
            self.client.sock.close()
            self.client = None

    def send(self, text, done):
        """Sends one admin line and returns the first reply whose lines satisfy done(lines)."""
        client = self.client
        raw = ("/admin " + text).encode("ascii", "replace")[:63]
        client.send("J", bytes([len(raw)]) + raw)
        deadline = time.monotonic() + self.timeout
        while time.monotonic() < deadline:
            payload = client.receive()
            if payload:
                client.key = payload[0]
            lines = reply_lines(payload)
            if any(line.strip() == "not logged in" for line in lines):
                raise PermissionError("not logged in")
            if done(lines):
                return lines
        raise TimeoutError(f"no reply to {text}")

    def login(self):
        lines = self.send("/login " + self.password,
                          lambda ls: any(l.strip() in ("logged in", "login failed", "login rate limited") for l in ls))
        if not any(line.strip() == "logged in" for line in lines):
            raise PermissionError(" ".join(lines))

    def open(self):
        self.client = self.rconlib.Rcon("127.0.0.1", self.port, self.timeout, self.local)
        self.client.hello()
        self.client.join()
        self.login()

    def ask(self, text):
        """An admin command's reply lines, after its "#<command>" line."""
        header = "#" + text.split()[0].lstrip("/").lower()

        def done(lines):
            return any(line.strip().lower() == header for line in lines)

        with self.lock:
            for _ in range(2):
                try:
                    if self.client is None:
                        self.open()
                    try:
                        lines = self.send(text, done)
                    except PermissionError:
                        self.login()
                        lines = self.send(text, done)
                    return lines[[l.strip().lower() for l in lines].index(header) + 1:]
                except OSError:
                    self.close()
            raise OSError("the game doesn't answer rcon")

    def state(self):
        return parse_state(self.ask("/state"))

    def names(self):
        names, first = [], None
        for _ in range(6):
            page, first = parse_playerinfo(self.ask("/playerinfo" + (f" {first}" if first else "")))
            names += page
            if first is None:
                break
        return names

    def command(self, text):
        return self.ask(text)


NAME = re.compile(r'^[ !#-\[\]-~]{1,15}$')


def valid_name(name):
    """A player name as the launcher's PlayerProfiles.IsValid takes it."""
    return isinstance(name, str) and bool(NAME.match(name)) and name.strip() == name and not name.startswith("[")


class Lobby:
    """Players waiting in their launchers (their game isn't running yet) for the match to start.
    An entry goes 20 s after its launcher last asked for /status."""

    MAX, MAX_PER_ADDRESS, EXPIRE = 64, 16, 20.0

    def __init__(self, clock=time.monotonic):
        self.clock = clock
        self.entries = {}
        self.lock = threading.Lock()

    def prune(self):
        now = self.clock()
        for token in [t for t, e in self.entries.items() if now - e["seen"] > self.EXPIRE]:
            del self.entries[token]

    def join(self, name, address):
        if not valid_name(name):
            raise Refused(400, "a player name is 1 to 15 plain characters")
        with self.lock:
            self.prune()
            if len(self.entries) >= self.MAX:
                raise Refused(503, "the lobby is full")
            if sum(1 for e in self.entries.values() if e["address"] == address) >= self.MAX_PER_ADDRESS:
                raise Refused(429, "too many players from one address")
            token = uuid.uuid4().hex
            self.entries[token] = {"name": name, "state": "downloading", "progress": 0, "address": address, "seen": self.clock()}
        return token

    def entry(self, token):
        entry = self.entries.get(token) if isinstance(token, str) else None
        if entry is None:
            raise Refused(404, "not in the lobby; join again")
        return entry

    def update(self, token, state, progress):
        if state not in ("downloading", "waiting"):
            raise Refused(400, "state is downloading or waiting")
        with self.lock:
            entry = self.entry(token)
            entry["state"] = state
            entry["progress"] = max(0, min(100, int(progress))) if isinstance(progress, (int, float)) and not isinstance(progress, bool) else 0
            entry["seen"] = self.clock()

    def touch(self, token):
        with self.lock:
            self.prune()
            if token in self.entries:
                self.entries[token]["seen"] = self.clock()
                return True
        return False

    def leave(self, token):
        with self.lock:
            self.entries.pop(token, None)

    def describe(self):
        with self.lock:
            self.prune()
            return [{"name": e["name"], "state": e["state"], "progress": e["progress"]} for e in self.entries.values()]

    def count(self, state=None):
        with self.lock:
            self.prune()
            return sum(1 for e in self.entries.values() if state is None or e["state"] == state)


class GameWatch:
    """What the game is doing, polled over rcon (every 2 s while a launcher waits, else every 15 s),
    and the lobby start: Start opens every waiting player's game into the server's held lobby, and
    the match begins once that many players have loaded (they are on a team), 15 s after that many
    are connected (some may sit on the team select screen), or after 60 s. Players still
    downloading don't hold it up; they join when their download is done."""

    BUSY, IDLE, NAMES, DEADLINE, COUNTDOWN, SETTLE = 2.0, 15.0, 4.0, 60.0, 5, 15.0
    OFFLINE = {"phase": "offline", "map": "", "next": "", "players": 0, "teams": 0}

    def __init__(self, game, lobby, clock=time.monotonic):
        self.game = game
        self.lobby = lobby
        self.clock = clock
        self.lock = threading.Lock()
        self.wake = threading.Event()
        self.state = dict(self.OFFLINE)
        self.connected = None
        self.names = []
        self.polled = None
        self.named = None
        self.loading = False
        self.deadline = None
        self.expected = 0

    @property
    def busy(self):
        return self.loading or self.lobby.count() > 0

    def refresh(self):
        try:
            state = self.game.state() if self.game else None
        except OSError:
            state = None
        now = self.clock()
        with self.lock:
            self.state = state or dict(self.OFFLINE)
            self.polled = now
        if state and self.busy and (self.named is None or now - self.named >= self.NAMES):
            try:
                names = self.game.names()
            except OSError:
                names = None
            with self.lock:
                if names is not None:
                    self.names = names
                self.named = now

    def step(self):
        """One look at the game, then the start once everyone is in (the loop calls it; tests too)."""
        self.refresh()
        with self.lock:
            if not self.loading:
                return
            phase, players, teams = self.state["phase"], self.state["players"], self.state.get("teams", 0)
            if phase != "lobby":
                self.loading = False  # started some other way, or the server changed map
                return
            now = self.clock()
            if players >= self.expected:
                self.connected = self.connected or now
            else:
                self.connected = None
            loaded = teams >= self.expected
            settled = self.connected is not None and now - self.connected >= self.SETTLE
            if not (loaded or settled or now >= self.deadline):
                return
            self.loading = False
        self.send_start()

    def send_start(self):
        try:
            self.game.command(f"/start {self.COUNTDOWN}")
            log(f"match starting: {self.state.get('teams', 0)} of {self.expected} players loaded, {self.state['players']} connected")
        except OSError as error:
            log(f"couldn't start the match: {error}")
        self.refresh()

    def run(self):
        while True:
            self.step()
            self.wake.wait(self.BUSY if self.busy else self.IDLE)
            self.wake.clear()

    def start_thread(self):
        threading.Thread(target=self.run, daemon=True).start()

    def players(self):
        """Everyone connected, or None while the game doesn't answer (for restart when empty)."""
        if self.polled is None or self.clock() - self.polled > 5:
            self.refresh()
        with self.lock:
            return None if self.state["phase"] == "offline" else self.state["players"]

    def start(self):
        self.refresh()
        with self.lock:
            if self.state["phase"] != "lobby":
                raise Refused(409, "the match isn't waiting in the lobby" if self.state["phase"] != "offline"
                              else "the game doesn't answer")
            if self.loading:
                raise Refused(409, "the start is already loading everyone's game")
            waiting = self.lobby.count("waiting")
            self.expected = self.state["players"] + waiting
            if waiting == 0:
                now_start = True
            else:
                now_start = False
                self.loading = True
                self.connected = None
                self.deadline = self.clock() + self.DEADLINE
        log(f"start: loading {waiting} waiting players' games" if not now_start else "start: nobody waits in a launcher")
        if now_start:
            self.send_start()
        self.wake.set()
        return self.describe()

    def cancel(self):
        with self.lock:
            self.loading = False
        return self.describe()

    def back_to_lobby(self):
        try:
            lines = self.game.command("/lobby now")
        except OSError:
            raise Refused(503, "the game doesn't answer")
        self.wake.set()
        return {"reply": lines}

    def describe(self):
        with self.lock:
            left = max(0, int(self.deadline - self.clock())) if self.loading else None
            return {**self.state, "inGame": list(self.names), "start": "loading" if self.loading else "none", "startIn": left}


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


# --- Server settings: kept on the server, set from the launcher's admin screen -----------------
MISSION = re.compile(r"^[A-Za-z0-9_]{1,31}$")
JOIN_PASSWORD = re.compile(r'^[^\s"]{0,15}$')
MAX_MAPS_LINE = 1200
TICK_RATES = (30, 60, 120)
# The game options as the launcher's ServerProfile.OptionCommands sends them: the values the game
# takes, and a new server's. ServerSettings.cfg lists them in this order.
OPTIONS = (
    ("difficulty", range(2, 5), 3), ("conaiperteam", range(0, 65), 32), ("ctfaiperteam", range(0, 65), 32),
    ("assaiperteam", range(0, 65), 32), ("eliaiperteam", range(0, 65), 32), ("teamdamage", range(0, 2), 1),
    ("shownames", range(0, 2), 1), ("autoassignteams", range(0, 2), 1), ("awards", range(0, 2), 1),
    ("heroes", range(0, 2), 1), ("hrunlock", (3, 5), 3), ("hrunlockvalue", range(0, 301), 10),
    ("hrplayer", (1, 7), 1), ("hrrespawnvalue", range(-1, 301), 120), ("aiheroes", range(0, 2), 1),
    ("conreinforcements", range(10, 501), 100),
    ("contimelimit", range(0, 61), 0), ("ctfscorelimit", range(1, 16), 5), ("ctftimelimit", range(0, 61), 0),
    ("huntimelimit", range(0, 61), 0), ("huntscorelimit", range(50, 151), 50), ("assscorelimit", range(150, 301), 180),
)
OPTION_VALUES = {name: values for name, values, _ in OPTIONS}
# A new server's settings, until an admin changes them: 8 stock Conquest maps, 32 bots a team (the
# maps' team size; a team takes 64 soldiers, so players push bots out past that), Hard
DEFAULTS = {
    "name": "OpenSWBF2 Server",
    "joinPassword": "",
    "maxPlayers": 32,
    "tickRate": 60,
    "lobby": False,
    "maps": [{"mission": m} for m in "geo1c_con kas2g_con nab2c_con hot1g_con tat2g_con cor1c_con myg1g_con uta1c_con".split()],
    "options": {name: default for name, _, default in OPTIONS},
}
# Fixed while the game runs: they take a restart (the game can't clear its join password live)
RESTART_KEYS = ("name", "joinPassword", "maxPlayers", "tickRate", "lobby")
# compose.yaml's variables from before the admin screen set all this (0.2.4): read once, at the
# first start without saved settings, so an existing server keeps its maps and options
LEGACY = {
    "SERVER_NAME": "name", "JOIN_PASSWORD": "joinPassword", "MAX_PLAYERS": "maxPlayers", "TICK_RATE": "tickRate",
    "LOBBY": "lobby", "MAPS": "maps", "BOTS_PER_TEAM": ("conaiperteam", "ctfaiperteam", "assaiperteam", "eliaiperteam"),
    "DIFFICULTY": "difficulty", "HEROES": "heroes", "AI_HEROES": "aiheroes", "HERO_UNLOCK": "hrunlock",
    "HERO_UNLOCK_VALUE": "hrunlockvalue", "HERO_PLAYER": "hrplayer", "TEAM_DAMAGE": "teamdamage",
    "AUTO_ASSIGN_TEAMS": "autoassignteams", "SHOW_NAMES": "shownames", "AWARDS": "awards",
    "CONQUEST_REINFORCEMENTS": "conreinforcements", "CONQUEST_TIME_LIMIT": "contimelimit",
    "CTF_TIME_LIMIT": "ctftimelimit", "HUNT_TIME_LIMIT": "huntimelimit", "CTF_CAPTURES": "ctfscorelimit",
    "HUNT_SCORE": "huntscorelimit", "ASSAULT_SCORE": "assscorelimit",
}


def is_int(value):
    return isinstance(value, int) and not isinstance(value, bool)


def clean_maps(maps):
    """[{mission, tickets1, tickets2}] with both tickets or neither (a lone one counts for both, as
    the launcher's PlaylistEntry.Tickets does), or None when an entry is wrong."""
    if not isinstance(maps, list) or not maps:
        return None
    entries = []
    for entry in maps:
        if not isinstance(entry, dict) or not isinstance(entry.get("mission"), str) or not MISSION.match(entry["mission"]):
            return None
        first, second = entry.get("tickets1"), entry.get("tickets2")
        if any(t is not None and not is_int(t) for t in (first, second)):
            return None
        first = first if first is not None else second
        second = second if second is not None else first
        tickets = {} if first is None else {"tickets1": min(max(first, 1), 9999), "tickets2": min(max(second, 1), 9999)}
        entries.append({"mission": entry["mission"], **tickets})
    return entries


def map_words(maps):
    """The game's command line for a playlist: geo1c_con 150 150 kas2g_con."""
    words = []
    for entry in maps:
        words.append(entry["mission"])
        if "tickets1" in entry:
            words += [str(entry["tickets1"]), str(entry["tickets2"])]
    return words


def parse_map_words(text):
    """An old compose.yaml's MAPS: mission names, each maybe followed by its two ticket numbers."""
    maps = []
    words = text.split()
    i = 0
    while i < len(words):
        entry = {"mission": words[i]}
        if i + 2 < len(words) and words[i + 1].isdigit() and words[i + 2].isdigit():
            entry["tickets1"], entry["tickets2"] = int(words[i + 1]), int(words[i + 2])
            i += 2
        maps.append(entry)
        i += 1
    return maps


def clean_settings(body):
    """The settings an admin sent, checked; Refused(400) names the first one that's wrong."""
    if not isinstance(body, dict):
        raise Refused(400, "the body is an object")
    unknown = sorted(set(body) - set(DEFAULTS))
    if unknown:
        raise Refused(400, f"unknown setting {unknown[0]}")
    settings = {}
    if "name" in body:
        name = body["name"].strip() if isinstance(body["name"], str) else ""
        if not 0 < len(name) <= 31 or '"' in name or any(ord(c) < 32 or ord(c) == 127 for c in name):
            raise Refused(400, "the server name is 1 to 31 characters, without quotes")
        settings["name"] = name
    if "joinPassword" in body:
        if not isinstance(body["joinPassword"], str) or not JOIN_PASSWORD.match(body["joinPassword"]):
            raise Refused(400, "the join password is at most 15 characters, without spaces or quotes")
        settings["joinPassword"] = body["joinPassword"]
    if "lobby" in body:
        if not isinstance(body["lobby"], bool):
            raise Refused(400, "lobby is true or false")
        settings["lobby"] = body["lobby"]
    if "maxPlayers" in body:
        if not is_int(body["maxPlayers"]) or not 2 <= body["maxPlayers"] <= 64:
            raise Refused(400, "the player limit is 2 to 64")
        settings["maxPlayers"] = body["maxPlayers"]
    if "tickRate" in body:
        if not is_int(body["tickRate"]) or body["tickRate"] not in TICK_RATES:
            raise Refused(400, "the tick rate is 30, 60 or 120")
        settings["tickRate"] = body["tickRate"]
    if "maps" in body:
        maps = clean_maps(body["maps"])
        if maps is None:
            raise Refused(400, "the playlist is a list of missions such as geo1c_con, with optional tickets")
        if len(" ".join(map_words(maps))) > MAX_MAPS_LINE:
            raise Refused(400, f"the playlist is too long for the game's command line ({MAX_MAPS_LINE} characters at most)")
        settings["maps"] = maps
    if "options" in body:
        options = body["options"]
        if not isinstance(options, dict):
            raise Refused(400, "options is an object of game options")
        settings["options"] = {}
        for name, value in options.items():
            if name not in OPTION_VALUES:
                raise Refused(400, f"unknown game option {name}")
            if not is_int(value) or value not in OPTION_VALUES[name]:
                raise Refused(400, f"{name} can't be {value}")
            settings["options"][name] = value
    return settings


def flatten(values):
    """One key per setting, each game option on its own, so a saved one counts by itself."""
    flat = {key: value for key, value in values.items() if key != "options"}
    for name, value in (values.get("options") or {}).items():
        flat["options." + name] = value
    return flat


def unflatten(flat):
    values = {}
    for key, value in flat.items():
        if key.startswith("options."):
            values.setdefault("options", {})[key[len("options."):]] = value
        else:
            values[key] = value
    return values


def describe_keys(keys):
    """'maps, name, 3 game options' for the log."""
    options = [key for key in keys if key.startswith("options.")]
    named = [key for key in keys if not key.startswith("options.")]
    if options:
        named.append(f"{len(options)} game option{'' if len(options) == 1 else 's'}")
    return ", ".join(named)


def merge(saved):
    """The settings a game start uses: the defaults, with what an admin saved over them, setting
    by setting (each game option on its own)."""
    return unflatten({**flatten(DEFAULTS), **flatten(saved)})


def legacy_settings(env):
    """(what an old compose.yaml sets, as the admin screen saves it; the variables it sets). Values
    are taken as the old entrypoint took them (clamped, on/off as 1/true/yes/on); a value the game
    can't take at all is left out."""
    def on(text):
        return text.lower() in ("1", "true", "yes", "on")

    def number(text):
        return int(text) if re.fullmatch(r"-?[0-9]+", text) else None

    found, flat = [], {}
    for variable, target in LEGACY.items():
        raw = (env.get(variable) or "").strip()
        if not raw:
            continue
        found.append(variable)
        if target == "name":
            flat["name"] = raw.replace('"', "")[:31].strip()
        elif target == "joinPassword":
            flat["joinPassword"] = raw
        elif target == "lobby":
            flat["lobby"] = on(raw)
        elif target == "maps":
            flat["maps"] = clean_maps(parse_map_words(raw))
        elif target == "maxPlayers" and number(raw) is not None:
            flat["maxPlayers"] = min(max(number(raw), 2), 64)
        elif target == "tickRate":
            flat["tickRate"] = number(raw)
        elif target in ("maxPlayers",):
            continue
        else:
            for option in target if isinstance(target, tuple) else (target,):
                values = OPTION_VALUES[option]
                if isinstance(values, tuple):  # two choices; anything else is the first, as before
                    flat["options." + option] = values[1] if number(raw) == values[1] else values[0]
                elif values == range(0, 2):
                    flat["options." + option] = 1 if on(raw) else 0
                elif number(raw) is not None:
                    flat["options." + option] = min(max(number(raw), values[0]), values[-1])
    settings = {}
    for key, value in unflatten({k: v for k, v in flat.items() if v is not None}).items():
        try:
            settings.update(clean_settings({key: value}))
        except Refused:
            pass
    return settings, found


class Settings:
    """The server's settings (config/settings/admin.json): the playlist, game options, name, join
    password, player limit, tick rate and lobby an admin saved from the launcher's admin screen,
    over the defaults; and what the running game started with (effective.json)."""

    def __init__(self, folder):
        self.folder = folder
        self.lock = threading.Lock()
        os.makedirs(folder, exist_ok=True)

    def path(self, name):
        return os.path.join(self.folder, name)

    def running(self):
        data = read_json(self.path("effective.json"), None)
        return data if isinstance(data, dict) else None

    def saved(self):
        """What an admin saved, or None before the server's first start set the file up."""
        data = read_json(self.path("admin.json"), None)
        if not isinstance(data, dict) or data.get("schema") != 1:
            return None
        values = data.get("values")
        if not isinstance(values, dict):
            return {}
        # Each one checked again, so a hand-edited file can't hand the game a bad value
        kept = {}
        for key, value in values.items():
            try:
                kept.update(clean_settings({key: value}))
            except Refused:
                pass
        return kept

    def store(self, values):
        write_json(self.path("admin.json"), {"schema": 1, "values": values})

    def upcoming(self):
        """The settings the next game start uses."""
        return merge(self.saved() or {})

    def describe(self):
        """GET /admin/settings: what the next start uses, which of it an admin saved, the defaults
        (the rest), and whether the running game needs a restart for its name or limits."""
        saved = self.saved() or {}
        upcoming = merge(saved)
        running = self.running()
        restart = running is not None and any(upcoming.get(key) != running.get(key) for key in RESTART_KEYS)
        return {"schema": 1, "settings": upcoming, "saved": sorted(flatten(saved)), "defaults": DEFAULTS,
                "restartNeeded": restart}

    def save(self, body):
        """PUT /admin/settings: the settings sent, over what was saved before."""
        settings = clean_settings(body)
        with self.lock:
            self.store(unflatten({**flatten(self.saved() or {}), **flatten(settings)}))
        return self.describe()

    def reset(self):
        """DELETE /admin/settings: back to the defaults (an old compose.yaml isn't read again)."""
        with self.lock:
            self.store({})
        return self.describe()

    def prepare(self, env, extra=""):
        """Before each game start: on the first, takes an old compose.yaml's settings once; then
        writes ServerSettings.cfg (the game options, then EXTRA_SETTINGS, whose lines come last
        and so win) and effective.json. Returns the settings the game starts with."""
        with self.lock:
            saved = self.saved()
            if saved is None:
                saved, found = legacy_settings(env)
                self.store(saved)
                if found:
                    print(f"[openswbf2] Took {', '.join(found)} from compose.yaml into the server's settings, this once. "
                          "The launcher's admin screen sets them now (Server manager > Connect to server as admin), "
                          "so they can go from compose.yaml.", file=sys.stderr)
            else:
                found = [variable for variable in LEGACY if (env.get(variable) or "").strip()]
                if found:
                    print(f"[openswbf2] compose.yaml still sets {', '.join(found)}: not used any more. The launcher's "
                          "admin screen sets them (Server manager > Connect to server as admin).", file=sys.stderr)
            used = sorted(flatten(saved))
            if used:
                print(f"[openswbf2] The server's saved settings: {describe_keys(used)} (the rest are the defaults)", file=sys.stderr)
            effective = merge(saved)
            lines = [f"/{name} {effective['options'][name]}" for name, _, _ in OPTIONS]
            lines += [line.strip() for line in extra.splitlines() if line.strip()]
            temp = self.path(f"ServerSettings.cfg.{uuid.uuid4().hex}.tmp")
            with open(temp, "w", encoding="utf-8", newline="") as file:
                file.write("".join(line + "\r\n" for line in lines))
            os.replace(temp, self.path("ServerSettings.cfg"))
            write_json(self.path("effective.json"), effective)
        return effective


class ModService:
    """join_password: None follows the running game's (the server's settings, as the game started
    with them), so downloads and joining always take the same one; a string fixes it (tests)."""

    def __init__(self, addons, config, join_password=None, admin_password="", version="",
                 uploads=True, rate=0, restart=None, players=None, game=None, clock=time.monotonic):
        self.uploads_root = os.path.join(config, "addons")
        os.makedirs(self.uploads_root, exist_ok=True)
        self.library = Library([(addons, "server"), (self.uploads_root, "uploaded")],
                               os.path.join(config, "mod-hashes.json"))
        self.settings = Settings(os.path.join(config, "settings"))
        self.follow_join = join_password is None
        self.join_password = self.game_join_password() if self.follow_join else join_password
        self.gate = Gate(self.join_password, admin_password)
        self.bucket = Bucket(rate)
        self.lobby = Lobby(clock)
        self.watch = GameWatch(game, self.lobby, clock)
        self.restarter = Restarter(restart or (lambda: None), players or self.watch.players)
        self.uploads = Uploads(self.uploads_root, addons)
        self.version = version
        self.uploads_on = uploads

    @property
    def server_name(self):
        """The name the running game started with, which an admin may have saved."""
        running = self.settings.running() or self.settings.upcoming()
        return running.get("name") or DEFAULTS["name"]

    def game_join_password(self):
        running = self.settings.running() or self.settings.upcoming()
        password = running.get("joinPassword")
        return password if isinstance(password, str) else ""

    def follow_game(self):
        """Before each request: the game restarted with another join password, so downloads and
        the lobby take that one now."""
        if not self.follow_join:
            return
        password = self.game_join_password()
        if password != self.join_password:
            self.join_password = password
            self.gate.set_join(password)

    def missions(self):
        """The server's own map list (the game's -listmissions, which the entrypoint runs once per
        version and set of mods), or None."""
        try:
            with open(self.settings.path("missions.json"), "rb") as file:
                return file.read()
        except OSError:
            return None

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
            reply["settings"] = True
        return reply

    def status(self, token=None):
        """The game and the lobby, for launchers waiting to play and for the admin screen."""
        mine = token is not None and self.lobby.touch(token)
        return {"schema": 1, "server": self.server_name, **self.watch.describe(),
                "lobby": self.lobby.describe(), "inLobby": mine}


class Server(http.server.ThreadingHTTPServer):
    daemon_threads = True
    request_queue_size = 64
    # Each launcher in the lobby keeps a connection, and players at one LAN party share an address
    MAX_CONNECTIONS, MAX_PER_ADDRESS = 96, 16

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
        self.body_started = False  # an upload read part of it: how much is left is unknown
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
            elif parts == ["status"] and method in ("GET", "HEAD"):
                self.who()
                self.send_json(200, self.service.status(query(self.path).get("token", [None])[0]), head=method == "HEAD")
            elif len(parts) == 2 and parts[0] == "lobby" and method == "POST":
                self.who()
                self.lobby(parts[1])
            elif parts == ["admin", "match"] and method in ("POST", "DELETE"):
                self.who(admin=True)
                self.match(method)
            elif parts == ["admin", "settings"] and method in ("GET", "PUT", "DELETE"):
                self.who(admin=True)
                if method == "PUT":
                    reply = self.service.settings.save(self.read_json())
                    log("an admin saved the server's settings")
                elif method == "DELETE":
                    reply = self.service.settings.reset()
                    log("an admin put the server's settings back to the defaults")
                else:
                    reply = self.service.settings.describe()
                self.send_json(200, reply)
            elif parts == ["missions.json"] and method in ("GET", "HEAD"):
                self.who()
                self.send_missions(head=method == "HEAD")
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
        self.service.follow_game()
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
            self.body_started = True
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

    def lobby(self, action):
        lobby = self.service.lobby
        body = self.read_json()
        if not isinstance(body, dict):
            raise Refused(400, "the body is an object")
        if action == "join":
            reply = {"token": lobby.join(body.get("name"), self.client_address[0])}
            log(f"{body.get('name')} is in the lobby")
        elif action == "update":
            lobby.update(body.get("token"), body.get("state"), body.get("progress", 0))
            reply = {"ok": True}
        elif action == "leave":
            lobby.leave(body.get("token"))
            reply = {"ok": True}
        else:
            raise Refused(404, "not found")
        self.service.watch.wake.set()
        self.send_json(200, reply)

    def match(self, method):
        watch = self.service.watch
        if method == "DELETE":
            reply = watch.cancel()
        else:
            what = query(self.path).get("do", [""])[0]
            if what == "start":
                reply = watch.start()
            elif what == "lobby":
                reply = watch.back_to_lobby()
            else:
                raise Refused(400, "do is start or lobby")
        self.send_json(200, reply)

    def send_json(self, status, body, head=False):
        data = json.dumps(body).encode("utf-8")
        linger = False
        if status >= 400 and not self.body_read:
            linger = not self.drop_body()
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
        if linger:
            self.linger()

    def drop_body(self):
        """Read and drop the unread body of a request being refused. Closing with it unread makes
        the client's TCP stack reset the connection, and the client loses the reply. Returns False
        when the body is too big to drop here; the connection then closes after linger()."""
        try:
            length = int(self.headers.get("Content-Length") or 0)
        except ValueError:
            length = -1
        if length == 0:
            return True
        self.close_connection = True  # whatever is left would be taken for the next request
        if 0 < length <= DRAIN and not self.body_started:
            try:
                while length > 0:
                    data = self.rfile.read(min(length, CHUNK))
                    if not data:
                        break
                    length -= len(data)
                self.body_read = True
                self.close_connection = False
                return True
            except OSError:
                return True
        return False

    def linger(self):
        """After the reply to a refused large upload: stop sending, then read and drop what the
        client still sends for a moment, so the reply reaches it before the connection closes."""
        try:
            self.wfile.flush()
            self.connection.shutdown(socket.SHUT_WR)
            self.connection.settimeout(LINGER)
            deadline = time.monotonic() + LINGER
            while time.monotonic() < deadline and self.connection.recv(CHUNK):
                pass
        except OSError:
            pass

    def send_missions(self, head):
        data = self.service.missions()
        if data is None:
            raise Refused(404, "the server has no map list yet")
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
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
    prepare = commands.add_parser("settings", help="write the settings for the next game start and print its name, "
                                                   "player limit, tick rate, lobby (1/0), join password and maps, "
                                                   "a line each (entrypoint.sh)")
    prepare.add_argument("--config", default="/config")
    args = parser.parse_args()

    if args.command == "apply-pending":
        for change in apply_pending(os.path.join(args.config, "addons")):
            log(change)
        return 0

    if args.command == "settings":
        effective = Settings(os.path.join(args.config, "settings")).prepare(os.environ, os.environ.get("EXTRA_SETTINGS", ""))
        print(effective["name"])
        print(effective["maxPlayers"])
        print(effective["tickRate"])
        print(1 if effective["lobby"] else 0)
        print(effective["joinPassword"])
        print(" ".join(map_words(effective["maps"])))
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
        admin_password=admin_password,
        version=os.environ.get("OPENSWBF2_VERSION", ""),
        uploads=os.environ.get("MOD_UPLOADS", "1") == "1",
        rate=env_int("MOD_RATE", 50) * 125000,
        restart=restart,
        game=RconGame(game_port, admin_password),
    )
    server = Server((args.host, args.port), service)
    signal.signal(signal.SIGTERM, lambda *_: sys.exit(0))
    service.watch.start_thread()
    folders = service.library.scan()
    log(f"sharing {len(folders)} mods on TCP {args.port}: {' '.join(f.name for f in folders) or 'none yet'}")
    try:
        server.serve_forever()
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
