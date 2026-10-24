#!/usr/bin/env bash
# Instalar a distribuição de arquivos do launcher em Debian/Ubuntu com systemd.
set -euo pipefail

if [ "$(id -u)" -ne 0 ]; then
    echo "Execute como root: sudo bash server/install.sh" >&2
    exit 1
fi

project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
for source_file in server/refresh_manifest.py server/eternal-manifest.service server/eternal-manifest.timer infrastructure/nginx/eternal-launcher.conf; do
    test -f "$project_root/$source_file"
done
command -v apt-get >/dev/null
command -v systemctl >/dev/null

site_link=/etc/nginx/sites-enabled/eternal-launcher
if [ -e "$site_link" ] || [ -L "$site_link" ]; then
    if [ ! -L "$site_link" ] || [ "$(readlink -f "$site_link")" != /etc/nginx/sites-available/eternal-launcher ]; then
        echo "O site habilitado já existe com outro destino: $site_link" >&2
        exit 1
    fi
fi

apt-get update
DEBIAN_FRONTEND=noninteractive apt-get install -y nginx python3 curl ca-certificates

files_root=/srv/eternal-launcher/files
install -d -m 0755 /srv/eternal-launcher "$files_root" \
    "$files_root/mods" "$files_root/config" "$files_root/resourcepacks" \
    "$files_root/shaderpacks" "$files_root/launcher/releases" \
    /usr/local/lib/eternal-launcher

# Preservar as configurações substituídas para permitir recuperação.
backup_root="$(mktemp -d /root/eternal-launcher-backup.XXXXXXXX)"
for target in /usr/local/lib/eternal-launcher/refresh_manifest.py \
    /etc/systemd/system/eternal-manifest.service \
    /etc/systemd/system/eternal-manifest.timer \
    /etc/nginx/sites-available/eternal-launcher; do
    if [ -e "$target" ]; then
        cp --parents -a -- "$target" "$backup_root/"
    fi
done

install -m 0644 "$project_root/server/refresh_manifest.py" /usr/local/lib/eternal-launcher/refresh_manifest.py
install -m 0644 "$project_root/server/eternal-manifest.service" /etc/systemd/system/eternal-manifest.service
install -m 0644 "$project_root/server/eternal-manifest.timer" /etc/systemd/system/eternal-manifest.timer
install -m 0644 "$project_root/infrastructure/nginx/eternal-launcher.conf" /etc/nginx/sites-available/eternal-launcher
# Normalizar arquivos copiados a partir de checkouts no Windows.
sed -i 's/\r$//' /usr/local/lib/eternal-launcher/refresh_manifest.py \
    /etc/systemd/system/eternal-manifest.service \
    /etc/systemd/system/eternal-manifest.timer \
    /etc/nginx/sites-available/eternal-launcher

if [ ! -L "$site_link" ]; then
    ln -s /etc/nginx/sites-available/eternal-launcher "$site_link"
fi

# Criar um manifesto vazio válido mesmo antes de receber os primeiros mods.
if [ ! -e "$files_root/manifest.json" ]; then
    printf '{"schemaVersion": 1, "files": []}\n' > "$files_root/manifest.json"
    chmod 0644 "$files_root/manifest.json"
fi

python3 -m py_compile /usr/local/lib/eternal-launcher/refresh_manifest.py
nginx -t
systemctl daemon-reload
systemctl start eternal-manifest.service
systemctl enable --now eternal-manifest.timer nginx
systemctl reload nginx
# O reload retorna antes de os novos workers começarem a atender.
curl --fail --silent --show-error --retry 5 --retry-delay 1 --retry-all-errors \
    --max-time 10 -H 'Host: eternalfast.aplot.cloud' http://127.0.0.1/manifest.json
printf '\nInstalação concluída. Envie os arquivos por SFTP para %s.\nBackup: %s\n' "$files_root" "$backup_root"
