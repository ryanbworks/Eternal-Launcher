# Animações do Eternal Launcher

O movimento preserva o tema escuro/violeta e os controles Win32. Nenhuma biblioteca
nova foi adicionada. O desenho continua em GDI+, com o buffer de quadro existente.

- Ao abrir, a paisagem aparece pela abertura do portal em 750 ms. Textos e ações
  ficam disponíveis desde o primeiro quadro. A animação não se repete ao voltar
  das configurações.
- O fundo da opção de acesso desliza entre Nickname local e Original. O formulário
  e a seleção acessível mudam imediatamente, sem esperar a animação.
- Configurações entra pela direita em 340 ms; a tela principal retorna pela
  esquerda em 280 ms. O cabeçalho fica parado. A desaceleração cúbica move também
  os HWNDs dos campos e botões, em um lote por quadro, sem recriar fontes.
  Foco, Tab, edição e ações continuam disponíveis durante o movimento. Uma nova
  abertura da mesma página não reinicia a transição nem descarta o rascunho.
- A entrada e a saída do acompanhamento da instalação revelam o painel em 240 ms.
  A imagem anterior fica em um buffer temporário, sem sobrepor textos. Cancelar,
  Abrir log e o console à direita respondem imediatamente.
- A barra de transferência avança suavemente até o percentual real. Percentual,
  bytes e contagens continuam exibindo a amostra recebida. Cada nova transferência
  reinicia a barra. Cancelar e Abrir log ganham hover interpolado.

A preferência **Mostrar animações no Windows**, lida por
`SPI_GETCLIENTAREAANIMATION`, torna o movimento imediato e mantém os sinais de
foco, seleção e progresso. `WM_SETTINGCHANGE` reaplica a preferência durante o uso.
O timer solicita `USER_TIMER_MINIMUM` (10 ms) durante transições, operações
indeterminadas e a atmosfera da tela inicial em primeiro plano. Ele para em janela
oculta/minimizada, encerramento e configurações após a entrada. A atmosfera deixa
de solicitar quadros ao perder o primeiro plano; efeitos de interação já iniciados
terminam normalmente. A preferência de movimento reduzido desativa tudo isso.

Na paisagem, 40 partículas em três camadas flutuam em velocidades distintas,
com tamanhos, rastros e brilho diferentes. Outras 24 acompanham dois arcos do portal.
O hover intensifica a energia e a abertura inicial produz uma ativação breve.
A atmosfera fica restrita ao lado da ilustração, sem encobrir os textos. O ciclo
base de deslocamento dura oito segundos; as camadas próximas passam mais rápido.

O botão Jogar ganha uma faixa de luz com bordas suaves de 480 ms ao receber hover
ou foco de teclado, luz que acompanha o cursor com interpolação de 55 ms e um
reflexo que percorre a borda. O ícone avança 10 pixels lógicos com um rastro curto;
a pressão comprime três pixels dentro do retângulo original.
O clique e Espaço produzem uma onda de 380 ms. A área clicável permanece a mesma.
A engrenagem gira 60 graus acompanhando o hover. As etapas em andamento recebem
um marcador orbital; as concluídas ou já disponíveis exibem um check e um pulso
de confirmação de 420 ms. O texto de cada estado e as métricas continuam imediatos.

No movimento reduzido, partículas, arcos do portal, faixa de luz, ondas e pulsos
são removidos. A seleção, a cor de hover, o foco, os checks e a resposta pressionada
continuam disponíveis, sem interpolação. A instalação mantém a indicação textual.

As imagens da paisagem e do logo são reduzidas com interpolação de alta qualidade
uma vez por tamanho/escala e guardadas em bitmaps com alpha pré-multiplicado. Os
quadros seguintes reutilizam esses bitmaps. Mudanças de tamanho ou DPI recriam o
cache na resolução exibida. Quadros de animação não repetem a sincronização de
visibilidade, rótulos e estado dos controles. Um relógio monotônico de alta
precisão calcula o avanço, sem a quantização do `GetTickCount64`.
O buffer é apresentado com origem e destino explícitos em pixels, evitando que
o GDI+ aplique outra escala por DPI. O host de testes usa a mesma consciência de
DPI por monitor da aplicação. Uma regressão compara pixels da apresentação com
o buffer mesmo quando a resolução física declarada do bitmap muda.

O cache da transição é liberado ao terminar, redimensionar ou reduzir movimento.
Interrupções começam a partir do último quadro renderizado. A apresentação não
altera as decisões de instalação, cancelamento, login ou lançamento do Minecraft.

## Verificação

Release compilada com `/W4`. As seis suítes CTest passaram, incluindo AutoUpdate.
`WindowPaintingTests` verifica seleção invertida durante movimento, interrupção
da troca de cena, descarte do cache, cancelamento disponível durante transição,
reinício da barra por transferência e suspensão do timer em janela oculta e
minimizada. Também verifica feedback imediato no modo de movimento reduzido.
A preferência é injetada no teste; a configuração do Windows não é alterada.

Prévia reproduzível, sem consultar serviços externos:

```powershell
./out/build/installation/WindowPaintingTests.exe --capture-motion .impeccable/review/motion
./out/build/installation/EternalLauncher.exe --render-preview .impeccable/review/motion/states
```

As imagens usam o renderizador de produção com estados simulados, em 1180 × 730
e 944 × 584. Configurações é capturada com `WM_PRINTCLIENT` no pai e `WM_PRINT`
em cada filho, com origem e recorte explícitos, incluindo os EDITs e
botões nativos em movimento. As outras sequências usam o buffer e o nickname
Steve desenhado sinteticamente, pois o EDIT nativo fica fora dele.
Elas comprovam composição e fases
do movimento, sem comprovar apresentação física no monitor, interação física
do mouse, leitores de tela ou mudanças de DPI entre monitores. Minecraft e
Forge reais não fazem parte desta validação. Nenhuma atualização foi publicada.

## Medição de fluidez

`WindowPaintingTests.exe --benchmark-motion` mede separadamente o desenho do
buffer e os intervalos entre `WM_PAINT` em uma janela nativa visível com os mesmos
controles, DPI por monitor e `WS_EX_COMPOSITED` da aplicação. O host usa estados
simulados, sem rede: acompanha atividade do portal/indicador por 1,4 s e, depois,
trocas contínuas de configurações por outros 1,4 s em cada tamanho. Os desenhos são
medidos em 40 amostras após 8 quadros de aquecimento. Não há asserções de tempo
absoluto na suíte: a medição depende da máquina e da carga do sistema.

Resultados locais de 2026-10-04, medianas antes/depois:

| Medida | Antes | Depois |
|---|---:|---:|
| Desenho da tela inicial, 1180 × 730 | 64,32 ms | 10,86 ms |
| Desenho do portal, 1180 × 730 | 67,82 ms | 10,76 ms |
| Desenho da instalação, 1180 × 730 | 33,54 ms | 7,15 ms |
| Intervalo entre pinturas, 1180 × 730 | 72,63 ms | 15,40 ms |
| Desenho da tela inicial, 944 × 584 | 63,80 ms | 9,63 ms |
| Intervalo entre pinturas, 944 × 584 | 68,48 ms | 15,20 ms |

O teste final recebeu 88/90 pinturas em 1,4 s nos tamanhos normal/compacto,
compatível com a meta de aproximadamente 60 quadros por segundo. O percentil 95
dos intervalos ficou em 19,04/21,16 ms. Isso mede a execução da janela de teste,
sem garantir a cadência física do compositor em todos os computadores. A primeira
construção do cache e o redimensionamento não entram nas medianas de aquecimento.

O GIF de demonstração usa amostras de 20 ms (50 FPS durante o movimento), com
pausas nas telas completas. Ele não é uma gravação do benchmark. A versão anterior
usava poucas amostras para seleção e troca de painel, o que também tornava a
prévia pouco fluida.

O timer não é uma garantia de FPS: [a documentação de WM_TIMER](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-timer)
explica a prioridade baixa da mensagem. A correção reduz o trabalho por quadro
e mantém as preferências do sistema; não altera globalmente a resolução dos timers.

### Ampliação dos efeitos

A medição com os novos efeitos registrou 88/85 pinturas em 1,4 s, normal/compacto.
Intervalos medianos: 15,45/16,36 ms; percentil 95: 18,91/21,06 ms. Desenho do portal
com energia: 9,07/12,00 ms. A carga do sistema varia entre medições; a evidência
continua limitada ao host de teste. Release compilada sem avisos, com as seis
suítes aprovadas em 47,76 s.

Os novos testes exercitam hover da paisagem, saída do mouse, compressão e onda em
um BUTTON nativo, cancelamento do gesto sem jogar, confirmação de etapa e remoção
dos efeitos no movimento reduzido. A prévia atual tem 167 quadros determinísticos,
com 20 ms durante movimento; inclui abertura, interação do portal e dos botões,
seleção, transição e conclusão de etapa. Estados e eventos são simulados; não há
instalação real nem gravação do monitor.

### Navegação, botão e atmosfera

Na entrega atual, o portal registrou 89/90 pinturas em 1,4 s nos tamanhos
normal/compacto: intervalos medianos de 15,39/15,01 ms, p95 de 19,62/21,20 ms.
O desenho com partículas ficou em 8,53/7,36 ms de mediana. A navegação com os
HWNDs em movimento registrou 70/76 pinturas em 1,4 s: medianas de 19,08/17,00 ms
e p95 de 26,85/31,13 ms. A troca de página tem mais trabalho nativo; não se promete
60 FPS constantes. O desenho de configurações ficou em 10,54/9,40 ms de mediana.

Release `/W4` sem avisos, seis suítes aprovadas em 47,87 s. As regressões novas
verificam entrada/retorno nos dois tamanhos, Tab durante movimento, preservação
de foco, fonte e texto editado, reabertura repetida, hit targets ao terminar,
troca imediata no movimento reduzido e apresentação independente do DPI do bitmap.

O host da prévia força a pintura nativa do cliente e de cada filho após
atualizar todos os HWNDs. `PrintWindow` devolvia um quadro antigo após reduzir
a janela neste ambiente. A captura verifica por amostras de pixels que o cabeçalho
é o quadro atual na escala correta. Isso não altera a execução do launcher.

### Otimização da entrada em configurações

O formulário passa a ser rasterizado na posição final uma vez durante a entrada.
Os quadros de movimento copiam o cabeçalho fixo e deslocam o corpo por pixels
inteiros, reutilizando textos, linhas e controles desenhados. EDITs, BUTTONs e
TRACKBAR continuam nativos e acompanham o movimento. Edição, foco, hover, mensagens
e mudanças de estado invalidam o cache imediatamente. A imagem temporária é
liberada ao terminar, sair, redimensionar, ocultar/minimizar ou reduzir movimento.
O visual, as durações e as curvas da animação são preservados.

Medição local antes/depois, em 2026-10-04, com o mesmo benchmark:

| Medida | Antes | Depois |
|---|---:|---:|
| Desenho de configurações, normal | 9,45 ms | 2,65 ms |
| Desenho de configurações, compacto | 9,61 ms | 2,38 ms |
| Intervalo de navegação, normal, mediana | 19,05 ms | 16,31 ms |
| Intervalo de navegação, compacto, mediana | 17,78 ms | 15,02 ms |
| Pinturas da navegação em 1,4 s, normal/compacto | 69/76 | 80/83 |

O p95 da navegação foi 31,09/24,09 ms após a alteração (24,18/35,83 ms antes).
As aberturas e retornos ainda incluem preparação inicial e desenho da paisagem;
os intervalos são do host nativo, sem garantir FPS físicos ou eliminar todos os
picos. A mediana do formulário considera oito quadros de aquecimento.

Release `/W4` sem avisos; seis suítes aprovadas em 47,89 s. Os novos testes
verificam reutilização, invalidação por edição/foco, equivalência com o formulário
sem cache e liberação ao sair/terminar. As capturas normal/compacta foram revistas;
o host corrige também o deslocamento da moldura que `WM_PRINT/PRF_CHILDREN`
aplicava aos filhos, pintando cada controle na origem correta do cliente.
