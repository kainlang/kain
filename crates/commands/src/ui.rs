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

    // Apply banner/footer first so they're included when we render the help.
    if preferences.experimental_help {
        command = command.before_help(leak_string(render_help_banner(preferences)));
        command = command.after_help(leak_string(render_help_footer(preferences)));
    }

    // Inject subcommand category headings by post-processing the rendered help.
    command = inject_subcommand_categories(command);

    command
}

/// Post-process the clap-rendered help to inject category section headings
/// between groups of subcommands. Only applies to the static clap-derive help
/// (detected by the presence of "check" as the first subcommand).
fn inject_subcommand_categories(mut root: Command) -> Command {
    let bin_name = root.get_name().to_string();
    if bin_name != "kain" && bin_name != "kn" {
        return root;
    }

    // Only apply to the static derive command tree (starts with "check", "build", …).
    // The dynamic registry tree is alphabetical and handled separately.
    let first_sc = root.get_subcommands().next().map(|sc| sc.get_name().to_string());
    if first_sc.as_deref() != Some("check") {
        return root;
    }

    // Define the category groups and their headings.
    let categories: &[(&str, &[&str])] = &[
        (
            "Core Commands",
            &[
                "  check", "  build", "  run", "  test", "  doctor", "  clean",
                "  format", "  repl", "  watch", "  init",
            ],
        ),
        (
            "Package Commands",
            &["  add", "  install", "  publish", "  amalgamate"],
        ),
        (
            "Import Commands",
            &[
                "  import", "  import-c", "  import-rust", "  import-crate",
                "  import-asm", "  import-ts",
            ],
        ),
        (
            "Tooling Commands",
            &["  lsp", "  config", "  selfhost", "  stdlib-map", "  commands"],
        ),
        (
            "Runtime & Platform",
            &["  runtime", "  native-ui", "  bridge", "  codebase"],
        ),
        (
            "Specialized Commands",
            &["  gpu-artifacts", "  inject", "  omni", "  fabric"],
        ),
    ];

    // Force color on for the render pass if the terminal supports it,
    // so ANSI escape codes are embedded in the override help text.
    let use_color = should_emit_manual_color(root.get_color());
    let mut render_root = if use_color {
        root.clone().color(ColorChoice::Always)
    } else {
        root.clone()
    };

    // Render the full help to a buffer with colors embedded.
    let mut output = Vec::new();
    if render_root.write_help(&mut output).is_err() {
        return root;
    }
    let help_text = String::from_utf8_lossy(&output).to_string();

    // Inject section headings before the first subcommand of each group.
    // We match on the indented command name (two spaces + command name).
    let mut result = help_text.clone();
    for (heading, patterns) in categories.iter() {
        for pattern in *patterns {
            if let Some(pos) = result.find(pattern) {
                // Verify we're at the start of a subcommand line (preceded by newline).
                if pos == 0 || result.as_bytes().get(pos.wrapping_sub(1)) == Some(&b'\n') {
                    let heading_line = format!("\n{heading}:\n");
                    result.insert_str(pos, &heading_line);
                    break;
                }
            }
        }
    }

    // Override the help output with our post-processed version.
    root = root.override_help(leak_string(result));
    root
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
            "kn" => "run-first launcher / hot authoring lane",
            _ => "compiler · interop · native",
        };
        return format!(
            " {bin:^6}\n {tagline}\n",
            bin = preferences.bin.to_ascii_uppercase()
        );
    }

    if matches!(preferences.theme, CommandUiTheme::Plain) {
        let tagline = match preferences.bin {
            "kn" => "run-first launcher / hot authoring lane",
            _ => "compiler · interop · native",
        };
        return format!(
            " {bin:^6}\n {tagline}\n",
            bin = preferences.bin.to_ascii_uppercase()
        );
    }

    let (accent, lead) = theme_banner_accent(command_theme_name(preferences.theme));
    let reset = "\x1b[0m";
    let tagline = match preferences.bin {
        "kn" => "run-first launcher / hot authoring lane",
        _ => "compiler · interop · native",
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
