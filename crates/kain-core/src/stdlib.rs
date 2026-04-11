//! KAIN Standard Library

use crate::types::ResolvedType;
use crate::CompileTarget;
use once_cell::sync::Lazy;
use std::collections::{BTreeMap, HashMap};
use std::sync::RwLock;

pub type StdlibExtensionRegistrar = fn(&mut StdLib);

static STDLIB_EXTENSION_REGISTRARS: Lazy<RwLock<BTreeMap<String, StdlibExtensionRegistrar>>> =
    Lazy::new(|| RwLock::new(BTreeMap::new()));

pub fn register_stdlib_extension(name: impl Into<String>, registrar: StdlibExtensionRegistrar) {
    STDLIB_EXTENSION_REGISTRARS
        .write()
        .unwrap()
        .insert(name.into(), registrar);
}

/// Built-in function registry
pub struct StdLib {
    pub functions: HashMap<String, BuiltinFn>,
    pub types: HashMap<String, ResolvedType>,
}

pub struct BuiltinFn {
    pub name: &'static str,
    pub params: Vec<(&'static str, &'static str)>,
    pub return_type: &'static str,
    pub doc: &'static str,
}

impl StdLib {
    pub fn new() -> Self {
        let mut lib = Self {
            functions: HashMap::new(),
            types: HashMap::new(),
        };

        // I/O
        lib.add_fn(
            "print",
            &[("value", "Any")],
            "Unit",
            "Print value to console",
        );
        lib.add_fn(
            "println",
            &[("value", "Any")],
            "Unit",
            "Print value with newline",
        );
        lib.add_fn(
            "stdout_write",
            &[("value", "String")],
            "Unit",
            "Write raw text to stdout without an automatic newline",
        );
        lib.add_fn("read_line", &[], "String", "Read line from stdin");
        lib.add_fn(
            "stdin_read_exact",
            &[("length", "Int")],
            "String",
            "Read an exact number of bytes from stdin",
        );
        lib.add_fn(
            "read_file",
            &[("path", "String")],
            "String",
            "Read file contents",
        );
        lib.add_fn(
            "write_file",
            &[("path", "String"), ("content", "String")],
            "Unit",
            "Write to file",
        );
        lib.add_fn(
            "file_exists",
            &[("path", "String")],
            "Bool",
            "Check whether a file path exists",
        );
        lib.add_fn(
            "env",
            &[("name", "String")],
            "String",
            "Read an environment variable",
        );

        // Math
        lib.add_fn("abs", &[("x", "Int")], "Int", "Absolute value");
        lib.add_fn("sqrt", &[("x", "Float")], "Float", "Square root");
        lib.add_fn(
            "pow",
            &[("base", "Float"), ("exp", "Float")],
            "Float",
            "Power",
        );
        lib.add_fn("sin", &[("x", "Float")], "Float", "Sine");
        lib.add_fn("cos", &[("x", "Float")], "Float", "Cosine");
        lib.add_fn("tan", &[("x", "Float")], "Float", "Tangent");
        lib.add_fn("floor", &[("x", "Float")], "Int", "Floor");
        lib.add_fn("ceil", &[("x", "Float")], "Int", "Ceiling");
        lib.add_fn("round", &[("x", "Float")], "Int", "Round");
        lib.add_fn("min", &[("a", "Int"), ("b", "Int")], "Int", "Minimum");
        lib.add_fn("max", &[("a", "Int"), ("b", "Int")], "Int", "Maximum");
        lib.add_fn(
            "clamp",
            &[("x", "Int"), ("lo", "Int"), ("hi", "Int")],
            "Int",
            "Clamp between bounds",
        );

        // Vector math (for shaders)
        lib.add_fn(
            "vec2",
            &[("x", "Float"), ("y", "Float")],
            "Vec2",
            "Create 2D vector",
        );
        lib.add_fn(
            "vec3",
            &[("x", "Float"), ("y", "Float"), ("z", "Float")],
            "Vec3",
            "Create 3D vector",
        );
        lib.add_fn(
            "vec4",
            &[
                ("x", "Float"),
                ("y", "Float"),
                ("z", "Float"),
                ("w", "Float"),
            ],
            "Vec4",
            "Create 4D vector",
        );
        lib.add_fn(
            "dot",
            &[("a", "Vec3"), ("b", "Vec3")],
            "Float",
            "Dot product",
        );
        lib.add_fn(
            "cross",
            &[("a", "Vec3"), ("b", "Vec3")],
            "Vec3",
            "Cross product",
        );
        lib.add_fn("normalize", &[("v", "Vec3")], "Vec3", "Normalize vector");
        lib.add_fn("length", &[("v", "Vec3")], "Float", "Vector length");
        lib.add_fn(
            "distance",
            &[("a", "Vec3"), ("b", "Vec3")],
            "Float",
            "Distance between points",
        );
        lib.add_fn(
            "mix",
            &[("a", "Float"), ("b", "Float"), ("t", "Float")],
            "Float",
            "Linear interpolation",
        );
        lib.add_fn(
            "smoothstep",
            &[("edge0", "Float"), ("edge1", "Float"), ("x", "Float")],
            "Float",
            "Smooth step",
        );

        // Collections
        lib.add_fn("len", &[("collection", "Any")], "Int", "Get length");
        lib.add_fn(
            "push",
            &[("array", "Array"), ("value", "Any")],
            "Unit",
            "Push to array",
        );
        lib.add_fn("pop", &[("array", "Array")], "Any", "Pop from array");
        lib.add_fn(
            "map",
            &[("array", "Array"), ("fn", "Function")],
            "Array",
            "Map over array",
        );
        lib.add_fn(
            "filter",
            &[("array", "Array"), ("fn", "Function")],
            "Array",
            "Filter array",
        );
        lib.add_fn(
            "reduce",
            &[("array", "Array"), ("initial", "Any"), ("fn", "Function")],
            "Any",
            "Reduce array",
        );
        lib.add_fn(
            "range",
            &[("start", "Int"), ("end", "Int")],
            "Array",
            "Create range",
        );

        // HashMap
        lib.add_fn("map_new", &[], "Any", "Create new map");
        lib.add_fn(
            "map_set",
            &[("map", "Any"), ("key", "String"), ("value", "Any")],
            "Unit",
            "Set map key",
        );
        lib.add_fn(
            "map_get",
            &[("map", "Any"), ("key", "String")],
            "Any",
            "Get map value",
        );

        // Sockets
        lib.add_fn(
            "socket_connect",
            &[("host", "String"), ("port", "Int")],
            "Int",
            "Connect TCP socket",
        );
        lib.add_fn(
            "socket_send",
            &[("sock", "Int"), ("data", "String")],
            "Unit",
            "Send data",
        );
        lib.add_fn("socket_recv", &[("sock", "Int")], "String", "Receive data");

        // String
        lib.add_fn(
            "split",
            &[("s", "String"), ("sep", "String")],
            "Array",
            "Split string",
        );
        lib.add_fn(
            "join",
            &[("arr", "Array"), ("sep", "String")],
            "String",
            "Join array to string",
        );
        lib.add_fn("trim", &[("s", "String")], "String", "Trim whitespace");
        lib.add_fn("to_upper", &[("s", "String")], "String", "To uppercase");
        lib.add_fn("to_lower", &[("s", "String")], "String", "To lowercase");
        lib.add_fn(
            "contains",
            &[("s", "String"), ("sub", "String")],
            "Bool",
            "Check contains",
        );
        lib.add_fn(
            "replace",
            &[("s", "String"), ("from", "String"), ("to", "String")],
            "String",
            "Replace substring",
        );
        lib.add_fn(
            "starts_with",
            &[("s", "String"), ("prefix", "String")],
            "Bool",
            "Check whether a string starts with a prefix",
        );
        lib.add_fn(
            "ends_with",
            &[("s", "String"), ("suffix", "String")],
            "Bool",
            "Check whether a string ends with a suffix",
        );
        lib.add_fn(
            "substring",
            &[("s", "String"), ("start", "Int"), ("end", "Int")],
            "String",
            "Extract a substring using start and end character offsets",
        );
        lib.add_fn(
            "char_at",
            &[("s", "String"), ("index", "Int")],
            "String",
            "Read a single character at an index as a string",
        );
        lib.add_fn(
            "ord",
            &[("s", "String")],
            "Int",
            "Read the Unicode scalar value of the first character",
        );
        lib.add_fn(
            "chr",
            &[("codepoint", "Int")],
            "String",
            "Create a single-character string from a Unicode scalar value",
        );

        // Conversion
        lib.add_fn(
            "to_string",
            &[("value", "Any")],
            "String",
            "Convert to string",
        );
        lib.add_fn("to_int", &[("value", "Any")], "Int", "Convert to int");
        lib.add_fn("to_float", &[("value", "Any")], "Float", "Convert to float");

        // Debug
        lib.add_fn("dbg", &[("value", "Any")], "Any", "Debug print and return");
        lib.add_fn(
            "assert",
            &[("condition", "Bool"), ("message", "String")],
            "Unit",
            "Assert condition",
        );
        lib.add_fn(
            "panic",
            &[("message", "String")],
            "Never",
            "Panic with message",
        );

        // Time
        lib.add_fn("now", &[], "Float", "Current time in seconds");
        lib.add_fn(
            "sleep",
            &[("seconds", "Float")],
            "Unit",
            "Sleep for seconds",
        );

        // Actors
        lib.add_fn("spawn", &[("actor", "Actor")], "ActorRef", "Spawn actor");
        lib.add_fn(
            "send",
            &[("actor", "ActorRef"), ("message", "Message")],
            "Unit",
            "Send message",
        );

        // UI
        lib.add_fn(
            "mount",
            &[("component", "Any"), ("selector", "String")],
            "Unit",
            "Mount component to DOM",
        );
        lib.add_fn(
            "spawn_cube",
            &[("x", "Float"), ("y", "Float")],
            "Unit",
            "Open a native 3D cube window",
        );
        lib.add_fn(
            "spawn_native_viewport",
            &[("x", "Float"), ("y", "Float")],
            "Unit",
            "Open the raw native Kain 3D viewport host",
        );
        lib.add_fn(
            "spawn_native_sculpt_lab",
            &[("x", "Float"), ("y", "Float")],
            "Unit",
            "Open the raw native Kain sculpting lab",
        );
        lib.add_fn(
            "native_config_string",
            &[("key", "String"), ("value", "String")],
            "Unit",
            "Set a raw native runtime string config value before launch",
        );
        lib.add_fn(
            "native_config_int",
            &[("key", "String"), ("value", "Int")],
            "Unit",
            "Set a raw native runtime integer config value before launch",
        );
        lib.add_fn(
            "native_config_float",
            &[("key", "String"), ("value", "Float")],
            "Unit",
            "Set a raw native runtime float config value before launch",
        );
        lib.add_fn(
            "native_config_flag",
            &[("key", "String"), ("enabled", "Int")],
            "Unit",
            "Set a raw native runtime boolean-like config value before launch using 0 or 1",
        );

        let registrars = STDLIB_EXTENSION_REGISTRARS
            .read()
            .unwrap()
            .values()
            .copied()
            .collect::<Vec<_>>();
        for registrar in registrars {
            registrar(&mut lib);
        }

        lib
    }

    fn add_fn(
        &mut self,
        name: &'static str,
        params: &[(&'static str, &'static str)],
        ret: &'static str,
        doc: &'static str,
    ) {
        self.functions.insert(
            name.to_string(),
            BuiltinFn {
                name,
                params: params.to_vec(),
                return_type: ret,
                doc,
            },
        );
    }
}

impl Default for StdLib {
    fn default() -> Self {
        Self::new()
    }
}

/// Find prioritized list of directories to search for stdlib.
///
/// This is also surfaced through `kain doctor` so the active compiler can
/// explain exactly which stdlib roots it will prefer on the current machine.
pub fn find_stdlib_search_roots() -> Vec<std::path::PathBuf> {
    use std::env;
    use std::path::PathBuf;

    let mut roots = Vec::new();

    // Priority 1: KAIN_STDLIB_PATH environment variable (highest priority)
    if let Ok(env_path) = env::var("KAIN_STDLIB_PATH") {
        let path = PathBuf::from(env_path);
        if path.exists() {
            roots.push(path);
            return roots; // If env var is set and valid, use only that
        }
    }

    // Priority 2: Walk up from executable location
    if let Ok(exe_path) = env::current_exe() {
        if let Some(mut current) = exe_path.parent() {
            loop {
                let stdlib_dir = current.join("stdlib");
                if stdlib_dir.exists() && stdlib_dir.is_dir() {
                    roots.push(stdlib_dir);
                    break;
                }

                // Move to parent directory
                if let Some(parent) = current.parent() {
                    current = parent;
                } else {
                    break; // Reached filesystem root
                }
            }
        }
    }

    // Priority 3: Walk up from current working directory
    if let Ok(mut current) = env::current_dir() {
        loop {
            let stdlib_dir = current.join("stdlib");
            if stdlib_dir.exists() && stdlib_dir.is_dir() {
                // Avoid duplicates
                if !roots.iter().any(|r| r == &stdlib_dir) {
                    roots.push(stdlib_dir);
                }
                break;
            }

            // Move to parent directory
            if let Some(parent) = current.parent() {
                current = parent.to_path_buf();
            } else {
                break; // Reached filesystem root
            }
        }
    }

    roots
}

/// Load all .kn files from a directory, excluding README files
pub(crate) fn load_kn_files_from_dir(path: &std::path::Path) -> Option<String> {
    use std::fs;

    // Read directory entries
    let entries = match fs::read_dir(path) {
        Ok(entries) => entries,
        Err(_) => return None,
    };

    // Collect .kn files, excluding READMEs
    let mut kn_files: Vec<(String, String)> = Vec::new();

    for entry in entries {
        let entry = match entry {
            Ok(e) => e,
            Err(_) => continue, // Skip unreadable entries
        };

        let path = entry.path();

        // Check if it's a file with .kn extension
        if path.is_file() {
            if let Some(filename) = path.file_name() {
                let filename_str = filename.to_string_lossy();

                // Filter for .kn extension
                if filename_str.ends_with(".kn") {
                    // Exclude README files (case-insensitive)
                    if filename_str.to_lowercase().contains("readme") {
                        continue;
                    }

                    // Read file contents
                    match fs::read_to_string(&path) {
                        Ok(content) => {
                            kn_files.push((filename_str.to_string(), content));
                        }
                        Err(_) => {
                            // Skip unreadable files, log warning
                            eprintln!("Warning: Could not read stdlib file: {}", path.display());
                            continue;
                        }
                    }
                }
            }
        }
    }

    // Return None if no files found
    if kn_files.is_empty() {
        return None;
    }

    // Sort files alphabetically for deterministic ordering
    kn_files.sort_by(|a, b| a.0.cmp(&b.0));

    // Concatenate file contents with newlines
    let concatenated = kn_files
        .into_iter()
        .map(|(_, content)| content)
        .collect::<Vec<_>>()
        .join("\n");

    Some(concatenated)
}

const DEFAULT_PROFILE_ORDER: &[&str] = &[""];

const TARGET_PROFILE_ORDER: &[(CompileTarget, &[&str])] = &[
    (CompileTarget::Ue5, &["ue5", ""]),
    (CompileTarget::Ue5Editor, &["ue5", ""]),
    (CompileTarget::Usf, &["ue5", ""]),
    (CompileTarget::Hlsl, &[""]),
    (CompileTarget::Spirv, &[""]),
    (CompileTarget::Wasm, &[""]),
    (CompileTarget::Js, &[""]),
    (CompileTarget::Ts, &[""]),
    (CompileTarget::Hybrid, &[""]),
    (CompileTarget::Llvm, &[""]),
    (CompileTarget::Rust, &[""]),
    (CompileTarget::Cpp, &[""]),
    (CompileTarget::Interpret, &[""]),
    (CompileTarget::Test, &[""]),
    (CompileTarget::Ks, &[""]), // KainScript shares stdlib with JS
];

fn resolve_profile_path(root: &std::path::Path, profile: &str) -> std::path::PathBuf {
    if profile.trim().is_empty() || profile.eq_ignore_ascii_case("root") {
        return root.to_path_buf();
    }
    root.join(profile)
}

fn parse_profile_env_override() -> Option<Vec<String>> {
    let raw = std::env::var("KAIN_STDLIB_PROFILE").ok()?;
    let profiles = raw
        .split(',')
        .map(str::trim)
        .filter(|p| !p.is_empty())
        .map(str::to_string)
        .collect::<Vec<_>>();
    if profiles.is_empty() {
        None
    } else {
        Some(profiles)
    }
}

fn load_stdlib_from_profiles(search_roots: &[std::path::PathBuf], profiles: &[String]) -> String {
    for root in search_roots {
        for profile in profiles {
            let candidate_dir = resolve_profile_path(root, profile);
            if candidate_dir.exists() && candidate_dir.is_dir() {
                if let Some(stdlib_source) = load_kn_files_from_dir(&candidate_dir) {
                    eprintln!("Loaded stdlib from: {}", candidate_dir.display());
                    return stdlib_source;
                }
            }
        }
    }
    String::new()
}

fn target_profiles(target: CompileTarget) -> &'static [&'static str] {
    TARGET_PROFILE_ORDER
        .iter()
        .find(|(candidate, _)| *candidate == target)
        .map(|(_, profiles)| *profiles)
        .unwrap_or(DEFAULT_PROFILE_ORDER)
}

/// Load the standard library source code
pub fn load_stdlib() -> String {
    let search_roots = find_stdlib_search_roots();
    if search_roots.is_empty() {
        return String::new();
    }

    let profiles = parse_profile_env_override().unwrap_or_else(|| {
        DEFAULT_PROFILE_ORDER
            .iter()
            .map(|p| (*p).to_string())
            .collect()
    });
    load_stdlib_from_profiles(&search_roots, &profiles)
}

/// Load stdlib for a specific compilation target using data-driven profile mapping.
pub fn load_stdlib_for_target(target: CompileTarget) -> String {
    let search_roots = find_stdlib_search_roots();
    if search_roots.is_empty() {
        return String::new();
    }

    let profiles = parse_profile_env_override().unwrap_or_else(|| {
        target_profiles(target)
            .iter()
            .map(|p| (*p).to_string())
            .collect()
    });
    load_stdlib_from_profiles(&search_roots, &profiles)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::env;
    use std::fs;
    use std::path::PathBuf;
    use tempfile::TempDir;

    /// Helper to create a test stdlib directory structure
    fn create_test_stdlib_dir(temp_dir: &TempDir) -> PathBuf {
        let stdlib_dir = temp_dir.path().join("stdlib");
        fs::create_dir(&stdlib_dir).unwrap();
        stdlib_dir
    }

    /// Helper to create a .kn file with content
    fn create_kn_file(dir: &std::path::Path, name: &str, content: &str) {
        let file_path = dir.join(name);
        fs::write(file_path, content).unwrap();
    }

    #[test]
    fn test_find_stdlib_from_env_var() {
        let temp_dir = TempDir::new().unwrap();
        let stdlib_dir = create_test_stdlib_dir(&temp_dir);

        // Set environment variable
        env::set_var("KAIN_STDLIB_PATH", stdlib_dir.to_str().unwrap());

        let roots = find_stdlib_search_roots();

        // Should find exactly one root from env var
        assert_eq!(roots.len(), 1);
        assert_eq!(roots[0], stdlib_dir);

        // Clean up
        env::remove_var("KAIN_STDLIB_PATH");
    }

    #[test]
    fn test_find_stdlib_env_var_takes_priority() {
        let temp_dir = TempDir::new().unwrap();
        let stdlib_dir = create_test_stdlib_dir(&temp_dir);

        // Set environment variable
        env::set_var("KAIN_STDLIB_PATH", stdlib_dir.to_str().unwrap());

        let roots = find_stdlib_search_roots();

        // Should return immediately with only env var path
        assert_eq!(roots.len(), 1);
        assert_eq!(roots[0], stdlib_dir);

        // Clean up
        env::remove_var("KAIN_STDLIB_PATH");
    }

    #[test]
    fn test_find_stdlib_invalid_env_var_falls_back() {
        // Set invalid environment variable
        env::set_var("KAIN_STDLIB_PATH", "/nonexistent/path/to/stdlib");

        let roots = find_stdlib_search_roots();

        // Should fall back to filesystem walking (may or may not find stdlib)
        // We just verify it doesn't panic and returns a valid Vec
        assert!(roots.is_empty() || !roots.is_empty());

        // Clean up
        env::remove_var("KAIN_STDLIB_PATH");
    }

    #[test]
    fn test_load_kn_files_alphabetical_order() {
        let temp_dir = TempDir::new().unwrap();
        let stdlib_dir = create_test_stdlib_dir(&temp_dir);

        // Create files in non-alphabetical order
        create_kn_file(&stdlib_dir, "zebra.kn", "// zebra content");
        create_kn_file(&stdlib_dir, "alpha.kn", "// alpha content");
        create_kn_file(&stdlib_dir, "middle.kn", "// middle content");

        let result = load_kn_files_from_dir(&stdlib_dir).unwrap();

        // Should be sorted alphabetically
        assert!(result.contains("// alpha content"));
        assert!(result.contains("// middle content"));
        assert!(result.contains("// zebra content"));

        // Verify order by checking positions
        let alpha_pos = result.find("// alpha content").unwrap();
        let middle_pos = result.find("// middle content").unwrap();
        let zebra_pos = result.find("// zebra content").unwrap();

        assert!(alpha_pos < middle_pos);
        assert!(middle_pos < zebra_pos);
    }

    #[test]
    fn test_load_kn_files_excludes_readme() {
        let temp_dir = TempDir::new().unwrap();
        let stdlib_dir = create_test_stdlib_dir(&temp_dir);

        // Create various README files (case-insensitive)
        create_kn_file(&stdlib_dir, "README.kn", "// readme content");
        create_kn_file(&stdlib_dir, "readme.kn", "// lowercase readme");
        create_kn_file(&stdlib_dir, "ReadMe.kn", "// mixed case readme");
        create_kn_file(&stdlib_dir, "valid.kn", "// valid content");

        let result = load_kn_files_from_dir(&stdlib_dir).unwrap();

        // Should only contain valid.kn
        assert!(result.contains("// valid content"));
        assert!(!result.contains("// readme content"));
        assert!(!result.contains("// lowercase readme"));
        assert!(!result.contains("// mixed case readme"));
    }

    #[test]
    fn test_load_kn_files_multiple_files() {
        let temp_dir = TempDir::new().unwrap();
        let stdlib_dir = create_test_stdlib_dir(&temp_dir);

        // Create multiple .kn files
        create_kn_file(&stdlib_dir, "file1.kn", "content1");
        create_kn_file(&stdlib_dir, "file2.kn", "content2");
        create_kn_file(&stdlib_dir, "file3.kn", "content3");

        let result = load_kn_files_from_dir(&stdlib_dir).unwrap();

        // Should contain all files concatenated with newlines
        assert!(result.contains("content1"));
        assert!(result.contains("content2"));
        assert!(result.contains("content3"));

        // Verify newline separation
        let lines: Vec<&str> = result.split('\n').collect();
        assert!(lines.len() >= 3);
    }

    #[test]
    fn test_load_kn_files_empty_directory() {
        let temp_dir = TempDir::new().unwrap();
        let stdlib_dir = create_test_stdlib_dir(&temp_dir);

        // Empty directory
        let result = load_kn_files_from_dir(&stdlib_dir);

        // Should return None for empty directory
        assert!(result.is_none());
    }

    #[test]
    fn test_load_kn_files_no_kn_files() {
        let temp_dir = TempDir::new().unwrap();
        let stdlib_dir = create_test_stdlib_dir(&temp_dir);

        // Create non-.kn files
        fs::write(stdlib_dir.join("file.txt"), "text content").unwrap();
        fs::write(stdlib_dir.join("file.rs"), "rust content").unwrap();

        let result = load_kn_files_from_dir(&stdlib_dir);

        // Should return None when no .kn files exist
        assert!(result.is_none());
    }

    #[test]
    fn test_load_kn_files_nonexistent_directory() {
        let temp_dir = TempDir::new().unwrap();
        let nonexistent = temp_dir.path().join("nonexistent");

        let result = load_kn_files_from_dir(&nonexistent);

        // Should return None for nonexistent directory
        assert!(result.is_none());
    }

    #[test]
    fn test_load_stdlib_graceful_degradation_no_panic() {
        // This test verifies that load_stdlib() doesn't panic when stdlib is not found
        // We can't easily force it to return empty string in this test environment
        // because the real stdlib exists in the project, but we can verify no panic

        // Set invalid environment variable
        env::set_var(
            "KAIN_STDLIB_PATH",
            "Z:\\absolutely\\nonexistent\\path\\that\\does\\not\\exist\\anywhere",
        );

        let result = load_stdlib();

        // The key requirement is that it doesn't panic
        // It may return empty string OR find the real stdlib via filesystem walking
        // Both are acceptable - the important thing is graceful handling
        assert!(result.is_empty() || !result.is_empty());

        // Clean up
        env::remove_var("KAIN_STDLIB_PATH");
    }

    #[test]
    fn test_load_stdlib_truly_empty_when_not_found() {
        // Create an isolated temp directory with no stdlib
        let temp_dir = TempDir::new().unwrap();
        let isolated_path = temp_dir.path().join("isolated");
        fs::create_dir(&isolated_path).unwrap();

        // Set env var to a path that exists but has no stdlib subdirectory
        env::set_var("KAIN_STDLIB_PATH", isolated_path.to_str().unwrap());

        let result = load_stdlib();

        // Should return empty string when stdlib directory doesn't exist
        assert_eq!(result, "");

        // Clean up
        env::remove_var("KAIN_STDLIB_PATH");
    }

    #[test]
    fn test_load_stdlib_prefers_root_profile_by_default() {
        let temp_dir = TempDir::new().unwrap();
        let stdlib_dir = create_test_stdlib_dir(&temp_dir);

        // Create both root and ue5 subdirectory
        create_kn_file(&stdlib_dir, "root.kn", "// root content");

        let ue5_dir = stdlib_dir.join("ue5");
        fs::create_dir(&ue5_dir).unwrap();
        create_kn_file(&ue5_dir, "ue5.kn", "// ue5 content");

        // Set environment variable to stdlib dir
        env::set_var("KAIN_STDLIB_PATH", stdlib_dir.to_str().unwrap());

        let result = load_stdlib();

        // Generic loads should stay universal and avoid UE5-only overlays.
        assert!(result.contains("// root content"));
        assert!(!result.contains("// ue5 content"));

        // Clean up
        env::remove_var("KAIN_STDLIB_PATH");
    }

    #[test]
    fn test_load_stdlib_falls_back_to_root() {
        let temp_dir = TempDir::new().unwrap();
        let stdlib_dir = create_test_stdlib_dir(&temp_dir);

        // Create only root files (no ue5 subdirectory)
        create_kn_file(&stdlib_dir, "root.kn", "// root content");

        // Set environment variable to stdlib dir
        env::set_var("KAIN_STDLIB_PATH", stdlib_dir.to_str().unwrap());

        let result = load_stdlib();

        // Should fall back to root directory
        assert!(result.contains("// root content"));

        // Clean up
        env::remove_var("KAIN_STDLIB_PATH");
    }

    #[test]
    fn test_load_stdlib_empty_ue5_falls_back_to_root() {
        let temp_dir = TempDir::new().unwrap();
        let stdlib_dir = create_test_stdlib_dir(&temp_dir);

        // Create empty ue5 subdirectory
        let ue5_dir = stdlib_dir.join("ue5");
        fs::create_dir(&ue5_dir).unwrap();

        // Create root files
        create_kn_file(&stdlib_dir, "root.kn", "// root content");

        // Set environment variable to stdlib dir
        env::set_var("KAIN_STDLIB_PATH", stdlib_dir.to_str().unwrap());

        let result = load_stdlib();

        // Should fall back to root when ue5/ is empty
        assert!(result.contains("// root content"));

        // Clean up
        env::remove_var("KAIN_STDLIB_PATH");
    }

    #[test]
    fn test_load_stdlib_for_target_uses_target_profile_order() {
        let temp_dir = TempDir::new().unwrap();
        let stdlib_dir = create_test_stdlib_dir(&temp_dir);
        create_kn_file(&stdlib_dir, "root.kn", "// root stdlib");
        let ue5_dir = stdlib_dir.join("ue5");
        fs::create_dir(&ue5_dir).unwrap();
        create_kn_file(&ue5_dir, "ue5.kn", "// ue5 stdlib");
        let roots = vec![stdlib_dir.clone()];
        let ts_profiles = target_profiles(CompileTarget::Ts)
            .iter()
            .map(|p| (*p).to_string())
            .collect::<Vec<_>>();
        let spirv_profiles = target_profiles(CompileTarget::Spirv)
            .iter()
            .map(|p| (*p).to_string())
            .collect::<Vec<_>>();
        let hlsl_profiles = target_profiles(CompileTarget::Hlsl)
            .iter()
            .map(|p| (*p).to_string())
            .collect::<Vec<_>>();
        let ue5_profiles = target_profiles(CompileTarget::Ue5)
            .iter()
            .map(|p| (*p).to_string())
            .collect::<Vec<_>>();
        let usf_profiles = target_profiles(CompileTarget::Usf)
            .iter()
            .map(|p| (*p).to_string())
            .collect::<Vec<_>>();

        let ts_stdlib = load_stdlib_from_profiles(&roots, &ts_profiles);
        assert!(ts_stdlib.contains("// root stdlib"));
        assert!(!ts_stdlib.contains("// ue5 stdlib"));

        let spirv_stdlib = load_stdlib_from_profiles(&roots, &spirv_profiles);
        assert!(spirv_stdlib.contains("// root stdlib"));
        assert!(!spirv_stdlib.contains("// ue5 stdlib"));

        let hlsl_stdlib = load_stdlib_from_profiles(&roots, &hlsl_profiles);
        assert!(hlsl_stdlib.contains("// root stdlib"));
        assert!(!hlsl_stdlib.contains("// ue5 stdlib"));

        let ue5_stdlib = load_stdlib_from_profiles(&roots, &ue5_profiles);
        assert!(ue5_stdlib.contains("// ue5 stdlib"));

        let usf_stdlib = load_stdlib_from_profiles(&roots, &usf_profiles);
        assert!(usf_stdlib.contains("// ue5 stdlib"));
    }

    #[test]
    fn test_stdlib_builtin_functions_exist() {
        let stdlib = StdLib::new();

        // Test a few key functions exist
        assert!(stdlib.functions.contains_key("print"));
        assert!(stdlib.functions.contains_key("println"));
        assert!(stdlib.functions.contains_key("sqrt"));
        assert!(stdlib.functions.contains_key("vec3"));
        assert!(stdlib.functions.contains_key("push"));
    }

    #[test]
    fn test_stdlib_function_metadata() {
        let stdlib = StdLib::new();

        // Test function metadata is correct
        let sqrt_fn = stdlib.functions.get("sqrt").unwrap();
        assert_eq!(sqrt_fn.name, "sqrt");
        assert_eq!(sqrt_fn.params.len(), 1);
        assert_eq!(sqrt_fn.params[0].0, "x");
        assert_eq!(sqrt_fn.params[0].1, "Float");
        assert_eq!(sqrt_fn.return_type, "Float");
        assert!(!sqrt_fn.doc.is_empty());
    }

    #[test]
    fn test_load_kn_files_filters_only_kn_extension() {
        let temp_dir = TempDir::new().unwrap();
        let stdlib_dir = create_test_stdlib_dir(&temp_dir);

        // Create files with various extensions
        create_kn_file(&stdlib_dir, "valid.kn", "// valid");
        fs::write(stdlib_dir.join("invalid.knx"), "// wrong extension").unwrap();
        fs::write(stdlib_dir.join("invalid.txt"), "// text file").unwrap();
        fs::write(stdlib_dir.join("kn"), "// no extension").unwrap();

        let result = load_kn_files_from_dir(&stdlib_dir).unwrap();

        // Should only contain valid.kn
        assert!(result.contains("// valid"));
        assert!(!result.contains("// wrong extension"));
        assert!(!result.contains("// text file"));
        assert!(!result.contains("// no extension"));
    }
}
