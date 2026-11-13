"""Tests for the rules that keep the group gallery small, owned and moderated."""

import struct
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import gallery_api
from gallery_api import ApiError, GalleryStore, jpeg_size, split_upload


def jpeg(width, height, padding=0):
    """A minimal JPEG: start of image, one baseline frame header, filler, end of image."""
    frame = struct.pack(">BHHB", 8, height, width, 3) + b"\x01\x11\x00\x02\x11\x01\x03\x11\x01"
    comment = b"\xff\xfe" + struct.pack(">H", 2 + padding) + b"x" * padding
    return b"\xff\xd8" + comment + b"\xff\xc0" + struct.pack(">H", 2 + len(frame)) + frame + b"\xff\xd9"


def upload_body(thumb=None, photo=None):
    thumb = jpeg(416, 234) if thumb is None else thumb
    photo = jpeg(1920, 1080) if photo is None else photo
    return struct.pack(">I", len(thumb)) + thumb + photo


class JpegTests(unittest.TestCase):
    def test_reads_the_frame_size(self):
        self.assertEqual((1920, 1080), jpeg_size(jpeg(1920, 1080, padding=40)))

    def test_rejects_anything_else(self):
        self.assertIsNone(jpeg_size(b"\x89PNG\r\n\x1a\n" + b"\x00" * 20))
        self.assertIsNone(jpeg_size(jpeg(10, 10)[:-2]))
        self.assertIsNone(jpeg_size(b""))

    def test_split_checks_both_parts(self):
        thumb, photo, size = split_upload(upload_body())
        self.assertEqual((1920, 1080), size)
        self.assertEqual(jpeg(416, 234), thumb)
        for body in (b"", struct.pack(">I", 0) + jpeg(1, 1), struct.pack(">I", 999) + jpeg(1, 1)):
            with self.assertRaises(ApiError):
                split_upload(body)
        with self.assertRaises(ApiError) as caught:
            split_upload(upload_body(photo=b"\xff\xd8\xffnot a jpeg\xff\xd9"))
        self.assertEqual(415, caught.exception.status)
        with self.assertRaises(ApiError) as caught:
            split_upload(upload_body(photo=jpeg(8000, 4000)))
        self.assertEqual(413, caught.exception.status)


class GalleryStoreTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.now = 1_000_000
        self.store = self.make()

    def make(self, review=False):
        return GalleryStore(str(self.root / "gallery.db"), self.root / "photos", review=review, clock=lambda: self.now)

    def upload(self, nickname="Steve", ip="1.1.1.1"):
        self.now += 1
        return self.store.upload(nickname, ip, upload_body())

    def assert_status(self, status, action, *arguments):
        with self.assertRaises(ApiError) as caught:
            action(*arguments)
        self.assertEqual(status, caught.exception.status)

    def test_upload_lists_newest_first_and_serves_both_files(self):
        first = self.upload("Steve")
        second = self.upload("Aurora_")
        photos = self.store.list()["photos"]
        self.assertEqual([second["id"], first["id"]], [photo["id"] for photo in photos])
        self.assertEqual(("Aurora_", 1920, 1080), (photos[0]["name"], photos[0]["width"], photos[0]["height"]))
        self.assertEqual(jpeg(416, 234), self.store.image(first["id"], True))
        self.assertEqual(jpeg(1920, 1080), self.store.image(first["id"], False))
        self.assertFalse(second["pending"])

    def test_invalid_nicknames_and_ids_are_refused(self):
        self.assert_status(400, self.store.upload, "<script>", "1.1.1.1", upload_body())
        self.assert_status(404, self.store.image, "../../etc/passwd", True)
        self.assert_status(404, self.store.image, "0123456789abcdef", True)

    def test_daily_limits_per_player_and_address(self):
        for _ in range(gallery_api.UPLOADS_PER_PLAYER_PER_DAY):
            self.upload("Steve", "1.1.1.1")
        self.assert_status(429, self.store.upload, "Steve", "9.9.9.9", upload_body())
        self.now += 86400
        self.upload("Steve", "1.1.1.1")
        with mock.patch.object(gallery_api, "UPLOADS_PER_IP_PER_DAY", 2):
            self.upload("Luna", "2.2.2.2")
            self.upload("Tico", "2.2.2.2")
            self.assert_status(429, self.store.upload, "Pedro", "2.2.2.2", upload_body())

    def test_a_refused_upload_leaves_no_files(self):
        with mock.patch.object(gallery_api, "UPLOADS_PER_PLAYER_PER_DAY", 0):
            self.assert_status(429, self.store.upload, "Steve", "1.1.1.1", upload_body())
        self.assertEqual([], list((self.root / "photos").iterdir()))

    def test_only_the_uploader_token_removes_a_picture(self):
        photo = self.upload()
        self.assert_status(403, self.store.delete, photo["id"], "guess")
        self.assert_status(403, self.store.delete, photo["id"], None)
        self.store.delete(photo["id"], photo["deleteToken"])
        self.assertEqual([], self.store.list()["photos"])
        self.assertEqual([], list((self.root / "photos").iterdir()))

    def test_reports_from_different_addresses_hide_a_picture(self):
        photo = self.upload()
        for _ in range(5):
            self.assertFalse(self.store.report(photo["id"], "7.7.7.7")["hidden"])
        self.store.report(photo["id"], "8.8.8.8")
        self.assertTrue(self.store.report(photo["id"], "9.9.9.9")["hidden"])
        self.assertEqual([], self.store.list()["photos"])
        self.assert_status(404, self.store.image, photo["id"], True)
        self.store.set_hidden(photo["id"], False)
        self.assertEqual(1, len(self.store.list()["photos"]))
        self.assertFalse(self.store.report(photo["id"], "9.9.9.9")["hidden"])

    def test_review_mode_waits_for_approval(self):
        self.store = self.make(review=True)
        photo = self.upload()
        self.assertTrue(photo["pending"])
        self.assertEqual([], self.store.list()["photos"])
        self.store.set_hidden(photo["id"], False)
        self.assertEqual(1, len(self.store.list()["photos"]))

    def test_the_oldest_pictures_leave_when_the_gallery_is_full(self):
        with mock.patch.object(gallery_api, "MAX_PHOTOS", 3):
            ids = [self.upload(f"Player{index}", f"10.0.0.{index}")["id"] for index in range(5)]
        self.assertEqual(ids[:1:-1], [photo["id"] for photo in self.store.list()["photos"]])
        self.assertEqual(6, len(list((self.root / "photos").iterdir())))

    def test_purge_removes_every_picture_of_a_player(self):
        self.upload("Steve")
        self.upload("steve", "2.2.2.2")
        self.upload("Aurora_")
        self.assertEqual(2, self.store.purge_player("STEVE"))
        self.assertEqual(["Aurora_"], [photo["name"] for photo in self.store.list()["photos"]])


def png(width=64, height=64, padding=0):
    """A PNG signature and IHDR header, enough for the size check."""
    return b"\x89PNG\r\n\x1a\n" + struct.pack(">I", 13) + b"IHDR" + struct.pack(">II", width, height) + b"\x08\x06\x00\x00\x00" + b"x" * padding


class SkinStoreTests(unittest.TestCase):
    # Same temporary store and helpers, without rerunning the picture tests.
    setUp = GalleryStoreTests.setUp
    make = GalleryStoreTests.make
    assert_status = GalleryStoreTests.assert_status

    def share(self, nickname="Steve", ip="1.1.1.1", title="Cavaleiro", model="classic", data=None):
        self.now += 1
        return self.store.upload_skin(nickname, ip, title, model, png() if data is None else data)

    def test_shared_skins_list_newest_first_with_their_texture(self):
        first = self.share("Steve", title="Cavaleiro")
        second = self.share("Aurora_", title="Raposa (fina)", model="slim", data=png(64, 32))
        skins = self.store.list_skins()["skins"]
        self.assertEqual([second["id"], first["id"]], [skin["id"] for skin in skins])
        self.assertEqual(("Aurora_", "Raposa (fina)", True), (skins[0]["name"], skins[0]["title"], skins[0]["slim"]))
        self.assertEqual(png(), self.store.skin_png(first["id"]))
        self.assertEqual([], self.store.list()["photos"])

    def test_bad_skins_are_refused(self):
        self.assert_status(400, self.share, "x")
        self.assert_status(400, self.share, "Steve", "1.1.1.1", "")
        self.assert_status(400, self.share, "Steve", "1.1.1.1", "<script>")
        self.assert_status(400, self.share, "Steve", "1.1.1.1", "ok", "wide")
        self.assert_status(415, self.share, "Steve", "1.1.1.1", "ok", "classic", png(128, 128))
        self.assert_status(415, self.share, "Steve", "1.1.1.1", "ok", "classic", b"GIF89a")
        self.assert_status(413, self.share, "Steve", "1.1.1.1", "ok", "classic", png(padding=gallery_api.MAX_SKIN_BYTES))
        self.assert_status(404, self.store.skin_png, "../../etc/passwd")
        self.assertEqual([], list((self.root / "photos" / "skins").glob("*")) if (self.root / "photos" / "skins").exists() else [])

    def test_daily_limit_per_player(self):
        for index in range(gallery_api.SKINS_PER_PLAYER_PER_DAY):
            self.share("Steve", f"10.0.0.{index}")
        self.assert_status(429, self.share, "steve", "9.9.9.9")
        self.now += 86400
        self.share("Steve")

    def test_only_the_owner_removes_a_skin_and_reports_hide_it(self):
        mine = self.share()
        self.assert_status(403, self.store.delete_skin, mine["id"], "wrong")
        other = self.share("Aurora_")
        for ip in ("1.1.1.1", "1.1.1.1", "2.2.2.2", "3.3.3.3"):
            self.store.report_skin(other["id"], ip)
        self.assertEqual([mine["id"]], [skin["id"] for skin in self.store.list_skins()["skins"]])
        self.assert_status(404, self.store.skin_png, other["id"])
        self.store.set_hidden(other["id"], False)
        self.assertEqual(2, len(self.store.list_skins()["skins"]))
        self.assertEqual({"deleted": True}, self.store.delete_skin(mine["id"], mine["deleteToken"]))
        self.assert_status(404, self.store.skin_png, mine["id"])
        self.store.remove(other["id"])
        self.assertEqual([], self.store.all_skins())

    def test_the_featured_catalog_is_listed_apart_and_never_rotates(self):
        self.share("Steve")
        self.store.add_featured("Technoblade", png(), False)
        self.store.add_featured("Dream", png(64, 32), True)
        listing = self.store.list_skins()
        self.assertEqual(["Steve"], [skin["name"] for skin in listing["skins"]])
        self.assertEqual([("Dream", True), ("Technoblade", False)], [(s["name"], s["slim"]) for s in listing["featured"]])
        # Adding a name again replaces its skin.
        self.store.add_featured("technoblade", png(), True)
        self.assertEqual(2, len(self.store.list_skins()["featured"]))
        # Shared skins rotate out; the catalog stays.
        with mock.patch.object(gallery_api, "MAX_SKINS", 2):
            for index in range(4):
                self.share(f"Player{index}", f"10.0.0.{index}")
        self.assertEqual(2, len(self.store.list_skins()["skins"]))
        self.assertEqual(2, len(self.store.list_skins()["featured"]))
        # Purging a player keeps the catalog; unfeature takes it out.
        self.store.purge_player("Dream")
        self.assertEqual(2, len(self.store.list_skins()["featured"]))
        self.assertEqual(1, self.store.remove_featured("dream"))
        self.assertEqual(["technoblade"], [s["name"] for s in self.store.list_skins()["featured"]])
        self.assert_status(415, self.store.add_featured, "Notch", b"nope", False)

    def test_a_database_from_before_the_catalog_gains_its_column(self):
        import sqlite3
        database = self.root / "old.db"
        db = sqlite3.connect(database)
        db.executescript(gallery_api.SCHEMA)
        db.execute("INSERT INTO skins(id, name, player_key, client_ip, created, title, slim, token_hash) "
                   "VALUES('0123456789abcdef', 'Steve', 'steve', '1.1.1.1', 1, 'x', 0, 'h')")
        db.commit()
        db.close()
        old = GalleryStore(str(database), self.root / "old-photos", clock=lambda: self.now)
        self.assertEqual(["Steve"], [skin["name"] for skin in old.list_skins()["skins"]])

    def test_purge_also_removes_skins(self):
        self.share("Steve")
        self.share("Aurora_")
        self.assertEqual(1, self.store.purge_player("steve"))
        self.assertEqual(["Aurora_"], [skin["name"] for skin in self.store.list_skins()["skins"]])


class FetchOriginalTests(unittest.TestCase):
    def fake(self, textures, missing=False):
        import base64
        import json
        import urllib.error
        value = base64.b64encode(json.dumps({"textures": textures}).encode()).decode()

        def opener(url):
            if url.startswith(gallery_api.MOJANG_PROFILE):
                if missing:
                    raise urllib.error.HTTPError(url, 404, "not found", {}, None)
                return json.dumps({"id": "069a79f444e94726a5befca90e38aaf5", "name": "Notch"}).encode()
            if url.startswith(gallery_api.MOJANG_SESSION):
                return json.dumps({"properties": [{"name": "textures", "value": value}]}).encode()
            self.assertTrue(url.startswith("https://textures.minecraft.net/texture/"))
            return png()

        return opener

    def test_reads_the_skin_and_model_with_the_accounts_spelling(self):
        found = gallery_api.fetch_original("notch", self.fake(
            {"SKIN": {"url": "http://textures.minecraft.net/texture/abc123", "metadata": {"model": "slim"}}}))
        self.assertEqual(("Notch", png(), True), found)

    def test_refuses_default_skins_other_hosts_and_missing_accounts(self):
        self.assertIsNone(gallery_api.fetch_original("notch", self.fake({})))
        self.assertIsNone(gallery_api.fetch_original("notch", self.fake({"SKIN": {"url": "http://evil.example/texture/abc"}})))
        self.assertIsNone(gallery_api.fetch_original("nobody", self.fake({}, missing=True)))
        self.assertIsNone(gallery_api.fetch_original("../x", self.fake({})))


class HttpTests(unittest.TestCase):
    def setUp(self):
        import threading
        from http.server import ThreadingHTTPServer
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        store = GalleryStore(str(Path(directory.name) / "gallery.db"), Path(directory.name) / "photos")
        self.server = ThreadingHTTPServer(("127.0.0.1", 0), gallery_api.make_handler(store))
        threading.Thread(target=self.server.serve_forever, daemon=True).start()
        self.addCleanup(self.server.server_close)
        self.addCleanup(self.server.shutdown)
        self.base = f"http://127.0.0.1:{self.server.server_address[1]}"

    def call(self, method, path, body=None):
        import json
        import urllib.error
        import urllib.request
        request = urllib.request.Request(self.base + path, data=body, method=method)
        try:
            with urllib.request.urlopen(request) as response:
                data = response.read()
                return response.status, response.headers.get("Content-Type"), data
        except urllib.error.HTTPError as error:
            return error.code, error.headers.get("Content-Type"), json.loads(error.read())

    def test_skin_routes(self):
        import json
        status, _, body = self.call("POST", "/api/gallery/skins?name=Steve&title=Minha%20skin&model=slim", png())
        self.assertEqual(200, status)
        shared = json.loads(body)
        status, _, body = self.call("GET", "/api/gallery/skins")
        self.assertEqual([("Steve", "Minha skin", True)], [(s["name"], s["title"], s["slim"]) for s in json.loads(body)["skins"]])
        self.assertEqual([], json.loads(body)["featured"])
        status, kind, body = self.call("GET", f"/api/gallery/skins/{shared['id']}.png")
        self.assertEqual((200, "image/png", png()), (status, kind, body))
        self.assertEqual(200, self.call("POST", f"/api/gallery/skins/{shared['id']}/report", b"")[0])
        token = json.dumps({"token": shared["deleteToken"]}).encode()
        self.assertEqual(200, self.call("POST", f"/api/gallery/skins/{shared['id']}/delete", token)[0])
        self.assertEqual(404, self.call("GET", f"/api/gallery/skins/{shared['id']}.png")[0])
        self.assertEqual(415, self.call("POST", "/api/gallery/skins?name=Steve&title=x", b"nope")[0])


if __name__ == "__main__":
    unittest.main()
