const { app, BrowserWindow, ipcMain } = require('electron');
const path = require('path');
const fs = require('fs');
const { spawn } = require('child_process');

// Suppress EPIPE when Electron has no terminal
process.stdout.on('error', (err) => { if (err.code === 'EPIPE') {} });
process.stderr.on('error', (err) => { if (err.code === 'EPIPE') {} });

let bridgeProcess = null;
let bridgePid = null;  // track which PID is current — ignore stale exits
let mainWindow = null;
let bridgeRunning = false;
let currentSource = null;

// ── Source discovery ──────────────────────────────────────────
// Lists every .exe in the electron dir and demos/ that could be
// spawned as a Kain bridge. The renderer uses this for the
// Jupyter-like source switcher in the top bar.
function discoverSources() {
  const sources = [];
  const searchDirs = [
    path.join(__dirname, 'demos'),
    __dirname,
  ];
  for (const dir of searchDirs) {
    if (!fs.existsSync(dir)) continue;
    let entries;
    try { entries = fs.readdirSync(dir, { withFileTypes: true }); }
    catch (e) { continue; }
    for (const entry of entries) {
      if (!entry.isFile() || !entry.name.endsWith('.exe')) continue;
      const full = path.join(dir, entry.name);
      const name = entry.name.replace(/\.exe$/, '');
      // Check if there's a matching .kn source file
      const knPath = path.join(dir, name + '.kn');
      const hasKn = fs.existsSync(knPath);
      sources.push({
        id: name,
        exe: full,
        kn: hasKn ? knPath : null,
        dir: path.basename(dir),
      });
    }
  }
  return sources;
}

function findBridgeExe(name) {
  if (name) {
    // Look up specific named exe
    const candidates = [
      path.join(__dirname, 'demos', name + '.exe'),
      path.join(__dirname, name + '.exe'),
    ];
    for (const c of candidates) {
      if (fs.existsSync(c)) return c;
    }
    return null;
  }
  // Default: bridge.exe
  return findBridgeExe('bridge');
}

function stopBridge() {
  if (bridgeProcess) {
    // Save PID before killing so the stale-exit guard can ignore this death.
    // (bridgeProcess is nulled immediately, so the exit handler would
    //  otherwise see the *new* process and incorrectly report "KILLED".)
    const killedPid = bridgeProcess.pid;
    try { bridgeProcess.kill(); } catch (e) {}
    bridgeProcess = null;
    // Set bridgePid to the killed pid so any late-arriving exit event
    // will match and be reported correctly (as the OLD bridge's death).
    bridgePid = killedPid;
  }
  bridgeRunning = false;
}

function startBridge(name) {
  stopBridge();
  const exePath = findBridgeExe(name);
  if (!exePath) {
    if (mainWindow && !mainWindow.isDestroyed()) {
      mainWindow.webContents.send('bridge-status', { state: 'missing', source: name });
    }
    return;
  }

  currentSource = name || 'bridge';
  // stdout may not be available in Electron — suppress EPIPE
  try { process.stdout.write(`[bridge] spawning: ${currentSource} (${exePath})\n`); } catch (e) { /* noop */ }
  bridgeRunning = true;

  // Capture the process in a local constant so event handlers use this
  // specific instance, NOT the mutable module-level bridgeProcess variable.
  // This prevents the stale-exit race: when stopBridge() kills the old
  // process, the old process's 'exit' event fires asynchronously — by
  // then bridgeProcess has been reassigned to the new process, causing
  // the old exit to be reported as the new process's death.
  let proc;
  try {
    proc = spawn(exePath, [], {
      cwd: path.dirname(exePath),
      stdio: ['pipe', 'pipe', 'pipe'],
      windowsHide: false,
    });
  } catch (e) {
    try { process.stderr.write(`[bridge] spawn failed: ${e.message}\n`); } catch (ee) {}
    bridgeRunning = false;
    return;
  }

  bridgeProcess = proc;
  bridgePid = proc.pid;  // set unconditionally — stale-exit guard below uses it

  if (mainWindow && !mainWindow.isDestroyed()) {
    mainWindow.webContents.send('bridge-status', {
      state: 'connected', source: currentSource, exe: exePath, pid: proc.pid,
    });
  }

  let buffer = '';
  proc.stdout.on('data', (chunk) => {
    buffer += chunk.toString('utf8');
    const lines = buffer.split('\n');
    buffer = lines.pop();
    for (const line of lines) {
      const trimmed = line.trim();
      if (!trimmed) continue;
      try {
        const data = JSON.parse(trimmed);
        if (mainWindow && !mainWindow.isDestroyed()) {
          mainWindow.webContents.send('kain-data', { source: currentSource, data });
        }
      } catch (e) {}
    }
  });

  proc.stderr.on('data', (chunk) => {
    try { process.stderr.write(`[${currentSource}:stderr] ` + chunk.toString()); } catch (e) {}
  });

  proc.on('error', (err) => {
    try { process.stderr.write(`[${currentSource}:error] ${err.message}\n`); } catch (e) {}
  });

  // Use proc (the captured local) NOT bridgeProcess (the mutable module var)
  // so we always report the correct pid for this specific process.
  proc.on('exit', (code, signal) => {
    bridgeRunning = false;
    // Stale-exit guard: only report if proc is still the current bridge.
    // bridgePid is updated by every startBridge/stopBridge call.
    if (proc.pid !== bridgePid) return;
    try { process.stdout.write(`[${currentSource}] exited code=${code} signal=${signal}\n`); } catch (e) {}
    if (mainWindow && !mainWindow.isDestroyed()) {
      const stateLabel = (code === 0) ? '✅ DONE' : (signal ? '💀 KILLED' : '🔴 EXITED');
      mainWindow.webContents.send('bridge-status', { state: 'disconnected', source: currentSource, code, signal, label: stateLabel });
    }
    // Don't auto-restart — let the user click again when ready
    // (Benchmark demos exit cleanly; live demos run forever)
  });
}

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1280,
    height: 800,
    webPreferences: { nodeIntegration: true, contextIsolation: false },
  });
  mainWindow.loadFile('index.html');
  if (process.argv.includes('--dev')) {
    mainWindow.webContents.openDevTools();
  }
  mainWindow.on('closed', () => { mainWindow = null; });
}

// IPC handlers
ipcMain.handle('discover-sources', () => discoverSources());
ipcMain.handle('switch-source', (_e, name) => {
  // Tell renderer to clear the scene before spawning new source
  if (mainWindow && !mainWindow.isDestroyed()) {
    mainWindow.webContents.send('kain-data', { data: { cmd: 'clear' } });
  }
  startBridge(name);
  return { ok: true, source: name };
});
ipcMain.handle('current-source', () => currentSource);

const { execSync } = require('child_process');

// Hot-load a .kn file: build it with kain, spawn the exe as the new bridge
ipcMain.handle('load-kain-file', async (_e, filePath) => {
  try {
    const knPath = path.resolve(filePath);
    const baseName = path.basename(knPath, '.kn');
    const knDir = path.dirname(knPath);
    console.log(`[loader] building: ${knPath}`);

    // Send status to renderer
    if (mainWindow && !mainWindow.isDestroyed()) {
      mainWindow.webContents.send('bridge-status', { state: 'building', source: baseName });
    }

    // Run kain build (suppress stdout/stderr)
    execSync(`kain build "${knPath}" --target llvm`, {
      cwd: knDir,
      encoding: 'utf8',
      timeout: 120000,
      stdio: 'ignore',
    });

    // Find the built exe matching baseName.
    // Search BOTH the electron project root's .kain/out (where build.kn puts
    // artifacts) AND the .kn file's directory .kain/out (where `kain build`
    // with cwd=<knDir> puts them for non-project compiles).
    const targetExe = baseName + '.exe';
    const searchRoots = [
      path.join(__dirname, '.kain', 'out'),        // electron project root
      path.join(knDir, '.kain', 'out'),             // .kn file's directory
    ];
    let exePath = null;
    function walkDir(dir, depth) {
      if (depth > 6 || exePath) return;
      try {
        const entries = fs.readdirSync(dir, { withFileTypes: true });
        for (const entry of entries) {
          if (exePath) return;
          const full = path.join(dir, entry.name);
          if (entry.isDirectory()) walkDir(full, depth + 1);
          else if (entry.name === targetExe) { exePath = full; return; }
        }
      } catch (_) {}
    }
    for (const root of searchRoots) {
      walkDir(root, 0);
      if (exePath) break;
    }
    if (!exePath) throw new Error('No ' + targetExe + ' found in .kain/out/ (searched: ' + searchRoots.join(', ') + ')');
    console.log(`[loader] built: ${exePath}`);

    // Copy to demos/ for discovery
    const demosDir = path.join(__dirname, 'demos');
    if (!fs.existsSync(demosDir)) fs.mkdirSync(demosDir, { recursive: true });
    const destExe = path.join(demosDir, baseName + '.exe');
    fs.copyFileSync(exePath, destExe);

    // Clear old scene and start new bridge
    if (mainWindow && !mainWindow.isDestroyed()) {
      mainWindow.webContents.send('kain-data', { data: { cmd: 'clear' } });
    }
    startBridge(baseName);

    return { ok: true, source: baseName, exe: destExe };
  } catch (e) {
    console.error('[loader] failed:', e.message);
    if (mainWindow && !mainWindow.isDestroyed()) {
      mainWindow.webContents.send('bridge-status', { state: 'error', source: path.basename(filePath, '.kn'), error: e.message });
    }
    return { ok: false, error: e.message };
  }
});

// ── Bidirectional: renderer events → stdin of Kain process ────
ipcMain.on('bridge-event', (_e, event) => {
  if (bridgeProcess && bridgeProcess.stdin && !bridgeProcess.stdin.destroyed) {
    try {
      bridgeProcess.stdin.write(JSON.stringify(event) + '\n');
    } catch (err) {
      // EPIPE — bridge exited, ignore
    }
  }
});

app.whenReady().then(() => {
  createWindow();
  setTimeout(() => startBridge('blackhole'), 2000);
});

app.on('window-all-closed', () => {
  stopBridge();
  app.quit();
});

app.on('activate', () => {
  if (BrowserWindow.getAllWindows().length === 0) createWindow();
});
