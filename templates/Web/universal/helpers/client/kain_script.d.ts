declare module "*.ks" {
  export function kainScriptTagline(): string;
  export function normalizePrompt(value: string): string;
  export function normalizeSelectionLabel(value: string): string;
  export function normalizeToolPayload(value: string): string;
}
