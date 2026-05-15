import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const scriptPath = fileURLToPath(import.meta.url);
const workspaceRoot = path.resolve(path.dirname(scriptPath), "..", "..");
const configPath = path.join(workspaceRoot, "automation", "config", "pipeline.config.json");
const reportsDir = path.join(workspaceRoot, "automation", "reports");

function parseArgs(argv) {
  const args = { turn: null, json: false };
  for (let i = 0; i < argv.length; i += 1) {
    const current = argv[i];
    if (current === "--turn") {
      args.turn = Number(argv[i + 1] ?? 0);
      i += 1;
      continue;
    }
    if (current === "--json") {
      args.json = true;
    }
  }
  if (args.turn !== null && (!Number.isInteger(args.turn) || args.turn < 1)) {
    throw new Error("--turn must be a positive integer.");
  }
  return args;
}

function countReports() {
  if (!fs.existsSync(reportsDir)) {
    return 0;
  }
  return fs
    .readdirSync(reportsDir, { withFileTypes: true })
    .filter((entry) => entry.isFile() && entry.name.toLowerCase().endsWith(".md"))
    .length;
}

function expandRotation(rotation) {
  const expanded = [];
  for (const item of rotation) {
    for (let i = 0; i < item.turns; i += 1) {
      expanded.push(item.lane);
    }
  }
  return expanded;
}

function uniqueValues(values) {
  return [...new Set(values)];
}

function buildBrief(config, explicitTurn) {
  const turn = explicitTurn ?? countReports() + 1;
  const rotationTurns = expandRotation(config.rotation);
  const cycleLength = rotationTurns.length;
  const turnIndex = (turn - 1) % cycleLength;
  const laneName = rotationTurns[turnIndex];
  const lane = config.lanes[laneName];

  const referenceBuckets = Object.entries(config.referencePaths).map(([bucket, paths]) => ({
    bucket,
    paths
  }));

  const validationCommands = uniqueValues([
    ...config.validation.globalCommands,
    ...(config.validation.laneCommands[laneName] ?? [])
  ]);

  return {
    project: config.project.name,
    turn,
    cycleLength,
    positionInCycle: turnIndex + 1,
    lane: laneName,
    goal: lane.goal,
    northStar: config.project.northStar,
    priorities: config.priorities,
    focusAreas: lane.focusAreas,
    hardRules: lane.hardRules,
    deliverables: lane.deliverables,
    protectedAreas: config.protectedAreas,
    referenceBuckets,
    validationCommands,
    fallbackCommands: config.validation.fallbackCommands,
    reportTemplate: config.output.reportTemplate,
    reportDirectory: config.output.reportDirectory
  };
}

function formatText(brief) {
  const lines = [];
  lines.push(`# ${brief.project}`);
  lines.push("");
  lines.push(`Turn: ${brief.turn}`);
  lines.push(`Lane: ${brief.lane}`);
  lines.push(`Cycle Position: ${brief.positionInCycle}/${brief.cycleLength}`);
  lines.push("");
  lines.push("North Star:");
  lines.push(`- ${brief.northStar}`);
  lines.push("");
  lines.push("Goal:");
  lines.push(`- ${brief.goal}`);
  lines.push("");
  lines.push("Priorities:");
  for (const item of brief.priorities) {
    lines.push(`- ${item}`);
  }
  lines.push("");
  lines.push("Focus Areas:");
  for (const item of brief.focusAreas) {
    lines.push(`- ${item}`);
  }
  lines.push("");
  lines.push("Hard Rules:");
  for (const item of brief.hardRules) {
    lines.push(`- ${item}`);
  }
  lines.push("");
  lines.push("Protected Areas:");
  for (const [bucket, paths] of Object.entries(brief.protectedAreas)) {
    lines.push(`- ${bucket}: ${paths.join(", ")}`);
  }
  lines.push("");
  lines.push("Reference Paths:");
  for (const bucket of brief.referenceBuckets) {
    lines.push(`- ${bucket.bucket}: ${bucket.paths.join(", ")}`);
  }
  lines.push("");
  lines.push("Validation Commands:");
  for (const command of brief.validationCommands) {
    lines.push(`- ${command}`);
  }
  lines.push("");
  lines.push("Fallback Commands:");
  for (const command of brief.fallbackCommands) {
    lines.push(`- ${command}`);
  }
  lines.push("");
  lines.push("Deliverables:");
  for (const item of brief.deliverables) {
    lines.push(`- ${item}`);
  }
  lines.push("");
  lines.push("Session Output:");
  lines.push(`- Report template: ${brief.reportTemplate}`);
  lines.push(`- Report directory: ${brief.reportDirectory}`);
  return lines.join("\n");
}

function main() {
  const args = parseArgs(process.argv.slice(2));
  const config = JSON.parse(fs.readFileSync(configPath, "utf8"));
  const brief = buildBrief(config, args.turn);
  if (args.json) {
    process.stdout.write(`${JSON.stringify(brief, null, 2)}\n`);
    return;
  }
  process.stdout.write(`${formatText(brief)}\n`);
}

main();
