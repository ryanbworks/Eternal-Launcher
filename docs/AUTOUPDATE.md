# AutoUpdate — v1.0.0-Aurora

A Aurora é a primeira versão com atualização do próprio executável. Quem usa
v0.1.1-Vagalume precisa baixar a Aurora uma vez e substituir o executável antigo.
Preferências, nickname, autenticação e Minecraft continuam na instância existente.
A distribuição é um único executável portátil Windows x64, com RAM máxima padrão
de 4096 MiB. O nome Aurora é separado da comparação numérica de versões.

## Fluxo no Windows

Na abertura normal, antes de habilitar as ações, o launcher consulta
`https://eternalfast.aplot.cloud/launcher/latest.json`. Se houver versão superior,
mostra um aviso nativo do Windows com a nova versão. **OK** confirma o download
e a instalação; **Cancelar** libera a versão atual e a próxima abertura tentará
novamente. Depois da confirmação, mostra progresso, valida o arquivo, fecha
normalmente e reabre no mesmo caminho. Os downloads do Minecraft continuam
vinculados ao botão Jogar.

Falhas liberam o fluxo normal da versão instalada, deixam um aviso e o link
**Baixar launcher**. A próxima abertura tenta novamente. O AutoUpdate não é
executado durante o jogo, nos testes de interface nem em `--render-preview`.
Uma falha no servidor de mods ainda segue o tratamento próprio da instalação.

O manifesto tem `schemaVersion: 1`, `payload` e `signature`. Os dois últimos
campos usam Base64. A assinatura RSA-PSS/SHA-256 (RSA 3072, salt de 32 bytes)
protege os bytes UTF-8 exatos do payload, sem reserialização. Seu conteúdo é:

```json
{
  "version": "1.0.0",
  "codename": "Aurora",
  "architecture": "x64",
  "path": "/launcher/releases/1.0.0/EternalLauncher.exe",
  "size": 123456,
  "sha256": "<64 caracteres hexadecimais minúsculos>"
}
```

O tamanho acima é ilustrativo; a ferramenta de release calcula os valores reais.
Somente versões numéricas `major.minor.patch` maiores são aplicadas. Cada número
fica entre 0 e 65535; zeros à esquerda e sufixos não são aceitos. A arquitetura
precisa corresponder ao processo. O caminho é derivado da versão e restrito ao
domínio HTTPS compilado. Redirecionamentos são recusados. Manifestos têm limite
de 64 KiB; executáveis, de 256 MiB. A consulta tem orçamento total de 10 segundos,
e o download, 30 segundos por leitura sem progresso. O fechamento cancela o I/O.

## Troca e recuperação

Uma pasta `<nome-do-executável>.update` ao lado do launcher guarda o manifesto,
o download, a cópia auxiliar e o registro da transação. Isso permite manter o
backup e o executável no mesmo volume. A cópia auxiliar verifica a identidade
do processo original, aguarda sua saída, revalida o download e usa `ReplaceFileW`
para substituir o executável com backup.

A nova versão confirma que criou sua janela em até 30 segundos. Sem confirmação,
somente o processo iniciado pelo auxiliar é encerrado, o backup é restaurado e
a versão anterior reabre. Um registro durável permite finalizar ou limpar uma
transação interrompida na próxima inicialização. A limpeza remove somente os
arquivos conhecidos, sem percorrer outras pastas.

Mutexes por caminho de executável impedem duas instâncias de atualizarem a mesma
cópia. Não há elevação automática: uma pasta sem permissão de escrita gera aviso
e permite o download manual. Se um bloqueio externo impedir também o rollback,
o auxiliar informa onde encontrar `previous.exe` para recuperação manual.

## Gerar uma release

### Assistente de dois cliques

Abra `PrepararAtualizacao.cmd` na raiz do projeto. Informe um número novo, como
`1.0.1`, e mantenha `Aurora` como nome se quiser. O assistente atualiza o CMake,
compila, executa os testes, assina o JSON e confere o pacote. Ele procura o
PowerShell 7 instalado, inclusive o runtime local do Codex quando disponível.
Se não encontrar, informa a dependência; não instala programas automaticamente.

Ao terminar, abre `out/releases/1.0.1/FILEZILLA/`. No FileZilla:

1. Abra `/srv/eternal-launcher/files` no servidor e envie a pasta `launcher`
   contida em `FILEZILLA`. Aguarde todos os envios terminarem sem falhas.
2. Volte à janela do assistente e pressione Enter. Ele baixa o executável pelo
   HTTPS público e confere assinatura do manifesto local, tamanho e SHA-256.
3. Após a confirmação, renomeie no servidor `launcher/latest.json.upload` para
   `launcher/latest.json`, substituindo o manifesto antigo.
4. Pressione Enter novamente no assistente para conferir a versão publicada.

O arquivo `.upload` ainda não anuncia uma atualização. A renomeação final é o
momento em que a versão fica disponível aos jogadores, na próxima abertura do
launcher. Não edite o JSON e não envie a chave privada. O nome da versão pode
continuar igual; o número precisa aumentar. Pacotes locais existentes são
preservados, e as pastas das releases anteriores devem continuar no servidor.

O pacote também contém `COMO-ENVIAR.txt`, com os passos e os comandos para retomar
a conferência se fechar o assistente. `-Preview -Version 1.0.1` permite conferir
as entradas sem alterar arquivos. O exemplo não cria nem publica a versão 1.0.1.

### Uso direto pelo PowerShell

Requisitos: Windows, Visual Studio com ferramentas C++ x64 e PowerShell 7.
O repositório usa CMake/Ninja disponibilizados pelo Visual Studio.
Esses são os requisitos da ferramenta de publicação. Para compilar e executar
o launcher na IDE, PowerShell 7 é opcional: sem ele, apenas a suíte de integração
do AutoUpdate deixa de ser registrada. A ferramenta de release passa seu próprio
`pwsh.exe` ao CMake e exige essa suíte com `ETERNAL_REQUIRE_UPDATE_TESTS=ON`,
impedindo a publicação sem a validação completa.

1. Atualize a versão e o codinome em `CMakeLists.txt`.
2. Execute `pwsh -File tools/Release.ps1` na raiz do projeto.
3. Verifique o pacote com `pwsh -File tools/VerifyRelease.ps1 -PackageDirectory out/releases/1.0.0`.

A ferramenta compila Release, roda todos os testes e gera:

```text
out/releases/1.0.0/
  SHA256SUMS.txt
  launcher/
    latest.json
    releases/1.0.0/EternalLauncher.exe
```

A chave privada está em `%LOCALAPPDATA%\EternalLauncher\Signing\release-key.dpapi`,
criptografada com DPAPI para a conta Windows atual. Somente a chave pública entra
no código. O comando `-InitializeKeyOnly` inicializa uma chave uma única vez e
recusa substituir uma chave pública diferente. Releases futuras devem usar a
mesma chave: preserve o perfil Windows e seu backup protegido. A perda da chave
exigirá redistribuir manualmente um launcher com outra chave de confiança.

`-SkipBuild` serve apenas para reempacotar uma compilação já validada. Os testes
usam uma chave própria deliberadamente pública em `tests/update-trust`; essa
chave não assina releases e não é aceita pelo launcher distribuído.

## Publicar por FileZilla/SFTP

Conexão SFTP: host `10.0.0.51`, porta `22`. Use as credenciais de publicação fora
do repositório. O destino é `/srv/eternal-launcher/files`.

1. Crie `launcher/releases/1.0.0/` e envie o executável como
   `EternalLauncher.exe.upload`.
2. Confira o SHA-256 remoto com `sha256sum` e o arquivo local `SHA256SUMS.txt`.
3. Renomeie para `EternalLauncher.exe`. Não substitua arquivos de uma versão já
   publicada por conteúdo diferente: gere um novo número de versão.
4. Envie `launcher/latest.json` como `launcher/latest.json.upload` e renomeie para
   `latest.json` somente após concluir e verificar o executável. A renomeação
   publica o anúncio completo de uma só vez.
5. Execute `pwsh -File tools/VerifyRelease.ps1 -Public` para conferir pelo HTTPS
   público a assinatura, o SHA-256, o tamanho e o cabeçalho `no-store`.

O NGINX precisa servir `/launcher/latest.json` como JSON com
`Cache-Control: no-store`. A configuração em `infrastructure/nginx` inclui essa
regra e bloqueia arquivos `.upload`/`.tmp` dentro de `launcher/`. Valide com
`nginx -t` antes da recarga. A pasta de mods e seu timer seguem independentes.

## Validação

`LauncherAutoUpdateTests` assina fixtures com a chave de testes e executa trocas
reais em pastas isoladas: sucesso, rollback por encerramento e por timeout,
arquivo bloqueado, pasta sem permissão, cancelamento, corrupção, versões iguais
ou antigas, concorrência e recuperação após interrupções. Também verifica
assinaturas, esquema, limites, arquitetura e caminhos inválidos.

`LauncherUpdateProbe.exe` é uma ferramenta de smoke test que não é distribuída.
`--cancel` verifica cancelamento real do WinHTTP; sem argumentos, consulta e
valida a release publicada; `--download <arquivo>` também baixa e verifica o PE.
As simulações nunca publicam versões de teste no servidor.
