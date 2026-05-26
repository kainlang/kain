use kain_core::ast::Type;
use kain_core::span::Span;

const S: Span = Span { start: 0, end: 0 };

#[derive(Clone, Copy)]
pub(crate) struct ExactPathRewrite {
    pub source_path: &'static [&'static str],
    pub target_path: &'static [&'static str],
}

#[derive(Clone, Copy)]
pub(crate) struct PrefixPathRewrite {
    pub source_prefix: &'static [&'static str],
    pub target_prefix: &'static [&'static str],
}

#[derive(Clone, Copy)]
struct NamedTypeRewrite {
    source_suffix: &'static [&'static str],
    target_name: &'static str,
}

#[derive(Clone, Copy)]
struct NamedMethodRewrite {
    method: &'static str,
    target_path: &'static [&'static str],
}

const EMITTED_USE_PREFIX_REWRITES: &[PrefixPathRewrite] = &[
    PrefixPathRewrite {
        source_prefix: &["tokio", "fs"],
        target_prefix: &["std", "fs"],
    },
    PrefixPathRewrite {
        source_prefix: &["tokio", "net"],
        target_prefix: &["std", "net"],
    },
    PrefixPathRewrite {
        source_prefix: &["tokio", "process"],
        target_prefix: &["std", "process"],
    },
    PrefixPathRewrite {
        source_prefix: &["tokio", "sync"],
        target_prefix: &["std", "sync"],
    },
    PrefixPathRewrite {
        source_prefix: &["tokio", "task"],
        target_prefix: &["std", "thread"],
    },
    PrefixPathRewrite {
        source_prefix: &["tokio", "time"],
        target_prefix: &["std", "time"],
    },
];

const EMITTED_USE_EXACT_REWRITES: &[ExactPathRewrite] = &[
    ExactPathRewrite {
        source_path: &["std", "env", "var"],
        target_path: &["std", "process", "process_environment"],
    },
    ExactPathRewrite {
        source_path: &["std", "env", "current_dir"],
        target_path: &["std", "process", "process_current_working_directory"],
    },
    ExactPathRewrite {
        source_path: &["std", "env", "current_exe"],
        target_path: &["std", "process", "process_current_executable_path"],
    },
    ExactPathRewrite {
        source_path: &["tokio", "time", "Duration"],
        target_path: &["std", "time", "Duration"],
    },
    ExactPathRewrite {
        source_path: &["tokio", "time", "Instant"],
        target_path: &["std", "time", "Instant"],
    },
];

const SILENTLY_SKIPPED_USE_PATHS: &[&[&str]] = &[
    &["std", "path", "Path"],
    &["std", "path", "PathBuf"],
    &["std", "ffi", "OsStr"],
    &["std", "ffi", "OsString"],
    &["tokio", "fs", "read"],
    &["tokio", "fs", "read_to_string"],
    &["tokio", "fs", "write"],
    &["tokio", "fs", "create_dir_all"],
    &["tokio", "fs", "metadata"],
    &["tokio", "fs", "remove_dir_all"],
    &["tokio", "fs", "remove_file"],
    &["tokio", "time", "sleep"],
    &["std", "thread", "sleep"],
];

const DIRECT_CALL_REWRITES: &[ExactPathRewrite] = &[
    ExactPathRewrite {
        source_path: &["std", "env", "var"],
        target_path: &["std", "process", "process_environment"],
    },
    ExactPathRewrite {
        source_path: &["std", "env", "current_dir"],
        target_path: &["std", "process", "process_current_working_directory"],
    },
    ExactPathRewrite {
        source_path: &["std", "env", "current_exe"],
        target_path: &["std", "process", "process_current_executable_path"],
    },
    ExactPathRewrite {
        source_path: &["std", "fs", "read"],
        target_path: &["std", "fs", "fs_read_bytes"],
    },
    ExactPathRewrite {
        source_path: &["std", "fs", "read_to_string"],
        target_path: &["std", "fs", "fs_read_text"],
    },
    ExactPathRewrite {
        source_path: &["std", "fs", "write"],
        target_path: &["std", "fs", "fs_write_text"],
    },
    ExactPathRewrite {
        source_path: &["std", "fs", "create_dir_all"],
        target_path: &["std", "fs", "fs_create_dir_all"],
    },
    ExactPathRewrite {
        source_path: &["std", "fs", "metadata"],
        target_path: &["std", "fs", "fs_metadata"],
    },
    ExactPathRewrite {
        source_path: &["std", "fs", "remove_dir_all"],
        target_path: &["std", "fs", "fs_remove_dir_all"],
    },
    ExactPathRewrite {
        source_path: &["std", "fs", "remove_file"],
        target_path: &["std", "fs", "fs_remove_file"],
    },
    ExactPathRewrite {
        source_path: &["std", "time", "Duration", "from_millis"],
        target_path: &["std", "time", "duration_from_millis"],
    },
    ExactPathRewrite {
        source_path: &["std", "time", "Duration", "from_secs"],
        target_path: &["std", "time", "duration_from_secs"],
    },
    ExactPathRewrite {
        source_path: &["std", "time", "Duration", "from_mins"],
        target_path: &["std", "time", "duration_from_mins"],
    },
    ExactPathRewrite {
        source_path: &["std", "time", "Duration", "from_hours"],
        target_path: &["std", "time", "duration_from_hours"],
    },
    ExactPathRewrite {
        source_path: &["std", "time", "Instant", "now"],
        target_path: &["std", "time", "instant_now"],
    },
    ExactPathRewrite {
        source_path: &["tokio", "time", "Duration", "from_millis"],
        target_path: &["std", "time", "duration_from_millis"],
    },
    ExactPathRewrite {
        source_path: &["tokio", "time", "Duration", "from_secs"],
        target_path: &["std", "time", "duration_from_secs"],
    },
    ExactPathRewrite {
        source_path: &["tokio", "time", "Duration", "from_mins"],
        target_path: &["std", "time", "duration_from_mins"],
    },
    ExactPathRewrite {
        source_path: &["tokio", "time", "Duration", "from_hours"],
        target_path: &["std", "time", "duration_from_hours"],
    },
    ExactPathRewrite {
        source_path: &["tokio", "time", "Instant", "now"],
        target_path: &["std", "time", "instant_now"],
    },
];

const ASYNC_CALL_REWRITES: &[ExactPathRewrite] = &[
    ExactPathRewrite {
        source_path: &["tokio", "fs", "read"],
        target_path: &["std", "fs", "fs_read_bytes"],
    },
    ExactPathRewrite {
        source_path: &["tokio", "fs", "read_to_string"],
        target_path: &["std", "fs", "fs_read_text"],
    },
    ExactPathRewrite {
        source_path: &["tokio", "fs", "write"],
        target_path: &["std", "fs", "fs_write_text"],
    },
    ExactPathRewrite {
        source_path: &["tokio", "fs", "create_dir_all"],
        target_path: &["std", "fs", "fs_create_dir_all"],
    },
    ExactPathRewrite {
        source_path: &["tokio", "fs", "metadata"],
        target_path: &["std", "fs", "fs_metadata"],
    },
    ExactPathRewrite {
        source_path: &["tokio", "fs", "remove_dir_all"],
        target_path: &["std", "fs", "fs_remove_dir_all"],
    },
    ExactPathRewrite {
        source_path: &["tokio", "fs", "remove_file"],
        target_path: &["std", "fs", "fs_remove_file"],
    },
];

const DIRECT_DURATION_CALL_REWRITES: &[ExactPathRewrite] = &[ExactPathRewrite {
    source_path: &["std", "thread", "sleep"],
    target_path: &["std", "time", "sleep_millis"],
}];

const ASYNC_DURATION_CALL_REWRITES: &[ExactPathRewrite] = &[ExactPathRewrite {
    source_path: &["tokio", "time", "sleep"],
    target_path: &["std", "time", "sleep_millis"],
}];

const IDENTITY_CONSTRUCTOR_PATHS: &[&[&str]] = &[
    &["Path", "new"],
    &["PathBuf", "from"],
    &["std", "path", "Path", "new"],
    &["std", "path", "PathBuf", "from"],
];

const SUPPORTED_EMITTED_USE_PREFIXES: &[&[&str]] = &[
    &["std", "actor"],
    &["std", "alloc"],
    &["std", "collections"],
    &["std", "crypto"],
    &["std", "diagnostics"],
    &["std", "fmt"],
    &["std", "fs"],
    &["std", "gpu"],
    &["std", "graphics"],
    &["std", "http"],
    &["std", "http2"],
    &["std", "input"],
    &["std", "math"],
    &["std", "net"],
    &["std", "path"],
    &["std", "platform"],
    &["std", "process"],
    &["std", "result"],
    &["std", "runtime"],
    &["std", "sync"],
    &["std", "text"],
    &["std", "thread"],
    &["std", "time"],
    &["std", "tls"],
    &["std", "ui"],
];

const QUALIFIED_TYPE_REWRITES: &[NamedTypeRewrite] = &[
    NamedTypeRewrite {
        source_suffix: &["time", "Deadline"],
        target_name: "std::time::Deadline",
    },
    NamedTypeRewrite {
        source_suffix: &["time", "Duration"],
        target_name: "std::time::Duration",
    },
    NamedTypeRewrite {
        source_suffix: &["time", "Instant"],
        target_name: "std::time::Instant",
    },
    NamedTypeRewrite {
        source_suffix: &["time", "Ticker"],
        target_name: "std::time::Ticker",
    },
    NamedTypeRewrite {
        source_suffix: &["time", "DateTime"],
        target_name: "std::time::DateTime",
    },
    NamedTypeRewrite {
        source_suffix: &["thread", "JoinHandle"],
        target_name: "std::thread::Thread",
    },
];

const HANDLE_TYPE_REWRITES: &[NamedTypeRewrite] = &[
    NamedTypeRewrite {
        source_suffix: &["net", "TcpStream"],
        target_name: "Int",
    },
    NamedTypeRewrite {
        source_suffix: &["net", "TcpListener"],
        target_name: "Int",
    },
    NamedTypeRewrite {
        source_suffix: &["process", "Child"],
        target_name: "Int",
    },
];

const STRING_LIKE_TYPE_SUFFIXES: &[&[&str]] = &[
    &["path", "Path"],
    &["path", "PathBuf"],
    &["ffi", "OsStr"],
    &["ffi", "OsString"],
];

const PATH_METHOD_REWRITES: &[NamedMethodRewrite] = &[
    NamedMethodRewrite {
        method: "join",
        target_path: &["std", "path", "path_join"],
    },
    NamedMethodRewrite {
        method: "parent",
        target_path: &["std", "path", "path_parent"],
    },
    NamedMethodRewrite {
        method: "file_name",
        target_path: &["std", "path", "path_file_name"],
    },
    NamedMethodRewrite {
        method: "extension",
        target_path: &["std", "path", "path_extension"],
    },
    NamedMethodRewrite {
        method: "file_stem",
        target_path: &["std", "path", "path_stem"],
    },
    NamedMethodRewrite {
        method: "canonicalize",
        target_path: &["std", "path", "path_canonicalize"],
    },
    NamedMethodRewrite {
        method: "normalize",
        target_path: &["std", "path", "path_normalize"],
    },
];

const DURATION_METHOD_REWRITES: &[NamedMethodRewrite] = &[
    NamedMethodRewrite {
        method: "as_millis",
        target_path: &["std", "time", "duration_to_millis"],
    },
    NamedMethodRewrite {
        method: "as_secs",
        target_path: &["std", "time", "duration_to_secs"],
    },
];

const INSTANT_METHOD_REWRITES: &[NamedMethodRewrite] = &[NamedMethodRewrite {
    method: "elapsed",
    target_path: &["std", "time", "instant_elapsed"],
}];

const DEADLINE_METHOD_REWRITES: &[NamedMethodRewrite] = &[
    NamedMethodRewrite {
        method: "is_elapsed",
        target_path: &["std", "time", "deadline_is_elapsed"],
    },
    NamedMethodRewrite {
        method: "remaining",
        target_path: &["std", "time", "deadline_remaining"],
    },
];

const PATH_PASSTHROUGH_METHODS: &[&str] = &["as_os_str", "as_path", "display", "to_string_lossy"];
const STRING_PASSTHROUGH_METHODS: &[&str] = &["as_str", "into_owned", "to_owned", "to_string"];

pub(crate) fn emitted_use_path(path: &[String]) -> Option<Vec<String>> {
    if path.is_empty() || matches_exact_list(path, SILENTLY_SKIPPED_USE_PATHS) {
        return None;
    }
    if let Some(rewritten) = apply_exact_rewrite(path, EMITTED_USE_EXACT_REWRITES) {
        return Some(rewritten);
    }
    if let Some(rewritten) = apply_prefix_rewrite(path, EMITTED_USE_PREFIX_REWRITES) {
        return Some(rewritten);
    }
    if is_local_path(path) || starts_with_exact_list(path, SUPPORTED_EMITTED_USE_PREFIXES) {
        return Some(path.to_vec());
    }
    None
}

pub(crate) fn direct_call_target(raw: &[String], resolved: &[String]) -> Option<Vec<String>> {
    apply_exact_rewrite_either(raw, resolved, DIRECT_CALL_REWRITES)
}

pub(crate) fn async_call_target(raw: &[String], resolved: &[String]) -> Option<Vec<String>> {
    apply_exact_rewrite_either(raw, resolved, ASYNC_CALL_REWRITES)
}

pub(crate) fn direct_duration_call_target(
    raw: &[String],
    resolved: &[String],
) -> Option<Vec<String>> {
    apply_exact_rewrite_either(raw, resolved, DIRECT_DURATION_CALL_REWRITES)
}

pub(crate) fn async_duration_call_target(
    raw: &[String],
    resolved: &[String],
) -> Option<Vec<String>> {
    apply_exact_rewrite_either(raw, resolved, ASYNC_DURATION_CALL_REWRITES)
}

pub(crate) fn is_identity_constructor(raw: &[String], resolved: &[String]) -> bool {
    matches_exact_list(raw, IDENTITY_CONSTRUCTOR_PATHS)
        || matches_exact_list(resolved, IDENTITY_CONSTRUCTOR_PATHS)
}

pub(crate) fn rewrite_type_path(
    raw: &[String],
    resolved: &[String],
    _generics: &[Type],
) -> Option<Type> {
    if has_type_suffix(raw, STRING_LIKE_TYPE_SUFFIXES)
        || has_type_suffix(resolved, STRING_LIKE_TYPE_SUFFIXES)
    {
        return Some(named_type("String"));
    }
    if let Some(rewritten) = rewrite_named_type(raw, resolved, QUALIFIED_TYPE_REWRITES) {
        return Some(rewritten);
    }
    rewrite_named_type(raw, resolved, HANDLE_TYPE_REWRITES)
}

pub(crate) fn path_method_target(method: &str) -> Option<&'static [&'static str]> {
    lookup_method_target(method, PATH_METHOD_REWRITES)
}

pub(crate) fn duration_method_target(method: &str) -> Option<&'static [&'static str]> {
    lookup_method_target(method, DURATION_METHOD_REWRITES)
}

pub(crate) fn instant_method_target(method: &str) -> Option<&'static [&'static str]> {
    lookup_method_target(method, INSTANT_METHOD_REWRITES)
}

pub(crate) fn deadline_method_target(method: &str) -> Option<&'static [&'static str]> {
    lookup_method_target(method, DEADLINE_METHOD_REWRITES)
}

pub(crate) fn is_path_passthrough_method(method: &str) -> bool {
    PATH_PASSTHROUGH_METHODS.contains(&method)
}

pub(crate) fn is_string_passthrough_method(method: &str) -> bool {
    STRING_PASSTHROUGH_METHODS.contains(&method)
}

pub(crate) fn is_path_like_type(ty: &Type) -> bool {
    matches!(
        ty,
        Type::Named { name, .. }
            if name == "String"
                || name == "std::path::Path"
                || name == "std::path::PathBuf"
    )
}

pub(crate) fn is_duration_type(ty: &Type) -> bool {
    matches!(ty, Type::Named { name, .. } if name == "std::time::Duration")
}

pub(crate) fn is_instant_type(ty: &Type) -> bool {
    matches!(ty, Type::Named { name, .. } if name == "std::time::Instant")
}

pub(crate) fn is_deadline_type(ty: &Type) -> bool {
    matches!(ty, Type::Named { name, .. } if name == "std::time::Deadline")
}

pub(crate) fn is_string_like_type(ty: &Type) -> bool {
    matches!(ty, Type::Named { name, .. } if name == "String")
}

pub(crate) fn is_kain_namespaced_path(path: &[String]) -> bool {
    matches!(path.first().map(String::as_str), Some("std" | "c"))
}

fn named_type(name: &str) -> Type {
    Type::Named {
        name: name.to_string(),
        generics: Vec::new(),
        span: S,
    }
}

fn rewrite_named_type(
    raw: &[String],
    resolved: &[String],
    rewrites: &[NamedTypeRewrite],
) -> Option<Type> {
    rewrites
        .iter()
        .find(|rewrite| {
            matches_path_suffix(raw, rewrite.source_suffix)
                || matches_path_suffix(resolved, rewrite.source_suffix)
        })
        .map(|rewrite| named_type(rewrite.target_name))
}

fn lookup_method_target(
    method: &str,
    rewrites: &[NamedMethodRewrite],
) -> Option<&'static [&'static str]> {
    rewrites
        .iter()
        .find(|rewrite| rewrite.method == method)
        .map(|rewrite| rewrite.target_path)
}

fn apply_exact_rewrite(path: &[String], rewrites: &[ExactPathRewrite]) -> Option<Vec<String>> {
    rewrites
        .iter()
        .find(|rewrite| matches_exact(path, rewrite.source_path))
        .map(|rewrite| to_owned_path(rewrite.target_path))
}

fn apply_exact_rewrite_either(
    raw: &[String],
    resolved: &[String],
    rewrites: &[ExactPathRewrite],
) -> Option<Vec<String>> {
    apply_exact_rewrite(raw, rewrites).or_else(|| apply_exact_rewrite(resolved, rewrites))
}

fn apply_prefix_rewrite(path: &[String], rewrites: &[PrefixPathRewrite]) -> Option<Vec<String>> {
    rewrites.iter().find_map(|rewrite| {
        if !matches_prefix(path, rewrite.source_prefix) {
            return None;
        }
        let mut rewritten = to_owned_path(rewrite.target_prefix);
        rewritten.extend(path.iter().skip(rewrite.source_prefix.len()).cloned());
        Some(rewritten)
    })
}

fn is_local_path(path: &[String]) -> bool {
    matches!(
        path.first().map(String::as_str),
        Some("crate" | "self" | "super" | "c")
    )
}

fn matches_exact(path: &[String], expected: &[&str]) -> bool {
    path.len() == expected.len()
        && path
            .iter()
            .zip(expected.iter())
            .all(|(actual, expected)| actual == expected)
}

fn matches_prefix(path: &[String], expected: &[&str]) -> bool {
    path.len() >= expected.len()
        && path
            .iter()
            .zip(expected.iter())
            .all(|(actual, expected)| actual == expected)
}

fn matches_path_suffix(path: &[String], suffix: &[&str]) -> bool {
    path.len() >= suffix.len()
        && path[path.len() - suffix.len()..]
            .iter()
            .zip(suffix.iter())
            .all(|(actual, expected)| actual == expected)
}

fn has_type_suffix(path: &[String], suffixes: &[&[&str]]) -> bool {
    suffixes
        .iter()
        .any(|suffix| matches_path_suffix(path, suffix))
}

fn to_owned_path(path: &[&str]) -> Vec<String> {
    path.iter().map(|segment| (*segment).to_string()).collect()
}

fn matches_exact_list(path: &[String], candidates: &[&[&str]]) -> bool {
    candidates
        .iter()
        .any(|candidate| matches_exact(path, candidate))
}

fn starts_with_exact_list(path: &[String], candidates: &[&[&str]]) -> bool {
    candidates
        .iter()
        .any(|candidate| matches_prefix(path, candidate))
}
