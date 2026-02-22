use std::fs;
use std::collections::HashMap;
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
    /// Data-driven module directory map: module name -> (public_dir, private_dir)
    ///
    /// Empty in legacy mode (no ue5.modules config).
    pub module_dirs: HashMap<String, (PathBuf, PathBuf)>,
}

/// Detect whether the program has editor-specific items (Slate, Details, Viewport, etc.)
/// Note: Graph editors are detected separately in the pipeline since they're extracted from AST before type checking
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
    has_graph_editors: bool,
) -> KainResult<PluginLayout> {
    // Handle plugin_dir = "." case to avoid nesting (e.g., MultiFileDemo/MultiFileDemo)
    let plugin_root = if config.plugin_dir == PathBuf::from(".") {
        cwd.to_path_buf()
    } else {
        cwd.join(&config.plugin_dir).join(&config.plugin_name)
    };
    let source_dir = plugin_root.join("Source");
    let shaders_dir = plugin_root.join("Shaders");

    // Data-driven module plan mode
    if config.has_module_plan() {
        let mut module_dirs: HashMap<String, (PathBuf, PathBuf)> = HashMap::new();
        for module in &config.modules {
            let base_dir = source_dir.join(&module.name);
            let public_dir = module.output.public.clone().map(|p| {
                if p.is_absolute() { p } else { plugin_root.join(p) }
            }).unwrap_or_else(|| base_dir.join("Public"));
            let private_dir = module.output.private.clone().map(|p| {
                if p.is_absolute() { p } else { plugin_root.join(p) }
            }).unwrap_or_else(|| base_dir.join("Private"));

            fs::create_dir_all(&public_dir).map_err(|e| KainError::Io(e))?;
            fs::create_dir_all(&private_dir).map_err(|e| KainError::Io(e))?;
            module_dirs.insert(module.name.clone(), (public_dir, private_dir));
        }
        fs::create_dir_all(&shaders_dir).map_err(|e| KainError::Io(e))?;

        let has_runtime_items = config.modules.iter().any(|m| {
            matches!(m.module_type, super::config::Ue5ModuleType::Runtime)
        });
        let has_editor_items = config.modules.iter().any(|m| m.module_type.is_editorish());
        let needs_split = has_runtime_items && has_editor_items;

        // Select primary runtime dirs for existing generation flow.
        let runtime_module_name = config.modules.iter()
            .find(|m| matches!(m.module_type, super::config::Ue5ModuleType::Runtime) && m.name == config.plugin_name)
            .or_else(|| config.modules.iter().find(|m| matches!(m.module_type, super::config::Ue5ModuleType::Runtime)))
            .map(|m| m.name.clone())
            .or_else(|| config.modules.first().map(|m| m.name.clone()))
            .ok_or_else(|| KainError::runtime("ue5.modules is configured but empty".to_string()))?;

        let (public_dir, private_dir) = module_dirs
            .get(&runtime_module_name)
            .cloned()
            .ok_or_else(|| KainError::runtime(format!("Primary module '{}' has no resolved directories", runtime_module_name)))?;

        // Select primary editor dirs if any
        let editor_module_name = config.modules.iter()
            .find(|m| m.module_type.is_editorish() && m.name == format!("{}Editor", config.plugin_name))
            .or_else(|| config.modules.iter().find(|m| m.module_type.is_editorish()))
            .map(|m| m.name.clone());

        let (editor_public_dir, editor_private_dir) = if let Some(name) = editor_module_name {
            if let Some((ed_pub, ed_priv)) = module_dirs.get(&name) {
                (Some(ed_pub.clone()), Some(ed_priv.clone()))
            } else {
                (None, None)
            }
        } else {
            (None, None)
        };

        println!("📦 Multi-module layout: {} module(s)", config.modules.len());

        return Ok(PluginLayout {
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
            module_dirs,
        });
    }
    
    // Detect runtime vs editor items EARLY for two-module split decision
    // Graph editors are editor-only items
    let has_editor_items = detect_editor_items(program) || has_graph_editors;
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
        module_dirs: HashMap::new(),
    })
}

/// Detect existing plugin layout without modifying it
pub fn detect_existing(plugin_root: &Path, plugin_name: &str) -> KainResult<PluginLayout> {
    let source_dir = plugin_root.join("Source");
    let shaders_dir = plugin_root.join("Shaders");
    
    if !source_dir.exists() {
        return Err(KainError::runtime(format!(
            "Plugin Source directory not found: {}",
            source_dir.display()
        )));
    }
    
    // Detect if this is a split-module layout
    let runtime_module_dir = source_dir.join(plugin_name);
    let editor_module_dir = source_dir.join(format!("{}Editor", plugin_name));
    
    let needs_split = runtime_module_dir.exists() && editor_module_dir.exists();
    
    let (public_dir, private_dir, editor_public_dir, editor_private_dir) = if needs_split {
        // Split layout
        let rt_pub = runtime_module_dir.join("Public");
        let rt_priv = runtime_module_dir.join("Private");
        let ed_pub = editor_module_dir.join("Public");
        let ed_priv = editor_module_dir.join("Private");
        
        (rt_pub, rt_priv, Some(ed_pub), Some(ed_priv))
    } else {
        // Single module layout
        let pub_dir = source_dir.join("Public");
        let priv_dir = source_dir.join("Private");
        
        if !pub_dir.exists() || !priv_dir.exists() {
            return Err(KainError::runtime(format!(
                "Invalid plugin layout. Expected Public and Private directories in {}",
                source_dir.display()
            )));
        }
        
        (pub_dir, priv_dir, None, None)
    };
    
    // Detect if editor items exist
    let has_editor_items = editor_public_dir.is_some() || 
        public_dir.join("Editor").exists();
    
    // Assume runtime items exist (we're injecting into an existing plugin)
    let has_runtime_items = true;
    
    Ok(PluginLayout {
        plugin_root: plugin_root.to_path_buf(),
        source_dir,
        shaders_dir,
        public_dir,
        private_dir,
        editor_public_dir,
        editor_private_dir,
        needs_split,
        has_editor_items,
        has_runtime_items,
        module_dirs: HashMap::new(),
    })
}
