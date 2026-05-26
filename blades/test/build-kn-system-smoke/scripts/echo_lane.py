#!/usr/bin/env python3
import argparse
import json
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description="Emit a tiny success payload for build.kn smoke tasks.")
    parser.add_argument("--lane", required=True)
    parser.add_argument("--output")
    args = parser.parse_args()

    payload = {
        "ok": True,
        "lane": args.lane,
    }
    if args.output:
        output_path = Path(args.output)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    print(json.dumps(payload))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
