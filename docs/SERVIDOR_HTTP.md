# Servidor HTTP de arquivos

## Estado atual

O servidor `10.0.0.51` utiliza Debian 13 e NGINX. O serviço está habilitado no
boot e publica arquivos através da porta HTTP 80.

- URL principal: `http://10.0.0.51/`
- Manifesto: `http://10.0.0.51/manifest.json`
- Mods: `http://10.0.0.51/mods/`
- Diretório local publicado: `/srv/eternal-launcher/files`
- Configuração do NGINX: `/etc/nginx/sites-available/eternal-launcher`

## Estrutura remota

```text
/srv/eternal-launcher/files/
├── config/
├── launcher/
├── manifests/
├── mods/
├── README.txt
└── manifest.json
```

O `manifest.json` inicial ainda não descreve arquivos reais. Ele existe apenas
como base para a futura implementação do serviço de atualização.

## Publicar mods com FileZilla

O servidor não expõe FTP tradicional na porta 21. Use **SFTP**, que transfere os
arquivos de forma criptografada pelo SSH:

```text
Protocolo: SFTP - SSH File Transfer Protocol
Host:      10.0.0.51
Porta:     22
Destino:   /srv/eternal-launcher/files/mods
```

Utilize uma credencial administrativa já autorizada no servidor. Senhas nunca
devem ser gravadas neste repositório.

> TODO: criar um usuário exclusivo de publicação, sem acesso administrativo,
> antes de disponibilizar o servidor fora da rede local.

## Administração

```bash
# Validar a configuração antes de recarregar.
nginx -t

# Aplicar alterações sem interromper downloads ativos.
systemctl reload nginx

# Consultar o estado do serviço.
systemctl status nginx

# Acompanhar requisições e erros.
tail -f /var/log/nginx/eternal-launcher.access.log
tail -f /var/log/nginx/eternal-launcher.error.log
```

## Segurança futura

- Criar uma conta SFTP dedicada somente ao diretório de publicação.
- Trocar credenciais administrativas que tenham sido compartilhadas.
- Adicionar HTTPS antes de permitir acesso pela internet.
- Restringir portas no firewall às redes realmente necessárias.
- Gerar hashes dos arquivos e assinar/versionar os manifestos.

Esses ajustes não são necessários para o protótipo dentro da rede local, mas
devem anteceder qualquer exposição pública do serviço.
