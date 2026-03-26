function readFloat32Values(sharedBufferContract) {
  const bytes = sharedBufferContract.bytes;
  if (!(bytes instanceof Uint8Array)) {
    throw new Error(`expected Uint8Array bytes, got ${typeof bytes}`);
  }
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const values = [];
  for (let offset = 0; offset < bytes.byteLength; offset += 4) {
    values.push(view.getFloat32(offset, true));
  }
  return values;
}

export function run(fabricInputs) {
  const report = fabricInputs.kain_orchestrator.report;
  const gpuOutput = fabricInputs.gpu_enrich.dst;
  const values = readFloat32Values(gpuOutput);
  return `${report}|gpu=${values.join(",")}|bytes=${gpuOutput.byte_length}`;
}
