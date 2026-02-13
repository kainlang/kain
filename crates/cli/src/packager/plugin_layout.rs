use std::fs;
use std::path::{Path, PathBuf};
use crate::error::{KainError, KainResult};
use super::config::Ue5Config;

/// Holds all resolved directory paths and split-mode flags for the plugin build.
pub struct PluginLayout {
    pub plugin_root: PathBuf,
    pub source_dir: PathBuf,
    pub shaders_dir: PathBuf,
    pub public_dir: PathBuf,
    pub private_dir: PathBuf,
    pub editor_public_dir: Option<PathBuf>,
    pub editor_private_dir: Option<PathBuf>,
    pub needs_split: bool,
    pub has_editor_items: bool,
    pub has_runtime_items: bool,
}

/// Detect whether the program has editor-specific items (Slate, Details, Viewport, etc.)
pub fn detect_editor_items(program: &kain_core::types::TypedProgram) -> bool {
    program.items.iter().any(|item| {
        match item {
            kain_core::types::TypedItem::Struct(s) => {
                s.ast.attributes.iter().any(|a| 
                    ue5_editor::is_editor_attribute(&a.name)
                )
            }
            _ => false
        }
    })
}

/// Detect whether the program has runtime items (actors, components, enums, structs, shaders, etc.)
pub fn detect_runtime_items(program: &kain_core::types::TypedProgram, has_shaders: bool) -> bool {
    program.items.iter().any(|item| {
        match item {
            kain_core::types::TypedItem::Actor(_) |
            kain_core::types::TypedItem::Component(_) |
            kain_core::types::TypedItem::Enum(_) |
            kain_core::types::TypedItem::Function(_) |
            kain_core::types::TypedItem::TypeAlias(_) => true,
            kain_core::types::TypedItem::Struct(s) => {
                // Non-editor structs are runtime
                !s.ast.attributes.iter().any(|a| 
                    ue5_editor::is_editor_attribute(&a.name)
                )
            }
            _ => false,
        }
    }) || has_shaders
}

/// Set up the plugin directory structure, handling two-module split when needed.
pub fn setup(
    config: &Ue5Config,
    cwd: &Path,
    program: &kain_core::types::TypedProgram,
    has_shaders: bool,
) -> KainResult<PluginLayout> {
    // Handle plugin_dir = "." case to avoid nesting (e.g., MultiFileDemo/MultiFileDemo)
    let plugin_root = if config.plugin_dir == PathBuf::from(".") {
        cwd.to_path_buf()
    } else {
        cwd.join(&config.plugin_dir).join(&config.plugin_name)
    };
    let source_dir = plugin_root.join("Source");
    let shaders_dir = plugin_root.join("Shaders");
    
    // Detect runtime vs editor items EARLY for two-module split decision
    let has_editor_items = detect_editor_items(program);
    let has_runtime_items = detect_runtime_items(program, has_shaders);
    
    // Two-module split: when BOTH runtime and editor items exist
    let needs_split = has_runtime_items && has_editor_items;
    
    // Directory layout depends on split mode:
    //   Split:  Source/Plugin/Public, Source/PluginEditor/Public
    //   Single: Source/Public (flat, legacy)
    let (public_dir, private_dir, editor_public_dir, editor_private_dir) = if needs_split {
        let rt_dir = source_dir.join(&config.plugin_name);
        let ed_dir = source_dir.join(format!("{}Editor", config.plugin_name));
        let rt_pub = rt_dir.join("Public");
        let rt_priv = rt_dir.join("Private");
        let ed_pub = ed_dir.join("Public");
        let ed_priv = ed_dir.join("Private");
        fs::create_dir_all(&rt_pub).map_err(|e| KainError::Io(e))?;
        fs::create_dir_all(&rt_priv).map_err(|e| KainError::Io(e))?;
        fs::create_dir_all(&ed_pub).map_err(|e| KainError::Io(e))?;
        fs::create_dir_all(&ed_priv).map_err(|e| KainError::Io(e))?;
        println!("📦 Two-module split: {} (Runtime) + {}Editor (Editor)", config.plugin_name, config.plugin_name);
        
        // Clean stale single-module layout files if they exist
        let stale_pub = source_dir.join("Public");
        let stale_priv = source_dir.join("Private");
        let stale_build_cs = source_dir.join(format!("{}.Build.cs", config.plugin_name));
        if stale_pub.exists() {
            let _ = fs::remove_dir_all(&stale_pub);
            println!("   🧹 Removed stale Source/Public (old single-module layout)");
        }
        if stale_priv.exists() {
            let _ = fs::remove_dir_all(&stale_priv);
            println!("   🧹 Removed stale Source/Private (old single-module layout)");
        }
        if stale_build_cs.exists() {
            let _ = fs::remove_file(&stale_build_cs);
            println!("   🧹 Removed stale Source/{}.Build.cs (old single-module layout)", config.plugin_name);
        }
        
        (rt_pub, rt_priv, Some(ed_pub), Some(ed_priv))
    } else {
        let pub_dir = source_dir.join("Public");
        let priv_dir = source_dir.join("Private");
        fs::create_dir_all(&pub_dir).map_err(|e| KainError::Io(e))?;
        fs::create_dir_all(&priv_dir).map_err(|e| KainError::Io(e))?;
        if has_editor_items {
            // Editor-only plugin (no runtime items) — create Editor subdirs
            let ed_pub = pub_dir.join("Editor");
            let ed_priv = priv_dir.join("Editor");
            fs::create_dir_all(&ed_pub).map_err(|e| KainError::Io(e))?;
            fs::create_dir_all(&ed_priv).map_err(|e| KainError::Io(e))?;
        }
        (pub_dir, priv_dir, None, None)
    };
    fs::create_dir_all(&shaders_dir).map_err(|e| KainError::Io(e))?;
    
    Ok(PluginLayout {
        plugin_root,
        source_dir,
        shaders_dir,
        public_dir,
        private_dir,
        editor_public_dir,
        editor_private_dir,
        needs_split,
        has_editor_items,
        has_runtime_items,
    })
}
