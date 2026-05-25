use clap::builder::styling::{AnsiColor, Effects, Styles};
use once_cell::sync::Lazy;
use ratatui::style::{Color, Modifier, Style};
use serde::Deserialize;
use std::collections::BTreeMap;
use toml::Value;

pub const SUPPORTED_THEME_NAMES: &[&str] = &[
    "plain",
    "lattice",
    "slate",
    "graphite",
    "arctic",
    "sandstone",
];

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum SemanticRole {
    UiChromeTitle,
    UiChromeAccent,
    UiChromeMuted,
    UiBorder,
    UiBorderFocus,
    UiPanelBackground,
    UiPanelBackgroundActive,
    UiTextPrimary,
    UiTextMuted,
    UiTextSubtle,
    UiStatusForeground,
    UiStatusBackground,
    DiagError,
    DiagWarning,
    DiagNote,
    DiagHelp,
    DiagGutter,
    DiagPointer,
    SyntaxDirective,
    SyntaxInvalid,
    SyntaxComment,
    SyntaxNumber,
    SyntaxString,
    SyntaxType,
    SyntaxIdentifier,
    SyntaxKeywordCore,
    SyntaxKeywordType,
    SyntaxKeywordEffect,
    SyntaxFamilyActor,
    SyntaxFamilyWorld,
    SyntaxFamilyOwnership,
    SyntaxFamilyProof,
    SyntaxFamilyShader,
    SyntaxOperator,
}

impl SemanticRole {
    pub const ALL: [Self; 34] = [
        Self::UiChromeTitle,
        Self::UiChromeAccent,
        Self::UiChromeMuted,
        Self::UiBorder,
        Self::UiBorderFocus,
        Self::UiPanelBackground,
        Self::UiPanelBackgroundActive,
        Self::UiTextPrimary,
        Self::UiTextMuted,
        Self::UiTextSubtle,
        Self::UiStatusForeground,
        Self::UiStatusBackground,
        Self::DiagError,
        Self::DiagWarning,
        Self::DiagNote,
        Self::DiagHelp,
        Self::DiagGutter,
        Self::DiagPointer,
        Self::SyntaxDirective,
        Self::SyntaxInvalid,
        Self::SyntaxComment,
        Self::SyntaxNumber,
        Self::SyntaxString,
        Self::SyntaxType,
        Self::SyntaxIdentifier,
        Self::SyntaxKeywordCore,
        Self::SyntaxKeywordType,
        Self::SyntaxKeywordEffect,
        Self::SyntaxFamilyActor,
        Self::SyntaxFamilyWorld,
        Self::SyntaxFamilyOwnership,
        Self::SyntaxFamilyProof,
        Self::SyntaxFamilyShader,
        Self::SyntaxOperator,
    ];

    pub fn key(self) -> &'static str {
        match self {
            Self::UiChromeTitle => "ui.chrome.title",
            Self::UiChromeAccent => "ui.chrome.accent",
            Self::UiChromeMuted => "ui.chrome.muted",
            Self::UiBorder => "ui.border.base",
            Self::UiBorderFocus => "ui.border.focus",
            Self::UiPanelBackground => "ui.panel.background.base",
            Self::UiPanelBackgroundActive => "ui.panel.background.active",
            Self::UiTextPrimary => "ui.text.primary",
            Self::UiTextMuted => "ui.text.muted",
            Self::UiTextSubtle => "ui.text.subtle",
            Self::UiStatusForeground => "ui.status.foreground",
            Self::UiStatusBackground => "ui.status.background",
            Self::DiagError => "diag.error",
            Self::DiagWarning => "diag.warning",
            Self::DiagNote => "diag.note",
            Self::DiagHelp => "diag.help",
            Self::DiagGutter => "diag.gutter",
            Self::DiagPointer => "diag.pointer",
            Self::SyntaxDirective => "syntax.directive",
            Self::SyntaxInvalid => "syntax.invalid",
            Self::SyntaxComment => "syntax.comment",
            Self::SyntaxNumber => "syntax.number",
            Self::SyntaxString => "syntax.string",
            Self::SyntaxType => "syntax.type",
            Self::SyntaxIdentifier => "syntax.identifier",
            Self::SyntaxKeywordCore => "syntax.keyword.core",
            Self::SyntaxKeywordType => "syntax.keyword.type",
            Self::SyntaxKeywordEffect => "syntax.keyword.effect",
            Self::SyntaxFamilyActor => "syntax.family.actor",
            Self::SyntaxFamilyWorld => "syntax.family.world",
            Self::SyntaxFamilyOwnership => "syntax.family.ownership",
            Self::SyntaxFamilyProof => "syntax.family.proof",
            Self::SyntaxFamilyShader => "syntax.family.shader",
            Self::SyntaxOperator => "syntax.operator",
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum KeywordFamily {
    Core,
    TypeSystem,
    Effect,
    Actor,
    World,
    Ownership,
    Proof,
    Shader,
}

pub fn classify_catalog_word(word: &str) -> Option<KeywordFamily> {
    match word {
        "fn" | "let" | "mut" | "var" | "const" | "if" | "else" | "elif" | "match" | "for"
        | "while" | "loop" | "break" | "continue" | "return" | "await" | "in" | "with" | "as"
        | "pub" | "mod" | "use" | "self" | "Self" | "true" | "false" | "none" | "and" | "or" => {
            Some(KeywordFamily::Core)
        }
        "type" | "struct" | "enum" | "trait" | "impl" => Some(KeywordFamily::TypeSystem),
        "Pure" | "IO" | "async" | "Async" | "GPU" | "Reactive" | "Unsafe" => {
            Some(KeywordFamily::Effect)
        }
        "actor" | "spawn" | "send" | "receive" | "emit" | "on" => Some(KeywordFamily::Actor),
        "state" | "patch" | "law" | "world" | "entangle" | "shatter" | "teleport" | "pulse"
        | "surface" | "native_ui" | "viewport3d" | "web" | "ue5" | "single_writer" => {
            Some(KeywordFamily::World)
        }
        "collapse" | "observe" | "decay" | "share" | "weak" => Some(KeywordFamily::Ownership),
        "axiom" | "orchestrate" | "converge" | "every" | "when" | "guarantee" | "fallback"
        | "spec" | "fast" | "verify" | "random" | "jitter" | "target" | "capability" | "from"
        | "to" | "via" => Some(KeywordFamily::Proof),
        "component" | "shader" | "comptime" | "macro" | "vertex" | "fragment" | "compute"
        | "uniform" | "render" | "fanout" | "test" => Some(KeywordFamily::Shader),
        _ => None,
    }
}

pub fn semantic_role_for_catalog_word(word: &str) -> Option<SemanticRole> {
    match classify_catalog_word(word) {
        Some(KeywordFamily::Core) => Some(SemanticRole::SyntaxKeywordCore),
        Some(KeywordFamily::TypeSystem) => Some(SemanticRole::SyntaxKeywordType),
        Some(KeywordFamily::Effect) => Some(SemanticRole::SyntaxKeywordEffect),
        Some(KeywordFamily::Actor) => Some(SemanticRole::SyntaxFamilyActor),
        Some(KeywordFamily::World) => Some(SemanticRole::SyntaxFamilyWorld),
        Some(KeywordFamily::Ownership) => Some(SemanticRole::SyntaxFamilyOwnership),
        Some(KeywordFamily::Proof) => Some(SemanticRole::SyntaxFamilyProof),
        Some(KeywordFamily::Shader) => Some(SemanticRole::SyntaxFamilyShader),
        None => None,
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct RgbColor {
    pub r: u8,
    pub g: u8,
    pub b: u8,
}

impl RgbColor {
    pub const fn new(r: u8, g: u8, b: u8) -> Self {
        Self { r, g, b }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Tone {
    rgb: Option<RgbColor>,
}

impl Tone {
    pub const RESET: Self = Self { rgb: None };

    pub const fn rgb(r: u8, g: u8, b: u8) -> Self {
        Self {
            rgb: Some(RgbColor::new(r, g, b)),
        }
    }

    pub fn is_reset(self) -> bool {
        self.rgb.is_none()
    }

    pub fn ratatui_color(self) -> Color {
        self.rgb
            .map(|rgb| Color::Rgb(rgb.r, rgb.g, rgb.b))
            .unwrap_or(Color::Reset)
    }

    pub fn ansi_fg_prefix(self) -> String {
        match self.rgb {
            Some(rgb) => format!("\x1b[38;2;{};{};{}m", rgb.r, rgb.g, rgb.b),
            None => "\x1b[0m".to_string(),
        }
    }

    pub fn ansi_bg_prefix(self) -> String {
        match self.rgb {
            Some(rgb) => format!("\x1b[48;2;{};{};{}m", rgb.r, rgb.g, rgb.b),
            None => "\x1b[0m".to_string(),
        }
    }
}

#[derive(Debug, Clone)]
pub struct LatticeTheme {
    name: String,
    aliases: Vec<String>,
    roles: BTreeMap<SemanticRole, Tone>,
}

impl LatticeTheme {
    pub fn name(&self) -> &str {
        &self.name
    }

    pub fn aliases(&self) -> &[String] {
        &self.aliases
    }

    pub fn tone(&self, role: SemanticRole) -> Tone {
        self.roles.get(&role).copied().unwrap_or(Tone::RESET)
    }

    pub fn ratatui_style(
        &self,
        fg: SemanticRole,
        bg: Option<SemanticRole>,
        modifiers: Modifier,
    ) -> Style {
        let mut style = Style::default().fg(self.tone(fg).ratatui_color());
        if let Some(background_role) = bg {
            style = style.bg(self.tone(background_role).ratatui_color());
        }
        if !modifiers.is_empty() {
            style = style.add_modifier(modifiers);
        }
        style
    }

    pub fn ansi_paint(&self, role: SemanticRole, text: &str, enabled: bool) -> String {
        if !enabled {
            return text.to_string();
        }
        format!("{}{}\x1b[0m", self.tone(role).ansi_fg_prefix(), text)
    }

    pub fn clap_styles(&self) -> Styles {
        let header = nearest_ansi(self.tone(SemanticRole::UiChromeTitle));
        let usage = nearest_ansi(self.tone(SemanticRole::UiChromeAccent));
        let literal = nearest_ansi(self.tone(SemanticRole::SyntaxFamilyProof));
        let placeholder = nearest_ansi(self.tone(SemanticRole::UiTextSubtle));
        let error = nearest_ansi(self.tone(SemanticRole::DiagError));
        let valid = nearest_ansi(self.tone(SemanticRole::SyntaxFamilyWorld));
        let invalid = nearest_ansi(self.tone(SemanticRole::DiagWarning));
        Styles::styled()
            .header(header.on_default().effects(Effects::BOLD))
            .usage(usage.on_default().effects(Effects::BOLD))
            .literal(literal.on_default().effects(Effects::BOLD))
            .placeholder(placeholder.on_default())
            .error(error.on_default().effects(Effects::BOLD))
            .valid(valid.on_default().effects(Effects::BOLD))
            .invalid(invalid.on_default().effects(Effects::BOLD))
    }
}

pub fn supported_theme_names() -> &'static [&'static str] {
    SUPPORTED_THEME_NAMES
}

pub fn normalize_theme_name(raw: &str) -> Result<String, String> {
    let normalized = raw.trim().to_ascii_lowercase();
    if normalized.is_empty() {
        return Ok("slate".to_string());
    }
    if SUPPORTED_THEME_NAMES.contains(&normalized.as_str()) {
        return Ok(normalized);
    }
    for theme_name in SUPPORTED_THEME_NAMES {
        let theme = theme_by_name(theme_name);
        if theme.aliases.iter().any(|alias| alias == &normalized) {
            return Ok((*theme_name).to_string());
        }
    }
    Err(format!(
        "unknown Kain theme `{}`; expected one of {}",
        raw.trim(),
        SUPPORTED_THEME_NAMES.join(", ")
    ))
}

pub fn theme_by_name(name: &str) -> &'static LatticeTheme {
    let normalized = normalize_theme_name(name).unwrap_or_else(|_| "slate".to_string());
    REGISTRY
        .themes
        .get(&normalized)
        .expect("theme registry should contain every supported theme")
}

pub fn theme_banner_accent(theme_name: &str) -> (String, String) {
    let theme = theme_by_name(theme_name);
    (
        theme.tone(SemanticRole::UiChromeAccent).ansi_fg_prefix(),
        theme.tone(SemanticRole::UiChromeTitle).ansi_fg_prefix(),
    )
}

#[derive(Debug, Deserialize)]
struct RegistryFile {
    theme_order: Vec<String>,
    themes: BTreeMap<String, ThemeFile>,
}

#[derive(Debug, Deserialize)]
struct ThemeFile {
    #[serde(default)]
    aliases: Vec<String>,
    roles: BTreeMap<String, Value>,
}

#[derive(Debug)]
struct ThemeRegistry {
    themes: BTreeMap<String, LatticeTheme>,
}

static REGISTRY: Lazy<ThemeRegistry> = Lazy::new(load_registry);

fn load_registry() -> ThemeRegistry {
    let source = include_str!("../lattice.toml");
    let decoded = toml::from_str::<RegistryFile>(source).expect("lattice.toml should parse");
    assert_eq!(
        decoded.theme_order,
        SUPPORTED_THEME_NAMES
            .iter()
            .map(|name| (*name).to_string())
            .collect::<Vec<_>>(),
        "lattice.toml theme order must match supported theme names"
    );

    let mut themes = BTreeMap::new();
    for theme_name in SUPPORTED_THEME_NAMES {
        let file_theme = decoded
            .themes
            .get(*theme_name)
            .unwrap_or_else(|| panic!("lattice.toml missing theme `{theme_name}`"));
        let mut roles = BTreeMap::new();
        let flattened_roles = flatten_role_values(&file_theme.roles, "");
        for role in SemanticRole::ALL {
            let value = flattened_roles
                .get(role.key())
                .unwrap_or_else(|| panic!("theme `{theme_name}` missing role `{}`", role.key()));
            roles.insert(role, parse_tone(value));
        }
        themes.insert(
            (*theme_name).to_string(),
            LatticeTheme {
                name: (*theme_name).to_string(),
                aliases: file_theme
                    .aliases
                    .iter()
                    .map(|alias| alias.to_ascii_lowercase())
                    .collect(),
                roles,
            },
        );
    }
    ThemeRegistry { themes }
}

fn parse_tone(raw: &str) -> Tone {
    if raw.eq_ignore_ascii_case("reset") {
        return Tone::RESET;
    }
    let value = raw.trim().trim_start_matches('#');
    assert_eq!(value.len(), 6, "tone `{raw}` must be #RRGGBB or reset");
    let r = u8::from_str_radix(&value[0..2], 16).expect("valid red hex");
    let g = u8::from_str_radix(&value[2..4], 16).expect("valid green hex");
    let b = u8::from_str_radix(&value[4..6], 16).expect("valid blue hex");
    Tone::rgb(r, g, b)
}

fn flatten_role_values(values: &BTreeMap<String, Value>, prefix: &str) -> BTreeMap<String, String> {
    let mut flattened = BTreeMap::new();
    for (key, value) in values {
        let dotted = if prefix.is_empty() {
            key.clone()
        } else {
            format!("{prefix}.{key}")
        };
        flatten_role_value(value, &dotted, &mut flattened);
    }
    flattened
}

fn flatten_role_value(value: &Value, key: &str, out: &mut BTreeMap<String, String>) {
    match value {
        Value::String(text) => {
            out.insert(key.to_string(), text.clone());
        }
        Value::Table(table) => {
            for (child_key, child_value) in table {
                let dotted = format!("{key}.{child_key}");
                flatten_role_value(child_value, &dotted, out);
            }
        }
        other => panic!("lattice role `{key}` must resolve to a string, found {other:?}"),
    }
}

fn nearest_ansi(tone: Tone) -> AnsiColor {
    let Some(rgb) = tone.rgb else {
        return AnsiColor::White;
    };
    const ANSI: &[(AnsiColor, RgbColor)] = &[
        (AnsiColor::Black, RgbColor::new(0, 0, 0)),
        (AnsiColor::Red, RgbColor::new(205, 49, 49)),
        (AnsiColor::Green, RgbColor::new(13, 188, 121)),
        (AnsiColor::Yellow, RgbColor::new(229, 229, 16)),
        (AnsiColor::Blue, RgbColor::new(36, 114, 200)),
        (AnsiColor::Magenta, RgbColor::new(188, 63, 188)),
        (AnsiColor::Cyan, RgbColor::new(17, 168, 205)),
        (AnsiColor::White, RgbColor::new(229, 229, 229)),
        (AnsiColor::BrightBlack, RgbColor::new(102, 102, 102)),
        (AnsiColor::BrightRed, RgbColor::new(241, 76, 76)),
        (AnsiColor::BrightGreen, RgbColor::new(35, 209, 139)),
        (AnsiColor::BrightYellow, RgbColor::new(245, 245, 67)),
        (AnsiColor::BrightBlue, RgbColor::new(59, 142, 234)),
        (AnsiColor::BrightMagenta, RgbColor::new(214, 112, 214)),
        (AnsiColor::BrightCyan, RgbColor::new(41, 184, 219)),
        (AnsiColor::BrightWhite, RgbColor::new(255, 255, 255)),
    ];

    ANSI.iter()
        .min_by_key(|(_, candidate)| {
            let dr = rgb.r as i32 - candidate.r as i32;
            let dg = rgb.g as i32 - candidate.g as i32;
            let db = rgb.b as i32 - candidate.b as i32;
            dr * dr + dg * dg + db * db
        })
        .map(|(ansi, _)| *ansi)
        .unwrap_or(AnsiColor::White)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn normalizes_legacy_aliases() {
        assert_eq!(normalize_theme_name("lattice").unwrap(), "lattice");
        assert_eq!(normalize_theme_name("hyperpop").unwrap(), "slate");
        assert_eq!(normalize_theme_name("oxide").unwrap(), "graphite");
        assert_eq!(normalize_theme_name("glacier").unwrap(), "arctic");
        assert_eq!(normalize_theme_name("ember").unwrap(), "sandstone");
    }

    #[test]
    fn plain_theme_resets_ui_chrome() {
        let theme = theme_by_name("plain");
        assert!(theme.tone(SemanticRole::UiChromeTitle).is_reset());
        assert!(theme.tone(SemanticRole::UiPanelBackground).is_reset());
    }

    #[test]
    fn catalog_classifies_weird_kain_words() {
        assert_eq!(
            classify_catalog_word("entangle"),
            Some(KeywordFamily::World)
        );
        assert_eq!(
            classify_catalog_word("collapse"),
            Some(KeywordFamily::Ownership)
        );
        assert_eq!(classify_catalog_word("actor"), Some(KeywordFamily::Actor));
        assert_eq!(classify_catalog_word("shader"), Some(KeywordFamily::Shader));
    }
}
