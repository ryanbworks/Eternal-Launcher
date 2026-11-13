#!/usr/bin/env python3
"""Group screenshot gallery API of the Eternal Launcher.

Players share screenshots from the launcher; everybody sees them in the Capturas
page. The launcher scales and encodes each picture before sending it, so this
service only checks that it received two reasonable JPEG files (a thumbnail and
the picture) and stores them. Nickname mode has no login: uploads are limited per
player and per address, the uploader gets a token to remove their own picture,
and reports from three different addresses hide a picture until an admin looks.

Listens on localhost only; NGINX publishes it under /api/gallery/.
"""

import argparse
import hashlib
import json
import os
import re
import secrets
import sqlite3
import struct
import sys
import time
from contextlib import contextmanager
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlsplit

NICKNAME = re.compile(r"[A-Za-z0-9_]{3,16}")
PHOTO_ID = re.compile(r"[a-f0-9]{16}")
MAX_THUMB_BYTES = 256 * 1024
MAX_PHOTO_BYTES = 3 * 1024 * 1024
MAX_DIMENSION = 4096
MAX_JSON_BYTES = 1024
# Generous for a group of friends, tight enough to stop a flood.
UPLOADS_PER_PLAYER_PER_DAY = 15
UPLOADS_PER_IP_PER_DAY = 40
# The oldest pictures leave once the gallery holds this many.
MAX_PHOTOS = 600
REPORTS_TO_HIDE = 3
MAX_LIST = 200

SCHEMA = """
CREATE TABLE IF NOT EXISTS photos (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    player_key TEXT NOT NULL,
    client_ip TEXT NOT NULL,
    created INTEGER NOT NULL,
    width INTEGER NOT NULL,
    height INTEGER NOT NULL,
    size INTEGER NOT NULL,
    token_hash TEXT NOT NULL,
    hidden INTEGER NOT NULL DEFAULT 0,
    reports INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS photos_by_created ON photos(created);
CREATE INDEX IF NOT EXISTS photos_by_player ON photos(player_key, created);
CREATE INDEX IF NOT EXISTS photos_by_ip ON photos(client_ip, created);
CREATE TABLE IF NOT EXISTS reports (
    photo_id TEXT NOT NULL,
    client_ip TEXT NOT NULL,
    PRIMARY KEY (photo_id, client_ip)
);
"""


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


def _photo_id(value):
    if not isinstance(value, str) or not PHOTO_ID.fullmatch(value):
        raise ApiError(404, "unknown photo")
    return value


def jpeg_size(data):
    """Width and height of a baseline or progressive JPEG, or None if it is not one."""
    if len(data) < 4 or data[:3] != b"\xff\xd8\xff" or data[-2:] != b"\xff\xd9":
        return None
    position = 2
    while position + 4 <= len(data):
        if data[position] != 0xFF:
            return None
        marker = data[position + 1]
        if marker == 0xFF:
            position += 1
            continue
        if marker in (0xD8, 0x01) or 0xD0 <= marker <= 0xD7:
            position += 2
            continue
        (length,) = struct.unpack(">H", data[position + 2 : position + 4])
        if length < 2:
            return None
        # Start of frame markers carry the dimensions (C4, C8 and CC are not frames).
        if 0xC0 <= marker <= 0xCF and marker not in (0xC4, 0xC8, 0xCC):
            if position + 9 > len(data):
                return None
            height, width = struct.unpack(">HH", data[position + 5 : position + 9])
            return (width, height) if width and height else None
        position += 2 + length
    return None


def split_upload(body):
    """The upload body is a 4-byte big-endian thumbnail length, the thumbnail, then the picture."""
    if len(body) < 4:
        raise ApiError(400, "invalid upload")
    (thumb_length,) = struct.unpack(">I", body[:4])
    if thumb_length == 0 or thumb_length > MAX_THUMB_BYTES or 4 + thumb_length >= len(body):
        raise ApiError(400, "invalid upload")
    thumb, photo = body[4 : 4 + thumb_length], body[4 + thumb_length :]
    if len(photo) > MAX_PHOTO_BYTES:
        raise ApiError(413, "picture too large")
    thumb_size, photo_size = jpeg_size(thumb), jpeg_size(photo)
    if thumb_size is None or photo_size is None:
        raise ApiError(415, "pictures must be JPEG")
    if max(photo_size) > MAX_DIMENSION or max(thumb_size) > 1024:
        raise ApiError(413, "picture too large")
    return thumb, photo, photo_size


class GalleryStore:
    def __init__(self, database, photos_directory, review=False, clock=time.time):
        self.database = database
        self.photos = Path(photos_directory)
        self.photos.mkdir(parents=True, exist_ok=True)
        self.review = review
        self.clock = clock
        with self._transaction() as db:
            for statement in SCHEMA.split(";"):
                if statement.strip():
                    db.execute(statement)

    def _now(self):
        return int(self.clock())

    def _connect(self):
        db = sqlite3.connect(self.database, timeout=10, isolation_level=None)
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

    def _files(self, photo_id):
        return self.photos / f"{photo_id}.jpg", self.photos / f"{photo_id}-thumb.jpg"

    def _remove_files(self, photo_id):
        for path in self._files(photo_id):
            try:
                path.unlink()
            except FileNotFoundError:
                pass

    def upload(self, nickname, client_ip, body):
        key = _player_key(nickname)
        thumb, photo, (width, height) = split_upload(body)
        now = self._now()
        day_ago = now - 86400
        photo_id = secrets.token_hex(8)
        token = secrets.token_urlsafe(24)
        removed = []
        try:
            with self._transaction() as db:
                mine = db.execute(
                    "SELECT COUNT(*) AS n FROM photos WHERE player_key = ? AND created > ?", (key, day_ago)
                ).fetchone()["n"]
                from_ip = db.execute(
                    "SELECT COUNT(*) AS n FROM photos WHERE client_ip = ? AND created > ?", (client_ip, day_ago)
                ).fetchone()["n"]
                if mine >= UPLOADS_PER_PLAYER_PER_DAY or from_ip >= UPLOADS_PER_IP_PER_DAY:
                    raise ApiError(429, "daily limit reached")
                # Files first: a row never points at a picture that does not exist.
                for path, data in zip(self._files(photo_id), (photo, thumb)):
                    partial = path.with_name(path.name + ".part")
                    partial.write_bytes(data)
                    os.replace(partial, path)
                db.execute(
                    "INSERT INTO photos(id, name, player_key, client_ip, created, width, height, size, token_hash, hidden) "
                    "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                    (photo_id, nickname, key, client_ip, now, width, height, len(photo), _token_hash(token), int(self.review)),
                )
                overflow = db.execute("SELECT COUNT(*) AS n FROM photos").fetchone()["n"] - MAX_PHOTOS
                if overflow > 0:
                    removed = [row["id"] for row in db.execute(
                        "SELECT id FROM photos ORDER BY created ASC, id ASC LIMIT ?", (overflow,)
                    )]
                    db.executemany("DELETE FROM photos WHERE id = ?", [(old,) for old in removed])
                    db.executemany("DELETE FROM reports WHERE photo_id = ?", [(old,) for old in removed])
        except BaseException:
            # A refused or failed upload leaves no files behind.
            self._remove_files(photo_id)
            raise
        for old in removed:
            self._remove_files(old)
        return {"id": photo_id, "deleteToken": token, "pending": self.review}

    def list(self, limit=60):
        limit = max(1, min(limit, MAX_LIST))
        db = self._connect()
        try:
            rows = db.execute(
                "SELECT id, name, created, width, height FROM photos WHERE hidden = 0 "
                "ORDER BY created DESC, id DESC LIMIT ?",
                (limit,),
            ).fetchall()
            return {"photos": [dict(row) for row in rows]}
        finally:
            db.close()

    def image(self, photo_id, thumbnail):
        photo_id = _photo_id(photo_id)
        db = self._connect()
        try:
            row = db.execute("SELECT hidden FROM photos WHERE id = ?", (photo_id,)).fetchone()
        finally:
            db.close()
        if row is None or row["hidden"]:
            raise ApiError(404, "unknown photo")
        full_path, thumb_path = self._files(photo_id)
        try:
            return (thumb_path if thumbnail else full_path).read_bytes()
        except FileNotFoundError:
            raise ApiError(404, "unknown photo") from None

    def delete(self, photo_id, token):
        photo_id = _photo_id(photo_id)
        if not isinstance(token, str) or not token:
            raise ApiError(403, "not your photo")
        with self._transaction() as db:
            row = db.execute("SELECT token_hash FROM photos WHERE id = ?", (photo_id,)).fetchone()
            if row is None:
                raise ApiError(404, "unknown photo")
            if not secrets.compare_digest(row["token_hash"], _token_hash(token)):
                raise ApiError(403, "not your photo")
            db.execute("DELETE FROM photos WHERE id = ?", (photo_id,))
            db.execute("DELETE FROM reports WHERE photo_id = ?", (photo_id,))
        self._remove_files(photo_id)
        return {"deleted": True}

    def report(self, photo_id, client_ip):
        photo_id = _photo_id(photo_id)
        with self._transaction() as db:
            if db.execute("SELECT 1 FROM photos WHERE id = ?", (photo_id,)).fetchone() is None:
                raise ApiError(404, "unknown photo")
            # One report per address; repeating it changes nothing.
            if db.execute(
                "INSERT OR IGNORE INTO reports(photo_id, client_ip) VALUES(?, ?)", (photo_id, client_ip)
            ).rowcount:
                db.execute("UPDATE photos SET reports = reports + 1 WHERE id = ?", (photo_id,))
                db.execute("UPDATE photos SET hidden = 1 WHERE id = ? AND reports >= ?", (photo_id, REPORTS_TO_HIDE))
            hidden = db.execute("SELECT hidden FROM photos WHERE id = ?", (photo_id,)).fetchone()["hidden"]
        return {"reported": True, "hidden": bool(hidden)}

    # Administration.
    def all(self):
        db = self._connect()
        try:
            return [dict(row) for row in db.execute(
                "SELECT id, name, created, width, height, size, hidden, reports FROM photos ORDER BY created DESC"
            )]
        finally:
            db.close()

    def set_hidden(self, photo_id, hidden):
        photo_id = _photo_id(photo_id)
        with self._transaction() as db:
            changed = db.execute("UPDATE photos SET hidden = ? WHERE id = ?", (int(hidden), photo_id)).rowcount
            if not hidden:
                db.execute("UPDATE photos SET reports = 0 WHERE id = ?", (photo_id,))
                db.execute("DELETE FROM reports WHERE photo_id = ?", (photo_id,))
        if not changed:
            raise ApiError(404, "unknown photo")

    def remove(self, photo_id):
        photo_id = _photo_id(photo_id)
        with self._transaction() as db:
            if not db.execute("DELETE FROM photos WHERE id = ?", (photo_id,)).rowcount:
                raise ApiError(404, "unknown photo")
            db.execute("DELETE FROM reports WHERE photo_id = ?", (photo_id,))
        self._remove_files(photo_id)

    def purge_player(self, nickname):
        key = _player_key(nickname)
        with self._transaction() as db:
            ids = [row["id"] for row in db.execute("SELECT id FROM photos WHERE player_key = ?", (key,))]
            db.execute("DELETE FROM photos WHERE player_key = ?", (key,))
            db.executemany("DELETE FROM reports WHERE photo_id = ?", [(photo,) for photo in ids])
        for photo in ids:
            self._remove_files(photo)
        return len(ids)


IMAGE_PATH = re.compile(r"/api/gallery/photos/([a-f0-9]{16})/(thumb|full)")
ACTION_PATH = re.compile(r"/api/gallery/photos/([a-f0-9]{16})/(delete|report)")


def make_handler(store):
    class Handler(BaseHTTPRequestHandler):
        server_version = "EternalGallery"
        sys_version = ""
        timeout = 30

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

        def _read(self, limit):
            try:
                length = int(self.headers.get("Content-Length", ""))
            except ValueError:
                raise ApiError(411, "length required") from None
            if length < 0 or length > limit:
                raise ApiError(413, "body too large")
            return self.rfile.read(length)

        def _json(self):
            try:
                document = json.loads(self._read(MAX_JSON_BYTES) or b"{}")
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
            except (sqlite3.Error, OSError) as error:
                print(f"storage error: {error}", file=sys.stderr)
                self._reply(503, {"error": "storage unavailable"})

        def do_GET(self):
            url = urlsplit(self.path)
            image = IMAGE_PATH.fullmatch(url.path)
            if image:
                try:
                    data = store.image(image.group(1), image.group(2) == "thumb")
                except ApiError as error:
                    self._reply(error.status, {"error": error.message})
                    return
                self.send_response(200)
                self.send_header("Content-Type", "image/jpeg")
                self.send_header("Content-Length", str(len(data)))
                # A picture never changes under its id.
                self.send_header("Cache-Control", "public, max-age=604800, immutable")
                self.send_header("X-Content-Type-Options", "nosniff")
                self.end_headers()
                self.wfile.write(data)
                return
            if url.path != "/api/gallery/photos":
                self._reply(404, {"error": "not found"})
                return
            query = parse_qs(url.query)

            def action():
                try:
                    limit = int(query.get("limit", ["60"])[0])
                except ValueError:
                    raise ApiError(400, "invalid limit") from None
                return store.list(limit)

            self._guarded(action)

        def do_POST(self):
            url = urlsplit(self.path)
            if url.path == "/api/gallery/photos":
                name = parse_qs(url.query).get("name", [None])[0]
                self._guarded(lambda: store.upload(name, self._client_ip(), self._read(4 + MAX_THUMB_BYTES + MAX_PHOTO_BYTES)))
                return
            action = ACTION_PATH.fullmatch(url.path)
            if action is None:
                self._reply(404, {"error": "not found"})
                return
            photo_id, verb = action.groups()
            if verb == "delete":
                self._guarded(lambda: store.delete(photo_id, self._json().get("token")))
            else:
                self._guarded(lambda: store.report(photo_id, self._client_ip()))

    return Handler


def open_store():
    state = os.environ.get("ETERNAL_GALLERY_DIR", "/var/lib/eternal-gallery")
    return GalleryStore(
        os.path.join(state, "gallery.db"),
        os.path.join(state, "photos"),
        review=os.environ.get("ETERNAL_GALLERY_REVIEW", "0") == "1",
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    commands = parser.add_subparsers(dest="command", required=True)
    serve = commands.add_parser("serve", help="run the API")
    serve.add_argument("--port", type=int, default=int(os.environ.get("ETERNAL_GALLERY_PORT", "8766")))
    commands.add_parser("list", help="every picture, newest first, with reports and visibility")
    for command, description in (("hide", "hide a picture from the gallery"),
                                 ("show", "show a hidden or pending picture and clear its reports"),
                                 ("delete", "delete a picture and its files")):
        commands.add_parser(command, help=description).add_argument("id")
    commands.add_parser("purge", help="delete every picture of a player").add_argument("nickname")
    arguments = parser.parse_args()

    store = open_store()
    try:
        if arguments.command == "serve":
            server = ThreadingHTTPServer(("127.0.0.1", arguments.port), make_handler(store))
            server.daemon_threads = True
            server.serve_forever()
        elif arguments.command == "list":
            for photo in store.all():
                state = "oculta" if photo["hidden"] else "visível"
                created = time.strftime("%Y-%m-%d %H:%M", time.localtime(photo["created"]))
                print(f"{photo['id']}  {photo['name']:<16}  {created}  {photo['width']}x{photo['height']}  "
                      f"{state:<8} denúncias: {photo['reports']}")
        elif arguments.command == "hide":
            store.set_hidden(arguments.id, True)
        elif arguments.command == "show":
            store.set_hidden(arguments.id, False)
        elif arguments.command == "delete":
            store.remove(arguments.id)
        elif arguments.command == "purge":
            print(f"{store.purge_player(arguments.nickname)} capturas removidas")
    except ApiError as error:
        print(error.message, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
