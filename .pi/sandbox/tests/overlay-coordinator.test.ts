import { describe, it, expect, vi, beforeEach, afterEach } from "vitest";
import { OverlayRenderCoordinator } from "../overlay-coordinator.js";

function createOverlayHarness() {
  let hidden = false;
  const baseRequestRender = vi.fn();
  const tui = {
    requestRender: baseRequestRender,
  };
  const handle = {
    hide: vi.fn(),
    setHidden: vi.fn((nextHidden: boolean) => {
      hidden = nextHidden;
    }),
    isHidden: vi.fn(() => hidden),
  };

  return { tui, handle, baseRequestRender, isHidden: () => hidden };
}

describe("OverlayRenderCoordinator", () => {
  beforeEach(() => {
    vi.useFakeTimers();
  });

  afterEach(() => {
    vi.useRealTimers();
  });

  it("attaches to TUI and wraps requestRender", () => {
    const coordinator = new OverlayRenderCoordinator();
    const { tui, handle, baseRequestRender } = createOverlayHarness();

    coordinator.attach(tui as any);
    coordinator.setHandle(handle as any);

    tui.requestRender();
    expect(baseRequestRender).toHaveBeenCalledTimes(1);
  });

  it("throttles rapid renders", () => {
    const coordinator = new OverlayRenderCoordinator();
    const { tui, handle, baseRequestRender } = createOverlayHarness();

    coordinator.attach(tui as any);
    coordinator.setHandle(handle as any);

    tui.requestRender();
    tui.requestRender();
    tui.requestRender();

    // Only first render goes through immediately
    expect(baseRequestRender).toHaveBeenCalledTimes(1);
  });

  it("schedules repair render after foreground activity", () => {
    const coordinator = new OverlayRenderCoordinator();
    const { tui, handle, baseRequestRender } = createOverlayHarness();

    coordinator.attach(tui as any);
    coordinator.setHandle(handle as any);

    // Initial render
    tui.requestRender();
    expect(baseRequestRender).toHaveBeenCalledTimes(1);

    // Simulate foreground activity
    vi.advanceTimersByTime(50);
    coordinator.noteForegroundActivity();

    // No immediate render
    expect(baseRequestRender).toHaveBeenCalledTimes(1);

    // After quiet period, repair render fires
    vi.advanceTimersByTime(100);
    expect(baseRequestRender).toHaveBeenCalledTimes(2);
  });

  it("does not render when overlay is hidden", () => {
    const coordinator = new OverlayRenderCoordinator();
    const { tui, handle, baseRequestRender } = createOverlayHarness();

    coordinator.attach(tui as any);
    coordinator.setHandle(handle as any);
    handle.setHidden(true);

    tui.requestRender();
    expect(baseRequestRender).not.toHaveBeenCalled();
  });

  it("coalesces multiple foreground activities into single repair", () => {
    const coordinator = new OverlayRenderCoordinator();
    const { tui, handle, baseRequestRender } = createOverlayHarness();

    coordinator.attach(tui as any);
    coordinator.setHandle(handle as any);

    tui.requestRender();
    expect(baseRequestRender).toHaveBeenCalledTimes(1);

    vi.advanceTimersByTime(50);
    coordinator.noteForegroundActivity();
    coordinator.noteForegroundActivity();
    coordinator.noteForegroundActivity();

    // Still just one repair scheduled
    vi.advanceTimersByTime(100);
    expect(baseRequestRender).toHaveBeenCalledTimes(2);
  });

  it("detaches cleanly", () => {
    const coordinator = new OverlayRenderCoordinator();
    const { tui, handle, baseRequestRender } = createOverlayHarness();

    coordinator.attach(tui as any);
    coordinator.setHandle(handle as any);
    coordinator.noteForegroundActivity();

    coordinator.detach();

    // Timer should be cleared, no repair render
    vi.advanceTimersByTime(200);
    expect(baseRequestRender).not.toHaveBeenCalled();
  });
});
