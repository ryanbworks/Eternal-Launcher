# Eternal Launcher

Launcher desktop privado para uma instalação padronizada de Minecraft com mods.
A versão atual mantém a interface nativa e já consulta o servidor HTTP, valida
uma instância local e inicia o Minecraft `1.20.1` com Forge `47.4.10` usando o
nickname informado.

![Interface principal do Eternal Launcher](docs/images/interface-principal.png)

## Funcionalidades atuais

- Interface nativa para Windows com tema escuro e identidade visual própria.
- Campo de nickname com validação de 3 a 16 letras, números ou `_`.
- Perfil fixo do Minecraft `1.20.1` com Forge `47.4.10`.
- Consulta real e assíncrona ao manifesto do servidor HTTP.
- Validação do Java 17, perfil Forge, bibliotecas, assets e arquivos nativos.
- Montagem dos argumentos do Forge e inicialização pelo `javaw.exe`.
- Acompanhamento do processo para impedir duas sessões pelo mesmo launcher.
- Pasta de jogo isolada das outras instalações de Minecraft do usuário.
- Estados visuais claros para instalação incompleta, jogo aberto e falha.

> O nickname funciona como identidade local, sem autenticação Microsoft. Por
> isso, essa sessão só é adequada para servidores configurados em modo offline.

## O que ainda é simulado

- Verificação de versões e hashes dos mods.
- Download e remoção de arquivos do modpack.
- Instalação automática do Java, Minecraft e Forge.
- Persistência do nickname e das preferências.
- Atualização automática do próprio launcher.

O launcher não altera arquivos ao validar a instalação. Quando algum requisito
está ausente, ele informa o problema e preserva a instância local.

## Tecnologias

- C++20
- CMake 3.21 ou superior
- Win32 API
- GDI+ para renderização dos componentes
- WinHTTP para comunicação com o servidor de arquivos
- Windows CNG (`bcrypt`) para o UUID local do jogador
- Visual Studio com a carga de trabalho de desenvolvimento desktop em C++

A aplicação utiliza somente bibliotecas fornecidas pelo Windows. O cliente não
fica acoplado ao NGINX: qualquer servidor HTTP compatível poderá hospedar os
arquivos futuramente.

## Como compilar

### Visual Studio

1. Abra a pasta do projeto no Visual Studio.
2. Selecione um dos presets presentes em `CMakePresets.json`.
3. Compile e execute o alvo `EternalLauncher`.

### Developer Command Prompt

```powershell
cmake --preset x64-debug
cmake --build out/build/x64-debug
ctest --test-dir out/build/x64-debug --output-on-failure
./out/build/x64-debug/EternalLauncher.exe
```

Também estão disponíveis os presets `x64-release`, `x86-debug` e `x86-release`.

## Organização

```text
assets/                       Recursos visuais futuros
docs/                         Documentação e capturas da interface
src/
├── app/                      Inicialização e ciclo de vida da aplicação
├── components/               Componentes visuais reutilizáveis
├── config/                   Versões e opções centrais do launcher
├── constants/                IDs e dimensões compartilhadas
├── mocks/                    Dados exclusivamente simulados
├── pages/                    Composição e interação das telas
├── services/                 HTTP, validação e inicialização do jogo
├── styles/                   Paleta e tipografia da interface
├── types/                    Estados e modelos compartilhados
└── utils/                    Utilitários de desenho
tests/                        Testes automatizados dos serviços
```

Documentação complementar:

- [Arquitetura do projeto](docs/ARQUITETURA.md)
- [Inicialização do Minecraft](docs/INICIALIZACAO_MINECRAFT.md)
- [Servidor HTTP de arquivos](docs/SERVIDOR_HTTP.md)

## Configuração central

Versões, memória e endereço HTTP ficam em `src/config/LauncherConfig.h`. A
instância é esperada em `%APPDATA%\.eternal-launcher\minecraft`, enquanto o
Java distribuído com o projeto pode ficar em
`%APPDATA%\.eternal-launcher\runtime\bin\javaw.exe`.

O servidor atual é consultado por meio de `FileServerBaseUrl`. Futuramente essa
URL será movida para um arquivo de configuração externo.

## Próximas etapas planejadas

- Interpretar o manifesto como uma lista versionada de arquivos.
- Comparar versões e hashes da instância local.
- Baixar somente arquivos ausentes ou desatualizados.
- Preparar automaticamente Java, Minecraft, Forge, assets e bibliotecas.
- Persistir o nickname e permitir configurar memória.
- Implementar atualização do próprio launcher.
