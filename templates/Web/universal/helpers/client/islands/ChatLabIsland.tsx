import { h } from "preact";
import { useEffect, useMemo, useRef, useState } from "preact/hooks";

import type { KainCardEntry, KainChatSeedMessage, KainProcessStep } from "../lib/kain_site_data";
import { normalizePrompt, normalizeSelectionLabel } from "../lib/kain_script_bridge.ks";

type ChatMessage = {
  role: "user" | "assistant";
  text: string;
};

type Props = {
  seed: KainChatSeedMessage[];
  personas?: KainCardEntry[];
  modes?: KainCardEntry[];
  agents?: KainCardEntry[];
  playbooks?: KainProcessStep[];
  tools?: KainCardEntry[];
  memory?: KainCardEntry[];
  chatEndpoint?: string;
  streamEndpoint?: string;
};

function normalizeSeed(seed: KainChatSeedMessage[]): ChatMessage[] {
  return (seed || [])
    .filter((entry) => entry && entry.text)
    .map((entry) => ({
      role: entry.role === "user" ? "user" : "assistant",
      text: String(entry.text)
    }));
}

async function requestChatReply(endpoint: string, prompt: string, persona?: string, mode?: string): Promise<string> {
  const url = new URL(endpoint, window.location.href);
  url.searchParams.set("prompt", prompt);
  if (persona) {
    url.searchParams.set("persona", persona);
  }
  if (mode) {
    url.searchParams.set("mode", mode);
  }
  const response = await fetch(url.toString(), { headers: { accept: "application/json" } });
  if (!response.ok) {
    throw new Error(`chat failed: ${response.status} ${response.statusText}`);
  }
  const payload = (await response.json()) as { reply?: string; text?: string };
  return payload.reply || payload.text || "Template runtime is ready for custom actor-backed chat flows.";
}

export function ChatLabIsland(props: Props) {
  const [prompt, setPrompt] = useState("");
  const seedMessages = useMemo(() => normalizeSeed(props.seed || []), [props.seed]);
  const [messages, setMessages] = useState<ChatMessage[]>(seedMessages);
  const [streaming, setStreaming] = useState(false);
  const personaOptions = props.personas || [];
  const modeOptions = props.modes || [];
  const agentRoster = props.agents || [];
  const playbooks = props.playbooks || [];
  const tools = props.tools || [];
  const memory = props.memory || [];
  const [persona, setPersona] = useState(() =>
    normalizeSelectionLabel(personaOptions[0]?.title || personaOptions[0]?.kicker || "")
  );
  const [mode, setMode] = useState(() =>
    normalizeSelectionLabel(modeOptions[0]?.title || modeOptions[0]?.kicker || "")
  );
  const containerRef = useRef<HTMLDivElement | null>(null);

  useEffect(() => {
    setMessages(seedMessages);
  }, [seedMessages]);

  useEffect(() => {
    const node = containerRef.current;
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
      if (persona) {
        streamUrl.searchParams.set("persona", persona);
      }
      if (mode) {
        streamUrl.searchParams.set("mode", mode);
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
        const reply = await requestChatReply(endpoint, trimmed, persona, mode);
        setMessages((prev) => [...prev, { role: "assistant", text: reply }]);
      };
    } catch (error) {
      setStreaming(false);
      const reply = await requestChatReply(endpoint, trimmed, persona, mode);
      setMessages((prev) => [...prev, { role: "assistant", text: reply }]);
    }
  };

  return (
    <div class="kain-island kain-island-chat">
      <div class="kain-island-header">
        <p class="kain-island-eyebrow">Chat</p>
        <h3 class="kain-island-title">Chat island (SSE streaming + actor routing)</h3>
        <p class="kain-island-copy">
          Uses `/api/chat/stream` when available, falls back to `/api/chat`. Plug a real LLM adapter into the Node FFI
          lane while keeping the authored intent in manifests + Kain.
        </p>
        {(personaOptions.length > 0 || modeOptions.length > 0) && (
          <div class="kain-chat-controls">
            {personaOptions.length > 0 && (
              <label>
                Persona
                <select value={persona} onChange={(event) => setPersona(event.currentTarget.value)}>
                  {personaOptions.map((entry, index) => {
                    const label = normalizeSelectionLabel(entry.title || entry.kicker || `Persona ${index + 1}`);
                    return (
                      <option key={label} value={label}>
                        {label}
                      </option>
                    );
                  })}
                </select>
              </label>
            )}
            {modeOptions.length > 0 && (
              <label>
                Mode
                <select value={mode} onChange={(event) => setMode(event.currentTarget.value)}>
                  {modeOptions.map((entry, index) => {
                    const label = normalizeSelectionLabel(entry.title || entry.kicker || `Mode ${index + 1}`);
                    return (
                      <option key={label} value={label}>
                        {label}
                      </option>
                    );
                  })}
                </select>
              </label>
            )}
          </div>
        )}
        {agentRoster.length > 0 && (
          <div class="kain-chat-agents">
            <p class="kain-chat-agents-label">Agent roster</p>
            <div class="kain-chat-agent-grid">
              {agentRoster.slice(0, 6).map((agent, index) => (
                <span class="kain-chat-agent-pill" key={`${agent.title || agent.kicker || "agent"}-${index}`}>
                  {agent.title || agent.kicker || "Agent"}
                </span>
              ))}
            </div>
          </div>
        )}
        {(playbooks.length > 0 || tools.length > 0 || memory.length > 0) && (
          <div class="kain-chat-systems">
            {playbooks.length > 0 && (
              <div class="kain-chat-system-block">
                <p class="kain-chat-system-title">Playbooks</p>
                <div class="kain-chat-system-grid">
                  {playbooks.map((entry, index) => (
                    <article class="kain-chat-system-card" key={`playbook-${index}`}>
                      <h4>{entry.title || `Playbook ${index + 1}`}</h4>
                      <p>{entry.body || "Describe the playbook intent in the manifest."}</p>
                    </article>
                  ))}
                </div>
              </div>
            )}
            {tools.length > 0 && (
              <div class="kain-chat-system-block">
                <p class="kain-chat-system-title">Tools</p>
                <div class="kain-chat-system-grid">
                  {tools.map((entry, index) => (
                    <article class="kain-chat-system-card" key={`tool-${index}`}>
                      <h4>{entry.title || entry.kicker || `Tool ${index + 1}`}</h4>
                      <p>{entry.body || entry.summary || "Describe the tool routing in the manifest."}</p>
                    </article>
                  ))}
                </div>
              </div>
            )}
            {memory.length > 0 && (
              <div class="kain-chat-system-block">
                <p class="kain-chat-system-title">Memory lanes</p>
                <div class="kain-chat-system-grid">
                  {memory.map((entry, index) => (
                    <article class="kain-chat-system-card" key={`memory-${index}`}>
                      <h4>{entry.title || entry.kicker || `Memory ${index + 1}`}</h4>
                      <p>{entry.body || entry.summary || "Describe the memory contract in the manifest."}</p>
                    </article>
                  ))}
                </div>
              </div>
            )}
          </div>
        )}
      </div>
      <div class="kain-chat-log" ref={containerRef}>
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
          placeholder="Ask about lanes, routes, pricing, actors…"
          disabled={streaming}
        />
        <button type="submit" disabled={streaming || !prompt.trim()}>
          {streaming ? "Streaming…" : "Ask"}
        </button>
      </form>
    </div>
  );
}
