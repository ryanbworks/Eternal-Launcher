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


if __name__ == "__main__":
    unittest.main()
