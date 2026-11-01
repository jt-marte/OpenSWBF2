#!/usr/bin/env python3
"""Tests for modserver.py, with made-up mods in a temp folder: python3 docker/test_modserver.py"""
import hashlib
import http.client
import json
import os
import shutil
import socket
import sys
import tempfile
import threading
import time
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import modserver  # noqa: E402

modserver.log = lambda text: None

JOIN = "joinpw"
ADMIN = "Admin12345"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def make(root, folder, files):
    for path, data in files.items():
        full = os.path.join(root, folder, *path.split("/"))
        os.makedirs(os.path.dirname(full), exist_ok=True)
        with open(full, "wb") as file:
            file.write(data)


class ServiceCase(unittest.TestCase):
    join_password = JOIN

    def setUp(self):
        self.dir = tempfile.mkdtemp(prefix="modserver-test-")
        self.addons = os.path.join(self.dir, "addons")
        self.config = os.path.join(self.dir, "config")
        with open(os.path.join(self.dir, "secret.txt"), "wb") as file:
            file.write(b"top secret")
        self.lvl = os.urandom(300_000)
        make(self.addons, "BF1", {
            "addme.script": b"addme",
            "data/_lvl_pc/bf1.lvl": self.lvl,
            "Readme.txt": b"hello",
            "setup.exe": b"MZ program",
            "tools/run.BAT": b"@echo off",
        })
        make(self.addons, "AAA-v1.3patch", {"addme.script": b"the patch"})
        make(self.addons, "NoAddme", {"x.lvl": b"x"})
        self.restarts = []
        self.players = [3]
        self.start()

    def start(self, **settings):
        settings.setdefault("join_password", self.join_password)
        if not os.path.exists(os.path.join(self.config, "settings", "admin.json")):
            modserver.Settings(os.path.join(self.config, "settings")).store({"name": "Test server"})
        self.service = modserver.ModService(
            self.addons, self.config, admin_password=ADMIN, version="0.2.2",
            restart=lambda: self.restarts.append(time.monotonic()), players=lambda: self.players[0], **settings)
        self.server = modserver.Server(("127.0.0.1", 0), self.service)
        self.port = self.server.server_address[1]
        self.thread = threading.Thread(target=self.server.serve_forever, args=(0.05,), daemon=True)
        self.thread.start()

    def stop(self):
        self.server.shutdown()
        self.server.server_close()

    def tearDown(self):
        self.stop()
        shutil.rmtree(self.dir, ignore_errors=True)

    def request(self, method, path, body=None, password=JOIN, headers=None):
        connection = http.client.HTTPConnection("127.0.0.1", self.port, timeout=10)
        headers = dict(headers or {})
        if password:
            headers["Authorization"] = "Bearer " + password
        if isinstance(body, (dict, list)):
            body = json.dumps(body).encode()
            headers["Content-Type"] = "application/json"
        try:
            connection.request(method, path, body=body, headers=headers)
            response = connection.getresponse()
            return response.status, response.headers, response.read()
        finally:
            connection.close()

    def manifest(self, password=JOIN):
        status, _, data = self.request("GET", "/mods.json", password=password)
        self.assertEqual(status, 200, data)
        return json.loads(data)

    def upload(self, folder, files, password=ADMIN):
        listing = {"files": [{"path": p, "size": len(d), "sha256": sha(d)} for p, d in files.items()]}
        status, _, data = self.request("POST", f"/upload/{folder}/begin", listing, password=password)
        if status != 200:
            return status, json.loads(data)
        for path, content in files.items():
            if path in json.loads(data)["have"]:
                continue
            status, _, reply = self.request("PUT", f"/upload/{folder}/{path}", content, password=password,
                                            headers={"X-Sha256": sha(content)})
            if status != 200:
                return status, json.loads(reply)
        status, _, data = self.request("POST", f"/upload/{folder}/commit", listing, password=password)
        return status, json.loads(data)


class ManifestTests(ServiceCase):
    def test_lists_mod_folders_with_hashes_and_no_programs(self):
        reply = self.manifest()
        self.assertEqual(reply["schema"], 1)
        self.assertEqual(reply["server"], "Test server")
        self.assertEqual(reply["openswbf2"], "0.2.2")
        self.assertNotIn("pending", reply)
        self.assertEqual([f["name"] for f in reply["folders"]], ["BF1"])
        bf1 = reply["folders"][0]
        files = {f["path"]: f for f in bf1["files"]}
        self.assertEqual(set(files), {"addme.script", "data/_lvl_pc/bf1.lvl", "Readme.txt"})
        self.assertEqual(files["data/_lvl_pc/bf1.lvl"]["sha256"], sha(self.lvl))
        self.assertEqual(files["data/_lvl_pc/bf1.lvl"]["size"], len(self.lvl))
        self.assertEqual(bf1["size"], sum(f["size"] for f in bf1["files"]))
        self.assertEqual(bf1["source"], "server")

    def test_admin_sees_pending(self):
        self.assertEqual(self.manifest(ADMIN)["pending"], [])

    def test_hashes_each_file_once(self):
        self.manifest()
        self.assertTrue(os.path.exists(os.path.join(self.config, "mod-hashes.json")))
        calls = []
        real = modserver.sha256_file
        modserver.sha256_file = lambda path: calls.append(path) or real(path)
        try:
            self.manifest()
            self.assertEqual(calls, [])
            make(self.addons, "BF1", {"Readme.txt": b"hello again"})
            self.manifest()
            self.assertEqual(len(calls), 1)
            # A new service reads the saved cache instead of hashing again
            self.stop()
            calls.clear()
            self.start()
            self.manifest()
            self.assertEqual(calls, [])
        finally:
            modserver.sha256_file = real

    def test_case_collisions_are_left_out(self):
        make(self.addons, "Case", {"addme.script": b"a", "Map.lvl": b"1"})
        try:
            make(self.addons, "Case", {"map.lvl": b"2"})
        except OSError:
            self.skipTest("this file system can't hold both names")
        if len(os.listdir(os.path.join(self.addons, "Case"))) < 3:
            self.skipTest("this file system ignores case")
        folder = next(f for f in self.manifest()["folders"] if f["name"] == "Case")
        self.assertEqual(len([f for f in folder["files"] if f["path"].lower() == "map.lvl"]), 1)


class AuthTests(ServiceCase):
    def test_needs_the_join_or_admin_password(self):
        status, headers, data = self.request("GET", "/mods.json", password=None)
        self.assertEqual(status, 401)
        self.assertEqual(json.loads(data)["error"], "password")
        self.assertIn("Bearer", headers["WWW-Authenticate"])
        self.assertEqual(self.request("GET", "/mods.json", password="wrong")[0], 401)
        self.assertEqual(self.request("GET", "/mods.json", password=JOIN)[0], 200)
        self.assertEqual(self.request("GET", "/mods.json", password=ADMIN)[0], 200)
        self.assertEqual(self.request("GET", "/mods/BF1/addme.script", password=None)[0], 401)

    def test_open_without_a_join_password(self):
        self.stop()
        self.start(join_password="")
        self.assertEqual(self.request("GET", "/mods.json", password=None)[0], 200)
        self.assertEqual(self.request("GET", "/mods/BF1/addme.script", password=None)[0], 200)
        # A wrong admin password still says so, for the admin screen
        self.assertEqual(self.request("GET", "/mods.json", password="wrong")[0], 401)

    def test_writes_need_the_admin_password(self):
        self.assertEqual(self.request("POST", "/admin/restart", password=JOIN)[0], 403)
        self.assertEqual(self.request("POST", "/admin/restart", password=None)[0], 401)
        self.assertEqual(self.request("DELETE", "/upload/Mine", password=JOIN)[0], 403)
        self.assertEqual(self.restarts, [])

    def test_locks_out_after_five_wrong_passwords(self):
        for _ in range(5):
            self.assertEqual(self.request("GET", "/mods.json", password="guess")[0], 401)
        self.assertEqual(self.request("GET", "/mods.json", password=ADMIN)[0], 429)


class PathTests(ServiceCase):
    BAD = [
        "/mods/BF1/../BF1/addme.script",
        "/mods/BF1/%2e%2e/%2e%2e/secret.txt",
        "/mods/BF1/..%2f..%2fsecret.txt",
        "/mods/BF1/%2E%2E/NoAddme/x.lvl",
        "/mods/BF1/data%5c_lvl_pc%5cbf1.lvl",
        "/mods/BF1/data\\_lvl_pc\\bf1.lvl",
        "//secret.txt",
        "/mods//BF1/addme.script",
        "/mods/BF1/C:/secret.txt",
        "/mods/BF1/addme.script%00.txt",
        "/mods/BF1/addme.script::$DATA",
        "/mods/%2e%2e/secret.txt",
        "/mods/BF1/",
        "/mods/" + "a/" * 600 + "x",
    ]

    def test_traversal_never_leaves_the_folder(self):
        for path in self.BAD:
            with self.subTest(path=path):
                status, _, data = self.request("GET", path, password=ADMIN)
                self.assertIn(status, (400, 404))
                self.assertNotIn(b"top secret", data)

    def test_only_listed_files_are_served(self):
        for path in ["/mods/BF1/setup.exe", "/mods/BF1/tools/run.BAT", "/mods/AAA-v1.3patch/addme.script",
                     "/mods/NoAddme/x.lvl", "/mods/bf1/addme.script", "/mods/BF1/ADDME.SCRIPT", "/mods.JSON"]:
            with self.subTest(path=path):
                self.assertEqual(self.request("GET", path)[0], 404)

    def test_links_out_of_the_folder_are_left_out(self):
        link = os.path.join(self.addons, "BF1", "escape.txt")
        try:
            os.symlink(os.path.join(self.dir, "secret.txt"), link)
        except (OSError, NotImplementedError):
            self.skipTest("can't make symlinks here")
        paths = [f["path"] for f in self.manifest()["folders"][0]["files"]]
        self.assertNotIn("escape.txt", paths)
        self.assertEqual(self.request("GET", "/mods/BF1/escape.txt")[0], 404)

    def test_split_url(self):
        self.assertEqual(modserver.split_url("/mods/BF1/data/a%20b.lvl?x=1"), ["mods", "BF1", "data", "a b.lvl"])
        for path in ["mods.json", "/a/../b", "/a/%2e", "/a/b%2fc", "/a/b%5cc", "/a/C:", "/a/con", "/a/NUL.txt",
                     "/a/b.", "/a/b ", "/a/%ff", "/a/%01"]:
            with self.subTest(path=path):
                self.assertIsNone(modserver.split_url(path))


class DownloadTests(ServiceCase):
    URL = "/mods/BF1/data/_lvl_pc/bf1.lvl"

    def test_whole_file(self):
        status, headers, data = self.request("GET", self.URL)
        self.assertEqual(status, 200)
        self.assertEqual(data, self.lvl)
        self.assertEqual(headers["Accept-Ranges"], "bytes")
        self.assertEqual(headers["ETag"], f'"{sha(self.lvl)}"')

    def test_ranges(self):
        size = len(self.lvl)
        cases = [
            ("bytes=100-199", 206, f"bytes 100-199/{size}", self.lvl[100:200]),
            ("bytes=299000-", 206, f"bytes 299000-{size - 1}/{size}", self.lvl[299000:]),
            ("bytes=-500", 206, f"bytes {size - 500}-{size - 1}/{size}", self.lvl[-500:]),
            ("bytes=0-999999", 206, f"bytes 0-{size - 1}/{size}", self.lvl),
            ("bytes=0-1,5-6", 200, None, self.lvl),
            ("items=0-1", 200, None, self.lvl),
        ]
        for value, want_status, want_range, want_body in cases:
            with self.subTest(range=value):
                status, headers, data = self.request("GET", self.URL, headers={"Range": value})
                self.assertEqual(status, want_status)
                self.assertEqual(headers.get("Content-Range"), want_range)
                self.assertEqual(data, want_body)

    def test_unsatisfiable_range(self):
        status, headers, data = self.request("GET", self.URL, headers={"Range": f"bytes={len(self.lvl)}-"})
        self.assertEqual(status, 416)
        self.assertEqual(headers["Content-Range"], f"bytes */{len(self.lvl)}")
        self.assertEqual(data, b"")

    def test_if_range_for_another_version_sends_it_all(self):
        status, _, data = self.request("GET", self.URL, headers={"Range": "bytes=10-", "If-Range": '"' + "0" * 64 + '"'})
        self.assertEqual(status, 200)
        self.assertEqual(data, self.lvl)
        status, _, data = self.request("GET", self.URL, headers={"Range": "bytes=10-", "If-Range": f'"{sha(self.lvl)}"'})
        self.assertEqual((status, data), (206, self.lvl[10:]))

    def test_head(self):
        status, headers, data = self.request("HEAD", self.URL)
        self.assertEqual(status, 200)
        self.assertEqual(headers["Content-Length"], str(len(self.lvl)))
        self.assertEqual(data, b"")

    def test_keep_alive_serves_several_files(self):
        connection = http.client.HTTPConnection("127.0.0.1", self.port, timeout=10)
        try:
            for path in ["/mods/BF1/addme.script", "/mods/BF1/Readme.txt", self.URL]:
                connection.request("GET", path, headers={"Authorization": "Bearer " + JOIN})
                response = connection.getresponse()
                self.assertEqual(response.status, 200)
                response.read()
        finally:
            connection.close()

    def test_speed_cap(self):
        bucket = modserver.Bucket(400_000)
        start = time.monotonic()
        for _ in range(3):
            bucket.take(100_000)
        self.assertGreaterEqual(time.monotonic() - start, 0.4)
        free = modserver.Bucket(0)
        start = time.monotonic()
        free.take(10 ** 9)
        self.assertLess(time.monotonic() - start, 0.1)

    def test_connection_limit_per_address(self):
        idle = []
        try:
            for _ in range(modserver.Server.MAX_PER_ADDRESS):
                sock = socket.create_connection(("127.0.0.1", self.port))
                idle.append(sock)
            time.sleep(0.2)
            extra = socket.create_connection(("127.0.0.1", self.port))
            extra.settimeout(5)
            extra.sendall(b"GET /mods.json HTTP/1.1\r\nHost: x\r\n\r\n")
            try:
                self.assertEqual(extra.recv(100), b"")
            except ConnectionResetError:
                pass
            extra.close()
        finally:
            for sock in idle:
                sock.close()
        for _ in range(50):
            time.sleep(0.1)
            if not self.server.connections:
                break
        self.assertEqual(self.request("GET", "/mods.json")[0], 200)


class UploadTests(ServiceCase):
    NEW = {"addme.script": b"new addme", "data/_lvl_pc/new.lvl": os.urandom(5000)}

    def test_upload_waits_for_the_restart(self):
        status, reply = self.upload("NEW", self.NEW)
        self.assertEqual((status, reply), (200, {"pending": True}))
        self.assertTrue(os.path.isdir(os.path.join(self.config, "addons", ".pending", "NEW")))
        self.assertEqual([f["name"] for f in self.manifest()["folders"]], ["BF1"])
        pending = self.manifest(ADMIN)["pending"]
        self.assertEqual(pending, [{"name": "NEW", "action": "add", "size": sum(len(d) for d in self.NEW.values())}])

        self.assertEqual(modserver.apply_pending(os.path.join(self.config, "addons")), ["added NEW"])
        folders = {f["name"]: f for f in self.manifest()["folders"]}
        self.assertEqual(set(folders), {"BF1", "NEW"})
        self.assertEqual(folders["NEW"]["source"], "uploaded")
        self.assertEqual(self.request("GET", "/mods/NEW/data/_lvl_pc/new.lvl")[2], self.NEW["data/_lvl_pc/new.lvl"])
        self.assertEqual(self.manifest(ADMIN)["pending"], [])

    def test_a_refused_body_still_gets_its_reply(self):
        # Closing with the body unread made the client's TCP reset the connection, often before it
        # read the reply. A small body is dropped and the connection stays usable.
        for _ in range(20):
            connection = http.client.HTTPConnection("127.0.0.1", self.port, timeout=10)
            try:
                connection.request("PUT", "/upload/NEW/addme.script", body=os.urandom(200000),
                                   headers={"Authorization": "Bearer " + JOIN, "X-Sha256": "0" * 64})
                response = connection.getresponse()
                self.assertEqual((response.status, json.loads(response.read())),
                                 (403, {"error": "that needs the admin password"}))
                connection.request("GET", "/mods.json", headers={"Authorization": "Bearer " + JOIN})
                response = connection.getresponse()
                self.assertEqual(response.status, 200)
                response.read()
            finally:
                connection.close()
        # A body too big to drop: the reply comes first, then the connection closes
        status, headers, data = self.request("PUT", "/upload/NEW/big.lvl", os.urandom(3 << 20), password=JOIN,
                                             headers={"X-Sha256": "0" * 64})
        self.assertEqual((status, json.loads(data)), (403, {"error": "that needs the admin password"}))
        self.assertEqual(headers["Connection"], "close")

    def test_begin_reports_files_already_there(self):
        listing = {"files": [{"path": p, "size": len(d), "sha256": sha(d)} for p, d in self.NEW.items()]}
        self.request("PUT", "/upload/NEW/addme.script", self.NEW["addme.script"], password=ADMIN,
                     headers={"X-Sha256": sha(self.NEW["addme.script"])})
        status, _, data = self.request("POST", "/upload/NEW/begin", listing, password=ADMIN)
        self.assertEqual((status, json.loads(data)), (200, {"have": ["addme.script"]}))
        # A changed file no longer counts
        listing["files"][0]["sha256"] = "1" * 64
        status, _, data = self.request("POST", "/upload/NEW/begin", listing, password=ADMIN)
        self.assertEqual(json.loads(data), {"have": []})

    def test_replacing_and_removing(self):
        self.upload("NEW", self.NEW)
        modserver.apply_pending(os.path.join(self.config, "addons"))
        changed = dict(self.NEW, **{"addme.script": b"version 2"})
        self.assertEqual(self.upload("NEW", changed)[0], 200)
        modserver.apply_pending(os.path.join(self.config, "addons"))
        self.assertEqual(self.request("GET", "/mods/NEW/addme.script")[2], b"version 2")

        status, _, data = self.request("DELETE", "/upload/NEW", password=ADMIN)
        self.assertEqual((status, json.loads(data)), (200, {"pending": True}))
        self.assertEqual(self.manifest(ADMIN)["pending"], [{"name": "NEW", "action": "remove"}])
        self.assertEqual(modserver.apply_pending(os.path.join(self.config, "addons")), ["removed NEW"])
        self.assertEqual([f["name"] for f in self.manifest()["folders"]], ["BF1"])
        self.assertEqual(self.request("DELETE", "/upload/NEW", password=ADMIN)[0], 404)

    def test_refuses_programs(self):
        status, _, _ = self.request("PUT", "/upload/NEW/setup.exe", b"MZ", password=ADMIN, headers={"X-Sha256": sha(b"MZ")})
        self.assertEqual(status, 403)
        status, reply = self.upload("NEW", dict(self.NEW, **{"Tools/Install.CMD": b"x"}))
        self.assertEqual(status, 403, reply)
        self.assertFalse(os.path.exists(os.path.join(self.config, "addons", ".pending", "NEW")))

    def test_refuses_a_damaged_file(self):
        status, _, data = self.request("PUT", "/upload/NEW/addme.script", b"what arrived", password=ADMIN,
                                       headers={"X-Sha256": sha(b"what was sent")})
        self.assertEqual(status, 400, data)
        self.assertFalse(os.path.exists(os.path.join(self.config, "addons", ".staging", "NEW", "addme.script")))

    def test_commit_needs_every_file(self):
        listing = {"files": [{"path": p, "size": len(d), "sha256": sha(d)} for p, d in self.NEW.items()]}
        self.request("POST", "/upload/NEW/begin", listing, password=ADMIN)
        status, _, data = self.request("POST", "/upload/NEW/commit", listing, password=ADMIN)
        self.assertEqual(status, 409)
        self.assertEqual(sorted(json.loads(data)["missing"]), sorted(self.NEW))

    def test_refuses_bad_names_and_lists(self):
        cases = [
            ("BF1", self.NEW, 409),              # the server's own
            ("bf1", self.NEW, 409),
            ("AAA-v1.3patch", self.NEW, 403),    # never the 1.3 patch
            (".staging", self.NEW, 400),
            ("CON", self.NEW, 400),
            ("NEW", {"data/x.lvl": b"no addme"}, 400),
            ("NEW", {"addme.script": b"a", "../escape.lvl": b"x"}, 400),
        ]
        for folder, files, want in cases:
            with self.subTest(folder=folder, files=list(files)):
                self.assertEqual(self.upload(folder, files)[0], want)

    def test_names_differing_only_in_case(self):
        self.upload("NEW", self.NEW)
        self.assertEqual(self.upload("New", self.NEW)[0], 409)

    def test_needs_the_admin_password_and_uploads_on(self):
        self.assertEqual(self.upload("NEW", self.NEW, password=JOIN)[0], 403)
        self.stop()
        self.start(uploads=False)
        self.assertEqual(self.upload("NEW", self.NEW)[0], 403)


class RestartTests(ServiceCase):
    def test_restart_now_once_a_minute(self):
        status, _, data = self.request("POST", "/admin/restart?when=now", password=ADMIN)
        self.assertEqual((status, json.loads(data)), (200, {"restart": "none"}))
        self.assertEqual(len(self.restarts), 1)
        self.assertEqual(self.request("POST", "/admin/restart?when=now", password=ADMIN)[0], 429)
        self.assertEqual(len(self.restarts), 1)

    def test_restart_when_empty(self):
        self.service.restarter.interval = 0.05
        status, _, data = self.request("POST", "/admin/restart?when=empty", password=ADMIN)
        self.assertEqual(json.loads(data), {"restart": "when-empty"})
        self.assertEqual(self.manifest()["restart"], "when-empty")
        time.sleep(0.3)
        self.assertEqual(self.restarts, [])
        self.players[0] = 0
        for _ in range(40):
            if self.restarts:
                break
            time.sleep(0.05)
        self.assertEqual(len(self.restarts), 1)
        self.assertEqual(self.manifest()["restart"], "none")

    def test_cancel_and_unknown_players(self):
        self.service.restarter.interval = 0.05
        self.players[0] = None  # the game isn't answering rcon
        self.request("POST", "/admin/restart?when=empty", password=ADMIN)
        time.sleep(0.3)
        self.assertEqual(self.restarts, [])
        status, _, data = self.request("DELETE", "/admin/restart", password=ADMIN)
        self.assertEqual(json.loads(data), {"restart": "none"})
        self.players[0] = 0
        time.sleep(0.3)
        self.assertEqual(self.restarts, [])
        self.assertEqual(self.request("POST", "/admin/restart?when=later", password=ADMIN)[0], 400)


class FakeClock:
    def __init__(self):
        self.now = 1000.0

    def __call__(self):
        return self.now


class FakeGame:
    """The game as rcon reports it; /start and /lobby change its phase as the server does."""

    def __init__(self):
        self.status = {"phase": "lobby", "map": "BS1g_con", "next": "geo1c_con", "players": 0, "teams": 0}
        self.players = []
        self.commands = []
        self.up = True

    def state(self):
        if not self.up:
            raise OSError("no answer")
        return dict(self.status)

    def names(self):
        return list(self.players)

    def command(self, text):
        self.commands.append(text)
        if text.startswith("/start"):
            self.status["phase"] = "warmup"
        elif text.startswith("/lobby"):
            self.status["phase"] = "lobby"
        return ["ok"]


class LobbyTests(ServiceCase):
    def start(self, **settings):
        self.clock = FakeClock()
        self.game = FakeGame()
        super().start(game=self.game, clock=self.clock, **settings)

    def join(self, name, password=JOIN):
        status, _, data = self.request("POST", "/lobby/join", {"name": name}, password=password)
        self.assertEqual(status, 200, data)
        return json.loads(data)["token"]

    def update(self, token, state, progress=0):
        status, _, data = self.request("POST", "/lobby/update", {"token": token, "state": state, "progress": progress})
        self.assertEqual(status, 200, data)

    def status(self, token=None, password=JOIN):
        path = "/status" + (f"?token={token}" if token else "")
        status, _, data = self.request("GET", path, password=password)
        self.assertEqual(status, 200, data)
        return json.loads(data)

    def match(self, what, method="POST", password=ADMIN):
        path = "/admin/match" + (f"?do={what}" if what else "")
        status, _, data = self.request(method, path, password=password)
        return status, json.loads(data)

    def test_join_update_leave_and_expiry(self):
        token = self.join("Pat")
        reply = self.status(token)
        self.assertEqual(reply["lobby"], [{"name": "Pat", "state": "downloading", "progress": 0}])
        self.assertTrue(reply["inLobby"])
        self.assertEqual(reply["server"], "Test server")
        self.update(token, "downloading", 45)
        self.assertEqual(self.status(token)["lobby"][0]["progress"], 45)
        self.update(token, "waiting", 100)
        self.assertEqual(self.status(token)["lobby"][0]["state"], "waiting")
        self.request("POST", "/lobby/leave", {"token": token})
        self.assertEqual(self.status()["lobby"], [])
        self.assertFalse(self.status(token)["inLobby"])

        self.join("Rex")
        self.clock.now += modserver.Lobby.EXPIRE + 1
        self.assertEqual(self.status()["lobby"], [])
        self.assertEqual(self.request("POST", "/lobby/update", {"token": "nope", "state": "waiting"})[0], 404)

    def test_names_and_passwords_are_checked(self):
        for name in ["", "x" * 16, 'a"b', "a\\b", " pad", "pad ", "[NULL]", 7, None]:
            with self.subTest(name=name):
                self.assertEqual(self.request("POST", "/lobby/join", {"name": name})[0], 400)
        self.assertEqual(self.request("POST", "/lobby/join", {"name": "Pat"}, password=None)[0], 401)
        self.assertEqual(self.request("GET", "/status", password=None)[0], 401)
        self.assertEqual(self.request("POST", "/lobby/update", {"token": "x", "state": "playing"})[0], 400)

    def test_start_loads_everyones_game_then_starts(self):
        self.update(self.join("Pat"), "waiting", 100)
        self.update(self.join("Rex"), "waiting", 100)
        self.join("Slow")  # still downloading: doesn't hold the start
        status, reply = self.match("start")
        self.assertEqual((status, reply["start"]), (200, "loading"))
        self.assertEqual(reply["startIn"], modserver.GameWatch.DEADLINE)
        self.assertEqual(self.status()["start"], "loading")

        self.game.status.update(players=1, teams=1)
        self.service.watch.step()
        self.assertEqual(self.game.commands, [])
        self.game.status.update(players=2, teams=1)  # the second is still loading the map
        self.service.watch.step()
        self.assertEqual(self.game.commands, [])
        self.game.status["teams"] = 2
        self.game.players = ["Pat", "Player 2"]
        self.service.watch.step()
        self.assertEqual(self.game.commands, ["/start 5"])
        reply = self.status()
        self.assertEqual((reply["start"], reply["phase"]), ("none", "warmup"))

    def test_start_waits_sixty_seconds_at_most(self):
        self.update(self.join("Pat"), "waiting")
        self.update(self.join("Rex"), "waiting")
        self.match("start")
        self.game.status["players"] = 1
        self.service.watch.step()
        self.assertEqual(self.game.commands, [])
        self.clock.now += modserver.GameWatch.DEADLINE + 1
        self.service.watch.step()
        self.assertEqual(self.game.commands, ["/start 5"])

    def test_connected_but_not_on_a_team_starts_after_a_while(self):
        self.update(self.join("Pat"), "waiting")
        self.match("start")
        self.game.status.update(players=1, teams=0)  # on the team select screen: auto-assign is off
        self.service.watch.step()
        self.clock.now += modserver.GameWatch.SETTLE - 1
        self.service.watch.step()
        self.assertEqual(self.game.commands, [])
        self.clock.now += 2
        self.service.watch.step()
        self.assertEqual(self.game.commands, ["/start 5"])

    def test_start_with_nobody_waiting_is_at_once(self):
        self.join("Slow")
        status, reply = self.match("start")
        self.assertEqual((status, reply["start"]), (200, "none"))
        self.assertEqual(self.game.commands, ["/start 5"])

    def test_start_only_from_the_lobby(self):
        self.game.status["phase"] = "playing"
        status, reply = self.match("start")
        self.assertEqual(status, 409)
        self.game.up = False
        status, reply = self.match("start")
        self.assertEqual((status, reply["error"]), (409, "the game doesn't answer"))
        self.assertEqual(self.status()["phase"], "offline")

    def test_a_start_from_elsewhere_ends_the_loading(self):
        self.update(self.join("Pat"), "waiting")
        self.match("start")
        self.game.status["phase"] = "warmup"  # an admin typed /admin /start in the game
        self.service.watch.step()
        self.assertEqual(self.status()["start"], "none")
        self.assertEqual(self.game.commands, [])

    def test_cancel_and_back_to_lobby(self):
        self.update(self.join("Pat"), "waiting")
        self.match("start")
        status, reply = self.match(None, method="DELETE")
        self.assertEqual((status, reply["start"]), (200, "none"))
        self.game.status["players"] = 5
        self.service.watch.step()
        self.assertEqual(self.game.commands, [])

        self.game.status["phase"] = "playing"
        self.assertEqual(self.match("lobby")[0], 200)
        self.assertEqual(self.game.commands, ["/lobby now"])
        self.assertEqual(self.match("later")[0], 400)

    def test_match_control_needs_the_admin_password(self):
        self.assertEqual(self.match("start", password=JOIN)[0], 403)
        self.assertEqual(self.match("lobby", password=None)[0], 401)
        self.assertEqual(self.game.commands, [])

    def test_status_names_whos_in_the_game_while_anyone_waits(self):
        self.game.players = ["Rex", "Player 2"]
        self.service.watch.step()
        self.assertEqual(self.status()["inGame"], [])  # nobody waits: names aren't asked for
        self.join("Pat")
        self.service.watch.step()
        reply = self.status()
        self.assertEqual(reply["inGame"], ["Rex", "Player 2"])
        self.assertEqual(reply["map"], "BS1g_con")

    def test_restart_when_empty_reads_the_watch(self):
        self.game.status["players"] = 2
        self.assertEqual(self.service.watch.players(), 2)
        self.game.up = False
        self.clock.now += 10
        self.assertIsNone(self.service.watch.players())


def cfg_lines(**changed):
    """ServerSettings.cfg for the defaults with some game options changed."""
    options = dict(modserver.DEFAULTS["options"], **changed)
    return "".join(f"/{name} {options[name]}\r\n" for name, _, _ in modserver.OPTIONS)


class SettingsTests(ServiceCase):
    """The server's settings: the defaults, with what the launcher's admin screen saved over them.
    The fixture saved the name "Test server"."""

    def setUp(self):
        super().setUp()
        self.settings = self.service.settings
        self.settings.prepare({})

    def put(self, body, password=ADMIN):
        status, _, data = self.request("PUT", "/admin/settings", body, password=password)
        return status, json.loads(data)

    def get(self):
        status, _, data = self.request("GET", "/admin/settings", password=ADMIN)
        self.assertEqual(status, 200, data)
        return json.loads(data)

    def read_cfg(self):
        with open(self.settings.path("ServerSettings.cfg"), "rb") as file:
            return file.read().decode()

    def test_saved_settings_are_used_at_the_next_start(self):
        status, reply = self.put({"maps": [{"mission": "tat2g_con", "tickets1": 150}, {"mission": "geo1c_con"}],
                                  "options": {"difficulty": 4}})
        self.assertEqual(status, 200, reply)
        self.assertEqual(reply["settings"]["maps"],
                         [{"mission": "tat2g_con", "tickets1": 150, "tickets2": 150}, {"mission": "geo1c_con"}])
        self.assertEqual(reply["settings"]["options"], dict(modserver.DEFAULTS["options"], difficulty=4))
        self.assertEqual(reply["saved"], ["maps", "name", "options.difficulty"])
        self.assertEqual(reply["defaults"], modserver.DEFAULTS)
        self.assertFalse(reply["restartNeeded"])

        effective = self.settings.prepare({}, "/bots 3\n\n/awards 0")
        self.assertEqual(modserver.map_words(effective["maps"]), ["tat2g_con", "150", "150", "geo1c_con"])
        self.assertEqual(self.read_cfg(), cfg_lines(difficulty=4) + "/bots 3\r\n/awards 0\r\n")
        self.assertEqual(self.get()["saved"], ["maps", "name", "options.difficulty"])

    def test_a_new_server_starts_with_the_defaults(self):
        settings = modserver.Settings(os.path.join(self.dir, "fresh"))
        self.assertIsNone(settings.saved())
        effective = settings.prepare({})
        self.assertEqual(effective, modserver.DEFAULTS)
        self.assertEqual(modserver.map_words(effective["maps"])[:2], ["geo1c_con", "kas2g_con"])
        self.assertEqual((effective["options"]["conaiperteam"], effective["options"]["difficulty"]), (32, 3))
        self.assertEqual(settings.saved(), {})

    def test_an_old_compose_yaml_is_taken_once(self):
        settings = modserver.Settings(os.path.join(self.dir, "upgraded"))
        old = {"SERVER_NAME": 'My "box"', "MAPS": "tat2g_con 150 150 hot1g_con", "BOTS_PER_TEAM": "8",
               "LOBBY": "yes", "DIFFICULTY": "9", "HERO_UNLOCK": "4", "TICK_RATE": "45", "MAX_PLAYERS": "99",
               "JOIN_PASSWORD": "pw", "HEROES": "off", "MOD_RATE": "10"}
        effective = settings.prepare(old)
        self.assertEqual((effective["name"], effective["lobby"], effective["joinPassword"]), ("My box", True, "pw"))
        self.assertEqual(modserver.map_words(effective["maps"]), ["tat2g_con", "150", "150", "hot1g_con"])
        # As the old entrypoint took them: clamped, on/off, the first choice for anything else
        self.assertEqual(effective["maxPlayers"], 64)
        self.assertEqual(effective["tickRate"], 60)
        options = effective["options"]
        self.assertEqual([options[n] for n in ("conaiperteam", "ctfaiperteam", "assaiperteam", "eliaiperteam")], [8] * 4)
        self.assertEqual((options["difficulty"], options["hrunlock"], options["heroes"]), (4, 3, 0))
        # Later, compose.yaml is only named in the log
        again = settings.prepare(dict(old, MAPS="nab2c_con"))
        self.assertEqual(again["maps"], effective["maps"])

    def test_name_limits_and_lobby_wait_for_a_restart(self):
        status, reply = self.put({"name": "  Friday Night  ", "maxPlayers": 16, "tickRate": 120, "lobby": True})
        self.assertEqual(status, 200, reply)
        self.assertTrue(reply["restartNeeded"])
        self.assertEqual(self.manifest()["server"], "Test server")
        effective = self.settings.prepare({})
        self.assertEqual((effective["name"], effective["maxPlayers"], effective["tickRate"], effective["lobby"]),
                         ("Friday Night", 16, 120, True))
        self.assertFalse(self.get()["restartNeeded"])
        self.assertEqual(self.manifest()["server"], "Friday Night")

    def test_downloads_take_the_running_games_join_password(self):
        self.stop()
        self.start(join_password=None)  # follows the game's, as in the container
        self.assertEqual(self.request("GET", "/mods.json", password=None)[0], 200)
        status, reply = self.put({"joinPassword": "fresh1"})
        self.assertEqual((status, reply["settings"]["joinPassword"], reply["restartNeeded"]), (200, "fresh1", True))
        # Not until the game restarts with it: players couldn't join with it before then
        self.assertEqual(self.request("GET", "/mods.json", password=None)[0], 200)
        self.settings.prepare({})
        self.assertEqual(self.request("GET", "/mods.json", password=None)[0], 401)
        self.assertEqual(self.request("GET", "/mods.json", password="fresh1")[0], 200)
        self.assertEqual(self.request("DELETE", "/admin/settings", password=ADMIN)[0], 200)
        self.settings.prepare({})
        self.assertEqual(self.request("GET", "/mods.json", password=None)[0], 200)

    def test_back_to_the_defaults(self):
        self.put({"options": {"difficulty": 2}})
        status, _, data = self.request("DELETE", "/admin/settings", password=ADMIN)
        self.assertEqual(status, 200, data)
        self.assertEqual(json.loads(data)["saved"], [])
        # The file stays, so an old compose.yaml isn't taken again
        effective = self.settings.prepare({"DIFFICULTY": "4"})
        self.assertEqual(effective["options"]["difficulty"], 3)
        self.assertEqual(effective["name"], "OpenSWBF2 Server")

    def test_refuses_what_the_game_cant_take(self):
        long_list = [{"mission": "a" * 31} for _ in range(40)]
        for body in ({"name": 'Say "hi"'}, {"name": "   "}, {"name": "x" * 32}, {"maxPlayers": 99}, {"maxPlayers": True},
                     {"tickRate": 50}, {"maps": []}, {"maps": [{"mission": "bad name!"}]},
                     {"maps": [{"mission": "geo1c_con", "tickets1": "5"}]}, {"maps": long_list},
                     {"options": {"bots": 3}}, {"options": {"difficulty": 9}}, {"options": {"hrunlock": 4}},
                     {"options": {"heroes": True}}, {"joinPassword": "two words"}, {"joinPassword": "x" * 16},
                     {"options": {"conaiperteam": 65}}, {"lobby": 1}, {"port": 3700}, ["maps"]):
            status, reply = self.put(body)
            self.assertEqual(status, 400, body)
            self.assertIn("error", reply)
        self.assertEqual(self.get()["saved"], ["name"])

    def test_a_team_takes_up_to_64_bots(self):
        status, reply = self.put({"options": {"conaiperteam": 64, "ctfaiperteam": 0, "assaiperteam": 40}})
        self.assertEqual(status, 200, reply)
        options = self.settings.prepare({})["options"]
        self.assertEqual([options[n] for n in ("conaiperteam", "ctfaiperteam", "assaiperteam", "eliaiperteam")],
                         [64, 0, 40, 32])

    def test_a_hand_edited_file_cant_hand_the_game_a_bad_value(self):
        modserver.write_json(self.settings.path("admin.json"),
                             {"schema": 1, "values": {"maxPlayers": 500, "maps": "geo1c_con; rm -rf /", "tickRate": 30}})
        effective = self.settings.prepare({})
        self.assertEqual((effective["maxPlayers"], effective["tickRate"]), (32, 30))
        self.assertEqual(effective["maps"], modserver.DEFAULTS["maps"])

    def test_needs_the_admin_password(self):
        self.assertEqual(self.request("GET", "/admin/settings")[0], 403)
        self.assertEqual(self.put({"maxPlayers": 8}, password=JOIN)[0], 403)
        self.assertEqual(self.request("DELETE", "/admin/settings", password=None)[0], 401)

    def test_admins_see_that_the_server_saves_settings(self):
        self.assertTrue(self.manifest(ADMIN)["settings"])
        self.assertNotIn("settings", self.manifest())

    def test_the_servers_map_list(self):
        status, _, data = self.request("GET", "/missions.json")
        self.assertEqual((status, json.loads(data)), (404, {"error": "the server has no map list yet"}))
        listing = b'{"format": 1, "maps": []}'
        with open(self.settings.path("missions.json"), "wb") as file:
            file.write(listing)
        status, headers, data = self.request("GET", "/missions.json")
        self.assertEqual((status, data, headers["Content-Type"]), (200, listing, "application/json"))
        status, headers, data = self.request("HEAD", "/missions.json")
        self.assertEqual((status, data, headers["Content-Length"]), (200, b"", str(len(listing))))
        self.assertEqual(self.request("GET", "/missions.json", password=None)[0], 401)


class SettingsCommandTests(unittest.TestCase):
    def run_settings(self, config, **env):
        import subprocess
        variables = {k: v for k, v in os.environ.items() if k not in modserver.LEGACY}
        result = subprocess.run([sys.executable, "-I", modserver.__file__, "settings", "--config", config],
                                env=dict(variables, **env), capture_output=True, text=True, timeout=60)
        self.assertEqual(result.returncode, 0, result.stderr)
        return result

    def test_prints_the_next_start(self):
        config = tempfile.mkdtemp(prefix="modserver-settings-")
        self.addCleanup(shutil.rmtree, config, True)
        # First start of a server upgraded from an old compose.yaml
        result = self.run_settings(config, SERVER_NAME="Box", MAPS="geo1c_con 150 150 kas2g_con", EXTRA_SETTINGS="/awards 0")
        self.assertEqual(result.stdout.splitlines(), ["Box", "32", "60", "0", "", "geo1c_con 150 150 kas2g_con"])
        self.assertIn("Took SERVER_NAME, MAPS from compose.yaml into the server's settings, this once", result.stderr)
        with open(os.path.join(config, "settings", "ServerSettings.cfg"), "rb") as file:
            self.assertEqual(file.read().decode(), cfg_lines() + "/awards 0\r\n")
        # An admin saved some, and compose.yaml still sets MAPS
        settings = modserver.Settings(os.path.join(config, "settings"))
        settings.save({"joinPassword": "pw", "lobby": True, "maxPlayers": 20, "options": {"difficulty": 4}})
        result = self.run_settings(config, MAPS="nab2c_con")
        self.assertEqual(result.stdout.splitlines(), ["Box", "20", "60", "1", "pw", "geo1c_con 150 150 kas2g_con"])
        self.assertIn("compose.yaml still sets MAPS: not used any more", result.stderr)
        self.assertIn("The server's saved settings: joinPassword, lobby, maps, maxPlayers, name, 1 game option", result.stderr)

    def test_map_words(self):
        maps = modserver.parse_map_words("geo1c_con 150 150 kas2g_con nab2c_con 5")
        self.assertEqual(maps, [{"mission": "geo1c_con", "tickets1": 150, "tickets2": 150},
                                {"mission": "kas2g_con"}, {"mission": "nab2c_con"}, {"mission": "5"}])
        self.assertEqual(modserver.clean_maps([{"mission": "geo1c_con", "tickets2": 0}]),
                         [{"mission": "geo1c_con", "tickets1": 1, "tickets2": 1}])


class RconParsingTests(unittest.TestCase):
    def test_state(self):
        line = "state=lobby hold=1 players=1/2/4 time=-1 tickets=0/0 scores=0/0 map=BS1g_con next=geo1c_con"
        self.assertEqual(modserver.parse_state([line]),
                         {"phase": "lobby", "map": "BS1g_con", "next": "geo1c_con", "players": 4, "teams": 3})
        self.assertEqual(modserver.parse_state(["state=weird"])["phase"], "offline")

    def test_playerinfo(self):
        names, more = modserver.parse_playerinfo(["p 1 2 0 0 0 0 16 ", "p 3 1 1 10 2 1 40 Rex CT 7567", "more 13"])
        self.assertEqual((names, more), (["Player 1", "Rex CT 7567"], 13))
        self.assertEqual(modserver.parse_playerinfo(["end 2"]), ([], None))

    def test_reply_lines(self):
        payload = bytes([9, 1, 6]) + b"#state" + bytes([1, 2]) + b"ok" + bytes([0])
        self.assertEqual(modserver.reply_lines(payload), ["#state", "ok"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
