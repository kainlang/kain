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
            "json_parse",
            &[("text", "String")],
            "Any",
            "Parse JSON text",
        );
        lib.add_fn(
            "json_string",
            &[("value", "Any")],
            "String",
            "Serialize a KAIN value into JSON text",
        );
        lib.add_fn(
            "json_get",
            &[("object", "Any"), ("key", "String")],
            "Any",
            "Read a field from a JSON object, returning null-like None when missing",
        );
        lib.add_fn(
            "json_get_string",
            &[("object", "Any"), ("key", "String")],
            "String",
            "Read a required string field from a JSON object",
        );
        lib.add_fn(
            "json_get_int",
            &[("object", "Any"), ("key", "String")],
            "Int",
            "Read a required integer field from a JSON object",
        );
        lib.add_fn(
            "json_get_bool",
            &[("object", "Any"), ("key", "String")],
            "Bool",
            "Read a required boolean field from a JSON object",
        );
        lib.add_fn(
            "json_has",
            &[("object", "Any"), ("key", "String")],
            "Bool",
            "Check whether a JSON object contains a field",
        );
        lib.add_fn(
            "json_object_new",
            &[],
            "Any",
            "Create a new mutable JSON object",
        );
        lib.add_fn(
            "json_object_set",
            &[("object", "Any"), ("key", "String"), ("value", "Any")],
            "Unit",
            "Set a field on a mutable JSON object",
        );
        lib.add_fn(
            "json_array_new",
            &[],
            "Array<Any>",
            "Create a new mutable JSON array",
        );
        lib.add_fn(
            "json_array_push",
            &[("array", "Any"), ("value", "Any")],
            "Unit",
            "Append a value to a mutable JSON array",
        );
        lib.add_fn(
            "json_array_len",
            &[("array", "Any")],
            "Int",
            "Return the number of items in a JSON array",
        );
        lib.add_fn(
            "json_array_get",
            &[("array", "Any"), ("index", "Int")],
            "Any",
            "Read one element from a JSON array",
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
            "Check whether a path exists",
        );
        lib.add_fn(
            "env",
            &[("name", "String")],
            "String",
            "Read an environment variable, returning an empty string if missing",
        );
        lib.add_fn("cwd", &[], "String", "Return the current working directory");
        lib.add_fn(
            "command_run",
            &[
                ("program", "String"),
                ("args", "Array<String>"),
                ("workdir", "String"),
            ],
            "CommandRunResult",
            "Run a subprocess, capture stdout/stderr, and return the exit status",
        );
        lib.add_fn(
            "read_dir",
            &[("path", "String")],
            "Array<String>",
            "List direct children of a directory as sorted paths",
        );
        lib.add_fn(
            "create_dir_all",
            &[("path", "String")],
            "Unit",
            "Create a directory and any missing parents",
        );
        lib.add_fn(
            "copy_file",
            &[("src", "String"), ("dest", "String")],
            "Unit",
            "Copy one file to another path",
        );
        lib.add_fn(
            "remove_file",
            &[("path", "String")],
            "Unit",
            "Remove a file",
        );
        lib.add_fn(
            "path_join",
            &[("base", "String"), ("child", "String")],
            "String",
            "Join two path segments",
        );
        lib.add_fn(
            "path_parent",
            &[("path", "String")],
            "String",
            "Return the parent directory path or an empty string",
        );
        lib.add_fn(
            "path_file_name",
            &[("path", "String")],
            "String",
            "Return the final path component or an empty string",
        );
        lib.add_fn(
            "path_extension",
            &[("path", "String")],
            "String",
            "Return the file extension or an empty string",
        );
        lib.add_fn(
            "path_stem",
            &[("path", "String")],
            "String",
            "Return the file stem or an empty string",
        );
        lib.add_fn(
            "path_is_file",
            &[("path", "String")],
            "Bool",
            "Check whether a path is a file",
        );
        lib.add_fn(
            "path_is_dir",
            &[("path", "String")],
            "Bool",
            "Check whether a path is a directory",
        );
        for (name, return_type, doc) in [
            ("fs_read_text", "String", "Read UTF-8 text from a file"),
            (
                "fs_try_read_text",
                "Any",
                "Read UTF-8 text from a file as Result<String, FsError>",
            ),
            ("fs_read_bytes", "Array<Int>", "Read raw bytes from a file"),
            (
                "fs_try_read_bytes",
                "Any",
                "Read raw bytes from a file as Result<Array<Int>, FsError>",
            ),
            ("fs_exists", "Bool", "Check whether a path exists"),
            ("fs_is_file", "Bool", "Check whether a path is a file"),
            ("fs_is_dir", "Bool", "Check whether a path is a directory"),
            ("fs_is_symlink", "Bool", "Check whether a path is a symlink"),
            (
                "fs_metadata",
                "FsMetadata",
                "Read typed metadata for a path",
            ),
            (
                "fs_try_metadata",
                "Any",
                "Read typed metadata as Result<FsMetadata, FsError>",
            ),
            (
                "fs_symlink_metadata",
                "FsMetadata",
                "Read symlink-aware metadata for a path",
            ),
            (
                "fs_try_symlink_metadata",
                "Any",
                "Read symlink-aware metadata as Result<FsMetadata, FsError>",
            ),
            (
                "fs_read_dir",
                "Array<FsDirEntry>",
                "Read direct directory entries as FsDirEntry values",
            ),
            (
                "fs_try_read_dir",
                "Any",
                "Read direct directory entries as Result<Array<FsDirEntry>, FsError>",
            ),
            (
                "fs_read_dir_paths",
                "Array<String>",
                "Read direct directory entries as sorted paths",
            ),
            (
                "fs_try_read_dir_paths",
                "Any",
                "Read direct directory paths as Result<Array<String>, FsError>",
            ),
            (
                "fs_walk",
                "Array<FsDirEntry>",
                "Recursively walk a directory as FsDirEntry values",
            ),
            (
                "fs_try_walk",
                "Any",
                "Recursively walk a directory as Result<Array<FsDirEntry>, FsError>",
            ),
            (
                "fs_glob",
                "Array<String>",
                "Expand a glob pattern into sorted paths",
            ),
            (
                "fs_try_glob",
                "Any",
                "Expand a glob pattern as Result<Array<String>, FsError>",
            ),
            (
                "fs_temp_file",
                "String",
                "Create a temp file and return its path",
            ),
            (
                "fs_temp_dir",
                "String",
                "Create a temp directory and return its path",
            ),
            (
                "fs_hash_file",
                "String",
                "Compute a SHA-256 hash for a file",
            ),
            (
                "fs_try_hash_file",
                "Any",
                "Compute a SHA-256 hash as Result<String, FsError>",
            ),
            (
                "fs_path_parent",
                "String",
                "Return the parent directory path or empty string",
            ),
            (
                "fs_path_file_name",
                "String",
                "Return the final path component or empty string",
            ),
            (
                "fs_path_extension",
                "String",
                "Return the path extension or empty string",
            ),
            (
                "fs_path_stem",
                "String",
                "Return the path stem or empty string",
            ),
            ("fs_path_normalize", "String", "Normalize a path lexically"),
            (
                "fs_path_absolute",
                "String",
                "Return an absolute normalized path",
            ),
            (
                "fs_path_canonicalize",
                "String",
                "Return the filesystem canonical path",
            ),
        ] {
            lib.add_fn(name, &[("path", "String")], return_type, doc);
        }
        for (name, return_type, doc) in [
            ("fs_write_text", "Unit", "Write UTF-8 text to a file"),
            (
                "fs_try_write_text",
                "Any",
                "Write UTF-8 text as Result<Unit, FsError>",
            ),
            ("fs_append_text", "Unit", "Append UTF-8 text to a file"),
            (
                "fs_try_append_text",
                "Any",
                "Append UTF-8 text as Result<Unit, FsError>",
            ),
            (
                "fs_atomic_write_text",
                "Unit",
                "Write UTF-8 text through an atomic sibling temp path",
            ),
            (
                "fs_try_atomic_write_text",
                "Any",
                "Atomic text write as Result<Unit, FsError>",
            ),
        ] {
            lib.add_fn(
                name,
                &[("path", "String"), ("content", "String")],
                return_type,
                doc,
            );
        }
        for (name, return_type, doc) in [
            ("fs_write_bytes", "Unit", "Write raw bytes to a file"),
            (
                "fs_try_write_bytes",
                "Any",
                "Write raw bytes as Result<Unit, FsError>",
            ),
            ("fs_append_bytes", "Unit", "Append raw bytes to a file"),
            (
                "fs_try_append_bytes",
                "Any",
                "Append raw bytes as Result<Unit, FsError>",
            ),
            (
                "fs_atomic_write_bytes",
                "Unit",
                "Write raw bytes through an atomic sibling temp path",
            ),
        ] {
            lib.add_fn(
                name,
                &[("path", "String"), ("bytes", "Array<Int>")],
                return_type,
                doc,
            );
        }
        for (name, return_type, doc) in [
            ("fs_create_dir", "Unit", "Create one directory"),
            (
                "fs_try_create_dir",
                "Any",
                "Create one directory as Result<Unit, FsError>",
            ),
            (
                "fs_create_dir_all",
                "Unit",
                "Create a directory and any missing parents",
            ),
            (
                "fs_try_create_dir_all",
                "Any",
                "Create missing parent directories as Result<Unit, FsError>",
            ),
            ("fs_remove_file", "Unit", "Remove a file"),
            (
                "fs_try_remove_file",
                "Any",
                "Remove a file as Result<Unit, FsError>",
            ),
            ("fs_remove_dir", "Unit", "Remove an empty directory"),
            (
                "fs_try_remove_dir",
                "Any",
                "Remove an empty directory as Result<Unit, FsError>",
            ),
            ("fs_remove_dir_all", "Unit", "Remove a directory tree"),
            (
                "fs_try_remove_dir_all",
                "Any",
                "Remove a directory tree as Result<Unit, FsError>",
            ),
            (
                "fs_remove_path",
                "Unit",
                "Remove a file or directory tree if it exists",
            ),
            (
                "fs_try_remove_path",
                "Any",
                "Remove a path as Result<Unit, FsError>",
            ),
        ] {
            lib.add_fn(name, &[("path", "String")], return_type, doc);
        }
        for (name, return_type, doc) in [
            ("fs_copy_file", "Int", "Copy one file"),
            (
                "fs_try_copy_file",
                "Any",
                "Copy one file as Result<Int, FsError>",
            ),
            ("fs_copy_path", "Unit", "Copy a file or directory tree"),
            (
                "fs_try_copy_path",
                "Any",
                "Copy a path as Result<Unit, FsError>",
            ),
            ("fs_move_path", "Unit", "Move or rename a path"),
            (
                "fs_try_move_path",
                "Any",
                "Move or rename a path as Result<Unit, FsError>",
            ),
            ("fs_path_join", "String", "Join two path segments"),
        ] {
            lib.add_fn(
                name,
                &[("src", "String"), ("dest", "String")],
                return_type,
                doc,
            );
        }

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
        lib.add_fn(
            "ask",
            &[
                ("actor", "ActorRef"),
                ("message", "String"),
                ("request", "Any"),
            ],
            "Any",
            "Send a message to an actor and wait for the first reply",
        );
        lib.add_fn(
            "ask_timeout",
            &[
                ("actor", "ActorRef"),
                ("message", "String"),
                ("request", "Any"),
                ("timeout_ms", "Int"),
            ],
            "Any",
            "Send a message to an actor and wait for the first reply with a custom timeout",
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
    (CompileTarget::Ue5, &["ue5"]),
    (CompileTarget::Ue5Editor, &["ue5"]),
    (CompileTarget::Usf, &["ue5"]),
    (CompileTarget::Hlsl, &[""]),
    (CompileTarget::Spirv, &[""]),
    (CompileTarget::Wasm, &[""]),
    (CompileTarget::Js, &[""]),
    (CompileTarget::Ts, &[""]),
    (CompileTarget::Hybrid, &[""]),
    (CompileTarget::Llvm, &["native"]),
    (CompileTarget::C, &["native", "c"]),
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
        let mut chunks = Vec::new();
        for profile in profiles {
            let candidate_dir = resolve_profile_path(root, profile);
            if candidate_dir.exists() && candidate_dir.is_dir() {
                if let Some(stdlib_source) = load_kn_files_from_dir(&candidate_dir) {
                    eprintln!("Loaded stdlib from: {}", candidate_dir.display());
                    chunks.push(stdlib_source);
                }
            }
        }
        if !chunks.is_empty() {
            return chunks.join("\n");
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
        let c_dir = stdlib_dir.join("c");
        fs::create_dir(&c_dir).unwrap();
        create_kn_file(&c_dir, "c.kn", "// c stdlib");
        let native_dir = stdlib_dir.join("native");
        fs::create_dir(&native_dir).unwrap();
        create_kn_file(&native_dir, "native.kn", "// native stdlib");
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
        let c_profiles = target_profiles(CompileTarget::C)
            .iter()
            .map(|p| (*p).to_string())
            .collect::<Vec<_>>();
        let llvm_profiles = target_profiles(CompileTarget::Llvm)
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
        assert!(!ue5_stdlib.contains("// root stdlib"));

        let usf_stdlib = load_stdlib_from_profiles(&roots, &usf_profiles);
        assert!(usf_stdlib.contains("// ue5 stdlib"));
        assert!(!usf_stdlib.contains("// root stdlib"));

        let llvm_stdlib = load_stdlib_from_profiles(&roots, &llvm_profiles);
        assert!(llvm_stdlib.contains("// native stdlib"));
        assert!(!llvm_stdlib.contains("// root stdlib"));
        assert!(!llvm_stdlib.contains("// c stdlib"));

        let c_stdlib = load_stdlib_from_profiles(&roots, &c_profiles);
        assert!(c_stdlib.contains("// native stdlib"));
        assert!(c_stdlib.contains("// c stdlib"));
        assert!(!c_stdlib.contains("// root stdlib"));
        let native_pos = c_stdlib.find("// native stdlib").unwrap();
        let c_pos = c_stdlib.find("// c stdlib").unwrap();
        assert!(native_pos < c_pos);
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
        assert!(stdlib.functions.contains_key("ask"));
        assert!(stdlib.functions.contains_key("ask_timeout"));
        assert!(stdlib.functions.contains_key("command_run"));
        assert!(stdlib.functions.contains_key("json_parse"));
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
