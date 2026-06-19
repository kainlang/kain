# PiSquaredSchema — Configuration Schema

> Column type and constraint definitions for config.md tables.
> Each table below mirrors a table in config.md with type annotations.
> ALL tables are parsed by markscript_loader.kn at startup.

## Metadata Columns

| Column | Type | Required | Default |
|--------|------|----------|---------|
| Property | string | true |  |
| Value | string | true |  |

## Build Columns

| Column | Type | Required | Default |
|--------|------|----------|---------|
| ArtifactRoot | string | true | .kain/out |
| CacheRoot | string | true | .kain/cache |
| SourceRoot | string | true | src |
| ModuleRoot | string | true | src |

## ConfigLayers Columns

| Column | Type | Required | Default |
|--------|------|----------|---------|
| Layer | string | true |  |
| Priority | int | true |  |
| File | string | true |  |
| Format | string | false | json |

## ModelProviders Columns

| Column | Type | Required | Default |
|--------|------|----------|---------|
| Provider | string | true |  |
| Api | string | true |  |
| AuthType | string | true | env |
| DefaultModel | string | false |  |

## Tools Columns

| Column | Type | Required | Default |
|--------|------|----------|---------|
| ToolName | string | true |  |
| Description | string | true |  |
| Effect | string | true | IO |

## Actors Columns

| Column | Type | Required | Default |
|--------|------|----------|---------|
| ActorName | string | true |  |
| Purpose | string | true |  |
| Messages | string | true |  |

## Pipelines Columns

| Column | Type | Required | Default |
|--------|------|----------|---------|
| Pipeline | string | true |  |
| Stages | string | true |  |
| Trigger | string | false | manual |

---

### CONFIG-DRIVEN EVERYTHING — Schema for all DOOM-mode config tables

## Keybindings Columns

| Column | Type | Required | Default |
|--------|------|----------|---------|
| Key | string | true |  |
| Action | string | true |  |
| Mode | string | true | global |
| Description | string | false |  |

## Theme Columns

| Column | Type | Required | Default |
|--------|------|----------|---------|
| Property | string | true |  |
| Value | string | true |  |
| Type | string | true | string |
| Overridable | string | false | yes |

## Animations Columns

| Column | Type | Required | Default |
|--------|------|----------|---------|
| Property | string | true |  |
| Value | string | true |  |
| Unit | string | false |  |
| Default | string | false |  |

## Plugins Columns

| Column | Type | Required | Default |
|--------|------|----------|---------|
| Name | string | true |  |
| Source | string | true | builtin |
| Enabled | bool | true | true |
| Description | string | false |  |

## ModelDefaults Columns

| Column | Type | Required | Default |
|--------|------|----------|---------|
| Provider | string | true |  |
| DefaultModel | string | true |  |
| Api | string | true |  |
| AuthType | string | false | env |

## SessionPaths Columns

| Column | Type | Required | Default |
|--------|------|----------|---------|
| Setting | string | true |  |
| Path | string | true |  |
| Default | string | false |  |

## Startup Columns

| Column | Type | Required | Default |
|--------|------|----------|---------|
| Setting | string | true |  |
| Value | string | true |  |
| Type | string | true |  |
| Description | string | false |  |

## SplashArt Columns

| Column | Type | Required | Default |
|--------|------|----------|---------|
| Line | int | true |  |
| Text | string | true |  |
| Style | string | false | normal |

## Invariants

| # | Invariant |
|---|-----------|
| 1 | Metadata table must exist and contain Name, Version, Entry |
| 2 | Keybindings must have unique Key+Mode combination |
| 3 | Theme Property "name" is required |
| 4 | All Animation values are non-negative |
| 5 | Plugin entries with Enabled=true have valid Source |
| 6 | Startup boolean settings are validated as "true" or "false" |
| 7 | SplashArt lines with empty Text are valid separators |
