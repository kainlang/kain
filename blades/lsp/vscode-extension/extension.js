// Kain Language Server — VS Code extension client
// Spawns the bundled Kain interpreter running the LSP blade.
"use strict";

const path = require("path");
const { LanguageClient, TransportKind } = require("vscode-languageclient/node");

/** @type {LanguageClient | null} */
let client = null;

/**
 * Activate: spawn the Kain LSP server via kain.exe run.
 * @param {import("vscode").ExtensionContext} context
 */
function activate(context) {
  // Paths relative to the extension root
  const kainExe = context.asAbsolutePath(path.join("server", "kain.exe"));
  const bladeDir = context.asAbsolutePath(".");
  const lspEntry = "src/main.kn";

  const serverOptions = {
    command: kainExe,
    args: ["run", lspEntry],
    options: {
      cwd: bladeDir,
      env: {
        ...process.env,
        // Ensure Kain can find its runtime manifest
        KAIN_RUNTIME_MANIFEST_PATH: context.asAbsolutePath(
          path.join("server", "native_core_runtime.toml")
        ),
      },
    },
    // TransportKind.pipe = stdio without auto-appending --stdio flag.
    // kain.exe doesn't accept --stdio; the LSP just reads/writes stdin/stdout.
    transport: TransportKind.pipe,
  };

  const clientOptions = {
    documentSelector: [
      { scheme: "file", language: "kain" },
      { scheme: "untitled", language: "kain" },
    ],
    outputChannelName: "Kain Language Server",
    traceOutputChannelName: "Kain Language Server (Trace)",
  };

  client = new LanguageClient(
    "kain-lsp",
    "Kain Language Server",
    serverOptions,
    clientOptions
  );

  client.start();
}

/**
 * Deactivate: stop the language server.
 * @returns {Thenable<void> | undefined}
 */
function deactivate() {
  if (client) {
    return client.stop();
  }
  return undefined;
}

module.exports = { activate, deactivate };
