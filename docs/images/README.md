# Capturas da interface

`interface-principal.png` é uma renderização determinística da interface final em 1180 × 730, com estado ilustrativo de instalação pronta e nickname Steve. Os dados desta imagem são exemplos; a janela real consulta os serviços existentes.

Para gerar as prévias sem iniciar downloads, autenticar ou abrir Minecraft:

```powershell
./out/build/x64-debug/EternalLauncher.exe --render-preview out/ui-preview
```

O comando usa o mesmo renderizador GDI+ da janela e produz 28 capturas: estados da tela principal, identidade, instalação e AutoUpdate, além de quatro configurações. As sete novas capturas de instalação incluem consulta, download, Forge, erro, cancelado, cancelando e tamanho compacto. A imagem do campo nativo nas prévias é desenhada; o comportamento de edição, foco, teclado e DPI precisa de verificação na janela real.

As prévias não representam resultados reais do servidor e não substituem testes de instalação/login.

`configuracoes.png` é a prévia de 1180 × 730 com os valores padrão, produzida pelo mesmo renderizador. Ela não mostra uma sessão de jogo nem preferências salvas do usuário.

`instalacao.png` é a prévia de download em 1180 × 730 com valores ilustrativos.
`instalacao.origin.txt` registra a origem. As capturas adicionais de um host nativo
visível estão em `.impeccable/review/installation/installation-live*.png`; exibem
controles reais com dados sintéticos, não uma instalação ou sessão Minecraft real.
