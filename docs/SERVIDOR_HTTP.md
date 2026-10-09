# Servidor HTTP de arquivos

## Endereço atual

- Base pública: `https://eternalfast.aplot.cloud/`
- Manifesto público: `https://eternalfast.aplot.cloud/manifest.json`
- Endereço interno: `http://10.0.0.51/`
- Diretório publicado: `/srv/eternal-launcher/files`
- Site NGINX: `/etc/nginx/sites-available/eternal-launcher`

O launcher depende somente de HTTP, não do NGINX. O endereço compilado fica em
`src/config/LauncherConfig.h`, na constante `FileServerBaseUrl`.

## Mods e configurações: apenas envie o arquivo

Envie cada `.jar` para `/srv/eternal-launcher/files/mods/` por SFTP. Para
configurações, preserve o caminho esperado pelo Minecraft dentro de
`/srv/eternal-launcher/files/config/`; por exemplo, os arquivos do Simple RPC
ficam em `config/simple-rpc/`. O serviço `eternal-manifest.timer` verifica
`mods/` e `config/` a cada 10 segundos e gera sozinho o `manifest.json`,
incluindo tamanho e SHA-256. Não edite o JSON nem calcule hashes manualmente.
O launcher consulta a lista novamente quando você clica em **Jogar**, baixa
arquivos novos e substitui arquivos cujo hash mudou.

Aguarde alguns segundos depois do fim do upload para o manifesto ser publicado.
Nomes com espaços ou colchetes também são aceitos. Arquivos ainda sendo enviados
não entram na lista. Mods removidos do servidor deixam de aparecer no manifesto;
no próximo clique em **Jogar**, o launcher remove as cópias `.jar` locais após
concluir a sincronização. Configurações removidas deixam de ser baixadas nas
próximas instalações; arquivos de configuração já existentes não são apagados
automaticamente.

Para verificar a automação no servidor:

```bash
systemctl status eternal-manifest.timer
systemctl status eternal-manifest.service
cat /srv/eternal-launcher/files/manifest.json
```

Arquivos fora de `mods/` e `config/` continuam preservados no manifesto, mas
não são descobertos automaticamente.

## Publicar com FileZilla

Para o executável e suas atualizações, siga [AutoUpdate e releases](AUTOUPDATE.md).
O manifesto assinado do launcher é publicado em `launcher/latest.json` e não é
gerado pelo timer de mods.

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
- Manter o HTTPS público e o cabeçalho `no-store` dos manifestos.
- Restringir o firewall às redes e portas necessárias.
- O manifesto do executável já é assinado; estender assinatura ao manifesto de mods.
- Implementar remoção segura de outros arquivos que saírem de `files`.
