import { h } from "preact";
import { useMemo, useState } from "preact/hooks";

import type { KainAppModule } from "../lib/kain_site_data";

type Props = {
  modules: KainAppModule[];
};

function normalizeTags(tags: string[] | null | undefined): string[] {
  return (tags || []).filter(Boolean).map((value) => String(value));
}

export function AppShellIsland(props: Props) {
  const modules = props.modules || [];
  const [active, setActive] = useState(modules[0]?.route || modules[0]?.name || "Overview");

  const activeModule = useMemo(() => {
    return modules.find((entry) => entry.route === active || entry.name === active) || modules[0] || null;
  }, [active, modules]);

  return (
    <div class="kain-island kain-island-app-shell">
      <div class="kain-island-header">
        <p class="kain-island-eyebrow">App Shell</p>
        <h3 class="kain-island-title">React/TypeScript-style workspace navigation</h3>
        <p class="kain-island-copy">
          This island turns manifest module entries into a client-side shell with selection state and route-ready
          metadata. Extend this with real data loaders, auth gates, and per-module actors.
        </p>
      </div>
      <div class="kain-island-body">
        <nav class="kain-island-tabs" aria-label="Workspace modules">
          {modules.map((entry) => {
            const key = entry.route || entry.name;
            const isActive = key === active;
            return (
              <button
                type="button"
                class={isActive ? "kain-island-tab active" : "kain-island-tab"}
                onClick={() => setActive(key)}
              >
                {entry.name}
              </button>
            );
          })}
        </nav>
        <section class="kain-island-panel" aria-live="polite">
          {activeModule ? (
            <div>
              <p class="kain-island-panel-kicker">{activeModule.route || "module"}</p>
              <h4 class="kain-island-panel-title">{activeModule.name}</h4>
              {activeModule.summary ? <p class="kain-island-panel-copy">{activeModule.summary}</p> : null}
              {normalizeTags(activeModule.tags).length ? (
                <p class="kain-island-panel-tags">{normalizeTags(activeModule.tags).join(" / ")}</p>
              ) : null}
              <div class="kain-island-panel-hint">
                <p>
                  Next step: map module routes to actor endpoints (or static bundles) and use `ui.schema.json` +
                  `system.contract.json` to keep contracts inspectable.
                </p>
              </div>
            </div>
          ) : (
            <p class="kain-island-muted">No modules defined in this experience.</p>
          )}
        </section>
      </div>
    </div>
  );
}
