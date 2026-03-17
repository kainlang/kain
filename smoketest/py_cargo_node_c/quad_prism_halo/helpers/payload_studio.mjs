function ppmText(width, height, bytes) {
  let out = `P3\n${width} ${height}\n255\n`;
  for (let index = 0; index < bytes.length; index += 4) {
    out += `${bytes[index]} ${bytes[index + 1]} ${bytes[index + 2]}\n`;
  }
  return out;
}

function canvasScript(id, width, height, bytes) {
  return `
    (() => {
      const width = ${width};
      const height = ${height};
      const bytes = [${bytes.join(',')}];
      const canvas = document.getElementById('${id}');
      const ctx = canvas.getContext('2d');
      const image = ctx.createImageData(width, height);
      for (let src = 0, dst = 0; src < bytes.length; src += 4, dst += 4) {
        image.data[dst + 0] = bytes[src + 0];
        image.data[dst + 1] = bytes[src + 1];
        image.data[dst + 2] = bytes[src + 2];
        image.data[dst + 3] = 255;
      }
      ctx.putImageData(image, 0, 0);
    })();
  `;
}

export function makePpmPayload(name, width, height, bytes, signature) {
  const text = ppmText(width, height, bytes);
  return {
    kind: 'image',
    mime_type: 'image/x-portable-pixmap',
    extension: 'ppm',
    width,
    height,
    channels: 4,
    layout: 'HWC',
    pixel_format: 'rgba8',
    representation: 'encoded',
    text,
    bytes: new TextEncoder().encode(text),
    name,
    signature,
  };
}

export function makeCompareDocument(title, width, height, baseBytes, finalBytes, signature, rustBase, rustKain, rustFinal, cChecksum, cSignature, legend) {
  const script = `
    ${canvasScript('base-view', width, height, baseBytes)}
    ${canvasScript('final-view', width, height, finalBytes)}
  `;
  const text = `<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>${title}</title>
  <style>
    :root { color-scheme: dark; }
    * { box-sizing: border-box; }
    body { margin: 0; min-height: 100vh; background: radial-gradient(circle at top, #19243c 0%, #060913 72%); color: #edf6ff; font-family: Consolas, "Courier New", monospace; }
    main { width: min(94vw, 1320px); margin: 32px auto; padding: 22px; border-radius: 28px; border: 1px solid rgba(126, 242, 255, 0.38); background: rgba(7, 10, 20, 0.82); box-shadow: 0 36px 120px rgba(0, 0, 0, 0.56); }
    h1 { margin: 0 0 10px; font-size: 16px; text-transform: uppercase; letter-spacing: 0.28em; color: #7ef2ff; }
    .meta { margin: 0 0 18px; color: #b2dfff; font-size: 13px; }
    .compare { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 18px; }
    .panel { padding: 14px; border-radius: 20px; background: rgba(255, 255, 255, 0.04); border: 1px solid rgba(255,255,255,0.08); }
    .eyebrow { margin: 0 0 10px; font-size: 11px; letter-spacing: 0.22em; text-transform: uppercase; color: #ffd166; }
    canvas { width: 100%; height: auto; display: block; border-radius: 16px; image-rendering: pixelated; background: #03050b; border: 1px solid rgba(255,255,255,0.08); }
    .stats { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 12px; margin-top: 18px; }
    .stat { padding: 12px; border-radius: 18px; background: rgba(255,255,255,0.035); }
    .legend { margin-top: 18px; padding: 16px; border-radius: 18px; background: rgba(255,255,255,0.04); line-height: 1.6; }
  </style>
</head>
<body>
  <main>
    <h1>${title}</h1>
    <p class="meta">${signature} / ${cSignature}</p>
    <div class="compare">
      <section class="panel">
        <p class="eyebrow">Python Base</p>
        <canvas id="base-view" width="${width}" height="${height}"></canvas>
      </section>
      <section class="panel">
        <p class="eyebrow">Kain + Cargo + C Final</p>
        <canvas id="final-view" width="${width}" height="${height}"></canvas>
      </section>
    </div>
    <div class="stats">
      <div class="stat">Rust base checksum: ${rustBase}</div>
      <div class="stat">Rust after Kain overlay: ${rustKain}</div>
      <div class="stat">Rust final checksum: ${rustFinal}</div>
      <div class="stat">C checksum: ${cChecksum}</div>
    </div>
    <div class="legend">${legend}</div>
  </main>
  <script>${script}</script>
</body>
</html>`;
  return {
    kind: 'document',
    title,
    mime_type: 'text/html',
    extension: 'html',
    text,
  };
}
