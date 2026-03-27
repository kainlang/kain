import { h } from "preact";
import { useMemo, useState } from "preact/hooks";

import type { KainCardEntry } from "../lib/kain_site_data";

type UiKitTab = "components" | "layouts" | "tokens";

type Props = {
  components?: KainCardEntry[];
  layouts?: KainCardEntry[];
  tokens?: KainCardEntry[];
};

const TAB_LABELS: Record<UiKitTab, string> = {
  components: "Components",
  layouts: "Layouts",
  tokens: "Tokens"
};

function normalizeEntries(entries?: KainCardEntry[]): KainCardEntry[] {
  return (entries || []).filter(Boolean);
}

export function UiKitIsland(props: Props) {
  const [tab, setTab] = useState<UiKitTab>("components");
  const components = normalizeEntries(props.components);
  const layouts = normalizeEntries(props.layouts);
  const tokens = normalizeEntries(props.tokens);
  const activeEntries = useMemo(() => {
    if (tab === "layouts") return layouts;
    if (tab === "tokens") return tokens;
    return components;
  }, [components, layouts, tokens, tab]);

  return (
    <div class="kain-island kain-island-ui-kit">
      <div class="kain-island-header">
        <p class="kain-island-eyebrow">UI Kit</p>
        <h3 class="kain-island-title">React-style UI primitives, layouts, and tokens</h3>
        <p class="kain-island-copy">
          The UI kit is manifest-driven. Pair these with `ui.schema.json` and module routes to wire a full React/TS
          application shell without authoring boilerplate site code.
        </p>
      </div>
      <div class="kain-island-tabs">
        {(Object.keys(TAB_LABELS) as UiKitTab[]).map((key) => (
          <button
            type="button"
            class={tab === key ? "kain-island-tab active" : "kain-island-tab"}
            onClick={() => setTab(key)}
          >
            {TAB_LABELS[key]}
          </button>
        ))}
      </div>
      <div class="kain-ui-kit-grid">
        {activeEntries.length ? (
          activeEntries.map((entry, index) => (
            <article class="kain-ui-kit-card" key={`${entry.title || entry.kicker || "ui"}-${index}`}>
              {entry.kicker ? <p class="kain-island-panel-kicker">{entry.kicker}</p> : null}
              <h4>{entry.title || "Untitled"}</h4>
              <p>{entry.body || entry.summary || "Describe this UI primitive in the manifest."}</p>
            </article>
          ))
        ) : (
          <article class="kain-ui-kit-card">
            <h4>No UI kit entries yet</h4>
            <p>Add entries to `ui_components`, `ui_layouts`, or `ui_tokens` in the content manifests.</p>
          </article>
        )}
      </div>
    </div>
  );
}
