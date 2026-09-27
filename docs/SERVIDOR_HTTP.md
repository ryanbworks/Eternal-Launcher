# Servidor HTTP de arquivos

## Endereço atual

- Base: `http://10.0.0.51/`
- Manifesto: `http://10.0.0.51/manifest.json`
- Diretório publicado: `/srv/eternal-launcher/files`
- Site NGINX: `/etc/nginx/sites-available/eternal-launcher`

O launcher depende somente de HTTP, não do NGINX. O endereço compilado fica em
`src/config/LauncherConfig.h`, na constante `FileServerBaseUrl`.

## Manifesto privado

O campo `files` descreve mods, configurações e outros arquivos próprios do
grupo. `path` é relativo à pasta da instância, `sha256` é obrigatório e `size`
é recomendado. Quando `url` não é informado, o endereço é `base + path`.

```json
{
  "schemaVersion": 1,
  "launcherVersion": "0.1.0-visual",
  "minecraftVersion": "1.20.1",
  "forgeVersion": "47.4.10",
  "files": [
    {
      "path": "mods/exemplo-1.20.1.jar",
      "sha256": "HASH_SHA256_COM_64_CARACTERES_HEXADECIMAIS",
      "size": 1234567
    },
    {
      "path": "config/exemplo.toml",
      "url": "http://10.0.0.51/config/exemplo.toml",
      "sha256": "OUTRO_HASH_SHA256_COM_64_CARACTERES_HEXADECIMAIS",
      "size": 321
    }
  ]
}
```

O launcher rejeita caminhos absolutos, segmentos `..`, hashes inválidos e
arquivos cujo conteúdo não corresponda ao manifesto. A lista vazia é válida e
resulta em zero arquivos privados sincronizados.

### Gerar SHA-256 no servidor

```bash
cd /srv/eternal-launcher/files
sha256sum mods/exemplo-1.20.1.jar
stat -c %s mods/exemplo-1.20.1.jar
```

Depois de editar o JSON, valide sua sintaxe:

```bash
jq . /srv/eternal-launcher/files/manifest.json
```

## Publicar com FileZilla

Use SFTP, não FTP sem criptografia:

```text
Protocolo: SFTP - SSH File Transfer Protocol
Host:      10.0.0.51
Porta:     22
Destino:   /srv/eternal-launcher/files
```

Envie o arquivo primeiro e publique `manifest.json` por último. Isso evita que
clientes recebam uma referência para um upload ainda incompleto. Credenciais e
senhas nunca devem ser salvas neste repositório.

## Administração

```bash
nginx -t
systemctl reload nginx
systemctl status nginx
tail -f /var/log/nginx/eternal-launcher.access.log
tail -f /var/log/nginx/eternal-launcher.error.log
```

## Segurança e evolução

- Criar uma conta SFTP dedicada à publicação, sem acesso administrativo.
- Trocar credenciais administrativas que tenham sido compartilhadas.
- Adicionar HTTPS antes de expor o serviço à internet.
- Restringir o firewall às redes e portas necessárias.
- Assinar o manifesto antes de aceitar atualizações públicas.
- Implementar remoção segura de arquivos que saírem de `files`.
