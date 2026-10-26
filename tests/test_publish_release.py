import base64
import contextlib
import hashlib
import io
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch

from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import padding, rsa

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import publish_release as publisher


class FakeSFTP:
    def __init__(self, root):
        self.root = root
        self.events = []

    def path(self, name):
        return self.root / name.lstrip("/")

    def open(self, name, mode):
        return self.path(name).open(mode)

    def lstat(self, name):
        return self.path(name).lstat()

    def mkdir(self, name, mode=0o755):
        self.path(name).mkdir(mode=mode)

    def rmdir(self, name):
        self.path(name).rmdir()

    def remove(self, name):
        self.path(name).unlink()

    def chmod(self, name, mode):
        self.path(name).chmod(mode)

    def put(self, source, target, confirm):
        shutil.copyfile(source, self.path(target))
        self.events.append("upload")

    def rename(self, source, target):
        self.path(source).rename(self.path(target))
        self.events.append("executable-ready")

    def posix_rename(self, source, target):
        self.path(source).replace(self.path(target))
        self.events.append("manifest-ready")


class HttpResponse(io.BytesIO):
    headers = {"Cache-Control": "no-store"}


class PublisherTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.key = rsa.generate_private_key(public_exponent=65537, key_size=3072)

    def envelope(self, version, artifact=b"launcher bytes"):
        payload = {"version": version, "codename": "Aura", "architecture": "x64",
                   "path": f"/launcher/releases/{version}/EternalLauncher.exe", "size": len(artifact),
                   "sha256": hashlib.sha256(artifact).hexdigest()}
        raw = json.dumps(payload, separators=(",", ":")).encode()
        signature = self.key.sign(raw, padding.PSS(mgf=padding.MGF1(hashes.SHA256()), salt_length=32), hashes.SHA256())
        return json.dumps({"schemaVersion": 1, "payload": base64.b64encode(raw).decode(),
                           "signature": base64.b64encode(signature).decode()}).encode()

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        root = Path(self.temp.name)
        self.package = root / "package"
        (self.package / "launcher/releases/1.0.12").mkdir(parents=True)
        self.artifact = self.package / "launcher/releases/1.0.12/EternalLauncher.exe"
        self.artifact.write_bytes(b"launcher bytes")
        self.new = self.envelope("1.0.12")
        (self.package / "launcher/latest.json").write_bytes(self.new)
        self.sftp = FakeSFTP(root / "server")
        self.remote = self.sftp.path("/files/launcher")
        self.remote.mkdir(parents=True)
        self.latest = self.remote / "latest.json"
        self.old = self.envelope("1.0.11")
        self.latest.write_bytes(self.old)

    def publish(self, **kwargs):
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            return publisher.publish(self.sftp, self.package, "/files/launcher", "https://example.test", self.key.public_key(), **kwargs)

    def test_announce_only_after_https_artifact_verification(self):
        def verify(url, payload):
            self.assertEqual(self.latest.read_bytes(), self.old)
            self.sftp.events.append("https-verified")
        with patch.object(publisher, "verify_public_artifact", side_effect=verify), patch.object(
                publisher, "https_download", return_value=HttpResponse(self.new)):
            self.publish()
        self.assertEqual(self.latest.read_bytes(), self.new)
        self.assertEqual(self.sftp.events, ["upload", "executable-ready", "https-verified", "manifest-ready"])
        self.assertFalse((self.remote / ".ci-publish-lock").exists())

    def test_upload_verification_failure_preserves_old_announcement(self):
        with patch.object(publisher, "verify_public_artifact", side_effect=ValueError("Hash mismatch")):
            with self.assertRaises(ValueError):
                self.publish()
        self.assertEqual(self.latest.read_bytes(), self.old)
        self.assertNotIn("manifest-ready", self.sftp.events)

    def test_public_manifest_failure_restores_previous_announcement(self):
        with patch.object(publisher, "verify_public_artifact"), patch.object(
                publisher, "https_download", return_value=HttpResponse(b"invalid json")):
            with self.assertRaises(ValueError):
                self.publish()
        self.assertEqual(self.latest.read_bytes(), self.old)
        self.assertEqual(self.sftp.events.count("manifest-ready"), 2)

    def test_old_or_same_version_is_rejected(self):
        self.latest.write_bytes(self.new)
        with self.assertRaisesRegex(ValueError, "higher"):
            self.publish()
        self.assertEqual(self.sftp.events, [])

    def test_tampered_package_is_rejected_before_remote_changes(self):
        self.artifact.write_bytes(b"tampered bytes")
        with self.assertRaises(ValueError):
            self.publish()
        self.assertEqual(self.latest.read_bytes(), self.old)
        self.assertEqual(self.sftp.events, [])
        self.assertFalse((self.remote / ".ci-publish-lock").exists())

    def test_foreign_signing_key_is_rejected(self):
        foreign = rsa.generate_private_key(public_exponent=65537, key_size=3072)
        with self.assertRaises(Exception):
            publisher.decode_manifest(self.new, foreign.public_key())

    def test_check_only_leaves_no_release_or_lock(self):
        self.publish(check_only=True)
        self.assertEqual(self.latest.read_bytes(), self.old)
        self.assertEqual(list(self.remote.iterdir()), [self.latest])

    def test_concurrent_publisher_is_rejected(self):
        (self.remote / ".ci-publish-lock").mkdir()
        with self.assertRaises(FileExistsError):
            self.publish()
        self.assertEqual(self.latest.read_bytes(), self.old)
        self.assertTrue((self.remote / ".ci-publish-lock").is_dir())

    def test_https_download_sends_custom_user_agent(self):
        with patch.object(publisher, "urlopen", return_value=HttpResponse(b"")) as opener:
            publisher.https_download("https://example.test/", "/launcher/latest.json")
        request = opener.call_args.args[0]
        self.assertEqual(request.full_url, "https://example.test/launcher/latest.json")
        self.assertEqual(request.get_header("User-agent"), publisher.USER_AGENT)
        self.assertNotIn("urllib", request.get_header("User-agent").lower())

    def test_https_error_reports_status(self):
        error = publisher.HTTPError("https://example.test/x", 403, "Forbidden", {"CF-RAY": "abc"}, None)
        with patch.object(publisher, "urlopen", side_effect=error):
            with self.assertRaisesRegex(RuntimeError, "HTTP 403 Forbidden \\(CF-RAY abc\\)"):
                publisher.https_download("https://example.test", "/x")

    def test_retry_does_not_overwrite_existing_different_release(self):
        destination = self.remote / "releases/1.0.12/EternalLauncher.exe"
        destination.parent.mkdir(parents=True)
        destination.write_bytes(b"another executable")
        with self.assertRaises(ValueError):
            self.publish()
        self.assertEqual(destination.read_bytes(), b"another executable")
        self.assertEqual(self.latest.read_bytes(), self.old)


    def test_news_is_validated_and_published_atomically(self):
        news = Path(self.temp.name) / "news.json"
        news.write_text(json.dumps({"schemaVersion": 1, "items": [
            {"category": "AVISO", "title": "Olá", "date": "2026-10-26", "url": "https://example.test"}]}), encoding="utf-8")
        with contextlib.redirect_stdout(io.StringIO()):
            publisher.publish_news(self.sftp, news, "/files/launcher", check_only=True)
        self.assertFalse((self.remote / "news.json").exists())
        with contextlib.redirect_stdout(io.StringIO()):
            publisher.publish_news(self.sftp, news, "/files/launcher")
        self.assertEqual((self.remote / "news.json").read_bytes(), news.read_bytes())
        self.assertEqual(sorted(path.name for path in self.remote.iterdir()), ["latest.json", "news.json"])

    def test_invalid_news_is_rejected_before_upload(self):
        news = Path(self.temp.name) / "news.json"
        for document in ({"schemaVersion": 1, "items": []},
                         {"schemaVersion": 1, "items": [{"title": "x", "url": "http://insecure.test"}]},
                         {"schemaVersion": 1, "items": [{"title": "x", "date": "26/10/2026"}]},
                         {"schemaVersion": 2, "items": [{"title": "x"}]}):
            news.write_text(json.dumps(document), encoding="utf-8")
            with self.assertRaises(ValueError):
                publisher.publish_news(self.sftp, news, "/files/launcher")
        self.assertFalse((self.remote / "news.json").exists())

    def test_repository_news_is_valid(self):
        repository = Path(__file__).resolve().parents[1] / "news" / "news.json"
        self.assertGreater(len(publisher.validate_news(repository.read_bytes())["items"]), 0)

if __name__ == "__main__":
    unittest.main()
