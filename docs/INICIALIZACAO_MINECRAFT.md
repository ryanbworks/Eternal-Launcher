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

Antes de iniciar, também inclui o servidor Multiplayer configurado em
`LauncherConfig.h` na lista `servers.dat`. O exemplo é `Eternal SMP` em
`mc.seugrupo.com:25565`. Trocar o nome ou endereço na configuração atualiza a
entrada gerenciada; servidores que você adicionou manualmente são preservados.
O arquivo `src/scripts/GzipFile.ps1` acompanha a aplicação para manter o formato
compactado do Minecraft.

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
    ├── eternal-classpath.txt
    └── versions\
        ├── 1.20.1\1.20.1.jar
        └── 1.20.1-forge-47.4.10\1.20.1-forge-47.4.10.json
```

Arquivos existentes com tamanho e hash corretos são reutilizados. Um download
interrompido permanece em `.part` e não corrompe o destino anterior.

## Identidade do jogador

O nickname aceita de 3 a 16 caracteres (`A-Z`, `a-z`, `0-9` e `_`). O UUID é
derivado de `OfflinePlayer:<nickname>`, compatível com servidores offline. Não
há autenticação Microsoft ou concessão de licença; os usuários devem possuir o
jogo.

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

// TODO: remover com segurança mods que deixarem de existir no manifesto.

// TODO: substituir a identidade offline por autenticação Microsoft, se desejado.
```
