/* Ikony zakładek w pixel art — jedno źródło dla makiety i galerii komponentów.
 *
 * Każda ikona to siatka 24 × 24 zapisana wprost w tekście: jeden znak to jeden piksel,
 * kropka znaczy przezroczysty, pozostałe znaki odsyłają do palety niżej. Poprawia się je
 * w edytorze tekstu. Rysowane są w skali 2 ×, więc każdy piksel pada na całe piksele ekranu.
 *
 * W firmware ta sama siatka staje się obrazem indeksowanym, skalowanym dwukrotnie metodą
 * najbliższego sąsiada — bez antyaliasingu, bo w pixel arcie nie ma czego wygładzać.
 */
(() => {
  const PALETTE = {
    'o': '#0e1118',
    'd': '#232a3a',
    'b': '#3c4660',
    'h': '#5d6a8c',
    'w': '#c6cde0',
    'S': '#52b6dd',
    's': '#20607f',
    'g': '#e9f8ff',
    'a': '#f2a93b',
    'r': '#ff5f5f',
    'n': '#56d6a0',
  };

  const PIXEL = {
    tv: [
      '......o..........o......',
      '.......o........o.......',
      '........o......o........',
      '.........o....o.........',
      '..........o..o..........',
      '.oooooooooooooooooooooo.',
      '.ohhhhhhhhhhhhhhhhhhhho.',
      '.oboooooooooooooobbbbbo.',
      '.oboSSSSSSSSSSSsobddddo.',
      '.oboSggSSSSSSgssobdaado.',
      '.oboSSggSSSSSsssobdaado.',
      '.oboSSSggSSSssssobddddo.',
      '.oboSSSSggSsssssobdhhdo.',
      '.oboSSSSSsssssssobdhhdo.',
      '.oboSSSSssssssssobddddo.',
      '.oboSSSsssssssssobddddo.',
      '.oboSSssssssssssobdrddo.',
      '.obossssssssssssobddddo.',
      '.oboooooooooooooobbbbbo.',
      '.oddddddddddddddddddddo.',
      '.oooooooooooooooooooooo.',
      '........dddddddd........',
      '......oooooooooooo......',
      '........................',
    ],
    appletv: [
      '........................',
      '........................',
      '........................',
      '........................',
      '........................',
      '........................',
      '........................',
      '........................',
      '..oooooooooooooooooooo..',
      '..ohhhhhhhhhhhhhhhhhho..',
      '..obbbbbbbgbbbbbbbbbbo..',
      '..obbdddbbggbbbbbbbbbo..',
      '..obbdddbbgggbbbbbbbbo..',
      '..obbdddbbggbbbbbbnbbo..',
      '..obbbbbbbgbbbbbbbbbbo..',
      '..oddddddddddddddddddo..',
      '..oooooooooooooooooooo..',
      '....oooooooooooooooo....',
      '........................',
      '........................',
      '........................',
      '........................',
      '........................',
      '........................',
    ],
    audio: [
      '........................',
      '.....oooooooooooooo.....',
      '.....ohhhhhhhhhhhho.....',
      '.....ohbbbbbbbbbbbo.....',
      '.....ohbbbohhobbbbo.....',
      '.....ohbbohhhhobbbo.....',
      '.....ohbbohwwhobbbo.....',
      '.....ohbbohhhhobbbo.....',
      '.....ohbbbohhobbbbo.....',
      '.....ohbbbbbbbbbbbo.....',
      '.....ohbbbbbbbbbbbo.....',
      '.....ohbboohhoobbbo.....',
      '.....ohbohhddhhobbo.....',
      '.....ohohhddddhhobo.....',
      '.....ohohdhhhhdhobo.....',
      '.....ohohdhwwhdhobo.....',
      '.....ohohdhhhhdhobo.....',
      '.....ohohhddddhhobo.....',
      '.....ohbohhddhhobbo.....',
      '.....ohbboohhoobbbo.....',
      '.....ohbbbbbbbbbbao.....',
      '.....oddddddddddddo.....',
      '.....oooooooooooooo.....',
      '........................',
    ],
    home: [
      '........................',
      '........................',
      '..........oooo..........',
      '.........oddddo.........',
      '........oddddddo........',
      '.......oddddddddo.......',
      '......oddddddddddo......',
      '.....oddddddddddddo.....',
      '....oddddddddddddddo....',
      '....oooooooooooooooo....',
      '....ohhhhhhhhhhhhhho....',
      '....obbbbbbbbbbbbbbo....',
      '....obooooobbooooobo....',
      '....oboaaaobboaaaobo....',
      '....oboaaaobboaaaobo....',
      '....oboaaaobboaaaobo....',
      '....obooooobbooooobo....',
      '....obbbboooooobbbbo....',
      '....obbbboddddobbbbo....',
      '....obbbbodddaobbbbo....',
      '....obbbboddddobbbbo....',
      '....oooooooooooooooo....',
      '........................',
      '........................',
    ],
    settings: [
      '........................',
      '........................',
      '........................',
      '...ooooo..ooooo..ooooo..',
      '...owwwo..ohhho..owwwo..',
      '...oaaao..obbbo..oaaao..',
      '...oaaao..obbbo..oaaao..',
      '...ooooo..ooooo..ooooo..',
      '........................',
      '........................',
      '...ooooo..ooooo..ooooo..',
      '...ohhho..owwwo..ohhho..',
      '...obbbo..oaaao..obbbo..',
      '...obbbo..oaaao..obbbo..',
      '...ooooo..ooooo..ooooo..',
      '........................',
      '........................',
      '...ooooo..ooooo..ooooo..',
      '...owwwo..ohhho..owwwo..',
      '...oaaao..obbbo..oaaao..',
      '...oaaao..obbbo..oaaao..',
      '...ooooo..ooooo..ooooo..',
      '........................',
      '........................',
    ],
  };

  const pixelSvg = rows => {
    const rects = [];
    rows.forEach((row, y) => {
      let x = 0;
      while (x < row.length) {
        const ch = row[x];
        if (!PALETTE[ch]) { x += 1; continue; }
        // Sąsiadujące piksele tego samego koloru łączymy w jeden prostokąt.
        let width = 1;
        while (row[x + width] === ch) width += 1;
        rects.push(`<rect x="${x}" y="${y}" width="${width}" height="1" fill="${PALETTE[ch]}"/>`);
        x += width;
      }
    });
    return `<svg viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg" aria-hidden="true">${rects.join('')}</svg>`;
  };

  // Ikony 48 × 48 z tools/draw_icons.py (assets/pixel/*.png) mają pierwszeństwo przed
  // siatkami 24 × 24 niżej, które zostają jako zapas dla galerii komponentów.
  const DETAILED = ['tv', 'appletv', 'audio', 'home', 'settings'];
  document.querySelectorAll('[data-pix]').forEach(slot => {
    const name = slot.dataset.pix;
    if (DETAILED.includes(name)) {
      slot.innerHTML = `<img src="assets/pixel/${name}.png" alt="" aria-hidden="true">`;
      return;
    }
    const rows = PIXEL[name];
    if (rows) slot.innerHTML = pixelSvg(rows);
  });

  globalThis.PULPIT_PIXEL = { PIXEL, PALETTE, pixelSvg };
})();
