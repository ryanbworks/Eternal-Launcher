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

The manifest service is `oneshot`: showing `inactive (dead)` after finishing
successfully is expected; the timer should stay `active (waiting)`.
If the local URL works and the domain returns 502, check the tunnel target on
Proxmox. The proxy must reach `http://10.0.0.13:80`.
