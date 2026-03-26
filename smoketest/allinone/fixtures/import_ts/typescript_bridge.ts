export interface SignalPoint {
  x: number;
  y: number;
  intensity: number;
}

export interface SignalEnvelope {
  title: string;
  accent: string;
  points: SignalPoint[];
}

export const DEFAULT_SIGNAL: SignalEnvelope = {
  title: "allinone-ts",
  accent: "#6cf2ff",
  points: [
    { x: 0, y: 8, intensity: 0.25 },
    { x: 12, y: 14, intensity: 0.75 },
    { x: 24, y: 10, intensity: 0.5 }
  ]
};

export function buildSignalSummary(envelope: SignalEnvelope): string {
  const peak = envelope.points.reduce((max, point) => {
    return Math.max(max, point.intensity);
  }, 0);
  return `${envelope.title}:${envelope.points.length}:${peak.toFixed(2)}:${envelope.accent}`;
}
