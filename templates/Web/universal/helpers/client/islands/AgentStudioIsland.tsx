import { h } from "preact";
import { useEffect, useMemo, useRef, useState } from "preact/hooks";

import type { KainCardEntry, KainProcessStep } from "../lib/kain_site_data";
import { normalizePrompt, normalizeSelectionLabel } from "../lib/kain_script_bridge.ks";

type AgentMessage = {
  role: "user" | "assistant";
  text: string;
};

type Props = {
  agents?: KainCardEntry[];
  workflows?: KainProcessStep[];
  tools?: KainCardEntry[];
  knowledge?: KainCardEntry[];
  memory?: KainCardEntry[];
  chatEndpoint?: string;
  streamEndpoint?: string;
};

function normalizeMessages(seed: AgentMessage[]): AgentMessage[] {
  return (seed || []).filter((entry) => entry && entry.text);
}

async function requestAgentReply(endpoint: string, prompt: string, agent?: string): Promise<string> {
  const url = new URL(endpoint, window.location.href);
  url.searchParams.set("prompt", prompt);
  if (agent) {
    url.searchParams.set("agent", agent);
  }
  const response = await fetch(url.toString(), { headers: { accept: "application/json" } });
  if (!response.ok) {
    throw new Error(`agent chat failed: ${response.status} ${response.statusText}`);
  }
  const payload = (await response.json()) as { reply?: string; text?: string };
  return payload.reply || payload.text || "Agent routing is ready once you wire a real provider adapter.";
}

export function AgentStudioIsland(props: Props) {
  const agents = props.agents || [];
  const workflows = props.workflows || [];
  const tools = props.tools || [];
  const knowledge = props.knowledge || [];
  const memory = props.memory || [];
  const [agent, setAgent] = useState(() =>
    normalizeSelectionLabel(agents[0]?.title || agents[0]?.kicker || "")
  );
  const [prompt, setPrompt] = useState("");
  const [messages, setMessages] = useState<AgentMessage[]>(() =>
    normalizeMessages([
      { role: "assistant", text: "Agent studio is live. Pick a persona and run a prompt." }
    ])
  );
  const [streaming, setStreaming] = useState(false);
  const logRef = useRef<HTMLDivElement | null>(null);

  useEffect(() => {
    const node = logRef.current;
    if (!node) return;
    node.scrollTop = node.scrollHeight;
  }, [messages]);

  const endpoint = props.chatEndpoint || "/api/chat";
  const streamEndpoint = props.streamEndpoint || "/api/chat/stream";

  const submit = async () => {
    const trimmed = normalizePrompt(prompt);
    if (!trimmed || streaming) return;
    setPrompt("");
    setMessages((prev) => [...prev, { role: "user", text: trimmed }]);

    try {
      setStreaming(true);
      const streamUrl = new URL(streamEndpoint, window.location.href);
      streamUrl.searchParams.set("prompt", trimmed);
      if (agent) {
        streamUrl.searchParams.set("agent", agent);
      }
      const source = new EventSource(streamUrl.toString());
      let collected = "";
      source.addEventListener("token", (event) => {
        collected += String((event as MessageEvent).data || "");
        setMessages((prev) => {
          const next = [...prev];
          const last = next[next.length - 1];
          if (last?.role === "assistant") {
            next[next.length - 1] = { role: "assistant", text: collected };
          } else {
            next.push({ role: "assistant", text: collected });
          }
          return next;
        });
      });
      source.addEventListener("done", () => {
        source.close();
        setStreaming(false);
      });
      source.onerror = async () => {
        source.close();
        setStreaming(false);
        const reply = await requestAgentReply(endpoint, trimmed, agent);
        setMessages((prev) => [...prev, { role: "assistant", text: reply }]);
      };
    } catch (error) {
      setStreaming(false);
      const reply = await requestAgentReply(endpoint, trimmed, agent);
      setMessages((prev) => [...prev, { role: "assistant", text: reply }]);
    }
  };

  return (
    <div class="kain-island kain-island-agent">
      <div class="kain-island-header">
        <p class="kain-island-eyebrow">Agents</p>
        <h3 class="kain-island-title">Agent studio (roster + tools + workflows)</h3>
        <p class="kain-island-copy">
          This island wires agent metadata, workflows, and tools into a live prompt console. It uses the same chat
          endpoints as the chat lab, but scopes prompts to an agent selection for routing.
        </p>
        {agents.length > 0 && (
          <label class="kain-agent-select">
            Active agent
            <select value={agent} onChange={(event) => setAgent(event.currentTarget.value)}>
              {agents.map((entry, index) => {
                const label = normalizeSelectionLabel(entry.title || entry.kicker || `Agent ${index + 1}`);
                return (
                  <option key={label} value={label}>
                    {label}
                  </option>
                );
              })}
            </select>
          </label>
        )}
        <div class="kain-agent-studio-grid">
          {knowledge.length > 0 && (
            <div>
              <p class="kain-agent-studio-title">Knowledge sources</p>
              <div class="kain-agent-studio-list">
                {knowledge.slice(0, 6).map((entry, index) => (
                  <article class="kain-agent-studio-card" key={`knowledge-${index}`}>
                    <h4>{entry.title || entry.kicker || `Source ${index + 1}`}</h4>
                    <p>{entry.body || entry.summary || "Describe the retrieval source."}</p>
                  </article>
                ))}
              </div>
            </div>
          )}
          {memory.length > 0 && (
            <div>
              <p class="kain-agent-studio-title">Memory stores</p>
              <div class="kain-agent-studio-list">
                {memory.slice(0, 6).map((entry, index) => (
                  <article class="kain-agent-studio-card" key={`memory-${index}`}>
                    <h4>{entry.title || entry.kicker || `Memory ${index + 1}`}</h4>
                    <p>{entry.body || entry.summary || "Describe the memory lane."}</p>
                  </article>
                ))}
              </div>
            </div>
          )}
          {tools.length > 0 && (
            <div>
              <p class="kain-agent-studio-title">Tool registry</p>
              <div class="kain-agent-studio-list">
                {tools.slice(0, 6).map((entry, index) => (
                  <article class="kain-agent-studio-card" key={`tool-${index}`}>
                    <h4>{entry.title || entry.kicker || `Tool ${index + 1}`}</h4>
                    <p>{entry.body || entry.summary || "Describe the tool contract."}</p>
                  </article>
                ))}
              </div>
            </div>
          )}
          {workflows.length > 0 && (
            <div>
              <p class="kain-agent-studio-title">Agent workflows</p>
              <div class="kain-agent-studio-list">
                {workflows.slice(0, 6).map((entry, index) => (
                  <article class="kain-agent-studio-card" key={`workflow-${index}`}>
                    <h4>{entry.title || `Workflow ${index + 1}`}</h4>
                    <p>{entry.body || "Describe the orchestration sequence."}</p>
                  </article>
                ))}
              </div>
            </div>
          )}
        </div>
      </div>
      <div class="kain-agent-log" ref={logRef}>
        {messages.map((entry, index) => (
          <article class={entry.role === "user" ? "kain-chat-bubble user" : "kain-chat-bubble assistant"} key={index}>
            <p class="kain-chat-role">{entry.role}</p>
            <p class="kain-chat-text">{entry.text}</p>
          </article>
        ))}
      </div>
      <form
        class="kain-chat-form"
        onSubmit={(event) => {
          event.preventDefault();
          void submit();
        }}
      >
        <input
          name="prompt"
          value={prompt}
          onInput={(event) => setPrompt((event.target as HTMLInputElement).value)}
          placeholder="Route a task to the agent mesh..."
          disabled={streaming}
        />
        <button type="submit" disabled={streaming || !prompt.trim()}>
          {streaming ? "Routing…" : "Route"}
        </button>
      </form>
    </div>
  );
}
