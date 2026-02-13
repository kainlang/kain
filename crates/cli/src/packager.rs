use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::fs;
use std::path::PathBuf;
use flate2::read::GzDecoder;
use tar::Archive;
use crate::error::{KainError, KainResult};
use chrono::Datelike;

const REGISTRY_URL: &str = "https://greeble.co/KAIN/index.json";

#[derive(Debug, Serialize, Deserialize)]
pub struct PackageManifest {
    pub package: PackageInfo,
    #[serde(default)]
    pub build: BuildConfig,
    #[serde(default)]
    pub dependencies: HashMap<String, String>,
    #[serde(default)]
    pub ue5: Option<Ue5Config>,
}

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct Ue5Config {
    pub plugin_name: String,
    #[serde(default = "default_plugin_dir")]
    pub plugin_dir: PathBuf,
    #[serde(default)]
    pub sources: Vec<PathBuf>,  // Multiple .kn files - GODMODE ENABLED
    #[serde(default)]
    pub shaders: Vec<String>,
    #[serde(default)]
    pub copyright: Option<String>,
    #[serde(default)]
    pub modular_output: bool,  // NEW: Generate separate .h/.cpp per source file
    #[serde(default)]
    pub stdlib_path: Option<PathBuf>,  // Optional custom stdlib path
}

fn default_plugin_dir() -> PathBuf { PathBuf::from("Plugins") }

#[derive(Debug, Serialize, Deserialize)]
pub struct BuildConfig {
    #[serde(default = "default_entry")]
    pub entry: PathBuf,
    #[serde(default = "default_output")]
    pub output: PathBuf,
    #[serde(default)]
    pub targets: Vec<String>,
}

fn default_entry() -> PathBuf { PathBuf::from("src/main.kn") }
fn default_output() -> PathBuf { PathBuf::from("dist") }

impl Default for BuildConfig {
    fn default() -> Self {
        Self {
            entry: default_entry(),
            output: default_output(),
            targets: vec!["wasm".to_string()],
        }
    }
}

#[derive(Debug, Serialize, Deserialize)]
pub struct PackageInfo {
    pub name: String,
    pub version: String,
    #[serde(default)]
    pub authors: Vec<String>,
    #[serde(default)]
    pub description: Option<String>,
}

// Registry Structures
#[derive(Debug, Deserialize)]
struct RegistryIndex {
    packages: HashMap<String, String>, // name -> meta.json path
}

#[derive(Debug, Deserialize)]
struct PackageMeta {
    versions: HashMap<String, PackageVersion>,
}

#[derive(Debug, Deserialize)]
struct PackageVersion {
    url: String,
    checksum: String,
}

impl PackageManifest {
    pub fn default(name: &str) -> Self {
        Self {
            package: PackageInfo {
                name: name.to_string(),
                version: "0.1.0".to_string(),
                authors: vec![],
                description: None,
            },
            build: BuildConfig::default(),
            dependencies: HashMap::new(),
            ue5: None,
        }
    }
}

pub fn init_project(path: &PathBuf, name: Option<String>) -> KainResult<()> {
    if !path.exists() {
        fs::create_dir_all(path).map_err(|e| KainError::Io(e))?;
    }

    let name = name.unwrap_or_else(|| {
        path.file_name()
            .and_then(|s| s.to_str())
            .unwrap_or("my_project")
            .to_string()
    });

    // Create KAIN.toml
    let manifest = PackageManifest::default(&name);
    let toml = toml::to_string_pretty(&manifest)
        .map_err(|e| KainError::runtime(format!("Failed to serialize manifest: {}", e)))?;
    
    fs::write(path.join("KAIN.toml"), toml).map_err(|e| KainError::Io(e))?;

    // Create src directory
    let src_dir = path.join("src");
    fs::create_dir_all(&src_dir).map_err(|e| KainError::Io(e))?;

    // Create main.kn
    let main_src = format!(r#"
# {} - Main Entry Point

fn main():
    println("Hello, KAIN World!")
"#, name);
    
    fs::write(src_dir.join("main.kn"), main_src.trim()).map_err(|e| KainError::Io(e))?;

    // Create .gitignore
    fs::write(path.join(".gitignore"), "target/\ndeps/\n").map_err(|e| KainError::Io(e))?;

    println!(" Initialized new KAIN project: {}", name);
    Ok(())
}

pub fn load_manifest(path: &PathBuf) -> KainResult<PackageManifest> {
    let manifest_path = if path.ends_with("KAIN.toml") {
        path.clone()
    } else {
        path.join("KAIN.toml")
    };

    if !manifest_path.exists() {
        return Err(KainError::runtime(format!("Manifest not found at {}", manifest_path.display())));
    }

    let content = fs::read_to_string(&manifest_path).map_err(|e| KainError::Io(e))?;
    let manifest: PackageManifest = toml::from_str(&content)
        .map_err(|e| KainError::runtime(format!("Failed to parse KAIN.toml: {}", e)))?;

    Ok(manifest)
}

pub fn add_dependency(package_name: &str, version: Option<String>) -> KainResult<()> {
    println!(" Fetching registry index...");
    let index: RegistryIndex = reqwest::blocking::get(REGISTRY_URL)
        .map_err(|e| KainError::runtime(format!("Failed to fetch registry: {}", e)))?
        .json()
        .map_err(|e| KainError::runtime(format!("Failed to parse registry index: {}", e)))?;

    let meta_path = index.packages.get(package_name)
        .ok_or_else(|| KainError::runtime(format!("Package '{}' not found in registry.", package_name)))?;

    let meta_url = format!("https://greeble.co/KAIN/{}", meta_path);
    println!(" Fetching metadata for {}...", package_name);
    
    let meta: PackageMeta = reqwest::blocking::get(&meta_url)
        .map_err(|e| KainError::runtime(format!("Failed to fetch package metadata: {}", e)))?
        .json()
        .map_err(|e| KainError::runtime(format!("Failed to parse package metadata: {}", e)))?;

    // Determine version
    let version_to_install = version.unwrap_or_else(|| {
        // Pick latest (naive)
        meta.versions.keys().max().unwrap().clone()
    });

    let pkg_ver = meta.versions.get(&version_to_install)
        .ok_or_else(|| KainError::runtime(format!("Version {} not found for package {}", version_to_install, package_name)))?;

    println!(" Resolving {} v{}...", package_name, version_to_install);

    // Update KAIN.toml
    let cwd = std::env::current_dir().unwrap_or_else(|_| PathBuf::from("."));
    let mut manifest = load_manifest(&cwd)?;
    
    manifest.dependencies.insert(package_name.to_string(), version_to_install.clone());
    
    let toml = toml::to_string_pretty(&manifest)
        .map_err(|e| KainError::runtime(format!("Failed to serialize manifest: {}", e)))?;
    
    fs::write(cwd.join("KAIN.toml"), toml).map_err(|e| KainError::Io(e))?;

    println!(" Added {} v{} to KAIN.toml", package_name, version_to_install);

    // Install it
    install_package(package_name, &version_to_install, &pkg_ver.url)
}

pub fn install_all() -> KainResult<()> {
    let cwd = std::env::current_dir().unwrap_or_else(|_| PathBuf::from("."));
    let manifest = load_manifest(&cwd)?;

    if manifest.dependencies.is_empty() {
        println!(" No dependencies to install.");
        return Ok(());
    }

    println!(" Fetching registry index...");
    let index: RegistryIndex = reqwest::blocking::get(REGISTRY_URL)
        .map_err(|e| KainError::runtime(format!("Failed to fetch registry: {}", e)))?
        .json()
        .map_err(|e| KainError::runtime(format!("Failed to parse registry index: {}", e)))?;

    for (name, version) in manifest.dependencies {
        // Resolve URL (duplicate logic for now, proper solver later)
        if let Some(meta_path) = index.packages.get(&name) {
             let meta_url = format!("https://greeble.co/KAIN/{}", meta_path);
             let meta: PackageMeta = reqwest::blocking::get(&meta_url)
                 .map_err(|_| KainError::runtime(format!("Failed to fetch meta for {}", name)))?
                 .json()
                 .map_err(|_| KainError::runtime(format!("Failed to parse meta for {}", name)))?;
             
             if let Some(v) = meta.versions.get(&version) {
                 install_package(&name, &version, &v.url)?;
             } else {
                 eprintln!(" Version {} not found for {}", version, name);
             }
        } else {
            eprintln!(" Package {} not found in registry", name);
        }
    }
    
    Ok(())
}

fn install_package(name: &str, version: &str, url: &str) -> KainResult<()> {
    let deps_dir = PathBuf::from("deps");
    if !deps_dir.exists() {
        fs::create_dir_all(&deps_dir).map_err(|e| KainError::Io(e))?;
    }

    let target_dir = deps_dir.join(name);
    if target_dir.exists() {
        println!(" {} v{} is already installed.", name, version);
        return Ok(());
    }

    println!(" Downloading {} from {}...", name, url);
    let response = reqwest::blocking::get(url)
        .map_err(|e| KainError::runtime(format!("Download failed: {}", e)))?;
    
    let content = response.bytes()
        .map_err(|e| KainError::runtime(format!("Failed to read bytes: {}", e)))?;

    println!(" Installing {}...", name);
    
    let tar = GzDecoder::new(std::io::Cursor::new(&content));
    let mut archive = Archive::new(tar);
    
    // Unpack to target directory
    archive.unpack(&target_dir).map_err(|e| KainError::Io(e))?;

    // Verify lib.kn exists (optional safety check)
    if !target_dir.join("lib.kn").exists() {
        // If the package was packed with a root folder (e.g. package-1.0.0/), we might need to handle stripping
        println!(" Warning: installed package {} might be nested.", name);
    }

    println!(" Installed {} v{}", name, version);
    Ok(())
}

/// Build all targets specified in KAIN.toml
pub fn build_project(target_overrides: Option<Vec<String>>) -> KainResult<()> {
    let cwd = std::env::current_dir().unwrap_or_else(|_| PathBuf::from("."));
    let manifest = load_manifest(&cwd)?;
    
    // Use overrides or manifest targets
    let targets = target_overrides.unwrap_or_else(|| manifest.build.targets.clone());
    
    if targets.is_empty() {
        println!(" No targets specified in KAIN.toml [build.targets]");
        println!(" Defaulting to wasm");
        return build_targets(&manifest, &cwd, &["wasm".to_string()]);
    }
    
    build_targets(&manifest, &cwd, &targets)
}

fn build_targets(manifest: &PackageManifest, cwd: &PathBuf, targets: &[String]) -> KainResult<()> {
    use crate::{compile, CompileTarget};
    
    // Ensure output directory exists
    let output_dir = cwd.join(&manifest.build.output);
    fs::create_dir_all(&output_dir).map_err(|e| KainError::Io(e))?;
    
    // Read source file
    let entry_path = cwd.join(&manifest.build.entry);
    if !entry_path.exists() {
        return Err(KainError::runtime(format!(
            "Entry file not found: {}", entry_path.display()
        )));
    }
    
    let source = fs::read_to_string(&entry_path).map_err(|e| KainError::Io(e))?;
    let file_stem = entry_path.file_stem()
        .and_then(|s| s.to_str())
        .unwrap_or("output");
    
    println!(" Building {} v{}", manifest.package.name, manifest.package.version);
    println!(" Entry: {}", manifest.build.entry.display());
    println!(" Output: {}/", manifest.build.output.display());
    println!();
    
    for target_str in targets {
        let target = parse_target(target_str)?;
        let ext = target_extension(target);
        let out_path = output_dir.join(file_stem).with_extension(ext);
        
        match compile(&source, target) {
            Ok(output) => {
                fs::write(&out_path, &output).map_err(|e| KainError::Io(e))?;
                println!(" [{}] -> {} ({} bytes)", target_str, out_path.display(), output.len());
            }
            Err(e) => {
                eprintln!(" [{}] FAILED: {}", target_str, e);
            }
        }
    }
    
    println!();
    println!(" Build complete!");
    Ok(())
}

fn parse_target(s: &str) -> KainResult<kain_core::CompileTarget> {
    use kain_core::CompileTarget;
    match s.to_lowercase().as_str() {
        "wasm" | "w" => Ok(CompileTarget::Wasm),
        "llvm" | "native" | "n" => Ok(CompileTarget::Llvm),
        "spirv" | "gpu" | "s" => Ok(CompileTarget::Spirv),
        "hlsl" | "h" => Ok(CompileTarget::Hlsl),
        "usf" => Ok(CompileTarget::Usf),
        "js" | "javascript" => Ok(CompileTarget::Js),
        "rust" | "rs" => Ok(CompileTarget::Rust),
        "hybrid" => Ok(CompileTarget::Hybrid),
        "cpp" | "c++" => Ok(CompileTarget::Cpp),
        "ue5" | "unreal" => Ok(CompileTarget::Ue5),
        _ => Err(KainError::runtime(format!("Unknown target: {}", s)))
    }
}

fn target_extension(target: kain_core::CompileTarget) -> &'static str {
    use kain_core::CompileTarget;
    match target {
        CompileTarget::Wasm => "wasm",
        CompileTarget::Llvm => "ll",
        CompileTarget::Spirv => "spv",
        CompileTarget::Hlsl => "hlsl",
        CompileTarget::Usf => "usf",
        CompileTarget::Js => "js",
        CompileTarget::Rust => "rs",
        CompileTarget::Hybrid => "js",
        CompileTarget::Cpp => "cpp",
        CompileTarget::Ue5 => "h",
        CompileTarget::Ue5Editor => "h",
        CompileTarget::Interpret | CompileTarget::Test => "txt",
    }
}



/// Build UE5 plugin from KAIN.toml configuration
pub fn build_ue5_plugin() -> KainResult<()> {
    let cwd = std::env::current_dir().unwrap_or_else(|_| PathBuf::from("."));
    let manifest = load_manifest(&cwd)?;
    
    let ue5_config = manifest.ue5.as_ref()
        .ok_or_else(|| KainError::runtime("No [ue5] section in KAIN.toml"))?;
    
    println!("🚀 Building UE5 Plugin: {}", ue5_config.plugin_name);
    println!(" Plugin directory: {}", ue5_config.plugin_dir.display());
    println!();
    
    // STEP 1: Load stdlib files FIRST (they contain type definitions)
    let mut all_source_files = Vec::new();
    let mut stdlib_files = Vec::new();
    
    // Determine stdlib path from config or use defaults
    // STDLIB IS NOW DISABLED BY DEFAULT - only load if explicitly configured
    let stdlib_search_paths: Vec<PathBuf> = if let Some(custom_path) = &ue5_config.stdlib_path {
        // Use custom path from KAIN.toml (only if user explicitly wants stdlib)
        vec![custom_path.clone()]
    } else {
        // STDLIB DISABLED BY DEFAULT - return empty vec
        // User must explicitly set stdlib_path in KAIN.toml to enable
        vec![]
    };
    
    // Try each search path
    for stdlib_path in stdlib_search_paths {
        if stdlib_path.exists() {
            if let Ok(entries) = fs::read_dir(&stdlib_path) {
                for entry in entries {
                    if let Ok(entry) = entry {
                        let path = entry.path();
                        if path.extension().map_or(false, |e| e == "kn") {
                            // Skip README files
                            if let Some(name) = path.file_name() {
                                if name.to_string_lossy().to_uppercase().contains("README") {
                                    continue;
                                }
                            }
                            stdlib_files.push(path);
                        }
                    }
                }
            }
            // Found stdlib, stop searching
            if !stdlib_files.is_empty() {
                println!("📚 Loaded stdlib from: {}", stdlib_path.display());
                break;
            }
        }
    }
    
    // Sort stdlib files for consistent ordering
    stdlib_files.sort();
    
    // Add stdlib files first
    for stdlib_file in &stdlib_files {
        all_source_files.push(stdlib_file.clone());
    }
    
    // STEP 2: Add user source files
    let user_source_files = if ue5_config.sources.is_empty() {
        // Fallback to single entry file
        vec![manifest.build.entry.clone()]
    } else {
        // Use multiple source files - GODMODE
        ue5_config.sources.clone()
    };
    
    // Add user files (as PathBuf, resolved relative to cwd)
    for user_file in &user_source_files {
        all_source_files.push(cwd.join(user_file));
    }
    
    println!("📁 Source files: {} (stdlib: {}, user: {})", 
        all_source_files.len(), 
        stdlib_files.len(), 
        user_source_files.len()
    );
    println!("   📚 Stdlib files:");
    for (i, file) in stdlib_files.iter().enumerate() {
        if let Some(name) = file.file_name() {
            println!("      {}. {}", i + 1, name.to_string_lossy());
        }
    }
    println!("   📝 User files:");
    for (i, file) in user_source_files.iter().enumerate() {
        println!("      {}. {}", i + 1, file.display());
    }
    println!();
    
    // Parse and validate EACH source file independently (LLM-optimized pipeline)
    let mut all_asts = Vec::new();
    let mut all_shader_names = Vec::new();
    
    println!("🔍 Validating source files...");
    for source_path in &all_source_files {
        if !source_path.exists() {
            return Err(KainError::runtime(format!(
                "Source file not found: {}", source_path.display()
            )));
        }
        
        let file_source = fs::read_to_string(&source_path).map_err(|e| KainError::Io(e))?;
        
        // Parse this file independently - catch errors early with clear file context
        let tokens = match kain_core::Lexer::new(&file_source).tokenize() {
            Ok(t) => t,
            Err(e) => {
                return Err(KainError::runtime(format!(
                    "❌ Syntax error in {}: {}", source_path.display(), e
                )));
            }
        };
        
        let ast = match kain_core::Parser::new(&tokens).parse() {
            Ok(a) => a,
            Err(e) => {
                // Convert span to line:column for better error messages
                let error_msg = format_error_with_location(&file_source, &e.to_string(), source_path.display().to_string());
                return Err(KainError::runtime(format!(
                    "❌ Parse error in {}:\n{}", source_path.display(), error_msg
                )));
            }
        };
        
        // Extract shaders from this file's AST
        for item in &ast.items {
            if let kain_core::ast::Item::Shader(shader) = item {
                all_shader_names.push(shader.name.clone());
            }
        }
        
        all_asts.push((source_path.display().to_string(), ast));
        
        // Only print validation for user files (not stdlib)
        if !stdlib_files.contains(source_path) {
            println!("   ✓ {} validated", source_path.file_name().unwrap().to_string_lossy());
        }
    }
    println!();
    
    // Merge ASTs into single program (proper AST-level merging, not string concat)
    let mut merged_ast = kain_core::ast::Program {
        items: Vec::new(),
        span: kain_core::span::Span::new(0, 0), // Merged program has synthetic span
    };
    
    for (_file_name, ast) in &all_asts {
        // Merge items from each file's AST
        for item in &ast.items {
            merged_ast.items.push(item.clone());
        }
    }
    
    // Type check the merged AST
    println!("🔍 Type checking merged program...");
    let typed_program = match kain_core::types::check(&merged_ast) {
        Ok(p) => p,
        Err(e) => {
            return Err(KainError::runtime(format!(
                "❌ Type error: {}", e
            )));
        }
    };
    println!("   ✓ Type checking passed");
    println!();
    
    // ORACLE VALIDATION: Validate against Unreal Engine semantic rules
    println!("🔮 Running Unreal Semantic Validator (Oracle)...");
    match ue5::validate_program(&typed_program) {
        Ok(_) => {
            println!("   ✓ Oracle validation passed");
        }
        Err(e) => {
            return Err(e);
        }
    }
    println!();
    
    // Setup plugin directory structure
    // Handle plugin_dir = "." case to avoid nesting (e.g., MultiFileDemo/MultiFileDemo)
    let plugin_root = if ue5_config.plugin_dir == PathBuf::from(".") {
        // If plugin_dir is ".", we're already in the plugin directory
        cwd.clone()
    } else {
        // Otherwise, join plugin_dir and plugin_name
        cwd.join(&ue5_config.plugin_dir).join(&ue5_config.plugin_name)
    };
    let source_dir = plugin_root.join("Source");
    let public_dir = source_dir.join("Public");
    let private_dir = source_dir.join("Private");
    let shaders_dir = plugin_root.join("Shaders");
    
    fs::create_dir_all(&public_dir).map_err(|e| KainError::Io(e))?;
    fs::create_dir_all(&private_dir).map_err(|e| KainError::Io(e))?;
    fs::create_dir_all(&shaders_dir).map_err(|e| KainError::Io(e))?;
    
    // Use shader names from config or auto-detected
    let shader_names = if ue5_config.shaders.is_empty() {
        all_shader_names
    } else {
        ue5_config.shaders.clone()
    };
    
    // Shaders are OPTIONAL - only compile if present
    if !shader_names.is_empty() {
        eprintln!("⚡ [PACKAGER] Found {} shaders:", shader_names.len());
        for name in &shader_names {
            eprintln!("   - {}", name);
        }
        println!("⚡ Found {} shaders:", shader_names.len());
        for name in &shader_names {
            println!("   - {}", name);
        }
        println!();
        
        // Compile each shader using the merged typed program
        for shader_name in &shader_names {
            eprintln!("🔨 [PACKAGER] Compiling shader: {}", shader_name);
            println!("🔨 Compiling shader: {}", shader_name);
            
            // Generate USF shader file (single shader only) from merged program
            match ue5_shaders::generate_single_usf_from_program(&typed_program, shader_name) {
                Ok(usf_code) => {
                    let usf_path = shaders_dir.join(format!("{}.usf", shader_name));
                    fs::write(&usf_path, usf_code).map_err(|e| KainError::Io(e))?;
                    println!("   ✓ {}.usf", shader_name);
                }
                Err(e) => {
                    eprintln!("   ✗ Failed to generate USF: {}", e);
                    continue;
                }
            }
            
            // Generate C++ header from merged program
            let header_code = ue5_shaders::generate_cpp_header(&typed_program, shader_name);
            let header_path = public_dir.join(format!("{}.h", shader_name));
            fs::write(&header_path, header_code).map_err(|e| KainError::Io(e))?;
            println!("   ✓ {}.h", shader_name);
            
            // Generate C++ implementation from merged program
            let cpp_code = ue5_shaders::generate_cpp_implementation(&typed_program, shader_name, &ue5_config.plugin_name);
            let cpp_path = private_dir.join(format!("{}.cpp", shader_name));
            fs::write(&cpp_path, cpp_code).map_err(|e| KainError::Io(e))?;
            println!("   ✓ {}.cpp", shader_name);
        }
    } else {
        println!("ℹ️  No shaders detected - skipping shader compilation");
        println!();
    }
    
    // Generate main plugin files (actors, structs, enums, etc.) from merged typed program
    println!();
    
    if ue5_config.modular_output {
        // MODULAR MODE: Generate separate .h/.cpp for each source file
        println!("🎯 Generating modular plugin files (per-file output)...");
        
        // STEP 1: Generate master header FIRST (forward declarations only)
        println!("   📦 Generating master header with forward declarations...");
        let mut master_header = String::new();
        master_header.push_str(&format!("// Copyright {} {}. All Rights Reserved.\n", 
            chrono::Utc::now().year(),
            ue5_config.copyright.as_deref().unwrap_or("Epic Games, Inc.")));
        master_header.push_str("// Generated by KAIN-PRO - Godmode v3 (Modular Output)\n");
        master_header.push_str("// Master header - forward declarations and includes\n\n");
        master_header.push_str("#pragma once\n\n");
        master_header.push_str("#include \"CoreMinimal.h\"\n\n");
        
        // Add forward declarations for all types
        master_header.push_str("// Forward declarations\n");
        for item in &typed_program.items {
            match item {
                kain_core::types::TypedItem::Struct(s) => {
                    let struct_name = ue5::naming::to_struct_name(&s.ast.name);
                    master_header.push_str(&format!("struct {};\n", struct_name));
                }
                kain_core::types::TypedItem::Enum(e) => {
                    let enum_name = ue5::naming::to_enum_name(&e.ast.name);
                    master_header.push_str(&format!("enum class {} : uint8;\n", enum_name));
                }
                kain_core::types::TypedItem::Actor(a) => {
                    let actor_name = ue5::naming::to_actor_name(&a.ast.name);
                    master_header.push_str(&format!("class {};\n", actor_name));
                }
                kain_core::types::TypedItem::Component(c) => {
                    let comp_name = ue5::naming::to_component_name(&c.ast.name);
                    master_header.push_str(&format!("class {};\n", comp_name));
                }
                _ => {}
            }
        }
        
        // STEP 1.5: Generate separate delegate header file (ARCHITECTURAL IMPROVEMENT!)
        // This solves the circular dependency problem:
        // - Delegates need type definitions (enums, structs)
        // - Slate widgets need delegate definitions
        // - Master header includes both
        // Solution: Separate delegate header that includes types first, then declares delegates
        
        let mut delegate_header_content = String::new();
        let mut delegate_count = 0;
        let mut delegate_type_dependencies: std::collections::HashSet<String> = std::collections::HashSet::new();
        
        // Collect all delegate declarations and their type dependencies
        for item in &typed_program.items {
            if let kain_core::types::TypedItem::TypeAlias(alias) = item {
                // Check if this is a delegate (function type)
                if let kain_core::ast::Type::Function { params, .. } = &alias.ast.target {
                    let delegate_name = format!("F{}", alias.ast.name);
                    
                    // Helper function to map KAIN types to UE5 types and track dependencies
                    let mut map_type = |ty: &kain_core::ast::Type| -> String {
                        match ty {
                            kain_core::ast::Type::Named { name, .. } => {
                                // Map KAIN types to UE5 types
                                match name.as_str() {
                                    "Int" => "int32".to_string(),
                                    "Float" => "float".to_string(),
                                    "Bool" => "bool".to_string(),
                                    "String" => "FString".to_string(),
                                    "Vec2" => "FVector2D".to_string(),
                                    "Vec3" => "FVector".to_string(),
                                    "Vec4" => "FVector4".to_string(),
                                    // Check if it's an enum (starts with capital letter)
                                    _ if name.chars().next().unwrap().is_uppercase() => {
                                        // Check if it's an enum in the program
                                        let is_enum = typed_program.items.iter().any(|item| {
                                            if let kain_core::types::TypedItem::Enum(e) = item {
                                                e.ast.name == *name
                                            } else {
                                                false
                                            }
                                        });
                                        if is_enum {
                                            // Use naming module to avoid double-prefixing
                                            // e.g. "EHealthStatus" stays "EHealthStatus", not "EEHealthStatus"
                                            let ue_name = ue5::ue5::naming::to_enum_name(name);
                                            delegate_type_dependencies.insert(format!("{}.h", ue_name));
                                            ue_name
                                        } else {
                                            // Use naming module to avoid double-prefixing
                                            let ue_name = ue5::ue5::naming::to_struct_name(name);
                                            delegate_type_dependencies.insert(format!("{}.h", ue_name));
                                            ue_name
                                        }
                                    }
                                    _ => name.clone(),
                                }
                            }
                            _ => "void".to_string(),
                        }
                    };
                    
                    // Generate delegate declaration based on parameter count
                    let delegate_decl = if params.is_empty() {
                        format!("DECLARE_DYNAMIC_MULTICAST_DELEGATE({});", delegate_name)
                    } else if params.len() == 1 {
                        let param_type = map_type(&params[0]);
                        format!("DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam({}, {}, Param0);", 
                            delegate_name, param_type)
                    } else if params.len() == 2 {
                        let param1_type = map_type(&params[0]);
                        let param2_type = map_type(&params[1]);
                        format!("DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams({}, {}, Param0, {}, Param1);", 
                            delegate_name, param1_type, param2_type)
                    } else if params.len() == 3 {
                        let param1_type = map_type(&params[0]);
                        let param2_type = map_type(&params[1]);
                        let param3_type = map_type(&params[2]);
                        format!("DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams({}, {}, Param0, {}, Param1, {}, Param2);", 
                            delegate_name, param1_type, param2_type, param3_type)
                    } else if params.len() <= 9 {
                        // UE5 supports up to 9 parameters
                        let param_types: Vec<String> = params.iter().map(|p| map_type(p)).collect();
                        let param_list: Vec<String> = param_types.iter().enumerate()
                            .map(|(i, ty)| format!("{}, Param{}", ty, i))
                            .collect();
                        let macro_name = match params.len() {
                            4 => "DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams",
                            5 => "DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams",
                            6 => "DECLARE_DYNAMIC_MULTICAST_DELEGATE_SixParams",
                            7 => "DECLARE_DYNAMIC_MULTICAST_DELEGATE_SevenParams",
                            8 => "DECLARE_DYNAMIC_MULTICAST_DELEGATE_EightParams",
                            9 => "DECLARE_DYNAMIC_MULTICAST_DELEGATE_NineParams",
                            _ => unreachable!(),
                        };
                        format!("{}({}, {});", macro_name, delegate_name, param_list.join(", "))
                    } else {
                        format!("// ERROR: Delegate {} has {} parameters - UE5 supports up to 9", 
                            delegate_name, params.len())
                    };
                    
                    delegate_header_content.push_str(&delegate_decl);
                    delegate_header_content.push('\n');
                    delegate_header_content.push('\n');
                    delegate_count += 1;
                }
            }
        }
        
        // Generate complete delegate header file if we have any delegates
        if delegate_count > 0 {
            let mut full_delegate_header = String::new();
            full_delegate_header.push_str(&format!("// Copyright {} {}. All Rights Reserved.\n", 
                chrono::Utc::now().year(),
                ue5_config.copyright.as_deref().unwrap_or("Epic Games, Inc.")));
            full_delegate_header.push_str("// Generated by KAIN-PRO - Delegate Declarations\n");
            full_delegate_header.push_str("// This file contains ONLY delegate declarations to avoid circular dependencies\n\n");
            full_delegate_header.push_str("#pragma once\n\n");
            full_delegate_header.push_str("#include \"CoreMinimal.h\"\n");
            
            // Include type dependencies (enums, structs that delegates reference)
            let mut sorted_deps: Vec<String> = delegate_type_dependencies.into_iter().collect();
            sorted_deps.sort();
            for dep in &sorted_deps {
                full_delegate_header.push_str(&format!("#include \"{}\"\n", dep));
            }
            full_delegate_header.push_str("\n");
            
            // CRITICAL: .generated.h MUST come before GENERATED_BODY() and delegate macros
            // UHT processes this file and generates the .generated.h content that GENERATED_BODY() expands into
            full_delegate_header.push_str(&format!("#include \"{}Delegates.generated.h\"\n", ue5_config.plugin_name));
            full_delegate_header.push_str("\n");
            
            // Generate complete delegate header with ACTUAL declarations
            // This allows Slate widgets to include delegate definitions without circular dependencies
            full_delegate_header.push_str("// Delegate declarations\n");
            full_delegate_header.push_str(&delegate_header_content);
            
            // Dummy USTRUCT to force UHT to process this header and generate .generated.h
            // This is necessary because UHT only processes headers that contain reflection macros
            full_delegate_header.push_str("// Internal struct for UHT processing\n");
            full_delegate_header.push_str("USTRUCT()\n");
            full_delegate_header.push_str(&format!("struct F{}Delegates_Internal\n", ue5_config.plugin_name));
            full_delegate_header.push_str("{\n");
            full_delegate_header.push_str("    GENERATED_BODY()\n");
            full_delegate_header.push_str("};\n");
            
            // Write delegate header file
            let delegate_header_path = public_dir.join(format!("{}Delegates.h", ue5_config.plugin_name));
            fs::write(&delegate_header_path, full_delegate_header).map_err(|e| KainError::Io(e))?;
            println!("      ✓ {}Delegates.h ({} delegate declarations - ARCHITECTURAL IMPROVEMENT!)", ue5_config.plugin_name, delegate_count);
        }
        
        // STEP 1.6: Generate EditorTypes header (OPTION 3 - ARCHITECTURAL COMPLETION!)
        // This header provides ALL types that editor code (Slate, Details, Viewport) needs
        // without circular dependencies. This is the FINAL SOLUTION to Slate type issues.
        let mut editor_types_header = String::new();
        editor_types_header.push_str(&format!("// Copyright {} {}. All Rights Reserved.\n", 
            chrono::Utc::now().year(),
            ue5_config.copyright.as_deref().unwrap_or("Epic Games, Inc.")));
        editor_types_header.push_str("// Generated by KAIN-PRO - Editor Types Header\n");
        editor_types_header.push_str("// This file provides ALL runtime types + delegates for editor code\n");
        editor_types_header.push_str("// Slate widgets, Details customizations, and Viewports should include this\n\n");
        editor_types_header.push_str("#pragma once\n\n");
        editor_types_header.push_str("#include \"CoreMinimal.h\"\n\n");
        
        // Include all runtime type headers (enums, structs, actors, components)
        editor_types_header.push_str("// Runtime types (enums, structs, actors, components)\n");
        for item in &typed_program.items {
            // Skip editor-only items
            if let kain_core::types::TypedItem::Struct(s) = item {
                let is_editor_struct = s.ast.attributes.iter().any(|a| 
                    ue5_editor::is_editor_attribute(&a.name)
                );
                if is_editor_struct {
                    continue;
                }
            }
            
            // Skip delegates (they're in the delegate header)
            if let kain_core::types::TypedItem::TypeAlias(alias) = item {
                if matches!(alias.ast.target, kain_core::ast::Type::Function { .. }) {
                    continue;
                }
            }
            
            let header_name = match item {
                kain_core::types::TypedItem::Actor(a) => Some(format!("{}.h", ue5::naming::to_actor_name(&a.ast.name))),
                kain_core::types::TypedItem::Component(c) => Some(format!("{}.h", ue5::naming::to_component_name(&c.ast.name))),
                kain_core::types::TypedItem::Struct(s) => Some(format!("{}.h", ue5::naming::to_struct_name(&s.ast.name))),
                kain_core::types::TypedItem::Enum(e) => Some(format!("{}.h", ue5::naming::to_enum_name(&e.ast.name))),
                _ => None,
            };
            
            if let Some(header) = header_name {
                editor_types_header.push_str(&format!("#include \"{}\"\n", header));
            }
        }
        editor_types_header.push_str("\n");
        
        // Include delegates if we have any
        if delegate_count > 0 {
            editor_types_header.push_str("// Delegates\n");
            editor_types_header.push_str(&format!("#include \"{}Delegates.h\"\n", ue5_config.plugin_name));
            editor_types_header.push_str("\n");
        }
        
        // Forward declare all Slate widgets to prevent circular dependencies
        editor_types_header.push_str("// Forward declarations for Slate widgets (prevents circular dependencies)\n");
        for item in &typed_program.items {
            if let kain_core::types::TypedItem::Struct(s) = item {
                if s.ast.attributes.iter().any(|a| a.name == "slate") {
                    let widget_name = format!("S{}", s.ast.name);
                    editor_types_header.push_str(&format!("class {};\n", widget_name));
                }
            }
        }
        editor_types_header.push_str("\n");
        
        // Write EditorTypes header file
        let editor_types_path = public_dir.join(format!("{}EditorTypes.h", ue5_config.plugin_name));
        fs::write(&editor_types_path, editor_types_header).map_err(|e| KainError::Io(e))?;
        println!("      ✓ {}EditorTypes.h (complete type definitions for editor code - OPTION 3!)", ue5_config.plugin_name);
        
        master_header.push_str("\n// Module includes\n");
        
        // Include delegate header FIRST if we have delegates (solves circular dependency!)
        if delegate_count > 0 {
            master_header.push_str(&format!("#include \"{}Delegates.h\"\n", ue5_config.plugin_name));
        }
        
        let master_header_path = public_dir.join(format!("{}.h", ue5_config.plugin_name));
        fs::write(&master_header_path, &master_header).map_err(|e| KainError::Io(e))?;
        println!("      ✓ {}.h (master header with forward decls)", ue5_config.plugin_name);
        
        // STEP 1: Build Global Type Registry (SICK MAGIC)
        let mut type_headers = std::collections::HashMap::new();
        for item in &typed_program.items {
            let (item_name, output_name) = match item {
                kain_core::types::TypedItem::Actor(a) => (&a.ast.name, ue5::naming::to_actor_name(&a.ast.name)),
                kain_core::types::TypedItem::Component(c) => (&c.ast.name, ue5::naming::to_component_name(&c.ast.name)),
                kain_core::types::TypedItem::Struct(s) => (&s.ast.name, ue5::naming::to_struct_name(&s.ast.name)),
                kain_core::types::TypedItem::Enum(e) => (&e.ast.name, ue5::naming::to_enum_name(&e.ast.name)),
                kain_core::types::TypedItem::TypeAlias(a) => {
                    // Delegates go in master header, not separate files
                    (&a.ast.name, format!("{}", ue5_config.plugin_name))
                },
                _ => continue,
            };
            type_headers.insert(item_name.clone(), format!("{}.h", output_name));
        }
        
        // STEP 2: Generate each item into its own dedicated file (Phase 2: World Domination)
        for item in &typed_program.items {
            // Skip editor-only structs (handled by ue5-editor crate)
            if let kain_core::types::TypedItem::Struct(s) = item {
                let is_editor_struct = s.ast.attributes.iter().any(|a| 
                    ue5_editor::is_editor_attribute(&a.name)
                );
                if is_editor_struct {
                    continue; // Skip - will be generated by editor codegen
                }
            }
            
            // Skip delegates - they're already in master header
            if let kain_core::types::TypedItem::TypeAlias(alias) = item {
                if matches!(alias.ast.target, kain_core::ast::Type::Function { .. }) {
                    continue; // Skip delegates
                }
            }
            
            let (item_name, output_name) = match item {
                kain_core::types::TypedItem::Actor(a) => (&a.ast.name, ue5::naming::to_actor_name(&a.ast.name)),
                kain_core::types::TypedItem::Component(c) => (&c.ast.name, ue5::naming::to_component_name(&c.ast.name)),
                kain_core::types::TypedItem::Struct(s) => (&s.ast.name, ue5::naming::to_struct_name(&s.ast.name)),
                kain_core::types::TypedItem::Enum(e) => (&e.ast.name, ue5::naming::to_enum_name(&e.ast.name)),
                kain_core::types::TypedItem::TypeAlias(a) => (&a.ast.name, format!("F{}", a.ast.name)),
                _ => continue,
            };

            println!("   📄 Slicing item: {} → {}.h/cpp", item_name, output_name);

            // Generate filtered output for this specific item using the FULL program shared state and type map
            match ue5::generate_filtered(&typed_program, &ue5_config.plugin_name, Some(&output_name), Some(item_name.clone()), ue5_config.copyright.as_deref(), type_headers.clone(), Some(shader_names.clone())) {
                Ok(ue5_output) => {
                    // Write header
                    let header_path = public_dir.join(format!("{}.h", output_name));
                    fs::write(&header_path, &ue5_output.header).map_err(|e| KainError::Io(e))?;
                    println!("      ✓ {}.h", output_name);
                    
                    // Only write .cpp if it has meaningful content (not just includes)
                    let has_implementation = ue5_output.source.lines()
                        .any(|line| {
                            let trimmed = line.trim();
                            !trimmed.is_empty() && 
                            !trimmed.starts_with("//") && 
                            !trimmed.starts_with("#include")
                        });
                    
                    if has_implementation {
                        let cpp_path = private_dir.join(format!("{}.cpp", output_name));
                        fs::write(&cpp_path, &ue5_output.source).map_err(|e| KainError::Io(e))?;
                        println!("      ✓ {}.cpp", output_name);
                    } else {
                        println!("      ⊘ {}.cpp (skipped - no implementation needed)", output_name);
                    }
                    
                    // Append this item's include to master header
                    let mut master = fs::read_to_string(&master_header_path).map_err(|e| KainError::Io(e))?;
                    master.push_str(&format!("#include \"{}.h\"\n", output_name));
                    fs::write(&master_header_path, master).map_err(|e| KainError::Io(e))?;
                }
                Err(e) => {
                    eprintln!("      ✗ Failed to generate {}: {}", output_name, e);
                }
            }
        }
        
        // STEP 3: Generate KainStdlib.h with all stdlib utility functions (ONLY if there are any)
        println!("   📦 Generating stdlib functions header...");
        match ue5::generate_stdlib_functions(&typed_program, &ue5_config.plugin_name, ue5_config.copyright.as_deref(), type_headers.clone()) {
            Ok(stdlib_output) => {
                // Check if there are any functions in the output (look for "static inline" which indicates functions)
                if stdlib_output.header.contains("static inline") {
                    let stdlib_header_path = public_dir.join("KainStdlib.h");
                    fs::write(&stdlib_header_path, &stdlib_output.header).map_err(|e| KainError::Io(e))?;
                    println!("      ✓ KainStdlib.h (stdlib utility functions)");
                    
                    // Add include to master header
                    let mut master = fs::read_to_string(&master_header_path).map_err(|e| KainError::Io(e))?;
                    // Insert KainStdlib include BEFORE the individual module includes
                    master = master.replace(
                        "// Module includes\n",
                        "// Stdlib functions\n#include \"KainStdlib.h\"\n\n// Module includes\n"
                    );
                    fs::write(&master_header_path, master).map_err(|e| KainError::Io(e))?;
                } else {
                    println!("      ℹ️  No stdlib functions to generate (skipped)");
                }
            }
            Err(e) => {
                eprintln!("      ✗ Failed to generate stdlib functions: {}", e);
            }
        }
        
        // STEP 4: Generate blueprint function library if any @blueprint functions exist
        let has_blueprint_funcs = typed_program.items.iter().any(|item| {
            if let kain_core::types::TypedItem::Function(f) = item {
                f.ast.attributes.iter().any(|a| a.name == "blueprint" || a.name == "ue5")
            } else {
                false
            }
        });
        
        if has_blueprint_funcs {
            println!("   📦 Generating blueprint function library...");
            // Generate blueprint functions with special target to skip type definitions
            // Use plugin-specific name to avoid collisions between multiple KAIN plugins
            let bp_lib_name = format!("{}BlueprintLibrary", ue5_config.plugin_name);
            match ue5::generate_filtered(&typed_program, &ue5_config.plugin_name, Some(&bp_lib_name), Some("__BLUEPRINT_LIBRARY_ONLY__".to_string()), ue5_config.copyright.as_deref(), type_headers.clone(), None) {
                Ok(bp_output) => {
                    let bp_header_path = public_dir.join(format!("{}.h", bp_lib_name));
                    fs::write(&bp_header_path, &bp_output.header).map_err(|e| KainError::Io(e))?;
                    println!("      ✓ {}.h", bp_lib_name);
                    
                    let bp_cpp_path = private_dir.join(format!("{}.cpp", bp_lib_name));
                    fs::write(&bp_cpp_path, &bp_output.source).map_err(|e| KainError::Io(e))?;
                    println!("      ✓ {}.cpp", bp_lib_name);
                    
                    // Add include to master header
                    let mut master = fs::read_to_string(&master_header_path).map_err(|e| KainError::Io(e))?;
                    master.push_str(&format!("#include \"{}.h\"\n", bp_lib_name));
                    fs::write(&master_header_path, master).map_err(|e| KainError::Io(e))?;
                }
                Err(e) => {
                    eprintln!("      ✗ Failed to generate blueprint library: {}", e);
                }
            }
        }
        
        // STEP 5: Generate editor tools if any editor attributes exist
        let has_editor_items = typed_program.items.iter().any(|item| {
            match item {
                kain_core::types::TypedItem::Struct(s) => {
                    s.ast.attributes.iter().any(|a| 
                        ue5_editor::is_editor_attribute(&a.name)
                    )
                }
                _ => false
            }
        });
        
        if has_editor_items {
            println!("   🎨 Generating editor tools (Slate UI, Details, Viewport, Toolbar...)...");
            
            // Create Editor subdirectory
            let editor_public_dir = public_dir.join("Editor");
            let editor_private_dir = private_dir.join("Editor");
            fs::create_dir_all(&editor_public_dir).map_err(|e| KainError::Io(e))?;
            fs::create_dir_all(&editor_private_dir).map_err(|e| KainError::Io(e))?;
            
            // Generate per-item editor files (modular output)
            match ue5_editor::generate_per_item(&typed_program, &ue5_config.plugin_name, ue5_config.copyright.as_deref()) {
                Ok(editor_items) => {
                    for editor_item in &editor_items {
                        println!("   📄 Editor item: {} [{}] → {}.h/cpp", editor_item.name, editor_item.kind, editor_item.name);
                        
                        // Write header
                        let header_path = editor_public_dir.join(format!("{}.h", editor_item.name));
                        fs::write(&header_path, &editor_item.header).map_err(|e| KainError::Io(e))?;
                        println!("      ✓ {}.h", editor_item.name);
                        
                        // Only write .cpp if it has meaningful content (not just includes)
                        let has_implementation = editor_item.source.lines()
                            .any(|line| {
                                let trimmed = line.trim();
                                !trimmed.is_empty() && 
                                !trimmed.starts_with("//") && 
                                !trimmed.starts_with("#include") &&
                                !trimmed.starts_with("BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION") &&
                                !trimmed.starts_with("END_SLATE_FUNCTION_BUILD_OPTIMIZATION") &&
                                trimmed != "{"  &&
                                trimmed != "}" &&
                                trimmed != "];" &&
                                trimmed != "[" &&
                                !trimmed.starts_with("ChildSlot") &&
                                !trimmed.starts_with("SNew(S") ||
                                (trimmed.starts_with("SNew(S") && trimmed.contains("+"))  // Has actual slots
                            });
                        
                        if has_implementation {
                            let cpp_path = editor_private_dir.join(format!("{}.cpp", editor_item.name));
                            fs::write(&cpp_path, &editor_item.source).map_err(|e| KainError::Io(e))?;
                            println!("      ✓ {}.cpp", editor_item.name);
                        } else {
                            println!("      ⊘ {}.cpp (skipped - no implementation needed)", editor_item.name);
                        }
                        
                        // Add include to master header
                        let mut master = fs::read_to_string(&master_header_path).map_err(|e| KainError::Io(e))?;
                        master.push_str(&format!("#include \"Editor/{}.h\"\n", editor_item.name));
                        fs::write(&master_header_path, master).map_err(|e| KainError::Io(e))?;
                    }
                    println!("   ✅ {} editor items generated", editor_items.len());
                }
                Err(e) => {
                    eprintln!("      ✗ Failed to generate editor tools: {}", e);
                }
            }
        } else {
            println!("   ℹ️  No editor items detected - skipping editor codegen");
        }
        
        println!();
        println!("   ✅ Master header finalized with all module includes");
        
        // NOTE: Do NOT add .generated.h to the master header.
        // The master header is a forward-declaration + include aggregation file, NOT a UHT-processed type.
        // Individual type headers (EHealthStatus.h, ADiagnosticPreviewActor.h, etc.) already have their
        // own .generated.h includes where needed (alongside UCLASS/USTRUCT/UENUM macros).
        
        // Detect if an @editor_module exists in the program.
        // If so, it already provides IMPLEMENT_MODULE — we must NOT generate a duplicate.
        let has_editor_module = typed_program.items.iter().any(|item| {
            if let kain_core::types::TypedItem::Struct(s) = item {
                s.ast.attributes.iter().any(|a| a.name == "editor_module")
            } else {
                false
            }
        });
        
        if has_editor_module {
            eprintln!("📦 [PACKAGER] @editor_module detected — skipping default module registration");
            eprintln!("   (Editor module provides its own IMPLEMENT_MODULE)");
            println!("   ℹ️  @editor_module provides IMPLEMENT_MODULE — skipping default {}.cpp", ue5_config.plugin_name);
        } else {
            eprintln!("📦 [PACKAGER] Generating module registration file...");
            // Generate minimal module registration .cpp (only when no @editor_module exists)
            let module_cpp = format!(r#"// Generated by KAIN-PRO - Module Registration
#include "{}.h"
#include "Modules/ModuleManager.h"

class F{}Module : public IModuleInterface
{{
public:
    virtual void StartupModule() override
    {{
        // Module startup
    }}

    virtual void ShutdownModule() override
    {{
        // Module shutdown
    }}
}};

IMPLEMENT_MODULE(F{}Module, {})
"#, ue5_config.plugin_name, ue5_config.plugin_name, ue5_config.plugin_name, ue5_config.plugin_name);
            
            let module_cpp_path = private_dir.join(format!("{}.cpp", ue5_config.plugin_name));
            fs::write(&module_cpp_path, module_cpp).map_err(|e| KainError::Io(e))?;
            eprintln!("   ✓ [PACKAGER] {}.cpp (IMPLEMENT_MODULE)", ue5_config.plugin_name);
            println!("      ✓ {}.cpp (module registration)", ue5_config.plugin_name);
        }
        
    } else {
        // MONOLITHIC MODE: Generate single .h/.cpp with all types merged
        println!("🎯 Generating main plugin files from merged program...");
        
        match ue5::generate(&typed_program, Some(&ue5_config.plugin_name), ue5_config.copyright.as_deref()) {
            Ok(ue5_output) => {
                // Write header
                let main_header_path = public_dir.join(format!("{}.h", ue5_config.plugin_name));
                fs::write(&main_header_path, &ue5_output.header).map_err(|e| KainError::Io(e))?;
                println!("   ✓ {}.h (actors, structs, enums, components)", ue5_config.plugin_name);
                
                // Write source
                let main_cpp_path = private_dir.join(format!("{}.cpp", ue5_config.plugin_name));
                fs::write(&main_cpp_path, &ue5_output.source).map_err(|e| KainError::Io(e))?;
                println!("   ✓ {}.cpp (implementations + module registration)", ue5_config.plugin_name);
            }
            Err(e) => {
                eprintln!("   ✗ Failed to generate UE5 code: {}", e);
            }
        }
    }
    
    // Check if we have editor items (needed for both .uplugin and .Build.cs)
    let has_editor_items = typed_program.items.iter().any(|item| {
        match item {
            kain_core::types::TypedItem::Struct(s) => {
                s.ast.attributes.iter().any(|a| 
                    ue5_editor::is_editor_attribute(&a.name)
                )
            }
            _ => false
        }
    });
    
    // Generate .uplugin file (ALWAYS regenerate to ensure it's up-to-date)
    let uplugin_path = plugin_root.join(format!("{}.uplugin", ue5_config.plugin_name));
    println!();
    println!("📦 Generating .uplugin file...");
    let uplugin_content = generate_uplugin_file(&ue5_config.plugin_name, &manifest.package.description, has_editor_items);
    fs::write(&uplugin_path, uplugin_content).map_err(|e| KainError::Io(e))?;
    println!("   ✓ {}.uplugin", ue5_config.plugin_name);
    
    // Generate .Build.cs file (ALWAYS regenerate to ensure dependencies are up-to-date)
    let build_cs_path = source_dir.join(format!("{}.Build.cs", ue5_config.plugin_name));
    println!();
    println!("🔨 Generating .Build.cs file...");
    
    let build_cs_content = generate_build_cs(&ue5_config.plugin_name, has_editor_items);
    fs::write(&build_cs_path, build_cs_content).map_err(|e| KainError::Io(e))?;
    println!("   ✓ {}.Build.cs", ue5_config.plugin_name);
    
    // PYTHON POST-PROCESSING - Auto-fix edge cases
    println!();
    println!("🐍 Running Python post-processor...");
    run_python_post_processor(&plugin_root, &ue5_config.plugin_name)?;
    
    println!();
    println!("✅ Plugin build complete!");
    println!("� Location: {}", plugin_root.display());
    println!();
    if ue5_config.modular_output {
        println!("💡 Modular compilation: {} user files + {} stdlib files → {} C++ modules", 
            user_source_files.len(), 
            stdlib_files.len(),
            user_source_files.len()); 
    } else {
        println!("💡 Multi-file compilation: {} user files + {} stdlib files combined", 
            user_source_files.len(),
            stdlib_files.len());
    }
    println!("⚡ Total shaders: {}", shader_names.len());
    Ok(())
}

/// Convert span position to line:column for better error messages
fn format_error_with_location(source: &str, error_msg: &str, file_name: String) -> String {
    // Extract span from error message if present
    if let Some(start_idx) = error_msg.find("Span { start: ") {
        if let Some(end_idx) = error_msg[start_idx..].find(" }") {
            let span_str = &error_msg[start_idx..start_idx + end_idx + 2];
            
            // Parse span
            if let Some(start_pos) = span_str.split("start: ").nth(1) {
                if let Some(start_num_str) = start_pos.split(',').next() {
                    if let Ok(start_pos) = start_num_str.parse::<usize>() {
                        // Convert position to line:column
                        let (line, col) = position_to_line_col(source, start_pos);
                        
                        // Extract the line content
                        let line_content = get_line_content(source, line);
                        
                        // Format nice error message
                        return format!(
                            "\n   {}:{}:{}\n   |\n{} | {}\n   | {}^\n   |\n   {}",
                            file_name,
                            line,
                            col,
                            line,
                            line_content,
                            " ".repeat(col.saturating_sub(1)),
                            error_msg.split(": ").last().unwrap_or(error_msg)
                        );
                    }
                }
            }
        }
    }
    
    // Fallback to original error message
    error_msg.to_string()
}

/// Convert byte position to line:column (1-indexed)
fn position_to_line_col(source: &str, pos: usize) -> (usize, usize) {
    let mut line = 1;
    let mut col = 1;
    
    for (i, ch) in source.chars().enumerate() {
        if i >= pos {
            break;
        }
        if ch == '\n' {
            line += 1;
            col = 1;
        } else {
            col += 1;
        }
    }
    
    (line, col)
}

/// Get the content of a specific line (1-indexed)
fn get_line_content(source: &str, line_num: usize) -> String {
    source
        .lines()
        .nth(line_num.saturating_sub(1))
        .unwrap_or("")
        .to_string()
}

/// Extract shader names from KAIN source code
fn extract_shader_names(source: &str) -> KainResult<Vec<String>> {
    match kain_core::Lexer::new(source).tokenize() {
        Ok(tokens) => {
            match kain_core::Parser::new(&tokens).parse() {
                Ok(ast) => {
                    let names: Vec<String> = ast.items.iter()
                        .filter_map(|item| {
                            if let kain_core::ast::Item::Shader(shader) = item {
                                Some(shader.name.clone())
                            } else {
                                None
                            }
                        })
                        .collect();
                    Ok(names)
                }
                Err(e) => Err(KainError::runtime(format!("Failed to parse source: {}", e)))
            }
        }
        Err(e) => Err(KainError::runtime(format!("Failed to tokenize source: {}", e)))
    }
}

/// Generate a .uplugin file with correct module type and loading phase
fn generate_uplugin_file(plugin_name: &str, description: &Option<String>, has_editor_items: bool) -> String {
    let desc = description.as_ref()
        .map(|s| s.as_str())
        .unwrap_or("Generated by KAIN-PRO");
    
    let (module_type, loading_phase) = if has_editor_items {
        ("Editor", "PostEngineInit")
    } else {
        ("Runtime", "PostConfigInit")
    };
    
    format!(r#"{{
  "FileVersion": 3,
  "Version": 1,
  "VersionName": "1.0.0",
  "FriendlyName": "{}",
  "Description": "{}",
  "Category": "KAIN-PRO",
  "CreatedBy": "KAIN-PRO Compiler",
  "CreatedByURL": "",
  "DocsURL": "",
  "MarketplaceURL": "",
  "SupportURL": "",
  "CanContainContent": false,
  "IsBetaVersion": false,
  "IsExperimentalVersion": false,
  "Installed": false,
  "Modules": [
    {{
      "Name": "{}",
      "Type": "{}",
      "LoadingPhase": "{}"
    }}
  ]
}}
"#, plugin_name, desc, plugin_name, module_type, loading_phase)
}

/// Generate a .Build.cs file for the UE5 plugin module
fn generate_build_cs(plugin_name: &str, has_editor_items: bool) -> String {
    let editor_deps = if has_editor_items {
        r#"
		// Editor dependencies (module Type is Editor, so these are always available)
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				"UnrealEd",
				"AssetTools",
				"EditorStyle",
				"PropertyEditor",
				"InputCore"
			}
		);"#
    } else {
        ""
    };
    
    let editor_include_paths = if has_editor_items {
        r#"
		PublicIncludePaths.AddRange(
			new string[] {
				System.IO.Path.Combine(ModuleDirectory, "Public/Editor")
			}
		);
				
		PrivateIncludePaths.AddRange(
			new string[] {
				System.IO.Path.Combine(ModuleDirectory, "Private/Editor")
			}
		);"#
    } else {
        r#"
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
		);
				
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
		);"#
    };
    
    format!(r#"// Copyright Epic Games, Inc. All Rights Reserved.
// Generated by KAIN Compiler

using UnrealBuildTool;
using System.IO;

public class {0} : ModuleRules
{{
	public {0}(ReadOnlyTargetRules Target) : base(Target)
	{{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		{2}
		PublicDependencyModuleNames.AddRange(
			new string[]
			{{
				"Core",
				"CoreUObject",
				"Engine",
				"RenderCore",
				"RHI",
				"Renderer",
				"Projects"
			}}
		);
			
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{{
				// ... add private dependencies that you statically link with here ...	
			}}
		);
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{{
				// ... add any modules that your module loads dynamically here ...
			}}
		);{1}
	}}
}}
"#, plugin_name, editor_deps, editor_include_paths)
}


/// Run Python post-processor to auto-fix edge cases
fn run_python_post_processor(plugin_path: &PathBuf, plugin_name: &str) -> KainResult<()> {
    use std::process::Command;
    
    // Find Python script - try multiple locations
    let mut script_path = None;
    
    // 1. Try relative to cwd
    let cwd_script = std::env::current_dir()
        .unwrap_or_else(|_| PathBuf::from("."))
        .join("kain")
        .join("python")
        .join("post_process.py");
    
    if cwd_script.exists() {
        script_path = Some(cwd_script);
    } else {
        // 2. Try walking up directories to find kain/python/post_process.py
        let mut current = std::env::current_dir().unwrap_or_else(|_| PathBuf::from("."));
        for _ in 0..5 {  // Try up to 5 levels up
            let candidate = current.join("kain").join("python").join("post_process.py");
            if candidate.exists() {
                script_path = Some(candidate);
                break;
            }
            if let Some(parent) = current.parent() {
                current = parent.to_path_buf();
            } else {
                break;
            }
        }
        
        // 3. Try relative to executable
        if script_path.is_none() {
            if let Ok(exe_path) = std::env::current_exe() {
                if let Some(exe_dir) = exe_path.parent() {
                    let exe_script = exe_dir.join("python").join("post_process.py");
                    if exe_script.exists() {
                        script_path = Some(exe_script);
                    }
                }
            }
        }
    }
    
    let script_path = match script_path {
        Some(p) => p,
        None => {
            println!("   ⚠️  Python post-processor not found (skipping)");
            return Ok(());
        }
    };
    
    // Run Python script
    let output = Command::new("python")
        .arg(&script_path)
        .arg(plugin_path)
        .arg(plugin_name)
        .arg("--verbose")
        .output();
    
    match output {
        Ok(output) => {
            if output.status.success() {
                // Parse JSON output
                let stdout = String::from_utf8_lossy(&output.stdout);
                
                // Find JSON in output (last line)
                if let Some(json_line) = stdout.lines().last() {
                    if let Ok(result) = serde_json::from_str::<serde_json::Value>(json_line) {
                        if let Some(fixes) = result.get("fixes_applied").and_then(|v| v.as_u64()) {
                            println!("   ✅ Applied {} auto-fixes", fixes);
                            
                            // Show fixes if verbose
                            if let Some(fixes_list) = result.get("fixes").and_then(|v| v.as_array()) {
                                for fix in fixes_list {
                                    if let Some(fix_str) = fix.as_str() {
                                        println!("      - {}", fix_str);
                                    }
                                }
                            }
                        }
                    }
                }
            } else {
                let stderr = String::from_utf8_lossy(&output.stderr);
                println!("   ⚠️  Python post-processor failed:");
                println!("{}", stderr);
            }
        }
        Err(e) => {
            println!("   ⚠️  Could not run Python post-processor: {}", e);
            println!("      (Make sure Python is installed and in PATH)");
        }
    }
    
    Ok(())
}
