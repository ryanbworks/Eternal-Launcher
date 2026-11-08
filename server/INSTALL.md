# Reinstalling the file server

Current server: `10.0.0.13`, Debian 13. NGINX listens on port 80; the Cloudflare
tunnel on Proxmox forwards `eternalfast.aplot.cloud` to `http://10.0.0.13:80` and
provides public HTTPS.

Copy the project's `server/` and `infrastructure/nginx/` folders to the server,
keeping the structure. In the folder that contains both, run as root:

```sh
bash server/install.sh
```

The installer installs NGINX and Python, creates the public folders, configures the
site and enables the timer that regenerates the manifest every ten seconds,
including after a reboot. Replaced configs are kept in `/root/eternal-launcher-backup.*`.
The group's existing files are preserved. If the IP or domain changes, adjust
`server_name` in `infrastructure/nginx/eternal-launcher.conf` before installing.

In FileZilla, connect over **SFTP**, host `10.0.0.13`, port **22**. Publish to:

```text
/srv/eternal-launcher/files/
  mods/           Client .jar mods
  config/         Configs and subfolders
  resourcepacks/  .zip textures or folders
  shaderpacks/    .zip shaders or folders
  launcher/       Executable and signed update manifest
```

Only upload mods meant for players to `mods/`. Files for the Minecraft server
managed by Wings/Pterodactyl go elsewhere. The generator waits five seconds without
changes to a file and publishes size and SHA-256 in `manifest.json`. Don't upload
that JSON manually. To replace files, prefer uploading with a temporary name ending
in `.upload` and renaming only after the transfer finishes.

The `server/defaults/resourcepackoverrides.json` preset is optional and depends on
the packs and the Resource Pack Overrides mod listed in the README. Publish it only
after uploading those files. The installer does not distribute that preset on its
own. Executable updates are published by the **Publish launcher** GitHub Actions
workflow ([guide](GITHUB_ACTIONS.md)) or by `PrepareUpdate.cmd`; the timer never
generates `launcher/latest.json`.

To check:

```sh
nginx -t
systemctl status nginx eternal-manifest.timer --no-pager
systemctl start eternal-manifest.service
journalctl -u eternal-manifest.service -n 20 --no-pager
curl -f http://10.0.0.13/manifest.json
curl -f https://eternalfast.aplot.cloud/manifest.json
```

## Play time and leaderboard

`install.sh` also installs the `eternal-playtime` service, which listens on
`127.0.0.1:8765` and keeps its database in `/var/lib/eternal-playtime/playtime.db`.
Include that file in your backups. Re-running the installer preserves it. To check:

```sh
systemctl status eternal-playtime --no-pager
curl -f 'https://eternalfast.aplot.cloud/api/playtime/leaderboard?limit=5'
```

The launcher sends `POST` requests to `/api/playtime/*`. If Cloudflare answers 403,
allow that path in a WAF rule, as described for `/launcher/*` in
[GITHUB_ACTIONS.md](GITHUB_ACTIONS.md).

To change the import limits, set `ETERNAL_PLAYTIME_IMPORT_CAP_HOURS` (default 200)
in the unit. Administration, for suspicious cases:

```sh
cd /usr/local/lib/eternal-launcher
sudo -u eternal-playtime ETERNAL_PLAYTIME_DB=/var/lib/eternal-playtime/playtime.db python3 playtime_api.py top
sudo -u eternal-playtime ETERNAL_PLAYTIME_DB=/var/lib/eternal-playtime/playtime.db python3 playtime_api.py set-imported NICK 12.5
sudo -u eternal-playtime ETERNAL_PLAYTIME_DB=/var/lib/eternal-playtime/playtime.db python3 playtime_api.py remove NICK
sudo -u eternal-playtime ETERNAL_PLAYTIME_DB=/var/lib/eternal-playtime/playtime.db python3 playtime_api.py import-window 0
```

`set-imported` replaces the imported hours of a player, `remove` deletes the player
from the ranking and `import-window N` closes the import N days from now.

## Troubleshooting

The manifest service is `oneshot`: showing `inactive (dead)` after finishing
successfully is expected; the timer should stay `active (waiting)`.
If the local URL works and the domain returns 502, check the tunnel target on
Proxmox. The proxy must reach `http://10.0.0.13:80`.
