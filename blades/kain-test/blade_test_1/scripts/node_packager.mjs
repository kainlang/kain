export function run(fabricInputs) {
  const report = fabricInputs.kain_orchestrator.report;
  const signature = fabricInputs.native_filter.signature;
  const analysis = fabricInputs.rust_metrics.analysis;
  const image = fabricInputs.native_filter.filtered_image;
  const snapshot = fabricInputs.native_filter.snapshot;

  return [
    "blade-smoke",
    report,
    signature,
    analysis,
    `image=${image.width}x${image.height}x${image.channels}`,
    `snapshot=${snapshot.byte_length}`,
  ].join("|");
}
