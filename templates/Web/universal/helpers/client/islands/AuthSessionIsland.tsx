import { h } from "preact";
import { useEffect, useState } from "preact/hooks";

type SessionPayload = {
  ok: boolean;
  session: {
    id: string;
    created_at: string;
    email: string | null;
    invite_code?: string | null;
    roles?: string[];
  } | null;
};

async function fetchSession(): Promise<SessionPayload> {
  const response = await fetch("/api/auth/session", { headers: { accept: "application/json" } });
  if (!response.ok) throw new Error(`session fetch failed: ${response.status}`);
  return (await response.json()) as SessionPayload;
}

async function login(email: string): Promise<SessionPayload> {
  const response = await fetch("/api/auth/session/login", {
    method: "POST",
    headers: { "content-type": "application/json", accept: "application/json" },
    body: JSON.stringify({ email })
  });
  if (!response.ok) throw new Error(`session login failed: ${response.status}`);
  return (await response.json()) as SessionPayload;
}

async function logout(): Promise<{ ok: boolean }> {
  const response = await fetch("/api/auth/session/logout", {
    method: "POST",
    headers: { accept: "application/json" }
  });
  if (!response.ok) throw new Error(`session logout failed: ${response.status}`);
  return (await response.json()) as { ok: boolean };
}

export function AuthSessionIsland() {
  const [session, setSession] = useState<SessionPayload["session"]>(null);
  const [email, setEmail] = useState("");
  const [status, setStatus] = useState<string | null>(null);

  const refresh = async () => {
    try {
      setStatus("loading");
      const payload = await fetchSession();
      setSession(payload.session);
      setStatus(payload.session ? "active" : "anonymous");
    } catch (error) {
      setStatus((error as Error).message || "error");
    }
  };

  useEffect(() => {
    void refresh();
  }, []);

  const submitLogin = async () => {
    const trimmed = email.trim();
    if (!trimmed) return;
    try {
      setStatus("logging in");
      const payload = await login(trimmed);
      setSession(payload.session);
      setEmail("");
      setStatus(payload.session ? "active" : "anonymous");
    } catch (error) {
      setStatus((error as Error).message || "error");
    }
  };

  const submitLogout = async () => {
    try {
      setStatus("logging out");
      await logout();
      await refresh();
    } catch (error) {
      setStatus((error as Error).message || "error");
    }
  };

  return (
    <div class="kain-island kain-island-auth-session">
      <div class="kain-island-header">
        <p class="kain-island-eyebrow">Session</p>
        <h3 class="kain-island-title">Cookie-backed identity (local runtime)</h3>
        <p class="kain-island-copy">
          This is a dev-only session surface so uploads, chat, and operator features can bind identity without needing a
          real auth provider yet.
        </p>
      </div>
      <div class="kain-auth-session-body">
        {session ? (
          <div class="kain-auth-session-card">
            <p class="kain-auth-session-label">Active session</p>
            <p class="kain-auth-session-value">{session.email || "anonymous"}</p>
            <p class="kain-auth-session-meta">
              {["id " + session.id.slice(0, 8), "created " + session.created_at].join(" · ")}
            </p>
            <button type="button" onClick={() => void submitLogout()}>
              Logout
            </button>
          </div>
        ) : (
          <form
            class="kain-auth-session-form"
            onSubmit={(event) => {
              event.preventDefault();
              void submitLogin();
            }}
          >
            <input
              name="email"
              value={email}
              placeholder="email@example.com"
              onInput={(event) => setEmail((event.target as HTMLInputElement).value)}
            />
            <button type="submit" disabled={!email.trim()}>
              Create session
            </button>
          </form>
        )}
        <p class="kain-island-status">{status || "idle"}</p>
      </div>
    </div>
  );
}

