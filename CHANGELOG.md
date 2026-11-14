# Changelog

All notable changes to Eternal Launcher are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)
and versions use [SemVer](https://semver.org/) plus a codename.

## [Unreleased]

### Added
- **OptiFine on 1.8.9.** Fetched once from optifine.net and installed with OptiFine's
  own patcher; the game starts with it. Without the site, 1.8.9 opens as before.
- **A turning skin.** The Skins stage shows the figure in 3D, turning slowly; drag it to
  turn it, or use Frente and Costas.
- **More motion.** Lists cascade in, the installation screen grows into place, the
  rail's selection springs to the page and buttons sink a little when pressed.

### Changed
- **Much faster Jogar.** Files that already passed their check are not read again on
  every Play; the rest is checked in parallel, Mojang's files on disk are reused and the
  native libraries are only extracted when they change. Reparar still checks everything.
- **Rail order.** Início, Versões, Skins, Capturas, Notícias, Mods, Configurações.
- **Signing in only signs in.** Jogar while signed out opens the sign-in; after it, Home
  comes back (with a new level, if any) and the game waits for another Jogar.

### Fixed
- The modpack's log configuration was never downloaded.

## [3.0.0] — Zenith · 2026-11-13

### Added
- **Conta Eternal.** Sign in with the group's own account (Drasl) instead of typing a
  nickname. Each reserved account keeps the player's UUID, so inventories, bases and
  progress stay as they were. The token is stored encrypted for the Windows user and the
  password is never saved; the game starts with authlib-injector.
- **Accounts and first access in the launcher.** Create an account from the sign-in
  screen. A reserved account signs in the first time with the nickname alone and then
  chooses a password, without a browser.
- **Skins page.** Preview the account's skin front and back, keep a library of PNG
  skins, pick classic or slim arms, use one on the server or go back to the default.
- **Capturas.** A new page with every screenshot taken with F2 (modpack and 1.8.9),
  a full-size viewer, copy to the clipboard, show in folder and move to the Recycle Bin.
- **Group gallery.** Share a screenshot from the viewer and the whole group sees it in
  the Galeria do grupo tab. Uploaders can remove their pictures; anyone can report one,
  and three reports hide it until an admin decides. Requires the new `eternal-gallery`
  service on the server (`server/install.sh`).
- **Play-time levels.** Pedra, Ferro, Ouro, Diamante and Netherite at 25, 75, 150 and
  300 hours, celebrated on Home when reached and shown on the player card.
- **Modpack changelog.** Before playing, Home lists the mods the server added, removed
  or updated since your last visit.
- **Explorar skins.** A second tab on the Skins page shows the skins players use on the
  server and the ones the group shared. Preview any of them front and back, use it on
  your account or save it to your library. Share a library skin with one click; you can
  take it back later, and anyone can report one. Served by the `eternal-gallery` service.
  A search field finds the skin of any original Minecraft account by nickname (or of a
  Conta Eternal), ready to use or save. A Populares catalog of skins from well-known
  players fills the tab from the start, with filters for each source.
- **First access hint.** The sign-in panel and the player card explain that a reserved
  Conta Eternal signs in the first time with the nickname alone, then gets a password.

### Changed
- **News you actually see.** New posts stay in a spotlight on Home until read or set
  aside, with a counter on the News icon and NOVO tags.
- **Top players.** The podium wears gold, silver and bronze and gets more ornate with
  hours; the card now scrolls through every player (the server lists up to 200).
- **Screens on Home.** Sign-in, the installation and the play-time levels open as
  screens centered over a dimmed Home instead of sharing the landscape's slot.

### Removed
- **Nickname mode.** Playing under any typed nickname is gone; the Conta Eternal takes
  its place. Conta original (Microsoft) stays available.

### Fixed
- Typing the nickname in the sign-in screen and moving the mouse no longer erases it.
- Cancelling or failing an installation no longer leaves its panel on Home for good,
  hiding the news spotlight, sign-in and the levels until the next game.

## [2.2.0] — Horizon · 2026-11-11

### Added
- **Real skin in the avatar.** The player card shows the face of the skin that belongs
  to the nickname (or Microsoft account) in use. Names without an original account
  show the classic default face.

### Changed
- **Installation no longer covers Home.** While the game is prepared, a compact panel
  with a six-step stepper replaces only the landscape; status tiles, news and the
  leaderboard stay visible and clickable.

### Fixed
- After choosing **Conta original**, the **Nickname** card could not be clicked, so
  there was no way back to nickname mode until the launcher was restarted.

## [2.1.0] — Horizon · 2026-11-08

### Added
- **Play time on the server.** While the game runs the launcher sends heartbeats and
  the server measures the time with its own clock, so the total cannot be edited on
  the player's computer. Previous local time is imported once per nickname, up to
  200 hours.
- **Top players** on the home page, with who is online and your own position.
- **Versions page.** Choose between the Eternal modpack and an unmodified
  Minecraft 1.8.9 with its own folder and Java 8. Time on 1.8.9 does not count.

### Changed
- The home page shows two news cards next to the leaderboard.
- The server's manifest script in the repository now matches the deployed one and
  publishes configs, resource packs and shader packs.

## [2.0.2] — Horizon · 2026-10-31

### Fixed
- The title bar and Settings › About showed the retired stepped-E monogram. They now
  show the Eternal emblem, the same artwork as the application icon.

### Removed
- The old monogram assets and the script that generated them.

## [2.0.1] — Horizon · 2026-10-27

### Improved
- **Much smoother animations.** Each animation frame used to repaint the whole window
  and copy it to every button with a slow GDI+ call, which could drop to about 5 fps
  on screens scaled to 125–150%. Frames now repaint only what moves, are presented
  with a plain pixel copy, and reuse a cached landscape and cached fonts: an
  animation frame takes under 1 ms on the build machine.
- **New loading animation on the Play button.** A spinner next to the label, a fill
  that brightens toward a glowing edge with a travelling glint, progress that glides
  between stages, and soft comets when the duration is unknown.

### Fixed
- After a repair or installation finished, part of the progress panel could stay on
  screen until the pointer moved over it.

## [2.0.0] — Horizon · 2026-10-26

The biggest update so far: a new interface, play time that keeps counting in the
notification area, news, mods and help pages, and playing without the file server.

### Added
- **New interface.** Custom title bar and a navigation rail with Home, News, Mods,
  Settings and Help. The home screen brings the landscape, server/mods/installation
  tiles, the latest news and a player card with avatar, sign-in mode, last session
  and total play time.
- **Play time that survives closing the window.** While Minecraft runs, an icon next
  to the clock shows "Jogando · 1 h 12 min". Closing the launcher during a session
  hides it there and keeps counting; it exits by itself when the game closes. Time
  is saved every minute.
- **News page.** Announcements published as `news.json` on the file server, plus the
  launcher's own release notes. A toast on the home screen points to what is new
  after an update.
- **Mods page.** Lists the server's mods and the player's own mods separately; player
  mods can be enabled, disabled, added or sent to the Recycle Bin.
- **Help page.** Repair the installation without starting the game, open the
  installation, Forge and game logs, and copy support information (with the
  Windows account name hidden).
- **Animations everywhere.** Pages rise and fade in with staggered cards, the
  navigation selection and the settings tab underline glide, switches slide, the
  sign-in choice slides between its cards, the Play button breathes, shines and
  ripples, status dots pulse while work is in progress and the avatar blinks. All of
  it follows the Windows "Show animations" preference.
- **Play even when the file server is down.** If the installation is complete, the
  launcher shows a warning and a "JOGAR ASSIM MESMO" button that starts the game
  with the files already on disk. It retries the server every minute, has a
  "Tentar conectar" link, and returns to normal as soon as the server answers. A
  first installation still needs the server.
- **Settings in tabs.** General (memory with a recommendation for this PC, window),
  Advanced (exact memory, Java, instance folder) and About.

### Changed
- "Minimizar ao jogar" became "Esconder na bandeja ao jogar".
- Page switches fade instead of sliding the native controls.

## [1.0.14] — Federation · 2026-10-25

### Fixed
- **Your mods now stay.** Syncing with the server manifest used to delete any
  `.jar` in the `mods` folder that was not on the official list, including mods
  the player added themselves. The launcher now records the files it installed in
  `mods/.eternal-managed-mods` and only removes those when they leave the manifest.
  Player mods, including ones with special names such as `[1.20.1] Usuário.jar`,
  are preserved. (Gabriel Henrique)

### Infrastructure
- Automated publishing through GitHub Actions: Windows x64 build and tests,
  signing with the original key and upload over SFTP through Tailscale.
- The publish step's HTTPS verification now sends its own User-Agent, accepted by
  the Cloudflare proxy in front of the production domain, and reports the HTTP
  status on errors.
- The workflow validates the version and codename before building.
- Continuous build: every push and pull request is built and tested on Windows.
- Automated installation of NGINX and the manifest service on the server.
- The animation test accounts for the real display scale.

> Versions 1.0.12 and 1.0.13 were only used to test the new publishing process and
> never reached players.

### Upgrade note
On the first sync after updating there is no record of managed mods yet, so no mod
is removed in that round. From the next sync on, mods removed from the server are
cleaned up normally.

## [1.0.11] — Aura · 2026-10-09

### Added
- New interface: landscape layout with an access console, dark/violet theme, logo,
  multi-resolution icon and the embedded Sora font.
- Portal, particle and transition animations that respect reduced motion.
- Persistent Minecraft preferences: initial/maximum RAM (0.5 GB steps), resolution,
  fullscreen, minimize while playing, custom Java 17 and a dedicated instance folder.
- Real installation progress: stages, bytes, speed, ETA and verified or reused
  files; cancel, try again and open the log.
- AutoUpdate with confirmation before downloading, RSA-PSS/SHA-256 signed manifest,
  backup, confirmation of the new window and rollback.

### Changed
- Asynchronous, cancellable WinHTTP transport with failures classified by type.
- Centralized preparation of Java, Minecraft, natives, Forge and mods.

## [1.0.0] — Aurora · 2026-10-03

### Added
- First stable release with executable AutoUpdate.
- Check on startup, validated download and automatic restart.
- Backup and rollback if the new version doesn't open within 30 seconds.
- Manual download link and a PowerShell 7 release tool.

## [0.1.1] — Vagalume · 2026-09-27

### Added
- Microsoft login.
- Automatic installation of portable Java 17, Minecraft and Forge.
- The Play button prepares and starts the game; default server in the Multiplayer list.
- Nickname saved between launches.
- Mod manifest generated automatically on the server.

### Fixed
- Mods with special names are included in the manifest.
- Forge recovery and portable Java repair when the process fails to start.

[1.0.14]: https://github.com/ryanbworks/native-game-launcher/releases/tag/v1.0.14
[1.0.11]: https://github.com/ryanbworks/native-game-launcher/compare/v0.1.1...v1.0.14
[1.0.0]: https://github.com/ryanbworks/native-game-launcher/blob/master/docs/RELEASE-1.0.0-Aurora.md
[0.1.1]: https://github.com/ryanbworks/native-game-launcher/releases/tag/v0.1.1
