import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const scriptPath = fileURLToPath(import.meta.url);
const workspaceRoot = path.resolve(path.dirname(scriptPath), "..", "..");
const changelogPath = path.join(workspaceRoot, "automation", "CHANGELOG.md");

function parseArgs(argv) {
  const args = {
    turn: null,
    lane: null,
    summary: "",
    date: new Date().toISOString()
  };

  for (let i = 0; i < argv.length; i += 1) {
    const current = argv[i];
    if (current === "--turn") {
      args.turn = Number(argv[i + 1] ?? 0);
      i += 1;
      continue;
    }
    if (current === "--lane") {
      args.lane = String(argv[i + 1] ?? "").trim();
      i += 1;
      continue;
    }
    if (current === "--summary") {
      args.summary = String(argv[i + 1] ?? "").trim();
      i += 1;
      continue;
    }
    if (current === "--date") {
      args.date = String(argv[i + 1] ?? "").trim();
      i += 1;
    }
  }

  if (!Number.isInteger(args.turn) || args.turn < 1) {
    throw new Error("--turn must be a positive integer.");
  }
  if (!args.lane) {
    throw new Error("--lane is required.");
  }
  if (!args.summary) {
    throw new Error("--summary is required.");
  }

  return args;
}

function buildEntry(args) {
  const turnLabel = `TURN-${String(args.turn).padStart(3, "0")}`;
  return [
    `### ${turnLabel} - ${args.lane}`,
    "",
    `- Date: ${args.date}`,
    `- Summary: ${args.summary}`,
    "- Kain changes:",
    "- OuroborosV2 changes:",
    "- Validation:",
    "- Next handoff:",
    ""
  ].join("\n");
}

function main() {
  const args = parseArgs(process.argv.slice(2));
  const turnLabel = `TURN-${String(args.turn).padStart(3, "0")}`;
  const content = fs.readFileSync(changelogPath, "utf8");

  if (content.includes(`### ${turnLabel} -`)) {
    process.stdout.write(`${changelogPath}\n`);
    return;
  }

  const marker = "## Entries";
  const entry = buildEntry(args);
  const updated = content.includes(marker)
    ? content.replace(marker, `${marker}\n\n${entry}`)
    : `${content.trimEnd()}\n\n${entry}`;

  fs.writeFileSync(changelogPath, updated, "utf8");
  process.stdout.write(`${changelogPath}\n`);
}

main();
