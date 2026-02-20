//! Surgical injection mode - non-destructively add KAIN files to existing plugins

use std::collections::{HashMap, HashSet};
use std::fs;
use std::path::{Path, PathBuf};
use crate::error::{KainError, KainResult};
use super::config::Ue5Config;
use super::plugin_layout::PluginLayout;

/// Inject KAIN file(s) into an existing plugin without overwriting
pub fn inject_into_plugin(
    inputs: &[PathBuf],
    plugin_dir: Option<&PathBuf>,
    plugin_name: Option<&str>,
    force: bool,
    dry_run: bool,
) -> KainResult<()> {
    println!("💉 KAIN Surgical Injection Mode");
    println!();
    
    // STEP 1: Detect plugin directory (where .uplugin is)
    let uplugin_dir = detect_plugin_dir(plugin_dir)?;
    println!("📍 Plugin directory: {}", uplugin_dir.display());
    
    // STEP 2: Detect plugin name
    let detected_plugin_name = detect_plugin_name(&uplugin_dir, plugin_name)?;
    println!("📦 Plugin name: {}", detected_plugin_name);
    
    // STEP 3: Find the actual plugin root (where Source/ exists)
    let plugin_root = find_source_root(&uplugin_dir)?;
    println!("📂 Source root: {}", plugin_root.display());
    println!();
    
    // STEP 4: Scan existing files
    let existing_files = scan_existing_files(&plugin_root)?;
    println!("📊 Existing files: {}", existing_files.len());
    
    // STEP 5: Parse and validate input files
    println!();
    println!("🔍 Parsing {} input file(s)...", inputs.len());
    
    let mut all_sources = Vec::new();
    for input in inputs {
        println!("   - {}", input.display());
        let source = fs::read_to_string(input)
            .map_err(|e| KainError::runtime(format!("Failed to read {}: {}", input.display(), e)))?;
        all_sources.push((input.clone(), source));
    }
    
    // STEP 6: Parse all sources into AST
    println!();
    println!("🔨 Parsing and type-checking...");
    
    let mut merged_items = Vec::new();
    for (path, source) in &all_sources {
        let tokens = kain_core::Lexer::new(source).tokenize()
            .map_err(|e| KainError::parse_error(format!("Lexer error in {}: {}", path.display(), e)))?;
        
        let ast = kain_core::Parser::new(&tokens).parse()
            .map_err(|e| KainError::parse_error(format!("Parse error in {}: {}", path.display(), e)))?;
        
        merged_items.extend(ast.items);
    }
    
    // Create merged program
    let merged_ast = kain_core::ast::Program { 
        items: merged_items,
        span: kain_core::Span::default(),
    };
    
    // Type check
    let typed_program = kain_core::types::check(&merged_ast)
        .map_err(|e| KainError::runtime(format!("Type check failed: {}", e)))?;
    
    // Oracle validation
    println!("   ✓ Parsed {} items", typed_program.items.len());
    println!();
    println!("🔍 Running Oracle validation...");
    
    ue5::ue5::oracle::validate_program(&typed_program)
        .map_err(|e| KainError::runtime(format!("Oracle validation failed: {}", e)))?;
    
    println!("   ✓ Validation passed");
    
    // STEP 7: Create minimal Ue5Config for codegen
    let ue5_config = Ue5Config {
        plugin_name: detected_plugin_name.clone(),
        plugin_dir: plugin_root.clone(),
        sources: inputs.to_vec(),
        shaders: Vec::new(),
        copyright: None,
        modular_output: true,  // Always use modular for injection
        stdlib_path: None,     // No stdlib for injection
        engine_version: None,  // Use default (5.2)
    };
    
    // STEP 8: Setup plugin layout (detect existing structure)
    println!();
    println!("📂 Analyzing plugin structure...");
    
    let layout = super::plugin_layout::detect_existing(&plugin_root, &detected_plugin_name)?;
    println!("   ✓ Detected layout: {}", if layout.needs_split { "split (runtime + editor)" } else { "single module" });
    
    // STEP 9: Generate code for new items
    println!();
    println!("⚙️  Generating code...");
    
    let generated_files = generate_injection_files(&layout, &ue5_config, &typed_program)?;
    
    println!("   ✓ Generated {} files", generated_files.len());
    
    // STEP 10: Check for conflicts
    println!();
    println!("🔍 Checking for conflicts...");
    
    let conflicts = check_conflicts(&generated_files, &existing_files, force)?;
    
    if !conflicts.is_empty() {
        println!("   ⚠️  {} file(s) will be overwritten (--force enabled)", conflicts.len());
        for conflict in &conflicts {
            println!("      - {}", conflict);
        }
    } else {
        println!("   ✓ No conflicts detected");
    }
    
    // STEP 11: Write files
    println!();
    if dry_run {
        println!("🔍 DRY RUN - Files that would be generated:");
        for (filename, _) in &generated_files {
            println!("   - {}", filename);
        }
    } else {
        println!("📝 Writing files...");
        write_injection_files(&plugin_root, &generated_files)?;
        
        // Update master header
        let new_includes: Vec<String> = generated_files.keys()
            .filter(|f| f.ends_with(".h"))
            .cloned()
            .collect();
        
        if !new_includes.is_empty() {
            update_master_header(&plugin_root, &detected_plugin_name, &new_includes, dry_run)?;
        }
    }
    
    println!();
    println!("✅ Injection complete!");
    println!("📍 Plugin: {}", plugin_root.display());
    println!("📦 Files: {}", generated_files.len());
    
    Ok(())
}

/// Detect plugin directory by looking for .uplugin file
fn detect_plugin_dir(explicit_dir: Option<&PathBuf>) -> KainResult<PathBuf> {
    if let Some(dir) = explicit_dir {
        if dir.exists() {
            return Ok(dir.clone());
        } else {
            return Err(KainError::runtime(format!(
                "Specified plugin directory does not exist: {}", 
                dir.display()
            )));
        }
    }
    
    // Search current directory and parents for .uplugin file
    let cwd = std::env::current_dir()
        .map_err(|e| KainError::runtime(format!("Failed to get current directory: {}", e)))?;
    
    let mut current = cwd.clone();
    for _ in 0..5 {  // Search up to 5 levels
        // Check if this directory contains a .uplugin file
        if let Ok(entries) = fs::read_dir(&current) {
            for entry in entries.flatten() {
                if let Some(ext) = entry.path().extension() {
                    if ext == "uplugin" {
                        return Ok(current);
                    }
                }
            }
        }
        
        // Move up one level
        if let Some(parent) = current.parent() {
            current = parent.to_path_buf();
        } else {
            break;
        }
    }
    
    Err(KainError::runtime(
        "Could not find plugin directory. No .uplugin file found in current directory or parents.\n\
         Use --plugin-dir to specify explicitly."
    ))
}

/// Find the actual plugin root where Source/ directory exists
/// Handles both flat (plugin_dir/Source/) and nested (plugin_dir/PluginName/Source/) structures
fn find_source_root(uplugin_dir: &Path) -> KainResult<PathBuf> {
    // Try flat structure first: uplugin_dir/Source/
    let flat_source = uplugin_dir.join("Source");
    if flat_source.exists() {
        return Ok(uplugin_dir.to_path_buf());
    }
    
    // Try nested structure: uplugin_dir/PluginName/Source/
    if let Ok(entries) = fs::read_dir(uplugin_dir) {
        for entry in entries.flatten() {
            let path = entry.path();
            if path.is_dir() {
                let nested_source = path.join("Source");
                if nested_source.exists() {
                    return Ok(path);
                }
            }
        }
    }
    
    Err(KainError::runtime(format!(
        "Could not find Source directory. Expected either:\n\
         - {}/Source/\n\
         - {}/PluginName/Source/",
        uplugin_dir.display(),
        uplugin_dir.display()
    )))
}

/// Detect plugin name from .uplugin file or directory name
fn detect_plugin_name(plugin_dir: &Path, explicit_name: Option<&str>) -> KainResult<String> {
    if let Some(name) = explicit_name {
        return Ok(name.to_string());
    }
    
    // Try to find .uplugin file
    if let Ok(entries) = fs::read_dir(plugin_dir) {
        for entry in entries.flatten() {
            if let Some(ext) = entry.path().extension() {
                if ext == "uplugin" {
                    if let Some(stem) = entry.path().file_stem() {
                        if let Some(name) = stem.to_str() {
                            return Ok(name.to_string());
                        }
                    }
                }
            }
        }
    }
    
    // Fallback to directory name
    if let Some(dir_name) = plugin_dir.file_name() {
        if let Some(name) = dir_name.to_str() {
            return Ok(name.to_string());
        }
    }
    
    Err(KainError::runtime(
        "Could not detect plugin name. Use --plugin to specify explicitly."
    ))
}

/// Scan existing files in the plugin to detect conflicts
fn scan_existing_files(plugin_root: &Path) -> KainResult<HashSet<String>> {
    let mut files = HashSet::new();
    
    // Scan Source/ directory (we know it exists at plugin_root/Source/)
    let source_dir = plugin_root.join("Source");
    if source_dir.exists() {
        scan_directory_recursive(&source_dir, &mut files)?;
    }
    
    Ok(files)
}

/// Recursively scan directory and collect filenames
fn scan_directory_recursive(dir: &Path, files: &mut HashSet<String>) -> KainResult<()> {
    if let Ok(entries) = fs::read_dir(dir) {
        for entry in entries.flatten() {
            let path = entry.path();
            if path.is_dir() {
                scan_directory_recursive(&path, files)?;
            } else if let Some(filename) = path.file_name() {
                if let Some(name) = filename.to_str() {
                    files.insert(name.to_string());
                }
            }
        }
    }
    Ok(())
}

/// Check for file conflicts before injection
fn check_conflicts(
    generated_files: &HashMap<String, String>,
    existing_files: &HashSet<String>,
    force: bool,
) -> KainResult<Vec<String>> {
    let mut conflicts = Vec::new();
    
    for filename in generated_files.keys() {
        if existing_files.contains(filename) {
            conflicts.push(filename.clone());
        }
    }
    
    if !conflicts.is_empty() && !force {
        let conflict_list = conflicts.join("\n   - ");
        return Err(KainError::runtime(format!(
            "File conflicts detected:\n   - {}\n\n\
             Use --force to overwrite existing files.",
            conflict_list
        )));
    }
    
    Ok(conflicts)
}

/// Update master header by appending new includes
fn update_master_header(
    plugin_root: &Path,
    plugin_name: &str,
    new_includes: &[String],
    dry_run: bool,
) -> KainResult<()> {
    let source_dir = plugin_root.join("Source");
    let master_header_path = source_dir
        .join("Public")
        .join(format!("{}.h", plugin_name));
    
    if !master_header_path.exists() {
        println!("   ⚠️  Master header not found: {}", master_header_path.display());
        return Ok(());
    }
    
    if dry_run {
        println!("   [DRY RUN] Would update master header: {}", master_header_path.display());
        for include in new_includes {
            println!("      + #include \"{}\"", include);
        }
        return Ok(());
    }
    
    let mut content = fs::read_to_string(&master_header_path)
        .map_err(|e| KainError::Io(e))?;
    
    // Append new includes
    for include in new_includes {
        let include_line = format!("#include \"{}\"\n", include);
        if !content.contains(&include_line) {
            content.push_str(&include_line);
        }
    }
    
    fs::write(&master_header_path, content)
        .map_err(|e| KainError::Io(e))?;
    
    println!("   ✓ Updated master header: {}", master_header_path.display());
    
    Ok(())
}

/// Generate files for injection (per-item modular output)
fn generate_injection_files(
    _layout: &PluginLayout,  // Prefix with _ to suppress unused warning
    config: &Ue5Config,
    program: &kain_core::types::TypedProgram,
) -> KainResult<HashMap<String, String>> {
    let mut files = HashMap::new();
    
    // Build type registry
    let mut type_headers = HashMap::new();
    for item in &program.items {
        let (item_name, output_name) = match item {
            kain_core::types::TypedItem::Actor(a) => (&a.ast.name, ue5::naming::to_actor_name(&a.ast.name)),
            kain_core::types::TypedItem::Component(c) => (&c.ast.name, ue5::naming::to_component_name(&c.ast.name)),
            kain_core::types::TypedItem::Struct(s) => (&s.ast.name, ue5::naming::to_struct_name(&s.ast.name)),
            kain_core::types::TypedItem::Enum(e) => (&e.ast.name, ue5::naming::to_enum_name(&e.ast.name)),
            _ => continue,
        };
        type_headers.insert(item_name.clone(), format!("{}.h", output_name));
    }
    
    // Generate per-item files
    for item in &program.items {
        // Skip editor-only structs (handled separately)
        if let kain_core::types::TypedItem::Struct(s) = item {
            let is_editor_struct = s.ast.attributes.iter().any(|a| 
                ue5_editor::is_editor_attribute(&a.name)
            );
            if is_editor_struct {
                continue;
            }
        }
        
        // Skip delegates (they go in delegate header)
        if let kain_core::types::TypedItem::TypeAlias(alias) = item {
            if matches!(alias.ast.target, kain_core::ast::Type::Function { .. }) {
                continue;
            }
        }
        
        let (item_name, output_name) = match item {
            kain_core::types::TypedItem::Actor(a) => (&a.ast.name, ue5::naming::to_actor_name(&a.ast.name)),
            kain_core::types::TypedItem::Component(c) => (&c.ast.name, ue5::naming::to_component_name(&c.ast.name)),
            kain_core::types::TypedItem::Struct(s) => (&s.ast.name, ue5::naming::to_struct_name(&s.ast.name)),
            kain_core::types::TypedItem::Enum(e) => (&e.ast.name, ue5::naming::to_enum_name(&e.ast.name)),
            _ => continue,
        };
        
        // Generate filtered output for this item
        match ue5::generate_filtered_typed(
            program,
            &config.plugin_name,
            Some(&output_name),
            Some(item_name.clone()),
            config.copyright.as_deref(),
            type_headers.clone(),
            None,
        ) {
            Ok(ue5_output) => {
                // Add header
                files.insert(format!("{}.h", output_name), ue5_output.header);
                
                // Add cpp if it has implementation
                let has_implementation = ue5_output.source.lines()
                    .any(|line| {
                        let trimmed = line.trim();
                        !trimmed.is_empty() && 
                        !trimmed.starts_with("//") && 
                        !trimmed.starts_with("#include")
                    });
                
                if has_implementation {
                    files.insert(format!("{}.cpp", output_name), ue5_output.source);
                }
            }
            Err(e) => {
                eprintln!("   ⚠️  Failed to generate {}: {}", output_name, e);
            }
        }
    }
    
    Ok(files)
}

/// Write generated files to plugin directory
fn write_injection_files(
    plugin_root: &Path,
    files: &HashMap<String, String>,
) -> KainResult<()> {
    let source_dir = plugin_root.join("Source");
    
    for (filename, content) in files {
        // Determine target directory (Public for .h, Private for .cpp)
        let target_dir = if filename.ends_with(".h") {
            source_dir.join("Public")
        } else {
            source_dir.join("Private")
        };
        
        // Ensure directory exists
        fs::create_dir_all(&target_dir)
            .map_err(|e| KainError::Io(e))?;
        
        let target_path = target_dir.join(filename);
        
        fs::write(&target_path, content)
            .map_err(|e| KainError::Io(e))?;
        
        println!("   ✓ {}", target_path.display());
    }
    
    Ok(())
}
