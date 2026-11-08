#!/usr/bin/env python3
"""Play time and leaderboard API of the Eternal Launcher.

The launcher never sends how long somebody played. It opens a session and sends
heartbeats; the time is measured here, with this machine's clock, so a client can
at most count in real time. The only number accepted from a client is the one-time
import of the old local counter, which is capped and closes after a deadline.

Listens on localhost only; NGINX publishes it under /api/playtime/.
"""

import argparse
import hashlib
import json
import os
import re
import secrets
import sqlite3
import sys
import time
from contextlib import contextmanager
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlsplit

NICKNAME = re.compile(r"[A-Za-z0-9_]{3,16}")
HEARTBEAT_SECONDS = 60
# A longer silence (sleep, lost connection) earns nothing: only proven time counts.
MAX_CREDIT_GAP = 150
SESSION_TTL = 900
MAX_SESSIONS_PER_IP = 6
MAX_BODY_BYTES = 4096
MAX_LEADERBOARD = 20

SCHEMA = """
CREATE TABLE IF NOT EXISTS players (
    player_key TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    counted_seconds INTEGER NOT NULL DEFAULT 0,
    imported_seconds INTEGER NOT NULL DEFAULT 0,
    import_used INTEGER NOT NULL DEFAULT 0,
    last_played INTEGER NOT NULL DEFAULT 0
);
CREATE TABLE IF NOT EXISTS sessions (
    token_hash TEXT PRIMARY KEY,
    player_key TEXT NOT NULL UNIQUE REFERENCES players(player_key),
    client_ip TEXT NOT NULL,
    last_seen INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS sessions_by_ip ON sessions(client_ip);
CREATE TABLE IF NOT EXISTS meta (name TEXT PRIMARY KEY, value INTEGER NOT NULL);
"""

TOTAL = "(counted_seconds + imported_seconds)"


class ApiError(Exception):
    def __init__(self, status, message):
        super().__init__(message)
        self.status = status
        self.message = message


def _token_hash(token):
    return hashlib.sha256(token.encode("utf-8")).hexdigest()


def _player_key(nickname):
    if not isinstance(nickname, str) or not NICKNAME.fullmatch(nickname):
        raise ApiError(400, "invalid nickname")
    return nickname.lower()


class PlaytimeStore:
    def __init__(self, path, import_cap_seconds, import_days, clock=time.time):
        self.path = path
        self.import_cap_seconds = import_cap_seconds
        self.clock = clock
        with self._transaction() as db:
            for statement in SCHEMA.split(";"):
                if statement.strip():
                    db.execute(statement)
            db.execute(
                "INSERT OR IGNORE INTO meta(name, value) VALUES('import_closes_at', ?)",
                (self._now() + import_days * 86400,),
            )

    def _now(self):
        return int(self.clock())

    def _connect(self):
        db = sqlite3.connect(self.path, timeout=10, isolation_level=None)
        db.row_factory = sqlite3.Row
        db.execute("PRAGMA journal_mode=WAL")
        db.execute("PRAGMA synchronous=NORMAL")
        return db

    @contextmanager
    def _transaction(self):
        db = self._connect()
        try:
            db.execute("BEGIN IMMEDIATE")
            yield db
            db.execute("COMMIT")
        except BaseException:
            if db.in_transaction:
                db.execute("ROLLBACK")
            raise
        finally:
            db.close()

    def _purge(self, db, now):
        db.execute("DELETE FROM sessions WHERE last_seen < ?", (now - SESSION_TTL,))

    def _credit(self, db, session, now):
        gap = now - session["last_seen"]
        earned = gap if 0 < gap <= MAX_CREDIT_GAP else 0
        db.execute(
            "UPDATE players SET counted_seconds = counted_seconds + ?, last_played = ? WHERE player_key = ?",
            (earned, now, session["player_key"]),
        )

    def _session(self, db, token):
        if not isinstance(token, str) or not token:
            raise ApiError(400, "invalid token")
        session = db.execute(
            "SELECT * FROM sessions WHERE token_hash = ?", (_token_hash(token),)
        ).fetchone()
        if session is None:
            raise ApiError(404, "unknown session")
        return session

    @staticmethod
    def _total(db, key):
        row = db.execute(f"SELECT {TOTAL} AS total FROM players WHERE player_key = ?", (key,)).fetchone()
        return row["total"]

    def _import_open(self, db, now):
        closes = db.execute("SELECT value FROM meta WHERE name = 'import_closes_at'").fetchone()
        return now < closes["value"]

    def start_session(self, nickname, client_ip):
        key = _player_key(nickname)
        now = self._now()
        with self._transaction() as db:
            self._purge(db, now)
            others = db.execute(
                "SELECT COUNT(*) AS n FROM sessions WHERE client_ip = ? AND player_key <> ?",
                (client_ip, key),
            ).fetchone()["n"]
            if others >= MAX_SESSIONS_PER_IP:
                raise ApiError(429, "too many sessions from this address")
            db.execute(
                "INSERT INTO players(player_key, name) VALUES(?, ?) "
                "ON CONFLICT(player_key) DO UPDATE SET name = excluded.name",
                (key, nickname),
            )
            # One running session per player: a second launcher can never count double.
            previous = db.execute("SELECT * FROM sessions WHERE player_key = ?", (key,)).fetchone()
            if previous is not None:
                self._credit(db, previous, now)
                db.execute("DELETE FROM sessions WHERE token_hash = ?", (previous["token_hash"],))
            token = secrets.token_urlsafe(32)
            db.execute(
                "INSERT INTO sessions(token_hash, player_key, client_ip, last_seen) VALUES(?, ?, ?, ?)",
                (_token_hash(token), key, client_ip, now),
            )
            player = db.execute("SELECT import_used FROM players WHERE player_key = ?", (key,)).fetchone()
            return {
                "token": token,
                "heartbeatSeconds": HEARTBEAT_SECONDS,
                "totalSeconds": self._total(db, key),
                "importAvailable": not player["import_used"] and self._import_open(db, now),
            }

    def heartbeat(self, token):
        now = self._now()
        with self._transaction() as db:
            self._purge(db, now)
            session = self._session(db, token)
            self._credit(db, session, now)
            db.execute("UPDATE sessions SET last_seen = ? WHERE token_hash = ?", (now, session["token_hash"]))
            return {"totalSeconds": self._total(db, session["player_key"])}

    def end_session(self, token):
        now = self._now()
        with self._transaction() as db:
            session = self._session(db, token)
            self._credit(db, session, now)
            db.execute("DELETE FROM sessions WHERE token_hash = ?", (session["token_hash"],))
            return {"totalSeconds": self._total(db, session["player_key"])}

    def import_local(self, token, seconds, last_played):
        if isinstance(seconds, bool) or not isinstance(seconds, int) or seconds < 0:
            raise ApiError(400, "invalid seconds")
        if isinstance(last_played, bool) or not isinstance(last_played, int):
            last_played = 0
        now = self._now()
        with self._transaction() as db:
            session = self._session(db, token)
            key = session["player_key"]
            player = db.execute("SELECT import_used FROM players WHERE player_key = ?", (key,)).fetchone()
            if player["import_used"]:
                raise ApiError(409, "import already used")
            if not self._import_open(db, now):
                raise ApiError(403, "import window closed")
            imported = min(seconds, self.import_cap_seconds)
            db.execute(
                "UPDATE players SET imported_seconds = ?, import_used = 1, last_played = MAX(last_played, ?) "
                "WHERE player_key = ?",
                (imported, max(0, min(last_played, now)), key),
            )
            return {"importedSeconds": imported, "totalSeconds": self._total(db, key)}

    def leaderboard(self, limit=10, player=None):
        now = self._now()
        limit = max(1, min(limit, MAX_LEADERBOARD))
        db = self._connect()
        try:
            rows = db.execute(
                f"SELECT p.name, {TOTAL} AS total, s.last_seen FROM players p "
                "LEFT JOIN sessions s ON s.player_key = p.player_key "
                f"WHERE {TOTAL} > 0 ORDER BY total DESC, p.player_key ASC LIMIT ?",
                (limit,),
            ).fetchall()
            result = {
                "players": [
                    {
                        "rank": index + 1,
                        "name": row["name"],
                        "totalSeconds": row["total"],
                        "online": row["last_seen"] is not None and now - row["last_seen"] <= MAX_CREDIT_GAP,
                    }
                    for index, row in enumerate(rows)
                ],
                "you": None,
            }
            if isinstance(player, str) and NICKNAME.fullmatch(player):
                key = player.lower()
                mine = db.execute(
                    f"SELECT {TOTAL} AS total, last_played FROM players WHERE player_key = ?", (key,)
                ).fetchone()
                if mine is not None:
                    ahead = db.execute(
                        f"SELECT COUNT(*) AS n FROM players WHERE {TOTAL} > ? OR ({TOTAL} = ? AND player_key < ?)",
                        (mine["total"], mine["total"], key),
                    ).fetchone()["n"]
                    result["you"] = {
                        "rank": ahead + 1,
                        "totalSeconds": mine["total"],
                        "lastPlayed": mine["last_played"],
                    }
            return result
        finally:
            db.close()

    # Administration, for the cases an automatic rule cannot judge.
    def set_imported(self, nickname, seconds):
        key = _player_key(nickname)
        with self._transaction() as db:
            changed = db.execute(
                "UPDATE players SET imported_seconds = ?, import_used = 1 WHERE player_key = ?",
                (max(0, seconds), key),
            ).rowcount
            if not changed:
                raise ApiError(404, "unknown player")

    def remove_player(self, nickname):
        key = _player_key(nickname)
        with self._transaction() as db:
            db.execute("DELETE FROM sessions WHERE player_key = ?", (key,))
            if not db.execute("DELETE FROM players WHERE player_key = ?", (key,)).rowcount:
                raise ApiError(404, "unknown player")

    def set_import_window(self, days):
        with self._transaction() as db:
            db.execute(
                "UPDATE meta SET value = ? WHERE name = 'import_closes_at'",
                (self._now() + days * 86400,),
            )


def make_handler(store):
    class Handler(BaseHTTPRequestHandler):
        server_version = "EternalPlaytime"
        sys_version = ""
        timeout = 15

        def log_message(self, format, *args):
            pass  # NGINX already keeps the access log.

        def _reply(self, status, payload):
            body = json.dumps(payload, separators=(",", ":")).encode("utf-8")
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)

        def _client_ip(self):
            return self.headers.get("X-Real-IP") or self.client_address[0]

        def _body(self):
            try:
                length = int(self.headers.get("Content-Length", ""))
            except ValueError:
                raise ApiError(411, "length required") from None
            if length < 0 or length > MAX_BODY_BYTES:
                raise ApiError(413, "body too large")
            try:
                document = json.loads(self.rfile.read(length))
            except (ValueError, UnicodeDecodeError):
                raise ApiError(400, "invalid json") from None
            if not isinstance(document, dict):
                raise ApiError(400, "invalid json")
            return document

        def _guarded(self, action):
            try:
                self._reply(200, action())
            except ApiError as error:
                self._reply(error.status, {"error": error.message})
            except sqlite3.Error as error:
                print(f"database error: {error}", file=sys.stderr)
                self._reply(503, {"error": "storage unavailable"})

        def do_GET(self):
            url = urlsplit(self.path)
            if url.path != "/api/playtime/leaderboard":
                self._reply(404, {"error": "not found"})
                return
            query = parse_qs(url.query)

            def action():
                try:
                    limit = int(query.get("limit", ["10"])[0])
                except ValueError:
                    raise ApiError(400, "invalid limit") from None
                return store.leaderboard(limit, query.get("player", [None])[0])

            self._guarded(action)

        def do_POST(self):
            path = urlsplit(self.path).path
            routes = {
                "/api/playtime/session/start": lambda b: store.start_session(b.get("nickname"), self._client_ip()),
                "/api/playtime/session/heartbeat": lambda b: store.heartbeat(b.get("token")),
                "/api/playtime/session/end": lambda b: store.end_session(b.get("token")),
                "/api/playtime/import": lambda b: store.import_local(
                    b.get("token"), b.get("totalSeconds"), b.get("lastPlayed")
                ),
            }
            route = routes.get(path)
            if route is None:
                self._reply(404, {"error": "not found"})
                return
            self._guarded(lambda: route(self._body()))

    return Handler


def open_store():
    return PlaytimeStore(
        os.environ.get("ETERNAL_PLAYTIME_DB", "/var/lib/eternal-playtime/playtime.db"),
        int(float(os.environ.get("ETERNAL_PLAYTIME_IMPORT_CAP_HOURS", "200")) * 3600),
        int(os.environ.get("ETERNAL_PLAYTIME_IMPORT_DAYS", "60")),
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    commands = parser.add_subparsers(dest="command", required=True)
    serve = commands.add_parser("serve", help="run the API")
    serve.add_argument("--port", type=int, default=int(os.environ.get("ETERNAL_PLAYTIME_PORT", "8765")))
    commands.add_parser("top", help="print the 20 players with the most time")
    set_imported = commands.add_parser("set-imported", help="replace the imported hours of a player")
    set_imported.add_argument("nickname")
    set_imported.add_argument("hours", type=float)
    remove = commands.add_parser("remove", help="delete a player from the ranking")
    remove.add_argument("nickname")
    window = commands.add_parser("import-window", help="close the import in N days from now (0 closes it)")
    window.add_argument("days", type=int)
    arguments = parser.parse_args()

    store = open_store()
    try:
        if arguments.command == "serve":
            server = ThreadingHTTPServer(("127.0.0.1", arguments.port), make_handler(store))
            server.daemon_threads = True
            server.serve_forever()
        elif arguments.command == "top":
            for player in store.leaderboard(MAX_LEADERBOARD)["players"]:
                print(f"{player['rank']:>3}  {player['name']:<16}  {player['totalSeconds'] / 3600:8.1f} h")
        elif arguments.command == "set-imported":
            store.set_imported(arguments.nickname, int(arguments.hours * 3600))
        elif arguments.command == "remove":
            store.remove_player(arguments.nickname)
        elif arguments.command == "import-window":
            store.set_import_window(arguments.days)
    except ApiError as error:
        print(error.message, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
