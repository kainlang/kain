// KAIN Compiler CLI

use clap::{CommandFactory, FromArgMatches, Parser as ClapParser};
use cli::fabric;
use cli::import_asm;
use cli::import_c;
use cli::import_crate;
use cli::import_rust;
use cli::import_typescript;
use cli::llvm_native_stage;
use cli::lsp;
use cli::native_ui_build;
use cli::omni;
use cli::packager;
use cli::repair::{self, DoctorRepairArgs};
use cli::rust_build;
use cli::selfhost;
use cli::{
    compile, detect_launcher_from_path, parse_compile_target, render_launcher_menu,
    resolve_legacy_target_alias, should_show_launcher_menu, supported_targets_csv,
    target_extension, CompileTarget, LauncherKind, BUILD_GIT_COMMIT_COUNT, BUILD_GIT_DIRTY,
    BUILD_GIT_SHA, BUILD_HOST_TRIPLE, BUILD_NUMBER, BUILD_PROFILE, BUILD_TARGET_TRIPLE,
    BUILD_UNIX_TIME, LANGUAGE_NAME, VERSION,
};
use kain_crate_ffi::{ArtifactMode, ImportCrateOptions};
use serde::Deserialize;
use std::collections::BTreeSet;
use std::fs;
use std::io::{self, BufRead, IsTerminal, Read, Write};
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::time::Duration;

#[derive(Debug, Default, Deserialize)]
struct NativeRuntimeManifest {
    #[serde(default)]
    name: Option<String>,
    #[serde(default)]
    sources: Vec<PathBuf>,
    #[serde(default)]
    windows_sources: Vec<PathBuf>,
    #[serde(default)]
    linux_sources: Vec<PathBuf>,
    #[serde(default)]
    macos_sources: Vec<PathBuf>,
    #[serde(default)]
    include_dirs: Vec<PathBuf>,
    #[serde(default)]
    defines: Vec<String>,
    #[serde(default)]
    windows_defines: Vec<String>,
    #[serde(default)]
    linux_defines: Vec<String>,
    #[serde(default)]
    macos_defines: Vec<String>,
    #[serde(default)]
    link: NativeRuntimeLinkManifest,
}

#[derive(Debug, Default, Deserialize)]
struct NativeRuntimeLinkManifest {
    #[serde(default)]
    windows: Vec<String>,
    #[serde(default)]
    linux: Vec<String>,
    #[serde(default)]
    macos: Vec<String>,
}

#[derive(Debug)]
struct ResolvedNativeRuntimeBundle {
    name: String,
    sources: Vec<PathBuf>,
    include_dirs: Vec<PathBuf>,
    defines: Vec<String>,
    link_libs: Vec<String>,
    uses_cpp_runtime: bool,
}

#[derive(ClapParser, Debug)]
#[command(name = "kain")]
#[command(author = "Kipp")]
#[command(version = VERSION)]
#[command(about = "The Ultimate Programming Language Compiler", long_about = None)]
struct Args {
    #[command(subcommand)]
    command: Option<Commands>,

    /// Source file to compile (legacy positional argument)
    input: Option<PathBuf>,

    /// Inline Kain source, similar to `python -c`
    #[arg(short = 'c', long, conflicts_with = "input")]
    code: Option<String>,

    /// Output file
    #[arg(short, long)]
    output: Option<PathBuf>,

    /// Compilation target
    #[arg(short, long, default_value = "wasm")]
    target: String,

    /// Run immediately after compilation
    #[arg(short, long)]
    run: bool,

    /// Watch for file changes and recompile
    #[arg(short, long)]
    watch: bool,

    /// Emit AST for debugging  
    #[arg(long)]
    emit_ast: bool,

    /// Emit typed AST
    #[arg(long)]
    emit_typed: bool,

    /// Verbose output
    #[arg(short, long)]
    verbose: bool,

    /// Target plugin name for UE5 shader copy
    #[arg(long)]
    plugin: Option<String>,

    /// Base plugins directory (defaults to u:\ue_factory\src-plugins)
    #[arg(long)]
    plugins_dir: Option<PathBuf>,

    /// Print planned actions without executing
    #[arg(long)]
    dry_run: bool,

    /// Treat transpiler warnings as errors when supported
    #[arg(long)]
    strict: bool,

    /// Analyze shader complexity (USF target only)
    #[arg(long)]
    analyze: bool,
}

#[derive(clap::Subcommand, Debug)]
enum BuildCommand {
    /// Build a standalone native UI app and optionally a desktop executable
    #[command(name = "native-ui")]
    NativeUi {
        /// Input Kain UI source file
        input: PathBuf,

        /// Root component override
        #[arg(long = "root")]
        root_component: Option<String>,

        /// Native app name / Cargo package name
        #[arg(long)]
        app_name: Option<String>,

        /// Window title for the native host
        #[arg(long)]
        window_title: Option<String>,

        /// Materialized project output directory
        #[arg(short = 'o', long = "out")]
        project_dir: Option<PathBuf>,

        /// Relative or absolute artifact directory inside the materialized project
        #[arg(long)]
        artifact_dir: Option<PathBuf>,

        /// Generate the native app project but skip cargo build
        #[arg(long)]
        bundle_only: bool,

        /// Build the generated app in release mode
        #[arg(long)]
        release: bool,

        /// Override the native runtime crate name
        #[arg(long, default_value = "kain-ui-native")]
        runtime_crate: String,

        /// Use an explicit path dependency for the native runtime crate
        #[arg(long, conflicts_with = "runtime_version")]
        runtime_path: Option<PathBuf>,

        /// Use a published version dependency for the native runtime crate
        #[arg(long, conflicts_with = "runtime_path")]
        runtime_version: Option<String>,
    },
}

#[derive(clap::Subcommand, Debug)]
enum Commands {
    /// Initialize a new KAIN project
    Init {
        /// Project name
        #[arg(default_value = ".")]
        path: PathBuf,

        /// Explicit project name
        #[arg(long)]
        name: Option<String>,
    },

    /// Start the Language Server
    Lsp,

    /// Show binary/build diagnostics and resolved compiler capabilities
    Doctor {
        #[command(flatten)]
        repair: DoctorRepairArgs,
    },

    /// Run self-host bootstrap workflows
    Selfhost {
        #[command(subcommand)]
        command: selfhost::SelfHostCommand,
    },

    /// Build mixed-language omni manifests through the dedicated orchestration layer
    Omni {
        #[command(subcommand)]
        command: omni::OmniCommand,
    },

    /// Validate and scaffold local-first Fabric manifests for polyglot execution
    Fabric {
        #[command(subcommand)]
        command: fabric::FabricCommand,
    },

    /// Build project or file. Without input, reads KAIN.toml for multi-target build.
    Build {
        #[command(subcommand)]
        command: Option<BuildCommand>,

        /// Optional input file. If omitted, builds all targets from KAIN.toml
        input: Option<PathBuf>,

        /// Output file path
        #[arg(short, long)]
        output: Option<PathBuf>,

        /// Single target override for file builds (e.g. ts, rust, wasm)
        #[arg(short, long)]
        target: Option<String>,

        /// Override targets (comma-separated: wasm,js,rust)
        #[arg(long, value_delimiter = ',')]
        targets: Option<Vec<String>>,

        /// Build UE5 plugin from KAIN.toml [ue5] config
        #[arg(long)]
        ue5: bool,

        #[arg(long)]
        r#rust: bool,

        /// Embed original KAIN source as comments in generated C++ (debugging/round-trip)
        #[arg(long)]
        embed: bool,
    },

    /// Run a file (explicit command)
    Run { input: PathBuf },

    /// Generate paired GPU artifacts (SPIR-V, Rust host wrappers, reflection JSON)
    GpuArtifacts {
        input: PathBuf,

        /// Output base path for generated GPU artifacts
        #[arg(short, long)]
        output: Option<PathBuf>,
    },

    /// Inject KAIN file into existing plugin (non-destructive)
    Inject {
        /// Input .kn file(s)
        inputs: Vec<PathBuf>,

        /// Target plugin directory (auto-detected if omitted)
        #[arg(long)]
        plugin_dir: Option<PathBuf>,

        /// Plugin name (auto-detected if omitted)
        #[arg(long)]
        plugin: Option<String>,

        /// Force overwrite existing files
        #[arg(long)]
        force: bool,

        /// Dry run (show what would be generated)
        #[arg(long)]
        dry_run: bool,

        /// Use UE5 codegen
        #[arg(long)]
        ue5: bool,
    },

    /// Import legacy assembly and transliterate into KAIN firmware scaffolding
    ImportAsm {
        /// Input assembly source file
        input: PathBuf,

        /// Input dialect format
        #[arg(long, default_value = "6502-furby")]
        format: String,

        /// Output .kn file (default: Kain/generated/furby_firmware.kn)
        #[arg(long)]
        out: Option<PathBuf>,

        /// Parse/canonicalize and report only, without writing generated .kn and map
        #[arg(long)]
        validate_only: bool,
    },

    /// Import C source code into KAIN
    ImportC {
        /// Input C source file or directory
        input: PathBuf,

        /// Output .kn file (optional - if omitted, only compiles if --target specified)
        #[arg(short, long)]
        output: Option<PathBuf>,

        /// Compilation target (optional - compile directly without writing .kn)
        #[arg(short, long)]
        target: Option<String>,

        /// Include paths for C preprocessor (-I flags)
        #[arg(short = 'I', long)]
        include_paths: Vec<String>,

        /// Preprocessor defines (-D flags)
        #[arg(short = 'D', long)]
        defines: Vec<String>,

        /// Flatten all imported symbols into one global scope (disables per-file modules)
        #[arg(long)]
        flat: bool,

        /// Include only files whose relative path contains one of these filters
        #[arg(long = "include", value_delimiter = ',')]
        include_filters: Vec<String>,

        /// Exclude files whose relative path contains one of these filters
        #[arg(long = "exclude", value_delimiter = ',')]
        exclude_filters: Vec<String>,

        /// Stop on first failed file import (default: continue and report failures)
        #[arg(long)]
        fail_fast: bool,

        /// Write import failure/report JSON (defaults automatically for directory imports with failures)
        #[arg(long)]
        report_json: Option<PathBuf>,
    },

    /// Import Rust source code into KAIN (Project Ouroboros)
    ImportRust {
        /// Input Rust source file or directory
        input: PathBuf,

        /// Output .kn file (optional - if omitted, only compiles if --target specified)
        #[arg(short, long)]
        output: Option<PathBuf>,

        /// Compilation target (optional - compile directly without writing .kn)
        #[arg(short, long)]
        target: Option<String>,

        /// Flatten all imported symbols into one global scope (disables per-file modules)
        #[arg(long)]
        flat: bool,

        /// Include only files whose relative path contains one of these filters
        #[arg(long = "include", value_delimiter = ',')]
        include_filters: Vec<String>,

        /// Exclude files whose relative path contains one of these filters
        #[arg(long = "exclude", value_delimiter = ',')]
        exclude_filters: Vec<String>,

        /// Stop on first failed file import (default: continue and report failures)
        #[arg(long)]
        fail_fast: bool,

        /// Write import failure/report JSON (defaults automatically for directory imports with failures)
        #[arg(long)]
        report_json: Option<PathBuf>,
    },

    /// Import a Rust crate into KAIN through the crate FFI layer
    ImportCrate {
        /// Crate import name used by `use rust::<crate_name>`
        crate_name: String,

        /// Cargo manifest used for workspace/dependency resolution
        #[arg(long)]
        manifest_path: Option<PathBuf>,

        /// Explicit local crate folder or Cargo.toml for standalone crates
        #[arg(long)]
        crate_path: Option<PathBuf>,

        /// Generated artifact mode: live, generate, or both
        #[arg(long, default_value = "both")]
        mode: String,

        /// Output directory for generated KAIN files and reports
        #[arg(short, long)]
        output: Option<PathBuf>,

        /// Override report JSON path
        #[arg(long)]
        report_json: Option<PathBuf>,

        /// Cargo feature list for the resolved crate
        #[arg(long, value_delimiter = ',')]
        features: Vec<String>,

        /// Enable all crate features
        #[arg(long)]
        all_features: bool,

        /// Disable default crate features
        #[arg(long)]
        no_default_features: bool,
    },

    /// Import TypeScript source code into KAIN
    ImportTs {
        /// Input TypeScript source file or directory
        input: PathBuf,

        /// Output .kn file (optional - if omitted, only compiles if --target specified)
        #[arg(short, long)]
        output: Option<PathBuf>,

        /// Compilation target (optional - compile directly without writing .kn)
        #[arg(short, long)]
        target: Option<String>,

        /// Flatten all imported symbols into one global scope (disables per-file modules)
        #[arg(long)]
        flat: bool,

        /// Include only files whose relative path contains one of these filters
        #[arg(long = "include", value_delimiter = ',')]
        include_filters: Vec<String>,

        /// Exclude files whose relative path contains one of these filters
        #[arg(long = "exclude", value_delimiter = ',')]
        exclude_filters: Vec<String>,

        /// Stop on first failed file import (default: continue and report failures)
        #[arg(long)]
        fail_fast: bool,

        /// Write import failure/report JSON (defaults automatically for directory imports with failures)
        #[arg(long)]
        report_json: Option<PathBuf>,
    },
}

fn normalize_script_source(source: String) -> String {
    let source = source.trim_start_matches('\u{feff}').to_string();
    if let Some(rest) = source.strip_prefix("#!") {
        if let Some(newline_index) = rest.find('\n') {
            rest[(newline_index + 1)..].to_string()
        } else {
            String::new()
        }
    } else {
        source
    }
}

fn read_source_from_path(input: &Path) -> Result<String, String> {
    let source = if input == Path::new("-") {
        let mut buffer = String::new();
        io::stdin()
            .read_to_string(&mut buffer)
            .map_err(|err| format!("Failed to read stdin: {err}"))?;
        buffer
    } else {
        fs::read_to_string(input)
            .map_err(|err| format!("Failed to read {}: {err}", input.display()))?
    };
    Ok(normalize_script_source(source))
}

fn run_source(
    source_name: &str,
    source_path: Option<&Path>,
    source: &str,
    target: CompileTarget,
    output: Option<&PathBuf>,
    _emit_ast: bool,
    _emit_typed: bool,
    verbose: bool,
    _analyze: bool,
    plugin_name: Option<&str>,
) -> bool {
    if verbose {
        println!(" Compiling: {}", source_name);
        println!(
            " Source: {} bytes, {} lines",
            source.len(),
            source.lines().count()
        );
    }

    // Compile SPIR-V as binary bytes (not the string summary used by compile()).
    if target == CompileTarget::Spirv {
        let output_path = match output
            .cloned()
            .or_else(|| source_path.map(|path| path.with_extension(target_extension(target))))
        {
            Some(path) => path,
            None => {
                eprintln!(" Output path is required when compiling inline or stdin source.");
                return false;
            }
        };
        match cli::compile_spirv_binary(&source) {
            Ok(spv_bytes) => {
                if !ensure_parent_dir(&output_path) {
                    return false;
                }
                if let Err(e) = fs::write(&output_path, &spv_bytes) {
                    eprintln!(" Failed to write output: {}", e);
                    return false;
                }
                println!(
                    " Compiled to: {} ({} bytes)",
                    output_path.display(),
                    spv_bytes.len()
                );
                return true;
            }
            Err(e) => {
                eprintln!(" Compile error: {}", e);
                return false;
            }
        }
    }

    // Compile
    match compile(&source, target) {
        Ok(compiled_output) => {
            if target == CompileTarget::Interpret || target == CompileTarget::Test {
                let trimmed_output = compiled_output.trim();
                if !trimmed_output.is_empty() && trimmed_output != "()" {
                    println!("{}", compiled_output);
                }
                println!(" Execution complete");
            } else {
                let default_ext = target_extension(target);

                // Determine where to write the primary output
                let output_path = if target == CompileTarget::Llvm {
                    // For LLVM, we always write the IR file first
                    if let Some(out) = output {
                        if out.extension().map_or(false, |e| e == "ll") {
                            out.clone()
                        } else {
                            let mut p = out.clone();
                            p.set_extension("ll");
                            p
                        }
                    } else if let Some(path) = source_path {
                        path.with_extension("ll")
                    } else {
                        eprintln!(
                            " Output path is required when compiling inline or stdin source."
                        );
                        return false;
                    }
                } else if target == CompileTarget::Usf {
                    // For USF, always ensure .usf extension
                    if let Some(out) = output {
                        let mut p = out.clone();
                        p.set_extension("usf");
                        p
                    } else if let Some(path) = source_path {
                        path.with_extension("usf")
                    } else {
                        eprintln!(
                            " Output path is required when compiling inline or stdin source."
                        );
                        return false;
                    }
                } else {
                    match output
                        .cloned()
                        .or_else(|| source_path.map(|path| path.with_extension(default_ext)))
                    {
                        Some(path) => path,
                        None => {
                            eprintln!(
                                " Output path is required when compiling inline or stdin source."
                            );
                            return false;
                        }
                    }
                };

                if !ensure_parent_dir(&output_path) {
                    return false;
                }
                if let Err(e) = fs::write(&output_path, &compiled_output) {
                    eprintln!(" Failed to write output: {}", e);
                    return false;
                }

                println!(
                    " Compiled to: {} ({} bytes)",
                    output_path.display(),
                    compiled_output.len()
                );

                if target == CompileTarget::Llvm {
                    match llvm_native_stage::stage_llvm_native_artifacts(
                        &source,
                        &output_path,
                        None,
                    ) {
                        Ok(staged) => {
                            println!(
                                " Runtime contract: {}",
                                staged.runtime_contract_path.display()
                            );
                            println!(" Realtime bundle: {}", staged.realtime_app_path.display());
                            if let Some(compute_residency_path) = staged.compute_residency_path {
                                println!(
                                    " Compute residency: {}",
                                    compute_residency_path.display()
                                );
                            }
                            if let Some(shader_bundle_path) = staged.shader_bundle_path {
                                println!(" Shader bundle: {}", shader_bundle_path.display());
                            }
                        }
                        Err(err) => {
                            eprintln!(" Failed to stage LLVM native artifacts: {}", err);
                            return false;
                        }
                    }
                }

                // Generate C++ reflection header for USF shaders (GODMODE Phase 3)
                if target == CompileTarget::Usf {
                    // Extract shader name from AST instead of filename
                    let shader_name = {
                        // Parse the source to get the actual shader name
                        match kain_core::Lexer::new(&source).tokenize() {
                            Ok(tokens) => {
                                let span_mapper = kain_core::diagnostics::SpanMapper::new(&source);
                                match kain_core::Parser::new(&tokens, &span_mapper, source_name)
                                    .parse()
                                {
                                    Ok(ast) => {
                                        // Find the first shader in the AST
                                        ast.items
                                            .iter()
                                            .find_map(|item| {
                                                if let kain_core::ast::Item::Shader(shader) = item {
                                                    Some(shader.name.clone())
                                                } else {
                                                    None
                                                }
                                            })
                                            .unwrap_or_else(|| {
                                                source_path
                                                    .and_then(|path| path.file_stem())
                                                    .and_then(|s| s.to_str())
                                                    .unwrap_or("Shader")
                                                    .to_string()
                                            })
                                    }
                                    Err(_) => source_path
                                        .and_then(|path| path.file_stem())
                                        .and_then(|s| s.to_str())
                                        .unwrap_or("Shader")
                                        .to_string(),
                                }
                            }
                            Err(_) => source_path
                                .and_then(|path| path.file_stem())
                                .and_then(|s| s.to_str())
                                .unwrap_or("Shader")
                                .to_string(),
                        }
                    };

                    // Generate header (.h)
                    match cli::generate_usf_header(&source, &shader_name) {
                        Ok(header_code) => {
                            let header_path = output_path.with_extension("h");
                            let header_path = output_path.with_extension("h");
                            if !ensure_parent_dir(&header_path) {
                                eprintln!(" Warning: Failed to create directory for header");
                            } else if let Err(e) = fs::write(&header_path, header_code.as_bytes()) {
                                eprintln!(" Warning: Failed to write header: {}", e);
                            } else {
                                println!(" Generated header: {}", header_path.display());
                            }
                        }
                        Err(e) => {
                            eprintln!(" Warning: Failed to generate header: {}", e);
                        }
                    }

                    // Generate implementation (.cpp) with shader registration
                    let plugin_name_str = plugin_name.unwrap_or("YourPlugin");
                    match cli::generate_usf_implementation(&source, &shader_name, plugin_name_str) {
                        Ok(cpp_code) => {
                            let cpp_path = output_path.with_extension("cpp");
                            let cpp_path = output_path.with_extension("cpp");
                            if !ensure_parent_dir(&cpp_path) {
                                eprintln!(
                                    " Warning: Failed to create directory for implementation"
                                );
                            } else if let Err(e) = fs::write(&cpp_path, cpp_code.as_bytes()) {
                                eprintln!(" Warning: Failed to write implementation: {}", e);
                            } else {
                                println!(" Generated implementation: {}", cpp_path.display());
                            }
                        }
                        Err(e) => {
                            eprintln!(" Warning: Failed to generate implementation: {}", e);
                        }
                    }

                    // GODMODE Phase 7: Shader Complexity Analysis
                    // TODO: Re-implement analyze_shader_complexity
                    // if verbose || analyze {
                    //     match ue5_shaders::analyze_shader_complexity(&source) {
                    //         Ok(report) => {
                    //             println!("{}", report);
                    //         },
                    //         Err(e) => {
                    //             eprintln!(" Warning: Failed to analyze shader: {}", e);
                    //         }
                    //     }
                    // }
                }

                // Generate separate .h and .cpp files for UE5 target (GODMODE)
                if target == CompileTarget::Ue5 {
                    let output_name = output_path.file_stem().and_then(|s| s.to_str());

                    match cli::compile_ue5(&source, output_name, None) {
                        Ok(ue5_output) => {
                            // Write header file
                            let header_path = output_path.with_extension("h");
                            let header_path = output_path.with_extension("h");
                            if !ensure_parent_dir(&header_path) {
                                eprintln!(" Warning: Failed to create directory for header");
                            } else if let Err(e) =
                                fs::write(&header_path, ue5_output.header.as_bytes())
                            {
                                eprintln!(" Warning: Failed to write header: {}", e);
                            } else {
                                println!(
                                    " Generated header: {} ({} bytes)",
                                    header_path.display(),
                                    ue5_output.header.len()
                                );
                            }

                            // Write source file
                            let source_path = output_path.with_extension("cpp");
                            let source_path = output_path.with_extension("cpp");
                            if !ensure_parent_dir(&source_path) {
                                eprintln!(" Warning: Failed to create directory for source");
                            } else if let Err(e) =
                                fs::write(&source_path, ue5_output.source.as_bytes())
                            {
                                eprintln!(" Warning: Failed to write source: {}", e);
                            } else {
                                println!(
                                    " Generated source: {} ({} bytes)",
                                    source_path.display(),
                                    ue5_output.source.len()
                                );
                            }

                            // Write shader files (USF + shader headers + shader cpp)
                            if !ue5_output.shader_files.is_empty() {
                                println!(
                                    " Generated {} shader files:",
                                    ue5_output.shader_files.len()
                                );
                                for (filename, content) in &ue5_output.shader_files {
                                    let shader_path = output_path.with_file_name(filename);
                                    let shader_path = output_path.with_file_name(filename);
                                    if !ensure_parent_dir(&shader_path) {
                                        eprintln!(
                                            "   Warning: Failed to create directory for {}",
                                            filename
                                        );
                                    } else if let Err(e) =
                                        fs::write(&shader_path, content.as_bytes())
                                    {
                                        eprintln!(
                                            "   Warning: Failed to write {}: {}",
                                            filename, e
                                        );
                                    } else {
                                        println!(
                                            "   - {} ({} bytes)",
                                            shader_path.display(),
                                            content.len()
                                        );
                                    }
                                }
                            }
                        }
                        Err(e) => {
                            eprintln!(" Warning: Failed to generate split UE5 files: {}", e);
                        }
                    }
                }

                // Generate separate .h and .cpp files for UE5 Editor target
                if target == CompileTarget::Ue5Editor {
                    match cli::compile_ue5editor(&source, "EditorTools", None) {
                        Ok(editor_output) => {
                            // Write header file
                            let header_path = output_path.with_extension("h");
                            let header_path = output_path.with_extension("h");
                            if !ensure_parent_dir(&header_path) {
                                eprintln!(" Warning: Failed to create directory for header");
                            } else if let Err(e) =
                                fs::write(&header_path, editor_output.header.as_bytes())
                            {
                                eprintln!(" Warning: Failed to write header: {}", e);
                            } else {
                                println!(
                                    " Generated header: {} ({} bytes)",
                                    header_path.display(),
                                    editor_output.header.len()
                                );
                            }

                            // Write source file
                            let source_path = output_path.with_extension("cpp");
                            let source_path = output_path.with_extension("cpp");
                            if !ensure_parent_dir(&source_path) {
                                eprintln!(" Warning: Failed to create directory for source");
                            } else if let Err(e) =
                                fs::write(&source_path, editor_output.source.as_bytes())
                            {
                                eprintln!(" Warning: Failed to write source: {}", e);
                            } else {
                                println!(
                                    " Generated source: {} ({} bytes)",
                                    source_path.display(),
                                    editor_output.source.len()
                                );
                            }
                        }
                        Err(e) => {
                            eprintln!(" Warning: Failed to generate UE5 Editor files: {}", e);
                        }
                    }
                }

                // Post-processing for LLVM
                if target == CompileTarget::Llvm {
                    let exe_path = if let Some(out) = output {
                        if out.extension().map_or(false, |e| e == "ll") {
                            if cfg!(windows) {
                                out.with_extension("exe")
                            } else {
                                out.with_extension("")
                            }
                        } else {
                            out.clone()
                        }
                    } else {
                        let Some(path) = source_path else {
                            eprintln!(
                                " Output path is required when compiling inline or stdin source."
                            );
                            return false;
                        };
                        if cfg!(windows) {
                            path.with_extension("exe")
                        } else {
                            path.with_extension("")
                        }
                    };

                    println!(" Linking executable...");

                    // Find clang: bundled toolchain > PATH > system install
                    let clang_cmd = find_bundled_clang()
                        .or_else(|| {
                            // Try PATH
                            if std::process::Command::new("clang")
                                .arg("--version")
                                .output()
                                .is_ok()
                            {
                                Some("clang".to_string())
                            } else {
                                None
                            }
                        })
                        .or_else(|| {
                            // Try standard Windows install
                            let default_path = r"C:\Program Files\LLVM\bin\clang.exe";
                            if std::path::Path::new(default_path).exists() {
                                Some(default_path.to_string())
                            } else {
                                None
                            }
                        })
                        .unwrap_or_else(|| "clang".to_string());

                    let mut cmd = std::process::Command::new(&clang_cmd);
                    let mut runtime_link_libs = Vec::new();

                    if let Some(runtime_bundle) = match resolve_native_runtime_bundle() {
                        Ok(bundle) => bundle,
                        Err(err) => {
                            eprintln!(" Failed to resolve native runtime bundle: {}", err);
                            return false;
                        }
                    } {
                        let runtime_objects = match compile_native_runtime_bundle(
                            &runtime_bundle,
                            &clang_cmd,
                            &exe_path,
                        ) {
                            Ok(objects) => objects,
                            Err(err) => {
                                eprintln!(" Failed to compile runtime library: {}", err);
                                return false;
                            }
                        };
                        for object in runtime_objects {
                            cmd.arg(object);
                        }
                        runtime_link_libs = runtime_bundle.link_libs;
                        if runtime_bundle.uses_cpp_runtime {
                            runtime_link_libs = unique_link_libs(
                                [runtime_link_libs, default_native_runtime_cpp_link_libs()]
                                    .concat(),
                            );
                        }
                    }

                    cmd.arg(&output_path)
                        .arg("-o")
                        .arg(&exe_path)
                        .arg("-Wno-override-module")
                        .arg("-g"); // Debug info

                    runtime_link_libs = unique_link_libs(
                        [runtime_link_libs, default_native_runtime_link_libs()].concat(),
                    );

                    for link_lib in runtime_link_libs {
                        cmd.arg(format!("-l{}", link_lib));
                    }

                    let status = cmd.status();

                    match status {
                        Ok(s) if s.success() => {
                            println!(" Generated executable: {}", exe_path.display());
                            match llvm_native_stage::stage_gpu_runtime_dll(&exe_path) {
                                Ok(Some(dll_path)) => {
                                    println!(" Compute runtime DLL: {}", dll_path.display());
                                }
                                Ok(None) => {}
                                Err(err) => {
                                    eprintln!(" Failed to stage compute runtime DLL: {}", err);
                                    return false;
                                }
                            }
                        }
                        Ok(_) => {
                            eprintln!(" Linking failed.");
                            return false;
                        }
                        Err(_) => {
                            eprintln!(" 'clang' not found in PATH or standard locations.");
                            eprintln!("   To generate an executable, install LLVM and run:");
                            eprintln!(
                                "   clang {} -o {}",
                                output_path.display(),
                                exe_path.display()
                            );
                            return false;
                        }
                    }
                }
            }
            true
        }
        Err(e) => {
            // Use pretty error formatting
            let diag = kain_core::diagnostics::Diagnostics::new(&source, source_name);
            eprint!("{}", diag.format_error(&e));
            false
        }
    }
}

fn run_compile(
    input: &PathBuf,
    target: CompileTarget,
    output: Option<&PathBuf>,
    emit_ast: bool,
    emit_typed: bool,
    verbose: bool,
    analyze: bool,
    plugin_name: Option<&str>,
) -> bool {
    let source = match read_source_from_path(input) {
        Ok(value) => value,
        Err(err) => {
            eprintln!(" {}", err);
            return false;
        }
    };
    let source_name = if input == Path::new("-") {
        "<stdin>"
    } else {
        input.to_str().unwrap_or("<input>")
    };
    let source_path = if input == Path::new("-") {
        None
    } else {
        Some(input.as_path())
    };
    run_source(
        source_name,
        source_path,
        &source,
        target,
        output,
        emit_ast,
        emit_typed,
        verbose,
        analyze,
        plugin_name,
    )
}

fn run_kn_repl() -> bool {
    println!(
        "Kain {} (build {}) [{}]",
        VERSION, BUILD_NUMBER, BUILD_TARGET_TRIPLE
    );

    let stdin = io::stdin();
    let mut stdin = stdin.lock();
    let mut stdout = io::stdout();
    let mut buffer = String::new();
    let mut line = String::new();

    loop {
        let prompt = if buffer.trim().is_empty() {
            ">>> "
        } else {
            "... "
        };
        if write!(stdout, "{}", prompt)
            .and_then(|_| stdout.flush())
            .is_err()
        {
            eprintln!(" Failed to write REPL prompt.");
            return false;
        }

        line.clear();
        let bytes_read = match stdin.read_line(&mut line) {
            Ok(value) => value,
            Err(err) => {
                eprintln!(" Failed to read REPL input: {}", err);
                return false;
            }
        };

        if bytes_read == 0 {
            if buffer.trim().is_empty() {
                println!();
                return true;
            }
            let source = normalize_script_source(std::mem::take(&mut buffer));
            if !run_source(
                "<repl>",
                None,
                &source,
                CompileTarget::Interpret,
                None,
                false,
                false,
                false,
                false,
                None,
            ) {
                return false;
            }
            println!();
            return true;
        }

        let trimmed = line.trim_end_matches(['\r', '\n']);
        match trimmed {
            ".quit" | ".exit" => {
                println!();
                return true;
            }
            ".clear" => {
                buffer.clear();
                continue;
            }
            ".run" => {
                if buffer.trim().is_empty() {
                    continue;
                }
                let source = normalize_script_source(std::mem::take(&mut buffer));
                if !run_source(
                    "<repl>",
                    None,
                    &source,
                    CompileTarget::Interpret,
                    None,
                    false,
                    false,
                    false,
                    false,
                    None,
                ) {
                    return false;
                }
                continue;
            }
            _ => {}
        }

        if trimmed.is_empty() {
            if buffer.trim().is_empty() {
                continue;
            }
            let source = normalize_script_source(std::mem::take(&mut buffer));
            if !run_source(
                "<repl>",
                None,
                &source,
                CompileTarget::Interpret,
                None,
                false,
                false,
                false,
                false,
                None,
            ) {
                return false;
            }
            continue;
        }

        buffer.push_str(&line);
    }
}

fn parse_artifact_mode(value: &str) -> Result<ArtifactMode, String> {
    match value.trim().to_ascii_lowercase().as_str() {
        "live" => Ok(ArtifactMode::Live),
        "generate" | "gen" => Ok(ArtifactMode::Generate),
        "both" => Ok(ArtifactMode::Both),
        other => Err(format!(
            "Unknown crate FFI mode '{other}'. Use one of: live, generate, both"
        )),
    }
}

fn watch_mode(
    input: PathBuf,
    target: CompileTarget,
    output: Option<PathBuf>,
    emit_ast: bool,
    emit_typed: bool,
    verbose: bool,
    analyze: bool,
    plugin_name: Option<String>,
) {
    use notify::{Event, RecursiveMode, Watcher};
    use std::sync::mpsc::channel;

    let running = Arc::new(AtomicBool::new(true));
    let r = running.clone();

    ctrlc::set_handler(move || {
        println!("\n Stopping watch mode...");
        r.store(false, Ordering::SeqCst);
    })
    .expect("Error setting Ctrl-C handler");

    println!(
        " Watching {} for changes... (Ctrl+C to stop)",
        input.display()
    );
    println!("");

    // Initial compile
    run_compile(
        &input,
        target,
        output.as_ref(),
        emit_ast,
        emit_typed,
        verbose,
        analyze,
        plugin_name.as_deref(),
    );
    println!("");

    let (tx, rx) = channel();

    let mut watcher = notify::recommended_watcher(move |res: Result<Event, notify::Error>| {
        if let Ok(event) = res {
            if event.kind.is_modify() {
                let _ = tx.send(());
            }
        }
    })
    .expect("Failed to create watcher");

    watcher
        .watch(&input, RecursiveMode::NonRecursive)
        .expect("Failed to watch file");

    // Also watch parent directory in case file is replaced
    if let Some(parent) = input.parent() {
        let _ = watcher.watch(parent, RecursiveMode::NonRecursive);
    }

    while running.load(Ordering::SeqCst) {
        match rx.recv_timeout(Duration::from_millis(100)) {
            Ok(_) => {
                // Debounce - wait a bit for writes to settle
                std::thread::sleep(Duration::from_millis(50));
                // Drain any pending events
                while rx.try_recv().is_ok() {}

                println!(" File changed, recompiling...");
                println!("");
                run_compile(
                    &input,
                    target,
                    output.as_ref(),
                    emit_ast,
                    emit_typed,
                    verbose,
                    analyze,
                    plugin_name.as_deref(),
                );
                println!("");
            }
            Err(std::sync::mpsc::RecvTimeoutError::Timeout) => {
                // Keep looping
            }
            Err(std::sync::mpsc::RecvTimeoutError::Disconnected) => {
                break;
            }
        }
    }
}

fn main() {
    let builder = std::thread::Builder::new()
        .name("main-thread".into())
        .stack_size(8 * 1024 * 1024); // 8MB

    let handler = builder
        .spawn(|| {
            let launcher = detect_launcher_from_path(std::env::current_exe().ok().as_deref());
            let matches = Args::command()
                .bin_name(launcher.display_name())
                .get_matches();
            let args = Args::from_arg_matches(&matches).unwrap_or_else(|err| err.exit());
            let mut stdin_source = None;
            if args.command.is_none()
                && args.input.is_none()
                && args.code.is_none()
                && launcher.prefers_interpret_default()
                && !io::stdin().is_terminal()
            {
                let mut buffer = String::new();
                if io::stdin().read_to_string(&mut buffer).is_ok() && !buffer.trim().is_empty() {
                    stdin_source = Some(normalize_script_source(buffer));
                }
            }

            println!(
                " {} Compiler v{} (build {})",
                LANGUAGE_NAME, VERSION, BUILD_NUMBER
            );

            if launcher.prefers_interpret_default()
                && args.command.is_none()
                && args.input.is_none()
                && args.code.is_none()
                && io::stdin().is_terminal()
            {
                if !run_kn_repl() {
                    std::process::exit(1);
                }
                return;
            }

            if should_show_launcher_menu(
                launcher,
                args.command.is_some(),
                args.input.is_some() || args.code.is_some() || stdin_source.is_some(),
            ) {
                if let Some(menu) = render_launcher_menu(launcher) {
                    print!("{menu}");
                }
                return;
            }

            match args.command {
                Some(Commands::Init { path, name }) => {
                    if let Err(e) = packager::init_project(&path, name) {
                        eprintln!(" Init failed: {}", e);
                    }
                }
                Some(Commands::Lsp) => {
                    eprintln!(" Starting KAIN Language Server...");
                    // Manual runtime for LSP
                    let rt = tokio::runtime::Builder::new_multi_thread()
                        .enable_all()
                        .build()
                        .expect("Failed to create tokio runtime");

                    rt.block_on(async {
                        lsp::run_server().await;
                    });
                }
                Some(Commands::Doctor {
                    repair: repair_args,
                }) => {
                    if let Some(mode) = repair_args.selected_mode() {
                        let profile_label = repair_args.selected_profile_label();
                        match repair_args.target_kind() {
                            Some(repair::DoctorRepairTargetKind::File) => {
                                let Some(path) = repair_args.repair.as_ref() else {
                                    eprintln!(" Doctor repair requested without a file path.");
                                    std::process::exit(1);
                                };
                                match repair::run(path, repair_args.profile, mode) {
                                    Ok(report) => {
                                        print_repair_report(path, &report, mode, profile_label);
                                    }
                                    Err(err) => {
                                        eprintln!(" Doctor repair failed: {}", err);
                                        std::process::exit(1);
                                    }
                                }
                            }
                            Some(repair::DoctorRepairTargetKind::Tree) => {
                                let Some(root) = repair_args.repair_tree.as_ref() else {
                                    eprintln!(
                                        " Doctor repair tree requested without a directory path."
                                    );
                                    std::process::exit(1);
                                };
                                match repair::run_tree(root, repair_args.profile, mode) {
                                    Ok(report) => {
                                        print_repair_tree_report(
                                            root,
                                            &report,
                                            mode,
                                            profile_label,
                                        );
                                    }
                                    Err(err) => {
                                        eprintln!(" Doctor repair tree failed: {}", err);
                                        std::process::exit(1);
                                    }
                                }
                            }
                            None => {
                                eprintln!(" Doctor repair requested without a target path.");
                                std::process::exit(1);
                            }
                        }
                        return;
                    }
                    print_doctor(launcher);
                }
                Some(Commands::Selfhost { command }) => {
                    if let Err(e) = selfhost::run(command) {
                        eprintln!(" Selfhost failed: {}", e);
                        std::process::exit(1);
                    }
                }
                Some(Commands::Omni { command }) => {
                    if let Err(e) = omni::run(command) {
                        eprintln!(" Omni command failed: {}", e);
                        std::process::exit(1);
                    }
                }
                Some(Commands::Fabric { command }) => {
                    if let Err(e) = fabric::run(command) {
                        eprintln!(" Fabric command failed: {}", e);
                        std::process::exit(1);
                    }
                }
                Some(Commands::Build {
                    command,
                    input,
                    output,
                    target,
                    targets,
                    ue5,
                    r#rust,
                    embed,
                }) => {
                    if let Some(BuildCommand::NativeUi {
                        input,
                        root_component,
                        app_name,
                        window_title,
                        project_dir,
                        artifact_dir,
                        bundle_only,
                        release,
                        runtime_crate,
                        runtime_path,
                        runtime_version,
                    }) = command
                    {
                        let runtime_dependency = if let Some(path) = runtime_path {
                            native_ui_build::NativeUiRuntimeDependencyConfig::Path(path)
                        } else if let Some(version) = runtime_version {
                            native_ui_build::NativeUiRuntimeDependencyConfig::Version(version)
                        } else {
                            native_ui_build::NativeUiRuntimeDependencyConfig::WorkspacePath
                        };
                        let config = native_ui_build::NativeUiBuildConfig {
                            root_component,
                            window_title,
                            app_name,
                            project_dir,
                            artifact_output_dir: artifact_dir
                                .unwrap_or_else(|| PathBuf::from("generated")),
                            build_executable: !bundle_only,
                            release,
                            runtime_crate_name: runtime_crate,
                            runtime_dependency,
                            ..Default::default()
                        };

                        match native_ui_build::run_native_ui_build_pipeline(&input, &config) {
                            Ok(result) => {
                                println!(
                                    " Native UI app: {} ({})",
                                    result.metadata.app_name, result.metadata.root_component
                                );
                                for path in result.written_paths() {
                                    println!("   ✓ {}", path.display());
                                }
                            }
                            Err(e) => {
                                eprintln!(" Native UI build failed: {}", e);
                                std::process::exit(1);
                            }
                        }
                    } else if ue5 {
                        // UE5 plugin build
                        if let Err(e) = packager::build_ue5_plugin_with_options(embed) {
                            // Error already contains formatted details with file:line:col
                            eprintln!("{}", e);
                            std::process::exit(1);
                        }
                    } else if r#rust {
                        match input {
                            Some(file) => {
                                match rust_build::run_rust_build_pipeline(
                                    &file,
                                    output.as_ref(),
                                    None,
                                ) {
                                    Ok(paths) => {
                                        for path in paths {
                                            println!("   ✓ {}", path.display());
                                        }
                                    }
                                    Err(e) => {
                                        eprintln!(" Rust build failed: {}", e);
                                        std::process::exit(1);
                                    }
                                }
                            }
                            None => {
                                if let Err(e) = packager::build_rust_project() {
                                    eprintln!(" Build failed: {}", e);
                                    std::process::exit(1);
                                }
                            }
                        }
                    } else {
                        match input {
                            Some(file) => {
                                let target_alias =
                                    target.as_deref().unwrap_or(args.target.as_str());
                                let Some(resolved_target) = parse_compile_target(target_alias)
                                else {
                                    eprintln!(
                                        " Unknown target: {}. Use: {}",
                                        target_alias,
                                        supported_targets_csv()
                                    );
                                    std::process::exit(1);
                                };
                                run_compile(
                                    &file,
                                    resolved_target,
                                    output.as_ref(),
                                    args.emit_ast,
                                    args.emit_typed,
                                    args.verbose,
                                    args.analyze,
                                    args.plugin.as_deref(),
                                );
                            }
                            None => {
                                // Project build from KAIN.toml
                                let target_overrides = if let Some(single_target) = target {
                                    Some(vec![single_target])
                                } else {
                                    targets.clone()
                                };
                                if let Err(e) = packager::build_project(target_overrides) {
                                    eprintln!(" Build failed: {}", e);
                                    std::process::exit(1);
                                }
                            }
                        }
                    }
                }
                Some(Commands::Run { input }) => {
                    run_compile(
                        &input,
                        CompileTarget::Interpret,
                        None,
                        args.emit_ast,
                        args.emit_typed,
                        args.verbose,
                        args.analyze,
                        args.plugin.as_deref(),
                    );
                }
                Some(Commands::GpuArtifacts { input, output }) => {
                    let config = packager::RustBuildConfig {
                        output: None,
                        artifacts: vec![
                            packager::RustBuildArtifact::ShaderHost,
                            packager::RustBuildArtifact::ShaderReflection,
                            packager::RustBuildArtifact::Spirv,
                        ],
                        native_ui: None,
                    };

                    match rust_build::run_rust_build_pipeline(
                        &input,
                        output.as_ref(),
                        Some(&config),
                    ) {
                        Ok(paths) => {
                            println!(" Generated {} Rust shader artifact files:", paths.len());
                            for path in paths {
                                println!("   - {}", path.display());
                            }
                        }
                        Err(e) => {
                            eprintln!(" Rust shader artifact generation failed: {}", e);
                            std::process::exit(1);
                        }
                    }
                }
                Some(Commands::Inject {
                    inputs,
                    plugin_dir,
                    plugin,
                    force,
                    dry_run,
                    ue5,
                }) => {
                    if ue5 {
                        if let Err(e) = packager::inject_into_plugin(
                            &inputs,
                            plugin_dir.as_ref(),
                            plugin.as_deref(),
                            force,
                            dry_run,
                        ) {
                            eprintln!(" Injection failed: {}", e);
                            std::process::exit(1);
                        }
                    } else {
                        eprintln!(" Only --ue5 target is supported for inject command");
                        std::process::exit(1);
                    }
                }
                Some(Commands::ImportAsm {
                    input,
                    format,
                    out,
                    validate_only,
                }) => {
                    match import_asm::import_asm(&input, &format, out.as_deref(), validate_only) {
                        Ok(result) => {
                            println!(" Import complete");
                            println!(" Canonical ASM: {}", result.canonical_asm_path.display());
                            if !validate_only {
                                println!(" Generated KAIN: {}", result.generated_kn_path.display());
                                println!(" Mapping JSON: {}", result.map_json_path.display());
                            }
                            println!(" Recovery report: {}", result.report_json_path.display());
                            println!(
                                " Parsed blocks: {}, data tables: {}, directives: {}",
                                result.parsed.blocks.len(),
                                result.parsed.data_tables.len(),
                                result.parsed.directives.len()
                            );
                        }
                        Err(e) => {
                            eprintln!(" import-asm failed: {}", e);
                            std::process::exit(1);
                        }
                    }
                }
                Some(Commands::ImportC {
                    input,
                    output,
                    target,
                    include_paths,
                    defines,
                    flat,
                    include_filters,
                    exclude_filters,
                    fail_fast,
                    report_json,
                }) => {
                    let batch = import_c::ImportCBatchOptions {
                        recursive: true,
                        flat,
                        include_filters,
                        exclude_filters,
                        fail_fast,
                        report_json,
                    };

                    match import_c::import_c_with_batch(
                        &input,
                        output.as_deref(),
                        target.as_deref(),
                        &include_paths,
                        &defines,
                        &batch,
                    ) {
                        Ok(_) => {
                            // Success message already printed by import_c
                        }
                        Err(e) => {
                            eprintln!("❌ import-c failed: {}", e);
                            std::process::exit(1);
                        }
                    }
                }
                Some(Commands::ImportRust {
                    input,
                    output,
                    target,
                    flat,
                    include_filters,
                    exclude_filters,
                    fail_fast,
                    report_json,
                }) => {
                    let batch = import_rust::ImportRustBatchOptions {
                        recursive: true,
                        flat,
                        include_filters,
                        exclude_filters,
                        fail_fast,
                        report_json,
                    };

                    match import_rust::import_rust_with_batch(
                        &input,
                        output.as_deref(),
                        target.as_deref(),
                        &batch,
                    ) {
                        Ok(_) => {
                            // Success message already printed by import_rust
                        }
                        Err(e) => {
                            eprintln!("❌ import-rust failed: {}", e);
                            std::process::exit(1);
                        }
                    }
                }
                Some(Commands::ImportCrate {
                    crate_name,
                    manifest_path,
                    crate_path,
                    mode,
                    output,
                    report_json,
                    features,
                    all_features,
                    no_default_features,
                }) => {
                    let mode = match parse_artifact_mode(&mode) {
                        Ok(value) => value,
                        Err(err) => {
                            eprintln!("❌ import-crate failed: {}", err);
                            std::process::exit(1);
                        }
                    };
                    let options = ImportCrateOptions {
                        manifest_path,
                        crate_path,
                        output_dir: output,
                        report_json,
                        mode,
                        features,
                        all_features,
                        no_default_features,
                    };

                    if let Err(err) = import_crate::import_crate(&crate_name, options) {
                        eprintln!("❌ import-crate failed: {}", err);
                        std::process::exit(1);
                    }
                }
                Some(Commands::ImportTs {
                    input,
                    output,
                    target,
                    flat,
                    include_filters,
                    exclude_filters,
                    fail_fast,
                    report_json,
                }) => {
                    let batch = import_typescript::ImportTypeScriptBatchOptions {
                        recursive: true,
                        flat,
                        include_filters,
                        exclude_filters,
                        fail_fast,
                        report_json,
                    };

                    match import_typescript::import_typescript_with_batch(
                        &input,
                        output.as_deref(),
                        target.as_deref(),
                        &batch,
                    ) {
                        Ok(_) => {}
                        Err(e) => {
                            eprintln!("❌ import-ts failed: {}", e);
                            std::process::exit(1);
                        }
                    }
                }
                None => {
                    // Legacy behavior
                    if let Some(ref input) = args.input {
                        if args.target.as_str() == "ue5-shader" {
                            if args.watch {
                                eprintln!(" Watch mode is not supported for ue5-shader target.");
                            }
                            if !run_ue5_shader_pipeline(&input, &args) {
                                std::process::exit(1);
                            }
                        } else {
                            let resolved_target_alias = resolve_legacy_target_alias(
                                launcher,
                                &args.target,
                                args.output.is_some(),
                            );
                            let Some(target) = parse_compile_target(&resolved_target_alias) else {
                                eprintln!(
                                    " Unknown target: {}. Use: {}",
                                    resolved_target_alias,
                                    supported_targets_csv()
                                );
                                std::process::exit(1);
                            };

                            if args.watch {
                                watch_mode(
                                    input.clone(),
                                    target,
                                    args.output.clone(),
                                    args.emit_ast,
                                    args.emit_typed,
                                    args.verbose,
                                    args.analyze,
                                    args.plugin.clone(),
                                );
                            } else {
                                if !run_compile(
                                    &input,
                                    target,
                                    args.output.as_ref(),
                                    args.emit_ast,
                                    args.emit_typed,
                                    args.verbose,
                                    args.analyze,
                                    args.plugin.as_deref(),
                                ) {
                                    std::process::exit(1);
                                }
                            }
                        }
                    } else if let Some(code) = args.code.as_deref() {
                        let resolved_target_alias = resolve_legacy_target_alias(
                            launcher,
                            &args.target,
                            args.output.is_some(),
                        );
                        let Some(target) = parse_compile_target(&resolved_target_alias) else {
                            eprintln!(
                                " Unknown target: {}. Use: {}",
                                resolved_target_alias,
                                supported_targets_csv()
                            );
                            std::process::exit(1);
                        };
                        if args.watch {
                            eprintln!(" Watch mode is only supported for file-backed input.");
                            std::process::exit(1);
                        }
                        if !run_source(
                            "<inline>",
                            None,
                            code,
                            target,
                            args.output.as_ref(),
                            args.emit_ast,
                            args.emit_typed,
                            args.verbose,
                            args.analyze,
                            args.plugin.as_deref(),
                        ) {
                            std::process::exit(1);
                        }
                    } else if let Some(stdin_source) = stdin_source.as_deref() {
                        let resolved_target_alias = resolve_legacy_target_alias(
                            launcher,
                            &args.target,
                            args.output.is_some(),
                        );
                        let Some(target) = parse_compile_target(&resolved_target_alias) else {
                            eprintln!(
                                " Unknown target: {}. Use: {}",
                                resolved_target_alias,
                                supported_targets_csv()
                            );
                            std::process::exit(1);
                        };
                        if args.watch {
                            eprintln!(" Watch mode is only supported for file-backed input.");
                            std::process::exit(1);
                        }
                        if !run_source(
                            "<stdin>",
                            None,
                            stdin_source,
                            target,
                            args.output.as_ref(),
                            args.emit_ast,
                            args.emit_typed,
                            args.verbose,
                            args.analyze,
                            args.plugin.as_deref(),
                        ) {
                            std::process::exit(1);
                        }
                    } else {
                        eprintln!(" No input file provided. Use --help for usage.");
                    }
                }
            }
        })
        .unwrap();

    handler.join().unwrap();
}

fn print_repair_report(
    path: &Path,
    report: &kain_repair::RepairReport,
    mode: kain_repair::RepairMode,
    profile: &str,
) {
    println!(" Repair Target: {}", path.display());
    println!(" Selected Profile: {}", profile);
    println!(" Repair Mode: {:?}", mode);
    println!(" Safety Class: {:?}", report.safety_class);
    println!(" Unknown Risk: {:?}", report.remaining_unknown_risk);
    println!(
        " Parser Proof: {}",
        match report.parser_proof_status() {
            Some(kain_repair::ParserProofStatus::Passed) => "passed",
            Some(kain_repair::ParserProofStatus::Failed) => "failed",
            None => "unverified",
        }
    );

    if report.fixes.is_empty() {
        println!(" Fixes Applied: none needed");
        println!(" Remaining Diagnostics: 0");
        return;
    }

    println!(" Fixes Applied: {}", report.fixes_applied);
    println!(" Fixes:");
    for fix in &report.fixes {
        let is_aggressive = is_aggressive_fix(fix);
        let note = fix.note.as_deref().unwrap_or("repair applied");
        println!(
            "   - [{}] {:?}: {}",
            if is_aggressive { "aggressive" } else { "safe" },
            fix.kind,
            note
        );
    }
    println!(" Remaining Diagnostics: 0");
}

fn print_repair_tree_report(
    root: &Path,
    report: &repair::DoctorRepairBatchReport,
    mode: kain_repair::RepairMode,
    profile: &str,
) {
    println!(" Repair Root: {}", root.display());
    println!(" Selected Profile: {}", profile);
    println!(" Repair Mode: {:?}", mode);
    println!(
        " Action Class: {}",
        if matches!(mode, kain_repair::RepairMode::ApplyAggressive) {
            "aggressive"
        } else {
            "safe"
        }
    );
    println!(" Files Scanned: {}", report.scanned);
    println!(" Files Changed: {}", report.changed);
    println!(" Files Written: {}", report.written);
    println!(" Files Failed: {}", report.failed);
    for outcome in &report.outcomes {
        match &outcome.result {
            Ok(file_report) => {
                println!(
                    "   - {} [{}] {}",
                    outcome.path.display(),
                    if file_report.changed() {
                        "changed"
                    } else {
                        "unchanged"
                    },
                    if file_report.fixes.is_empty() {
                        "no fixes"
                    } else {
                        "repairs applied"
                    }
                );
            }
            Err(err) => {
                println!("   - {} [failed] {}", outcome.path.display(), err);
            }
        }
    }
}

fn is_aggressive_fix(fix: &kain_repair::AppliedFix) -> bool {
    matches!(
        fix.kind,
        kain_repair::FixKind::RewriteReservedIdentifier
            | kain_repair::FixKind::NormalizeSelfConstructorSyntax
            | kain_repair::FixKind::RewriteInlineInitialization
            | kain_repair::FixKind::NormalizeNamespacePath
            | kain_repair::FixKind::ReconstructParserSafeBlock
    )
}

fn print_doctor(active_launcher: LauncherKind) {
    let current_exe = std::env::current_exe().ok();
    let kain_path_command = which::which("kain").ok();
    let kn_path_command = which::which("kn").ok();
    let kain_path_matches = collect_path_matches("kain");
    let kn_path_matches = collect_path_matches("kn");
    let stdlib_roots = kain_core::stdlib::find_stdlib_search_roots();
    let runtime_c = find_runtime_c();
    let runtime_manifest = find_native_runtime_manifest();
    let resolved_clang = if cfg!(feature = "sys") {
        find_bundled_clang()
    } else {
        None
    };

    println!(" KAIN Doctor");
    println!(" Version: {}", VERSION);
    println!(" Build: {}", BUILD_NUMBER);
    println!(" Built At (UTC): {}", format_build_time(BUILD_UNIX_TIME));
    println!(
        " Git: {} (commit #{}, {})",
        BUILD_GIT_SHA, BUILD_GIT_COMMIT_COUNT, BUILD_GIT_DIRTY
    );
    println!(" Profile: {}", BUILD_PROFILE);
    println!(
        " Target Triple: {} (host {})",
        BUILD_TARGET_TRIPLE, BUILD_HOST_TRIPLE
    );

    match &current_exe {
        Some(path) => {
            println!(" Binary Path: {}", path.display());
            println!(" Binary Kind: {}", classify_binary_path(path));
        }
        None => println!(" Binary Path: <unknown>"),
    }
    println!(" Active Launcher: {}", active_launcher.display_name());

    match &kain_path_command {
        Some(path) => println!(" PATH kain: {}", path.display()),
        None => println!(" PATH kain: <not found>"),
    }

    match &kn_path_command {
        Some(path) => println!(" PATH kn: {}", path.display()),
        None => println!(" PATH kn: <not found>"),
    }

    if !kain_path_matches.is_empty() {
        println!(" PATH Matches (kain):");
        for path in &kain_path_matches {
            println!("   - {}", path.display());
        }
    }

    if !kn_path_matches.is_empty() {
        println!(" PATH Matches (kn):");
        for path in &kn_path_matches {
            println!("   - {}", path.display());
        }
    }

    match std::env::current_dir() {
        Ok(cwd) => {
            println!(" Current Dir: {}", cwd.display());
            if let Some(root) = find_project_root(&cwd) {
                println!(" Project Root: {}", root.display());
            } else {
                println!(" Project Root: <not found (no KAIN.toml in parent chain)>");
            }
        }
        Err(err) => println!(" Current Dir: <unknown> ({})", err),
    }

    println!(" Supported Targets: {}", supported_targets_csv());
    println!(
        " TS Target Available: {}",
        if parse_compile_target("ts").is_some() {
            "yes"
        } else {
            "no"
        }
    );
    println!(
        " Features: ue5={}, web={}, gpu={}, sys={}",
        if cfg!(feature = "ue5") { "on" } else { "off" },
        if cfg!(feature = "web") { "on" } else { "off" },
        if cfg!(feature = "gpu") { "on" } else { "off" },
        if cfg!(feature = "sys") { "on" } else { "off" },
    );

    println!(
        " PATH Match Status (kain): {}",
        doctor_path_status(current_exe.as_deref(), kain_path_command.as_deref(), "kain")
    );
    println!(
        " PATH Match Status (kn): {}",
        doctor_path_status(current_exe.as_deref(), kn_path_command.as_deref(), "kn")
    );

    if stdlib_roots.is_empty() {
        println!(" Resolved Stdlib Roots: <none>");
    } else {
        println!(" Resolved Stdlib Roots:");
        for root in &stdlib_roots {
            println!("   - {}", root.display());
        }
    }

    match runtime_c {
        Some(path) => println!(" Resolved Runtime C: {}", path.display()),
        None => println!(" Resolved Runtime C: <not found>"),
    }

    match runtime_manifest {
        Some(path) => println!(" Resolved Runtime Manifest: {}", path.display()),
        None => println!(" Resolved Runtime Manifest: <not found>"),
    }

    if cfg!(feature = "sys") {
        match resolved_clang {
            Some(path) => println!(" Resolved LLVM Clang: {}", path),
            None => println!(" Resolved LLVM Clang: <not found in bundled locations>"),
        }
    }

    if let Some(path) = current_exe.as_deref() {
        if is_repo_target_binary(path) {
            println!(" Warning: active kain comes from a repo target directory.");
            if cfg!(windows) {
                println!(
                    "          Refresh/install a stable PATH binary with scripts/sync-kain-source-of-truth.ps1."
                );
            } else {
                println!(
                    "          Refresh/install a stable PATH binary with python3 install_kain.py and source generated/kain-env.sh."
                );
            }
        }
    }
}

fn format_build_time(unix_time: &str) -> String {
    let Ok(secs) = unix_time.parse::<i64>() else {
        return format!("unknown (raw: {})", unix_time);
    };

    let Some(dt) = chrono::DateTime::<chrono::Utc>::from_timestamp(secs, 0) else {
        return format!("unknown (raw: {})", unix_time);
    };

    format!("{} (unix {})", dt.to_rfc3339(), unix_time)
}

fn classify_binary_path(path: &Path) -> &'static str {
    if is_repo_target_binary(path) {
        "repo-target"
    } else if is_cargo_bin_binary(path) {
        "cargo-bin"
    } else {
        "custom"
    }
}

fn is_repo_target_binary(path: &Path) -> bool {
    let normalized = path
        .to_string_lossy()
        .replace('/', "\\")
        .to_ascii_lowercase();
    normalized.contains("\\target\\debug\\") || normalized.contains("\\target\\release\\")
}

fn is_cargo_bin_binary(path: &Path) -> bool {
    let cargo_home = std::env::var_os("CARGO_HOME")
        .map(PathBuf::from)
        .or_else(|| {
            std::env::var_os("USERPROFILE").map(|profile| PathBuf::from(profile).join(".cargo"))
        });
    cargo_home
        .map(|home| path.starts_with(home.join("bin")))
        .unwrap_or(false)
}

fn collect_path_matches(command_name: &str) -> Vec<PathBuf> {
    which::which_all(command_name)
        .ok()
        .map(|mut paths| paths.by_ref().take(4).collect::<Vec<_>>())
        .unwrap_or_default()
}

fn doctor_path_status(
    current_exe: Option<&Path>,
    path_command: Option<&Path>,
    command_name: &str,
) -> String {
    match (current_exe, path_command) {
        (Some(current), Some(path_entry)) if paths_equivalent(current, path_entry) => {
            "current process matches PATH".to_string()
        }
        (Some(_), Some(_)) => "drift: current process differs from PATH".to_string(),
        (Some(_), None) => {
            format!("current process exists, but {command_name} is not resolvable from PATH")
        }
        (None, Some(_)) => {
            format!("PATH resolves {command_name}, but current process path is unknown")
        }
        (None, None) => "unknown".to_string(),
    }
}

fn paths_equivalent(a: &Path, b: &Path) -> bool {
    match (fs::canonicalize(a), fs::canonicalize(b)) {
        (Ok(left), Ok(right)) => left == right,
        _ => a == b,
    }
}

fn find_project_root(start: &std::path::Path) -> Option<PathBuf> {
    for dir in start.ancestors() {
        if dir.join("KAIN.toml").exists() {
            return Some(dir.to_path_buf());
        }
    }
    None
}

fn staging_dir() -> PathBuf {
    let cwd = std::env::current_dir().unwrap_or_else(|_| PathBuf::from("."));
    let candidate1 = cwd.join("src-plugins");
    if candidate1.exists() {
        return candidate1.join("_shaders");
    }
    if let Some(parent) = cwd.parent() {
        let candidate2 = parent.join("src-plugins");
        if candidate2.exists() {
            return candidate2.join("_shaders");
        }
    }
    cwd.join("src-plugins").join("_shaders")
}

fn ensure_dir(p: &PathBuf) -> bool {
    if let Err(e) = fs::create_dir_all(p) {
        eprintln!(" Failed to create directory {}: {}", p.display(), e);
        return false;
    }
    true
}

/// Ensure parent directory exists for a file path.
/// Creates all missing parent directories recursively.
/// Returns true on success, false on failure (with error printed).
fn ensure_parent_dir(file_path: &PathBuf) -> bool {
    if let Some(parent) = file_path.parent() {
        if !parent.exists() {
            if let Err(e) = fs::create_dir_all(parent) {
                eprintln!(" Failed to create directory {}: {}", parent.display(), e);
                return false;
            }
        }
    }
    true
}

fn find_runtime_c() -> Option<PathBuf> {
    if let Ok(env_path) = std::env::var("KAIN_RUNTIME_C_PATH") {
        let p = PathBuf::from(env_path);
        if p.exists() {
            return Some(p);
        }
    }

    let candidates = [
        PathBuf::from("runtime/kain_runtime.c"),
        PathBuf::from("runtime/KAIN_runtime.c"),
        PathBuf::from("src/runtime/c/KAIN_runtime.c"),
    ];

    for candidate in candidates {
        if candidate.exists() {
            return Some(candidate);
        }
    }

    if let Ok(exe_path) = std::env::current_exe() {
        if let Some(mut dir) = exe_path.parent().map(|p| p.to_path_buf()) {
            loop {
                let candidate = dir.join("runtime").join("kain_runtime.c");
                if candidate.exists() {
                    return Some(candidate);
                }
                if !dir.pop() {
                    break;
                }
            }
        }
    }

    None
}

fn resolve_native_runtime_bundle() -> Result<Option<ResolvedNativeRuntimeBundle>, String> {
    if let Some(manifest_path) = find_native_runtime_manifest() {
        return load_native_runtime_manifest(&manifest_path).map(Some);
    }

    if let Some(runtime_c) = find_runtime_c() {
        return Ok(Some(ResolvedNativeRuntimeBundle {
            name: "kain-runtime-legacy".to_string(),
            sources: vec![runtime_c],
            include_dirs: Vec::new(),
            defines: Vec::new(),
            link_libs: default_native_runtime_link_libs(),
            uses_cpp_runtime: false,
        }));
    }

    Ok(None)
}

fn load_native_runtime_manifest(
    manifest_path: &Path,
) -> Result<ResolvedNativeRuntimeBundle, String> {
    let manifest_source = fs::read_to_string(manifest_path).map_err(|err| {
        format!(
            "unable to read runtime manifest {}: {}",
            manifest_path.display(),
            err
        )
    })?;
    let manifest: NativeRuntimeManifest = toml::from_str(&manifest_source).map_err(|err| {
        format!(
            "unable to parse runtime manifest {}: {}",
            manifest_path.display(),
            err
        )
    })?;
    let selected_sources = current_platform_runtime_sources(&manifest);
    if manifest.sources.is_empty() && selected_sources.is_empty() {
        return Err(format!(
            "runtime manifest {} does not declare any sources",
            manifest_path.display()
        ));
    }

    let manifest_dir = manifest_path.parent().ok_or_else(|| {
        format!(
            "runtime manifest {} has no parent directory",
            manifest_path.display()
        )
    })?;
    let sources = manifest
        .sources
        .iter()
        .chain(selected_sources.iter())
        .map(|path| resolve_runtime_path(manifest_dir, path))
        .collect::<Vec<_>>();
    let include_dirs = manifest
        .include_dirs
        .iter()
        .map(|path| resolve_runtime_path(manifest_dir, path))
        .collect::<Vec<_>>();
    let defines = current_platform_runtime_defines(&manifest);

    for source in &sources {
        if !source.exists() {
            return Err(format!(
                "runtime source {} does not exist",
                source.display()
            ));
        }
    }

    for include_dir in &include_dirs {
        if !include_dir.exists() {
            return Err(format!(
                "runtime include directory {} does not exist",
                include_dir.display()
            ));
        }
    }

    Ok(ResolvedNativeRuntimeBundle {
        name: manifest
            .name
            .clone()
            .filter(|value| !value.trim().is_empty())
            .unwrap_or_else(|| "kain-native-runtime".to_string()),
        sources,
        include_dirs,
        defines,
        link_libs: unique_link_libs({
            let mut libs = default_native_runtime_link_libs();
            libs.extend(platform_link_libs(&manifest.link));
            libs
        }),
        uses_cpp_runtime: manifest
            .sources
            .iter()
            .chain(selected_sources.iter())
            .any(|path| runtime_source_uses_cpp(path)),
    })
}

fn current_platform_runtime_sources(manifest: &NativeRuntimeManifest) -> &[PathBuf] {
    if cfg!(windows) {
        &manifest.windows_sources
    } else if cfg!(target_os = "macos") {
        &manifest.macos_sources
    } else {
        &manifest.linux_sources
    }
}

fn current_platform_runtime_defines(manifest: &NativeRuntimeManifest) -> Vec<String> {
    let mut defines = manifest.defines.clone();
    if cfg!(windows) {
        defines.extend(manifest.windows_defines.iter().cloned());
    } else if cfg!(target_os = "macos") {
        defines.extend(manifest.macos_defines.iter().cloned());
    } else {
        defines.extend(manifest.linux_defines.iter().cloned());
    }
    defines
}

fn compile_native_runtime_bundle(
    bundle: &ResolvedNativeRuntimeBundle,
    clang_cmd: &str,
    exe_path: &Path,
) -> Result<Vec<PathBuf>, String> {
    let output_dir = exe_path.parent().unwrap_or_else(|| Path::new("."));
    let runtime_obj_dir = output_dir
        .join(".kain-runtime")
        .join(sanitize_runtime_name(&bundle.name));
    fs::create_dir_all(&runtime_obj_dir).map_err(|err| {
        format!(
            "unable to create runtime object directory {}: {}",
            runtime_obj_dir.display(),
            err
        )
    })?;

    let object_ext = if cfg!(windows) { "obj" } else { "o" };
    let mut objects = Vec::with_capacity(bundle.sources.len());
    for (index, source) in bundle.sources.iter().enumerate() {
        let object_path = runtime_obj_dir.join(format!(
            "{:02}_{}.{}",
            index,
            sanitize_runtime_name(
                &source
                    .file_stem()
                    .and_then(|stem| stem.to_str())
                    .unwrap_or("runtime")
            ),
            object_ext
        ));

        let mut compile_cmd = std::process::Command::new(clang_cmd);
        compile_cmd
            .arg("-c")
            .arg(source)
            .arg("-o")
            .arg(&object_path)
            .arg("-g");

        if runtime_source_uses_cpp(source) {
            compile_cmd.arg("-std=c++20");
        }

        for include_dir in &bundle.include_dirs {
            compile_cmd.arg("-I").arg(include_dir);
        }
        for define in &bundle.defines {
            compile_cmd.arg(format!("-D{}", define));
        }

        let status = compile_cmd
            .status()
            .map_err(|err| format!("unable to invoke clang for {}: {}", source.display(), err))?;
        if !status.success() {
            return Err(format!(
                "clang returned a non-zero status while compiling {}",
                source.display()
            ));
        }
        objects.push(object_path);
    }

    Ok(objects)
}

fn sanitize_runtime_name(value: &str) -> String {
    let sanitized = value
        .chars()
        .map(|ch| {
            if ch.is_ascii_alphanumeric() {
                ch.to_ascii_lowercase()
            } else {
                '_'
            }
        })
        .collect::<String>();
    let trimmed = sanitized.trim_matches('_');
    if trimmed.is_empty() {
        "runtime".to_string()
    } else {
        trimmed.to_string()
    }
}

fn resolve_runtime_path(root: &Path, value: &Path) -> PathBuf {
    if value.is_absolute() {
        value.to_path_buf()
    } else {
        root.join(value)
    }
}

fn runtime_source_uses_cpp(value: &Path) -> bool {
    matches!(
        value.extension().and_then(|extension| extension.to_str()),
        Some("cc" | "cpp" | "cxx" | "mm")
    )
}

fn unique_link_libs(values: Vec<String>) -> Vec<String> {
    let mut seen = BTreeSet::new();
    let mut ordered = Vec::new();
    for value in values {
        if seen.insert(value.clone()) {
            ordered.push(value);
        }
    }
    ordered
}

fn platform_link_libs(link: &NativeRuntimeLinkManifest) -> Vec<String> {
    if cfg!(windows) {
        link.windows.clone()
    } else if cfg!(target_os = "macos") {
        link.macos.clone()
    } else {
        link.linux.clone()
    }
}

fn default_native_runtime_link_libs() -> Vec<String> {
    if cfg!(windows) {
        vec![
            "legacy_stdio_definitions".to_string(),
            "user32".to_string(),
            "gdi32".to_string(),
            "opengl32".to_string(),
            "ws2_32".to_string(),
        ]
    } else if cfg!(target_os = "linux") {
        vec!["m".to_string()]
    } else {
        Vec::new()
    }
}

fn default_native_runtime_cpp_link_libs() -> Vec<String> {
    if cfg!(windows) {
        Vec::new()
    } else if cfg!(target_os = "macos") {
        vec!["c++".to_string()]
    } else {
        vec!["stdc++".to_string()]
    }
}

fn find_native_runtime_manifest() -> Option<PathBuf> {
    if let Ok(env_path) = std::env::var("KAIN_RUNTIME_MANIFEST_PATH") {
        let path = PathBuf::from(env_path);
        if path.exists() {
            return Some(path);
        }
    }

    let candidate_suffixes = [
        PathBuf::from("runtime/native_runtime.toml"),
        PathBuf::from("runtime/native/runtime.toml"),
    ];

    for root in runtime_search_roots() {
        for suffix in &candidate_suffixes {
            let candidate = root.join(suffix);
            if candidate.exists() {
                return Some(candidate);
            }
        }
    }

    None
}

fn runtime_search_roots() -> Vec<PathBuf> {
    let mut roots = Vec::new();
    if let Ok(cwd) = std::env::current_dir() {
        roots.push(cwd);
    }
    if let Ok(exe_path) = std::env::current_exe() {
        if let Some(mut dir) = exe_path.parent().map(|path| path.to_path_buf()) {
            loop {
                roots.push(dir.clone());
                if !dir.pop() {
                    break;
                }
            }
        }
    }
    roots
}

#[cfg(test)]
mod tests {
    use super::{
        default_native_runtime_link_libs, load_native_runtime_manifest,
        platform_link_libs, runtime_source_uses_cpp, sanitize_runtime_name, unique_link_libs,
        NativeRuntimeLinkManifest,
    };
    use std::{fs, path::Path};

    #[test]
    fn sanitize_runtime_name_keeps_object_filenames_stable() {
        assert_eq!(sanitize_runtime_name("Kain Runtime"), "kain_runtime");
        assert_eq!(sanitize_runtime_name("###"), "runtime");
    }

    #[test]
    fn runtime_manifest_resolves_relative_paths() {
        let temp_dir = tempfile::tempdir().expect("temp dir");
        let manifest_dir = temp_dir.path().join("runtime");
        let source_dir = manifest_dir.join("src");
        let include_dir = manifest_dir.join("include");
        fs::create_dir_all(&source_dir).expect("source dir");
        fs::create_dir_all(&include_dir).expect("include dir");
        fs::write(
            source_dir.join("kain_runtime.c"),
            "int main(void) { return 0; }\n",
        )
        .expect("source file");
        fs::write(
            manifest_dir.join("native_runtime.toml"),
            r#"
name = "test-runtime"
sources = ["src/kain_runtime.c"]
include_dirs = ["include"]
defines = ["KAIN_TEST=1"]
windows_defines = ["KAIN_WINDOWS=1"]
linux_defines = ["KAIN_LINUX=1"]
macos_defines = ["KAIN_MACOS=1"]

[link]
windows = ["user32", "gdi32"]
linux = ["m"]
macos = ["Cocoa"]
"#,
        )
        .expect("manifest file");

        let resolved = load_native_runtime_manifest(&manifest_dir.join("native_runtime.toml"))
            .expect("resolved manifest");

        assert_eq!(resolved.name, "test-runtime");
        assert_eq!(resolved.sources.len(), 1);
        assert!(resolved.sources[0].is_absolute());
        assert!(resolved.include_dirs[0].is_absolute());
        assert!(!resolved.uses_cpp_runtime);
        if cfg!(windows) {
            assert_eq!(
                resolved.defines,
                vec!["KAIN_TEST=1".to_string(), "KAIN_WINDOWS=1".to_string()]
            );
        } else if cfg!(target_os = "macos") {
            assert_eq!(
                resolved.defines,
                vec!["KAIN_TEST=1".to_string(), "KAIN_MACOS=1".to_string()]
            );
        } else {
            assert_eq!(
                resolved.defines,
                vec!["KAIN_TEST=1".to_string(), "KAIN_LINUX=1".to_string()]
            );
        }
        assert_eq!(
            resolved.link_libs,
            unique_link_libs({
                let mut libs = default_native_runtime_link_libs();
                libs.extend(platform_link_libs(&NativeRuntimeLinkManifest {
                    windows: vec!["user32".to_string(), "gdi32".to_string()],
                    linux: vec!["m".to_string()],
                    macos: vec!["Cocoa".to_string()],
                }));
                libs
            })
        );
    }

    #[test]
    fn runtime_source_cpp_detection_matches_known_extensions() {
        assert!(runtime_source_uses_cpp(Path::new("renderer.cpp")));
        assert!(runtime_source_uses_cpp(Path::new("renderer.cxx")));
        assert!(!runtime_source_uses_cpp(Path::new("renderer.c")));
    }
}

fn find_bundled_clang() -> Option<String> {
    if let Ok(env_path) = std::env::var("KAIN_CLANG_PATH") {
        let path = PathBuf::from(env_path);
        if path.exists() {
            return Some(path.to_string_lossy().into_owned());
        }
    }

    let candidate_suffixes = [
        PathBuf::from("toolchain/llvm/bin/clang.exe"),
        PathBuf::from("toolchain/llvm/bin/clang"),
        PathBuf::from("third_party/llvm/bin/clang.exe"),
        PathBuf::from("third_party/llvm/bin/clang"),
        PathBuf::from("llvm/bin/clang.exe"),
        PathBuf::from("llvm/bin/clang"),
    ];

    let mut search_roots = Vec::new();
    if let Ok(cwd) = std::env::current_dir() {
        search_roots.push(cwd);
    }
    if let Ok(exe_path) = std::env::current_exe() {
        if let Some(dir) = exe_path.parent() {
            let mut cursor = dir.to_path_buf();
            loop {
                search_roots.push(cursor.clone());
                if !cursor.pop() {
                    break;
                }
            }
        }
    }

    for root in search_roots {
        for suffix in &candidate_suffixes {
            let candidate = root.join(suffix);
            if candidate.exists() {
                return Some(candidate.to_string_lossy().into_owned());
            }
        }
    }

    None
}

fn find_binary(name: &str, fallback: Option<&str>) -> Option<PathBuf> {
    if std::process::Command::new(name)
        .arg("--version")
        .output()
        .is_ok()
    {
        return Some(PathBuf::from(name));
    }
    if let Some(f) = fallback {
        let pb = PathBuf::from(f);
        if pb.exists() {
            return Some(pb);
        }
    }
    None
}

fn derive_shader_paths(input: &PathBuf) -> (PathBuf, PathBuf, PathBuf) {
    let stage = staging_dir();
    let stem = input
        .file_stem()
        .and_then(|s| s.to_str())
        .unwrap_or("shader");
    let spv = stage.join(format!("{}.spv", stem));
    let hlsl = stage.join(format!("{}.hlsl", stem));
    let usf = stage.join(format!("{}.usf", stem));
    (spv, hlsl, usf)
}

fn resolve_plugin_dir(plugin: &str, base_opt: &Option<PathBuf>) -> PathBuf {
    if let Some(base) = base_opt {
        return base.join(plugin).join("Shaders");
    }
    let cwd = std::env::current_dir().unwrap_or_else(|_| PathBuf::from("."));
    let candidate1 = cwd.join("src-plugins");
    if candidate1.exists() {
        return candidate1.join(plugin).join("Shaders");
    }
    if let Some(parent) = cwd.parent() {
        let candidate2 = parent.join("src-plugins");
        if candidate2.exists() {
            return candidate2.join(plugin).join("Shaders");
        }
    }
    cwd.join("src-plugins").join(plugin).join("Shaders")
}

fn run_ue5_shader_pipeline(input: &PathBuf, args: &Args) -> bool {
    let (spv_path, hlsl_path, usf_path) = derive_shader_paths(input);
    let stage_dir = staging_dir();
    if !ensure_dir(&stage_dir) {
        return false;
    }

    let source = match fs::read_to_string(input) {
        Ok(s) => s,
        Err(e) => {
            eprintln!(" Failed to read {}: {}", input.display(), e);
            return false;
        }
    };

    if args.verbose {
        println!(" Compiling: {}", input.display());
    }

    let compiled_spv = match cli::compile_spirv_binary(&source) {
        Ok(bytes) => bytes,
        Err(e) => {
            let filename = input
                .file_name()
                .and_then(|s| s.to_str())
                .unwrap_or("input.kn");
            let diag = kain_core::diagnostics::Diagnostics::new(&source, filename);
            eprint!("{}", diag.format_error(&e));
            return false;
        }
    };

    if args.dry_run {
        println!("→ Write SPIR-V {}", spv_path.display());
    } else if let Err(e) = fs::write(&spv_path, &compiled_spv) {
        eprintln!(" Failed to write {}: {}", spv_path.display(), e);
        return false;
    } else {
        if args.verbose {
            println!(" {}", spv_path.display());
        }
    }

    if let Some(val_bin) = find_binary("spirv-val", None) {
        if args.verbose {
            println!(" Validating SPIR-V");
        }
        if !args.dry_run {
            let status = std::process::Command::new(val_bin).arg(&spv_path).status();
            if let Ok(s) = status {
                if !s.success() {
                    eprintln!(" SPIR-V validation failed");
                    return false;
                }
            }
        }
    }

    let naga_bin = match find_binary("naga", None) {
        Some(p) => p,
        None => {
            eprintln!(" 'naga' not found. Install with: cargo install naga-cli");
            return false;
        }
    };

    if args.verbose {
        println!(" Transpiling to HLSL");
    }
    if args.dry_run {
        println!("→ Run naga {} {}", spv_path.display(), hlsl_path.display());
    } else {
        let status = std::process::Command::new(naga_bin)
            .arg(&spv_path)
            .arg(&hlsl_path)
            .status();
        match status {
            Ok(s) if s.success() => {
                if args.verbose {
                    println!(" {}", hlsl_path.display());
                }
            }
            _ => {
                eprintln!(" Naga transpilation failed");
                return false;
            }
        }
    }

    if args.dry_run {
        println!("→ Write USF {}", usf_path.display());
    } else {
        let stem = input
            .file_stem()
            .and_then(|s| s.to_str())
            .unwrap_or("shader");
        let content = format!("#include \"{}.hlsl\"\n", stem);
        if let Err(e) = fs::write(&usf_path, content) {
            eprintln!(" Failed to write {}: {}", usf_path.display(), e);
            return false;
        }
    }

    if let Some(plugin) = &args.plugin {
        let target_dir = resolve_plugin_dir(plugin, &args.plugins_dir);
        if args.dry_run {
            println!("→ Copy to {}", target_dir.display());
        } else {
            if !ensure_dir(&target_dir.clone()) {
                return false;
            }
            let hlsl_target = target_dir.join(hlsl_path.file_name().unwrap());
            let usf_target = target_dir.join(usf_path.file_name().unwrap());
            if let Err(e) = fs::copy(&hlsl_path, &hlsl_target) {
                eprintln!(" Copy failed: {}", e);
                return false;
            }
            if let Err(e) = fs::copy(&usf_path, &usf_target) {
                eprintln!(" Copy failed: {}", e);
                return false;
            }
            println!(" {}", target_dir.display());
        }
    } else {
        if args.verbose {
            println!(" Staged in {}", stage_dir.display());
        }
    }

    true
}
