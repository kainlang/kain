import { h } from "preact";
import { useEffect, useMemo, useState } from "preact/hooks";

import type { KainRealtimeChannel } from "../lib/kain_site_data";

type Props = {
  channels: KainRealtimeChannel[];
  streamEndpoint?: string;
};

type StreamPayload = {
  channels: KainRealtimeChannel[];
  at: string;
  tick: number;
};

function normalizeChannels(channels: KainRealtimeChannel[]): KainRealtimeChannel[] {
  return (channels || []).filter((entry) => entry && entry.name);
}

export function RealtimeChannelsIsland(props: Props) {
  const baseChannels = useMemo(() => normalizeChannels(props.channels || []), [props.channels]);
  const [channels, setChannels] = useState(baseChannels);
  const [connected, setConnected] = useState(false);
  const [status, setStatus] = useState<string | null>(null);

  useEffect(() => {
    setChannels(baseChannels);
  }, [baseChannels]);

  const connect = () => {
    if (connected) return;
    const endpoint = props.streamEndpoint || "/api/realtime/stream";
    const source = new EventSource(new URL(endpoint, window.location.href).toString());
    setConnected(true);
    setStatus("connecting");

    source.addEventListener("tick", (event) => {
      try {
        const payload = JSON.parse(String((event as MessageEvent).data || "")) as StreamPayload;
        setChannels(payload.channels || []);
        setStatus(`tick ${payload.tick}`);
      } catch {
        setStatus("tick");
      }
    });

    source.addEventListener("channels", (event) => {
      try {
        const payload = JSON.parse(String((event as MessageEvent).data || "")) as StreamPayload;
        setChannels(payload.channels || []);
        setStatus("channels received");
      } catch {
        setStatus("channels");
      }
    });

    source.onerror = () => {
      source.close();
      setConnected(false);
      setStatus("stream unavailable (static deploy or server offline)");
    };
  };

  return (
    <div class="kain-island kain-island-realtime">
      <div class="kain-island-header">
        <p class="kain-island-eyebrow">Realtime</p>
        <h3 class="kain-island-title">Realtime island (SSE + WS-ready)</h3>
        <p class="kain-island-copy">
          The Node lane exposes `/api/realtime/stream` and `/ws/realtime` for live dashboards. Use manifests to describe
          channels first, then bind them to real transports later.
        </p>
        <div class="kain-island-actions">
          <button type="button" onClick={connect} disabled={connected}>
            {connected ? "Connected" : "Connect stream"}
          </button>
          <span class="kain-island-status">{status || "offline"}</span>
        </div>
      </div>
      <div class="kain-realtime-grid">
        {channels.map((entry, index) => (
          <article class="kain-realtime-card" key={index}>
            <p class="kain-realtime-kicker">{entry.protocol || "channel"}</p>
            <h4 class="kain-realtime-title">{entry.name}</h4>
            {entry.summary ? <p class="kain-realtime-copy">{entry.summary}</p> : null}
            <p class="kain-realtime-meta">
              {[entry.cadence, entry.producer].filter(Boolean).join(" · ") || "manifest contract"}
            </p>
          </article>
        ))}
      </div>
    </div>
  );
}
