#!/usr/bin/env python3
"""Remote admin for an OpenSWBF2 dedicated server, the protocol of tests/rcon.ps1 (NetAdmin.cpp).

  rcon /status                      logs in with ADMIN_PASSWORD (or /config/admin-password)
  rcon "/password newpass" /status  several commands, one session
  rcon --ping                       just checks the server answers (no login): the healthcheck

Requests are datagrams of type 28: group (u16), index 0, count 1, the command byte, 0 and the
session key; 'J' lines add a length byte and up to 63 ASCII characters. Replies are type 29: the
key, then [1, length, text] per line and a 0, maybe over several datagrams of one group. The key
rotates, so each reply's key is used for the next request. A fixed local port keeps repeated calls
in one admin session: the server keeps only 3.
"""
import argparse
import os
import random
import socket
import sys


class Rcon:
    def __init__(self, host, port, timeout, local_port):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        if local_port:
            try:
                self.sock.bind(("127.0.0.1" if host in ("127.0.0.1", "localhost") else "", local_port))
            except OSError:
                pass  # another call holds it: an unbound socket takes any port
        self.sock.settimeout(timeout)
        self.sock.connect((host, port))
        self.group = random.randint(1, 59999)
        self.key = 0

    def send(self, command, payload=b""):
        group = self.group & 0xFFFF
        self.group += 1
        header = bytes([28, group & 0xFF, group >> 8, 0, 1, ord(command), 0, self.key])
        self.sock.send(header + payload)

    def receive(self):
        """One reply group's payload: the bytes after each datagram's 5-byte header, in order."""
        parts, count, wanted = {}, 1, None
        while True:
            data = self.sock.recv(2048)
            if len(data) < 5 or data[0] != 29:
                continue
            group = data[1] | (data[2] << 8)
            if wanted is None:
                wanted = group
            if group != wanted:
                continue
            parts[data[3]] = data[5:]
            count = max(1, data[4] & 0x7F)
            if len(parts) >= count:
                return b"".join(parts.get(i, b"") for i in range(count))

    def hello(self):
        self.send("s")
        payload = self.receive()
        self.key = payload[0] if payload else 0

    def join(self):
        """The heartbeat that gives this address an admin session."""
        self.send(")", b"\0")
        self.receive()

    def line(self, text):
        raw = text.encode("ascii", "replace")[:63]
        self.send("J", bytes([len(raw)]) + raw)
        payload = self.receive()
        if payload:
            self.key = payload[0]
        lines, i = [], 1
        while i + 1 < len(payload) and payload[i] == 1:
            length = payload[i + 1]
            lines.append(payload[i + 2 : i + 2 + length].decode("ascii", "replace"))
            i += 2 + length
        return lines


def admin_password(given):
    if given:
        return given
    if os.environ.get("ADMIN_PASSWORD"):
        return os.environ["ADMIN_PASSWORD"]
    try:
        with open("/config/admin-password", encoding="ascii") as file:
            return file.read().strip()
    except OSError:
        return ""


def main():
    port = int(os.environ.get("PORT", "3658"))
    parser = argparse.ArgumentParser(description="Remote admin for an OpenSWBF2 dedicated server.")
    parser.add_argument("commands", nargs="*", help='admin commands such as /status or "/password newpass"')
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=port)
    parser.add_argument("--password", help="the admin password (default: ADMIN_PASSWORD or /config/admin-password)")
    parser.add_argument("--ping", action="store_true", help="only check that the server answers")
    parser.add_argument("--timeout", type=float, default=3.0)
    args = parser.parse_args()

    local_port = args.port + 10000 if args.port + 10000 <= 65535 else 0
    rcon = Rcon(args.host, args.port, args.timeout, local_port)
    try:
        rcon.hello()
        if args.ping:
            print("ok")
            return 0
        if not args.commands:
            parser.error("give a command, such as /status")
        password = admin_password(args.password)
        if not password:
            print("no admin password: set ADMIN_PASSWORD or pass --password", file=sys.stderr)
            return 2
        rcon.join()
        for text in ["/login " + password] + args.commands:
            text = text.strip()
            if not text.startswith("/admin"):
                text = "/admin " + text
            for line in rcon.line(text):
                print(line)
        return 0
    except socket.timeout:
        print(f"no answer from {args.host}:{args.port}", file=sys.stderr)
        return 1
    except OSError as error:
        print(f"{args.host}:{args.port}: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
