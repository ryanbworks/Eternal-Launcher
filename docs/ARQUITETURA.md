# Arquitetura inicial

## Objetivo

A estrutura separa a interface das integrações que serão implementadas no futuro.
O servidor poderá usar NGINX, outro servidor web ou um serviço gerenciado sem que
os componentes visuais precisem conhecer essa decisão de infraestrutura.

## Responsabilidades

### Aplicação

`LauncherApplication` inicializa o GDI+, cria a janela principal e mantém o loop
de mensagens do Windows. Esse nível não contém regras específicas do Minecraft.

### Página

`MainPage` compõe os componentes, mantém o estado visual e encaminha as ações do
usuário. Ela inicia a consulta HTTP em segundo plano e recebe o resultado por uma
mensagem interna do Windows. O fluxo acionado pelo botão principal permanece
temporário e está marcado como `MOCK` no código.

### Componentes

- `HeaderComponent`: marca, versão e acesso futuro às configurações.
- `HeroComponent`: apresentação do modpack e versões principais.
- `PlayerProfileComponent`: área de identificação do jogador.
- `StatusPanelComponent`: resumo de instalação, mods e servidor HTTP.
- `PlayPanelComponent`: status atual e ação primária.
- `ProgressComponent`: atividade e progresso de operações futuras.
- `NewsPanelComponent`: avisos e notícias do grupo.

Os componentes recebem dados e desenham sua própria área. Eles não acessam rede,
arquivos ou configurações persistidas.

### Estado e mocks

`LauncherViewState` centraliza os dados necessários para desenhar a tela. O enum
`LauncherOperation` já representa estados futuros como erro de conexão, arquivo
corrompido, atualização disponível e preparação do Minecraft.

`MockLauncherData` fornece os dados temporários de instalação, mods, notícias e
progresso. O cartão do servidor começa como “Conectando” e é substituído pelo
resultado real do `HttpFileService`.

### Serviços

Os contratos de `src/services` separam a interface das integrações:

- `IFileServerService`: contrato para obter o manifesto e representar a resposta;
- `HttpFileService`: implementação real com WinHTTP, timeout e suporte a HTTP/HTTPS;
- `IUpdateService`: comparação e atualização dos arquivos locais;
- `IMinecraftLauncherService`: preparação e inicialização do jogo.

Os dois últimos continuam sem implementação concreta. O manifesto recebido ainda
não é interpretado como uma lista de arquivos e não dispara downloads.

Não existe uma classe chamada `NginxService`: NGINX é apenas uma opção de
hospedagem e não deve se tornar uma dependência conceitual do launcher.

## Fluxo futuro de referência

```text
Iniciar launcher
  → carregar configurações
  → verificar servidor HTTP
  → obter manifesto
  → verificar arquivos locais
  → comparar versões e hashes
  → baixar arquivos necessários
  → verificar Minecraft e Forge
  → preparar argumentos
  → iniciar Minecraft
```

Esse fluxo não está implementado na versão atual. Ele orienta somente os nomes,
estados e contratos já preparados no projeto.

## Decisões desta etapa

- A interface é nativa e não exige runtime web ou framework externo.
- As formas da identidade visual são vetoriais e escaláveis.
- O layout é fixo nesta primeira versão para preservar alinhamento e hierarquia.
- Configurações, rede e sistema de arquivos permanecem fora dos componentes.
- Comentários explicam decisões, mocks e pontos de integração, evitando narrar
  instruções óbvias do código.
