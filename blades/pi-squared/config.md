# PiSquaredConfig — pi-squared Configuration

> Project-level configuration stored as markscript data tables.
> Validated against schemas/pi_squared_schema.md.
> 
> ALL tables below are read by markscript_loader.kn at startup to drive
> every configurable aspect of the TUI. No hardcoded values survive —
> everything comes from these tables.

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

---

### CONFIG-DRIVEN EVERYTHING — All tables below are read by markscript_loader.kn

---

## Keybindings

| Key | Action | Mode | Description |
|-----|--------|------|-------------|
| Ctrl+P | OpenCommandPalette | global | Open the command palette |
| Ctrl+E | OpenEditor | global | Open multi-line editor |
| Ctrl+D | ScrollDown | conversation | Scroll conversation down |
| Ctrl+U | ScrollUp | conversation | Scroll conversation up |
| Ctrl+S | OpenSessionSelector | global | Open session list |
| Ctrl+O | OpenModelSelector | global | Open model selector |
| Alt+Enter | Submit | editor | Submit from editor |
| Ctrl+L | Cascade | global | Continue LLM generation |
| Ctrl+N | NewSession | global | Create new session |
| Ctrl+F | ForkSession | global | Fork from current position |
| Ctrl+Shift+C | Compact | global | Compact session |
| Ctrl+T | ToggleThinking | global | Toggle thinking mode |
| Ctrl+Shift+T | ToggleTools | global | Toggle tools |
| Alt+Up | ScrollUp | conversation | Scroll up 1 line |
| Alt+Down | ScrollDown | conversation | Scroll down 1 line |
| PageUp | PageUp | conversation | Page up |
| PageDown | PageDown | conversation | Page down |
| Escape | Abort | global | Abort generation / close panel |
| Enter | Submit | input | Submit current input |
| Ctrl+R | Search | conversation | Search through conversation |
| Alt+S | StatusBarToggle | global | Toggle status bar |
| Alt+F | FpsToggle | global | Toggle FPS counter |
| Ctrl+H | ShowHelp | global | Show help overlay |
| Ctrl+Q | Quit | global | Quit application |
| Tab | TabComplete | input | Complete command |
| Up | CommandHistoryPrev | input | Previous command history |
| Down | CommandHistoryNext | input | Next command history |
| Ctrl+Space | TogglePauseMenu | global | Open/close pause menu |

## Theme

| Property | Value | Type | Overridable |
|----------|-------|------|-------------|
| name | dracula | string | yes |
| background | #282a36 | hex | yes |
| foreground | #f8f8f2 | hex | yes |
| accent | #ff79c6 | hex | yes |
| success | #50fa7b | hex | yes |
| warning | #f1fa8c | hex | yes |
| error | #ff5555 | hex | yes |
| info | #8be9fd | hex | yes |
| muted | #6272a4 | hex | yes |
| border | #44475a | hex | yes |
| selection | #44475a | hex | yes |
| cursor | #ff79c6 | hex | yes |
| scrollbar | #44475a | hex | yes |
| link | #8be9fd | hex | yes |
| code_bg | #21222c | hex | yes |
| code_fg | #f8f8f2 | hex | yes |
| heading | #ffb86c | hex | yes |
| list_bullet | #ff79c6 | hex | yes |
| quote_bar | #bd93f9 | hex | yes |

## Animations

| Property | Value | Unit | Default |
|----------|-------|------|---------|
| typing_speed_ms | 10 | ms | 10 |
| loader_frame_interval_ms | 80 | ms | 80 |
| toast_fade_in_ms | 200 | ms | 200 |
| toast_fade_out_ms | 500 | ms | 500 |
| toast_linger_ms | 3000 | ms | 3000 |
| splash_duration_ms | 2000 | ms | 2000 |
| scroll_animation_ms | 100 | ms | 100 |
| status_bar_update_ms | 1000 | ms | 1000 |
| fps_update_interval_ms | 500 | ms | 500 |
| cursor_blink_ms | 530 | ms | 530 |
| smooth_scroll_enabled | true | bool | true |
| pause_menu_blur | true | bool | true |

## Plugins

| Name | Source | Enabled | Description |
|------|--------|---------|-------------|
| tools | builtin | true | Tool execution |
| skills | builtin | true | Skill loading |
| extensions | builtin | true | Extension loading |
| themes | builtin | true | Theme loading |
| commands | builtin | true | Command processing |

## ModelDefaults

| Provider | DefaultModel | Api | AuthType |
|----------|-------------|-----|----------|
| anthropic | claude-sonnet-4-20250514 | anthropic-messages | env |
| openai | gpt-4o | openai-completions | env |
| google | gemini-2.5-pro | google-generative-ai | env |
| mistral | mistral-large | mistral-conversations | env |
| github-copilot | gpt-4o | openai-responses | env |

## SessionPaths

| Setting | Path | Default |
|---------|------|---------|
| session_dir | ~/.pi/sessions | ~/.pi/sessions |
| log_dir | ~/.pi/logs | ~/.pi/logs |
| cache_dir | ~/.pi/cache | ~/.pi/cache |
| config_dir | ~/.pi | ~/.pi |
| theme_dir | ~/.pi/themes | ~/.pi/themes |
| extension_dir | ~/.pi/extensions | ~/.pi/extensions |
| skill_dir | ~/.pi/skills | ~/.pi/skills |
| prompts_dir | ~/.pi/prompts | ~/.pi/prompts |

## Startup

| Setting | Value | Type | Description |
|---------|-------|------|-------------|
| show_splash | true | bool | Show ASCII art splash at startup |
| quiet_startup | false | bool | Skip startup diagnostics |
| show_status_bar | true | bool | Show footer status bar |
| show_fps | false | bool | Show FPS counter in corner |
| enable_toasts | true | bool | Enable toast notifications |
| enable_tab_complete | true | bool | Enable tab completion |
| command_history_size | 100 | int | Max command history entries |
| scrollback_lines | 10000 | int | Max scrollback lines |
| snapshot_interval_ms | 5000 | int | Session auto-save interval |

## SplashArt

| Line | Text | Style |
|------|------|-------|
| 1 | ██████╗ ██╗        ███████╗ ██████╗ ██╗   ██╗ █████╗ ██████╗ ███████╗██████╗ | heading |
| 2 | ██╔══██╗██║        ██╔════╝██╔═══██╗██║   ██║██╔══██╗██╔══██╗██╔════╝██╔══██╗ | heading |
| 3 | ██████╔╝██║        ███████╗██║   ██║██║   ██║███████║██████╔╝█████╗  ██║  ██║ | accent |
| 4 | ██╔═══╝ ██║        ╚════██║██║▄▄ ██║██║   ██║██╔══██║██╔══██╗██╔══╝  ██║  ██║ | accent |
| 5 | ██║     ██║        ███████║╚██████╔╝╚██████╔╝██║  ██║██║  ██║███████╗██████╔╝ | info |
| 6 | ╚═╝     ╚═╝        ╚══════╝ ╚══▀▀═╝  ╚═════╝ ╚═╝  ╚═╝╚═╝  ╚═╝╚══════╝╚═════╝ | info |
| 7 |                                                                                   | |
| 8 |                      pi-squared TUI v0.1.0 — DOOM EDITION                       | heading |
