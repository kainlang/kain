import { h } from "preact";
import { useEffect, useMemo, useState } from "preact/hooks";

import type { KainStatusDescriptor, KainStatusService } from "../lib/kain_site_data";
import { fetchJson } from "../lib/kain_site_data";

type Props = {
  status: KainStatusDescriptor | null;
  endpoint?: string;
  refreshMs?: number;
};

function normalizeServices(status: KainStatusDescriptor | null): KainStatusService[] {
  return (status?.services || []).filter(Boolean).map((service) => ({
    name: service.name,
    status: service.status || "unknown",
    detail: service.detail || null,
    uptime: service.uptime || null
  }));
}

export function StatusWatchIsland(props: Props) {
  const [status, setStatus] = useState<KainStatusDescriptor | null>(props.status || null);
  const services = useMemo(() => normalizeServices(status), [status]);
  const endpoint = props.endpoint || "/api/status";
  const refreshMs = props.refreshMs ?? 20000;

  useEffect(() => {
    let mounted = true;
    let timer: number | undefined;

    const load = async () => {
      try {
        const next = await fetchJson<KainStatusDescriptor>(endpoint);
        if (mounted) setStatus(next);
      } catch {
        if (mounted) setStatus((current) => current || props.status || null);
      }
    };

    void load();
    if (refreshMs > 0) {
      timer = window.setInterval(() => void load(), refreshMs);
    }

    return () => {
      mounted = false;
      if (timer) window.clearInterval(timer);
    };
  }, [endpoint, refreshMs, props.status]);

  return (
    <div class="kain-island kain-island-status">
      <div class="kain-island-header">
        <p class="kain-island-eyebrow">Status</p>
        <h3 class="kain-island-title">Live runtime status</h3>
        <p class="kain-island-copy">
          This island pulls `/api/status` so local operator dashboards can keep runtime health visible.
        </p>
      </div>
      <div class="kain-island-body">
        {services.length ? (
          <div class="kain-status-grid">
            {services.map((service) => (
              <article class={`kain-status-card ${service.status || "unknown"}`} key={service.name}>
                <p class="kain-status-label">{service.status}</p>
                <h4>{service.name}</h4>
                {service.detail ? <p>{service.detail}</p> : null}
                {service.uptime ? <p class="kain-status-meta">uptime {service.uptime}</p> : null}
              </article>
            ))}
          </div>
        ) : (
          <p class="kain-island-muted">No status services defined.</p>
        )}
      </div>
    </div>
  );
}
