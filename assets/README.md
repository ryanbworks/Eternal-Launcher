# Identidade visual do Eternal

- `eternal-logo.svg`: monograma vetorial próprio. O E em degraus termina em uma abertura de portal. A mesma geometria é desenhada por `DrawingUtils::drawLogo`.
- `eternal.ico`: ícone do aplicativo em 16, 24, 32, 48, 64, 128 e 256 pixels.
- `eternal-icon.png`: versão da marca para prévia.
- `lavender-cosmic-portal.png`: emblema Aurora refinado, com fundo transparente; é embutido no cabeçalho do launcher como recurso Win32.
- `realm.png`: arte conceitual de um mundo em blocos, produzida pela ferramenta ImageGen integrada. Não é uma captura do servidor. O prompt completo está em `realm.prompt.txt` e nos metadados PNG.
- `fonts/Sora-Bold.ttf`: tipografia dos títulos e da marca, originária de [Sora](https://github.com/sora-xor/sora-font/blob/master/fonts/ttf/v2.1beta/Sora-Bold.ttf). Licença SIL OFL em `fonts/OFL.txt`. Controles e edição continuam usando Segoe UI.

A paisagem, o ícone, a fonte e sua licença são incorporados como recursos Win32: o launcher distribuído não depende de arquivos de imagem ao lado do executável. As fontes são privadas ao processo e não são instaladas no Windows.

Cor de marca: #A582FF. Fundo: #0D0E14. O SVG é a fonte geométrica reutilizável; o desenho nativo usa as mesmas coordenadas.
