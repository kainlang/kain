#!/usr/bin/env python3
"""Stub for bundle_resources.py - creates empty output file."""
import sys
if len(sys.argv) >= 1:
    open(sys.argv[1], 'w').close()
