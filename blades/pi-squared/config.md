# PiSquaredConfig — pi-squared Configuration

> Project-level configuration stored as markscript data tables.
> Validated against schemas/pi_squared_schema.md.

## Metadata

| Property | Value |
|----------|-------|
| Name | pi-squared |
| Version | 0.1.0 |
| Kind | kain_executable |
| Entry | src/main.kn |
| Target | llvm |
| Profile | debug |

## Build

| ArtifactRoot | CacheRoot | SourceRoot | ModuleRoot |
|-------------|-----------|------------|------------|
| .kain/out | .kain/cache | src | src |

## ConfigLayers

| Layer | Priority | File | Format |
|-------|----------|------|--------|
| CodeDefaults | 1 | (builtin) | code |
| UserConfig | 2 | ~/.pi/agent/settings.json | json |
| ProjectConfig | 3 | .pi/settings.json | json |
| CliFlags | 4 | (argv) | flags |

## Actors

| ActorName | Purpose | Messages |
|-----------|---------|----------|
| PiSettingsManager | Layered config merge | GetSetting, SetSetting, GetEffective, ApplyCliOverrides |
| SessionTree | Session persistence + tree | AppendEntry, Branch, GetContext, GetTree, LoadFile |
| ResourceLoader | Extension/skill loading | Reload, LoadSkills, LoadPromptTemplates, LoadContextFiles |
| LlmProviderRegistry | Provider registration | get_provider, provider_available |
| AgentEventBus | Lifecycle event dispatch | Subscribe, Unsubscribe, Emit |

## Pipelines

| Pipeline | Stages | Trigger |
|----------|--------|---------|
| LLMComplete | Build→Call→Parse→Accumulate→Validate | User prompt |
| Compact | Analyze→Summarize→Apply | Token threshold |
| Startup | Init→Parse→Migrate→Load→Resolve→Ready | Process start |
