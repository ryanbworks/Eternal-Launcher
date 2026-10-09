# Arquitetura

## Objetivo

A estrutura separa interface, fontes de arquivos, preparação da instância e
execução. O servidor pode usar NGINX ou qualquer implementação HTTP sem alterar
os componentes visuais ou as regras do Minecraft.

## Camadas

### Aplicação e página

`LauncherApplication` inicializa GDI+, janela e loop de mensagens.
`MainPage` compõe a tela, mantém `LauncherViewState` e coordena operações em
threads próprias. Downloads nunca bloqueiam a thread da interface.

### Componentes visuais

- `HeaderComponent`: marca, versão e acesso às configurações.
- `HeroComponent`: identidade do modpack e versões.
- `PlayerProfileComponent`: nickname local.
- `StatusPanelComponent`: instalação, arquivos privados e servidor.
- `PlayPanelComponent`: estado atual e ação principal.
- `ProgressComponent`: resumo de atividade e progresso do AutoUpdate.
- `InstallationPanelComponent`: etapas, transferência atual e controles de recuperação da preparação.
- `NewsPanelComponent`: conteúdo editorial ainda mockado.

Os componentes apenas desenham dados recebidos. Eles não acessam rede nem disco.

### Serviços

- `HttpFileService`: obtém o manifesto privado via WinHTTP.
- `HttpDownloadClient`: transfere textos e binários.
- `JavaRuntimeService`: coordena o Java portátil.
- `PortableJavaInstaller`: baixa, verifica e publica o Temurin 17.
- `MinecraftManifestService`: converte manifestos oficiais em um plano explícito.
- `VerifiedDownloader`: baixa em paralelo, usa `.part` e verifica hash/tamanho.
- `GameInstallationService`: orquestra Mojang, Forge e arquivos privados.
- `InstallationCoordinator`: consulta o manifesto, prepara Java/jogo e valida a instância em um worker; não inicia o Minecraft.
- `MinecraftLaunchCommandBuilder`: monta módulos, classpath e argumentos.
- `MinecraftLauncherService`: valida e acompanha o processo do jogo.
- `IUpdateService`: fronteira futura para remoção de obsoletos e política de versão.
- `LauncherUpdateService`: preparação e aplicação do AutoUpdate com backup/rollback.
- `LauncherUpdateManifest`: versão numérica, assinatura RSA-PSS e validação do contrato.
- `LauncherUpdateTransport`: WinHTTP assíncrono, cancelamento e limites do AutoUpdate.

### Utilitários

- `JsonValue`: parser JSON mínimo usado nos manifestos.
- `FileHash`: SHA-1 e SHA-256 com Windows CNG.
- `ArchiveExtractor`: extração de ZIP/JAR usando o suporte do Windows.
- `InstallationProcess`: espera cancelável compartilhada pela extração e pelo instalador Forge.
- `DrawingUtils`: primitivas compartilhadas da interface.

## Fluxo atual

```text
Abrir launcher
  → conferir AutoUpdate assinado, aplicar e reabrir se houver versão nova
  → consultar manifesto privado e inspecionar a instalação local
  → aguardar o clique em Jogar
  → garantir Java 17 portátil
  → obter metadados oficiais do Minecraft 1.20.1
  → baixar/verificar cliente, bibliotecas, assets e logs
  → extrair bibliotecas nativas
  → baixar/verificar e executar o instalador Forge 47.4.10
  → sincronizar arquivos privados declarados no manifesto
  → gerar eternal-classpath.txt
  → validar nickname e instalação
  → iniciar Java e acompanhar o processo
```

## Decisões importantes

O [acompanhamento da instalação](ACOMPANHAMENTO_INSTALACAO.md) documenta o
contrato compartilhado, métricas, mailbox, cancelamento e evidências de teste.

- A instância em `%APPDATA%\.eternal-launcher\minecraft` não mistura mods com a
  `.minecraft` convencional.
- Java e jogo são obtidos de fontes oficiais; o servidor privado entrega apenas
  o conteúdo específico do grupo.
- Nenhum arquivo baixado é publicado antes da verificação de integridade.
- O classpath é salvo explicitamente para não incluir JARs auxiliares do
  instalador Forge.
- DLLs nativos são normalizados na raiz de `natives`, conforme o
  `java.library.path` usado no comando.
- A saída do Java fica em `logs/launcher-console.log` sem abrir console.
- A identidade offline é determinística e poderá ser substituída por um serviço
  de autenticação sem alterar o instalador.
- Memória, versões e endpoints permanecem centralizados em `LauncherConfig`.

## Extensões futuras

```text
Configurações persistentes
  → política de atualização e remoção segura
  → autenticação Microsoft
  → assinatura do manifesto de mods (AutoUpdate do launcher já implementado)
```
