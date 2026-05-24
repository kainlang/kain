#!/usr/bin/env python3
"""
Kain-Lang Documentation and Simulator Generator (generate_sim.py)
Automates creation of high-contrast, leak-proof interactive documentation panels
with pre-baked high-performance canvas animations, CLI argparse controls, and auto-computed styles.
"""

import os
import sys
import argparse

def get_nodes_js(accent):
    return f"""
                // --- NODES VISUALIZER ---
                const nodes = [];
                const nodeCount = 24;
                const connectionDist = 70;
                let surgeActive = false;
                let surgeTime = 0;

                for (let i = 0; i < nodeCount; i++) {{
                    nodes.push({{
                        x: Math.random(),
                        y: Math.random(),
                        vx: (Math.random() - 0.5) * 0.002,
                        vy: (Math.random() - 0.5) * 0.002,
                        radius: 2 + Math.random() * 3,
                        pulse: Math.random() * Math.PI
                    }});
                }}

                btn1.addEventListener('click', () => {{
                    surgeActive = true;
                    surgeTime = 0;
                    logEvent("[Surge] Data packet surge injected across network nodes!", true);
                }});

                btn2.addEventListener('click', () => {{
                    nodes.forEach(n => {{
                        n.x = Math.random();
                        n.y = Math.random();
                        n.vx = (Math.random() - 0.5) * 0.004;
                        n.vy = (Math.random() - 0.5) * 0.004;
                    }});
                    logEvent("[Topology] Node coordinates randomised and velocities reset.");
                }});

                btn3.addEventListener('click', () => {{
                    logEvent("[FFI] Entangled bridging connection established.");
                }});

                function updateAndDraw(time) {{
                    // Physics & boundaries
                    nodes.forEach(n => {{
                        n.x += n.vx;
                        n.y += n.vy;
                        n.pulse += 0.05;

                        if (n.x < 0.05 || n.x > 0.95) n.vx *= -1;
                        if (n.y < 0.05 || n.y > 0.95) n.vy *= -1;
                    }});

                    // Connections
                    ctx.lineWidth = 0.8;
                    for (let i = 0; i < nodeCount; i++) {{
                        for (let j = i + 1; j < nodeCount; j++) {{
                            const dx = (nodes[i].x - nodes[j].x) * canvas.width;
                            const dy = (nodes[i].y - nodes[j].y) * canvas.height;
                            const dist = Math.sqrt(dx * dx + dy * dy);

                            if (dist < connectionDist) {{
                                const alpha = (1 - dist / connectionDist) * 0.25;
                                if (surgeActive) {{
                                    ctx.strokeStyle = `rgba(255, 255, 255, ${{alpha * 2.0}})`;
                                }} else {{
                                    ctx.strokeStyle = `rgba(0, 255, 204, ${{alpha}})`;
                                }}
                                ctx.beginPath();
                                ctx.moveTo(nodes[i].x * canvas.width, nodes[i].y * canvas.height);
                                ctx.lineTo(nodes[j].x * canvas.width, nodes[j].y * canvas.height);
                                ctx.stroke();
                            }}
                        }}
                    }}

                    // Render nodes
                    nodes.forEach(n => {{
                        const cx = n.x * canvas.width;
                        const cy = n.y * canvas.height;
                        const sizePulse = Math.sin(n.pulse) * 1;

                        ctx.fillStyle = '#020406';
                        ctx.strokeStyle = '{accent}';
                        ctx.lineWidth = surgeActive ? 2 : 1;
                        ctx.beginPath();
                        ctx.arc(cx, cy, n.radius + sizePulse, 0, Math.PI * 2);
                        ctx.fill();
                        ctx.stroke();

                        if (surgeActive) {{
                            ctx.fillStyle = 'rgba(255, 255, 255, 0.7)';
                            ctx.beginPath();
                            ctx.arc(cx, cy, 1.5, 0, Math.PI * 2);
                            ctx.fill();
                        }}
                    }});

                    if (surgeActive) {{
                        surgeTime += 0.02;
                        if (surgeTime > 1.0) surgeActive = false;
                    }}
                }}
    """

def get_particles_js(accent):
    return f"""
                // --- PARTICLES VISUALIZER ---
                const particles = [];
                const maxParticles = 60;
                let reverseMode = false;
                let blastRadius = 0;

                for (let i = 0; i < maxParticles; i++) {{
                    particles.push({{
                        x: Math.random() * canvas.width,
                        y: Math.random() * canvas.height,
                        vx: (Math.random() - 0.5) * 1.2,
                        vy: (Math.random() - 0.5) * 1.2,
                        size: 1 + Math.random() * 2,
                        color: Math.random() > 0.3 ? '{accent}' : '#ffffff'
                    }});
                }}

                btn1.addEventListener('click', () => {{
                    blastRadius = 40;
                    logEvent("[Blast] Particle swarm scatter triggered!", true);
                }});

                btn2.addEventListener('click', () => {{
                    reverseMode = !reverseMode;
                    particles.forEach(p => {{
                        p.vx *= -1;
                        p.vy *= -1;
                    }});
                    logEvent(`[Direction] Swarm vectors inverted. reverse = ${{reverseMode}}`);
                }});

                btn3.addEventListener('click', () => {{
                    logEvent("[Entropy] System entropy decayed. Resetting velocities.");
                    particles.forEach(p => {{
                        p.vx = (Math.random() - 0.5) * 1.2;
                        p.vy = (Math.random() - 0.5) * 1.2;
                    }});
                }});

                function updateAndDraw(time) {{
                    particles.forEach(p => {{
                        // Apply slight vector flow noise
                        p.vx += Math.sin(time + p.y * 0.01) * 0.02;
                        p.vy += Math.cos(time + p.x * 0.01) * 0.02;

                        if (blastRadius > 0) {{
                            const dx = p.x - canvas.width / 2;
                            const dy = p.y - canvas.height / 2;
                            const d = Math.sqrt(dx * dx + dy * dy) || 1;
                            p.vx += (dx / d) * 0.4;
                            p.vy += (dy / d) * 0.4;
                        }}

                        p.x += p.vx;
                        p.y += p.vy;

                        // Bounds wrap
                        if (p.x < 0) p.x = canvas.width;
                        if (p.x > canvas.width) p.x = 0;
                        if (p.y < 0) p.y = canvas.height;
                        if (p.y > canvas.height) p.y = 0;

                        // Render
                        ctx.fillStyle = p.color;
                        ctx.shadowColor = '{accent}';
                        ctx.shadowBlur = 4;
                        ctx.beginPath();
                        ctx.arc(p.x, p.y, p.size, 0, Math.PI * 2);
                        ctx.fill();
                        ctx.shadowBlur = 0;
                    }});

                    if (blastRadius > 0) blastRadius -= 1.2;
                }}
    """

def get_lattice_js(accent):
    return f"""
                // --- LATTICE MATRIX VISUALIZER ---
                const cols = 6;
                const rows = 4;
                const cells = [];
                let activeWave = false;
                let waveCol = -1;
                let waveTime = 0;

                for (let r = 0; r < rows; r++) {{
                    for (let c = 0; c < cols; c++) {{
                        cells.push({{
                            r, c,
                            val: Math.random() > 0.7 ? 1 : 0,
                            glow: 0
                        }});
                    }}
                }}

                btn1.addEventListener('click', () => {{
                    cells.forEach(cell => {{
                        if (Math.random() > 0.5) {{
                            cell.val = Math.floor(Math.random() * 9) + 1;
                            cell.glow = 1.0;
                        }}
                    }});
                    logEvent("[Tick] Epoch incremented. Cellular lattice values updated.", true);
                }});

                btn2.addEventListener('click', () => {{
                    activeWave = true;
                    waveCol = 0;
                    waveTime = 0;
                    logEvent("[Wave] Signal wavefront propagation started...", true);
                }});

                btn3.addEventListener('click', () => {{
                    cells.forEach(cell => {{
                        cell.val = 0;
                        cell.glow = 0;
                    }});
                    logEvent("[Matrix] State lattice wiped to zero.");
                }});

                function updateAndDraw(time) {{
                    const w = canvas.width / cols;
                    const h = canvas.height / rows;

                    cells.forEach(cell => {{
                        const cx = cell.c * w + w / 2;
                        const cy = cell.r * h + h / 2;

                        if (activeWave && cell.c === waveCol) {{
                            cell.val = (cell.val + 2) % 10;
                            cell.glow = 1.0;
                        }}

                        if (cell.glow > 0) cell.glow -= 0.02;

                        // Draw Grid Border
                        ctx.strokeStyle = 'rgba(255,255,255,0.02)';
                        ctx.lineWidth = 1;
                        ctx.strokeRect(cell.c * w, cell.r * h, w, h);

                        // Draw Glowing Cell
                        if (cell.glow > 0 || cell.val > 0) {{
                            ctx.fillStyle = `rgba(0, 255, 204, ${{Math.max(0.02, cell.glow * 0.12)}})`;
                            ctx.fillRect(cell.c * w + 2, cell.r * h + 2, w - 4, h - 4);
                        }}

                        // Value
                        ctx.fillStyle = cell.glow > 0 ? '#ffffff' : 'rgba(255, 255, 255, 0.4)';
                        ctx.font = '7px monospace';
                        ctx.textAlign = 'center';
                        ctx.fillText(cell.val > 0 ? cell.val.toString() : '.', cx, cy + 2);
                    }});

                    if (activeWave) {{
                        waveTime += 0.05;
                        if (waveTime > 1.0) {{
                            waveTime = 0;
                            waveCol++;
                            if (waveCol >= cols) {{
                                activeWave = false;
                                waveCol = -1;
                            }}
                        }}
                    }}
                }}
    """

def get_empty_js(accent):
    return """
                // --- CUSTOM EMTPY VISUALIZER ---
                function updateAndDraw(time) {
                    ctx.fillStyle = 'rgba(255, 255, 255, 0.15)';
                    ctx.font = '8px monospace';
                    ctx.textAlign = 'center';
                    ctx.fillText("[CUSTOM RUNTIME SANDBOX ACTIVE]", canvas.width / 2, canvas.height / 2 - 10);
                    ctx.fillText("Time count: " + Math.floor(time * 10) / 10 + "s", canvas.width / 2, canvas.height / 2 + 10);
                }
                btn1.addEventListener('click', () => logEvent("[Action] Control A executed."));
                btn2.addEventListener('click', () => logEvent("[Action] Control B executed."));
                btn3.addEventListener('click', () => logEvent("[Action] Control C executed."));
    """

def generate_html(args):
    accent = args.accent
    keyword = args.keyword.lower()
    title = args.title
    badge = args.badge
    desc = args.description
    eli5 = args.eli5
    senior = args.senior
    
    # Process code parameter (either string or filepath)
    code_content = args.code
    if os.path.exists(code_content):
        with open(code_content, 'r', encoding='utf-8') as f:
            code_content = f.read()
    
    code_content = code_content.replace('<', '&lt;').replace('>', '&gt;')

    # Load proper visualizer JS
    if args.canvas == 'nodes':
        visualizer_js = get_nodes_js(accent)
    elif args.canvas == 'particles':
        visualizer_js = get_particles_js(accent)
    elif args.canvas == 'lattice':
        visualizer_js = get_lattice_js(accent)
    else:
        visualizer_js = get_empty_js(accent)

    # Compile the final layout
    html = f"""<div class="kain-doc-container font-mono text-gray-300">
    <style>
        .kain-doc-container {{
            max-width: 900px;
            margin: 0 auto;
            color: #d1d5db;
        }}
        .kain-doc-header {{
            border-bottom: 2px solid rgba(0, 255, 204, 0.2);
            padding-bottom: 1.5rem;
            margin-bottom: 2rem;
        }}
        .kain-neon-title {{
            font-size: 2.25rem;
            font-weight: 900;
            color: #ffffff;
            text-shadow: 0 0 10px rgba(0, 255, 204, 0.2);
            margin: 0;
            letter-spacing: -0.025em;
        }}
        .kain-badge {{
            display: inline-block;
            border: 1px solid rgba(0, 255, 204, 0.3);
            background: rgba(0, 255, 204, 0.05);
            color: {accent};
            font-size: 0.75rem;
            font-weight: bold;
            text-transform: uppercase;
            padding: 0.25rem 0.75rem;
            border-radius: 4px;
            letter-spacing: 0.1em;
            margin-top: 0.5rem;
        }}
        .kain-doc-description {{
            font-size: 0.875rem;
            color: #9ca3af;
            line-height: 1.6;
            margin-top: 1rem;
            border-left: 3px solid {accent};
            padding-left: 1rem;
            background: rgba(0, 255, 204, 0.01);
        }}
        .kain-section-title {{
            font-size: 1.25rem;
            font-weight: 800;
            color: #ffffff;
            border-bottom: 1px solid rgba(255, 255, 255, 0.05);
            padding-bottom: 0.5rem;
            margin-top: 2.5rem;
            margin-bottom: 1rem;
            display: flex;
            align-items: center;
            gap: 0.5rem;
        }}
        .kain-comparison-box {{
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 1.5rem;
            margin: 1.5rem 0;
        }}
        @media (max-width: 768px) {{
            .kain-comparison-box {{
                grid-template-columns: 1fr;
            }}
        }}
        .kain-comp-card {{
            background: #020406;
            border: 1px solid rgba(255, 255, 255, 0.03);
            border-radius: 12px;
            padding: 1.25rem;
            border-left: 4px solid #4b5563;
        }}
        .kain-comp-card.eli5 {{
            border-left-color: #3b82f6;
            background: rgba(59, 130, 246, 0.01);
        }}
        .kain-comp-card.senior {{
            border-left-color: {accent};
            background: rgba(0, 255, 204, 0.01);
        }}
        .kain-card-title {{
            font-size: 0.85rem;
            font-weight: bold;
            text-transform: uppercase;
            letter-spacing: 0.05em;
            margin-bottom: 0.75rem;
            color: #ffffff;
        }}
        .kain-card-content {{
            font-size: 0.75rem;
            line-height: 1.6;
            color: #9ca3af;
        }}
        .kain-card-content strong {{
            color: #ffffff;
        }}
        .kain-code-block {{
            position: relative;
            background: #000000;
            border: 1px solid rgba(0, 255, 204, 0.15);
            border-radius: 12px;
            padding: 1.25rem;
            margin: 1.5rem 0;
            overflow-x: auto;
        }}
        .kain-code-label {{
            position: absolute;
            top: 6px;
            right: 12px;
            font-size: 8px;
            color: #4b5563;
            text-transform: uppercase;
            letter-spacing: 0.1em;
        }}
        pre, code {{
            font-family: inherit;
            font-size: 0.75rem;
            line-height: 1.5;
            color: {accent};
        }}
        .kain-interactive-sandbox {{
            background: #020406;
            border: 1px solid rgba(0, 255, 204, 0.2);
            border-radius: 12px;
            padding: 1.5rem;
            margin: 1.5rem 0;
            box-shadow: inset 0 2px 10px rgba(0,0,0,0.8);
            display: grid;
            grid-template-columns: 3fr 2fr;
            gap: 1.5rem;
        }}
        @media (max-width: 768px) {{
            .kain-interactive-sandbox {{
                grid-template-columns: 1fr;
            }}
        }}
        .kain-lattice-screen {{
            position: relative;
            background: #000000;
            border: 1px solid rgba(0, 255, 204, 0.15);
            border-radius: 8px;
            height: 220px;
            overflow: hidden;
        }}
        .kain-sandbox-controls {{
            display: flex;
            flex-direction: column;
            gap: 0.75rem;
            justify-content: center;
        }}
        .kain-btn {{
            background: rgba(0, 255, 204, 0.1);
            border: 1px solid rgba(0, 255, 204, 0.3);
            color: {accent};
            font-family: inherit;
            font-size: 0.7rem;
            font-weight: bold;
            padding: 0.65rem 1rem;
            border-radius: 6px;
            cursor: pointer;
            transition: all 0.2s ease;
            text-transform: uppercase;
            letter-spacing: 0.05em;
            text-align: center;
        }}
        .kain-btn:hover {{
            background: rgba(0, 255, 204, 0.25);
            border-color: {accent};
            transform: translateY(-1px);
        }}
        .kain-terminal-log {{
            background: #000000;
            border: 1px solid rgba(255, 255, 255, 0.05);
            border-radius: 6px;
            padding: 0.75rem;
            height: 80px;
            overflow-y: auto;
            font-size: 0.6rem;
            color: #888888;
            line-height: 1.4;
        }}
        .kain-terminal-line {{
            margin-bottom: 0.25rem;
        }}
        .kain-terminal-line.success {{
            color: {accent};
        }}
    </style>

    <div class="kain-doc-header">
        <div class="flex items-center justify-between">
            <h1 class="kain-neon-title">{title}</h1>
            <span class="kain-badge">{badge}</span>
        </div>
        <p class="kain-doc-description">
            {desc}
        </p>
    </div>

    <div class="kain-comparison-box">
        <div class="kain-comp-card eli5">
            <div class="kain-card-title" style="color: #3b82f6;">Explain Like I'm 5 🧸</div>
            <div class="kain-card-content">
                {eli5}
            </div>
        </div>

        <div class="kain-comp-card senior">
            <div class="kain-card-title" style="color: {accent};">Senior Architect Definition ⚙️</div>
            <div class="kain-card-content">
                {senior}
            </div>
        </div>
    </div>

    <h2 class="kain-section-title"><span>✦</span> Interactive Semantics Sandbox</h2>
    <p class="text-xs text-gray-400 mb-4">
        Interact with the live <code>{keyword}</code> simulation below to see real-time state bounds and execution tracking from the virtual runtime scheduler.
    </p>

    <div class="kain-interactive-sandbox">
        <div class="kain-lattice-screen">
            <canvas id="{keyword}-canvas" style="width: 100%; height: 100%; display: block;"></canvas>
        </div>

        <div class="kain-sandbox-controls">
            <button id="btn-control-1" class="kain-btn">Trigger Surge</button>
            <button id="btn-control-2" class="kain-btn">Perturb Topology</button>
            <button id="btn-control-3" class="kain-btn">Sync Handshake</button>
            
            <div class="kain-terminal-log" id="{keyword}-log">
                <div class="kain-terminal-line success">[SYSTEM] virtual runtime scheduler initialised.</div>
                <div class="kain-terminal-line">[{keyword.upper()}] dynamic sandbox monitoring active.</div>
            </div>
        </div>
    </div>

    <h2 class="kain-section-title"><span>✦</span> Direct Kain Syntax Blueprint</h2>
    <p class="text-xs text-gray-400 mb-4">
        This is how you leverage the <code>{keyword}</code> keyword in standard Kain-Lang source code:
    </p>

    <div class="kain-code-block">
        <div class="kain-code-label">{keyword}_usage.kn</div>
        <pre><code>{code_content}</code></pre>
    </div>

    <script>
        (function() {{
            setTimeout(() => {{
                const canvas = document.getElementById('{keyword}-canvas');
                const btn1 = document.getElementById('btn-control-1');
                const btn2 = document.getElementById('btn-control-2');
                const btn3 = document.getElementById('btn-control-3');
                const logBox = document.getElementById('{keyword}-log');

                if (!canvas || !btn1 || !btn2 || !btn3 || !logBox) {{
                    console.error('[{keyword.upper()} Doc] Failed to find required DOM elements!');
                    return;
                }}

                const ctx = canvas.getContext('2d');

                function logEvent(text, isSuccess = false) {{
                    const line = document.createElement('div');
                    line.className = 'kain-terminal-line' + (isSuccess ? ' success' : '');
                    line.innerText = `[${{new Date().toLocaleTimeString()}}] ${{text}}`;
                    logBox.appendChild(line);
                    logBox.scrollTop = logBox.scrollHeight;
                }}

                // Handle resize
                const resizeObserver = new ResizeObserver(entries => {{
                    if (!canvas.isConnected) return;
                    for (let entry of entries) {{
                        canvas.width = entry.contentRect.width;
                        canvas.height = entry.contentRect.height;
                    }}
                }});
                if (canvas.parentElement) {{
                    resizeObserver.observe(canvas.parentElement);
                }}

                {visualizer_js}

                let time = 0;
                function frame() {{
                    if (!canvas.isConnected) {{
                        resizeObserver.disconnect();
                        return;
                    }}
                    requestAnimationFrame(frame);
                    time += 0.02;

                    // Clear black overlay
                    ctx.fillStyle = '#000000';
                    ctx.fillRect(0, 0, canvas.width, canvas.height);

                    // Grid backdrop
                    ctx.strokeStyle = '#051208';
                    ctx.lineWidth = 1;
                    const gridSpacing = 20;
                    for (let x = 0; x < canvas.width; x += gridSpacing) {{
                        ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, canvas.height); ctx.stroke();
                    }}
                    for (let y = 0; y < canvas.height; y += gridSpacing) {{
                        ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(canvas.width, y); ctx.stroke();
                    }}

                    // Draw inner visualizer
                    updateAndDraw(time);
                }}

                frame();
            }}, 50);
        }})();
    </script>
</div>
"""
    return html

def main():
    parser = argparse.ArgumentParser(description="Kain-Lang Interactive Docs & Simulator Generator")
    parser.add_argument("-k", "--keyword", required=True, help="Name of the keyword (e.g. entangle)")
    parser.add_argument("-t", "--title", required=True, help="Title displayed on the page")
    parser.add_argument("-b", "--badge", default="semantics // advanced", help="Header badge string")
    parser.add_argument("-a", "--accent", default="#00FFCC", help="Neon accent hex color (e.g. #00FF66)")
    parser.add_argument("-d", "--description", required=True, help="High level summary text")
    parser.add_argument("-e", "--eli5", required=True, help="Explain like I'm 5 explanation")
    parser.add_argument("-s", "--senior", required=True, help="Senior Architect definition")
    parser.add_argument("-c", "--code", required=True, help="Direct code text or path to a .kn file")
    parser.add_argument("-v", "--canvas", choices=["particles", "nodes", "lattice", "empty"], default="particles", help="Boilerplate visualizer style")
    parser.add_argument("-o", "--output", help="Optional output filepath. Defaults to website/public/docs/<keyword>.html")

    args = parser.parse_args()

    # Determine default output
    output_path = args.output
    if not output_path:
        # Default to website/public/docs/<keyword>.html
        script_dir = os.path.dirname(os.path.abspath(__file__))
        output_path = os.path.join(script_dir, "website", "public", "docs", f"{args.keyword.lower()}.html")

    html_content = generate_html(args)
    
    # Ensure directory exists
    os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)
    
    with open(output_path, "w", encoding="utf-8") as f:
        f.write(html_content)
        
    print(f"⚡ Success! Interactive simulator doc generated at: {output_path}")

if __name__ == "__main__":
    main()
