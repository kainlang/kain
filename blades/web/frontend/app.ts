/**
 * Kauri Example App — TypeScript source.
 *
 * A simple counter + echo + telemetry dashboard that talks
 * to the Kain backend via the KauriClient bridge.
 *
 * Compile with: tsc frontend/*.ts --outDir frontend/ --target ES2020 --module ES2022
 * Or just use the pre-compiled app.js directly (no build step).
 */

import { kauri, KauriClient, TelemetryData, CounterData } from './kauri-client.js';

// -- DOM refs --
const counterEl = document.getElementById('counter') as HTMLElement;
const counterStatus = document.getElementById('counter-status') as HTMLElement;
const echoInput = document.getElementById('echo-input') as HTMLInputElement;
const echoResult = document.getElementById('echo-result') as HTMLElement;

const tReqs = document.getElementById('t-reqs') as HTMLElement;
const tConns = document.getElementById('t-conns') as HTMLElement;
const tWorkers = document.getElementById('t-workers') as HTMLElement;
const tRestarts = document.getElementById('t-restarts') as HTMLElement;
const tPatches = document.getElementById('t-patches') as HTMLElement;

// -- Update counter display --
async function updateCounter(): Promise<void> {
    try {
        const data = await kauri.getCounter();
        counterEl.textContent = String(data.counter ?? 0);
        counterStatus.textContent = `epoch ${data.epoch ?? 0}`;
        counterStatus.className = 'status ok';
    } catch (err) {
        counterStatus.textContent = `Error: ${(err as Error).message}`;
        counterStatus.className = 'status error';
    }
}

// -- Increment action --
async function handleIncrement(amount: number = 1): Promise<void> {
    try {
        const result = await kauri.increment(amount);
        counterStatus.textContent = `+${result.incremented ?? amount}`;
        counterStatus.className = 'status ok';
        await updateCounter();
    } catch (err) {
        counterStatus.textContent = `Error: ${(err as Error).message}`;
        counterStatus.className = 'status error';
    }
}

// -- Echo action --
async function handleEcho(): Promise<void> {
    const text = echoInput.value || 'hello from kauri';
    try {
        const result = await kauri.echo(text);
        echoResult.textContent = JSON.stringify(result, null, 2);
    } catch (err) {
        echoResult.textContent = `Error: ${(err as Error).message}`;
    }
}

// -- Update telemetry --
async function updateTelemetry(): Promise<void> {
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
document.getElementById('btn-increment')!.addEventListener('click', () => handleIncrement(1));
document.getElementById('btn-increment-10')!.addEventListener('click', () => handleIncrement(10));
document.getElementById('btn-decrement')!.addEventListener('click', () => handleIncrement(-1));
document.getElementById('btn-reset')!.addEventListener('click', async () => {
    try {
        await kauri.post('/api/config', { counter: 0 });
        await updateCounter();
    } catch (err) {
        counterStatus.textContent = `Error: ${(err as Error).message}`;
        counterStatus.className = 'status error';
    }
});
document.getElementById('btn-echo')!.addEventListener('click', handleEcho);
echoInput.addEventListener('keydown', (e: KeyboardEvent) => {
    if (e.key === 'Enter') handleEcho();
});

// -- Init --
async function init(): Promise<void> {
    await updateCounter();
    await updateTelemetry();
    setInterval(updateTelemetry, 2000);
}

init().catch(console.error);
