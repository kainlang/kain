export function run(fabricInputs) {
  const settings = fabricInputs.python_suite_bootstrap.project_settings;
  const topologyReport = fabricInputs.rig_graph_analysis.topology_report;
  const trainReport = fabricInputs.tensor_train_stage.tensor_training_report;
  const inferReport = fabricInputs.tensor_infer_stage.tensor_inference_report;
  const sculptSignature = fabricInputs.native_sculpt_kernel.sculpt_signature;
  const sculptReport = fabricInputs.native_sculpt_kernel.sculpt_report;
  const materialAuthoring = fabricInputs.material_authoring_projection.material_authoring_report;
  const svgMask = fabricInputs.svg_material_mask_projection.svg_mask_report;
  const materialExport = fabricInputs.material_texture_export_projection.material_texture_export_report;
  const sculptDelta = fabricInputs.gpu_sculpt_displacement.sculpt_delta;
  const previewBuffer = fabricInputs.gpu_material_preview.preview_dst;

  return [
    "<article data-kain='fabric-dcc-suite'>",
    `<h1>${settings.project_name}</h1>`,
    `<p>workspace=${settings.workspace_mode}</p>`,
    `<p>${topologyReport}</p>`,
    `<p>${sculptSignature}</p>`,
    `<p>${sculptReport}</p>`,
    `<p>${materialAuthoring}</p>`,
    `<p>${svgMask}</p>`,
    `<p>${materialExport}</p>`,
    `<p>${trainReport.summary ?? trainReport}</p>`,
    `<p>${inferReport.summary ?? inferReport}</p>`,
    `<p>gpu-sculpt-bytes=${sculptDelta.byte_length}</p>`,
    `<p>gpu-preview-bytes=${previewBuffer.byte_length}</p>`,
    "</article>",
  ].join("");
}
