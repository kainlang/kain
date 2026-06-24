#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum LauncherKind {
    Kain,
    Kn,
    Blade,
    Unknown,
}

impl LauncherKind {
    pub fn display_name(self) -> &'static str {
        match self {
            Self::Kn => "kn",
            Self::Blade => "blade",
            Self::Kain | Self::Unknown => "kain",
        }
    }

    pub fn prefers_interpret_default(self) -> bool {
        matches!(self, Self::Kn)
    }
}

pub fn detect_launcher_from_path(path: Option<&std::path::Path>) -> LauncherKind {
    // Allow KAIN_LAUNCHER_KIND env var to override detection.
    // This lets `kain.exe` be invoked as `kn` via argv[0] alias without
    // requiring a separate binary — a single copy/symlink suffices.
    // Also used in tests to simulate the `kn` launcher.
    if let Ok(kind) = std::env::var("KAIN_LAUNCHER_KIND") {
        match kind.to_ascii_lowercase().as_str() {
            "kn" => return LauncherKind::Kn,
            "blade" => return LauncherKind::Blade,
            "kain" => return LauncherKind::Kain,
            _ => {}
        }
    }
    let stem = path
        .and_then(|value| value.file_stem())
        .and_then(|value| value.to_str())
        .map(|value| value.to_ascii_lowercase());
    match stem.as_deref() {
        Some("kn") => LauncherKind::Kn,
        Some("blade") => LauncherKind::Blade,
        Some("kain") => LauncherKind::Kain,
        Some(_) | None => LauncherKind::Unknown,
    }
}

pub fn should_show_launcher_menu(
    launcher: LauncherKind,
    has_command: bool,
    has_input: bool,
) -> bool {
    launcher == LauncherKind::Kn && !has_command && !has_input
}

pub fn resolve_legacy_target_alias(
    launcher: LauncherKind,
    requested_target: &str,
    has_output: bool,
) -> String {
    if launcher.prefers_interpret_default()
        && requested_target.eq_ignore_ascii_case("wasm")
        && !has_output
    {
        "run".to_string()
    } else {
        requested_target.to_string()
    }
}

const KN_SHORTCUTS: &[&str] = &[
    "kn <file.kn>                Run a Kain file immediately",
    "kn -c \"fn main(): ...\"      Run inline Kain code",
    "Get-Content script.kn | kn   Run piped Kain source",
    "kn <file.kn> --watch        Re-run on save for fast authoring",
    "kn native-ui dev <file.kn>  Launch native desktop dev loop with hot reload",
    "kn run <file.kn>            Explicit interpret mode",
    "kn check <file.kn>          Typecheck Kain source without emitting artifacts",
    "kn test <path>              Run Kain test directives and `test` items",
    "kn build <file.kn> -t rust  Generate Rust output",
    "kn fmt <file.kn>            Canonicalize Kain source",
    "kn doctor                   Inspect PATH + runtime wiring",
    "kn doctor --repair <file>    Repair a source file in place or dry-run",
    "kn doctor --repair-tree <dir> Repair every .kn file under a tree",
    "kn doctor --repair <file> --profile aggressive",
];

const KN_PYTHON_INTEROP_HINTS: &[&str] = &[
    "use std::python",
    "use std::js",
    "use std::interop",
    "import numpy as np",
    "import mypyfile",
];

pub fn render_launcher_menu(launcher: LauncherKind) -> Option<String> {
    if launcher != LauncherKind::Kn {
        return None;
    }

    let mut menu = String::from(" kn Quick Start\n");
    menu.push_str(" Run-first authoring is active for this launcher.\n\n");
    for line in KN_SHORTCUTS {
        menu.push_str(" ");
        menu.push_str(line);
        menu.push('\n');
    }
    menu.push('\n');
    menu.push_str(" Python interop is already wired in:\n");
    for hint in KN_PYTHON_INTEROP_HINTS {
        menu.push_str("   - ");
        menu.push_str(hint);
        menu.push('\n');
    }
    menu.push('\n');
    menu.push_str(" Example:\n");
    menu.push_str("   sibling `mypyfile.py` can be imported directly from `main.kn`\n");
    Some(menu)
}
