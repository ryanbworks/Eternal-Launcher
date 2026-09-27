# Inicialização do Minecraft

## Perfil suportado

O launcher prepara a linha de comando exclusivamente para:

- Minecraft `1.20.1`;
- Forge `47.4.10`;
- Java `17`;
- asset index `5`;
- alvo Forge `forgeclient`.

Esses valores ficam centralizados em `src/config/LauncherConfig.h`.

## Identidade do jogador

O nickname deve ter de 3 a 16 caracteres e conter somente letras, números ou
underline. O launcher gera o UUID determinístico compatível com
`OfflinePlayer:<nickname>` e inicia o jogo sem autenticação Microsoft.

Essa abordagem é destinada ao grupo privado em um servidor com `online-mode=false`.
Ela não autentica nem concede propriedade de uma conta oficial.

## Diretórios esperados

```text
%APPDATA%\.eternal-launcher\
├── runtime\bin\javaw.exe
└── minecraft\
    ├── assets\
    │   └── indexes\5.json
    ├── libraries\
    ├── mods\
    ├── natives\
    │   └── *.dll
    └── versions\
        ├── 1.20.1\1.20.1.jar
        └── 1.20.1-forge-47.4.10\1.20.1-forge-47.4.10.json
```

O `runtime` empacotado tem prioridade. Como alternativas, o serviço procura um
Java 17 em `JAVA_HOME` e depois no `PATH`.

## Validação anterior à abertura

Antes de chamar `CreateProcessW`, o serviço confirma:

1. Java 17 disponível;
2. perfil JSON do Forge presente;
3. JAR cliente do Minecraft presente;
4. índice de assets presente;
5. bibliotecas essenciais do bootstrap e do Forge presentes;
6. pelo menos uma biblioteca nativa extraída em `natives`.

Se algum requisito estiver ausente, a interface mostra a causa e não inicia um
processo parcial.

## Montagem e execução

`MinecraftLaunchCommandBuilder` reúne os JARs da pasta isolada, cria o module
path do Forge, define a memória e inclui o nickname e UUID offline. O
`MinecraftLauncherService` usa o diretório da instância como diretório de
trabalho, abre `javaw.exe` e guarda o identificador e o handle do processo.

Enquanto o processo estiver ativo, o botão fica bloqueado. Quando o jogo fecha,
a interface permite iniciar outra sessão.

## Limites desta etapa

O launcher ainda não prepara a instalação automaticamente.

```cpp
// TODO: obter no manifesto a relação de arquivos necessários para a instância.

// TODO: baixar e verificar Java, Minecraft, Forge, assets e bibliotecas.

// TODO: substituir a identidade offline por autenticação Microsoft, se desejado.
```

Até esses TODOs serem implementados, a pasta isolada precisa ser preparada antes
do primeiro uso. Mods podem ser copiados para `minecraft\mods`, mas a sincronização
automática com o servidor HTTP continua reservada para uma próxima etapa.
