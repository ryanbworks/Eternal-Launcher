# Eternal Launcher

Launcher desktop privado para uma instalação padronizada de Minecraft com mods.
Esta versão (`0.1.0-visual`) representa a primeira etapa de interface e
arquitetura. A consulta ao manifesto HTTP já é real; download, verificação de
arquivos e inicialização do jogo continuam fora do escopo.

![Interface principal do Eternal Launcher](docs/images/interface-principal.png)

## Escopo desta versão

- Interface nativa para Windows com tema escuro e identidade visual própria.
- Campo editável de nickname, preenchido inicialmente com um valor de demonstração.
- Informações do Minecraft `1.20.1` e Forge `47.4.10`.
- Cartões simulados de instalação e mods.
- Consulta real e assíncrona ao manifesto do servidor HTTP.
- Botão principal de jogo com uma sequência visual simulada de preparação.
- Áreas de notícias, atividade, progresso e versão do launcher.
- Cliente HTTP baseado no WinHTTP e contratos para atualização e inicialização.

> **Importante:** pressionar **Jogar agora** apenas executa uma animação de estados.
> Nenhum arquivo local é verificado ou alterado e o Minecraft não é iniciado.

## Tecnologias

- C++20
- CMake 3.21 ou superior
- Win32 API
- GDI+ para renderização dos componentes
- WinHTTP para comunicação com o servidor de arquivos
- Visual Studio com a carga de trabalho de desenvolvimento desktop em C++

A interface usa somente bibliotecas fornecidas pelo Windows. Não há dependências
externas, gerenciador de pacotes ou código acoplado ao NGINX.

## Como compilar

### Visual Studio

1. Abra a pasta do projeto no Visual Studio.
2. Selecione um dos presets presentes em `CMakePresets.json`.
3. Compile e execute o alvo `EternalLauncher`.

### Developer Command Prompt

```powershell
cmake --preset x64-debug
cmake --build out/build/x64-debug
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
├── mocks/                    Dados e sequências exclusivamente simulados
├── pages/                    Composição e interação das telas
├── services/                 Contratos para integrações futuras
├── styles/                   Paleta e tipografia da interface
├── types/                    Estados e modelos compartilhados
└── utils/                    Utilitários de desenho
```

Uma explicação mais detalhada das responsabilidades está em
[docs/ARQUITETURA.md](docs/ARQUITETURA.md).

A configuração e as instruções do servidor de arquivos estão em
[docs/SERVIDOR_HTTP.md](docs/SERVIDOR_HTTP.md).

## Funcionalidade HTTP real

Ao abrir a tela, o launcher solicita `http://10.0.0.51/manifest.json` em uma
thread separada. A interface exibe o código HTTP e o tempo total da requisição.
Uma indisponibilidade altera o cartão do servidor e desabilita a ação principal.

O conteúdo recebido ainda não é usado para baixar ou comparar arquivos.

## Funcionalidades simuladas

Os dados em `src/mocks` simulam:

- instalação pronta;
- 37 mods atualizados;
- notícia do servidor;
- etapas de verificação, atualização, download e preparação do Minecraft.

## Próximas etapas planejadas

- Persistir nickname e preferências do usuário.
- Obter um manifesto genérico do servidor HTTP de arquivos.
- Comparar versões e hashes dos arquivos locais.
- Baixar somente arquivos ausentes ou desatualizados.
- Validar a instalação do Minecraft e do Forge.
- Preparar argumentos e iniciar o Minecraft.
- Implementar atualização do próprio launcher.

Essas etapas estão deliberadamente fora do escopo da versão visual atual.
