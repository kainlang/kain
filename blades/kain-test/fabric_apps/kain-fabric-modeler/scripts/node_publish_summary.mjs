export function run(fabricInputs) {
  const settings = fabricInputs.python_project_seed.project_settings;
  const sceneReport = fabricInputs.model_seed.scene_report;
  const topologyReport = fabricInputs.topology_analyzer.topology_report;
  const signature = fabricInputs.native_brush.signature;
  const preview = fabricInputs.gpu_preview.preview_dst;
  const brush = fabricInputs.native_brush.brush_snapshot;

  return [
    "<article data-kain='fabric-modeler'>",
    `<h1>${settings.project_name}</h1>`,
    `<p>${sceneReport}</p>`,
    `<p>${topologyReport}</p>`,
    `<p>native=${signature}</p>`,
    `<p>gpu-preview-bytes=${preview.byte_length}</p>`,
    `<p>brush-snapshot-bytes=${brush.byte_length}</p>`,
    "</article>",
  ].join("");
}
