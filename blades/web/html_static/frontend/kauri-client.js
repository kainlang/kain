/**
 * Kauri Client — Bridge between TypeScript frontend and Kain backend.
 *
 * Zero dependencies. Uses fetch() over HTTP to communicate with the
 * Kain HTTP server running on localhost.
 *
 * The Kain backend has http_route_actor endpoints registered:
 *   GET  /api/counter    → actor reads entangled mirror
 *   POST /api/increment  → actor patches world state
 *   POST /api/echo       → actor echoes back
 *   GET  /api/telemetry  → actor collects runtime counters
 *   POST /api/config     → actor patches config
 *   POST /api/process    → actor runs shell command
 *   GET  /*              → actor serves static files
 *
 * Usage:
 *   import { kauri } from './kauri-client.js';
 *   const data = await kauri.get('/api/counter');
 *   const result = await kauri.post('/api/increment', { amount: '1' });
 */

const KAURI_DEFAULTS = {
    baseUrl: 'http://127.0.0.1:9090',
    timeoutMs: 5000,
};

class KauriClient {
    constructor(opts = {}) {
        this.baseUrl = opts.baseUrl || KAURI_DEFAULTS.baseUrl;
        this.timeoutMs = opts.timeoutMs || KAURI_DEFAULTS.timeoutMs;
    }

    async request(method, path, body = null) {
        const url = `${this.baseUrl}${path}`;
        const controller = new AbortController();
        const timeoutId = setTimeout(() => controller.abort(), this.timeoutMs);

        try {
            const options = {
                method,
                headers: { 'Content-Type': 'application/json' },
                signal: controller.signal,
            };

            if (body !== null) {
                options.body = typeof body === 'string' ? body : JSON.stringify(body);
            }

            const response = await fetch(url, options);

            if (!response.ok) {
                const text = await response.text();
                throw new Error(`HTTP ${response.status}: ${text.slice(0, 200)}`);
            }

            const text = await response.text();
            try {
                return JSON.parse(text);
            } catch {
                return text;
            }
        } catch (err) {
            if (err.name === 'AbortError') {
                throw new Error(`Request to ${path} timed out after ${this.timeoutMs}ms`);
            }
            throw err;
        } finally {
            clearTimeout(timeoutId);
        }
    }

    // -- Convenience methods --

    async get(path) {
        return this.request('GET', path);
    }

    async post(path, data = null) {
        return this.request('POST', path, data);
    }

    // -- Domain-specific API --

    /** Read the current counter from the entangled world mirror */
    async getCounter() {
        return this.get('/api/counter');
    }

    /** Increment the counter by N */
    async increment(amount = 1) {
        return this.post('/api/increment', { amount: String(amount) });
    }

    /** Echo text back from the Kain backend */
    async echo(text) {
        return this.post('/api/echo', text);
    }

    /** Get full runtime telemetry snapshot */
    async getTelemetry() {
        return this.get('/api/telemetry');
    }
}

// Singleton for the app
export const kauri = new KauriClient();
