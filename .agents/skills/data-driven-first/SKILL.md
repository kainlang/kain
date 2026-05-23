---
name: data-driven-first
description: Enforce a data-driven-first coding approach. Use when designing or editing code that might hardcode paths, routes, URLs, versions, endpoints, file locations, feature switches, mappings, or similar values. Prefer configuration, schemas, lookup tables, and environment-driven values over inline constants across all languages and frameworks.
---

# Data-Driven First

## Overview

Apply a default rule: think in data structures before hardcoding values.
Replace hardcoded paths and constants with configurable, typed, and validated data whenever practical.

## Core Rule

Do not hardcode values that can change by environment, version, deployment, tenant, region, or product evolution.
Represent those values as data.

## Decision Flow

1. Identify candidate hardcoded values in the task.
2. Ask whether each value could change over time or differ by context.
3. If yes, move it into a data source:
   - Configuration file
   - Environment variable
   - Typed settings object
   - Route or endpoint registry
   - Declarative mapping/table/schema
4. Keep a single source of truth and reference it from code.
5. Add validation/defaulting at load time and fail early on invalid configuration.

## Preferred Patterns

- Use `base_url`, `api_version`, and route keys in config rather than inline URL strings.
- Use per-environment config (dev/stage/prod) rather than `if` ladders with string literals.
- Use dictionaries/maps for path or behavior selection rather than repeated branching.
- Use typed config structs/classes/interfaces for discoverability and validation.
- Use migration-friendly keys and schemas so values can evolve without code churn.

## Language-Agnostic Guidance

- Rust: prefer strongly typed config structs, enums, and serde-deserialized files/env.
- Python: prefer settings modules, dataclasses/pydantic models, and env/config loaders.
- JavaScript/TypeScript: prefer central config modules, typed interfaces, and route maps.
- Apply the same principle in any language: data first, hardcoded constants last.

## Output Expectations

When applying this skill in edits:
- Introduce or extend configuration/data structures before changing call sites.
- Minimize scattered literals by consolidating into one source.
- Preserve backward compatibility when possible by providing defaults.
- Note tradeoffs only when data-driven refactoring is not worth complexity.
