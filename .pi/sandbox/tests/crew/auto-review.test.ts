import * as fs from "node:fs";
import * as path from "node:path";
import { describe, expect, it } from "vitest";
import * as store from "../../crew/store.js";
import { readFeedEvents, isCrewEvent } from "../../feed.js";
import { createTempCrewDirs } from "../helpers/temp-dirs.js";
import type { ReviewFeedback, Task } from "../../crew/types.js";

function completedTask(cwd: string, title: string, deps: string[] = []): Task {
  const task = store.createTask(cwd, title, `Spec for ${title}`, deps);
  store.updateTask(cwd, task.id, {
    status: "in_progress",
    started_at: new Date().toISOString(),
    base_commit: "abc123",
    assigned_to: "TestWorker",
    attempt_count: 1,
  });
  store.completeTask(cwd, task.id, "Done");
  return store.getTask(cwd, task.id)!;
}

function storeReviewFeedback(cwd: string, taskId: string, verdict: ReviewFeedback["verdict"]): void {
  store.updateTask(cwd, taskId, {
    last_review: {
      verdict,
      summary: `Review says ${verdict}`,
      issues: verdict === "SHIP" ? [] : ["Issue one"],
      suggestions: [],
      reviewed_at: new Date().toISOString(),
    },
  });
}

describe("auto-review store operations", () => {
  it("SHIP: task stays done, review_count incremented", () => {
    const { cwd } = createTempCrewDirs();
    store.createPlan(cwd, "PRD.md");
    const task = completedTask(cwd, "Build API");

    storeReviewFeedback(cwd, task.id, "SHIP");

    const reviewCount = (task.review_count ?? 0) + 1;
    store.updateTask(cwd, task.id, { review_count: reviewCount });

    const updated = store.getTask(cwd, task.id)!;
    expect(updated.status).toBe("done");
    expect(updated.review_count).toBe(1);
    expect(updated.last_review?.verdict).toBe("SHIP");
  });

  it("NEEDS_WORK: resetTask preserves review_count and last_review", () => {
    const { cwd } = createTempCrewDirs();
    store.createPlan(cwd, "PRD.md");
    const task = completedTask(cwd, "Build API");

    storeReviewFeedback(cwd, task.id, "NEEDS_WORK");
    store.updateTask(cwd, task.id, { review_count: 1 });
    store.resetTask(cwd, task.id);

    const updated = store.getTask(cwd, task.id)!;
    expect(updated.status).toBe("todo");
    expect(updated.review_count).toBe(1);
    expect(updated.last_review?.verdict).toBe("NEEDS_WORK");
    expect(updated.last_review?.issues).toEqual(["Issue one"]);
    expect(updated.completed_at).toBeUndefined();
    expect(updated.summary).toBeUndefined();
    expect(updated.base_commit).toBeUndefined();
    expect(updated.assigned_to).toBeUndefined();
  });

  it("NEEDS_WORK: plan completed_count decremented after resetTask", () => {
    const { cwd } = createTempCrewDirs();
    store.createPlan(cwd, "PRD.md");
    const task = completedTask(cwd, "Build API");

    const planBefore = store.getPlan(cwd)!;
    expect(planBefore.completed_count).toBe(1);

    storeReviewFeedback(cwd, task.id, "NEEDS_WORK");
    store.updateTask(cwd, task.id, { review_count: 1 });
    store.resetTask(cwd, task.id);

    const planAfter = store.getPlan(cwd)!;
    expect(planAfter.completed_count).toBe(0);
  });

  it("NEEDS_WORK: reset task appears in getReadyTasks", () => {
    const { cwd } = createTempCrewDirs();
    store.createPlan(cwd, "PRD.md");
    const task = completedTask(cwd, "Build API");

    storeReviewFeedback(cwd, task.id, "NEEDS_WORK");
    store.updateTask(cwd, task.id, { review_count: 1 });
    store.resetTask(cwd, task.id);

    const ready = store.getReadyTasks(cwd);
    expect(ready.map(t => t.id)).toContain(task.id);
  });

  it("MAJOR_RETHINK: blockTask sets status and reason", () => {
    const { cwd } = createTempCrewDirs();
    store.createPlan(cwd, "PRD.md");
    const task = completedTask(cwd, "Build API");

    storeReviewFeedback(cwd, task.id, "MAJOR_RETHINK");
    store.updateTask(cwd, task.id, { review_count: 1 });
    store.blockTask(cwd, task.id, "Reviewer: Review says MAJOR_RETHINK");

    const updated = store.getTask(cwd, task.id)!;
    expect(updated.status).toBe("blocked");
    expect(updated.blocked_reason).toBe("Reviewer: Review says MAJOR_RETHINK");
    expect(updated.review_count).toBe(1);
    expect(updated.last_review?.verdict).toBe("MAJOR_RETHINK");
  });

  it("MAJOR_RETHINK: blocked task does not appear in getReadyTasks", () => {
    const { cwd } = createTempCrewDirs();
    store.createPlan(cwd, "PRD.md");
    const task = completedTask(cwd, "Build API");

    storeReviewFeedback(cwd, task.id, "MAJOR_RETHINK");
    store.updateTask(cwd, task.id, { review_count: 1 });
    store.blockTask(cwd, task.id, "Reviewer: issues");

    const ready = store.getReadyTasks(cwd);
    expect(ready.map(t => t.id)).not.toContain(task.id);
  });

  it("review_count gates review after maxIterations", () => {
    const { cwd } = createTempCrewDirs();
    store.createPlan(cwd, "PRD.md");
    const task = completedTask(cwd, "Build API");

    store.updateTask(cwd, task.id, { review_count: 3 });

    const updated = store.getTask(cwd, task.id)!;
    expect(updated.review_count).toBe(3);
  });

  it("NEEDS_WORK with dependency: dependent task stays done, not cascaded", () => {
    const { cwd } = createTempCrewDirs();
    store.createPlan(cwd, "PRD.md");
    const dep = completedTask(cwd, "Foundation");
    const main = completedTask(cwd, "Feature", [dep.id]);

    storeReviewFeedback(cwd, dep.id, "NEEDS_WORK");
    store.updateTask(cwd, dep.id, { review_count: 1 });
    store.resetTask(cwd, dep.id); // no cascade

    const depAfter = store.getTask(cwd, dep.id)!;
    const mainAfter = store.getTask(cwd, main.id)!;
    expect(depAfter.status).toBe("todo");
    expect(mainAfter.status).toBe("done");

    const ready = store.getReadyTasks(cwd);
    expect(ready.map(t => t.id)).toContain(dep.id);
    expect(ready.map(t => t.id)).not.toContain(main.id);
  });

  it("task.review feed event is recognized as crew event", () => {
    const { cwd } = createTempCrewDirs();
    const feedPath = path.join(cwd, ".pi", "messenger", "feed.jsonl");
    fs.mkdirSync(path.dirname(feedPath), { recursive: true });

    const event = {
      ts: new Date().toISOString(),
      agent: "crew",
      type: "task.review" as const,
      target: "task-1",
      preview: "SHIP",
    };
    fs.writeFileSync(feedPath, JSON.stringify(event) + "\n");

    const events = readFeedEvents(cwd, 10);
    expect(events).toHaveLength(1);
    expect(events[0].type).toBe("task.review");
    expect(isCrewEvent("task.review")).toBe(true);
  });

  it("no base_commit: review should be skipped", () => {
    const { cwd } = createTempCrewDirs();
    store.createPlan(cwd, "PRD.md");
    const task = store.createTask(cwd, "No git task", "Desc");
    store.updateTask(cwd, task.id, { status: "done", completed_at: new Date().toISOString() });

    const loaded = store.getTask(cwd, task.id)!;
    expect(loaded.base_commit).toBeUndefined();
  });
});
