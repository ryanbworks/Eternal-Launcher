"""Local tests for the automatic mod manifest generation."""

import hashlib
import json
import os
import tempfile
import time
import unittest
from pathlib import Path

from refresh_manifest import refresh


class RefreshManifestTests(unittest.TestCase):
    def test_upload_e_atualizacao_sem_editar_json(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            mods = root / "mods"
            mods.mkdir()
            manifest = root / "manifest.json"
            manifest.write_text(json.dumps({
                "schemaVersion": 1,
                "files": [{"path": "config/grupo.toml", "sha256": "abc"}],
            }), encoding="utf-8")
            mod = mods / "teste-1.20.1.jar"
            mod.write_bytes(b"versao-1")
            old = time.time() - 10
            os.utime(mod, (old, old))
            cache = root / "cache.json"

            self.assertEqual(refresh(root, cache), 1)
            entries = json.loads(manifest.read_text(encoding="utf-8"))["files"]
            self.assertEqual(entries[0]["path"], "config/grupo.toml")
            self.assertEqual(entries[1], {
                "path": "mods/teste-1.20.1.jar",
                "sha256": hashlib.sha256(b"versao-1").hexdigest(),
                "size": 8,
            })

            # Even with the same size, a replacement must change the hash.
            mod.write_bytes(b"versao-2")
            os.utime(mod, (old + 1, old + 1))
            self.assertEqual(refresh(root, cache), 1)
            entries = json.loads(manifest.read_text(encoding="utf-8"))["files"]
            self.assertEqual(entries[1]["sha256"], hashlib.sha256(b"versao-2").hexdigest())

            mod.unlink()
            self.assertEqual(refresh(root, cache), 0)
            entries = json.loads(manifest.read_text(encoding="utf-8"))["files"]
            self.assertEqual(len(entries), 1)

            # Real mod names can contain spaces and brackets.
            named_mod = mods / "[1.20.1] SecurityCraft v1.10.1.jar"
            named_mod.write_bytes(b"mod")
            os.utime(named_mod, (old, old))
            self.assertEqual(refresh(root, cache), 1)
            entries = json.loads(manifest.read_text(encoding="utf-8"))["files"]
            self.assertEqual(entries[1]["path"], f"mods/{named_mod.name}")


if __name__ == "__main__":
    unittest.main()
