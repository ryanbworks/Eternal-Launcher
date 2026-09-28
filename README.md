# Eternal Launcher

Launcher desktop privado para uma instalação padronizada de Minecraft `1.20.1`
com Forge `47.4.10`. A aplicação consulta um manifesto HTTP, prepara uma
instância isolada e inicia o jogo com o nickname informado.

![Interface principal do Eternal Launcher](docs/images/interface-principal.png)

## Baixar e usar

Baixe o executável ou o pacote ZIP na página de
[releases do Eternal Launcher](https://github.com/ryanbworks/Eternal-Launcher/releases/latest).
A distribuição é para Windows x64. Se escolher o ZIP, extraia os arquivos e abra
`EternalLauncher.exe`. O launcher inclui o runtime C++ e prepara o Java 17
automaticamente no primeiro clique em **Jogar**, com conexão à internet.

## Funcionalidades atuais

- Interface nativa para Windows com tema escuro e componentes separados.
- Escolha entre nickname local e login original com conta Microsoft.
- Campo de nickname com validação de 3 a 16 letras, números ou `_`.
- Consulta assíncrona ao manifesto do servidor HTTP configurado.
- Descoberta automática dos mods enviados à pasta `mods/` do servidor.
- Download automático do Java 17 portátil pela API oficial do Eclipse Adoptium.
- Instalação automática do cliente Minecraft, bibliotecas, assets e Forge.
- Verificação de SHA-1/SHA-256 e publicação atômica dos downloads.
- Sincronização dos arquivos descritos em `files` no manifesto privado.
- Remoção de mods locais que saíram do manifesto após a sincronização.
- Log do instalador Forge e recuperação automática de falhas do Java portátil.
- Extração das bibliotecas nativas e geração de classpath controlado.
- Inicialização real do Forge e log técnico em `logs/launcher-console.log`.
- Instância isolada em `%APPDATA%\.eternal-launcher\minecraft`.

No modo **Nickname local**, o servidor privado precisa aceitar sessões offline.
No modo **Original**, o launcher obtém nome, UUID e token do perfil Minecraft:
Java Edition da conta autenticada. A sessão fica apenas na memória e é solicitada
novamente quando o launcher é reaberto.

### Configurar login original

O Application (client) ID público já está configurado em
`src/config/LauncherConfig.h` e acompanha o executável compilado. Para usar
outro registro sem recompilar, defina a variável de ambiente antes de abrir o
launcher:

```powershell
$env:ETERNAL_LAUNCHER_MICROSOFT_CLIENT_ID = "SEU-APPLICATION-CLIENT-ID"
./out/build/x64-debug/EternalLauncher.exe
```

O ID é público; nunca coloque um client secret no executável. Ao escolher
**Original** e clicar em **Jogar**, abra a URL mostrada,
digite o código e conclua o login no navegador. A conta precisa possuir um
perfil Minecraft: Java Edition. A Microsoft/Minecraft também pode exigir
autorização específica do registro do aplicativo; se o serviço responder
`Invalid app registration` (HTTP 403), o login Microsoft e Xbox chegou ao
Minecraft Services, mas o serviço recusou o Application ID. Mudar o ID no
executável ou recompilar não concede essa autorização. O erro aponta para
`https://aka.ms/AppRegInfo`. Um desenvolvedor com o mesmo erro relatou ter
obtido aprovação da Mojang após enviar o Client ID pelo [formulário de registro
de aplicativo](https://forms.cloud.microsoft/Pages/ResponsePage.aspx?id=v4j5cvGGr0GRqy180BHbR-ajEQ1td1ROpz00KtS8Gd5UNVpPTkVLNFVROVQxNkdRMEtXVjNQQjdXVC4u).
A aprovação depende da análise da Microsoft/Mojang. Enquanto o registro não
for autorizado, o modo Original não conseguirá iniciar uma sessão online; o
modo de nickname local continua disponível.

## Primeiro uso

Ao abrir, o launcher consulta somente o servidor e mostra se a instalação local
está pronta. Os downloads começam depois que você clica em **Jogar**:

```text
Preparar Java 17 portátil
  → consultar manifestos oficiais da Mojang
  → baixar e verificar Minecraft, bibliotecas e assets
  → extrair DLLs nativos
  → baixar e executar o instalador oficial do Forge 47.4.10
  → sincronizar os arquivos do manifesto privado
  → iniciar o jogo com o nickname escolhido
```

Downloads já válidos são reaproveitados. Arquivos incompletos usam a extensão
`.part` e só substituem o destino depois da conferência do hash.

## O que ainda é simulado ou futuro

- Notícias exibidas na tela principal.
- Persistência das demais preferências.
- Atualização automática do executável do launcher.
- Tela funcional de configurações.

## Tecnologias

- C++20, CMake e Win32 API;
- GDI+ para a interface;
- WinHTTP para downloads;
- Windows CNG (`bcrypt`) para hashes e UUID local;
- Eclipse Temurin 17 como runtime portátil.

Não existe acoplamento ao NGINX. Qualquer servidor HTTP capaz de publicar o
manifesto e os arquivos pode ser utilizado.

## Como compilar

No Visual Studio, abra a pasta, selecione um preset de `CMakePresets.json` e
compile o alvo `EternalLauncher`. Em um Developer PowerShell:

```powershell
cmake --preset x64-debug
cmake --build out/build/x64-debug
ctest --test-dir out/build/x64-debug --output-on-failure
./out/build/x64-debug/EternalLauncher.exe
```

Também existem os presets `x64-release`, `x86-debug` e `x86-release`.

## Organização

```text
assets/                       Recursos visuais futuros
docs/                         Documentação e capturas
src/
├── app/                      Ciclo de vida da aplicação
├── components/               Componentes visuais
├── config/                   Versões, URLs e memória
├── constants/                IDs e dimensões compartilhadas
├── mocks/                    Conteúdo ainda simulado
├── pages/                    Composição e interação das telas
├── services/                 HTTP, instalação e inicialização
├── styles/                   Paleta e tipografia
├── types/                    Estados compartilhados
└── utils/                    JSON, hashes, arquivos e desenho
tests/                        Testes automatizados e diagnósticos locais
```

Documentação complementar:

- [Arquitetura do projeto](docs/ARQUITETURA.md)
- [Inicialização do Minecraft](docs/INICIALIZACAO_MINECRAFT.md)
- [Servidor HTTP de arquivos](docs/SERVIDOR_HTTP.md)

## Configuração central

Versões, memória e servidor ficam em `src/config/LauncherConfig.h`. Para trocar
o IP atual, altere `FileServerBaseUrl` e recompile. O launcher obtém Minecraft
dos serviços oficiais e usa esse servidor apenas para o manifesto, mods,
configurações e outros arquivos privados.

O launcher não configura o servidor Minecraft automaticamente. Adicione o IP
manualmente na tela Multiplayer do jogo. Essa lista pertence à instalação
isolada em `%APPDATA%\.eternal-launcher\minecraft`.

Para distribuir mods, basta enviar os `.jar` via SFTP para
`/srv/eternal-launcher/files/mods/`. O serviço no servidor atualiza o manifesto
automaticamente; ao clicar em **Jogar**, o launcher busca a lista mais recente.

O nickname válido é salvo automaticamente ao clicar em **Jogar** ou fechar o
launcher. Na próxima abertura ele reaparece no campo. O valor fica em
`%APPDATA%\.eternal-launcher\nickname.txt`; isso não é uma conta ou autenticação.

## TODOs principais

- Mover a URL HTTP e a memória para configurações persistidas.
- Estender a remoção de arquivos obsoletos para outros tipos além dos mods.
- Implementar atualização assinada do próprio launcher.
