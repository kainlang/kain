import { render } from "preact";

import { StudioApp } from "./studio_app";

declare global {
  interface Window {
    __KAIN_CANVAS_FORGE_MODEL__?: unknown;
  }
}

const mountTarget = document.getElementById("app-root");

if (!mountTarget) {
  throw new Error("Missing app root for Kain Canvas Forge.");
}

render(<StudioApp rawModel={window.__KAIN_CANVAS_FORGE_MODEL__} />, mountTarget);
