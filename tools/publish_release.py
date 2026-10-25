"""Publish a signed launcher package over SFTP, announcing it only after HTTPS verification."""
from __future__ import annotations

import argparse
import base64
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import struct
import sys
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen
import uuid

import paramiko
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import padding, rsa

MAX_MANIFEST = 65536
MAX_EXECUTABLE = 268435456
USER_AGENT = "EternalLauncher-Publisher/1.0 (+https://github.com/ryanbworks/Eternal-Launcher)"


def public_key(header: Path):
    blob = bytes(int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]{2})", header.read_text()))
    if len(blob) < 24:
        raise ValueError("Chave pública ausente ou inválida")
    magic, bits, exponent_size, modulus_size, _, _ = struct.unpack("<6I", blob[:24])
    if magic != 0x31415352 or bits < 3072 or len(blob) != 24 + exponent_size + modulus_size:
        raise ValueError("Chave pública inválida")
    exponent = int.from_bytes(blob[24:24 + exponent_size], "big")
    modulus = int.from_bytes(blob[24 + exponent_size:], "big")
    return rsa.RSAPublicNumbers(exponent, modulus).public_key()


def decode_manifest(raw: bytes, key) -> dict:
    if len(raw) > MAX_MANIFEST:
        raise ValueError("Manifesto excede 64 KiB")
    envelope = json.loads(raw)
    if envelope.get("schemaVersion") != 1:
        raise ValueError("Schema incompatível")
    encoded = base64.b64decode(envelope["payload"], validate=True)
    key.verify(base64.b64decode(envelope["signature"], validate=True), encoded,
               padding.PSS(mgf=padding.MGF1(hashes.SHA256()), salt_length=32), hashes.SHA256())
    payload = json.loads(encoded)
    version = payload.get("version", "")
    if not re.fullmatch(r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)", version):
        raise ValueError("Versão inválida")
    if any(int(part) > 65535 for part in version.split(".")):
        raise ValueError("Versão fora do intervalo suportado")
    if (payload.get("architecture") != "x64"
            or payload.get("path") != f"/launcher/releases/{version}/EternalLauncher.exe"
            or not re.fullmatch(r"[A-Za-z0-9]{1,40}", payload.get("codename", ""))
            or type(payload.get("size")) is not int
            or not 0 < payload["size"] <= MAX_EXECUTABLE
            or not re.fullmatch(r"[a-f0-9]{64}", payload.get("sha256", ""))):
        raise ValueError("Dados inválidos no manifesto")
    return payload


def verify_stream(stream, payload: dict):
    digest = hashlib.sha256()
    size = 0
    while chunk := stream.read(65536):
        size += len(chunk)
        if size > payload["size"]:
            raise ValueError("Executável maior que o manifesto")
        digest.update(chunk)
    if size != payload["size"] or digest.hexdigest() != payload["sha256"]:
        raise ValueError("Tamanho ou SHA-256 do executável não confere")


def https_download(base_url: str, resource: str):
    # The production domain is behind Cloudflare, which rejects the default
    # "Python-urllib" User-Agent. Identify as the launcher itself would.
    url = base_url.rstrip("/") + resource
    request = Request(url, headers={"Cache-Control": "no-cache", "User-Agent": USER_AGENT})
    try:
        return urlopen(request, timeout=30)
    except HTTPError as error:
        ray = error.headers.get("CF-RAY", "") if error.headers else ""
        detail = f" (CF-RAY {ray})" if ray else ""
        raise RuntimeError(f"Falha ao conferir {url} pelo HTTPS: HTTP {error.code} {error.reason}{detail}") from error
    except URLError as error:
        raise RuntimeError(f"Falha ao conferir {url} pelo HTTPS: {error.reason}") from error


def verify_public_artifact(base_url: str, payload: dict):
    with https_download(base_url, payload["path"]) as response:
        if response.geturl().split(":", 1)[0] != "https":
            raise ValueError("O download precisa permanecer em HTTPS")
        verify_stream(response, payload)


def remote_bytes(sftp, name: str) -> bytes | None:
    try:
        with sftp.open(name, "rb") as stream:
            raw = stream.read(MAX_MANIFEST + 1)
    except FileNotFoundError:
        return None
    if len(raw) > MAX_MANIFEST:
        raise ValueError("Manifesto remoto excede 64 KiB")
    return raw


def ensure_directory(sftp, name: str):
    try:
        attributes = sftp.lstat(name)
    except FileNotFoundError:
        sftp.mkdir(name, mode=0o755)
    else:
        if not stat.S_ISDIR(attributes.st_mode):
            raise ValueError("O destino deve ser uma pasta real")


def publish(sftp, package: Path, remote_root: str, base_url: str, key, *, check_only=False):
    manifest = (package / "launcher/latest.json").read_bytes()
    payload = decode_manifest(manifest, key)
    artifact = package / payload["path"].lstrip("/")
    with artifact.open("rb") as stream:
        verify_stream(stream, payload)
    root = PurePosixPath(remote_root)
    if not root.is_absolute() or ".." in root.parts:
        raise ValueError("O destino remoto deve ser uma pasta absoluta")
    latest = str(root / "latest.json")
    lock = str(root / ".ci-publish-lock")
    sftp.mkdir(lock, mode=0o700)  # Atomic lock shared by all publishers using this protocol.
    pending = []
    announced = False
    old_manifest = None
    suffix = uuid.uuid4().hex
    try:
        old_manifest = remote_bytes(sftp, latest)
        if old_manifest is not None:
            old_payload = decode_manifest(old_manifest, key)
            old_version = tuple(map(int, old_payload["version"].split(".")))
            version = tuple(map(int, payload["version"].split(".")))
            if version <= old_version:
                raise ValueError(f"A versão deve ser maior que a publicada ({old_payload['version']})")
        if check_only:
            print(f"Conexão, assinatura e versão verificadas: {payload['version']}. Nenhuma release enviada.")
            return
        releases = str(root / "releases")
        folder = str(root / "releases" / payload["version"])
        ensure_directory(sftp, releases)
        ensure_directory(sftp, folder)
        target = str(PurePosixPath(folder) / "EternalLauncher.exe")
        try:
            existing = sftp.lstat(target)
        except FileNotFoundError:
            existing = None
        if existing is not None:
            if not stat.S_ISREG(existing.st_mode):
                raise ValueError("Executável remoto não é um arquivo regular")
            with sftp.open(target, "rb") as stream:
                verify_stream(stream, payload)  # A retry can reuse identical immutable bytes.
        else:
            temporary = target + f".{suffix}.upload"
            pending.append(temporary)
            sftp.put(str(artifact), temporary, confirm=True)
            sftp.chmod(temporary, 0o644)
            sftp.rename(temporary, target)
        verify_public_artifact(base_url, payload)
        temporary_manifest = str(root / f"latest.{suffix}.json.upload")
        pending.append(temporary_manifest)
        with sftp.open(temporary_manifest, "wb") as stream:
            stream.write(manifest)
        sftp.chmod(temporary_manifest, 0o644)
        if remote_bytes(sftp, latest) != old_manifest:
            raise RuntimeError("O manifesto mudou durante a publicação; operação interrompida")
        sftp.posix_rename(temporary_manifest, latest)
        announced = True
        with https_download(base_url, "/launcher/latest.json") as response:
            if "no-store" not in response.headers.get("Cache-Control", "").lower():
                raise ValueError("latest.json precisa de Cache-Control: no-store")
            public_manifest = response.read(MAX_MANIFEST + 1)
        if decode_manifest(public_manifest, key) != payload:
            raise ValueError("O manifesto público não corresponde à release")
        print(f"Publicado e conferido: {payload['version']}-{payload['codename']}; SHA-256 {payload['sha256']}")
    except Exception:
        if announced and remote_bytes(sftp, latest) == manifest:
            if old_manifest is None:
                sftp.remove(latest)
            else:
                rollback = str(root / f"rollback.{suffix}.json.upload")
                pending.append(rollback)
                with sftp.open(rollback, "wb") as stream:
                    stream.write(old_manifest)
                sftp.chmod(rollback, 0o644)
                sftp.posix_rename(rollback, latest)
            print("O anúncio da atualização foi revertido após falha na conferência.", file=sys.stderr)
        raise
    finally:
        for name in pending:
            try:
                sftp.remove(name)
            except FileNotFoundError:
                pass
        sftp.rmdir(lock)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", type=Path, required=True)
    parser.add_argument("--host", default=os.environ.get("SFTP_HOST", "10.0.0.13"))
    parser.add_argument("--port", type=int, default=22)
    parser.add_argument("--user", default="eternal-deploy")
    parser.add_argument("--remote-root", default="/files/launcher")
    parser.add_argument("--base-url", default="https://eternalfast.aplot.cloud")
    parser.add_argument("--public-key", type=Path, default=Path(__file__).resolve().parents[1] / "src/config/UpdatePublicKey.h")
    parser.add_argument("--check-only", action="store_true")
    args = parser.parse_args()
    if args.base_url != "https://eternalfast.aplot.cloud":
        parser.error("A publicação deve ser conferida no domínio de produção configurado")
    key_text = os.environ.pop("SFTP_PRIVATE_KEY", "")
    known_hosts = os.environ.pop("SFTP_KNOWN_HOSTS", "")
    if not key_text or not known_hosts:
        parser.error("Secrets SFTP_PRIVATE_KEY e SFTP_KNOWN_HOSTS são obrigatórios")
    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.RejectPolicy())
    for line in known_hosts.splitlines():
        if line.strip() and not line.lstrip().startswith("#"):
            entry = paramiko.hostkeys.HostKeyEntry.from_line(line)
            if entry is None:
                raise ValueError("Secret SFTP_KNOWN_HOSTS inválido")
            for hostname in entry.hostnames:
                client.get_host_keys().add(hostname, entry.key.get_name(), entry.key)
    private_key = paramiko.Ed25519Key.from_private_key(io.StringIO(key_text))
    key_text = None
    try:
        client.connect(args.host, port=args.port, username=args.user, pkey=private_key,
                       look_for_keys=False, allow_agent=False, timeout=30, auth_timeout=30, banner_timeout=30)
        client.get_transport().set_keepalive(15)
        with client.open_sftp() as sftp:
            sftp.get_channel().settimeout(60)
            publish(sftp, args.package, args.remote_root, args.base_url, public_key(args.public_key), check_only=args.check_only)
    finally:
        client.close()


if __name__ == "__main__":
    main()
