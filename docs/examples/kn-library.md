# KAIN Library

`kn_library/` is the curated corpus of Kain source files collected from the workspace. It is a language corpus, not a product scaffold.

## How To Read It

Use this folder when you want to see the language used repeatedly across real modules, not just once in a tutorial snippet. It is especially useful for spotting common idioms, feature combinations, and naming patterns that appear across the codebase.

## Purpose

The library is used for:

- training data
- pattern mining
- examples and learning
- corpus analysis

## Structure

- `actors/`
- `shaders/`
- `editor/`
- `components/`
- `datatables/`
- `utilities/`
- `examples/`

## Why It Matters

This folder is a compact map of real language usage. It is one of the best places to see how Kain syntax is used across gameplay, shader, editor, and utility code.

## Maintenance

The corpus is generated, deduplicated, and searchable. Treat it like a living reference corpus rather than a random example dump. When the docs and corpus disagree, prefer the corpus for idiom discovery and the live source for truth about current behavior.
