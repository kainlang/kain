#!/usr/bin/env python3

def run(fabric_inputs):
    return {
        "summary": {
            "ok": True,
            "lane": "fabric",
            "input_keys": sorted(fabric_inputs.keys()),
        }
    }
