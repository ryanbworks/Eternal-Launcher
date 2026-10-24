# Reinstalar o servidor de arquivos

Servidor atual: `10.0.0.13`, Debian 13. O NGINX atende na porta 80;
o túnel Cloudflare no Proxmox encaminha `eternalfast.aplot.cloud` para
`http://10.0.0.13:80` e fornece o HTTPS público.

Copie as pastas `server/` e `infrastructure/nginx/` do projeto para o servidor,
preservando a estrutura. Na pasta que contém ambas, execute como root:

```sh
bash server/install.sh
```

O instalador instala NGINX e Python, cria as pastas públicas, configura o site
e habilita o timer de geração do manifesto a cada dez segundos, inclusive após
reiniciar. Configurações substituídas ficam em `/root/eternal-launcher-backup.*`.
Arquivos existentes do grupo são preservados. Se mudar o IP ou o domínio,
ajuste `server_name` em `infrastructure/nginx/eternal-launcher.conf` antes de instalar.

No FileZilla, conecte por **SFTP**, host `10.0.0.13`, porta **22**. Publique em:

```text
/srv/eternal-launcher/files/
  mods/           Mods .jar do cliente
  config/         Configurações e subpastas
  resourcepacks/  Texturas .zip ou pastas
  shaderpacks/    Shaders .zip ou pastas
  launcher/       Executável e manifesto assinado de atualização
```

Envie somente os mods apropriados para os jogadores em `mods/`. Os arquivos do
servidor Minecraft gerenciado pelo Wings/Pterodactyl têm outro destino.
O gerador aguarda cinco segundos sem alterações no arquivo e publica tamanho e
SHA-256 em `manifest.json`. Não envie esse JSON manualmente. Para substituir
arquivos, prefira upload com nome temporário terminado em `.upload` e renomeie
somente após terminar a transferência.

O preset `server/defaults/resourcepackoverrides.json` é opcional e depende dos
packs e do mod Resource Pack Overrides indicados no README. Publique-o somente
depois de enviar esses arquivos. O instalador não distribui esse preset sozinho.
As atualizações do executável continuam sendo preparadas por
`PrepararAtualizacao.cmd`; o timer não gera `launcher/latest.json`.

Para verificar:

```sh
nginx -t
systemctl status nginx eternal-manifest.timer --no-pager
systemctl start eternal-manifest.service
journalctl -u eternal-manifest.service -n 20 --no-pager
curl -f http://10.0.0.13/manifest.json
curl -f https://eternalfast.aplot.cloud/manifest.json
```

O serviço do manifesto é `oneshot`: aparecer como `inactive (dead)` depois de
concluir com sucesso é esperado; o timer deve permanecer `active (waiting)`.
Se a URL local funcionar e o domínio retornar 502, confira o destino do túnel
no Proxmox. O proxy deve alcançar `http://10.0.0.13:80`.
