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
  const values = readFloat32Values(fabricInputs.gpu_copy.dst);
  return `${fabricInputs.gpu_seed.report}|gpu=${values.join(",")}`;
}
