use std::fs;
use std::io;
use std::path::Path;

const ROOTS: [&str; 4] = ["blades", "benchmark/cases_v2", "benchmark/cases", "smoketest/src"];
const RAW_RELATIVE_ROOT: &str = "benchmark/cases/file_copy/raw/rust";
const SKIP_RELATIVE_ROOT: &str = "benchmark/cases/file_copy";
const EXPECTED_FILES: i64 = 962;
const EXPECTED_BYTES: i64 = 4_474_583;

fn normalize(path: &Path) -> String {
    let mut text = path.to_string_lossy().replace('/', "\\").to_lowercase();
    if text.starts_with("\\\\?\\") {
        text = text[4..].to_string();
    }
    if text.starts_with(".\\") {
        text = text[2..].to_string();
    }
    while text.len() > 3 && text.ends_with('\\') {
        text.pop();
    }
    text
}

fn copy_tree(
    root: &Path,
    source_root: &Path,
    skip_key: &str,
    skip_prefix: &str,
    raw_root: &Path,
    copied_files: &mut i64,
    copied_bytes: &mut i64,
) -> io::Result<()> {
    for entry in fs::read_dir(source_root)? {
        let entry = entry?;
        let path = entry.path();
        let key = normalize(&path);
        if key == skip_key || key.starts_with(skip_prefix) {
            continue;
        }
        if path.is_dir() {
            copy_tree(root, &path, skip_key, skip_prefix, raw_root, copied_files, copied_bytes)?;
            continue;
        }
        if path.is_file()
            && path
                .file_name()
                .and_then(|s| s.to_str())
                .map(|name| name.ends_with(".kn"))
                .unwrap_or(false)
        {
            let rel = path.strip_prefix(root).expect("repo-relative path");
            let mut dest = raw_root.join(rel);
            if path.file_name().and_then(|s| s.to_str()) == Some("main.kn") {
                if let Some(parent) = rel.parent() {
                    if let Some(folder) = parent.file_name().and_then(|s| s.to_str()) {
                        dest = raw_root.join(parent).join(format!("{folder}.kn"));
                    }
                }
            }
            if let Some(parent) = dest.parent() {
                fs::create_dir_all(parent)?;
            }
            let content = fs::read_to_string(&path)?;
            let content_len = content.len() as i64;
            fs::write(&dest, &content)?;
            *copied_files += 1;
            *copied_bytes += content_len;
        }
    }
    Ok(())
}

fn run() -> io::Result<()> {
    let root = std::env::current_dir()?;
    let raw_root = root.join(RAW_RELATIVE_ROOT);
    let skip_root = root.join(SKIP_RELATIVE_ROOT);
    let skip_key = normalize(&skip_root);
    let skip_prefix = format!("{skip_key}\\");

    if raw_root.exists() {
        fs::remove_dir_all(&raw_root)?;
    }
    fs::create_dir_all(&raw_root)?;

    let mut copied_files = 0_i64;
    let mut copied_bytes = 0_i64;
    for relative in ROOTS {
        let source_root = root.join(relative);
        if !source_root.exists() {
            return Err(io::Error::new(
                io::ErrorKind::NotFound,
                format!("missing source root: {}", source_root.display()),
            ));
        }
        copy_tree(
            &root,
            &source_root,
            &skip_key,
            &skip_prefix,
            &raw_root,
            &mut copied_files,
            &mut copied_bytes,
        )?;
    }

    println!("files={copied_files} bytes={copied_bytes}");
    if copied_files != EXPECTED_FILES || copied_bytes != EXPECTED_BYTES {
        return Err(io::Error::new(
            io::ErrorKind::Other,
            format!("unexpected totals: files={copied_files} bytes={copied_bytes}"),
        ));
    }

    Ok(())
}

fn main() {
    if let Err(err) = run() {
        eprintln!("{err}");
        std::process::exit(1);
    }
}
