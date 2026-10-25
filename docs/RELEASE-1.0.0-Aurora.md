# v1.0.0-Aurora

First stable release with executable AutoUpdate. Published on October 3, 2026 for
Windows x64.

- [Download EternalLauncher.exe](https://eternalfast.aplot.cloud/launcher/releases/1.0.0/EternalLauncher.exe)
- [Signed manifest](https://eternalfast.aplot.cloud/launcher/latest.json)
- [Build and publish the next versions](AUTOUPDATE.md)

## Changes

- Automatic check on startup, validated download and automatic restart.
- RSA-PSS/SHA-256 signature with an embedded public key.
- Backup and rollback if the new version doesn't confirm it opened within 30 seconds.
- Recovery after interruptions; failures let you keep using the installed version.
- Manual download link in the interface and a PowerShell 7 release tool.
- Version centralized in CMake and in the executable's properties.

Users of v0.1.1-Vagalume must replace the executable manually once. Later updates
are automatic starting with Aurora. The default maximum RAM stays at 4 GB, and
preferences and the game instance are preserved.

## Distribution evidence

```text
File:         EternalLauncher.exe
Version:      1.0.0-Aurora
Architecture: x64
Size:         3469824 bytes
SHA-256:      c1699d4ad1adc6ee6f892f0beeebd921e256807aa101432b5485d2af629b48ed
```

MSVC Release `/W4` build with no warnings. CTest: **5/5 passed**, including
**53 AutoUpdate checks** with a test-only key, real executable swaps, rollback,
timeout, permissions, locking, cancellation, corruption, concurrency and recovery.
Native lock/recovery controls were also exercised in a hidden test window.

The seven captures in `.impeccable/review/autoupdate/` were checked at 1180 × 730
and 944 × 584. The independent reviewer returned `ship` for the scope of the update
states, recovery link and version identity. They are synthetic previews from the
production renderer; they don't prove the behavior of a game session. Microsoft
login, full installation and gameplay were not repeated for this release.

Published over SFTP to `/srv/eternal-launcher/files/launcher/`, with the remote
SHA-256 checked and the manifest published by rename after the executable. NGINX
passed `nginx -t` before reloading; the mods timer stayed active.

After publishing, the signature, size, SHA-256 and `Cache-Control: no-store` were
checked over public HTTPS. The same WinHTTP transport the launcher uses downloaded
and validated the published executable. The real transport cancellation test
responded in 63 ms. Temporary `.upload` files return HTTP 403; the mod manifest
stayed at HTTP 200 with 146 entries.

No fictitious versions, private keys or credentials were published.
