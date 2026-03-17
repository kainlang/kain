export type SignalPoint = [number, number, number, number];

export function buildSignalPrism(width: number, height: number, phase: number, count: number) {
  const points: SignalPoint[] = [];
  for (let i = 0; i < count; i += 1) {
    const t = i / Math.max(count - 1, 1);
    const wave = Math.sin(phase * 0.09 + t * Math.PI * 10.0);
    const twist = Math.cos(phase * 0.04 + t * Math.PI * 7.0);
    const x = Math.round(width * 0.1 + t * width * 0.8);
    const y = Math.round(height * 0.5 + wave * height * 0.22 + twist * 28);
    const radius = 2 + ((i * 11 + phase) % 5);
    const alpha = 0.24 + (((i * 9 + phase) % 60) / 100);
    points.push([x, y, radius, alpha]);
  }

  return {
    width,
    height,
    points,
    accent: "#ffd166",
    secondary: "#7ae7ff",
    background: "linear-gradient(180deg, #130f1f 0%, #070810 100%)",
  };
}

export function htmlDocument(title: string, body: string, background: string, accent: string) {
  return `<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>${title}</title>
  <style>
    body { margin: 0; min-height: 100vh; display: grid; place-items: center; background: ${background}; font-family: Consolas, monospace; color: white; }
    main { width: min(92vw, 1080px); border: 1px solid ${accent}; border-radius: 24px; padding: 18px; background: rgba(7, 8, 18, 0.78); }
    .title { letter-spacing: 0.24em; text-transform: uppercase; font-size: 13px; color: ${accent}; }
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

export function encodeUtf8(input: string): Uint8Array {
  return new TextEncoder().encode(input);
}
