import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const scriptPath = fileURLToPath(import.meta.url);
const workspaceRoot = path.resolve(path.dirname(scriptPath), "..", "..");
const templatePath = path.join(workspaceRoot, "automation", "templates", "session-report.md");
const reportsDir = path.join(workspaceRoot, "automation", "reports");
const changelogPath = path.join(workspaceRoot, "automation", "CHANGELOG.md");

function parseArgs(argv) {
  const args = { turn: null, lane: null };
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
    }
  }
  if (args.turn !== null && (!Number.isInteger(args.turn) || args.turn < 1)) {
    throw new Error("--turn must be a positive integer.");
  }
  if (!args.lane) {
    throw new Error("--lane is required.");
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

function buildReportName(turn, lane) {
  return `TURN-${String(turn).padStart(3, "0")}-${lane}.md`;
}

function buildSeededContent(template, turn, lane) {
  const now = new Date().toISOString();
  return template
    .replace("- Turn number:", `- Turn number: ${turn}`)
    .replace("- Lane:", `- Lane: ${lane}`)
    .replace("- Date/time:", `- Date/time: ${now}`)
    .replace("- Changelog entry added:", `- Changelog entry added: ${changelogPath}`)
    .replace("- Short summary line:", `- Short summary line: TURN-${String(turn).padStart(3, "0")} - ${lane}`);
}

function main() {
  const args = parseArgs(process.argv.slice(2));
  const turn = args.turn ?? countReports() + 1;
  const template = fs.readFileSync(templatePath, "utf8");
  fs.mkdirSync(reportsDir, { recursive: true });
  const reportName = buildReportName(turn, args.lane);
  const reportPath = path.join(reportsDir, reportName);
  if (fs.existsSync(reportPath)) {
    process.stdout.write(`${reportPath}\n`);
    return;
  }
  const content = buildSeededContent(template, turn, args.lane);
  fs.writeFileSync(reportPath, content, "utf8");
  process.stdout.write(`${reportPath}\n`);
}

main();
