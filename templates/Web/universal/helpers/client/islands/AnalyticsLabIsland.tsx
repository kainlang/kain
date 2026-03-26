import { h } from "preact";
import { useEffect, useState } from "preact/hooks";

type AnalyticsEvent = {
  received_at?: string | null;
  name?: string;
  path?: string | null;
  client_at?: string | null;
  properties?: Record<string, unknown>;
  session?: { id: string; email: string | null } | null;
  raw?: string;
};

type AnalyticsListResponse = {
  ok: boolean;
  items: AnalyticsEvent[];
};

async function fetchEvents(limit = 30): Promise<AnalyticsListResponse> {
  const url = new URL("/api/analytics/events", window.location.href);
  url.searchParams.set("limit", String(limit));
  const response = await fetch(url.toString(), { headers: { accept: "application/json" } });
  if (!response.ok) throw new Error(`events fetch failed: ${response.status}`);
  return (await response.json()) as AnalyticsListResponse;
}

async function postEvent(name: string, properties: Record<string, unknown>): Promise<{ ok: boolean }> {
  const response = await fetch("/api/analytics/event", {
    method: "POST",
    headers: { "content-type": "application/json", accept: "application/json" },
    body: JSON.stringify({
      name,
      path: window.location.pathname,
      client_at: new Date().toISOString(),
      properties
    })
  });
  if (!response.ok) throw new Error(`event post failed: ${response.status}`);
  return (await response.json()) as { ok: boolean };
}

export function AnalyticsLabIsland() {
  const [items, setItems] = useState<AnalyticsEvent[]>([]);
  const [status, setStatus] = useState<string | null>(null);

  const refresh = async () => {
    try {
      setStatus("loading");
      const payload = await fetchEvents(30);
      setItems(payload.items || []);
      setStatus(`loaded ${payload.items?.length || 0}`);
    } catch (error) {
      setStatus((error as Error).message || "error");
    }
  };

  const sendPing = async () => {
    try {
      setStatus("posting");
      await postEvent("kain.template.ping", { tag: "manual", tick: Date.now() });
      await refresh();
    } catch (error) {
      setStatus((error as Error).message || "error");
    }
  };

  useEffect(() => {
    void refresh();
  }, []);

  return (
    <div class="kain-island kain-island-analytics">
      <div class="kain-island-header">
        <p class="kain-island-eyebrow">Analytics</p>
        <h3 class="kain-island-title">Event capture (JSONL)</h3>
        <p class="kain-island-copy">
          Writes events to the local runtime folder so operator dashboards can reason about usage before wiring external
          analytics.
        </p>
        <div class="kain-island-actions">
          <button type="button" onClick={() => void sendPing()}>
            Emit ping event
          </button>
          <button type="button" onClick={() => void refresh()}>
            Refresh
          </button>
          <span class="kain-island-status">{status || "idle"}</span>
        </div>
      </div>
      <div class="kain-analytics-log">
        {items.length ? (
          items
            .slice()
            .reverse()
            .map((entry, index) => (
              <article class="kain-analytics-row" key={index}>
                <p class="kain-analytics-kicker">{entry.name || "event"}</p>
                <p class="kain-analytics-copy">
                  {[entry.received_at, entry.path, entry.session?.email].filter(Boolean).join(" · ") || entry.raw}
                </p>
              </article>
            ))
        ) : (
          <p class="kain-island-muted">No events captured yet.</p>
        )}
      </div>
    </div>
  );
}

