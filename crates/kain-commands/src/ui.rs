use std::io::IsTerminal;

use clap::builder::styling::{AnsiColor, Effects, Styles};
use clap::{ColorChoice, Command};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CommandUiTheme {
    Hyperpop,
    Ember,
    Glacier,
    Oxide,
}

impl CommandUiTheme {
    pub fn from_name(name: &str) -> Self {
        match name.trim().to_ascii_lowercase().as_str() {
            "ember" => Self::Ember,
            "glacier" => Self::Glacier,
            "oxide" => Self::Oxide,
            _ => Self::Hyperpop,
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
    match theme {
        CommandUiTheme::Hyperpop => Styles::styled()
            .header(AnsiColor::BrightCyan.on_default().effects(Effects::BOLD))
            .usage(AnsiColor::BrightMagenta.on_default().effects(Effects::BOLD))
            .literal(AnsiColor::BrightYellow.on_default().effects(Effects::BOLD))
            .placeholder(AnsiColor::BrightGreen.on_default())
            .error(AnsiColor::BrightRed.on_default().effects(Effects::BOLD))
            .valid(AnsiColor::BrightCyan.on_default().effects(Effects::BOLD))
            .invalid(AnsiColor::BrightYellow.on_default().effects(Effects::BOLD)),
        CommandUiTheme::Ember => Styles::styled()
            .header(AnsiColor::BrightRed.on_default().effects(Effects::BOLD))
            .usage(AnsiColor::BrightYellow.on_default().effects(Effects::BOLD))
            .literal(AnsiColor::BrightMagenta.on_default().effects(Effects::BOLD))
            .placeholder(AnsiColor::BrightCyan.on_default())
            .error(AnsiColor::BrightRed.on_default().effects(Effects::BOLD))
            .valid(AnsiColor::BrightGreen.on_default().effects(Effects::BOLD))
            .invalid(AnsiColor::BrightYellow.on_default().effects(Effects::BOLD)),
        CommandUiTheme::Glacier => Styles::styled()
            .header(AnsiColor::BrightBlue.on_default().effects(Effects::BOLD))
            .usage(AnsiColor::BrightCyan.on_default().effects(Effects::BOLD))
            .literal(AnsiColor::BrightWhite.on_default().effects(Effects::BOLD))
            .placeholder(AnsiColor::BrightBlue.on_default())
            .error(AnsiColor::BrightRed.on_default().effects(Effects::BOLD))
            .valid(AnsiColor::BrightGreen.on_default().effects(Effects::BOLD))
            .invalid(AnsiColor::BrightYellow.on_default().effects(Effects::BOLD)),
        CommandUiTheme::Oxide => Styles::styled()
            .header(AnsiColor::Red.on_default().effects(Effects::BOLD))
            .usage(AnsiColor::Yellow.on_default().effects(Effects::BOLD))
            .literal(AnsiColor::Green.on_default().effects(Effects::BOLD))
            .placeholder(AnsiColor::BrightWhite.on_default())
            .error(AnsiColor::BrightRed.on_default().effects(Effects::BOLD))
            .valid(AnsiColor::Green.on_default().effects(Effects::BOLD))
            .invalid(AnsiColor::Yellow.on_default().effects(Effects::BOLD)),
    }
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

    let (lead, accent, _tail) = match preferences.theme {
        CommandUiTheme::Hyperpop => ("\x1b[38;2;92;225;230m", "\x1b[38;2;255;89;168m", "\x1b[38;2;255;206;86m"),
        CommandUiTheme::Ember => ("\x1b[38;2;255;119;51m", "\x1b[38;2;255;184;77m", "\x1b[38;2;255;84;112m"),
        CommandUiTheme::Glacier => ("\x1b[38;2;113;205;255m", "\x1b[38;2;171;244;255m", "\x1b[38;2;214;230;255m"),
        CommandUiTheme::Oxide => ("\x1b[38;2;210;90;58m", "\x1b[38;2;255;185;90m", "\x1b[38;2;196;214;110m"),
    };
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
    let theme_name = match preferences.theme {
        CommandUiTheme::Hyperpop => "hyperpop",
        CommandUiTheme::Ember => "ember",
        CommandUiTheme::Glacier => "glacier",
        CommandUiTheme::Oxide => "oxide",
    };
    format!(
        "Theme: {theme_name}  Override: --theme <name> --color <auto|always|never>  Config: ~/.kain/config.toml"
    )
}

fn leak_string(value: String) -> &'static str {
    Box::leak(value.into_boxed_str())
}
