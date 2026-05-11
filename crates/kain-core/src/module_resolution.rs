//! Shared filesystem and stdlib module resolution helpers.

use std::path::{Path, PathBuf};

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct FilesystemModuleResolution {
    pub file_path: PathBuf,
    pub selected_item: Option<String>,
    pub tried_paths: Vec<PathBuf>,
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

pub fn resolve_stdlib_module_file(module_name: &str) -> Option<PathBuf> {
    find_stdlib_roots()
        .into_iter()
        .map(|root| root.join(format!("{module_name}.kn")))
        .find(|candidate| candidate.exists())
}

pub fn resolve_filesystem_module_file(
    path_segments: &[String],
) -> Option<FilesystemModuleResolution> {
    let candidates = filesystem_module_candidates(path_segments);
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

    append_blade_module_candidates(&mut candidates, path_segments);

    candidates
}

fn append_blade_module_candidates(candidates: &mut Vec<PathBuf>, path_segments: &[String]) {
    let Ok(current_dir) = std::env::current_dir() else {
        return;
    };
    let Ok(module_roots) = kain_blades::discover_blade_module_roots_from(current_dir) else {
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

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn blade_module_roots_extend_filesystem_candidates() {
        let mut candidates = filesystem_module_candidates(&["local".to_string()]);
        let root = PathBuf::from("blades/math/src");
        append_blade_module_candidates_for_roots(&mut candidates, &["math".to_string()], &[root]);

        assert!(candidates
            .iter()
            .any(|candidate| candidate.ends_with("blades/math/src/math.kn")));
    }
}
