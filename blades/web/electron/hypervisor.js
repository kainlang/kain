// ============================================================================
//  hypervisor.js — K-OS Hypervisor Bar: Blender-style workflow switcher
// ============================================================================
//
//  WHAT:      Top navigation bar that switches the main content area
//             between pages: 3D Viewport, Audio Lab, and future panels.
//             Thick 96px bar with workflow categories + module buttons.
//
//  LAYOUT:    Logo (left) → Workflow Modules (center) → Actions (right)
// ============================================================================

// ── Workflow definitions ─────────────────────────────────────────────
const WORKFLOWS = [
    {
        label: '3D', color: '#58a6ff',
        modules: [
            { id: 'viewport', name: 'Viewport', icon: '🎮' },
            { id: 'terrain',  name: 'Terrain',  icon: '⛰️' },
        ]
    },
    {
        label: 'AUD', color: '#ff6644',
        modules: [
            { id: 'audio',    name: 'Audio Lab', icon: '🔊' },
            { id: 'waveform', name: 'Waveform',  icon: '〰️' },
        ]
    },
    {
        label: 'FX', color: '#aa44ff',
        modules: [
            { id: 'shaders',  name: 'Shaders',   icon: '✨' },
            { id: 'particle', name: 'Particles',  icon: '💫' },
        ]
    },
    {
        label: 'DEV', color: '#7ee787',
        modules: [
            { id: 'demos',    name: 'Demos',     icon: '📁' },
            { id: 'telemetry',name: 'Telemetry', icon: '📊' },
        ]
    },
];

class Hypervisor {
    constructor() {
        this.activeModule = 'viewport';
        this.pages = new Map();      // moduleId → HTMLElement
        this._callbacks = new Map();  // moduleId → onShow callbacks
    }

    // ── Register a page container for a module ───────────────────────
    registerPage(moduleId, element, onShow = null) {
        this.pages.set(moduleId, element);
        if (onShow) this._callbacks.set(moduleId, onShow);
    }

    // ── Build the top bar into a container ───────────────────────────
    build(container) {
        container.innerHTML = '';
        container.style.cssText = `
            height:96px; background:rgba(10,10,10,0.95);
            border-bottom:1px solid #222; display:flex; align-items:center;
            position:relative; z-index:50; user-select:none;
            flex-shrink:0; overflow:hidden;
        `;

        // Logo (left)
        const logo = document.createElement('div');
        logo.style.cssText = `
            padding:0 24px; display:flex; align-items:center; gap:10px;
            cursor:pointer; flex-shrink:0; height:100%;
        `;
        logo.innerHTML = `
            <svg width="22" height="22" viewBox="0 0 100 100" style="color:#58a6ff;">
                <polygon points="50,5 95,27.5 95,72.5 50,95 5,72.5 5,27.5"
                    fill="none" stroke="currentColor" stroke-width="6"/>
            </svg>
            <span style="color:#e6edf3;font-weight:700;font-size:16px;letter-spacing:2px;">K_OS</span>
        `;
        container.appendChild(logo);

        // Center: workflow groups
        const center = document.createElement('div');
        center.style.cssText = `
            flex:1; display:flex; align-items:center; justify-content:center;
            gap:12px; padding:0 20px; overflow-x:auto;
        `;
        center.id = 'hypervisor-center';

        for (const wf of WORKFLOWS) {
            const group = document.createElement('div');
            group.style.cssText = `
                display:flex; align-items:center; gap:4px;
                background:rgba(15,15,15,0.5); border-radius:999px;
                border:1px solid #222; padding:4px 6px;
            `;

            // Group label
            const label = document.createElement('span');
            label.style.cssText = `
                font-size:9px; font-weight:900; letter-spacing:1px;
                color:${wf.color}; opacity:0.4; writing-mode:vertical-lr;
                padding:4px 4px; text-orientation:mixed; margin-right:2px;
            `;
            label.textContent = wf.label;
            group.appendChild(label);

            // Module buttons
            for (const mod of wf.modules) {
                const btn = document.createElement('button');
                btn.dataset.module = mod.id;
                btn.style.cssText = this._btnStyle(mod.id === this.activeModule, wf.color);
                btn.innerHTML = `<span style="font-size:11px;margin-right:4px;">${mod.icon}</span>${mod.name}`;
                btn.onmouseenter = () => { if (mod.id !== this.activeModule) btn.style.background = '#1a1a1a'; };
                btn.onmouseleave = () => { if (mod.id !== this.activeModule) btn.style.background = 'transparent'; };
                btn.onclick = () => this.switchTo(mod.id);
                group.appendChild(btn);
            }

            center.appendChild(group);
        }

        container.appendChild(center);

        // Right: status
        const right = document.createElement('div');
        right.style.cssText = `
            padding:0 20px; display:flex; align-items:center; gap:12px;
            flex-shrink:0; font-size:11px; color:#555;
        `;
        right.innerHTML = `
            <span id="hypervisor-status" style="display:flex;align-items:center;gap:6px;">
                <span style="width:6px;height:6px;border-radius:50%;background:#7ee787;box-shadow:0 0 6px #7ee787;"></span>
                READY
            </span>
        `;
        container.appendChild(right);

        // Initial page show
        this._showPage(this.activeModule);
    }

    // ── Switch to a module ───────────────────────────────────────────
    switchTo(moduleId) {
        if (moduleId === this.activeModule) return;
        this.activeModule = moduleId;

        // Update button styles
        document.querySelectorAll('#hypervisor-center button[data-module]').forEach(btn => {
            const wf = WORKFLOWS.flatMap(w => w.modules).find(m => m.id === btn.dataset.module);
            const color = WORKFLOWS.find(w => w.modules.some(m => m.id === btn.dataset.module))?.color || '#58a6ff';
            btn.style.cssText = this._btnStyle(btn.dataset.module === moduleId, color);
        });

        this._showPage(moduleId);
    }

    // ── Internal: show/hide pages ────────────────────────────────────
    _showPage(moduleId) {
        for (const [id, el] of this.pages) {
            el.style.display = id === moduleId ? 'flex' : 'none';
        }
        const cb = this._callbacks.get(moduleId);
        if (cb) cb();
    }

    _btnStyle(active, color) {
        const base = `
            padding:6px 12px; border-radius:6px; border:none;
            font-size:11px; font-weight:600; cursor:pointer;
            font-family:inherit; white-space:nowrap;
            transition:all 0.15s; display:flex; align-items:center;
        `;
        if (active) {
            return base + `
                background:#1a1a1a; color:#e6edf3;
                box-shadow:0 0 12px rgba(255,255,255,0.04);
                outline:1px solid ${color}33;
            `;
        }
        return base + `background:transparent;color:#555;&:hover{background:#1a1a1a;color:#e6edf3;}`;
    }
}

window.Hypervisor = Hypervisor;
