# Minecraft installation and launch

## Supported profile

- Minecraft `1.20.1`;
- Forge `47.4.10`;
- Java `17`;
- asset index `5`;
- `forgeclient` target;
- 512 MB initial and 4 GB maximum memory.

The values live in `src/config/LauncherConfig.h`.

## Automatic installation

On startup, the launcher queries the private server and inspects the local
installation without starting any download. After **Jogar** (Play) is clicked, it
downloads portable Temurin 17 if needed and validates its SHA-256. It then reads
Mojang's official manifests and prepares the client, libraries, assets and log
configuration.

The official Forge installer is checked against the SHA-1 defined in the
configuration and run with `--installClient` inside the isolated instance. After
that, the launcher extracts the native DLLs, synchronizes the private manifest,
writes `eternal-classpath.txt` and starts the game with the chosen nickname.
While Forge installs, the installer's output and errors are written to
`minecraft\logs\forge-installer.log`. If the process fails to start or exits with
an error, the launcher shows the matching code and the path to that log.
If the installer starts but fails, the launcher retries once with the IPv4 network
preference disabled. Both attempts go to the same log.
The portable Java is only accepted when it contains `java.exe` and `javaw.exe`.
If Windows cannot start `java.exe` because of a path or executable error, the
launcher downloads and reinstalls the portable Java and tries Forge again. The log
records each attempt and whether the required paths exist.

The launcher does not change the Multiplayer list. The game server's IP is added
manually inside Minecraft, and existing entries are preserved.

```text
%APPDATA%\.eternal-launcher\
├── downloads\
│   └── forge-1.20.1-47.4.10-installer.jar
├── runtime\bin\java.exe
└── minecraft\
    ├── assets\
    ├── libraries\
    ├── mods\
    │   └── .eternal-managed-mods
    ├── natives\*.dll
    ├── logs\launcher-console.log
    ├── logs\forge-installer.log
    ├── eternal-classpath.txt
    └── versions\
        ├── 1.20.1\1.20.1.jar
        └── 1.20.1-forge-47.4.10\1.20.1-forge-47.4.10.json
```

Existing files with the correct size and hash are reused. An interrupted download
stays in `.part` and never corrupts the previous destination.
After validating the manifest and finishing the downloads, the launcher removes
from `minecraft\mods` the `.jar` files it had installed that are no longer on the
server's list. The installed files are tracked in `mods\.eternal-managed-mods`, so
mods added by the player are never deleted.

## Player identity

In local nickname mode, the name accepts 3 to 16 characters (`A-Z`, `a-z`, `0-9`
and `_`). The UUID is derived from `OfflinePlayer:<nickname>`, compatible with
offline servers. In Original mode, the player signs in with Microsoft and the
launcher uses the name, UUID and token of the Minecraft: Java Edition profile. See
the README to configure the Application ID that mode requires.

## Validation and execution

Before starting, `MinecraftLauncherService` confirms Java 17, the Forge profile,
client, asset index, classpath, essential libraries and native DLLs.
`MinecraftLaunchCommandBuilder` builds the module path and the controlled classpath,
leaving out temporary installer artifacts.

The game runs through `java.exe` with `CREATE_NO_WINDOW`. Standard output and errors
go to `minecraft\logs\launcher-console.log`, which helps when Forge or a mod fails
before `latest.log` is created.

## Local diagnostics

The test executable has manual modes that are not part of the normal flow:

```powershell
./out/build/x64-debug/EternalLauncherTests.exe --validate-installed
./out/build/x64-debug/EternalLauncherTests.exe --launch-installed Steve
```

The second command waits 30 seconds to confirm the process stays alive; close the
game after the diagnostic.

## TODOs

```cpp
// TODO: load memory and server from persisted preferences.
```
