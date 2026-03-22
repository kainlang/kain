# Kain Bootstrap Folder Guide

This folder holds bootstrap assets and selfhost scaffolding for the Kain toolchain.

## What Lives Here

- selfhost helpers that stabilize Kain when the compiler is rebuilding itself
- scripts or inputs used for early-stage builds and recovery

## Output Hygiene

- treat any generated binaries or build caches as disposable
- keep this folder focused on reproducible inputs, not outputs

## When You Add Something

- include a short note explaining which stage of the bootstrap flow it supports
- prefer data-driven config files over hardcoded paths