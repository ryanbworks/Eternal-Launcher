"""Tests for the launcher's first access to reserved Conta Eternal accounts."""

import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "drasl"))
from first_access import ApiError, FirstAccess  # noqa: E402


class FakeDrasl:
    def __init__(self, passwords):
        self.passwords = dict(passwords)

    def login(self, username, password):
        return f"token-{username}" if self.passwords.get(username.lower()) == password else None

    def set_password(self, token, password):
        self.passwords[token[len("token-"):].lower()] = password


class FirstAccessTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.pending = Path(self.directory.name) / "pending.txt"
        self.pending.write_text("Nippw;tempA\nRyan;tempB\n", encoding="utf-8")
        self.drasl = FakeDrasl({"nippw": "tempA", "ryan": "changed-already"})
        self.service = FirstAccess(self.pending, self.drasl)

    def assert_status(self, status, action, *arguments):
        with self.assertRaises(ApiError) as caught:
            action(*arguments)
        self.assertEqual(status, caught.exception.status)

    def test_a_reserved_name_with_its_temporary_password_is_a_first_access(self):
        self.assertEqual({"firstAccess": True, "name": "Nippw"}, self.service.status("nippw"))

    def test_a_changed_password_or_unknown_name_is_not(self):
        self.assertFalse(self.service.status("Ryan")["firstAccess"])
        self.assertFalse(self.service.status("Stranger")["firstAccess"])
        self.assert_status(400, self.service.status, "<bad>")

    def test_claiming_sets_the_password_once(self):
        self.assertEqual({"name": "Nippw", "claimed": True}, self.service.claim("NIPPW", "minhasenha123"))
        self.assertEqual("minhasenha123", self.drasl.passwords["nippw"])
        self.assertNotIn("Nippw", self.pending.read_text())
        self.assert_status(409, self.service.claim, "Nippw", "outrasenha123")

    def test_passwords_must_be_new_and_long_enough(self):
        self.assert_status(400, self.service.claim, "Nippw", "curta")
        self.assert_status(400, self.service.claim, "Nippw", "tempA")
        self.assert_status(409, self.service.claim, "Ryan", "minhasenha123")


if __name__ == "__main__":
    unittest.main()
