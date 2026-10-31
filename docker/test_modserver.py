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
        self.service = modserver.ModService(
            self.addons, self.config, admin_password=ADMIN, server_name="Test server", version="0.2.2",
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


if __name__ == "__main__":
    unittest.main(verbosity=2)
