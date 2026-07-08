// ============================================================================
//  audio_bridge.js — Kain Audio DSP: spawn effect exes, route WAV data
// ============================================================================
//
//  WHAT:      Bridge between JS (AudioEngine) and Kain audio processing
//             executables compiled from audio/*.kn files. Spawns a Kain
//             process per effect call, sends WAV path + params via stdin
//             JSON, reads processed samples from stdout JSON.
//
//  PROTOCOL:
//    stdin  → { "command":"process", "samples":[0.1,-0.05,...], "sr":44100, "params":{...} }
//             { "command":"process", "wavPath":"...", "params":{...} }
//    stdout ← { "status":"ok", "samples":[0.1,-0.05,...], "sr":44100,
//                "count":N }
//
//  ERROR:
//    stdout ← { "status":"error", "error":"..." }
//
//  USAGE:
//    const bridge = new AudioBridge();
//    const result = await bridge.process('lowpass', '/path/to/file.wav',
//      { freq_hz: 1000, q: 0.707 });
//    engine._playRawBuffer(result.samples, result.sampleRate);
//
//  BUILDING EFFECTS:
//    kain build audio/lowpass.kn --emit sharedlib  →  audio/lowpass.exe
//
// ============================================================================

const { spawn } = require('child_process');
const path = require('path');
const fs = require('fs');

class AudioBridge {
    constructor() {
        this._audioDir = path.join(__dirname, 'audio');
        this._demosDir = path.join(__dirname, 'demos');
        this._processes = new Map();
    }

    // ── Process audio through a Kain DSP effect ─────────────────────────
    //
    //  effectName:  'lowpass', 'reverb', 'delay', 'chorus', 'tremolo'
    //  wavPath:     absolute or relative path to a .wav file
    //  params:      { freq_hz: 1000, q: 0.707, gain: 1.0, mix: 0.5, ... }
    //
    //  Returns: { samples: Float32Array, sampleRate: number, channels: number,
    //             duration: number }
    //
    async process(effectName, wavPath, params = {}) {
        let exePath = this._findExe(effectName);
        if (!exePath) {
            // Auto-build if source exists
            const knPath = path.join(this._audioDir, effectName + '.kn');
            if (fs.existsSync(knPath)) {
                const { execSync } = require('child_process');
                try {
                    execSync(`kain build "${knPath}" --target llvm`, {
                        cwd: this._audioDir,
                        stdio: 'pipe',
                        timeout: 120000,
                    });
                    const builtExe = path.join(this._audioDir, '.kain', 'out', 'x86_64-windows', 'dev', 'll', effectName, 'compile', effectName + '.exe');
                    if (fs.existsSync(builtExe)) {
                        fs.copyFileSync(builtExe, path.join(this._audioDir, effectName + '.exe'));
                        exePath = path.join(this._audioDir, effectName + '.exe');
                    }
                } catch (buildErr) {
                    throw new Error(`Auto-build failed for "${effectName}": ${buildErr.message}`);
                }
            }
            if (!exePath) {
                throw new Error(`Audio effect "${effectName}" not found and could not be built.`);
            }
        }

        return new Promise((resolve, reject) => {
            const proc = spawn(exePath, [], {
                cwd: this._audioDir,
                stdio: ['pipe', 'pipe', 'pipe'],
                windowsHide: true,
            });

            let stdoutBuf = Buffer.alloc(0);
            let stderrBuf = '';

            proc.stdout.on('data', (chunk) => {
                stdoutBuf = Buffer.concat([stdoutBuf, chunk]);
            });

            proc.stderr.on('data', (chunk) => {
                stderrBuf += chunk.toString('utf8');
            });

            proc.on('error', (err) => {
                this._processes.delete(effectName);
                reject(new Error(`Process spawn failed for "${effectName}": ${err.message}`));
            });

            proc.on('exit', (code) => {
                this._processes.delete(effectName);

                // If we got valid JSON on stdout, use it regardless of exit code
                const stdoutStr = stdoutBuf.toString('utf8').trim();

                if (!stdoutStr) {
                    const detail = stderrBuf ? ` — ${stderrBuf.trim()}` : '';
                    reject(new Error(`Effect "${effectName}" exited code ${code} with no output${detail}`));
                    return;
                }

                try {
                    const result = JSON.parse(stdoutStr);

                    if (result.status === 'error') {
                        reject(new Error(`Effect "${effectName}" error: ${result.error || 'unknown'}`));
                        return;
                    }

                    if (result.status !== 'ok') {
                        reject(new Error(`Effect "${effectName}" unknown status: "${result.status}"`));
                        return;
                    }

                    if (!result.samples) {
                        reject(new Error(`Effect "${effectName}" returned no samples`));
                        return;
                    }

                    // Use JSON float array directly (no base64 decode)
                    const samples = new Float32Array(result.samples);

                    resolve({
                        samples,
                        sampleRate: result.sr || 44100,
                        channels: 1,
                        duration: samples.length / (result.sr || 44100),
                        effectName,
                    });
                } catch (e) {
                    reject(new Error(`Effect "${effectName}" output parse failed: ${e.message}\nRaw: ${stdoutStr.slice(0, 200)}`));
                }
            });

            // Track the process so we can kill it if needed
            this._processes.set(effectName, proc);

            // Build command: prefer inline samples over file path
            const { samples, sampleRate, channels, ...effectParams } = params;
            const command = {
                command: 'process',
                params: effectParams,
            };

            if (samples && Array.isArray(samples) && samples.length > 0) {
                // Send samples inline (no file I/O needed on Kain side)
                command.samples = samples;
                command.sr = sampleRate || 44100;
                command.channels = channels || 1;
            } else {
                // Fall back to WAV file path
                command.wavPath = path.resolve(wavPath || '');
            }

            proc.stdin.write(JSON.stringify(command) + '\n');
            proc.stdin.end();
        });
    }

    // ── Decode a base64 string into a Float32Array ──────────────────────
    _decodeB64F32(b64) {
        const binary = atob(b64);
        const bytes = new Uint8Array(binary.length);
        for (let i = 0; i < binary.length; i++) {
            bytes[i] = binary.charCodeAt(i);
        }
        return new Float32Array(bytes.buffer);
    }

    // ── Discover available audio effects in audio/ directory ────────────
    scanEffects() {
        const effects = [];
        if (!fs.existsSync(this._audioDir)) return effects;

        try {
            const entries = fs.readdirSync(this._audioDir, { withFileTypes: true });
            for (const entry of entries) {
                if (!entry.isFile()) continue;
                if (entry.name.endsWith('.exe')) {
                    const name = entry.name.replace(/\.exe$/, '');
                    const knFile = path.join(this._audioDir, name + '.kn');
                    effects.push({
                        name,
                        exe: path.join(this._audioDir, entry.name),
                        hasKn: fs.existsSync(knFile),
                        knPath: fs.existsSync(knFile) ? knFile : null,
                    });
                }
            }
        } catch (e) {
            console.error('[AudioBridge] scan error:', e.message);
        }

        return effects.sort((a, b) => a.name.localeCompare(b.name));
    }

    // ── Check if an effect executable exists ────────────────────────────
    hasEffect(name) {
        return this._findExe(name) !== null;
    }

    // ── Test an effect's connectivity (spawn, send ping, expect pong) ───
    async ping(effectName) {
        const exePath = this._findExe(effectName);
        if (!exePath) return { ok: false, error: 'Not found' };

        return new Promise((resolve) => {
            const proc = spawn(exePath, ['--ping'], {
                stdio: ['pipe', 'pipe', 'pipe'],
                windowsHide: true,
                timeout: 5000,
            });

            let stdout = '';
            proc.stdout.on('data', (chunk) => { stdout += chunk.toString(); });

            proc.on('exit', (code) => {
                resolve({
                    ok: code === 0,
                    code,
                    stdout: stdout.trim(),
                });
            });

            proc.on('error', (err) => {
                resolve({ ok: false, error: err.message });
            });

            proc.stdin.write(JSON.stringify({ command: 'ping' }) + '\n');
            proc.stdin.end();
        });
    }

    // ── Kill all running effect processes ───────────────────────────────
    killAll() {
        for (const [name, proc] of this._processes) {
            try {
                if (!proc.killed) proc.kill();
            } catch (e) { /* ignore */ }
        }
        this._processes.clear();
    }

    // ── Find an effect executable by name ───────────────────────────────
    _findExe(name) {
        const candidates = [
            path.join(this._audioDir, name + '.exe'),
            path.join(this._demosDir, name + '.exe'),
            path.join(__dirname, name + '.exe'),
        ];
        for (const c of candidates) {
            if (fs.existsSync(c)) return c;
        }
        return null;
    }
}

// ── Expose for renderer (nodeIntegration: true, contextIsolation: false) ─
window.AudioBridge = AudioBridge;
