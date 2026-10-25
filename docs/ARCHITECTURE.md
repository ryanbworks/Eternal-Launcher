# Architecture

## Goal

The structure separates the interface, file sources, instance preparation and
execution. The server can run NGINX or any HTTP implementation without changing
the visual components or the Minecraft rules.

## Layers

### Application and page

`LauncherApplication` initializes GDI+, the window and the message loop.
`MainPage` composes the screen, holds `LauncherViewState` and coordinates work on
its own threads. Downloads never block the UI thread.

### Visual components

- `HeaderComponent`: brand, version and access to settings.
- `HeroComponent`: modpack identity and versions.
- `PlayerProfileComponent`: local nickname.
- `StatusPanelComponent`: installation, private files and server.
- `PlayPanelComponent`: current state and the main action.
- `ProgressComponent`: activity summary and AutoUpdate progress.
- `InstallationPanelComponent`: stages, current transfer and preparation recovery controls.
- `NewsPanelComponent`: editorial content, still mocked.

Components only draw the data they receive. They never touch the network or disk.

### Services

- `HttpFileService`: fetches the private manifest over WinHTTP.
- `HttpDownloadClient`: transfers text and binaries.
- `JavaRuntimeService`: coordinates the portable Java.
- `PortableJavaInstaller`: downloads, verifies and publishes Temurin 17.
- `MinecraftManifestService`: turns the official manifests into an explicit plan.
- `VerifiedDownloader`: downloads in parallel, uses `.part` files and checks hash/size.
- `GameInstallationService`: orchestrates Mojang, Forge and private files.
- `InstallationCoordinator`: reads the manifest, prepares Java and the game and validates the instance on a worker; it does not start Minecraft.
- `MinecraftLaunchCommandBuilder`: builds modules, classpath and arguments.
- `MinecraftLauncherService`: validates and monitors the game process.
- `IUpdateService`: future boundary for obsolete-file removal and version policy.
- `LauncherUpdateService`: prepares and applies the AutoUpdate with backup/rollback.
- `LauncherUpdateManifest`: numeric version, RSA-PSS signature and contract validation.
- `LauncherUpdateTransport`: asynchronous WinHTTP, cancellation and AutoUpdate limits.

### Utilities

- `JsonValue`: minimal JSON parser used for the manifests.
- `FileHash`: SHA-1 and SHA-256 with Windows CNG.
- `ArchiveExtractor`: ZIP/JAR extraction using Windows' built-in support.
- `InstallationProcess`: cancellable wait shared by extraction and the Forge installer.
- `DrawingUtils`: shared UI drawing primitives.

## Current flow

```text
Open the launcher
  → check the signed AutoUpdate, apply it and restart if there is a new version
  → read the private manifest and inspect the local installation
  → wait for the Play click
  → ensure portable Java 17
  → get the official Minecraft 1.20.1 metadata
  → download/verify client, libraries, assets and log configs
  → extract native libraries
  → download/verify and run the Forge 47.4.10 installer
  → synchronize private files declared in the manifest
  → generate eternal-classpath.txt
  → validate nickname and installation
  → start Java and monitor the process
```

## Key decisions

[Installation tracking](INSTALLATION_TRACKING.md) documents the shared contract,
metrics, mailbox, cancellation and test evidence.

- The instance at `%APPDATA%\.eternal-launcher\minecraft` never mixes mods with the
  regular `.minecraft`.
- Java and the game come from official sources; the private server only delivers
  the group's specific content.
- No downloaded file is published before its integrity is verified.
- The classpath is saved explicitly so it never includes helper JARs from the
  Forge installer.
- Native DLLs are flattened into the root of `natives`, matching the
  `java.library.path` used in the command.
- Java output goes to `logs/launcher-console.log` without opening a console.
- The offline identity is deterministic and can be replaced by an authentication
  service without changing the installer.
- Memory, versions and endpoints stay centralized in `LauncherConfig`.

## Future extensions

```text
Persistent settings
  → update policy and safe removal
  → Microsoft authentication
  → signed mod manifest (launcher AutoUpdate already implemented)
```
