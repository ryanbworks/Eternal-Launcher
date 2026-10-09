# Validação do redesign

Direção confirmada pelo proprietário: launcher C++ nativo para o servidor privado, tema escuro futurista com violeta, implementação direta no código.

## Acompanhamento da instalação — 2026-10-04

Release MSVC `/W4` sem avisos em `out/build/installation`; as seis suítes CTest
passaram, incluindo AutoUpdate e os novos testes de instalação. O host nativo
visível confirmou Tab e Enter dos controles de acompanhamento em tamanhos normal
e compacto. As capturas usam dados sintéticos; não foi executada instalação
completa pelos serviços públicos nem Minecraft/Forge real. Veja o
[registro de implementação e validação](ACOMPANHAMENTO_INSTALACAO.md) e o parecer
em `.impeccable/review/installation/review.md`.

## Implementação

- Layout de paisagem à esquerda e console contínuo de acesso à direita.
- Marca vetorial E/portal, ícone multirresolução, arte e fonte incorporadas no EXE.
- Botões Win32 personalizados, campo EDIT nativo e foco visível.
- Ordem de teclado: Nickname local → Original → nickname → Jogar → Configurações; Original oculta o campo.
- Transições de hover próximas de 180 ms e interpolação do progresso. A preferência de animação da área cliente do Windows desativa o movimento; o timer para quando não há transição ou operação indeterminada.
- Janela redimensionável e renderização escalada pelo tamanho/DPI. Composição lógica de 1180 × 730.
- Estados próprios para primeira instalação, autenticação, falha de autenticação e nickname inválido. Erros usam classificação compartilhada no painel de ação/atividade.
- Serviços de instalação, manifesto, Java, Microsoft, downloads, sincronização e inicialização preservados.

## Evidências

Compilação Release MSVC /W4 sem avisos. CTest: MinecraftLauncherServiceTests, 1/1 aprovado.

Dez renderizações determinísticas do renderizador final, incluindo dimensões 1180 × 730 e 944 × 584, sem cortes observados. As cenas usam dados ilustrativos.

Na primeira versão compilada, Computer Use verificou janela real, acesso Original por clique, ocultação do campo, Tab e Shift+Tab entre os controles e retorno ao modo local por Enter. O usuário interrompeu o controle com Esc; não houve mais automação da janela. A ordem de Tab corrigida na versão final foi revisada no código, sem novo teste da janela real.

Não foram executados login Microsoft, instalação de arquivos ou inicialização de Minecraft nesta tarefa. Transições completas de DPI, preferência de movimento reduzido e cadência das animações não foram exercitadas ao vivo. A tela de configurações foi implementada na extensão registrada abaixo. Notícias são explicitamente rotuladas como demonstração.

A revisão independente inicial e o parecer das correções estão em `.impeccable/review/`. O sistema visual está documentado em `DESIGN.md` e `.impeccable/design.json`.

## Extensão: configurações do Minecraft

Tela funcional no sistema visual existente, com RAM inicial/máxima, resolução,
tela cheia, Java automático/personalizado, pasta exclusiva do jogo, minimização
ao jogar e atalhos da instância salva. Só Salvar persiste o formulário; reset é
um rascunho. Instalação, login e jogo em execução bloqueiam alterações.

Release MSVC /W4 sem avisos em `out/build/settings`. CTest: **3/3 aprovados**.
`MinecraftLauncherServiceTests` mantém as regressões existentes;
`LauncherSettingsTests` verifica JSON UTF-8, defaults e arquivo inválido preservado,
salvamento inválido preservando dados, RAM/resolução/tela cheia no comando,
Java 17 e proteção de pastas. `SettingsPageTests` usa um host Win32 oculto e
controles EDIT/BUTTON reais para verificar edição, opções, bloqueios, validação,
restauração e descarte de rascunhos ao reabrir.

Capturas em `.impeccable/review/settings/`: normal 1180 × 730, compacto 944 × 584,
erro com RAM inicial 8192 e máxima 4096 MiB, além da tela principal preservada.
São renderizações determinísticas do código de produção, com campos nativos
representados por texto desenhado. Não comprovam foco/Tab ao vivo, seleção/caret,
seletor de arquivos/pastas, DPI ou comportamento real do Minecraft.

Não houve controle do desktop após a interrupção anterior via Esc. Nenhum login,
download, instalação ou início de jogo foi executado nesta extensão. A opção de
minimizar/restaurar foi inspecionada no código, sem exercício com Minecraft real.
O parecer independente e seu veredito sobre as correções estão na mesma pasta de revisão.

## Correção: flashes claros ao passar o mouse

O teste `WindowPaintingTests` reproduziu o apagamento do fundo dos botões das
duas telas com cor `#F0F0F0` antes da correção. O procedimento padrão de BUTTON
era executado antes do desenho personalizado. As prévias estáticas anteriores
não mostravam essa etapa do ciclo de pintura.

Os botões agora preservam seus pixels no `WM_ERASEBKGND`; o brush de fallback
é escuro. A janela inclui os controles nativos na apresentação com
`WS_EX_COMPOSITED`. Hover/foco invalidam o canvas e os botões personalizados,
sem pedir repintura dos campos EDIT a cada movimento. Mudanças na tela de
configurações notificam o mesmo controlador de pintura da tela principal.
Um frame marcado como desatualizado é renderizado e finalizado antes de qualquer
cópia para os controles, inclusive quando `WM_DRAWITEM` precede `WM_PAINT`.

Release MSVC /W4 sem avisos em `out/build/paint-fix`; CTest **4/4 aprovado**.
O teste de regressão usa a WindowProc de produção, controles Win32 reais num
host oculto e um HDC em memória. Confirma fundo escuro nas duas telas,
quadros atualizados ao entrar no hover antes de WM_PAINT e cobertura dos cantos
em 24 alternâncias de hover. O resultado anterior tinha quatro falhas; o novo
resultado tem zero. Nenhuma janela do usuário foi controlada.

Comparação pixel a pixel das prévias principal, compacta, configurações,
configurações compactas e erro: todas iguais às capturas aprovadas antes desta
correção. O layout e a paleta foram preservados. Esses testes verificam o caminho
nativo de pintura; não medem a cadência de apresentação na tela física.

## Extensão: barra de RAM e minimização padrão

A RAM máxima recebe um TRACKBAR nativo com passos de 512 MiB (0,5 GB),
leitura em GB com vírgula decimal e atalhos de 4, 6 e 8 GB. Os campos MiB
preservam entradas inteiras precisas. A faixa inicial de 0,5–16 GB cresce
para valores maiores; reduzir pela barra ou por um atalho também reduz uma
RAM inicial que ultrapasse a nova máxima. A opção de minimizar vem ativada
nos padrões e na restauração; uma preferência explicitamente salva como
desativada permanece respeitada.

Release MSVC /W4 sem avisos em `out/build/ram-slider`; CTest **4/4 aprovado**.
O host oculto usa os controles nativos e o controlador da tela de produção.
Verifica 1/1,5/2/4,5/6,5 GB, argumentos JVM exatos, setas/Page Down,
atalhos, edição precisa, faixa ampliada, bloqueios, restauração e pintura
escura. Os testes de preferências verificam minimização ativada sem arquivo
ou sem a propriedade, persistência de uma escolha desativada e restauração
da opção na tela.

Capturas finais em `.impeccable/review/ram-slider/`: normal 1180 × 730,
compacto 944 × 584, seleção 6,5 GB/6656 MiB e erro de limites de RAM.
Todas mostram a minimização padrão marcada. São renderizações determinísticas
GDI+ com dados ilustrativos; não comprovam arraste físico, cadência na tela
ou início do Minecraft. A revisão independente está na mesma pasta.
