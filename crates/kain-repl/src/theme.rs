use kain_core::tooling_config::active_kain_tooling_config;
use kain_lattice::{supported_theme_names, theme_by_name, SemanticRole};
use ratatui::style::{Color, Modifier, Style};

pub const DEFAULT_REPL_THEME: &str = "plain";

#[derive(Debug, Clone, Copy)]
pub struct ReplPalette {
    pub chrome_accent: Color,
    pub chrome_secondary: Color,
    pub chrome_muted: Color,
    pub border: Color,
    pub border_focus: Color,
    pub panel_background: Color,
    pub panel_background_active: Color,
    pub text_primary: Color,
    pub text_muted: Color,
    pub text_subtle: Color,
    pub status_fg: Color,
    pub status_bg: Color,
    pub title_info: Color,
    pub title_success: Color,
    pub title_error: Color,
    pub number: Color,
    pub string: Color,
    pub identifier_type: Color,
    pub identifier_plain: Color,
    pub keyword: Color,
    pub keyword_type: Color,
    pub keyword_effect: Color,
    pub keyword_actor: Color,
    pub keyword_world: Color,
    pub keyword_ownership: Color,
    pub keyword_proof: Color,
    pub keyword_shader: Color,
    pub operator: Color,
    pub directive: Color,
    pub invalid: Color,
}

pub fn active_repl_theme_name() -> String {
    let active = active_kain_tooling_config();
    if active.ui.theme == "plain" || active.ui.theme == "lattice" {
        return active.ui.theme;
    }
    if active.ui.theme == "slate" {
        return DEFAULT_REPL_THEME.to_string();
    }
    active.ui.theme
}

pub fn repl_palette(theme_name: &str) -> ReplPalette {
    let theme = theme_by_name(theme_name);
    ReplPalette {
        chrome_accent: theme.tone(SemanticRole::UiChromeTitle).ratatui_color(),
        chrome_secondary: theme.tone(SemanticRole::UiChromeAccent).ratatui_color(),
        chrome_muted: theme.tone(SemanticRole::UiChromeMuted).ratatui_color(),
        border: theme.tone(SemanticRole::UiBorder).ratatui_color(),
        border_focus: theme.tone(SemanticRole::UiBorderFocus).ratatui_color(),
        panel_background: theme.tone(SemanticRole::UiPanelBackground).ratatui_color(),
        panel_background_active: theme
            .tone(SemanticRole::UiPanelBackgroundActive)
            .ratatui_color(),
        text_primary: theme.tone(SemanticRole::UiTextPrimary).ratatui_color(),
        text_muted: theme.tone(SemanticRole::UiTextMuted).ratatui_color(),
        text_subtle: theme.tone(SemanticRole::UiTextSubtle).ratatui_color(),
        status_fg: theme.tone(SemanticRole::UiStatusForeground).ratatui_color(),
        status_bg: theme.tone(SemanticRole::UiStatusBackground).ratatui_color(),
        title_info: theme.tone(SemanticRole::DiagNote).ratatui_color(),
        title_success: theme.tone(SemanticRole::SyntaxFamilyWorld).ratatui_color(),
        title_error: theme.tone(SemanticRole::DiagError).ratatui_color(),
        number: theme.tone(SemanticRole::SyntaxNumber).ratatui_color(),
        string: theme.tone(SemanticRole::SyntaxString).ratatui_color(),
        identifier_type: theme.tone(SemanticRole::SyntaxType).ratatui_color(),
        identifier_plain: theme.tone(SemanticRole::SyntaxIdentifier).ratatui_color(),
        keyword: theme.tone(SemanticRole::SyntaxKeywordCore).ratatui_color(),
        keyword_type: theme.tone(SemanticRole::SyntaxKeywordType).ratatui_color(),
        keyword_effect: theme
            .tone(SemanticRole::SyntaxKeywordEffect)
            .ratatui_color(),
        keyword_actor: theme.tone(SemanticRole::SyntaxFamilyActor).ratatui_color(),
        keyword_world: theme.tone(SemanticRole::SyntaxFamilyWorld).ratatui_color(),
        keyword_ownership: theme
            .tone(SemanticRole::SyntaxFamilyOwnership)
            .ratatui_color(),
        keyword_proof: theme.tone(SemanticRole::SyntaxFamilyProof).ratatui_color(),
        keyword_shader: theme.tone(SemanticRole::SyntaxFamilyShader).ratatui_color(),
        operator: theme.tone(SemanticRole::SyntaxOperator).ratatui_color(),
        directive: theme.tone(SemanticRole::SyntaxDirective).ratatui_color(),
        invalid: theme.tone(SemanticRole::SyntaxInvalid).ratatui_color(),
    }
}

pub fn repl_theme_names() -> Vec<&'static str> {
    supported_theme_names().to_vec()
}

pub fn cycle_repl_theme_name(current: &str, reverse: bool) -> String {
    let names = supported_theme_names();
    let fallback_index = names
        .iter()
        .position(|name| *name == DEFAULT_REPL_THEME)
        .unwrap_or(0);
    let current_index = names
        .iter()
        .position(|name| *name == current)
        .unwrap_or(fallback_index);
    let next_index = if reverse {
        current_index.checked_sub(1).unwrap_or(names.len() - 1)
    } else {
        (current_index + 1) % names.len()
    };
    names[next_index].to_string()
}

impl ReplPalette {
    pub fn title_style(self) -> Style {
        Style::default()
            .fg(self.chrome_accent)
            .add_modifier(Modifier::BOLD)
    }

    pub fn muted_style(self) -> Style {
        Style::default().fg(self.text_subtle)
    }
}
