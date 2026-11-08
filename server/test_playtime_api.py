"""Tests for the rules that keep the server-side play time trustworthy."""

import tempfile
import unittest
from pathlib import Path

from playtime_api import ApiError, MAX_CREDIT_GAP, MAX_SESSIONS_PER_IP, SESSION_TTL, PlaytimeStore

HOUR = 3600


class PlaytimeStoreTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.now = 1_000_000
        self.store = PlaytimeStore(
            str(Path(self.directory.name) / "playtime.db"), 200 * HOUR, 60, clock=lambda: self.now
        )

    def start(self, nickname="Steve", ip="1.1.1.1"):
        return self.store.start_session(nickname, ip)

    def assert_status(self, status, action, *arguments):
        with self.assertRaises(ApiError) as caught:
            action(*arguments)
        self.assertEqual(status, caught.exception.status)

    def test_time_comes_from_the_server_clock(self):
        token = self.start()["token"]
        self.now += 60
        self.assertEqual(60, self.store.heartbeat(token)["totalSeconds"])
        self.now += 45
        self.assertEqual(105, self.store.end_session(token)["totalSeconds"])

    def test_faster_heartbeats_never_earn_more_than_real_time(self):
        token = self.start()["token"]
        for _ in range(50):
            total = self.store.heartbeat(token)["totalSeconds"]
        self.assertEqual(0, total)

    def test_a_long_silence_earns_nothing_but_keeps_the_session(self):
        token = self.start()["token"]
        self.now += MAX_CREDIT_GAP + 1
        self.assertEqual(0, self.store.heartbeat(token)["totalSeconds"])
        self.now += 60
        self.assertEqual(60, self.store.heartbeat(token)["totalSeconds"])

    def test_expired_and_ended_sessions_are_rejected(self):
        token = self.start()["token"]
        self.now += SESSION_TTL + 1
        self.assert_status(404, self.store.heartbeat, token)
        other = self.start()["token"]
        self.store.end_session(other)
        self.assert_status(404, self.store.heartbeat, other)
        self.assert_status(404, self.store.heartbeat, "invented-token")

    def test_a_second_launcher_cannot_count_double(self):
        first = self.start()["token"]
        self.now += 30
        second = self.start(ip="2.2.2.2")["token"]
        self.now += 30
        self.assert_status(404, self.store.heartbeat, first)
        self.assertEqual(60, self.store.heartbeat(second)["totalSeconds"])

    def test_nicknames_are_case_insensitive_and_validated(self):
        token = self.start("Steve")["token"]
        self.now += 60
        self.store.heartbeat(token)
        self.assertEqual(60, self.start("STEVE")["totalSeconds"])
        for invalid in ("ab", "a" * 17, "bad name", "semi;colon", "", None, 5):
            self.assert_status(400, self.store.start_session, invalid, "1.1.1.1")

    def test_one_address_cannot_farm_many_nicknames(self):
        for index in range(MAX_SESSIONS_PER_IP):
            self.start(f"Player{index}")
        self.assert_status(429, self.start, "OneTooMany")
        self.start("Player0")  # Restarting an existing session is still allowed.

    def test_import_happens_once_and_is_capped(self):
        started = self.start()
        self.assertTrue(started["importAvailable"])
        result = self.store.import_local(started["token"], 900 * HOUR, self.now - 50)
        self.assertEqual(200 * HOUR, result["importedSeconds"])
        self.assertEqual(200 * HOUR, result["totalSeconds"])
        self.assert_status(409, self.store.import_local, started["token"], 10, 0)
        self.assertFalse(self.start()["importAvailable"])

    def test_import_adds_to_counted_time(self):
        token = self.start()["token"]
        self.now += 60
        self.store.heartbeat(token)
        self.assertEqual(3660, self.store.import_local(token, HOUR, 0)["totalSeconds"])

    def test_import_rejects_bad_values_without_using_the_chance(self):
        token = self.start()["token"]
        for invalid in (-1, 1.5, "100", True, None):
            self.assert_status(400, self.store.import_local, token, invalid, 0)
        self.assertEqual(5, self.store.import_local(token, 5, 0)["importedSeconds"])

    def test_import_closes_after_the_deadline(self):
        self.now += 60 * 86400
        started = self.start()
        self.assertFalse(started["importAvailable"])
        self.assert_status(403, self.store.import_local, started["token"], 10, 0)

    def test_a_future_last_played_is_not_stored(self):
        token = self.start()["token"]
        self.store.import_local(token, 10, self.now + 99999)
        self.assertLessEqual(self.store.leaderboard(5, "Steve")["you"]["lastPlayed"], self.now)

    def test_leaderboard_order_rank_and_online_state(self):
        slow = self.start("Slow", "1.1.1.1")["token"]
        fast = self.start("Fast", "2.2.2.2")["token"]
        self.store.import_local(fast, 2 * HOUR, 0)
        self.store.import_local(slow, HOUR, 0)
        self.store.end_session(slow)
        board = self.store.leaderboard(10, "slow")
        self.assertEqual(["Fast", "Slow"], [p["name"] for p in board["players"]])
        self.assertEqual([1, 2], [p["rank"] for p in board["players"]])
        self.assertEqual([True, False], [p["online"] for p in board["players"]])
        self.assertEqual(2, board["you"]["rank"])
        self.assertEqual(HOUR, board["you"]["totalSeconds"])
        self.assertIsNone(self.store.leaderboard(10, "Nobody")["you"])

    def test_players_without_time_stay_out_of_the_ranking(self):
        self.start("Fresh")
        self.assertEqual([], self.store.leaderboard()["players"])

    def test_administration(self):
        token = self.start()["token"]
        self.store.import_local(token, 100 * HOUR, 0)
        self.store.set_imported("steve", HOUR)
        self.assertEqual(HOUR, self.store.leaderboard(5, "Steve")["you"]["totalSeconds"])
        self.store.remove_player("Steve")
        self.assertIsNone(self.store.leaderboard(5, "Steve")["you"])
        self.assert_status(404, self.store.remove_player, "Steve")
        self.store.set_import_window(0)
        self.assertFalse(self.start("Other")["importAvailable"])


if __name__ == "__main__":
    unittest.main()
