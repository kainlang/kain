# ProjectSchema ‒ Configuration Schema

> Column type and constraint definitions for config.md tables.
> Each table below mirrors a table in config.md with type annotations.

## Metadata Columns

| Column | Type | Required | Default |
|--------|------|----------|---------|
| Property | string | true |  |
| Value | string | true |  |

## Dependencies Columns

| Column | Type | Required | Default |
|--------|------|----------|---------|
| Name | string | true |  |
| Source | string | true | builtin |
| Version | string | false | * |
| Optional | bool | false | false |

## Build Columns

| Column | Type | Required | Default |
|--------|------|----------|---------|
| ArtifactRoot | string | true | .kain/out |
| CacheRoot | string | true | .kain/cache |
| SourceRoot | string | true | src |
| ModuleRoot | string | true | src |

## Platforms Columns

| Column | Type | Required | Default |
|--------|------|----------|---------|
| OS | string | true |  |
| Arch | string | true | x86_64 |
| Supported | bool | true | false |
| Notes | string | false |  |

## Features Columns

| Column | Type | Required | Default |
|--------|------|----------|---------|
| Feature | string | true |  |
| Enabled | bool | true | false |
| Description | string | true |  |

## Scripts Columns

| Column | Type | Required | Default |
|--------|------|----------|---------|
| Script | string | true |  |
| Path | string | true |  |
| Description | string | true |  |

## Invariants Columns

| Column | Type | Required | Default |
|--------|------|----------|---------|
| # | int | true |  |
| Invariant | string | true |  |
