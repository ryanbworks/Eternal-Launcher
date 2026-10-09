# Acompanhamento da instalação

O fluxo de Jogar usa um único worker para obter o manifesto privado, preparar
Java e jogo e validar a instância. Só a janela inicia o Minecraft após receber
um resultado bem-sucedido da tentativa atual. Um pedido de cancelamento sempre
vence uma conclusão simultânea e impede o início automático.

## Contratos e estados

`InstallationProgress` identifica etapa, atividade e estado, percentual opcional,
bytes transferidos, total opcional, contadores de arquivos e identificador de
transferência. Java e jogo usam o mesmo callback. Resultados distinguem sucesso,
cancelamento e falha; categorias incluem rede, HTTP, disco, integridade, processo
e configuração. O coordenador apresenta orientação e conserva detalhes no log.

`InstallationMailbox` conserva a última amostra, o estado das seis etapas e o
resultado terminal. Mensagens Win32 apenas sinalizam que há dados; não carregam
ponteiros com posse de memória. Cada tentativa possui um identificador crescente.
Etapas e resultados são publicados imediatamente; amostras de transferência são
limitadas a 200 ms e um timer de 200 ms recolhe a amostra pendente.

O downloader verifica o cache antes de construir a fila. Arquivos completos
válidos não entram no total a transferir nem nos bytes usados para velocidade.
Workers paralelos serializam as atualizações. O contador de arquivos concluídos
só avança após hash/tamanho e publicação atômica; 100% exige a fila verificada.

O medidor usa `steady_clock`, uma janela móvel de cinco segundos e amostras da
transferência atual. Estimativa aparece após dois segundos, com total conhecido
e velocidade positiva. Dois segundos sem tráfego removem velocidade e estimativa.
Verificação, extração e execução do Forge não recebem estimativas artificiais.

## Cancelamento e recuperação

WinHTTP opera assincronamente dentro do worker, preservando buffers e contexto
até `HANDLE_CLOSING`. Metadados e arquivos recebem `stop_token`; leitura usa
disponibilidade de dados para entregar blocos pequenos prontamente. Hashes
verificam o sinal entre blocos. Tar e Forge compartilham uma espera que encerra
o subprocesso ao cancelar. A ativação do Java e a publicação de cada arquivo
mantêm seus limites atômicos; arquivos completos permanecem disponíveis.

Parciais são removidos. Continuar/tentar novamente consulta os manifestos e
reconfere o cache. Não há checkpoint persistente, pausa ou retomada por bytes.
A limpeza de mods verifica cancelamento antes de cada remoção. Ao fechar durante
a preparação, a janela continua processando mensagens até o resultado terminal,
aguarda os workers e libera os payloads antigos antes da destruição do HWND.

O log UTF-8 da tentativa fica em `logs/installation.log`; detalhes do instalador
Forge permanecem em `logs/forge-installer.log`. O fluxo não registra a identidade
ou tokens Microsoft e remove URLs/indicadores de credenciais dos detalhes.

## Verificação

Build de referência: Release MSVC `/W4` em `out/build/installation`.
As seis suítes CTest cobrem o AutoUpdate e regressões existentes, métricas com
relógio controlado, coalescência e tentativas antigas, servidor HTTP local,
cache e downloads paralelos, tamanhos desconhecidos, HTTP 503, interrupção,
cancelamento em metadados/corpo, integridade, disco e subprocesso.

O teste do coordenador usa serviços isolados para primeira preparação, instância
existente, Java personalizado validado, cancelamento nas etapas de nativas,
Forge e validação e preservação de mundo. O host Win32 oculto verifica os botões,
cliques repetidos, resultado atrasado, sucesso versus cancelamento, início único
e fechamento sem bloquear a janela.

`WindowPaintingTests --capture-installation <pasta>` abre um host de teste nativo
visível com controles reais, confirma Tab de Abrir log para Cancelar instalação
e Enter para cancelar e captura o cliente via PrintWindow em 1180 × 730 e
944 × 584. Usa dados sintéticos; não chama rede, login ou Minecraft real.

`EternalLauncher --render-preview <pasta>` produz os estados de verificação,
download, Forge, erro, cancelado, cancelando e compacto com o renderer de
produção. As capturas estão em `.impeccable/review/installation`.

Não houve instalação completa por serviços públicos, execução do Forge real ou
início de Minecraft real nesta validação. A espera de subprocessos foi testada
com um executável isolado. DPI entre monitores, leitor de tela e duração de uma
instalação real não foram medidos. Nenhuma release foi publicada.
