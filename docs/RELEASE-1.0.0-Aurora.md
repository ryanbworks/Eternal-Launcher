# v1.0.0-Aurora

Primeira release estável com AutoUpdate do executável. Publicada em 3 de outubro
de 2026 para Windows x64.

- [Baixar EternalLauncher.exe](https://eternalfast.aplot.cloud/launcher/releases/1.0.0/EternalLauncher.exe)
- [Manifesto assinado](https://eternalfast.aplot.cloud/launcher/latest.json)
- [Gerar e publicar as próximas versões](AUTOUPDATE.md)

## Mudanças

- Verificação automática na abertura, download validado e reabertura automática.
- Assinatura RSA-PSS/SHA-256 com chave pública incorporada.
- Backup e rollback se a nova versão não confirmar a abertura em 30 segundos.
- Recuperação após interrupções; falhas permitem continuar na versão instalada.
- Link de download manual na interface e ferramenta de release para PowerShell 7.
- Versão centralizada no CMake e nas propriedades do executável.

Usuários da v0.1.1-Vagalume devem substituir o executável manualmente uma vez.
As atualizações seguintes são automáticas a partir da Aurora. A RAM máxima
padrão continua em 4 GB, e as preferências e a instância do jogo são preservadas.

## Evidência da distribuição

```text
Arquivo: EternalLauncher.exe
Versão: 1.0.0-Aurora
Arquitetura: x64
Tamanho: 3469824 bytes
SHA-256: c1699d4ad1adc6ee6f892f0beeebd921e256807aa101432b5485d2af629b48ed
```

Compilação Release MSVC `/W4` sem avisos. CTest: **5/5 aprovado**, incluindo
**53 verificações de AutoUpdate** com chave exclusiva de testes, troca real de
executáveis, rollback, timeout, permissões, bloqueio, cancelamento, corrupção,
concorrência e recuperação. Controles nativos de bloqueio/recuperação também
foram exercitados em janela de testes oculta.

As sete capturas em `.impeccable/review/autoupdate/` foram verificadas nos tamanhos
1180 × 730 e 944 × 584. O revisor independente retornou `ship` para o escopo dos
estados de atualização, link de recuperação e identidade de versão. São previews
sintéticos do renderer de produção; não comprovam comportamento de uma sessão
de jogo. Login Microsoft, instalação completa e gameplay não foram repetidos
nesta release.

Publicação feita por SFTP em `/srv/eternal-launcher/files/launcher/`, com SHA-256
remoto conferido e manifesto publicado por renomeação após o executável. NGINX
passou em `nginx -t` antes da recarga; o timer de mods permaneceu ativo.

Após publicar, foram verificados pelo HTTPS público a assinatura, tamanho,
SHA-256 e `Cache-Control: no-store`. O mesmo transporte WinHTTP usado pelo
launcher baixou e validou o executável publicado. O teste de cancelamento real
do transporte respondeu em 63 ms. Arquivos temporários `.upload` retornam HTTP
403; o manifesto de mods continuou HTTP 200 com 146 entradas.

Não foram publicadas versões fictícias, chaves privadas nem credenciais.
