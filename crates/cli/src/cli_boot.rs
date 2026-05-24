use clap::{ColorChoice, CommandFactory, FromArgMatches};
use kain_commands::blade::BladeCli;
use kain_commands::kain::KainCli;
use kain_commands::shared::LauncherKind;
use kain_commands::ui::{apply_command_ui, CommandUiPreferences, CommandUiTheme};
use kain_core::install_layout::KAIN_CONFIG_ENV_VAR;
use kain_core::tooling_config::{
    install_active_kain_tooling_config, load_kain_tooling_config, supported_theme_names,
    KainColorPreference, ResolvedKainToolingConfig,
};
use std::path::PathBuf;

#[derive(Debug, Default)]
struct CliBootOverrides {
    config_path: Option<PathBuf>,
    color: Option<KainColorPreference>,
    theme: Option<String>,
}

pub fn parse_kain_cli(
    launcher: LauncherKind,
) -> Result<(KainCli, ResolvedKainToolingConfig, Option<String>), String> {
    let config = load_and_install_boot_config()?;
    let preferences = command_ui_preferences(launcher.display_name(), &config);
    let matches = apply_command_ui(
        KainCli::command().bin_name(launcher.display_name()),
        preferences,
    )
    .get_matches();
    let external_command_name = matches.subcommand().map(|(name, _)| name.to_string());
    let args = KainCli::from_arg_matches(&matches).unwrap_or_else(|err| err.exit());
    Ok((args, config, external_command_name))
}

pub fn parse_blade_cli() -> Result<(BladeCli, ResolvedKainToolingConfig), String> {
    let config = load_and_install_boot_config()?;
    let preferences = command_ui_preferences("blade", &config);
    let matches =
        apply_command_ui(BladeCli::command().bin_name("blade"), preferences).get_matches();
    let args = BladeCli::from_arg_matches(&matches).unwrap_or_else(|err| err.exit());
    Ok((args, config))
}

pub fn active_command_ui_preferences(bin: &'static str) -> CommandUiPreferences<'static> {
    let config = kain_core::tooling_config::active_kain_tooling_config();
    command_ui_preferences(bin, &config)
}

fn load_and_install_boot_config() -> Result<ResolvedKainToolingConfig, String> {
    let overrides = scan_boot_overrides()?;
    if let Some(path) = overrides.config_path.as_deref() {
        std::env::set_var(KAIN_CONFIG_ENV_VAR, path);
    }
    let mut config = load_kain_tooling_config(overrides.config_path.as_deref())?;
    if let Some(color) = overrides.color {
        config.ui.color = color;
    }
    if let Some(theme) = overrides.theme {
        config.ui.theme = normalize_theme_override(&theme)?;
    }
    install_active_kain_tooling_config(config.clone());
    Ok(config)
}

fn command_ui_preferences<'a>(
    bin: &'a str,
    config: &ResolvedKainToolingConfig,
) -> CommandUiPreferences<'a> {
    CommandUiPreferences {
        bin,
        theme: CommandUiTheme::from_name(&config.ui.theme),
        color_choice: match config.ui.color {
            KainColorPreference::Auto => ColorChoice::Auto,
            KainColorPreference::Always => ColorChoice::Always,
            KainColorPreference::Never => ColorChoice::Never,
        },
        experimental_help: config.ui.experimental_help,
    }
}

fn normalize_theme_override(theme: &str) -> Result<String, String> {
    let normalized = theme.trim().to_ascii_lowercase();
    if supported_theme_names().contains(&normalized.as_str()) {
        Ok(normalized)
    } else {
        Err(format!(
            "unknown Kain theme `{}`; expected one of {}",
            theme.trim(),
            supported_theme_names().join(", ")
        ))
    }
}

fn scan_boot_overrides() -> Result<CliBootOverrides, String> {
    let argv = std::env::args().collect::<Vec<_>>();
    let mut overrides = CliBootOverrides::default();
    let mut index = 1usize;

    while index < argv.len() {
        let arg = &argv[index];
        if let Some(value) = arg.strip_prefix("--config=") {
            overrides.config_path = Some(PathBuf::from(value));
            index += 1;
            continue;
        }
        if let Some(value) = arg.strip_prefix("--color=") {
            overrides.color = Some(KainColorPreference::parse_str(value)?);
            index += 1;
            continue;
        }
        if let Some(value) = arg.strip_prefix("--theme=") {
            overrides.theme = Some(value.to_string());
            index += 1;
            continue;
        }

        match arg.as_str() {
            "--config" => {
                let value = argv
                    .get(index + 1)
                    .ok_or_else(|| "--config expects a path".to_string())?;
                overrides.config_path = Some(PathBuf::from(value));
                index += 2;
            }
            "--color" => {
                let value = argv
                    .get(index + 1)
                    .ok_or_else(|| "--color expects auto, always, or never".to_string())?;
                overrides.color = Some(KainColorPreference::parse_str(value)?);
                index += 2;
            }
            "--theme" => {
                let value = argv
                    .get(index + 1)
                    .ok_or_else(|| "--theme expects a theme name".to_string())?;
                overrides.theme = Some(value.to_string());
                index += 2;
            }
            _ => {
                index += 1;
            }
        }
    }

    Ok(overrides)
}
