import path from "node:path";
import { fileURLToPath } from "node:url";

import { BrowserWindow, app } from "electron";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const appRoot = path.resolve(__dirname, "..");
const indexPath = path.resolve(appRoot, "outputs", "index.html");
const preloadPath = path.resolve(__dirname, "preload.mjs");

function createWindow() {
  const window = new BrowserWindow({
    width: 1680,
    height: 1020,
    minWidth: 1280,
    minHeight: 760,
    backgroundColor: "#121318",
    autoHideMenuBar: true,
    webPreferences: {
      preload: preloadPath,
      contextIsolation: true,
      nodeIntegration: false
    }
  });

  window.loadFile(indexPath);
}

app.whenReady().then(() => {
  createWindow();

  app.on("activate", () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow();
    }
  });
});

app.on("window-all-closed", () => {
  if (process.platform !== "darwin") {
    app.quit();
  }
});

