#!/usr/bin/env python3
"""Move the group's offline-mode players to Drasl accounts without losing anything.

Each player of the Forge server's usercache.json gets a Drasl account whose player has
exactly the UUID the world already uses, so inventories, bases, teams and claims stay
with their owner when the server switches to online-mode. The accounts get temporary
passwords that the admin hands out privately; players change them on the website.

  bootstrap                    create the admin account and keep its API token
  reserve USERCACHE PLAYERDATA [--usernamecache F] [--dry-run]
                               create the missing accounts, keep name;password
  verify USERCACHE PLAYERDATA [--usernamecache F]
                               every player's Drasl UUID owns their playerdata
"""

import argparse
import hashlib
import json
import os
import re
import secrets
import sys
import urllib.error
import urllib.parse
import urllib.request
import uuid as uuidlib
from pathlib import Path

NICKNAME = re.compile(r"[A-Za-z0-9_]{3,16}")
ADMIN = "eternal_admin"
TOKEN_FILE = Path(os.environ.get("DRASL_ADMIN_FILE", "/root/.drasl-admin"))


def offline_uuid(name):
    """The UUID an online-mode=false server gives `name` (Java's nameUUIDFromBytes)."""
    digest = bytearray(hashlib.md5(f"OfflinePlayer:{name}".encode("utf-8")).digest())
    digest[6] = (digest[6] & 0x0F) | 0x30
    digest[8] = (digest[8] & 0x3F) | 0x80
    return str(uuidlib.UUID(bytes=bytes(digest)))


def read_usercache(path):
    """[(name, uuid)] from usercache.json, newest entry per name, sorted by name."""
    entries = {}
    for entry in json.loads(Path(path).read_text(encoding="utf-8")):
        name, player_uuid = entry.get("name"), entry.get("uuid")
        if not isinstance(name, str) or not NICKNAME.fullmatch(name) or not isinstance(player_uuid, str):
            continue
        entries[name.lower()] = (name, str(uuidlib.UUID(player_uuid)))
    return sorted(entries.values(), key=lambda item: item[0].lower())


FTB_PLAYER_NAME = re.compile(r'player_name:\s*"([A-Za-z0-9_]{3,16})"')


def read_players(usercache, playerdata, usernamecache=None, ftbteams=None):
    """The players to migrate: [(name, uuid)] with uuid = the offline UUID of the name.

    The world keeps each player under the offline UUID of the name they typed. The
    caches may also hold Mojang UUIDs (skull lookups, original accounts), so they only
    supply names; a name counts when its offline UUID owns a playerdata file.
    Returns (players, orphans): orphans are playerdata files no known name explains.
    """
    names = {name for name, _ in read_usercache(usercache)}
    if usernamecache and Path(usernamecache).exists():
        for value in json.loads(Path(usernamecache).read_text(encoding="utf-8")).values():
            if isinstance(value, str) and NICKNAME.fullmatch(value):
                names.add(value)
    # FTB Teams keeps each player's name in world/ftbteams/player/<uuid>.snbt.
    if ftbteams and Path(ftbteams).is_dir():
        for snbt in Path(ftbteams).glob("*.snbt"):
            found = FTB_PLAYER_NAME.search(snbt.read_text(encoding="utf-8", errors="replace"))
            if found:
                names.add(found.group(1))
    data ={path.stem for path in Path(playerdata).glob("*.dat")}
    players = {}
    for name in names:
        candidate = offline_uuid(name)
        if candidate in data:
            players[candidate] = name
    orphans = sorted(data - set(players))
    return sorted(((name, player_uuid) for player_uuid, name in players.items()), key=lambda item: item[0].lower()), orphans


def temporary_password():
    # Readable for a chat message: no look-alike characters.
    alphabet = "abcdefghjkmnpqrstuvwxyzABCDEFGHJKMNPQRSTUVWXYZ23456789"
    return "".join(secrets.choice(alphabet) for _ in range(14))


class Drasl:
    def __init__(self, base_url, token=None):
        self.base_url = base_url.rstrip("/")
        self.token = token

    def request(self, method, path, body=None, authenticated=True):
        data = json.dumps(body).encode("utf-8") if body is not None else None
        request = urllib.request.Request(self.base_url + path, data=data, method=method)
        request.add_header("Content-Type", "application/json")
        if authenticated and self.token:
            request.add_header("Authorization", f"Bearer {self.token}")
        try:
            with urllib.request.urlopen(request, timeout=20) as response:
                text = response.read().decode("utf-8")
                return response.status, json.loads(text) if text else None
        except urllib.error.HTTPError as error:
            text = error.read().decode("utf-8", "replace")
            try:
                return error.code, json.loads(text)
            except ValueError:
                return error.code, {"message": text}

    def profile(self, name):
        """{'id', 'name'} of a player Drasl knows, or None."""
        status, body = self.request("GET", "/account/users/profiles/minecraft/" + urllib.parse.quote(name), authenticated=False)
        return body if status == 200 and isinstance(body, dict) and body.get("id") else None

    def create_user(self, name, password, player_uuid):
        return self.request("POST", "/drasl/api/v3/users", {
            "username": name, "password": password, "playerName": name, "chosenUuid": player_uuid,
        })


def dashed(value):
    return str(uuidlib.UUID(value))


def bootstrap(drasl):
    if TOKEN_FILE.exists():
        print(f"admin token already kept in {TOKEN_FILE}")
        return 0
    password = temporary_password() + temporary_password()
    status, body = drasl.request("POST", "/drasl/api/v3/users", {
        "username": ADMIN, "password": password, "requestApiToken": True,
        "playerName": ADMIN[:16],
    }, authenticated=False)
    if status != 200 or not body.get("apiToken"):
        print(f"could not create {ADMIN}: HTTP {status} {body}", file=sys.stderr)
        return 1
    if not body["user"].get("isAdmin"):
        print(f"{ADMIN} was created but is not an admin: add it to DefaultAdmins and restart Drasl", file=sys.stderr)
    TOKEN_FILE.write_text(json.dumps({"username": ADMIN, "password": password, "apiToken": body["apiToken"]}))
    TOKEN_FILE.chmod(0o600)
    print(f"{ADMIN} created; password and API token kept in {TOKEN_FILE}")
    return 0


def plan(players, lookup):
    """What reserve would do for each player: 'ok', 'create' or a conflict message."""
    steps = []
    for name, player_uuid in players:
        existing = lookup(name)
        if existing is None:
            steps.append((name, player_uuid, "create"))
        elif dashed(existing["id"]) == player_uuid:
            steps.append((name, player_uuid, "ok"))
        else:
            steps.append((name, player_uuid, f"CONFLICT: Drasl already has {existing['name']} with {dashed(existing['id'])}"))
    return steps


def reserve(drasl, usercache, playerdata, usernamecache, ftbteams, dry_run, output):
    players, orphans = read_players(usercache, playerdata, usernamecache, ftbteams)
    for orphan in orphans:
        print(f"{'?':<16} {orphan}  NO NAME: this player's data has no known nickname; it stays as it is")
    steps = plan(players, drasl.profile)
    created, failures = [], 0
    for name, player_uuid, action in steps:
        if action != "create":
            print(f"{name:<16} {player_uuid}  {action}")
            failures += action.startswith("CONFLICT")
            continue
        if dry_run:
            print(f"{name:<16} {player_uuid}  would create")
            continue
        password = temporary_password()
        status, body = drasl.create_user(name, password, player_uuid)
        if status == 200:
            got = dashed(body["user"]["players"][0]["uuid"])
            mark = "created" if got == player_uuid else f"CREATED WITH WRONG UUID {got}"
            failures += got != player_uuid
            created.append((name, password))
            print(f"{name:<16} {player_uuid}  {mark}")
        else:
            failures += 1
            print(f"{name:<16} {player_uuid}  FAILED: HTTP {status} {body}")
    if created:
        with open(output, "a", encoding="utf-8") as handle:
            for name, password in created:
                handle.write(f"{name};{password}\n")
        os.chmod(output, 0o600)
        print(f"{len(created)} temporary passwords appended to {output}")
    return 1 if failures else 0


def verify(drasl, usercache, playerdata, usernamecache, ftbteams):
    problems = 0
    players, _ = read_players(usercache, playerdata, usernamecache, ftbteams)
    for name, player_uuid in players:
        existing = drasl.profile(name)
        has_data = (Path(playerdata) / f"{player_uuid}.dat").exists()
        if existing is None:
            state = "MISSING in Drasl"
        elif dashed(existing["id"]) != player_uuid:
            state = f"UUID MISMATCH (Drasl {dashed(existing['id'])})"
        else:
            state = "ok"
        problems += state != "ok"
        print(f"{name:<16} {player_uuid}  {state:<40} {'playerdata ok' if has_data else 'no playerdata'}")
    print("all players keep their UUID" if not problems else f"{problems} problem(s)")
    return 1 if problems else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--url", default=os.environ.get("DRASL_URL", "http://127.0.0.1:25585"))
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("bootstrap")
    reserve_parser = commands.add_parser("reserve")
    reserve_parser.add_argument("usercache")
    reserve_parser.add_argument("playerdata")
    reserve_parser.add_argument("--usernamecache")
    reserve_parser.add_argument("--ftbteams")
    reserve_parser.add_argument("--dry-run", action="store_true")
    reserve_parser.add_argument("--output", default="/root/eternal-temporary-passwords.txt")
    verify_parser = commands.add_parser("verify")
    verify_parser.add_argument("usercache")
    verify_parser.add_argument("playerdata")
    verify_parser.add_argument("--usernamecache")
    verify_parser.add_argument("--ftbteams")
    arguments = parser.parse_args()

    token = None
    if arguments.command == "reserve" and not arguments.dry_run:
        if not TOKEN_FILE.exists():
            print(f"run bootstrap first ({TOKEN_FILE} is missing)", file=sys.stderr)
            return 1
        token = json.loads(TOKEN_FILE.read_text())["apiToken"]
    drasl = Drasl(arguments.url, token)
    if arguments.command == "bootstrap":
        return bootstrap(drasl)
    if arguments.command == "reserve":
        return reserve(drasl, arguments.usercache, arguments.playerdata, arguments.usernamecache, arguments.ftbteams, arguments.dry_run, arguments.output)
    return verify(drasl, arguments.usercache, arguments.playerdata, arguments.usernamecache, arguments.ftbteams)


if __name__ == "__main__":
    sys.exit(main())
