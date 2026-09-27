# Arquitetura

## Objetivo

A estrutura separa interface, comunicação HTTP e inicialização do Minecraft. O
servidor pode usar NGINX, outro servidor web ou um serviço gerenciado sem que os
componentes visuais conheçam a infraestrutura escolhida.

## Responsabilidades

### Aplicação

`LauncherApplication` inicializa o GDI+, cria a janela principal e mantém o loop
de mensagens do Windows. Esse nível não contém regras específicas do Minecraft.

### Página

`MainPage` compõe os componentes, mantém o estado visual e encaminha as ações do
usuário. A consulta HTTP acontece em segundo plano. A página usa os contratos de
serviço para validar a instalação, abrir o jogo e acompanhar o processo, sem
conhecer os argumentos internos do Java.

### Componentes

- `HeaderComponent`: marca, versão e acesso futuro às configurações.
- `HeroComponent`: apresentação do modpack e versões principais.
- `PlayerProfileComponent`: identificação local do jogador.
- `StatusPanelComponent`: instalação, mods e servidor HTTP.
- `PlayPanelComponent`: status atual e ação primária.
- `ProgressComponent`: atividade e progresso de operações.
- `NewsPanelComponent`: avisos e notícias do grupo.

Os componentes recebem dados e desenham sua própria área. Eles não acessam rede,
arquivos ou configurações persistidas.

### Estado e mocks

`LauncherViewState` centraliza os dados da tela. `LauncherOperation` representa
estados como conexão, atualização, inicialização, jogo em execução e erro.

`MockLauncherData` ainda fornece notícias, status dos mods e valores temporários
de progresso. Os cartões de servidor e instalação são substituídos por resultados
reais quando a verificação inicial termina.

### Serviços

- `IFileServerService`: contrato para obter o manifesto HTTP.
- `HttpFileService`: implementação assíncrona baseada em WinHTTP.
- `IMinecraftLauncherService`: contrato de validação e inicialização do jogo.
- `MinecraftLauncherService`: localiza Java 17, valida a instância, cria e
  acompanha o processo do Minecraft.
- `MinecraftLaunchCommandBuilder`: monta classpath, módulos, parâmetros de JVM e
  argumentos do Forge `47.4.10`.
- `IUpdateService`: fronteira reservada para comparação e atualização de arquivos.

O download e a interpretação do manifesto como lista de arquivos ainda não foram
implementados. Não existe `NginxService`: NGINX é somente uma possibilidade de
hospedagem e não faz parte do domínio do launcher.

## Fluxo atual

```text
Iniciar launcher
  → consultar manifesto HTTP
  → localizar Java 17
  → validar Minecraft, Forge, assets, bibliotecas e nativas
  → validar nickname
  → montar parâmetros JVM/Forge
  → criar processo javaw.exe
  → acompanhar o processo até o encerramento
```

## Fluxo futuro

```text
Iniciar launcher
  → carregar configurações
  → verificar servidor HTTP
  → obter e interpretar manifesto
  → verificar arquivos locais
  → comparar versões e hashes
  → baixar arquivos necessários
  → validar a instalação preparada
  → iniciar Minecraft
```

## Decisões importantes

- A instância em `%APPDATA%\.eternal-launcher\minecraft` é isolada da
  `.minecraft` convencional.
- O nickname gera uma identidade offline determinística; autenticação Microsoft
  não faz parte desta etapa.
- O serviço aceita apenas Java 17, versão exigida pelo Minecraft 1.20.1.
- A memória está centralizada na configuração, atualmente entre 512 MB e 4 GB.
- Configurações, rede e sistema de arquivos permanecem fora dos componentes.
- Comentários explicam decisões, mocks e integrações futuras sem narrar código
  óbvio.
