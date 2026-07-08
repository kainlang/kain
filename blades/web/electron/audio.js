// ============================================================================
//  audio.js — Kain Audio Lab: 2D Canvas + DAW Controls + DSP Effect Pipeline
// ============================================================================
//
//  WHAT:      Audio lab with waveform viz, file picker, per-track playback,
//             master volume, and a Kain DSP effect pipeline. Effect processing
//             is done by Kain-native executables via AudioBridge.
//
//  LAYOUT:    Toolbar → Effect Controls → 2D Canvas → Tracks → Transport
//
//  EFFECT FLOW:
//    1. User loads a WAV file (existing loadFile)
//    2. User selects an effect, adjusts sliders
//    3. User clicks "Process" → AudioBridge spawns a Kain exe
//    4. Kain reads the WAV, applies DSP, outputs base64 samples
//    5. AudioEngine decodes and plays the result
//
//  BUILDING EFFECTS (Kain side):
//    Create audio/effect_name.kn, then:
//      kain build audio/effect_name.kn --target llvm
//
// ============================================================================

// ── Available DSP effects with their parameter schemas ──────────────────
const EFFECTS = {
    lowpass: {
        label: 'Low-Pass Filter',
        icon: '🔽',
        params: [
            { key: 'freq_hz', label: 'Cutoff', min: 20, max: 8000, step: 10, default: 1000, unit: 'Hz' },
            { key: 'q',       label: 'Q',       min: 0.1, max: 20, step: 0.1, default: 0.707, unit: '' },
            { key: 'gain',    label: 'Gain',    min: 0,   max: 2,  step: 0.05, default: 1.0, unit: 'x' },
        ],
    },
    reverb: {
        label: 'Reverb',
        icon: '🌊',
        params: [
            { key: 'decay',   label: 'Decay',    min: 0.1, max: 10, step: 0.1, default: 2.0,  unit: 's' },
            { key: 'mix',     label: 'Mix',      min: 0,   max: 1,  step: 0.05, default: 0.5,  unit: '' },
            { key: 'room',    label: 'Room Size', min: 0.1, max: 1,  step: 0.05, default: 0.7, unit: '' },
        ],
    },
    delay: {
        label: 'Delay',
        icon: '⏳',
        params: [
            { key: 'delay_ms', label: 'Time',     min: 10,  max: 2000, step: 10,  default: 300, unit: 'ms' },
            { key: 'feedback', label: 'Feedback',  min: 0,   max: 0.99, step: 0.01, default: 0.5, unit: '' },
            { key: 'mix',      label: 'Mix',       min: 0,   max: 1,    step: 0.05, default: 0.4, unit: '' },
        ],
    },
    chorus: {
        label: 'Chorus',
        icon: '🎶',
        params: [
            { key: 'rate',     label: 'Rate',     min: 0.1, max: 10,  step: 0.1,  default: 1.5, unit: 'Hz' },
            { key: 'depth',    label: 'Depth',    min: 0,   max: 1,   step: 0.05, default: 0.5, unit: '' },
            { key: 'mix',      label: 'Mix',      min: 0,   max: 1,   step: 0.05, default: 0.5, unit: '' },
        ],
    },
    tremolo: {
        label: 'Tremolo',
        icon: '〰️',
        params: [
            { key: 'rate',     label: 'Rate',     min: 0.5, max: 20,  step: 0.5,  default: 5.0, unit: 'Hz' },
            { key: 'depth',    label: 'Depth',    min: 0,   max: 1,   step: 0.05, default: 0.7, unit: '' },
            { key: 'shape',    label: 'Shape',    min: 0,   max: 1,   step: 0.05, default: 0.5, unit: '' },
        ],
    },
    distortion: {
        label: 'Distortion',
        icon: '⚡',
        params: [
            { key: 'drive',    label: 'Drive',    min: 1,   max: 50,  step: 0.5, default: 5.0, unit: 'x' },
            { key: 'tone',     label: 'Tone',     min: 0,   max: 1,   step: 0.05, default: 0.5, unit: '' },
            { key: 'mix',      label: 'Mix',      min: 0,   max: 1,   step: 0.05, default: 0.7, unit: '' },
        ],
    },
};

class AudioEngine {
    constructor() {
        this.audioCtx = null;
        this.buffers = new Map();
        this.filePaths = new Map();       // name → original file path
        this.sources = new Map();
        this.gains = new Map();
        this.masterGain = null;
        this.canvas = null;
        this.ctx = null;
        this._animId = null;
        this._bridge = null;
        this._processing = false;
    }

    // ── Init ─────────────────────────────────────────────────────────
    _ensureAudio() {
        if (this.audioCtx) return;
        this.audioCtx = new (window.AudioContext || window.webkitAudioContext)();
        this.masterGain = this.audioCtx.createGain();
        this.masterGain.gain.value = 0.8;
        this.masterGain.connect(this.audioCtx.destination);
    }

    // ── Build the full UI ────────────────────────────────────────────
    buildUI(container) {
        container.innerHTML = '';
        container.style.cssText = `
            display:flex; flex-direction:column; height:100%;
            color:#e6edf3; font-family:'Courier New',monospace; font-size:13px;
            background:rgba(6,8,15,0.97);
        `;

        // ── TOOLBAR ──────────────────────────────────────────────────
        const toolbar = document.createElement('div');
        toolbar.id = 'audio-toolbar';
        toolbar.style.cssText = `
            display:flex; align-items:center; gap:8px; flex-wrap:wrap;
            padding:10px 14px; border-bottom:1px solid #1e293b; flex-shrink:0;
        `;

        this._addBtn(toolbar, '🔊 Audio Lab', null, true);
        this._addBtn(toolbar, '+ Load Audio', () => this._pickFiles());
        this._addBtn(toolbar, '⏹ Stop All', () => this.stopAll());
        this._addBtn(toolbar, '🗑 Clear All', () => { this.stopAll(); this.buffers.clear(); this.filePaths.clear(); this.gains.clear(); this._clearTracks(); this._clearWaveform(); });
        container.appendChild(toolbar);

        // ── EFFECT CONTROLS PANEL ────────────────────────────────────
        const effectsPanel = document.createElement('div');
        effectsPanel.id = 'audio-effects';
        effectsPanel.style.cssText = `
            display:flex; align-items:center; gap:12px; padding:8px 14px;
            border-bottom:1px solid #1e293b; flex-shrink:0; flex-wrap:wrap;
            background:rgba(10,12,22,0.7);
        `;
        this._buildEffectsPanel(effectsPanel);
        container.appendChild(effectsPanel);

        // ── 2D CANVAS — waveform visualization ───────────────────────
        const canvasWrap = document.createElement('div');
        canvasWrap.style.cssText = `
            flex-shrink:0; height:140px; border-bottom:1px solid #1e293b;
            background:#080b18; position:relative;
        `;
        this.canvas = document.createElement('canvas');
        this.canvas.style.cssText = 'width:100%;height:100%;display:block;';
        canvasWrap.appendChild(this.canvas);
        container.appendChild(canvasWrap);

        // ── TRACK LIST ───────────────────────────────────────────────
        const trackWrap = document.createElement('div');
        trackWrap.id = 'audio-tracks';
        trackWrap.style.cssText = 'flex:1; overflow-y:auto; padding:4px 10px;';
        trackWrap.innerHTML = '<div style="color:#555;padding:30px;text-align:center;">Drop or load audio files.</div>';
        container.appendChild(trackWrap);

        // ── TRANSPORT ────────────────────────────────────────────────
        const transport = document.createElement('div');
        transport.style.cssText = `
            display:flex; align-items:center; gap:12px;
            padding:10px 14px; border-top:1px solid #1e293b;
            flex-shrink:0; background:rgba(10,12,25,0.9);
        `;
        transport.innerHTML = '<span style="color:#8b949e;">Master</span>';
        const volLabel = document.createElement('span');
        volLabel.id = 'mvol';
        volLabel.style.cssText = 'color:#8b949e;font-size:11px;min-width:32px;';
        volLabel.textContent = '80%';
        transport.appendChild(volLabel);
        const volSlider = document.createElement('input');
        volSlider.type = 'range'; volSlider.min = 0; volSlider.max = 100; volSlider.value = 80;
        volSlider.style.cssText = 'flex:1;height:4px;accent-color:#58a6ff;cursor:pointer;';
        volSlider.oninput = () => {
            const v = volSlider.value / 100;
            this.setMasterVolume(v);
            volLabel.textContent = Math.round(v * 100) + '%';
        };
        transport.appendChild(volSlider);
        container.appendChild(transport);

        // Kick off canvas render loop
        this._drawLoop();

        // Initialize the AudioBridge
        this._initBridge();
    }

    // ── Build the effect controls panel ───────────────────────────────
    _buildEffectsPanel(parent) {
        // ── Effect selector ──────────────────────────────────────────
        const selLabel = document.createElement('span');
        selLabel.style.cssText = 'color:#8b949e;font-size:11px;white-space:nowrap;';
        selLabel.textContent = 'Effect:';
        parent.appendChild(selLabel);

        const selector = document.createElement('select');
        selector.id = 'fx-selector';
        selector.style.cssText = `
            background:#1e293b; border:1px solid #334155; color:#e6edf3;
            padding:4px 8px; border-radius:4px; font-size:11px;
            font-family:inherit; cursor:pointer;
        `;

        // Default placeholder
        const noneOpt = document.createElement('option');
        noneOpt.value = '';
        noneOpt.textContent = '— None —';
        selector.appendChild(noneOpt);

        // Populate from EFFECTS config
        for (const [key, fx] of Object.entries(EFFECTS)) {
            const opt = document.createElement('option');
            opt.value = key;
            opt.textContent = `${fx.icon} ${fx.label}`;
            selector.appendChild(opt);
        }
        selector.onchange = () => this._onEffectChange(selector.value);
        parent.appendChild(selector);

        // ── Parameter sliders container ──────────────────────────────
        const paramsWrap = document.createElement('div');
        paramsWrap.id = 'fx-params';
        paramsWrap.style.cssText = `
            display:flex; align-items:center; gap:8px; flex-wrap:wrap;
            flex:1; min-width:200px;
        `;
        parent.appendChild(paramsWrap);

        // ── Process button ───────────────────────────────────────────
        const procBtn = document.createElement('button');
        procBtn.id = 'fx-process-btn';
        procBtn.textContent = '⚙ Process';
        procBtn.style.cssText = `
            background:#1e6f2f; border:1px solid #238636; color:#e6edf3;
            padding:5px 14px; border-radius:4px; cursor:pointer;
            font-size:11px; font-family:inherit; font-weight:600;
            transition:all 0.15s;
        `;
        procBtn.onmouseenter = () => {
            if (!procBtn.disabled) procBtn.style.background = '#238636';
        };
        procBtn.onmouseleave = () => {
            if (!procBtn.disabled) procBtn.style.background = '#1e6f2f';
        };
        procBtn.onclick = () => this._processEffect();
        parent.appendChild(procBtn);

        // ── Loading indicator ────────────────────────────────────────
        const spinner = document.createElement('span');
        spinner.id = 'fx-spinner';
        spinner.style.cssText = 'display:none;color:#ffa657;font-size:11px;';
        spinner.textContent = '⏳ processing...';
        parent.appendChild(spinner);

        // ── Build default (no effect) params ─────────────────────────
        this._rebuildParams('');
    }

    // ── Rebuild parameter sliders for the selected effect ─────────────
    _rebuildParams(effectKey) {
        const wrap = document.getElementById('fx-params');
        if (!wrap) return;
        wrap.innerHTML = '';

        if (!effectKey || !EFFECTS[effectKey]) {
            wrap.innerHTML = '<span style="color:#555;font-size:11px;">Select an effect to configure</span>';
            return;
        }

        const fx = EFFECTS[effectKey];
        for (const p of fx.params) {
            const group = document.createElement('div');
            group.style.cssText = 'display:flex;align-items:center;gap:4px;';

            const label = document.createElement('span');
            label.style.cssText = 'color:#8b949e;font-size:10px;white-space:nowrap;min-width:38px;';
            label.textContent = p.label;

            const slider = document.createElement('input');
            slider.type = 'range';
            slider.min = p.min;
            slider.max = p.max;
            slider.step = p.step;
            slider.value = p.default;
            slider.dataset.key = p.key;
            slider.style.cssText = `
                width:60px; height:3px; accent-color:#58a6ff;
                cursor:pointer; vertical-align:middle;
            `;

            const valLabel = document.createElement('span');
            valLabel.style.cssText = 'color:#e6edf3;font-size:10px;min-width:32px;text-align:right;';
            valLabel.textContent = this._formatParamValue(p.default, p);

            slider.oninput = () => {
                const v = parseFloat(slider.value);
                valLabel.textContent = this._formatParamValue(v, p);
            };

            group.appendChild(label);
            group.appendChild(slider);
            group.appendChild(valLabel);
            wrap.appendChild(group);
        }
    }

    // ── Format a parameter value for display ──────────────────────────
    _formatParamValue(v, paramDef) {
        const p = paramDef;
        if (p.step >= 1) {
            return Math.round(v) + (p.unit || '');
        }
        return v.toFixed(p.step < 0.1 ? 2 : 1) + (p.unit || '');
    }

    // ── Handle effect selection change ────────────────────────────────
    _onEffectChange(effectKey) {
        const btn = document.getElementById('fx-process-btn');
        if (!btn) return;

        if (effectKey) {
            btn.disabled = false;
            btn.style.background = '#1e6f2f';
            btn.style.cursor = 'pointer';
            btn.style.opacity = '1';
        } else {
            btn.disabled = true;
            btn.style.background = '#1e293b';
            btn.style.cursor = 'default';
            btn.style.opacity = '0.4';
        }

        this._rebuildParams(effectKey);
    }

    // ── Initialize the AudioBridge (lazy) ─────────────────────────────
    _initBridge() {
        if (this._bridge) return;
        try {
            if (typeof AudioBridge !== 'undefined') {
                this._bridge = new AudioBridge();
            }
        } catch (e) {
            console.warn('[audio] AudioBridge not available:', e.message);
        }
    }

    // ── Process audio through the selected Kain DSP effect ────────────
    async _processEffect() {
        const selector = document.getElementById('fx-selector');
        const btn = document.getElementById('fx-process-btn');
        const spinner = document.getElementById('fx-spinner');
        if (!selector || !btn || !spinner) return;

        const effectKey = selector.value;
        if (!effectKey) return;

        // Find the first loaded audio buffer
        const names = Array.from(this.buffers.keys());
        if (names.length === 0) {
            spinner.textContent = '⚠️ Load a WAV file first';
            spinner.style.display = 'inline';
            setTimeout(() => { spinner.style.display = 'none'; }, 2500);
            return;
        }

        if (!this._bridge) {
            spinner.textContent = '⚠️ AudioBridge not available';
            spinner.style.display = 'inline';
            setTimeout(() => { spinner.style.display = 'none'; }, 3000);
            return;
        }

        const trackName = names[0];
        const audioBuf = this.buffers.get(trackName);

        // Gather parameter values from sliders
        const params = {};
        const wrap = document.getElementById('fx-params');
        if (wrap) {
            const sliders = wrap.querySelectorAll('input[type="range"]');
            for (const s of sliders) {
                params[s.dataset.key] = parseFloat(s.value);
            }
        }

        // Enter processing state
        this._processing = true;
        btn.disabled = true;
        btn.style.opacity = '0.4';
        spinner.textContent = '⏳ Processing through Kain DSP...';
        spinner.style.display = 'inline';

        try {
            // ── Approach 1: Use AudioBridge with a Kain exe ──────────
            //     The Kain exe reads the WAV from disk, processes,
            //     and outputs base64 samples stdout.
            //
            //     The track name is the original file name. We need
            //     to write the buffer to a temp WAV first if the
            //     AudioContext can't give us the original file path.

            const audioCtx = this.audioCtx;
            const sampleRate = audioBuf.sampleRate;
            const channels = audioBuf.numberOfChannels;
            const length = audioBuf.length;

            // Export AudioBuffer to raw interleaved Float32Array
            // so the Kain exe can process it
            let inputSamples;
            if (channels === 1) {
                inputSamples = audioBuf.getChannelData(0).slice();
            } else {
                // Interleave channels
                const ch0 = audioBuf.getChannelData(0);
                const ch1 = audioBuf.getChannelData(1);
                inputSamples = new Float32Array(length * channels);
                for (let i = 0; i < length; i++) {
                    inputSamples[i * channels] = ch0[i];
                    if (channels > 1) inputSamples[i * channels + 1] = ch1[i];
                }
            }

            // ── Send raw float samples directly to Kain exe ────────────
            const MAX_SAMPLES = 44100 * 2; // 2 seconds max for now
            const result = await this._bridge.process(effectKey, '', {
                ...params,
                samples: Array.from(inputSamples.slice(0, MAX_SAMPLES)),
                sampleRate,
                channels,
            });

            if (result.samples && result.samples.length > 0) {
                // Play the processed audio
                this._playProcessed(result.samples, result.sampleRate, effectKey);
                spinner.textContent = `✅ ${effectKey} done (${result.duration.toFixed(1)}s)`;
            } else {
                throw new Error('No samples returned');
            }
        } catch (err) {
            console.error('[audio] process error:', err);
            spinner.textContent = `❌ ${err.message.slice(0, 60)}`;
        }

        // Clear processing state
        this._processing = false;
        btn.disabled = false;
        btn.style.opacity = '1';
        setTimeout(() => {
            spinner.style.display = 'none';
        }, 3000);
    }

    // ── Encode Float32Array to base64 ────────────────────────────────
    _encodeF32B64(samples) {
        const bytes = new Uint8Array(samples.buffer);
        let binary = '';
        for (let i = 0; i < bytes.length; i++) {
            binary += String.fromCharCode(bytes[i]);
        }
        return btoa(binary);
    }

    // ── Play processed audio samples ─────────────────────────────────
    _playProcessed(samples, sampleRate, label) {
        this._ensureAudio();
        if (this.audioCtx.state === 'suspended') this.audioCtx.resume();

        const channels = 1; // Mono output from DSP for now
        const buf = this.audioCtx.createBuffer(channels, samples.length, sampleRate);

        for (let ch = 0; ch < channels; ch++) {
            const data = buf.getChannelData(ch);
            for (let i = 0; i < samples.length; i++) {
                data[i] = samples[i];
            }
        }

        // Add as a track
        const name = `🎛 ${label} — processed`;
        this.buffers.set(name, buf);
        this._addTrack(name, buf.duration);
        this._drawWaveform(buf);

        // Auto-play
        this.playTrack(name);
    }

    // ── Load audio file ──────────────────────────────────────────────
    loadFile(file) {
        this._ensureAudio();
        const reader = new FileReader();
        reader.onload = async (e) => {
            try {
                const buf = await this.audioCtx.decodeAudioData(e.target.result);
                this.buffers.set(file.name, buf);
                if (file.path) this.filePaths.set(file.name, file.path);
                this._addTrack(file.name, buf.duration);
                this._drawWaveform(buf);
            } catch (err) {
                console.error('[audio] decode failed:', file.name, err);
            }
        };
        reader.readAsArrayBuffer(file);
    }

    // ── Add a button to toolbar ──────────────────────────────────────
    _addBtn(parent, text, onClick, isLabel = false) {
        const el = document.createElement(isLabel ? 'span' : 'button');
        el.textContent = text;
        if (isLabel) {
            el.style.cssText = 'color:#58a6ff;font-weight:700;font-size:14px;margin-right:6px;';
        } else {
            el.style.cssText = `
                background:#1e293b; border:1px solid #334155; color:#e6edf3;
                padding:5px 12px; border-radius:4px; cursor:pointer;
                font-size:11px; font-family:inherit; white-space:nowrap;
            `;
            el.onmouseenter = () => el.style.background = '#334155';
            el.onmouseleave = () => el.style.background = '#1e293b';
            el.onclick = onClick;
        }
        parent.appendChild(el);
        return el;
    }

    // ── File picker ──────────────────────────────────────────────────
    _pickFiles() {
        const input = document.createElement('input');
        input.type = 'file';
        input.accept = '.wav,.mp3,.ogg,.flac,.m4a,.aac,.aiff';
        input.multiple = true;
        input.style.display = 'none';
        input.onchange = () => {
            for (const f of input.files) this.loadFile(f);
            input.remove();
        };
        document.body.appendChild(input);
        input.click();
    }

    // ── Playback ─────────────────────────────────────────────────────
    playTrack(name) {
        this._ensureAudio();
        if (this.audioCtx.state === 'suspended') this.audioCtx.resume();
        this.stopTrack(name);
        const buf = this.buffers.get(name);
        if (!buf) return;
        const src = this.audioCtx.createBufferSource();
        src.buffer = buf;
        let g = this.gains.get(name);
        if (!g) { g = this.audioCtx.createGain(); g.gain.value = 0.8; this.gains.set(name, g); }
        src.connect(g); g.connect(this.masterGain);
        src.start(0);
        this.sources.set(name, src);
        this._setState(name, 'playing');
        src.onended = () => {
            if (this.sources.get(name) === src) {
                this.sources.delete(name);
                this._setState(name, 'stopped');
            }
        };
    }

    stopTrack(name) {
        const src = this.sources.get(name);
        if (src) { try { src.stop(); } catch(e) {} this.sources.delete(name); }
        this._setState(name, 'stopped');
    }

    stopAll() { for (const [n] of this.sources) this.stopTrack(n); }
    setTrackVolume(name, v) {
        let g = this.gains.get(name);
        if (!g) { g = this.audioCtx.createGain(); this.gains.set(name, g); }
        g.gain.value = Math.max(0, Math.min(1, v));
    }
    setMasterVolume(v) { if (this.masterGain) this.masterGain.gain.value = Math.max(0, Math.min(1, v)); }

    // ── Track UI ─────────────────────────────────────────────────────
    _addTrack(name, dur) {
        const wrap = document.getElementById('audio-tracks');
        if (!wrap) return;
        const ph = wrap.querySelector('div[style*="text-align:center"]');
        if (ph) ph.remove();
        const row = document.createElement('div');
        row.id = 'trk-' + this._id(name);
        row.style.cssText = 'display:flex;align-items:center;gap:8px;padding:6px 8px;border-bottom:1px solid #1e293b;';

        const play = this._mkBtn('▶', () => this.playTrack(name), '28px');
        const stop = this._mkBtn('⏹', () => this.stopTrack(name), '28px');
        row.appendChild(play); row.appendChild(stop);

        const nm = document.createElement('span');
        nm.style.cssText = 'flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;font-size:12px;';
        nm.textContent = name;
        row.appendChild(nm);

        const st = document.createElement('span');
        st.id = 'st-' + this._id(name);
        st.style.cssText = 'color:#555;font-size:11px;min-width:36px;text-align:right;';
        st.textContent = '⏹';
        row.appendChild(st);

        const du = document.createElement('span');
        du.style.cssText = 'color:#555;font-size:11px;min-width:44px;text-align:right;';
        du.textContent = dur.toFixed(1) + 's';
        row.appendChild(du);

        const vs = document.createElement('input');
        vs.type = 'range'; vs.min = 0; vs.max = 100; vs.value = 80;
        vs.style.cssText = 'width:54px;height:4px;accent-color:#58a6ff;cursor:pointer;';
        vs.oninput = () => this.setTrackVolume(name, vs.value / 100);
        row.appendChild(vs);

        const rm = this._mkBtn('✕', () => { this.stopTrack(name); this.buffers.delete(name); this.filePaths.delete(name); this.gains.delete(name); row.remove(); this._maybePlaceholder(); }, '22px');
        rm.style.background = 'transparent'; rm.style.color = '#555'; rm.style.border = 'none';
        rm.onmouseenter = () => rm.style.color = '#ff7b72';
        rm.onmouseleave = () => rm.style.color = '#555';
        row.appendChild(rm);
        wrap.appendChild(row);
    }

    _setState(name, s) {
        const el = document.getElementById('st-' + this._id(name));
        if (!el) return;
        el.textContent = s === 'playing' ? '▶' : '⏹';
        el.style.color = s === 'playing' ? '#7ee787' : '#555';
    }

    _clearTracks() {
        const wrap = document.getElementById('audio-tracks');
        if (wrap) wrap.innerHTML = '<div style="color:#555;padding:30px;text-align:center;">Drop or load audio files.</div>';
    }
    _maybePlaceholder() {
        const wrap = document.getElementById('audio-tracks');
        if (wrap && wrap.children.length === 0)
            wrap.innerHTML = '<div style="color:#555;padding:30px;text-align:center;">Drop or load audio files.</div>';
    }

    // ── Waveform drawing ─────────────────────────────────────────────
    _drawWaveform(buffer) {
        const data = buffer.getChannelData(0);
        const step = Math.ceil(data.length / 2048);
        this._cachedWave = [];
        for (let i = 0; i < 2048; i++) {
            let sum = 0, n = 0;
            for (let j = i * step; j < (i + 1) * step && j < data.length; j++) {
                sum += Math.abs(data[j]); n++;
            }
            this._cachedWave.push(n > 0 ? sum / n : 0);
        }
    }

    _clearWaveform() { this._cachedWave = null; }

    _drawLoop() {
        const render = () => {
            this._animId = requestAnimationFrame(render);
            if (!this.canvas) return;
            const rect = this.canvas.parentElement.getBoundingClientRect();
            if (this.canvas.width !== rect.width || this.canvas.height !== rect.height) {
                this.canvas.width = rect.width;
                this.canvas.height = rect.height;
            }
            const ctx = this.canvas.getContext('2d');
            const w = this.canvas.width, h = this.canvas.height;
            ctx.fillStyle = '#080b18';
            ctx.fillRect(0, 0, w, h);

            if (this._cachedWave) {
                const mid = h / 2;
                ctx.strokeStyle = '#58a6ff';
                ctx.lineWidth = 1.5;
                ctx.beginPath();
                for (let i = 0; i < this._cachedWave.length; i++) {
                    const x = (i / this._cachedWave.length) * w;
                    const amp = this._cachedWave[i] * h * 4;
                    if (i === 0) ctx.moveTo(x, mid - amp);
                    else ctx.lineTo(x, mid - amp);
                }
                ctx.stroke();
                ctx.beginPath();
                for (let i = 0; i < this._cachedWave.length; i++) {
                    const x = (i / this._cachedWave.length) * w;
                    const amp = this._cachedWave[i] * h * 4;
                    if (i === 0) ctx.moveTo(x, mid + amp);
                    else ctx.lineTo(x, mid + amp);
                }
                ctx.stroke();
            } else {
                ctx.fillStyle = '#1e293b';
                ctx.font = '12px monospace';
                ctx.textAlign = 'center';
                ctx.fillText('Load audio to see waveform', w / 2, h / 2 + 4);
            }
        };
        render();
    }

    // ── Helpers ──────────────────────────────────────────────────────
    _mkBtn(text, onClick, width) {
        const b = document.createElement('button');
        b.textContent = text;
        b.style.cssText = `background:#1e293b;border:1px solid #334155;color:#e6edf3;
            padding:4px 0;border-radius:3px;cursor:pointer;font-size:10px;
            width:${width || '30px'};font-family:inherit;`;
        b.onmouseenter = () => b.style.background = '#334155';
        b.onmouseleave = () => b.style.background = '#1e293b';
        b.onclick = onClick;
        return b;
    }
    _id(name) { return name.replace(/[^a-zA-Z0-9_-]/g, '_'); }
}

window.AudioEngine = AudioEngine;
