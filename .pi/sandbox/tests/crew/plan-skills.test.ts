import { describe, it, expect, vi, beforeEach, afterEach } from "vitest";
import * as fs from "node:fs";
import * as path from "node:path";
import { createTempCrewDirs } from "../helpers/temp-dirs.js";

vi.mock("../../crew/agents.js", () => ({
  spawnAgents: vi.fn(),
}));

describe("plan with skills", () => {
  let planHandler: typeof import("../../crew/handlers/plan.js");
  let spawnAgents: ReturnType<typeof vi.fn>;
  let store: typeof import("../../crew/store.js");
  let state: typeof import("../../crew/state.js");
  let tmpDir: string;
  let mockCtx: any;

  function plannerOutputWithSkills(tasks: unknown[]) {
    return `## 1. PRD Understanding Summary\nSummary\n## 2. Relevant Code/Docs/Resources Reviewed\nResources\n## 3. Sequential Implementation Steps\nSteps\n## 4. Parallelized Task Graph\nGraph\n\`\`\`tasks-json\n${JSON.stringify(tasks, null, 2)}\n\`\`\``;
  }

  function writeProjectSkill(name: string, description: string) {
    const skillsDir = path.join(tmpDir, ".pi", "messenger", "crew", "skills");
    fs.mkdirSync(skillsDir, { recursive: true });
    fs.writeFileSync(path.join(skillsDir, `${name}.md`), `---\nname: ${name}\ndescription: ${description}\n---\nSkill content for ${name}.\n`);
  }

  beforeEach(async () => {
    vi.resetModules();
    planHandler = await import("../../crew/handlers/plan.js");
    store = await import("../../crew/store.js");
    state = await import("../../crew/state.js");
    const agents = await import("../../crew/agents.js");
    spawnAgents = agents.spawnAgents as ReturnType<typeof vi.fn>;

    const dirs = createTempCrewDirs();
    tmpDir = dirs.cwd;
    fs.mkdirSync(path.join(tmpDir, "docs"), { recursive: true });
    fs.writeFileSync(path.join(tmpDir, "docs", "PRD.md"), "# PRD\nBuild a feature");
    mockCtx = { cwd: tmpDir, hasUI: false, ui: {} };
  });

  afterEach(() => {
    if (state.planningState.cwd) state.clearPlanningState(state.planningState.cwd);
  });

  it("persists planner-tagged skills on created tasks", async () => {
    spawnAgents.mockResolvedValue([{
      exitCode: 0,
      output: plannerOutputWithSkills([
        { title: "Build API", description: "REST endpoints", dependsOn: [], skills: ["api-design", "testing"] },
        { title: "Build UI", description: "React components", dependsOn: ["Build API"], skills: ["react-patterns"] },
        { title: "Write docs", description: "Documentation", dependsOn: [] },
      ]),
      error: null,
      progress: { toolCallCount: 0, tokens: 0 },
    }]);

    const r = await planHandler.execute({ action: "plan" }, mockCtx, "agent");
    expect(r.details?.error).toBeUndefined();

    const tasks = store.getTasks(tmpDir);
    expect(tasks).toHaveLength(3);

    const apiTask = tasks.find(t => t.title === "Build API");
    const uiTask = tasks.find(t => t.title === "Build UI");
    const docsTask = tasks.find(t => t.title === "Write docs");

    expect(apiTask?.skills).toEqual(["api-design", "testing"]);
    expect(uiTask?.skills).toEqual(["react-patterns"]);
    expect(docsTask?.skills).toBeUndefined();
  });

  it("handles tasks-json with no skills field gracefully", async () => {
    spawnAgents.mockResolvedValue([{
      exitCode: 0,
      output: plannerOutputWithSkills([
        { title: "Task A", description: "Do A", dependsOn: [] },
      ]),
      error: null,
      progress: { toolCallCount: 0, tokens: 0 },
    }]);

    const r = await planHandler.execute({ action: "plan" }, mockCtx, "agent");
    expect(r.details?.error).toBeUndefined();

    const tasks = store.getTasks(tmpDir);
    expect(tasks).toHaveLength(1);
    expect(tasks[0].skills).toBeUndefined();
  });

  it("ignores non-string elements in skills array", async () => {
    spawnAgents.mockResolvedValue([{
      exitCode: 0,
      output: plannerOutputWithSkills([
        { title: "Task A", description: "Do A", dependsOn: [], skills: ["valid", 42, null, "also-valid"] },
      ]),
      error: null,
      progress: { toolCallCount: 0, tokens: 0 },
    }]);

    const r = await planHandler.execute({ action: "plan" }, mockCtx, "agent");
    expect(r.details?.error).toBeUndefined();

    const tasks = store.getTasks(tmpDir);
    expect(tasks[0].skills).toEqual(["valid", "also-valid"]);
  });

  it("injects project skill index into planner prompt", async () => {
    writeProjectSkill("react-patterns", "React conventions and hooks");
    writeProjectSkill("testing", "Test setup and patterns");

    spawnAgents.mockResolvedValue([{
      exitCode: 0,
      output: plannerOutputWithSkills([
        { title: "Task A", description: "Do A", dependsOn: [] },
      ]),
      error: null,
      progress: { toolCallCount: 0, tokens: 0 },
    }]);

    await planHandler.execute({ action: "plan" }, mockCtx, "agent");

    const plannerPrompt = spawnAgents.mock.calls[0][0][0].task;
    expect(plannerPrompt).toContain("Available Skills");
    expect(plannerPrompt).toContain("react-patterns");
    expect(plannerPrompt).toContain("testing");
    expect(plannerPrompt).toContain("skills");
  });

  it("planner prompt mentions skills in format instructions", async () => {
    writeProjectSkill("some-skill", "A skill");

    spawnAgents.mockResolvedValue([{
      exitCode: 0,
      output: plannerOutputWithSkills([
        { title: "Task A", description: "Do A", dependsOn: [] },
      ]),
      error: null,
      progress: { toolCallCount: 0, tokens: 0 },
    }]);

    await planHandler.execute({ action: "plan" }, mockCtx, "agent");

    const plannerPrompt = spawnAgents.mock.calls[0][0][0].task;
    expect(plannerPrompt).toContain("optionally skills");
  });

});
