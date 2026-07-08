/**
 * Kauri Client — TypeScript source.
 *
 * Bridge between a TypeScript/React frontend and the Kain backend.
 * Communication is via HTTP/JSON over localhost.
 *
 * The Kain backend registers http_route_actor endpoints:
 *   GET  /api/counter    → entangled world mirror read
 *   POST /api/increment  → patch world state
 *   POST /api/echo       → echo back
 *   GET  /api/telemetry  → runtime counters
 *   POST /api/config     → world config patch
 *   POST /api/process    → shell command
 *   GET  /*              → static file server
 *
 * Usage:
 *   import { kauri } from './kauri-client.js';
 *   const data = await kauri.get('/api/counter');
 */

interface KauriClientOptions {
    baseUrl?: string;
    timeoutMs?: number;
}

interface CounterData {
    counter: number;
    connections: number;
    epoch: number;
}

interface IncrementResult {
    counter: number;
    incremented: number;
}

interface EchoResult {
    echo: string;
    server: string;
}

interface TelemetryData {
    uptime_seconds: number;
    router_hits: number;
    active_connections: number;
    busy_workers: number;
    worker_count: number;
    restart_count: number;
    patch_count: number;
    entangle_count: number;
    epoch: number;
}

const KAURI_DEFAULTS: Required<KauriClientOptions> = {
    baseUrl: 'http://127.0.0.1:9090',
    timeoutMs: 5000,
};

export class KauriClient {
    private baseUrl: string;
    private timeoutMs: number;

    constructor(opts: KauriClientOptions = {}) {
        this.baseUrl = opts.baseUrl || KAURI_DEFAULTS.baseUrl;
        this.timeoutMs = opts.timeoutMs || KAURI_DEFAULTS.timeoutMs;
    }

    // -- Generic HTTP request --

    async request<T = any>(method: string, path: string, body: any = null): Promise<T> {
        const url = `${this.baseUrl}${path}`;
        const controller = new AbortController();
        const timeoutId = setTimeout(() => controller.abort(), this.timeoutMs);

        try {
            const options: RequestInit = {
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
                return JSON.parse(text) as T;
            } catch {
                return text as unknown as T;
            }
        } catch (err) {
            if ((err as Error).name === 'AbortError') {
                throw new Error(`Request to ${path} timed out after ${this.timeoutMs}ms`);
            }
            throw err;
        } finally {
            clearTimeout(timeoutId);
        }
    }

    // -- Convenience methods --

    async get<T = any>(path: string): Promise<T> {
        return this.request<T>('GET', path);
    }

    async post<T = any>(path: string, data: any = null): Promise<T> {
        return this.request<T>('POST', path, data);
    }

    // -- Domain-specific API --

    /** Read the current counter from the entangled world mirror */
    async getCounter(): Promise<CounterData> {
        return this.get<CounterData>('/api/counter');
    }

    /** Increment the counter by N */
    async increment(amount: number = 1): Promise<IncrementResult> {
        return this.post<IncrementResult>('/api/increment', { amount: String(amount) });
    }

    /** Echo text back from the Kain backend */
    async echo(text: string): Promise<EchoResult> {
        return this.post<EchoResult>('/api/echo', text);
    }

    /** Get full runtime telemetry snapshot */
    async getTelemetry(): Promise<TelemetryData> {
        return this.get<TelemetryData>('/api/telemetry');
    }
}

/** Singleton for the app */
export const kauri = new KauriClient();
