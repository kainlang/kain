// KainScript module (ES2022 + JSDoc) used by the universal web template.
// This file is optional but proves the web pack can mix KainScript with TSX islands.

/**
 * @returns {string}
 */
export function kainScriptTagline() {
  return "KainScript lane active";
}

/**
 * @param {string} value
 * @returns {string}
 */
export function normalizePrompt(value) {
  return String(value || "")
    .trim()
    .replace(/\s+/g, " ")
    .slice(0, 240);
}

/**
 * @param {string} value
 * @returns {string}
 */
export function normalizeSelectionLabel(value) {
  return String(value || "")
    .trim()
    .replace(/\s+/g, " ")
    .replace(/[^a-zA-Z0-9\s\-_.]/g, "")
    .slice(0, 80);
}
