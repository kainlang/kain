export function run(fabricInputs) {
  const report = fabricInputs.kain_orchestrator.report;
  const analysis = fabricInputs.rust_analyzer.analysis;
  const image = fabricInputs.native_filter.filtered_image;
  const snapshot = fabricInputs.native_filter.snapshot;
  return [
    "<article data-fabric='local-first'>",
    `<h1>${analysis}</h1>`,
    `<p>${report}</p>`,
    `<p>image=${image.width}x${image.height} channels=${image.channels}</p>`,
    `<p>snapshot-bytes=${snapshot.byte_length}</p>`,
    "</article>",
  ].join("");
}
