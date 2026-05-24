import argparse
import subprocess
import sys


def split_columns(line: str) -> list[str]:
    return [column.strip() for column in line.split("  ") if column.strip()]


def parse_line(line: str) -> dict[str, object] | None:
    columns = split_columns(line.strip())
    if not columns:
        return None

    payload: dict[str, object] = {
        "path": columns[0],
        "bins": [],
        "pack": "",
        "handler": "",
        "source": "",
        "tags": [],
        "about": "",
    }

    if len(columns) > 1:
        bins_text = columns[1]
        if bins_text.startswith("[") and bins_text.endswith("]"):
            bins_text = bins_text[1:-1]
        payload["bins"] = [item.strip() for item in bins_text.split(",") if item.strip()]

    index = 2
    while index < len(columns):
        column = columns[index]
        if column.startswith("pack="):
            payload["pack"] = column[5:]
            index += 1
            continue
        if column.startswith("handler="):
            payload["handler"] = column[8:]
            index += 1
            continue
        if column.startswith("source="):
            source_part, _, tags_part = column.partition(" tags=")
            payload["source"] = source_part[7:]
            if tags_part:
                payload["tags"] = [item.strip() for item in tags_part.split(",") if item.strip()]
            index += 1
            continue
        if column.startswith("tags="):
            payload["tags"] = [item.strip() for item in column[5:].split(",") if item.strip()]
            index += 1
            continue
        payload["about"] = "  ".join(columns[index:])
        break

    return payload


def build_haystack(payload: dict[str, object]) -> str:
    tags = " ".join(payload.get("tags", []))
    return " ".join(
        [
            str(payload.get("path", "")),
            str(payload.get("pack", "")),
            str(payload.get("handler", "")),
            str(payload.get("source", "")),
            str(payload.get("about", "")),
            tags,
        ]
    ).lower()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--kain-bin", required=True)
    parser.add_argument("--bin", dest="bin_name", default="kain")
    parser.add_argument("--mode", choices=["list", "describe"], required=True)
    parser.add_argument("--query", default="")
    parser.add_argument("--pack", default="")
    parser.add_argument("--limit", type=int, default=20)
    parser.add_argument("--runtime", action="store_true")
    args = parser.parse_args()

    command = [args.kain_bin, "commands", "list", "--bin", args.bin_name]
    if args.runtime:
        command.append("--runtime")

    completed = subprocess.run(command, capture_output=True, text=True)
    if completed.returncode != 0:
        if completed.stderr:
            sys.stderr.write(completed.stderr)
        return completed.returncode

    query_lower = args.query.strip().lower()
    pack_filter = args.pack.strip().lower()
    limit = max(args.limit, 1)
    matched_lines: list[str] = []

    for raw_line in completed.stdout.splitlines():
        line = raw_line.strip()
        if not line:
            continue
        payload = parse_line(line)
        if payload is None:
            continue
        if pack_filter and str(payload.get("pack", "")).lower() != pack_filter:
            continue

        if args.mode == "describe":
            if str(payload.get("path", "")).lower() == query_lower:
                print(line)
                return 0
            continue

        if query_lower and query_lower not in build_haystack(payload):
            continue

        matched_lines.append(line)
        if len(matched_lines) >= limit:
            break

    if args.mode == "describe":
        return 3

    if matched_lines:
        print("\n".join(matched_lines))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
