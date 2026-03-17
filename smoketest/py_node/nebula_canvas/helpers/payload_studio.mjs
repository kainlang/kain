const encoder = new TextEncoder();

function htmlShell(title, body, background, accent) {
  return `<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>${title}</title>
  <style>
    :root { color-scheme: dark; }
    * { box-sizing: border-box; }
    body { margin: 0; min-height: 100vh; display: grid; place-items: center; background: ${background}; font-family: Consolas, monospace; color: #eef6ff; }
    main { width: min(92vw, 1120px); border: 1px solid ${accent}; border-radius: 28px; padding: 18px; background: rgba(6, 9, 20, 0.78); box-shadow: 0 28px 120px rgba(0, 0, 0, 0.5); }
    .title { margin: 0 0 12px; letter-spacing: 0.28em; text-transform: uppercase; font-size: 13px; color: ${accent}; }
    svg { display: block; width: 100%; height: auto; border-radius: 18px; }
  </style>
</head>
<body>
  <main>
    <p class="title">${title}</p>
    ${body}
  </main>
</body>
</html>`;
}

export function makeDocumentPayload(title, body, background, accent) {
  return {
    kind: "document",
    title,
    mime_type: "text/html",
    extension: "html",
    text: htmlShell(title, body, background, accent),
  };
}

export function makeSvgImagePayload(name, svg, width, height, accent) {
  return {
    kind: "canvas",
    name,
    mime_type: "image/svg+xml",
    extension: "svg",
    width,
    height,
    accent,
    text: svg,
    bytes: encoder.encode(svg),
  };
}
