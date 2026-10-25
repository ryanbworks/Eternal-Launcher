# AutoUpdate — v1.0.0-Aurora

Aurora is the first version that updates its own executable. Users of
v0.1.1-Vagalume need to download Aurora once and replace the old executable.
Preferences, nickname, authentication and Minecraft stay in the existing instance.
The distribution is a single portable Windows x64 executable with a default maximum
RAM of 4096 MiB. The Aurora name is separate from the numeric version comparison.

> Releases are now published by the **Publish launcher** GitHub Actions workflow;
> see [server/GITHUB_ACTIONS.md](../server/GITHUB_ACTIONS.md). The local tools below
> remain available as an alternative.

## Flow on Windows

On a normal start, before enabling any action, the launcher reads
`https://eternalfast.aplot.cloud/launcher/latest.json`. If a newer version exists,
it shows a native Windows prompt with the new version. **OK** confirms the download
and installation; **Cancelar** (Cancel) keeps the current version and the next
start will try again. After confirmation, it shows progress, validates the file,
closes normally and reopens from the same path. Minecraft downloads remain tied to
the Play button.

Failures release the installed version's normal flow and leave a notice with the
**Baixar launcher** (Download launcher) link. The next start tries again. AutoUpdate
does not run during gameplay, in interface tests or in `--render-preview`.
A mod server failure still follows the installation's own handling.

The manifest has `schemaVersion: 1`, `payload` and `signature`. The last two fields
are Base64. The RSA-PSS/SHA-256 signature (RSA 3072, 32-byte salt) covers the exact
UTF-8 bytes of the payload, with no re-serialization. Its content is:

```json
{
  "version": "1.0.0",
  "codename": "Aurora",
  "architecture": "x64",
  "path": "/launcher/releases/1.0.0/EternalLauncher.exe",
  "size": 123456,
  "sha256": "<64 lowercase hex characters>"
}
```

The size above is illustrative; the release tool computes the real values.
Only higher numeric `major.minor.patch` versions are applied. Each number is
between 0 and 65535; leading zeros and suffixes are rejected. The architecture
must match the process. The path is derived from the version and restricted to
the compiled HTTPS domain. Redirects are refused. Manifests are limited to 64 KiB
and executables to 256 MiB. The query has a total budget of 10 seconds, and the
download 30 seconds per read without progress. Closing the window cancels the I/O.

## Swap and recovery

A `<executable-name>.update` folder next to the launcher holds the manifest, the
download, the helper copy and the transaction record. This keeps the backup and
the executable on the same volume. The helper copy checks the original process's
identity, waits for it to exit, revalidates the download and uses `ReplaceFileW`
to replace the executable with a backup.

The new version confirms it created its window within 30 seconds. Without that
confirmation, only the process started by the helper is terminated, the backup is
restored and the previous version reopens. A durable record allows an interrupted
transaction to be finished or cleaned up on the next start. Cleanup removes only
the known files, never walking other folders.

Per-executable-path mutexes prevent two instances from updating the same copy.
There is no automatic elevation: a folder without write permission produces a
notice and allows the manual download. If an external lock also blocks the
rollback, the helper says where to find `previous.exe` for manual recovery.

## Building a release

### Two-click wizard

Open `PrepareUpdate.cmd` at the project root. Enter a new number, such as `1.0.1`,
and keep `Aurora` as the name if you like. The wizard updates CMake, builds, runs
the tests, signs the JSON and checks the package. It looks for an installed
PowerShell 7, including the local Codex runtime when available. If it can't find
one, it reports the dependency; it never installs programs automatically.

When it finishes, it opens `out/releases/1.0.1/FILEZILLA/`. In FileZilla:

1. Open `/srv/eternal-launcher/files` on the server and upload the `launcher`
   folder inside `FILEZILLA`. Wait for every transfer to finish without errors.
2. Go back to the wizard window and press Enter. It downloads the executable over
   public HTTPS and checks the local manifest's signature, size and SHA-256.
3. After the confirmation, rename `launcher/latest.json.upload` to
   `launcher/latest.json` on the server, replacing the old manifest.
4. Press Enter again in the wizard to check the published version.

The `.upload` file does not announce an update yet. The final rename is the moment
the version becomes available to players, on the launcher's next start. Don't edit
the JSON and never upload the private key. The version name may stay the same; the
number must increase. Existing local packages are preserved, and previous release
folders must stay on the server.

The package also contains `HOW-TO-UPLOAD.txt`, with the steps and the commands to
resume verification if you close the wizard. `-Preview -Version 1.0.1` lets you
check the inputs without changing files. The example neither creates nor publishes
version 1.0.1.

### Running PowerShell directly

Requirements: Windows, Visual Studio with the x64 C++ tools and PowerShell 7.
The repository uses the CMake/Ninja shipped with Visual Studio.
These are the publishing tool's requirements. To build and run the launcher from
the IDE, PowerShell 7 is optional: without it, only the AutoUpdate integration
suite is not registered. The release tool passes its own `pwsh.exe` to CMake and
requires that suite with `ETERNAL_REQUIRE_UPDATE_TESTS=ON`, preventing publication
without full validation.

1. Update the version and codename in `CMakeLists.txt`.
2. Run `pwsh -File tools/Release.ps1` at the project root.
3. Check the package with `pwsh -File tools/VerifyRelease.ps1 -PackageDirectory out/releases/1.0.0`.

The tool builds Release, runs every test and generates:

```text
out/releases/1.0.0/
  SHA256SUMS.txt
  launcher/
    latest.json
    releases/1.0.0/EternalLauncher.exe
```

The private key lives at `%LOCALAPPDATA%\EternalLauncher\Signing\release-key.dpapi`,
encrypted with DPAPI for the current Windows account. Only the public key is in the
code. `-InitializeKeyOnly` initializes a key exactly once and refuses to replace a
different public key. Future releases must use the same key: preserve the Windows
profile and its protected backup. Losing the key would require manually
redistributing a launcher with a different trusted key.

`-SkipBuild` is only for repackaging an already validated build. The tests use
their own deliberately public key in `tests/update-trust`; that key never signs
releases and is not accepted by the distributed launcher.

## Publishing with FileZilla/SFTP

SFTP connection: host `10.0.0.51`, port `22`. Keep the publishing credentials
outside the repository. The destination is `/srv/eternal-launcher/files`.

1. Create `launcher/releases/1.0.0/` and upload the executable as
   `EternalLauncher.exe.upload`.
2. Check the remote SHA-256 with `sha256sum` against the local `SHA256SUMS.txt`.
3. Rename it to `EternalLauncher.exe`. Never replace files of a published version
   with different content: build a new version number instead.
4. Upload `launcher/latest.json` as `launcher/latest.json.upload` and rename it to
   `latest.json` only after the executable is complete and verified. The rename
   publishes the whole announcement at once.
5. Run `pwsh -File tools/VerifyRelease.ps1 -Public` to check the signature,
   SHA-256, size and the `no-store` header over public HTTPS.

NGINX must serve `/launcher/latest.json` as JSON with `Cache-Control: no-store`.
The configuration in `infrastructure/nginx` includes that rule and blocks
`.upload`/`.tmp` files inside `launcher/`. Validate with `nginx -t` before
reloading. The mods folder and its timer remain independent.

## Validation

`LauncherAutoUpdateTests` signs fixtures with the test key and performs real swaps
in isolated folders: success, rollback on exit and on timeout, locked file, folder
without permission, cancellation, corruption, equal or older versions, concurrency
and recovery after interruptions. It also checks signatures, schema, limits,
architecture and invalid paths.

`LauncherUpdateProbe.exe` is a smoke-test tool that is not distributed.
`--cancel` checks real WinHTTP cancellation; with no arguments, it reads and
validates the published release; `--download <file>` also downloads and checks the PE.
The simulations never publish test versions to the server.
