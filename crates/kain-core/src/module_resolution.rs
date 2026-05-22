//! Shared filesystem and stdlib module resolution helpers.

use std::path::{Path, PathBuf};

const STDLIB_FLAT_NESTED_ALIAS_PAIRS: &[(&str, &str)] = &[("graphics/shared", "graphics_shared")];

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct FilesystemModuleResolution {
    pub file_path: PathBuf,
    pub selected_item: Option<String>,
    pub tried_paths: Vec<PathBuf>,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct FilesystemModuleResolutionContext {
    pub importer_file: Option<PathBuf>,
}

pub fn find_stdlib_roots() -> Vec<PathBuf> {
    let mut roots = Vec::new();

    if let Ok(env_path) = std::env::var("KAIN_STDLIB_PATH") {
        roots.push(PathBuf::from(env_path));
    }

    if let Ok(exe_path) = std::env::current_exe() {
        if let Some(mut dir) = exe_path.parent().map(|p| p.to_path_buf()) {
            loop {
                roots.push(dir.join("stdlib"));
                if !dir.pop() {
                    break;
                }
            }
        }
    }

    if let Ok(mut dir) = std::env::current_dir() {
        loop {
            roots.push(dir.join("stdlib"));
            if !dir.pop() {
                break;
            }
        }
    }

    roots
}

pub fn canonical_stdlib_module_name(module_name: &str) -> String {
    let normalized_module_name = module_name.strip_prefix("native/").unwrap_or(module_name);
    for (canonical_name, flat_name) in STDLIB_FLAT_NESTED_ALIAS_PAIRS {
        if normalized_module_name == *flat_name {
            return (*canonical_name).to_string();
        }
    }
    normalized_module_name.to_string()
}

pub fn stdlib_module_lookup_names(module_name: &str) -> Vec<String> {
    let normalized_module_name = module_name.strip_prefix("native/").unwrap_or(module_name);
    let mut candidates = Vec::new();
    push_unique_module_name(&mut candidates, normalized_module_name);
    for (canonical_name, flat_name) in STDLIB_FLAT_NESTED_ALIAS_PAIRS {
        if normalized_module_name == *canonical_name {
            push_unique_module_name(&mut candidates, flat_name);
        }
    }
    candidates
}

pub fn resolve_stdlib_module_file(module_name: &str) -> Option<PathBuf> {
    let candidate_names = stdlib_module_lookup_names(module_name);
    for root in find_stdlib_roots() {
        for candidate_name in &candidate_names {
            let candidate = root.join(format!("{candidate_name}.kn"));
            if candidate.exists() {
                return Some(candidate);
            }
        }
    }
    None
}

pub fn resolve_filesystem_module_file(
    path_segments: &[String],
) -> Option<FilesystemModuleResolution> {
    resolve_filesystem_module_file_with_context(
        path_segments,
        &FilesystemModuleResolutionContext::default(),
    )
}

pub fn resolve_filesystem_module_file_with_context(
    path_segments: &[String],
    context: &FilesystemModuleResolutionContext,
) -> Option<FilesystemModuleResolution> {
    let candidates = filesystem_module_candidates_with_context(path_segments, context);
    let fallback_start = filesystem_item_import_fallback_start(path_segments, candidates.len());

    for (index, candidate) in candidates.iter().enumerate() {
        if candidate.exists() {
            let selected_item = if index >= fallback_start {
                path_segments.last().cloned()
            } else {
                None
            };

            return Some(FilesystemModuleResolution {
                file_path: candidate.clone(),
                selected_item,
                tried_paths: candidates,
            });
        }
    }

    None
}

pub fn filesystem_module_candidates(path_segments: &[String]) -> Vec<PathBuf> {
    filesystem_module_candidates_with_context(
        path_segments,
        &FilesystemModuleResolutionContext::default(),
    )
}

pub fn filesystem_module_candidates_with_context(
    path_segments: &[String],
    context: &FilesystemModuleResolutionContext,
) -> Vec<PathBuf> {
    if path_segments.is_empty() {
        return Vec::new();
    }

    let path = path_segments.join("/");
    let base_path = Path::new(&path);
    let module_base = path_segments
        .first()
        .map(|segment| segment.as_str())
        .unwrap_or(path.as_str());
    let module_base_path = Path::new(module_base);

    let mut candidates = Vec::new();
    push_unique_path(&mut candidates, base_path.with_extension("kn"));
    push_unique_path(&mut candidates, PathBuf::from(format!("src/{path}.kn")));
    push_unique_path(
        &mut candidates,
        PathBuf::from(format!("src/core/{module_base}.kn")),
    );
    push_unique_path(&mut candidates, PathBuf::from(format!("{path}.kn")));
    push_unique_path(&mut candidates, base_path.with_extension("god"));

    if path_segments.len() > 1 {
        push_unique_path(&mut candidates, module_base_path.with_extension("kn"));
        push_unique_path(
            &mut candidates,
            PathBuf::from(format!("src/{module_base}.kn")),
        );
        push_unique_path(&mut candidates, module_base_path.with_extension("god"));
    }

    append_context_module_candidates(&mut candidates, path_segments, context);

    candidates
}

fn append_context_module_candidates(
    candidates: &mut Vec<PathBuf>,
    path_segments: &[String],
    context: &FilesystemModuleResolutionContext,
) {
    if let Some(importer_file) = context.importer_file.as_deref() {
        append_importer_relative_candidates(candidates, path_segments, importer_file);
    }
    append_blade_module_candidates(candidates, path_segments, context);
}

fn append_importer_relative_candidates(
    candidates: &mut Vec<PathBuf>,
    path_segments: &[String],
    importer_file: &Path,
) {
    let Some(importer_dir) = importer_file.parent() else {
        return;
    };
    let path = path_segments.join("/");
    let base_path = Path::new(&path);
    let module_base = path_segments
        .first()
        .map(|segment| segment.as_str())
        .unwrap_or(path.as_str());
    let module_base_path = Path::new(module_base);

    let mut current = importer_dir.to_path_buf();
    loop {
        push_unique_path(candidates, current.join(&path).with_extension("kn"));
        push_unique_path(candidates, current.join("src").join(&path).with_extension("kn"));
        push_unique_path(
            candidates,
            current.join("src").join("core").join(module_base).with_extension("kn"),
        );
        push_unique_path(candidates, current.join(format!("{path}.kn")));
        push_unique_path(candidates, current.join(base_path).with_extension("god"));

        if path_segments.len() > 1 {
            push_unique_path(candidates, current.join(module_base).with_extension("kn"));
            push_unique_path(
                candidates,
                current.join("src").join(module_base).with_extension("kn"),
            );
            push_unique_path(candidates, current.join(module_base_path).with_extension("god"));
        }

        let Some(parent) = current.parent() else {
            break;
        };
        current = parent.to_path_buf();
    }
}

fn append_blade_module_candidates(
    candidates: &mut Vec<PathBuf>,
    path_segments: &[String],
    context: &FilesystemModuleResolutionContext,
) {
    let start = context.importer_file.as_deref().unwrap_or_else(|| Path::new("."));
    let Ok(module_roots) = blade::discover_blade_module_roots_from(start) else {
        return;
    };
    append_blade_module_candidates_for_roots(candidates, path_segments, &module_roots);
}

fn append_blade_module_candidates_for_roots(
    candidates: &mut Vec<PathBuf>,
    path_segments: &[String],
    module_roots: &[PathBuf],
) {
    if path_segments.is_empty() {
        return;
    }
    let path = path_segments.join("/");
    let module_base = path_segments
        .first()
        .map(|segment| segment.as_str())
        .unwrap_or(path.as_str());

    for root in module_roots {
        push_unique_path(candidates, root.join(&path).with_extension("kn"));
        if path_segments.len() > 1 {
            push_unique_path(candidates, root.join(module_base).with_extension("kn"));
        }
    }
}

fn filesystem_item_import_fallback_start(
    path_segments: &[String],
    candidate_count: usize,
) -> usize {
    if path_segments.len() > 1 {
        candidate_count.saturating_sub(3)
    } else {
        usize::MAX
    }
}

fn push_unique_path(paths: &mut Vec<PathBuf>, path: PathBuf) {
    if !paths.iter().any(|existing| existing == &path) {
        paths.push(path);
    }
}

fn push_unique_module_name(names: &mut Vec<String>, name: &str) {
    if !names.iter().any(|existing| existing == name) {
        names.push(name.to_string());
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::env;
    use std::fs;
    use tempfile::TempDir;

    #[test]
    fn blade_module_roots_extend_filesystem_candidates() {
        let mut candidates = filesystem_module_candidates(&["local".to_string()]);
        let root = PathBuf::from("blades/math/src");
        append_blade_module_candidates_for_roots(&mut candidates, &["math".to_string()], &[root]);

        assert!(candidates
            .iter()
            .any(|candidate| candidate.ends_with("blades/math/src/math.kn")));
    }

    #[test]
    fn stdlib_native_prefix_resolves_to_root_module_file() {
        let temp_dir = TempDir::new().unwrap();
        let stdlib_dir = temp_dir.path().join("stdlib");
        fs::create_dir_all(&stdlib_dir).unwrap();
        let actor_path = stdlib_dir.join("actor.kn");
        fs::write(&actor_path, "pub fn actor_ping() -> Int:\n    return 1\n").unwrap();

        let previous_stdlib_path = env::var_os("KAIN_STDLIB_PATH");
        env::set_var("KAIN_STDLIB_PATH", &stdlib_dir);

        let resolved = resolve_stdlib_module_file("native/actor");

        match previous_stdlib_path {
            Some(previous) => env::set_var("KAIN_STDLIB_PATH", previous),
            None => env::remove_var("KAIN_STDLIB_PATH"),
        }

        assert_eq!(resolved, Some(actor_path));
    }

    #[test]
    fn stdlib_flat_nested_alias_resolves_to_root_module_file() {
        let temp_dir = TempDir::new().unwrap();
        let stdlib_dir = temp_dir.path().join("stdlib");
        fs::create_dir_all(&stdlib_dir).unwrap();
        let shared_path = stdlib_dir.join("graphics_shared.kn");
        fs::write(
            &shared_path,
            "pub fn graphics_shared_ping() -> Int:\n    return 1\n",
        )
        .unwrap();

        let previous_stdlib_path = env::var_os("KAIN_STDLIB_PATH");
        env::set_var("KAIN_STDLIB_PATH", &stdlib_dir);

        let resolved = resolve_stdlib_module_file("graphics/shared");
        let canonical = canonical_stdlib_module_name("graphics_shared");

        match previous_stdlib_path {
            Some(previous) => env::set_var("KAIN_STDLIB_PATH", previous),
            None => env::remove_var("KAIN_STDLIB_PATH"),
        }

        assert_eq!(resolved, Some(shared_path));
        assert_eq!(canonical, "graphics/shared");
    }

    #[test]
    fn importer_relative_resolution_prefers_source_ancestors() {
        let temp_dir = TempDir::new().unwrap();
        let project_root = temp_dir.path();
        let importer_dir = project_root.join("feature").join("nested");
        let importer_path = importer_dir.join("panel.kn");
        let module_path = project_root.join("src").join("layout.kn");
        fs::create_dir_all(importer_dir).unwrap();
        fs::create_dir_all(module_path.parent().unwrap()).unwrap();
        fs::write(&importer_path, "use layout\n").unwrap();
        fs::write(&module_path, "pub fn layout_ping() -> Int:\n    return 1\n").unwrap();

        let resolution = resolve_filesystem_module_file_with_context(
            &["layout".to_string()],
            &FilesystemModuleResolutionContext {
                importer_file: Some(importer_path.clone()),
            },
        )
        .expect("importer-relative module should resolve");

        assert_eq!(resolution.file_path, module_path);
    }
}
