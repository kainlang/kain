import * as fs from "node:fs";
import * as os from "node:os";
import * as path from "node:path";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { autonomousState, startAutonomous } from "../../crew/state.js";
import { createTempCrewDirs } from "../helpers/temp-dirs.js";

vi.mock("@mariozechner/pi-tui", () => ({
  matchesKey: () => false,
  truncateToWidth: (value: string) => value,
  visibleWidth: (value: string) => value.length,
}));

vi.mock("typebox", () => ({
  Type: {
    Unsafe: (schema: unknown) => schema,
    Optional: (schema: unknown) => schema,
    String: (schema: unknown) => schema,
    Number: (schema: unknown) => schema,
    Boolean: (schema: unknown) => schema,
    Any: (schema: unknown) => schema,
    Array: (schema: unknown) => schema,
    Object: (schema: unknown) => schema,
  },
}));

function createMockPi() {
  const handlers = new Map<string, Array<(event: unknown, ctx: any) => unknown>>();

  return {
    handlers,
    on: vi.fn((event: string, handler: (event: unknown, ctx: any) => unknown) => {
      const existing = handlers.get(event) ?? [];
      existing.push(handler);
      handlers.set(event, existing);
    }),
    registerTool: vi.fn(),
    registerCommand: vi.fn(),
    registerMessageRenderer: vi.fn(),
    sendMessage: vi.fn(),
    appendEntry: vi.fn(),
  };
}

function createEventContext(cwd: string) {
  return {
    cwd,
    hasUI: false,
    ui: {
      notify: vi.fn(),
      setStatus: vi.fn(),
      custom: vi.fn(),
      theme: { fg: (_color: string, text: string) => text },
    },
    sessionManager: {
      getEntries: () => [],
      getSessionId: () => "test-session",
    },
    model: { id: "test-model" },
  } as any;
}

function resetAutonomousState(): void {
  autonomousState.active = false;
  autonomousState.cwd = null;
  autonomousState.waveNumber = 0;
  autonomousState.waveHistory = [];
  autonomousState.startedAt = null;
  autonomousState.stoppedAt = null;
  autonomousState.stopReason = null;
  autonomousState.concurrency = 2;
  autonomousState.autoOverlayPending = false;
  autonomousState.pid = null;
}

describe("agent_end autonomous continuation guards", () => {
  const tempHomes: string[] = [];

  beforeEach(() => {
    resetAutonomousState();
    const home = fs.mkdtempSync(path.join(os.tmpdir(), "pi-messenger-home-"));
    tempHomes.push(home);
    vi.stubEnv("HOME", home);
    vi.stubEnv("PI_MESSENGER_DIR", path.join(home, ".pi", "agent", "messenger"));
  });

  afterEach(() => {
    vi.unstubAllEnvs();
    for (const home of tempHomes) {
      fs.rmSync(home, { force: true, recursive: true });
    }
    tempHomes.length = 0;
  });

  it("stops restored autonomous state when session is not registered", async () => {
    const { cwd } = createTempCrewDirs();
    const ctx = createEventContext(cwd);
    const pi = createMockPi();
    const { default: piMessengerExtension } = await import("../../index.js");
    piMessengerExtension(pi as any);

    const agentEndHandler = pi.handlers.get("agent_end")?.[0];
    expect(agentEndHandler).toBeTruthy();

    startAutonomous(cwd, 2);
    expect(autonomousState.active).toBe(true);

    await agentEndHandler?.({}, ctx);

    expect(autonomousState.active).toBe(false);
    expect(autonomousState.stopReason).toBe("manual");
    expect(pi.appendEntry).toHaveBeenCalledWith("crew-state", autonomousState);
    expect(pi.sendMessage).not.toHaveBeenCalled();
  });

  it("skips autonomous continuation handling inside worker sessions", async () => {
    const { cwd } = createTempCrewDirs();
    const ctx = createEventContext(cwd);
    const pi = createMockPi();
    const { default: piMessengerExtension } = await import("../../index.js");
    piMessengerExtension(pi as any);

    const agentEndHandler = pi.handlers.get("agent_end")?.[0];
    expect(agentEndHandler).toBeTruthy();

    startAutonomous(cwd, 2);
    vi.stubEnv("PI_CREW_WORKER", "1");

    await agentEndHandler?.({}, ctx);

    expect(autonomousState.active).toBe(true);
    expect(pi.appendEntry).not.toHaveBeenCalled();
    expect(pi.sendMessage).not.toHaveBeenCalled();
  });
});
