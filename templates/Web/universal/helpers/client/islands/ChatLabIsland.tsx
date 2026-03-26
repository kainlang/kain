import { h } from "preact";
import { useEffect, useMemo, useRef, useState } from "preact/hooks";

import type { KainChatSeedMessage } from "../lib/kain_site_data";

type ChatMessage = {
  role: "user" | "assistant";
  text: string;
};

type Props = {
  seed: KainChatSeedMessage[];
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

async function requestChatReply(endpoint: string, prompt: string): Promise<string> {
  const url = new URL(endpoint, window.location.href);
  url.searchParams.set("prompt", prompt);
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
    const trimmed = prompt.trim();
    if (!trimmed || streaming) return;
    setPrompt("");
    setMessages((prev) => [...prev, { role: "user", text: trimmed }]);

    try {
      setStreaming(true);

      const streamUrl = new URL(streamEndpoint, window.location.href);
      streamUrl.searchParams.set("prompt", trimmed);

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
        const reply = await requestChatReply(endpoint, trimmed);
        setMessages((prev) => [...prev, { role: "assistant", text: reply }]);
      };
    } catch (error) {
      setStreaming(false);
      const reply = await requestChatReply(endpoint, trimmed);
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
