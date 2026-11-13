#!/usr/bin/env python3
"""First access to the reserved Conta Eternal accounts, from the launcher.

The migration reserved one Drasl account per player, each with a temporary password.
In the launcher a player types only their nickname and chooses a password: this service
keeps the temporary passwords, signs in with the player's one and sets the new password
through the player's own Drasl API token. Once the password changed, the temporary one
stops working, so the nickname is no longer a first access.

Listens on the LAN; NGINX publishes it under /api/accounts/.
"""

import argparse
import json
import os
import re
import sys
import threading
import urllib.error
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import unquote, urlsplit

NICKNAME = re.compile(r"[A-Za-z0-9_]{3,16}")
MIN_PASSWORD = 8
MAX_PASSWORD = 128
MAX_BODY_BYTES = 1024


class ApiError(Exception):
    def __init__(self, status, message):
        super().__init__(message)
        self.status = status
        self.message = message


class Drasl:
    """The few Drasl API calls this service needs."""

    def __init__(self, base_url):
        self.base_url = base_url.rstrip("/")

    def _call(self, method, path, body=None, token=None):
        request = urllib.request.Request(self.base_url + path, method=method,
                                         data=json.dumps(body).encode() if body is not None else None)
        request.add_header("Content-Type", "application/json")
        if token:
            request.add_header("Authorization", f"Bearer {token}")
        try:
            with urllib.request.urlopen(request, timeout=15) as response:
                text = response.read().decode()
                return response.status, json.loads(text) if text else {}
        except urllib.error.HTTPError as error:
            try:
                return error.code, json.loads(error.read().decode() or "{}")
            except ValueError:
                return error.code, {}

    def login(self, username, password):
        """The user's API token, or None when the password does not work."""
        status, body = self._call("POST", "/drasl/api/v3/login", {"username": username, "password": password})
        return body.get("apiToken") if status == 200 else None

    def set_password(self, token, password):
        status, body = self._call("PATCH", "/drasl/api/v3/user", {"password": password}, token)
        if status != 200:
            raise ApiError(400 if status == 400 else 502, body.get("message", "password not changed"))


class FirstAccess:
    def __init__(self, pending_file, drasl):
        self.pending_file = Path(pending_file)
        self.drasl = drasl
        self.lock = threading.Lock()

    def _entries(self):
        entries = {}
        if self.pending_file.exists():
            for line in self.pending_file.read_text(encoding="utf-8").splitlines():
                name, _, temporary = line.partition(";")
                if NICKNAME.fullmatch(name) and temporary:
                    entries[name.lower()] = (name, temporary)
        return entries

    def _save(self, entries):
        partial = self.pending_file.with_name(self.pending_file.name + ".part")
        partial.write_text("".join(f"{name};{temporary}\n" for name, temporary in entries.values()), encoding="utf-8")
        os.chmod(partial, 0o600)
        os.replace(partial, self.pending_file)

    def _pending(self, name):
        if not isinstance(name, str) or not NICKNAME.fullmatch(name):
            raise ApiError(400, "invalid nickname")
        entry = self._entries().get(name.lower())
        # Still a first access only while the temporary password works.
        if entry is None:
            return None, None
        token = self.drasl.login(entry[0], entry[1])
        return (entry, token) if token else (None, None)

    def status(self, name):
        entry, _ = self._pending(name)
        return {"firstAccess": entry is not None, "name": entry[0] if entry else None}

    def claim(self, name, password):
        if not isinstance(password, str) or not MIN_PASSWORD <= len(password) <= MAX_PASSWORD:
            raise ApiError(400, "password length")
        with self.lock:
            entry, token = self._pending(name)
            if entry is None:
                raise ApiError(409, "not a first access")
            if password == entry[1]:
                raise ApiError(400, "choose a new password")
            self.drasl.set_password(token, password)
            entries = self._entries()
            entries.pop(entry[0].lower(), None)
            self._save(entries)
        return {"name": entry[0], "claimed": True}


def make_handler(service):
    class Handler(BaseHTTPRequestHandler):
        server_version = "EternalFirstAccess"
        sys_version = ""
        timeout = 20

        def log_message(self, format, *args):
            pass  # NGINX keeps the access log; passwords never reach a log here.

        def _reply(self, status, payload):
            body = json.dumps(payload).encode()
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)

        def _guarded(self, action):
            try:
                self._reply(200, action())
            except ApiError as error:
                self._reply(error.status, {"error": error.message})
            except (OSError, ValueError) as error:
                print(f"first access error: {type(error).__name__}", file=sys.stderr)
                self._reply(503, {"error": "unavailable"})

        def do_GET(self):
            path = urlsplit(self.path).path
            prefix = "/api/accounts/first-access/"
            if not path.startswith(prefix):
                self._reply(404, {"error": "not found"})
                return
            self._guarded(lambda: service.status(unquote(path[len(prefix):])))

        def do_POST(self):
            if urlsplit(self.path).path != "/api/accounts/first-access":
                self._reply(404, {"error": "not found"})
                return

            def action():
                try:
                    length = int(self.headers.get("Content-Length", ""))
                except ValueError:
                    raise ApiError(411, "length required") from None
                if not 0 < length <= MAX_BODY_BYTES:
                    raise ApiError(413, "body too large")
                body = json.loads(self.rfile.read(length))
                if not isinstance(body, dict):
                    raise ApiError(400, "invalid json")
                return service.claim(body.get("name"), body.get("password"))

            self._guarded(action)

    return Handler


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--port", type=int, default=int(os.environ.get("ETERNAL_FIRST_ACCESS_PORT", "25586")))
    parser.add_argument("--pending", default=os.environ.get("ETERNAL_FIRST_ACCESS_FILE", "/var/lib/eternal-first-access/pending.txt"))
    parser.add_argument("--drasl", default=os.environ.get("DRASL_URL", "http://127.0.0.1:25585"))
    arguments = parser.parse_args()
    service = FirstAccess(arguments.pending, Drasl(arguments.drasl))
    server = ThreadingHTTPServer(("0.0.0.0", arguments.port), make_handler(service))
    server.daemon_threads = True
    server.serve_forever()


if __name__ == "__main__":
    sys.exit(main())
