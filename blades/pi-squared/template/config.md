# ProjectConfig — my-kain-app Configuration

> Project-level configuration stored as markscript data tables.
> Validated against schemas/project_schema.md.

## Metadata

| Property | Value |
|----------|-------|
| Name | my-kain-app |
| Version | 0.1.0 |
| Kind | kain_executable |
| Entry | src/main.kn |
| Target | llvm |
| Profile | debug |
| Language | Kain |

## Dependencies

| Name | Source | Version | Optional |
|------|--------|---------|----------|
| std | builtin | * | false |

## Build

| ArtifactRoot | CacheRoot | SourceRoot | ModuleRoot |
|-------------|-----------|------------|------------|
| .kain/out | .kain/cache | src | src |

## Platforms

| OS | Arch | Supported | Notes |
|----|------|-----------|-------|
| windows | x86_64 | true | Primary target |
| linux | x86_64 | true | CI target |
| macos | arm64 | false | Not yet supported |

## Features

| Feature | Enabled | Description |
|---------|---------|-------------|
| ui | false | User interface |
| logging | true | Console output |
| telemetry | false | Metrics and tracing |
| gpu | false | GPU compute |

## Scripts

| Script | Path | Description |
|--------|------|-------------|
| build | scripts/build.md | Full build pipeline |
| dev | scripts/dev.md | Development loop |
| test | scripts/test.md | Test runner |
| clean | scripts/clean.md | Clean artifacts |
| help | scripts/help.md | CLI help |

## Invariants

| # | Invariant |
|---|-----------|
| 1 | Entry point exists at src/main.kn |
| 2 | All Script paths are valid .md files |
| 3 | Platform with Supported=true has valid OS+Arch |
| 4 | Build directories use relative paths |
