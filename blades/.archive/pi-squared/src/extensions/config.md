# PiSquared Config

The markscript-powered configuration for pi-squared. This file is loaded at startup to determine which plugins to load, their enable/disable status, and global settings overrides.

## Metadata
| Property | Value |
|----------|-------|
| config_version | 1.0.0 |
| auto_load_plugins | true |
| plugin_dir | plugins/ |
| default_enabled | true |

## Plugins
| Plugin | Enabled | Path | Description |
|--------|---------|------|-------------|
| calculator | true | extensions/examples/calculator.md | Evaluate math expressions |
| game_of_life | true | extensions/examples/game_of_life.md | Conway's Game of Life in TUI |

## Settings
| Property | Value |
|----------|-------|
| tui_widget_sidebar | right |
| tui_widget_width | 35 |
| tui_widget_refresh_ms | 5000 |
| show_startup_banner | true |

> load plugin configuration
