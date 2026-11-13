#!/usr/bin/env bash
# Builds and installs Drasl for the Conta Eternal on Debian (the "accounts" container).
#   sudo DRASL_DOMAIN=accountseternal.aplot.cloud bash server/drasl/install.sh
# DRASL_BASE_URL defaults to https://$DRASL_DOMAIN; a LAN test uses
#   DRASL_DOMAIN=10.0.0.17 DRASL_BASE_URL=http://10.0.0.17:25585
# Re-running keeps the database and only replaces the binary, config and unit.
set -euo pipefail

[ "$(id -u)" -eq 0 ] || { echo "Run as root." >&2; exit 1; }
here="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
version="${DRASL_VERSION:-v4.0.2}"
domain="${DRASL_DOMAIN:?set DRASL_DOMAIN}"
base_url="${DRASL_BASE_URL:-https://$domain}"
source_dir=/opt/src/drasl
export LC_ALL=C DEBIAN_FRONTEND=noninteractive

apt-get install -y -qq git make build-essential nodejs npm ca-certificates curl jq >/dev/null

# Go: the latest stable release from go.dev, checked against its published SHA-256.
if [ ! -x /usr/local/go/bin/go ]; then
    info=$(curl -fsSL 'https://go.dev/dl/?mode=json' | jq -r '[.[] | select(.stable)][0].files[]? | select(.os=="linux" and .arch=="amd64" and .kind=="archive") | "\(.filename) \(.sha256)"')
    file=${info% *}; sum=${info#* }
    curl -fsSL -o "/tmp/$file" "https://go.dev/dl/$file"
    echo "$sum  /tmp/$file" | sha256sum -c -
    tar -C /usr/local -xzf "/tmp/$file" && rm -f "/tmp/$file"
fi
export PATH=/usr/local/go/bin:$PATH GOPATH=/root/go

mkdir -p /opt/src
if [ ! -d "$source_dir/.git" ]; then
    git clone --quiet --depth 1 --branch "$version" https://github.com/unmojang/drasl.git "$source_dir"
fi
cd "$source_dir"
if [ "$(git describe --tags --exact-match 2>/dev/null)" != "$version" ]; then
    git fetch --quiet --depth 1 origin tag "$version" && git checkout --quiet "$version"
fi
[ -x drasl ] && [ "$(git describe --tags --exact-match)" = "$version" ] && [ drasl -nt main.go ] || make build
make install prefix=/usr >/dev/null

id drasl >/dev/null 2>&1 || useradd --system --no-create-home --shell /usr/sbin/nologin drasl
install -d -m 0755 /etc/drasl
[ -f /etc/drasl/config.toml ] && cp /etc/drasl/config.toml "/etc/drasl/config.toml.bak-$(date +%Y%m%d-%H%M%S)"
sed -e "s|@DOMAIN@|$domain|" -e "s|@BASE_URL@|$base_url|" "$here/config.toml" > /etc/drasl/config.toml
chmod 0644 /etc/drasl/config.toml
install -m 0644 "$here/drasl.service" /etc/systemd/system/drasl.service
sed -i 's/\r$//' /etc/drasl/config.toml /etc/systemd/system/drasl.service

# First access from the launcher for the accounts reserved by drasl_migrate.py.
id eternal-first-access >/dev/null 2>&1 || useradd --system --no-create-home --shell /usr/sbin/nologin eternal-first-access
install -d -m 0755 /usr/local/lib/eternal
install -m 0644 "$here/first_access.py" /usr/local/lib/eternal/first_access.py
install -m 0644 "$here/eternal-first-access.service" /etc/systemd/system/eternal-first-access.service
sed -i 's/\r$//' /usr/local/lib/eternal/first_access.py /etc/systemd/system/eternal-first-access.service
install -d -m 0700 -o eternal-first-access -g eternal-first-access /var/lib/eternal-first-access
pending=/var/lib/eternal-first-access/pending.txt
if [ ! -f "$pending" ] && [ -f /root/eternal-temporary-passwords.txt ]; then
    install -m 0600 -o eternal-first-access -g eternal-first-access /root/eternal-temporary-passwords.txt "$pending"
fi

systemctl daemon-reload
systemctl enable --now drasl eternal-first-access >/dev/null
systemctl restart drasl eternal-first-access
for _ in $(seq 1 20); do curl -fsS http://127.0.0.1:25585/authlib-injector >/dev/null 2>&1 && break; sleep 1; done
curl -fsS http://127.0.0.1:25585/authlib-injector | jq -c '.meta | {serverName, implementationName, implementationVersion}'
echo "Drasl $version installed for $base_url"
