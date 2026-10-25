# Publishing the launcher from GitHub

The **Publish launcher** workflow builds and tests on Windows x64, signs the package
with the same key as existing installations and publishes it over SFTP. Publishing
uses Proxmox as a subnet router to reach `10.0.0.13:22` through Tailscale.

## Network

- Proxmox: `10.0.0.10`, Tailscale `100.102.117.89`.
- Advertised and approved network: `10.0.0.0/24`.
- Persistent IPv4 forwarding: `/etc/sysctl.d/99-eternal-tailscale.conf`.
- The Action already enables `--accept-routes` on the Linux runner; don't repeat
  that option in `args`. Proxmox keeps the default SNAT.
- Allow the forwarding needed for `10.0.0.13:22` in the Proxmox/container firewall
  if an existing rule blocks it.

This route keeps the local addresses: no IP changes are made to the VMs.
Clients using the route need access allowed by the Tailscale policy.

## GitHub identity in Tailscale

In the Tailscale admin console, create `tag:eternal-ci` and allow it to reach
`10.0.0.13` on TCP/22 only. Example snippets to merge into the existing policy:

```json
{
  "tagOwners": {
    "tag:eternal-ci": ["autogroup:admin"]
  },
  "grants": [
    {
      "src": ["tag:eternal-ci"],
      "dst": ["10.0.0.13"],
      "ip": ["tcp:22"]
    }
  ]
}
```

Rules are cumulative. An old rule granting everything to `*` also grants everything
to the runner: review that rule to keep users' access while excluding the CI
identity from that broad access. Adding the example alone does not remove
permissions granted by other rules.

Under **Trust credentials**, create an **OpenID Connect** credential:

- Issuer: GitHub Actions (`https://token.actions.githubusercontent.com`).
- Subject: `repo:ryanbworks@213251617/Eternal-Launcher@1391560141:environment:production`.
- Custom claim `ref`: `refs/heads/master`.
- Scope: `auth_keys` with write permission and tag `tag:eternal-ci`.
- Copy the Client ID and Audience into the GitHub environment variables below.

The token GitHub issues for this repository includes the account and repository
IDs in the Subject. The format without those IDs does not match the current token.
On a 403 error, the credential details show the Subject that was received; check the
IDs with the GitHub API before adjusting, keeping the `production` environment and
the `master` branch restriction.

The temporary Actions identity is removed when the run ends. This access uses the
container's OpenSSH/SFTP; it does not require enabling Tailscale SSH.

## The production environment on GitHub

Under Settings → Environments, create the `production` environment and allow only
the `master` branch. The workflow does not publish from other branches.

| Type | Name | Content |
|---|---|---|
| Variable | `TS_CLIENT_ID` | Client ID of the Tailscale OIDC identity |
| Variable | `TS_AUDIENCE` | Audience of the Tailscale OIDC identity |
| Secret | `SFTP_PRIVATE_KEY` | OpenSSH Ed25519 key of the `eternal-deploy` user |
| Secret | `SFTP_KNOWN_HOSTS` | Host public key of `10.0.0.13`, verified through a trusted channel |
| Secret | `ETERNAL_SIGNING_KEY_PKCS8` | Original RSA key, PKCS#8 DER in Base64 |

The signing key must be extracted from DPAPI by the same Windows account that
published the launcher and stored in the secret without going through the checkout,
artifacts or logs. `CiSigning.ps1` imports the key in memory and clears the
environment variable. `Release.ps1` checks that it matches the project's public key.
The build and tests run before the step that receives that secret.

The SFTP user authenticates by key, with no shell or port forwarding. The chroot is
`/srv/eternal-launcher`; that's why the publisher uses `/files/launcher`, which maps
to `/srv/eternal-launcher/files/launcher` in the container. Only the `launcher` and
`launcher/releases` folders are writable by the publishing group. The code never
deletes previous versions.

## Using the button

1. Open Actions → Publish launcher → Run workflow, on the `master` branch.
2. Enter a version higher than the published one, such as `1.0.15`, and a name
   without accents or spaces, such as `Aura`.
3. On the first run, keep **Build and check access only** checked.
4. After the check, run again with the same version and name and uncheck the option
   to publish. Check mode never uploads a release or changes the announcement.

The version/name you enter are applied to the runner's temporary checkout.
The workflow doesn't change `CMakeLists.txt` on the branch or create version commits.
The package stays available in the run's artifacts for 30 days.

The publisher checks the signature and SHA-256, rejects equal or lower versions,
uploads the EXE under a temporary name and verifies the download over HTTPS before
swapping `latest.json` with an atomic rename. If the final manifest check fails, it
restores the previous announcement as long as it still matches its own publication.
A retry can reuse an identical EXE; it never replaces different bytes of an existing
version.

The production domain goes through Cloudflare. The HTTPS check sends its own
User-Agent (`EternalLauncher-Publisher`), because the default `Python-urllib` is
blocked with HTTP 403. If Cloudflare still blocks the runner (Bot Fight Mode or a WAF
rule), the log shows the HTTP status and the CF-RAY; allow that User-Agent in a WAF
rule for `/launcher/*`.

If a run fails after the EXE upload, the version folder stays on the server without
being announced. A new build produces different bytes, so the next attempt must use
a higher version or re-run only the `publish` job of the same run.

The workflow serializes publications and the server uses the `.ci-publish-lock`
folder as an extra lock. If the process is killed abruptly, make sure no publication
is running before removing that empty folder over SFTP. Temporary files end in
`.upload` and are blocked by the NGINX configuration.

## Validating the scripts

```powershell
pwsh -NoProfile -File tests/TestCiSigning.ps1
python -m pip install -r tools/ci-requirements.txt
python -m unittest discover -s tests -p test_publish_release.py -v
```

Sources: [Subnet routers](https://tailscale.com/docs/features/subnet-routers),
[official Action](https://tailscale.com/docs/integrations/github/github-action),
[OIDC](https://tailscale.com/docs/features/workload-identity-federation).
