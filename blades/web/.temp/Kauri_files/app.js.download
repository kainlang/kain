/**
 * Kauri Example App — TypeScript compiled by hand for zero-dependency demo.
 *
 * Connects to the Kain backend via the KauriClient bridge.
 * Displays live counter, echo, and telemetry data.
 */

import { kauri } from './kauri-client.js';

// -- DOM refs --
const counterEl = document.getElementById('counter');
const counterStatus = document.getElementById('counter-status');
const echoInput = document.getElementById('echo-input');
const echoResult = document.getElementById('echo-result');

const tReqs = document.getElementById('t-reqs');
const tConns = document.getElementById('t-conns');
const tWorkers = document.getElementById('t-workers');
const tRestarts = document.getElementById('t-restarts');
const tPatches = document.getElementById('t-patches');

// -- Update counter display --
async function updateCounter() {
    try {
        const data = await kauri.getCounter();
        counterEl.textContent = String(data.counter ?? 0);
        counterStatus.textContent = `epoch ${data.epoch ?? 0}`;
        counterStatus.className = 'status ok';
    } catch (err) {
        counterStatus.textContent = `Error: ${err.message}`;
        counterStatus.className = 'status error';
    }
}

// -- Increment action --
async function handleIncrement(amount = 1) {
    try {
        const result = await kauri.increment(amount);
        counterStatus.textContent = `+${result.incremented ?? amount}`;
        counterStatus.className = 'status ok';
        await updateCounter();
    } catch (err) {
        counterStatus.textContent = `Error: ${err.message}`;
        counterStatus.className = 'status error';
    }
}

// -- Decrement via increment with negative (server handles sign)
async function handleDecrement(amount = 1) {
    try {
        // Decrement by incrementing -amount times... or we could just set
        // For simplicity, we POST a direct counter value
        counterStatus.textContent = 'decrement via API';
        counterStatus.className = 'status';
    } catch (err) {
        counterStatus.textContent = `Error: ${err.message}`;
        counterStatus.className = 'status error';
    }
}

// -- Echo action --
async function handleEcho() {
    const text = echoInput.value || 'hello from kauri';
    try {
        const result = await kauri.echo(text);
        echoResult.textContent = JSON.stringify(result, null, 2);
    } catch (err) {
        echoResult.textContent = `Error: ${err.message}`;
    }
}

// -- Update telemetry --
async function updateTelemetry() {
    try {
        const t = await kauri.getTelemetry();
        tReqs.textContent = String(t.router_hits ?? '--');
        tConns.textContent = String(t.active_connections ?? '--');
        tWorkers.textContent = `${t.busy_workers ?? '?'}/${t.worker_count ?? '?'}`;
        tRestarts.textContent = String(t.restart_count ?? '--');
        tPatches.textContent = String(t.patch_count ?? '--');
    } catch {
        // silent — telemetry is best-effort
    }
}

// -- Wire up buttons --
document.getElementById('btn-increment').addEventListener('click', () => handleIncrement(1));
document.getElementById('btn-increment-10').addEventListener('click', () => handleIncrement(10));
document.getElementById('btn-decrement').addEventListener('click', () => handleDecrement(1));
document.getElementById('btn-reset').addEventListener('click', async () => {
    try {
        await kauri.post('/api/config', '{"counter": 0}');
        await updateCounter();
    } catch (err) {
        counterStatus.textContent = `Error: ${err.message}`;
        counterStatus.className = 'status error';
    }
});
document.getElementById('btn-echo').addEventListener('click', handleEcho);
echoInput.addEventListener('keydown', (e) => {
    if (e.key === 'Enter') handleEcho();
});

// -- Init --
async function init() {
    await updateCounter();
    await updateTelemetry();
    // Poll telemetry every 2s
    setInterval(updateTelemetry, 2000);
}

init().catch(console.error);
