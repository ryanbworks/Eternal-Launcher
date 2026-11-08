#!/usr/bin/env python3
"""Refresh the HTTP manifest of mods, configs, resource packs and shader packs."""

import argparse
import hashlib
import json
import os
import time
from pathlib import Path, PurePosixPath


QUIET_SECONDS = 5
MANAGED_ROOTS = ("mods", "config", "resourcepacks", "shaderpacks")
TEMPORARY_SUFFIXES = (".part", ".filepart", ".tmp", ".upload")


def read_json(path: Path, default: dict) -> dict:
    if not path.exists():
        return default
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError(f"Invalid JSON in {path}")
    return value


def write_json_atomic(path: Path, value: dict) -> None:
    # The atomic swap keeps the launcher from reading half-written JSON.
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(
        json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    temporary.chmod(0o644)
    temporary.replace(path)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def windows_safe_relative(relative: str) -> bool:
    """Accept only relative paths that can be created on Windows."""
    path = PurePosixPath(relative)
    if not relative or path.is_absolute():
        return False
    for part in path.parts:
        if (part in ("", ".", "..")
                or any(ord(character) < 32 or character in '<>:"/\\|?*'
                       for character in part)
                or part.rstrip(" .") != part):
            return False
    return True


def publishable_relative(relative: str) -> bool:
    # NGINX blocks any path component starting with a dot. Such entries cannot
    # stay in the manifest, because a single HTTP 403 stops the launcher.
    return (windows_safe_relative(relative)
            and not any(part.startswith(".") for part in PurePosixPath(relative).parts)
            and not relative.lower().endswith(TEMPORARY_SUFFIXES))


def files_to_publish(files_root: Path):
    """Yield (relative path, local path) for files managed by this service."""
    mods_root = files_root / "mods"
    if mods_root.is_dir() and not mods_root.is_symlink():
        for path in sorted(mods_root.iterdir(), key=lambda item: item.name.lower()):
            if (path.is_symlink() or not path.is_file()
                    or path.suffix.lower() != ".jar"):
                continue
            relative = f"mods/{path.name}"
            if publishable_relative(relative):
                yield relative, path
            else:
                print(f"Skipping file with an unpublishable path: {relative}")

    for name in MANAGED_ROOTS[1:]:
        root = files_root / name
        if not root.is_dir() or root.is_symlink():
            continue
        # Never visit hidden directories or links, including those pointing
        # outside the public folder. Unpacked packs are accepted too.
        for directory, subdirectories, filenames in os.walk(root):
            directory = Path(directory)
            subdirectories[:] = sorted(
                (part for part in subdirectories
                 if not (directory / part).is_symlink()
                 and publishable_relative((directory / part).relative_to(files_root).as_posix())),
                key=str.lower,
            )
            for filename in sorted(filenames, key=str.lower):
                path = directory / filename
                relative = path.relative_to(files_root).as_posix()
                if path.is_symlink() or not path.is_file():
                    continue
                if publishable_relative(relative):
                    yield relative, path
                else:
                    print(f"Skipping file with an unpublishable path: {relative}")


def refresh(files_root: Path, cache_path: Path) -> int:
    manifest_path = files_root / "manifest.json"
    if not (files_root / "mods").is_dir():
        raise FileNotFoundError(f"Mods folder missing: {files_root / 'mods'}")

    manifest = read_json(manifest_path, {"schemaVersion": 1, "files": []})
    previous_files = manifest.get("files")
    if not isinstance(previous_files, list):
        raise ValueError("The manifest's files field must be a list")

    managed_prefixes = tuple(f"{root}/" for root in MANAGED_ROOTS)
    previous_entries = {
        entry["path"]: entry
        for entry in previous_files
        if isinstance(entry, dict)
        and isinstance(entry.get("path"), str)
        and entry["path"].startswith(managed_prefixes)
    }
    other_files = [
        entry
        for entry in previous_files
        if not (isinstance(entry, dict)
                and isinstance(entry.get("path"), str)
                and entry["path"].startswith(managed_prefixes))
    ]

    cache = read_json(cache_path, {})
    now_ns = time.time_ns()
    published = []
    updated_cache = {}
    for relative, path in files_to_publish(files_root):
        stat = path.stat()
        stamp = f"{stat.st_size}:{stat.st_mtime_ns}"
        old_cache = cache.get(relative, {})
        old_entry = previous_entries.get(relative)

        if (old_cache.get("stamp") == stamp and old_entry
                and isinstance(old_cache.get("sha256"), str)):
            digest = old_cache["sha256"]
        elif now_ns - stat.st_mtime_ns < QUIET_SECONDS * 1_000_000_000:
            # An upload still in progress stays out of the manifest.
            if old_entry:
                published.append(old_entry)
            continue
        else:
            digest = sha256(path)
            # If the upload changes while hashing, wait for the next check.
            after = path.stat()
            if after.st_size != stat.st_size or after.st_mtime_ns != stat.st_mtime_ns:
                if old_entry:
                    published.append(old_entry)
                continue

        published.append({"path": relative, "sha256": digest, "size": stat.st_size})
        updated_cache[relative] = {"stamp": stamp, "sha256": digest}

    result = dict(manifest)
    result["files"] = other_files + published
    cache_path.parent.mkdir(parents=True, exist_ok=True)
    if result != manifest:
        write_json_atomic(manifest_path, result)
    if updated_cache != cache:
        write_json_atomic(cache_path, updated_cache)
    return len(published)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--files-root", type=Path,
                        default=Path("/srv/eternal-launcher/files"))
    parser.add_argument("--cache", type=Path,
                        default=Path("/srv/eternal-launcher/mods-cache.json"))
    arguments = parser.parse_args()
    count = refresh(arguments.files_root, arguments.cache)
    print(f"Manifest updated: {count} managed file(s).")
