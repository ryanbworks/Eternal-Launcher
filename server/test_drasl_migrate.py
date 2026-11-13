"""Tests for the rules that keep every player's UUID through the Drasl migration."""

import json
import tempfile
import unittest
from pathlib import Path

from drasl_migrate import offline_uuid, plan, read_players, read_usercache, temporary_password


class OfflineUuidTests(unittest.TestCase):
    def test_matches_minecraft(self):
        # Known values from online-mode=false servers (and the launcher's own builder).
        self.assertEqual("ebd6f9b1-f13d-3fff-8ec7-8f69247400b4", offline_uuid("Claude_Test"))
        self.assertNotEqual(offline_uuid("Steve"), offline_uuid("steve"))
        self.assertEqual("3", offline_uuid("Steve")[14])


class UsercacheTests(unittest.TestCase):
    def test_reads_valid_names_once(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "usercache.json"
            path.write_text(json.dumps([
                {"name": "Steve", "uuid": "8667ba71-b85a-4004-af54-457a9734eed7", "expiresOn": "x"},
                {"name": "<bad>", "uuid": "8667ba71-b85a-4004-af54-457a9734eed7"},
                {"name": "Aurora_", "uuid": "0123456789abcdef0123456789abcdef"},
                {"name": "Aurora_", "uuid": "11111111-2222-3333-4444-555555555555"},
            ]))
            self.assertEqual([("Aurora_", "11111111-2222-3333-4444-555555555555"),
                              ("Steve", "8667ba71-b85a-4004-af54-457a9734eed7")], read_usercache(path))


class PlayersTests(unittest.TestCase):
    def test_the_offline_uuid_with_data_wins_over_the_cached_one(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "playerdata").mkdir()
            # Ryan is cached with his Mojang UUID, but his data lives under the offline one.
            (root / "usercache.json").write_text(json.dumps([
                {"name": "Ryan", "uuid": "7d6676b5-a56c-4d29-a12a-df6432f186e5"},
                {"name": "MHF_Spider", "uuid": "5ad55f34-41b6-4bd2-9c32-18983c635936"},
            ]))
            (root / "usernamecache.json").write_text(json.dumps({offline_uuid("Luna"): "Luna"}))
            for name in ("Ryan", "Luna"):
                (root / "playerdata" / f"{offline_uuid(name)}.dat").touch()
            (root / "playerdata" / "2732e5cd-e1ab-3873-aa9a-ef328ab0bb96.dat").touch()
            players, orphans = read_players(root / "usercache.json", root / "playerdata", root / "usernamecache.json")
            self.assertEqual([("Luna", offline_uuid("Luna")), ("Ryan", offline_uuid("Ryan"))], players)
            self.assertEqual(["2732e5cd-e1ab-3873-aa9a-ef328ab0bb96"], orphans)


class PlanTests(unittest.TestCase):
    def test_creates_missing_keeps_matching_and_flags_conflicts(self):
        known = {
            "steve": {"id": "8667ba71b85a4004af54457a9734eed7", "name": "Steve"},
            "luna": {"id": "00000000000000000000000000000001", "name": "Luna"},
        }
        steps = plan([("Steve", "8667ba71-b85a-4004-af54-457a9734eed7"),
                      ("Luna", "8667ba71-b85a-4004-af54-457a9734eed8"),
                      ("Tico", "8667ba71-b85a-4004-af54-457a9734eed9")], lambda name: known.get(name.lower()))
        self.assertEqual("ok", steps[0][2])
        self.assertTrue(steps[1][2].startswith("CONFLICT"))
        self.assertEqual("create", steps[2][2])

    def test_temporary_passwords_are_readable_and_random(self):
        first, second = temporary_password(), temporary_password()
        self.assertEqual(14, len(first))
        self.assertNotEqual(first, second)
        self.assertFalse(set(first) & set("0O1lI"))


if __name__ == "__main__":
    unittest.main()
