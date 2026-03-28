import { h } from "preact";
import { useEffect, useMemo, useState } from "preact/hooks";

import type { KainCardEntry } from "../lib/kain_site_data";
import { normalizeSelectionLabel } from "../lib/kain_script_bridge.ks";

type StackGroup = {
  id: string;
  label: string;
  entries: KainCardEntry[];
};

type Props = {
  uiRuntime?: KainCardEntry[];
  uiState?: KainCardEntry[];
  uiRouting?: KainCardEntry[];
  uiData?: KainCardEntry[];
  uiForms?: KainCardEntry[];
  uiMotion?: KainCardEntry[];
  uiTesting?: KainCardEntry[];
  uiTooling?: KainCardEntry[];
};

function normalizeStack(entries?: KainCardEntry[]): KainCardEntry[] {
  return (entries || []).filter((entry) => entry && (entry.title || entry.kicker || entry.body || entry.summary));
}

export function UiStacksIsland(props: Props) {
  const stacks = useMemo<StackGroup[]>(
    () => [
      { id: "runtime", label: "UI Runtime", entries: normalizeStack(props.uiRuntime) },
      { id: "state", label: "State", entries: normalizeStack(props.uiState) },
      { id: "routing", label: "Routing", entries: normalizeStack(props.uiRouting) },
      { id: "data", label: "Data", entries: normalizeStack(props.uiData) },
      { id: "forms", label: "Forms", entries: normalizeStack(props.uiForms) },
      { id: "motion", label: "Motion", entries: normalizeStack(props.uiMotion) },
      { id: "testing", label: "Testing", entries: normalizeStack(props.uiTesting) },
      { id: "tooling", label: "Tooling", entries: normalizeStack(props.uiTooling) }
    ],
    [
      props.uiRuntime,
      props.uiState,
      props.uiRouting,
      props.uiData,
      props.uiForms,
      props.uiMotion,
      props.uiTesting,
      props.uiTooling
    ]
  );

  const initialStack = stacks.find((stack) => stack.entries.length > 0) || stacks[0];
  const [activeId, setActiveId] = useState<string>(initialStack?.id || "runtime");

  useEffect(() => {
    if (stacks.some((stack) => stack.id === activeId)) return;
    setActiveId(stacks[0]?.id || "runtime");
  }, [activeId, stacks]);

  const active = stacks.find((stack) => stack.id === activeId) || stacks[0];

  return (
    <div class="kain-island kain-island-ui-stack">
      <div class="kain-island-header">
        <p class="kain-island-eyebrow">UI systems</p>
        <h3 class="kain-island-title">React/TypeScript-style stack inventory</h3>
        <p class="kain-island-copy">
          Each stack below is manifest-driven so the UI runtime, routing, state, data, forms, motion, testing, and
          tooling layers stay explicit before you wire real implementation adapters.
        </p>
      </div>
      <div class="kain-island-tabs">
        {stacks.map((stack) => {
          const label = normalizeSelectionLabel(stack.label || stack.id);
          const isActive = stack.id === active?.id;
          return (
            <button
              class={isActive ? "kain-island-tab active" : "kain-island-tab"}
              type="button"
              onClick={() => setActiveId(stack.id)}
              key={stack.id}
            >
              {label}
            </button>
          );
        })}
      </div>
      <div class="kain-island-body">
        {active && active.entries.length > 0 ? (
          <div class="kain-ui-stack-grid">
            {active.entries.map((entry, index) => (
              <article class="kain-ui-stack-card" key={`${active.id}-${index}`}>
                <p class="kain-ui-stack-kicker">{entry.kicker || active.label}</p>
                <h4>{entry.title || entry.kicker || `Stack ${index + 1}`}</h4>
                <p>{entry.body || entry.summary || "Describe the stack component or lane."}</p>
              </article>
            ))}
          </div>
        ) : (
          <p class="kain-island-panel-hint">No entries recorded for this stack yet.</p>
        )}
      </div>
    </div>
  );
}
