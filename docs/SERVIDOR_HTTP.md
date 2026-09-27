# Servidor HTTP de arquivos

## Endereço atual

- Base: `http://10.0.0.51/`
- Manifesto: `http://10.0.0.51/manifest.json`
- Diretório publicado: `/srv/eternal-launcher/files`
- Site NGINX: `/etc/nginx/sites-available/eternal-launcher`

O launcher depende somente de HTTP, não do NGINX. O endereço compilado fica em
`src/config/LauncherConfig.h`, na constante `FileServerBaseUrl`.

## Mods: apenas envie o arquivo

Envie cada `.jar` para `/srv/eternal-launcher/files/mods/` por SFTP. O serviço
`eternal-manifest.timer` verifica essa pasta a cada 10 segundos e gera sozinho
o `manifest.json`, incluindo tamanho e SHA-256. Não edite o JSON nem calcule
hashes manualmente. O launcher consulta a lista novamente quando você clica em
**Jogar**, baixa mods novos e substitui arquivos cujo hash mudou.

Aguarde alguns segundos depois do fim do upload para o manifesto ser publicado.
Nomes com espaços ou colchetes também são aceitos. Arquivos ainda sendo enviados
não entram na lista. Mods removidos do
servidor deixam de aparecer no manifesto, mas a remoção automática de cópias
locais ainda não foi implementada.

Para verificar a automação no servidor:

```bash
systemctl status eternal-manifest.timer
systemctl status eternal-manifest.service
cat /srv/eternal-launcher/files/manifest.json
```

Outros arquivos privados, como configurações, ainda podem ser adicionados
manualmente a `files`; a automação preserva entradas fora de `mods/`.

## Publicar com FileZilla

Use SFTP, não FTP sem criptografia:

```text
Protocolo: SFTP - SSH File Transfer Protocol
Host:      10.0.0.51
Porta:     22
Destino:   /srv/eternal-launcher/files
```

Não é necessário enviar `manifest.json`: ele é gerado no servidor. Credenciais
e senhas nunca devem ser salvas neste repositório.

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
