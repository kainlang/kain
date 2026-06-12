import * as fs from "node:fs";
import * as path from "node:path";
import { beforeEach, describe, expect, it } from "vitest";
import { createTempCrewDirs, type TempCrewDirs } from "../../helpers/temp-dirs.js";
import { discoverCrewAgents, discoverCrewSkills } from "../../../crew/utils/discover.js";

function writeAgent(filePath: string, content: string): void {
  fs.mkdirSync(path.dirname(filePath), { recursive: true });
  fs.writeFileSync(filePath, content);
}

describe("crew/utils/discover", () => {
  let dirs: TempCrewDirs;
  let extensionAgentsDir: string;
  let projectAgentsDir: string;

  beforeEach(() => {
    dirs = createTempCrewDirs();
    extensionAgentsDir = path.join(dirs.root, "extension-agents");
    projectAgentsDir = path.join(dirs.cwd, ".pi", "messenger", "crew", "agents");
    fs.mkdirSync(extensionAgentsDir, { recursive: true });
  });

  it("discovers agents from injected extension directory", () => {
    writeAgent(path.join(extensionAgentsDir, "crew-worker.md"), `---
name: crew-worker
description: Worker implementation agent
tools: read, bash, pi_messenger
model: gpt-4.1-mini
crewRole: worker
---
You are a worker.
`);

    const agents = discoverCrewAgents(dirs.cwd, extensionAgentsDir);
    expect(agents).toHaveLength(1);
    expect(agents[0].name).toBe("crew-worker");
    expect(agents[0].source).toBe("extension");
    expect(agents[0].model).toBe("gpt-4.1-mini");
    expect(agents[0].tools).toEqual(["read", "bash", "pi_messenger"]);
  });

  it("project agents override extension agents with the same name", () => {
    writeAgent(path.join(extensionAgentsDir, "crew-reviewer.md"), `---
name: crew-reviewer
description: Extension reviewer
crewRole: reviewer
model: extension-model
---
Extension prompt.
`);

    writeAgent(path.join(projectAgentsDir, "crew-reviewer.md"), `---
name: crew-reviewer
description: Project reviewer
crewRole: reviewer
model: project-model
---
Project prompt.
`);

    const agents = discoverCrewAgents(dirs.cwd, extensionAgentsDir);
    expect(agents).toHaveLength(1);
    expect(agents[0].name).toBe("crew-reviewer");
    expect(agents[0].description).toBe("Project reviewer");
    expect(agents[0].model).toBe("project-model");
    expect(agents[0].source).toBe("project");
    expect(agents[0].systemPrompt).toContain("Project prompt.");
  });

  it("includes project-only agents alongside extension defaults", () => {
    writeAgent(path.join(extensionAgentsDir, "crew-worker.md"), `---
name: crew-worker
description: Extension worker
crewRole: worker
---
Extension worker prompt.
`);

    writeAgent(path.join(projectAgentsDir, "crew-custom.md"), `---
name: crew-custom
description: Project custom agent
crewRole: worker
---
Project custom prompt.
`);

    const agents = discoverCrewAgents(dirs.cwd, extensionAgentsDir);
    const names = agents.map(agent => agent.name).sort();
    expect(names).toEqual(["crew-custom", "crew-worker"]);
    expect(agents.find(agent => agent.name === "crew-worker")?.source).toBe("extension");
    expect(agents.find(agent => agent.name === "crew-custom")?.source).toBe("project");
  });

  it("returns extension agents when project directory is missing", () => {
    writeAgent(path.join(extensionAgentsDir, "crew-planner.md"), `---
name: crew-planner
description: Planner
crewRole: planner
---
Planner prompt.
`);

    const agents = discoverCrewAgents(dirs.cwd, extensionAgentsDir);
    expect(agents).toHaveLength(1);
    expect(agents[0].name).toBe("crew-planner");
    expect(agents[0].source).toBe("extension");
  });

  it("parses thinking from frontmatter", () => {
    writeAgent(path.join(extensionAgentsDir, "crew-thinker.md"), `---
name: crew-thinker
description: Deep thinker
thinking: high
model: claude-opus-4-6
---
Think hard.
`);
    const agents = discoverCrewAgents(dirs.cwd, extensionAgentsDir);
    expect(agents[0].thinking).toBe("high");
  });

  it("thinking defaults to undefined when not specified", () => {
    writeAgent(path.join(extensionAgentsDir, "crew-simple.md"), `---
name: crew-simple
description: No thinking
---
Simple.
`);
    const agents = discoverCrewAgents(dirs.cwd, extensionAgentsDir);
    expect(agents[0].thinking).toBeUndefined();
  });

  it("parses frontmatter fields", () => {
    writeAgent(path.join(extensionAgentsDir, "crew-analyst.md"), `---
name: crew-analyst
description: Analyst
tools: read,  bash ,edit,   , write
crewRole: analyst
model: claude-3-5-haiku
maxOutput: { bytes: 2048, lines: 100 }
---
Analyst prompt
`);

    const agents = discoverCrewAgents(dirs.cwd, extensionAgentsDir);
    expect(agents).toHaveLength(1);
    expect(agents[0].name).toBe("crew-analyst");
    expect(agents[0].model).toBe("claude-3-5-haiku");
    expect(agents[0].crewRole).toBe("analyst");
    expect(agents[0].tools).toEqual(["read", "bash", "edit", "write"]);
    expect(agents[0].maxOutput).toEqual({ bytes: 2048, lines: 100 });
  });
});

describe("crew/utils/discover - skills", () => {
  let dirs: TempCrewDirs;
  let extensionSkillsDir: string;
  let userSkillsDir: string;
  let projectSkillsDir: string;

  function writeSkill(filePath: string, content: string): void {
    fs.mkdirSync(path.dirname(filePath), { recursive: true });
    fs.writeFileSync(filePath, content);
  }

  beforeEach(() => {
    dirs = createTempCrewDirs();
    extensionSkillsDir = path.join(dirs.root, "extension-skills");
    userSkillsDir = path.join(dirs.root, "user-skills");
    projectSkillsDir = path.join(dirs.cwd, ".pi", "messenger", "crew", "skills");
    fs.mkdirSync(extensionSkillsDir, { recursive: true });
    fs.mkdirSync(userSkillsDir, { recursive: true });
  });

  it("discovers flat skills from extension directory", () => {
    writeSkill(path.join(extensionSkillsDir, "testing.md"), `---
name: testing
description: Test patterns and setup
---
Full testing guide here.
`);

    const skills = discoverCrewSkills(dirs.cwd, extensionSkillsDir, userSkillsDir);
    expect(skills).toHaveLength(1);
    expect(skills[0].name).toBe("testing");
    expect(skills[0].description).toBe("Test patterns and setup");
    expect(skills[0].source).toBe("extension");
    expect(skills[0].path).toBe(path.join(extensionSkillsDir, "testing.md"));
  });

  it("discovers user skills from directory/SKILL.md format", () => {
    writeSkill(path.join(userSkillsDir, "react-best-practices", "SKILL.md"), `---
name: react-best-practices
description: React and Next.js performance optimization
---
React guidelines here.
`);

    const skills = discoverCrewSkills(dirs.cwd, extensionSkillsDir, userSkillsDir);
    expect(skills).toHaveLength(1);
    expect(skills[0].name).toBe("react-best-practices");
    expect(skills[0].source).toBe("user");
    expect(skills[0].path).toBe(path.join(userSkillsDir, "react-best-practices", "SKILL.md"));
  });

  it("project skills override extension skills with the same name", () => {
    writeSkill(path.join(extensionSkillsDir, "testing.md"), `---
name: testing
description: Extension testing patterns
---
Extension content.
`);

    writeSkill(path.join(projectSkillsDir, "testing.md"), `---
name: testing
description: Project testing patterns
---
Project content.
`);

    const skills = discoverCrewSkills(dirs.cwd, extensionSkillsDir, userSkillsDir);
    expect(skills).toHaveLength(1);
    expect(skills[0].name).toBe("testing");
    expect(skills[0].description).toBe("Project testing patterns");
    expect(skills[0].source).toBe("project");
  });

  it("extension skills override user skills with the same name", () => {
    writeSkill(path.join(userSkillsDir, "api-design", "SKILL.md"), `---
name: api-design
description: User API design patterns
---
User content.
`);

    writeSkill(path.join(extensionSkillsDir, "api-design.md"), `---
name: api-design
description: Extension API design patterns
---
Extension content.
`);

    const skills = discoverCrewSkills(dirs.cwd, extensionSkillsDir, userSkillsDir);
    expect(skills).toHaveLength(1);
    expect(skills[0].name).toBe("api-design");
    expect(skills[0].source).toBe("extension");
  });

  it("merges skills from all three layers", () => {
    writeSkill(path.join(userSkillsDir, "cloudflare", "SKILL.md"), `---
name: cloudflare
description: Cloudflare Workers and Pages
---
Cloudflare guide.
`);

    writeSkill(path.join(extensionSkillsDir, "coordination.md"), `---
name: coordination
description: Worker coordination patterns
---
Coordination guide.
`);

    writeSkill(path.join(projectSkillsDir, "react-patterns.md"), `---
name: react-patterns
description: Project React conventions
---
React conventions.
`);

    const skills = discoverCrewSkills(dirs.cwd, extensionSkillsDir, userSkillsDir);
    const names = skills.map(s => s.name).sort();
    expect(names).toEqual(["cloudflare", "coordination", "react-patterns"]);
  });

  it("returns empty array when no skills directories exist", () => {
    const emptyDir = path.join(dirs.root, "nonexistent-skills");
    const skills = discoverCrewSkills(dirs.cwd, emptyDir, path.join(dirs.root, "no-user-skills"));
    expect(skills).toEqual([]);
  });

  it("skips files without name or description in frontmatter", () => {
    writeSkill(path.join(extensionSkillsDir, "bad.md"), `---
name: incomplete
---
No description field.
`);

    writeSkill(path.join(extensionSkillsDir, "good.md"), `---
name: good-skill
description: Has both fields
---
Content.
`);

    const skills = discoverCrewSkills(dirs.cwd, extensionSkillsDir, userSkillsDir);
    expect(skills).toHaveLength(1);
    expect(skills[0].name).toBe("good-skill");
  });

  it("returns absolute paths for all skills", () => {
    writeSkill(path.join(extensionSkillsDir, "ext.md"), `---
name: ext-skill
description: Extension skill
---
Content.
`);

    writeSkill(path.join(userSkillsDir, "usr", "SKILL.md"), `---
name: usr-skill
description: User skill
---
Content.
`);

    const skills = discoverCrewSkills(dirs.cwd, extensionSkillsDir, userSkillsDir);
    for (const skill of skills) {
      expect(path.isAbsolute(skill.path)).toBe(true);
    }
  });

  it("truncates multiline descriptions to first line", () => {
    writeSkill(path.join(extensionSkillsDir, "multi.md"), `---
name: multi-desc
description: First line of description
  - /extra
  - /lines
---
Content.
`);

    const skills = discoverCrewSkills(dirs.cwd, extensionSkillsDir, userSkillsDir);
    expect(skills).toHaveLength(1);
    expect(skills[0].description).toBe("First line of description");
  });
});
