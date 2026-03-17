function ppmText(width, height, bytes) {
  let out = `P3\n${width} ${height}\n255\n`;
  for (let index = 0; index < bytes.length; index += 3) {
    out += `${bytes[index]} ${bytes[index + 1]} ${bytes[index + 2]}\n`;
  }
  return out;
}

export function makePpmPayload(name, width, height, bytes, signature) {
  const text = ppmText(width, height, bytes);
  return {
    kind: 'image',
    mime_type: 'image/x-portable-pixmap',
    extension: 'ppm',
    width,
    height,
    channels: 3,
    layout: 'HWC',
    pixel_format: 'rgb8',
    representation: 'encoded',
    text,
    bytes: new TextEncoder().encode(text),
    name,
    signature,
  };
}

export function makeViewerDocument(title, width, height, bytes, signature, checksum, legend) {
  const script = `
    const width = ${width};
    const height = ${height};
    const bytes = [${bytes.join(',')}];
    const canvas = document.getElementById('view');
    const ctx = canvas.getContext('2d');
    const image = ctx.createImageData(width, height);
    for (let src = 0, dst = 0; src < bytes.length; src += 3, dst += 4) {
      image.data[dst + 0] = bytes[src + 0];
      image.data[dst + 1] = bytes[src + 1];
      image.data[dst + 2] = bytes[src + 2];
      image.data[dst + 3] = 255;
    }
    ctx.putImageData(image, 0, 0);
  `;
  const text = `<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>${title}</title>
  <style>
    :root { color-scheme: dark; }
    body { margin: 0; min-height: 100vh; display: grid; place-items: center; background: radial-gradient(circle at top, #14213d 0%, #040712 72%); color: #ecf7ff; font-family: Consolas, "Courier New", monospace; }
    main { width: min(94vw, 1180px); padding: 22px; border-radius: 28px; border: 1px solid rgba(115, 201, 255, 0.45); background: rgba(6, 10, 18, 0.84); box-shadow: 0 32px 120px rgba(0, 0, 0, 0.58); }
    h1 { margin: 0 0 8px; font-size: 16px; letter-spacing: 0.28em; text-transform: uppercase; color: #73c9ff; }
    .meta { margin: 0 0 16px; color: #a7d7ff; font-size: 13px; }
    canvas { width: 100%; height: auto; image-rendering: pixelated; border-radius: 18px; border: 1px solid rgba(255,255,255,0.1); background: #02040a; }
    .legend { margin-top: 16px; padding: 14px; border-radius: 18px; background: rgba(255,255,255,0.04); line-height: 1.6; }
  </style>
</head>
<body>
  <main>
    <h1>${title}</h1>
    <p class="meta">${signature} / checksum ${checksum}</p>
    <canvas id="view" width="${width}" height="${height}"></canvas>
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
