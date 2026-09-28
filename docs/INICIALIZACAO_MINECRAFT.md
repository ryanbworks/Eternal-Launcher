# Instalação e inicialização do Minecraft

## Perfil suportado

- Minecraft `1.20.1`;
- Forge `47.4.10`;
- Java `17`;
- asset index `5`;
- alvo `forgeclient`;
- memória inicial de 512 MB e máxima de 4 GB.

Os valores ficam em `src/config/LauncherConfig.h`.

## Instalação automática

Ao abrir, o launcher consulta o servidor privado e inspeciona a instalação local,
sem iniciar downloads. Depois do clique em **Jogar**, baixa o Temurin 17 portátil
se necessário e valida seu SHA-256. Em seguida, lê os manifestos oficiais da
Mojang e prepara cliente, bibliotecas, assets e configuração de log.

O instalador oficial do Forge é conferido pelo SHA-1 definido na configuração e
executado com `--installClient` dentro da instância isolada. Depois disso, o
launcher extrai as DLLs nativas, sincroniza o manifesto privado, grava
`eternal-classpath.txt` e inicia o jogo com o nickname informado.
Durante a instalação do Forge, a saída e os erros do instalador são gravados
em `minecraft\logs\forge-installer.log`. Se o processo não iniciar ou terminar
com erro, o launcher mostra o código correspondente e o caminho desse log.
Se o instalador iniciar mas falhar, o launcher repete uma vez com a preferência
de rede IPv4 desativada. As tentativas ficam no mesmo log.
O Java portátil só é aceito quando contém `java.exe` e `javaw.exe`.
Se o Windows não conseguir iniciar `java.exe` por erro de caminho ou de
executável, o launcher baixa e reinstala o Java portátil e tenta o Forge
novamente. O log registra cada tentativa e a presença dos caminhos necessários.

O launcher não altera a lista Multiplayer. O IP do servidor de jogo é adicionado
manualmente dentro do Minecraft, e as entradas existentes são preservadas.

```text
%APPDATA%\.eternal-launcher\
├── downloads\
│   └── forge-1.20.1-47.4.10-installer.jar
├── runtime\bin\java.exe
└── minecraft\
    ├── assets\
    ├── libraries\
    ├── mods\
    ├── natives\*.dll
    ├── logs\launcher-console.log
    ├── logs\forge-installer.log
    ├── eternal-classpath.txt
    └── versions\
        ├── 1.20.1\1.20.1.jar
        └── 1.20.1-forge-47.4.10\1.20.1-forge-47.4.10.json
```

Arquivos existentes com tamanho e hash corretos são reutilizados. Um download
interrompido permanece em `.part` e não corrompe o destino anterior.
Após validar o manifesto e concluir os downloads, o launcher remove da pasta
`minecraft\mods` os arquivos `.jar` que não aparecem mais na lista do servidor.

## Identidade do jogador

No modo de nickname local, o nome aceita de 3 a 16 caracteres (`A-Z`, `a-z`,
`0-9` e `_`). O UUID é derivado de `OfflinePlayer:<nickname>`, compatível com
servidores offline. No modo Original, o jogador entra com Microsoft, e o
launcher usa o nome, UUID e token do perfil Minecraft: Java Edition. Consulte
o README para configurar o Application ID exigido por esse modo.

## Validação e execução

Antes de iniciar, `MinecraftLauncherService` confirma Java 17, perfil Forge,
cliente, asset index, classpath, bibliotecas essenciais e DLLs nativos.
`MinecraftLaunchCommandBuilder` monta o module path e o classpath controlado,
evitando incluir artefatos temporários do instalador.

O jogo é executado por `java.exe` com `CREATE_NO_WINDOW`. A saída padrão e os
erros são direcionados para `minecraft\logs\launcher-console.log`, útil quando
Forge ou um mod falha antes de criar `latest.log`.

## Diagnóstico local

O executável de testes possui modos manuais que não fazem parte do fluxo normal:

```powershell
./out/build/x64-debug/EternalLauncherTests.exe --validate-installed
./out/build/x64-debug/EternalLauncherTests.exe --launch-installed Steve
```

O segundo comando aguarda 30 segundos para confirmar que o processo permanece
ativo; feche o jogo depois do diagnóstico.

## TODOs

```cpp
// TODO: carregar memória e servidor a partir de preferências persistidas.

```
