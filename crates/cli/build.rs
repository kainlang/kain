use std::env;
use std::path::Path;
use std::process::Command;
use std::time::{SystemTime, UNIX_EPOCH};

fn git_output(args: &[&str]) -> Option<String> {
    let output = Command::new("git").args(args).output().ok()?;
    if !output.status.success() {
        return None;
    }

    let value = String::from_utf8(output.stdout).ok()?;
    let trimmed = value.trim().to_string();
    if trimmed.is_empty() {
        None
    } else {
        Some(trimmed)
    }
}

fn env_override(key: &str) -> Option<String> {
    env::var(key)
        .ok()
        .map(|value| value.trim().to_string())
        .filter(|value| !value.is_empty())
}

fn main() {
    println!("cargo:rerun-if-changed=build.rs");
    println!("cargo:rerun-if-changed=src");
    println!("cargo:rerun-if-changed=Cargo.toml");
    println!("cargo:rerun-if-env-changed=KAIN_BUILD_NUMBER");
    println!("cargo:rerun-if-env-changed=KAIN_BUILD_TRACKING_MODE");
    println!("cargo:rerun-if-env-changed=KAIN_BUILD_GIT_SHA");
    println!("cargo:rerun-if-env-changed=KAIN_BUILD_GIT_COMMIT_COUNT");
    println!("cargo:rerun-if-env-changed=KAIN_BUILD_GIT_DIRTY");
    println!("cargo:rerun-if-env-changed=SOURCE_DATE_EPOCH");

    let unix_time = env::var("SOURCE_DATE_EPOCH")
        .ok()
        .and_then(|v| v.parse::<u64>().ok())
        .unwrap_or_else(|| {
            SystemTime::now()
                .duration_since(UNIX_EPOCH)
                .map(|d| d.as_secs())
                .unwrap_or(0)
        });

    // Resolve git info early so it can be reused for build-number derivation.
    let git_sha = env_override("KAIN_BUILD_GIT_SHA")
        .or_else(|| git_output(&["rev-parse", "--short=12", "HEAD"]));
    let git_commit_count = env_override("KAIN_BUILD_GIT_COMMIT_COUNT")
        .or_else(|| git_output(&["rev-list", "--count", "HEAD"]));
    let git_dirty = env_override("KAIN_BUILD_GIT_DIRTY").unwrap_or_else(|| {
        match git_output(&["status", "--porcelain"]) {
            Some(status) if status.is_empty() => "clean".to_string(),
            Some(_) => "dirty".to_string(),
            None => "unknown".to_string(),
        }
    });

    let managed_build_number = env::var("KAIN_BUILD_NUMBER")
        .ok()
        .map(|value| value.trim().to_string())
        .filter(|value| !value.is_empty());
    let build_tracking_mode = env::var("KAIN_BUILD_TRACKING_MODE")
        .ok()
        .map(|value| value.trim().to_string())
        .filter(|value| !value.is_empty());

    let (build_number, tracking_mode) = match managed_build_number {
        Some(number) => (
            number,
            build_tracking_mode.unwrap_or_else(|| "managed".to_string()),
        ),
        None => {
            // Auto-derive from git when KAIN_BUILD_NUMBER is not explicitly
            // set.  This gives every build a trackable identity instead of
            // silently marking it "unmanaged" just because nobody set an env
            // var.  Bazel sandbox builds will still get "unmanaged" unless the
            // .git directory is reachable or the sync layer passes
            // --action_env, but normal Cargo / IDE builds get full tracking.
            match (git_sha.as_ref(), git_commit_count.as_ref()) {
                (Some(sha), Some(count)) => (
                    format!("git-{}-{}", sha, count),
                    "git".to_string(),
                ),
                _ => ("unmanaged".to_string(), "unmanaged".to_string()),
            }
        }
    };

    println!("cargo:rustc-env=KAIN_BUILD_NUMBER={}", build_number);
    println!("cargo:rustc-env=KAIN_BUILD_TRACKING_MODE={}", tracking_mode);
    println!("cargo:rustc-env=KAIN_BUILD_UNIX_TIME={}", unix_time);
    println!(
        "cargo:rustc-env=KAIN_BUILD_PROFILE={}",
        env::var("PROFILE").unwrap_or_else(|_| "unknown".to_string())
    );
    println!(
        "cargo:rustc-env=KAIN_BUILD_TARGET_TRIPLE={}",
        env::var("TARGET").unwrap_or_else(|_| "unknown".to_string())
    );
    println!(
        "cargo:rustc-env=KAIN_BUILD_HOST_TRIPLE={}",
        env::var("HOST").unwrap_or_else(|_| "unknown".to_string())
    );

    let git_sha = git_sha.unwrap_or_else(|| "unknown".to_string());
    let git_commit_count = git_commit_count.unwrap_or_else(|| "0".to_string());

    println!("cargo:rustc-env=KAIN_GIT_SHA={}", git_sha);
    println!("cargo:rustc-env=KAIN_GIT_COMMIT_COUNT={}", git_commit_count);
    println!("cargo:rustc-env=KAIN_GIT_DIRTY={}", git_dirty);

    if let Some(git_dir) = git_output(&["rev-parse", "--git-dir"]) {
        let git_path = Path::new(&git_dir);
        let head_path = git_path.join("HEAD");
        if head_path.exists() {
            println!("cargo:rerun-if-changed={}", head_path.display());
        }
        if let Some(symbolic_ref) = git_output(&["symbolic-ref", "-q", "HEAD"]) {
            let ref_path = git_path.join(symbolic_ref);
            if ref_path.exists() {
                println!("cargo:rerun-if-changed={}", ref_path.display());
            }
        }
        let packed_refs_path = git_path.join("packed-refs");
        if packed_refs_path.exists() {
            println!("cargo:rerun-if-changed={}", packed_refs_path.display());
        }
    }
}
