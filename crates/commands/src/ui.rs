use std::io::IsTerminal;

use clap::builder::styling::Styles;
use clap::{ColorChoice, Command};
use kain_lattice::{theme_banner_accent, theme_by_name};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CommandUiTheme {
    Plain,
    Lattice,
    Slate,
    Graphite,
    Arctic,
    Sandstone,
}

impl CommandUiTheme {
    pub fn from_name(name: &str) -> Self {
        match name.trim().to_ascii_lowercase().as_str() {
            "plain" => Self::Plain,
            "lattice" => Self::Lattice,
            "graphite" | "oxide" => Self::Graphite,
            "arctic" | "glacier" => Self::Arctic,
            "sandstone" | "ember" => Self::Sandstone,
            _ => Self::Slate,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct CommandUiPreferences<'a> {
    pub bin: &'a str,
    pub theme: CommandUiTheme,
    pub color_choice: ColorChoice,
    pub experimental_help: bool,
}

pub fn apply_command_ui(mut command: Command, preferences: CommandUiPreferences<'_>) -> Command {
    command = command.color(preferences.color_choice);
    command = command.styles(styles_for_theme(preferences.theme));
    if preferences.experimental_help {
        command = command.before_help(leak_string(render_help_banner(preferences)));
        command = command.after_help(leak_string(render_help_footer(preferences)));
    }
    command
}

fn should_emit_manual_color(choice: ColorChoice) -> bool {
    match choice {
        ColorChoice::Always => true,
        ColorChoice::Never => false,
        _ => std::io::stdout().is_terminal(),
    }
}

fn styles_for_theme(theme: CommandUiTheme) -> Styles {
    theme_by_name(command_theme_name(theme)).clap_styles()
}

fn render_help_banner(preferences: CommandUiPreferences<'_>) -> String {
    if !should_emit_manual_color(preferences.color_choice) {
        let tagline = match preferences.bin {
            "blade" => "workspace forge / build graph blade runner",
            "kn" => "run-first launcher / hot authoring lane",
            _ => "compiler / interop / native weirdness",
        };
        return format!(
            " {bin:^6}\n {tagline}\n",
            bin = preferences.bin.to_ascii_uppercase()
        );
    }

    if matches!(preferences.theme, CommandUiTheme::Plain) {
        let tagline = match preferences.bin {
            "blade" => "workspace forge / build graph blade runner",
            "kn" => "run-first launcher / hot authoring lane",
            _ => "compiler / interop / native weirdness",
        };
        return format!(
            " {bin:^6}\n {tagline}\n",
            bin = preferences.bin.to_ascii_uppercase()
        );
    }

    let (accent, lead) = theme_banner_accent(command_theme_name(preferences.theme));
    let reset = "\x1b[0m";
    let tagline = match preferences.bin {
        "blade" => "workspace forge / build graph blade runner",
        "kn" => "run-first launcher / hot authoring lane",
        _ => "compiler / interop / native weirdness",
    };
    format!(
        "{lead}╭─{accent} {bin:^6} {lead}─╮{reset}\n{accent} {tagline}{reset}\n",
        bin = preferences.bin.to_ascii_uppercase()
    )
}

fn render_help_footer(preferences: CommandUiPreferences<'_>) -> String {
    let theme_name = command_theme_name(preferences.theme);
    format!(
        "Theme: {theme_name}  Override: --theme <name> --color <auto|always|never>  Config: nearest .kain/config.toml, KAIN_CONFIG, or KAIN_HOME/config.toml"
    )
}

fn command_theme_name(theme: CommandUiTheme) -> &'static str {
    match theme {
        CommandUiTheme::Plain => "plain",
        CommandUiTheme::Lattice => "lattice",
        CommandUiTheme::Slate => "slate",
        CommandUiTheme::Graphite => "graphite",
        CommandUiTheme::Arctic => "arctic",
        CommandUiTheme::Sandstone => "sandstone",
    }
}

fn leak_string(value: String) -> &'static str {
    Box::leak(value.into_boxed_str())
}
