#!/usr/bin/env python3
"""Atualiza o manifesto HTTP a partir dos mods enviados por SFTP."""

import argparse
import hashlib
import json
import re
import time
from pathlib import Path


MOD_NAME = re.compile(r"[A-Za-z0-9][A-Za-z0-9._+-]*\.jar", re.IGNORECASE)
QUIET_SECONDS = 5


def read_json(path: Path, default: dict) -> dict:
    if not path.exists():
        return default
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError(f"JSON inválido em {path}")
    return value


def write_json_atomic(path: Path, value: dict) -> None:
    # A troca atômica impede que o launcher leia JSON pela metade.
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


def refresh(files_root: Path, cache_path: Path) -> int:
    manifest_path = files_root / "manifest.json"
    mods_root = files_root / "mods"
    if not mods_root.is_dir():
        raise FileNotFoundError(f"Pasta de mods ausente: {mods_root}")

    manifest = read_json(manifest_path, {"schemaVersion": 1, "files": []})
    previous_files = manifest.get("files")
    if not isinstance(previous_files, list):
        raise ValueError("O campo files do manifesto precisa ser uma lista")
    cache = read_json(cache_path, {})
    previous_mods = {
        entry["path"]: entry
        for entry in previous_files
        if isinstance(entry, dict)
        and isinstance(entry.get("path"), str)
        and entry["path"].startswith("mods/")
    }
    other_files = [
        entry
        for entry in previous_files
        if not (isinstance(entry, dict)
                and isinstance(entry.get("path"), str)
                and entry["path"].startswith("mods/"))
    ]

    now_ns = time.time_ns()
    mods = []
    updated_cache = {}
    for path in sorted(mods_root.iterdir(), key=lambda item: item.name.lower()):
        if path.is_symlink() or not path.is_file() or path.suffix.lower() != ".jar":
            continue
        if not MOD_NAME.fullmatch(path.name):
            raise ValueError(f"Nome de mod não suportado: {path.name}")

        relative = f"mods/{path.name}"
        stat = path.stat()
        stamp = f"{stat.st_size}:{stat.st_mtime_ns}"
        old_cache = cache.get(relative, {})
        old_entry = previous_mods.get(relative)
        if (old_cache.get("stamp") == stamp and old_entry
                and isinstance(old_cache.get("sha256"), str)):
            digest = old_cache.get("sha256")
        elif now_ns - stat.st_mtime_ns < QUIET_SECONDS * 1_000_000_000:
            # Um upload ainda em andamento não entra no manifesto.
            if old_entry:
                mods.append(old_entry)
            continue
        else:
            digest = sha256(path)
            # Se o upload mudar durante o hash, esperamos a próxima verificação.
            after = path.stat()
            if after.st_size != stat.st_size or after.st_mtime_ns != stat.st_mtime_ns:
                if old_entry:
                    mods.append(old_entry)
                continue

        entry = {"path": relative, "sha256": digest, "size": stat.st_size}
        mods.append(entry)
        updated_cache[relative] = {"stamp": stamp, "sha256": digest}

    result = dict(manifest)
    result["files"] = other_files + mods
    cache_path.parent.mkdir(parents=True, exist_ok=True)
    if result != manifest:
        write_json_atomic(manifest_path, result)
    if updated_cache != cache:
        write_json_atomic(cache_path, updated_cache)
    return len(mods)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--files-root", type=Path,
                        default=Path("/srv/eternal-launcher/files"))
    parser.add_argument("--cache", type=Path,
                        default=Path("/srv/eternal-launcher/mods-cache.json"))
    arguments = parser.parse_args()
    count = refresh(arguments.files_root, arguments.cache)
    print(f"Manifesto atualizado: {count} mod(s).")
