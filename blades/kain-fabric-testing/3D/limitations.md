# Limitations

## Confirmed Gaps

- The GPU kernel reflection snapshot is now descriptor-rooted and projects
  `source_id` values locally, but the upstream Kain GPU emitter still does not
  own that committed surface directly. The template generator remains the
  source of truth for `generated/runtime-reflection/gpu/catalog.json` and
  `generated/runtime-reflection/gpu/descriptors/gpu_reflection_catalog.json`.
- The tensor-pipeline reflection snapshot is template-generated from
  `manifests/tensor_pipelines.json`; upstream Kain does not yet emit a committed
  tensor-pipeline reflection surface, so the template generator stays the source
  of truth for `generated/runtime-reflection/tensor-pipelines/catalog.json` and
  `generated/runtime-reflection/tensor-pipelines/descriptors/tensor_pipeline_catalog.json`.

## Capture Here

- Add upstream language, runtime, compiler, UI, or pipeline limitations here
  whenever template behavior depends on a local workaround.
