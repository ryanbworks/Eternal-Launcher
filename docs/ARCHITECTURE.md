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

The window uses a custom title bar and a navigation rail; both are anchored to the
window edges, while the content area scales uniformly from a 1280 × 800 canvas.
Every interactive element is a native owner-drawn `BUTTON` (or `EDIT`) placed over
the pre-rendered frame, so keyboard navigation and accessibility keep working.

- `Layout`: logical geometry for every page.
- `ChromeView`: title bar (logo, version, caption buttons) and the navigation rail.
- `HeroComponent`: landscape, portal animation and modpack identity.
- `HomeView`: hero, status tiles, news cards and the player card (nickname, mode,
  play time, split play button and quick actions).
- `InstallationPanelComponent`: stages, current transfer and preparation recovery controls.
- `ContentViews`: the News, Mods, Screenshots and Help pages.
- `SettingsPage`: the General, Advanced and About tabs with their native controls.
- `Buttons` and `Icons`: shared button styles and the stroke icon set.

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
- `NewsFeed`: reads the optional `news.json` from the file server (see below).
- `NewsReadState`: remembers which news the player read or set aside (`news-state.txt`).
- `ScreenshotLibrary`: lists the `screenshots` folders of the modpack and of the 1.8.9,
  decodes thumbnails and viewer images on a worker, copies to the clipboard and
  recycles files.
- `GalleryApi` / `GallerySharedStore`: client of the group gallery (`server/gallery_api.py`)
  and the remove tokens of the pictures this installation shared (`gallery-shared.txt`).
- `ModpackChangelog`: compares the server's mod list with the one the player last
  acknowledged (`modpack-snapshot.txt`) and names what came in, left or was updated.
- `MilestoneStore`: the highest play-time level celebrated per nickname (`milestones.txt`).
- `EternalAccount` / `EternalSessionStore`: the Conta Eternal on the group's Drasl server
  (Yggdrasil sign-in, validate/refresh, skin upload and reset, the pinned authlib-injector),
  with the access token stored encrypted by DPAPI in `account.dat`. See
  [DRASL_MIGRATION.md](DRASL_MIGRATION.md).
- `SkinRenderer`: decodes skin textures and draws the flat front and back of a player
  for the Skins page.
- `ModInventory`: separates server mods from the player's own and enables, disables or
  recycles player mods.
- `PlaytimeStore` / `PlaySession`: total and last play time in `playtime.json`.
- `PlaytimeApi`: server-side play time session, one-time import and leaderboard.
- `SkinLookup`: finds the skin of a nickname through Mojang's public API and composes
  the 8 × 8 face shown in the player card.

### Notification area and play time

While Minecraft runs, `TrayIcon` shows an icon next to the clock with the session
length in its tooltip. Play time is flushed to `playtime.json` every minute, when
the game exits and when Windows ends the session. Closing the window during a
session hides it to the notification area; the launcher exits on its own when the
game closes. "Hide in the notification area while playing" in Settings does the
same as soon as the game starts.

### Server-side play time and leaderboard

`PlaytimeApi` talks to `server/playtime_api.py`, published by NGINX under
`/api/playtime/`. The launcher never reports a duration: while the game runs it
opens a session and sends a heartbeat every minute, and the server adds up the time
with its own clock. A gap longer than 150 seconds earns nothing and each player has
one session at a time. `playtime.json` remains as the fallback shown while the
server is unknown and keeps counting when it is unreachable, but that time does not
reach the leaderboard.

The only number a client sends is the one-time import of the old local counter:
once per nickname, capped (200 h by default) and closed after a deadline (60 days
after the service first runs). `playtime-imported.marker` keeps an installation from
importing again under a second nickname. Nickname mode has no login, so a nickname
can be used by anyone and a script could imitate the launcher in real time; the
server rules limit that but cannot rule it out. See [server/INSTALL.md](../server/INSTALL.md)
for the administration commands.

### News feed

The launcher reads `/launcher/news.json` from the file server. A missing file (404)
simply means nothing is published; the launcher's own release notes are always
shown. The file lives in the repository as [`news/news.json`](../news/news.json):
the publishing workflow validates it and uploads it next to `latest.json` after the
release is announced, so editing the news is a pull request away.

```json
{
  "schemaVersion": 1,
  "items": [
    {
      "category": "EVENTO",
      "title": "Noite de construção no spawn",
      "date": "2026-10-25",
      "body": "Sábado às 20h. Tragam blocos e ideias.",
      "url": "https://example.com/evento"
    }
  ]
}
```

Only `title` is required. Links must use `https://`; at most 20 items are read.

An item is **new** until the player opens the News page, as long as it is dated within
the last 21 days (undated items and the launcher's own release notes count too). The
newest one is shown in a spotlight over the Home landscape until it is read or set aside
with "Depois"; setting it aside keeps the counter on the News icon and the NOVO tags.
Read and set-aside items are kept in `news-state.txt` in the data folder.

### Group gallery

From the screenshot viewer, Compartilhar scales the picture to 1920 × 1080 at most,
encodes it and a 416 × 234 thumbnail as JPEG and posts both to `/api/gallery/photos`
under the player's nickname. The Galeria do grupo tab lists the newest 120 pictures and
downloads thumbnails as they scroll into view. The server returns a token with each
upload; only the installation holding it can remove that picture, and everyone else
can report it. Set `ETERNAL_GALLERY_URL` to point the launcher at a local server
(`python3 server/gallery_api.py serve`) while developing.

### Explorar skins

The Skins page has two tabs. Minhas skins is the account's current skin and the local
library (`skins/` in the data folder). Explorar loads two sources on a worker:

- Community skins from the gallery service: `GET /api/gallery/skins` and each
  `/api/gallery/skins/<id>.png` (64 × 64 or 64 × 32, 64 KB at most). Sharing a library
  skin posts the PNG to `/api/gallery/skins?name=&title=&model=`; the remove token is
  kept in `skins-shared.txt`, like the pictures' tokens.
- The group's skins: the leaderboard's names go through the Drasl batch lookup
  (`/authlib-injector/api/profiles/minecraft`), then each player's session profile and
  texture. Players on the default skin are left out.

A skin from Explorar can be used on the account (the same upload as a library skin) or
saved to the library.

### Modpack changelog

When the manifest's `mods/` list differs from the one the player last acknowledged,
Home shows the mods that came in, left or were updated in place of the landscape.
Updates are recognized by the file name without its version, and versions are shown
without the Minecraft version or loader. Entendi, Ver mods or Jogar acknowledge it.

### Play-time levels

Every player climbs Pedra, Ferro, Ouro, Diamante and Netherite with the server's play
time (the same number as the ranking). Reaching a level is celebrated once on Home.

### Leaderboard podium

The top 3 wear gold, silver and bronze medals. Each row gets more ornate at 25, 75, 150
and 300 hours (`RankTier` in `HomeView.h`): a medal-colored wash, then a glowing medal
and tinted name, then a crown and a passing sheen, then a shimmering outline with sparkles.

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
