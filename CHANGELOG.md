# Changelog

All notable changes to Eternal Launcher are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)
and versions use [SemVer](https://semver.org/) plus a codename.

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

[1.0.14]: https://github.com/ryanbworks/Eternal-Launcher/releases/tag/v1.0.14
[1.0.11]: https://github.com/ryanbworks/Eternal-Launcher/compare/v0.1.1...v1.0.14
[1.0.0]: https://github.com/ryanbworks/Eternal-Launcher/blob/master/docs/RELEASE-1.0.0-Aurora.md
[0.1.1]: https://github.com/ryanbworks/Eternal-Launcher/releases/tag/v0.1.1
