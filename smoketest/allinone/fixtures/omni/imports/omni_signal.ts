export interface OmniSignalSample {
  tick: number;
  energy: number;
}

export const OMNI_SIGNAL_HEADER = "omni-import-ts";

export function makeOmniSignal(count: number): OmniSignalSample[] {
  const samples: OmniSignalSample[] = [];
  for (let index = 0; index < count; index += 1) {
    samples.push({
      tick: index,
      energy: Number((0.2 + index * 0.1).toFixed(3))
    });
  }
  return samples;
}
