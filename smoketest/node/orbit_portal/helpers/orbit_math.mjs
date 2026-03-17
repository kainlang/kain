export function buildOrbitField(width, height, phase, count) {
  const points = [];
  for (let i = 0; i < count; i += 1) {
    const t = i / Math.max(count, 1);
    const angle = phase * 0.031 + t * Math.PI * 9.0;
    const radius = 42 + t * Math.min(width, height) * 0.36;
    const wobble = Math.sin(phase * 0.07 + i * 0.8) * 18;
    const cx = Math.round(width * 0.5 + Math.cos(angle) * (radius + wobble));
    const cy = Math.round(height * 0.5 + Math.sin(angle * 1.18) * (radius * 0.62));
    const size = 2 + ((i * 7 + phase) % 5);
    const alpha = 0.22 + (((i * 13 + phase) % 55) / 100);
    points.push([cx, cy, size, alpha]);
  }

  return {
    width,
    height,
    background: "radial-gradient(circle at 50% 50%, #22163a 0%, #0a0916 52%, #020204 100%)",
    accent: "#7ef2ff",
    halo: "#ff9cda",
    points,
  };
}

export function htmlShell(title, svg, background, accent) {
  return `<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>${title}</title>
  <style>
    :root { color-scheme: dark; }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      min-height: 100vh;
      display: grid;
      place-items: center;
      background: ${background};
      font-family: Consolas, "Courier New", monospace;
      color: #eef6ff;
    }
    .frame {
      width: min(92vw, 1100px);
      border: 1px solid color-mix(in srgb, ${accent} 40%, transparent);
      background: rgba(7, 8, 18, 0.7);
      backdrop-filter: blur(16px);
      box-shadow: 0 28px 120px rgba(0, 0, 0, 0.55);
      padding: 18px;
      border-radius: 24px;
    }
    .title {
      margin: 0 0 12px;
      letter-spacing: 0.28em;
      font-size: 13px;
      text-transform: uppercase;
      color: ${accent};
    }
    svg {
      width: 100%;
      height: auto;
      display: block;
      border-radius: 18px;
    }
  </style>
</head>
<body>
  <main class="frame">
    <p class="title">${title}</p>
    ${svg}
  </main>
</body>
</html>`;
}
