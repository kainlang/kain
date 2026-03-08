//! KAIN Compiler CLI

use clap::Parser as ClapParser;
use std::path::PathBuf;
use std::fs;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::time::Duration;
use cli::{
    BUILD_GIT_COMMIT_COUNT, BUILD_GIT_DIRTY, BUILD_GIT_SHA, BUILD_HOST_TRIPLE, BUILD_NUMBER,
    BUILD_PROFILE, BUILD_TARGET_TRIPLE, BUILD_UNIX_TIME,
    compile, parse_compile_target, supported_targets_csv, target_extension, CompileTarget,
    LANGUAGE_NAME, VERSION,
};
use cli::packager;
use cli::lsp;
use cli::omni;
use cli::import_asm;
use cli::import_c;
use cli::import_rust;
use cli::import_typescript;
use cli::rust_build;

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
    Doctor,

    /// Build mixed-language omni manifests through the dedicated orchestration layer
    Omni {
        #[command(subcommand)]
        command: omni::OmniCommand,
    },

    /// Build project or file. Without input, reads KAIN.toml for multi-target build.
    Build {
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
    Run {
        input: PathBuf,
    },

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

fn run_compile(input: &PathBuf, target: CompileTarget, output: Option<&PathBuf>, _emit_ast: bool, _emit_typed: bool, verbose: bool, _analyze: bool, plugin_name: Option<&str>) -> bool {
    // Read source
    let source = match fs::read_to_string(input) {
        Ok(s) => s,
        Err(e) => {
            eprintln!(" Failed to read {}: {}", input.display(), e);
            return false;
        }
    };

    if verbose {
        println!(" Compiling: {}", input.display());
        println!(" Source: {} bytes, {} lines", source.len(), source.lines().count());
    }

    // Compile SPIR-V as binary bytes (not the string summary used by compile()).
    if target == CompileTarget::Spirv {
        let output_path = output
            .cloned()
            .unwrap_or_else(|| input.with_extension(target_extension(target)));
        match cli::compile_spirv_binary(&source) {
            Ok(spv_bytes) => {
                if !ensure_parent_dir(&output_path) {
                    return false;
                }
                if let Err(e) = fs::write(&output_path, &spv_bytes) {
                    eprintln!(" Failed to write output: {}", e);
                    return false;
                }
                println!(" Compiled to: {} ({} bytes)", output_path.display(), spv_bytes.len());
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
                    } else {
                        input.with_extension("ll")
                    }
                } else if target == CompileTarget::Usf {
                    // For USF, always ensure .usf extension
                    if let Some(out) = output {
                        let mut p = out.clone();
                        p.set_extension("usf");
                        p
                    } else {
                        input.with_extension("usf")
                    }
                } else {
                    output.cloned().unwrap_or_else(|| {
                        input.with_extension(default_ext)
                    })
                };
                
                
                if !ensure_parent_dir(&output_path) {
                    return false;
                }
                if let Err(e) = fs::write(&output_path, &compiled_output) {
                    eprintln!(" Failed to write output: {}", e);
                    return false;
                }
                
                println!(" Compiled to: {} ({} bytes)", output_path.display(), compiled_output.len());

                // Generate C++ reflection header for USF shaders (GODMODE Phase 3)
                if target == CompileTarget::Usf {
                    // Extract shader name from AST instead of filename
                    let shader_name = {
                        // Parse the source to get the actual shader name
                        match kain_core::Lexer::new(&source).tokenize() {
                            Ok(tokens) => {
                                let span_mapper = kain_core::diagnostics::SpanMapper::new(&source);
                                match kain_core::Parser::new(&tokens, &span_mapper, input.to_str().unwrap_or("<unknown>")).parse() {
                                    Ok(ast) => {
                                        // Find the first shader in the AST
                                        ast.items.iter()
                                            .find_map(|item| {
                                                if let kain_core::ast::Item::Shader(shader) = item {
                                                    Some(shader.name.clone())
                                                } else {
                                                    None
                                                }
                                            })
                                            .unwrap_or_else(|| {
                                                input.file_stem()
                                                    .and_then(|s| s.to_str())
                                                    .unwrap_or("Shader")
                                                    .to_string()
                                            })
                                    }
                                    Err(_) => {
                                        input.file_stem()
                                            .and_then(|s| s.to_str())
                                            .unwrap_or("Shader")
                                            .to_string()
                                    }
                                }
                            }
                            Err(_) => {
                                input.file_stem()
                                    .and_then(|s| s.to_str())
                                    .unwrap_or("Shader")
                                    .to_string()
                            }
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
                        },
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
                                eprintln!(" Warning: Failed to create directory for implementation");
                            } else if let Err(e) = fs::write(&cpp_path, cpp_code.as_bytes()) {
                                eprintln!(" Warning: Failed to write implementation: {}", e);
                            } else {
                                println!(" Generated implementation: {}", cpp_path.display());
                            }
                        },
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
                    let output_name = output_path.file_stem()
                        .and_then(|s| s.to_str());
                    
                    match cli::compile_ue5(&source, output_name, None) {
                        Ok(ue5_output) => {
                            // Write header file
                            let header_path = output_path.with_extension("h");
                            let header_path = output_path.with_extension("h");
                            if !ensure_parent_dir(&header_path) {
                                eprintln!(" Warning: Failed to create directory for header");
                            } else if let Err(e) = fs::write(&header_path, ue5_output.header.as_bytes()) {
                                eprintln!(" Warning: Failed to write header: {}", e);
                            } else {
                                println!(" Generated header: {} ({} bytes)", header_path.display(), ue5_output.header.len());
                            }
                            
                            // Write source file
                            let source_path = output_path.with_extension("cpp");
                            let source_path = output_path.with_extension("cpp");
                            if !ensure_parent_dir(&source_path) {
                                eprintln!(" Warning: Failed to create directory for source");
                            } else if let Err(e) = fs::write(&source_path, ue5_output.source.as_bytes()) {
                                eprintln!(" Warning: Failed to write source: {}", e);
                            } else {
                                println!(" Generated source: {} ({} bytes)", source_path.display(), ue5_output.source.len());
                            }
                            
                            // Write shader files (USF + shader headers + shader cpp)
                            if !ue5_output.shader_files.is_empty() {
                                println!(" Generated {} shader files:", ue5_output.shader_files.len());
                                for (filename, content) in &ue5_output.shader_files {
                                    let shader_path = output_path.with_file_name(filename);
                                    let shader_path = output_path.with_file_name(filename);
                                    if !ensure_parent_dir(&shader_path) {
                                        eprintln!("   Warning: Failed to create directory for {}", filename);
                                    } else if let Err(e) = fs::write(&shader_path, content.as_bytes()) {
                                        eprintln!("   Warning: Failed to write {}: {}", filename, e);
                                    } else {
                                        println!("   - {} ({} bytes)", shader_path.display(), content.len());
                                    }
                                }
                            }
                        },
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
                            } else if let Err(e) = fs::write(&header_path, editor_output.header.as_bytes()) {
                                eprintln!(" Warning: Failed to write header: {}", e);
                            } else {
                                println!(" Generated header: {} ({} bytes)", header_path.display(), editor_output.header.len());
                            }
                            
                            // Write source file
                            let source_path = output_path.with_extension("cpp");
                            let source_path = output_path.with_extension("cpp");
                            if !ensure_parent_dir(&source_path) {
                                eprintln!(" Warning: Failed to create directory for source");
                            } else if let Err(e) = fs::write(&source_path, editor_output.source.as_bytes()) {
                                eprintln!(" Warning: Failed to write source: {}", e);
                            } else {
                                println!(" Generated source: {} ({} bytes)", source_path.display(), editor_output.source.len());
                            }
                        },
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
                    } else if cfg!(windows) {
                        input.with_extension("exe")
                    } else {
                        input.with_extension("")
                    };

                    println!(" Linking executable...");
                    
                    // Find clang: bundled toolchain > PATH > system install
                    let clang_cmd = find_bundled_clang()
                        .or_else(|| {
                            // Try PATH
                            if std::process::Command::new("clang").arg("--version").output().is_ok() {
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

                    if let Some(runtime_c) = find_runtime_c() {
                        let mut runtime_o = runtime_c.clone();
                        runtime_o.set_extension(if cfg!(windows) { "obj" } else { "o" });

                        let status = std::process::Command::new(&clang_cmd)
                            .arg("-c")
                            .arg(&runtime_c)
                            .arg("-o")
                            .arg(&runtime_o)
                            .status();

                        if let Ok(s) = status {
                            if s.success() {
                                cmd.arg(&runtime_o);
                            } else {
                                eprintln!(" Failed to compile runtime library.");
                                return false;
                            }
                        } else {
                            eprintln!(" Failed to invoke clang for runtime library compilation.");
                            return false;
                        }
                    }

                    cmd.arg(&output_path)
                        .arg("-o")
                        .arg(&exe_path)
                        .arg("-Wno-override-module")
                        .arg("-g"); // Debug info

                    if cfg!(windows) {
                        cmd.arg("-llegacy_stdio_definitions");
                        cmd.arg("-luser32");
                        cmd.arg("-lgdi32");
                    }

                    let status = cmd.status();

                    match status {
                        Ok(s) if s.success() => {
                            println!(" Generated executable: {}", exe_path.display());
                        },
                        Ok(_) => {
                            eprintln!(" Linking failed."); 
                            return false;
                        },
                        Err(_) => {
                            eprintln!(" 'clang' not found in PATH or standard locations.");
                            eprintln!("   To generate an executable, install LLVM and run:");
                            eprintln!("   clang {} -o {}", output_path.display(), exe_path.display());
                            return false;
                        }
                    }
                }
            }
            true
        }
        Err(e) => {
            // Use pretty error formatting
            let filename = input.file_name()
                .and_then(|s| s.to_str())
                .unwrap_or("input.kn");
            let diag = kain_core::diagnostics::Diagnostics::new(&source, filename);
            eprint!("{}", diag.format_error(&e));
            false
        }
    }
}

fn watch_mode(input: PathBuf, target: CompileTarget, output: Option<PathBuf>, emit_ast: bool, emit_typed: bool, verbose: bool, analyze: bool, plugin_name: Option<String>) {
    use notify::{Watcher, RecursiveMode, Event};
    use std::sync::mpsc::channel;
    
    let running = Arc::new(AtomicBool::new(true));
    let r = running.clone();
    
    ctrlc::set_handler(move || {
        println!("\n Stopping watch mode...");
        r.store(false, Ordering::SeqCst);
    }).expect("Error setting Ctrl-C handler");
    
    println!(" Watching {} for changes... (Ctrl+C to stop)", input.display());
    println!("");
    
    // Initial compile
    run_compile(&input, target, output.as_ref(), emit_ast, emit_typed, verbose, analyze, plugin_name.as_deref());
    println!("");
    
    let (tx, rx) = channel();
    
    let mut watcher = notify::recommended_watcher(move |res: Result<Event, notify::Error>| {
        if let Ok(event) = res {
            if event.kind.is_modify() {
                let _ = tx.send(());
            }
        }
    }).expect("Failed to create watcher");
    
    watcher.watch(&input, RecursiveMode::NonRecursive).expect("Failed to watch file");
    
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
                run_compile(&input, target, output.as_ref(), emit_ast, emit_typed, verbose, analyze, plugin_name.as_deref());
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

    let handler = builder.spawn(|| {
        let args = Args::parse();

        println!(" {} Compiler v{} (build {})", LANGUAGE_NAME, VERSION, BUILD_NUMBER);

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
            Some(Commands::Doctor) => {
                print_doctor();
            }
            Some(Commands::Omni { command }) => {
                if let Err(e) = omni::run(command) {
                    eprintln!(" Omni command failed: {}", e);
                    std::process::exit(1);
                }
            }
            Some(Commands::Build {
                input,
                output,
                target,
                targets,
                ue5,
                r#rust,
                embed,
            }) => {
                if ue5 {
                    // UE5 plugin build
                    if let Err(e) = packager::build_ue5_plugin_with_options(embed) {
                        // Error already contains formatted details with file:line:col
                        eprintln!("{}", e);
                        std::process::exit(1);
                    }
                } else if r#rust {
                    match input {
                        Some(file) => {
                            match rust_build::run_rust_build_pipeline(&file, output.as_ref(), None) {
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
                            let target_alias = target
                                .as_deref()
                                .unwrap_or(args.target.as_str());
                            let Some(resolved_target) = parse_compile_target(target_alias) else {
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
                run_compile(&input, CompileTarget::Interpret, None, args.emit_ast, args.emit_typed, args.verbose, args.analyze, args.plugin.as_deref());
            }
            Some(Commands::GpuArtifacts { input, output }) => {
                let config = packager::RustBuildConfig {
                    output: None,
                    artifacts: vec![
                        packager::RustBuildArtifact::ShaderHost,
                        packager::RustBuildArtifact::ShaderReflection,
                        packager::RustBuildArtifact::Spirv,
                    ],
                };

                match rust_build::run_rust_build_pipeline(&input, output.as_ref(), Some(&config)) {
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
            Some(Commands::Inject { inputs, plugin_dir, plugin, force, dry_run, ue5 }) => {
                if ue5 {
                    if let Err(e) = packager::inject_into_plugin(&inputs, plugin_dir.as_ref(), plugin.as_deref(), force, dry_run) {
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
                        let Some(target) = parse_compile_target(&args.target) else {
                            eprintln!(
                                " Unknown target: {}. Use: {}",
                                args.target,
                                supported_targets_csv()
                            );
                            std::process::exit(1);
                        };

                        if args.watch {
                            watch_mode(input.clone(), target, args.output.clone(), args.emit_ast, args.emit_typed, args.verbose, args.analyze, args.plugin.clone());
                        } else {
                            if !run_compile(&input, target, args.output.as_ref(), args.emit_ast, args.emit_typed, args.verbose, args.analyze, args.plugin.as_deref()) {
                                std::process::exit(1);
                            }
                        }
                    }
                } else {
                    eprintln!(" No input file provided. Use --help for usage.");
                }
            }
        }
    }).unwrap();

    handler.join().unwrap();
}

fn print_doctor() {
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

    match std::env::current_exe() {
        Ok(path) => println!(" Binary Path: {}", path.display()),
        Err(err) => println!(" Binary Path: <unknown> ({})", err),
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
    if cfg!(feature = "sys") {
        match find_bundled_clang() {
            Some(path) => println!(" Resolved LLVM Clang: {}", path),
            None => println!(" Resolved LLVM Clang: <not found in bundled locations>"),
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
    if std::process::Command::new(name).arg("--version").output().is_ok() {
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
    let stem = input.file_stem().and_then(|s| s.to_str()).unwrap_or("shader");
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
            let filename = input.file_name().and_then(|s| s.to_str()).unwrap_or("input.kn");
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
            let status = std::process::Command::new(val_bin)
                .arg(&spv_path)
                .status();
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
        let stem = input.file_stem().and_then(|s| s.to_str()).unwrap_or("shader");
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
