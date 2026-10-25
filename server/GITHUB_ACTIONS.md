# Publicar o launcher pelo GitHub

O workflow **Publicar launcher** compila e testa em Windows x64, assina o pacote
com a mesma chave das instalações existentes e publica por SFTP. A publicação
usa o Proxmox como subnet router para alcançar `10.0.0.13:22` pelo Tailscale.

## Rede

- Proxmox: `10.0.0.10`, Tailscale `100.102.117.89`.
- Rede anunciada e aprovada: `10.0.0.0/24`.
- Encaminhamento IPv4 persistente: `/etc/sysctl.d/99-eternal-tailscale.conf`.
- A Action já habilita `--accept-routes` no executor Linux; não repetir essa
  opção em `args`. O Proxmox conserva o SNAT padrão.
- Liberar no firewall do Proxmox/container o encaminhamento necessário para
  `10.0.0.13:22`, caso alguma regra existente o bloqueie.

Essa rota mantém os endereços locais: nenhuma alteração de IP é feita nas VMs.
Os clientes que usam a rota precisam de acesso permitido pela política Tailscale.

## Identidade do GitHub no Tailscale

No painel Tailscale, criar `tag:eternal-ci` e permitir que ela alcance
`10.0.0.13` somente em TCP/22. Exemplo de trechos a integrar à política existente:

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

As regras são cumulativas. Uma regra antiga que concede tudo a `*` também
concede tudo ao executor: revisar essa regra para manter os acessos dos usuários
e excluir a identidade de CI desse acesso amplo. Acrescentar o exemplo sozinho
não remove permissões concedidas por outras regras.

Em **Trust credentials**, criar uma credencial **OpenID Connect**:

- Issuer: GitHub Actions (`https://token.actions.githubusercontent.com`).
- Subject: `repo:ryanbworks@213251617/Eternal-Launcher@1391560141:environment:production`.
- Custom claim `ref`: `refs/heads/master`.
- Escopo: `auth_keys` com permissão de escrita e tag `tag:eternal-ci`.
- Copiar Client ID e Audience para as variáveis do ambiente GitHub abaixo.

O token emitido pelo GitHub para este repositório inclui os IDs da conta e do
repositório no Subject. O formato sem esses IDs não corresponde ao token atual.
Se houver erro 403, o detalhe da credencial mostra o Subject recebido; conferir
os IDs com a API do GitHub antes de ajustar, preservando o ambiente `production`
e a restrição da branch `master`.

A identidade temporária do Actions é removida ao encerrar a execução. Esse
acesso usa o OpenSSH/SFTP do container; não depende de habilitar Tailscale SSH.

## Ambiente production no GitHub

Criar em Settings → Environments o ambiente `production` e permitir somente
a branch `master`. O workflow não publica a partir de outras branches.

| Tipo | Nome | Conteúdo |
|---|---|---|
| Variable | `TS_CLIENT_ID` | Client ID da identidade OIDC Tailscale |
| Variable | `TS_AUDIENCE` | Audience da identidade OIDC Tailscale |
| Secret | `SFTP_PRIVATE_KEY` | Chave OpenSSH Ed25519 do usuário `eternal-deploy` |
| Secret | `SFTP_KNOWN_HOSTS` | Chave pública do host `10.0.0.13`, conferida por um canal confiável |
| Secret | `ETERNAL_SIGNING_KEY_PKCS8` | Chave RSA original, PKCS#8 DER em Base64 |

A chave de assinatura deve ser extraída do DPAPI pela mesma conta Windows que
publicou o launcher e cadastrada no secret sem passar pelo checkout, artefatos
ou logs. `CiSigning.ps1` importa a chave em memória e limpa a variável de
ambiente. `Release.ps1` confere se ela corresponde à chave pública do projeto.
O build e os testes executam antes da etapa que recebe esse secret.

O usuário SFTP usa autenticação por chave, sem shell nem encaminhamento de
portas. O chroot é `/srv/eternal-launcher`; por isso o publicador usa
`/files/launcher`, que corresponde a `/srv/eternal-launcher/files/launcher` no
container. Somente as pastas `launcher` e `launcher/releases` recebem permissão
de escrita para o grupo de publicação. O código não apaga versões anteriores.

## Usar o botão

1. Abrir Actions → Publicar launcher → Run workflow, na branch `master`.
2. Informar uma versão maior que a publicada, como `1.0.12`, e um nome como `Aura`.
3. Na primeira execução, manter **Somente compilar e verificar acesso** marcado.
4. Depois da conferência, executar com a mesma versão e nome e desmarcar a opção
   para publicar. O modo de verificação não envia uma release nem modifica o anúncio.

A versão/nome informados são aplicados ao checkout temporário do executor.
O workflow não altera `CMakeLists.txt` na branch nem cria commits de versão.
O pacote fica disponível nos artefatos da execução por 30 dias.

O publicador confere assinatura e SHA-256, rejeita versões iguais ou menores,
envia o EXE com nome temporário e confere o download pelo HTTPS antes de trocar
`latest.json` por rename atômico. Se a conferência final do manifesto falhar,
restaura o anúncio anterior quando ele ainda corresponde à própria publicação.
Uma repetição pode reutilizar um EXE idêntico; nunca substitui bytes diferentes
de uma versão existente.

O workflow serializa publicações e o servidor usa a pasta `.ci-publish-lock`
como trava adicional. Se o processo for encerrado abruptamente, conferir que
nenhuma publicação está rodando antes de remover essa pasta vazia pelo SFTP.
Arquivos temporários terminam em `.upload`, bloqueados pela configuração NGINX.

## Validar os scripts

```powershell
pwsh -NoProfile -File tests/TestCiSigning.ps1
python -m pip install -r tools/ci-requirements.txt
python -m unittest discover -s tests -p test_publish_release.py -v
```

Fontes: [Subnet routers](https://tailscale.com/docs/features/subnet-routers),
[Action oficial](https://tailscale.com/docs/integrations/github/github-action),
[OIDC](https://tailscale.com/docs/features/workload-identity-federation).
