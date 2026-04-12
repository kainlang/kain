# Kain Smoketest Folder Guide

This folder is the proof matrix for Kain's runtime bridges, UI, GPU, and mixed-language workflows.

Canonical smoke guidance lives in [`../guides/examples/smoketest.md`](../guides/examples/smoketest.md)
and the root guide tree at [`../guides/README.md`](../guides/README.md).

## What Lives Here

- focused smoke apps that validate a single capability or integration
- batch scripts that run the smoke lane quickly

## Output Hygiene

- treat generated native apps, build outputs, and `target/` folders as disposable
- keep only the minimal inputs needed to regenerate smoke outputs

## When Adding A Smoke

- include a README inside the smoke folder with run steps
- record expected outputs and where they are emitted
