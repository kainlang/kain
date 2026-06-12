import * as fs from "node:fs";
import * as path from "node:path";
import { beforeEach, describe, expect, it, vi } from "vitest";
import type { MessengerState, Dirs } from "../../lib.js";
import { executeCrewAction } from "../../crew/index.js";
import { autonomousState, startAutonomous } from "../../crew/state.js";
import { createTempCrewDirs } from "../helpers/temp-dirs.js";
import { createMockContext } from "../helpers/mock-context.js";

function createTestState(agentName: string): MessengerState {
  return {
    agentName,
    registered: true,
    watcher: null,
    watcherRetries: 0,
    watcherRetryTimer: null,
    watcherDebounceTimer: null,
    reservations: [],
    chatHistory: new Map(),
    unreadCounts: new Map(),
    broadcastHistory: [],
    seenSenders: new Map(),
    model: "test-model",
    cwd: process.cwd(),
    gitBranch: undefined,
    spec: undefined,
    scopeToFolder: false,
    isHuman: false,
    session: { toolCalls: 0, tokens: 0, filesModified: [] },
    activity: { lastActivityAt: new Date().toISOString() },
    statusMessage: undefined,
    customStatus: false,
    registryFlushTimer: null,
    sessionStartedAt: new Date().toISOString(),
  };
}

function createDirs(cwd: string): Dirs {
  const base = path.join(cwd, ".pi", "messenger");
  const registry = path.join(base, "registry");
  const inbox = path.join(base, "inbox");
  fs.mkdirSync(registry, { recursive: true });
  fs.mkdirSync(inbox, { recursive: true });
  return { base, registry, inbox };
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

describe("crew work.stop action", () => {
  beforeEach(() => {
    resetAutonomousState();
  });

  it("returns no-op message when autonomous is not active for cwd", async () => {
    const { cwd } = createTempCrewDirs();
    const state = createTestState("AgentOne");
    const dirs = createDirs(cwd);
    const ctx = createMockContext(cwd);

    const response = await executeCrewAction(
      "work.stop",
      {},
      state,
      dirs,
      ctx,
      () => {},
      () => {},
      vi.fn(),
    );

    expect(response.content[0].text).toContain("No autonomous work running for this project.");
    expect(response.details.mode).toBe("work.stop");
  });

  it("stops active autonomous work and persists crew-state", async () => {
    const { cwd } = createTempCrewDirs();
    const state = createTestState("AgentOne");
    const dirs = createDirs(cwd);
    const ctx = createMockContext(cwd);
    const appendEntry = vi.fn();

    startAutonomous(cwd, 2);
    expect(autonomousState.active).toBe(true);

    const response = await executeCrewAction(
      "work.stop",
      {},
      state,
      dirs,
      ctx,
      () => {},
      () => {},
      appendEntry,
    );

    expect(response.content[0].text).toContain("Autonomous work stopped.");
    expect(autonomousState.active).toBe(false);
    expect(autonomousState.stopReason).toBe("manual");
    expect(appendEntry).toHaveBeenCalledWith("crew-state", autonomousState);
  });
});
