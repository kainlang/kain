# PiSquaredSchema — Configuration Schema

> Column type and constraint definitions for config.md tables.
> Each table below mirrors a table in config.md with type annotations.

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
