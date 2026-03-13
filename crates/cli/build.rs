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

fn main() {
    println!("cargo:rerun-if-changed=build.rs");
    println!("cargo:rerun-if-changed=src");
    println!("cargo:rerun-if-changed=Cargo.toml");
    println!("cargo:rerun-if-env-changed=KAIN_BUILD_NUMBER");
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

    let build_number = env::var("KAIN_BUILD_NUMBER").unwrap_or_else(|_| unix_time.to_string());

    println!("cargo:rustc-env=KAIN_BUILD_NUMBER={}", build_number);
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

    let git_sha =
        git_output(&["rev-parse", "--short=12", "HEAD"]).unwrap_or_else(|| "unknown".to_string());
    let git_commit_count =
        git_output(&["rev-list", "--count", "HEAD"]).unwrap_or_else(|| "0".to_string());
    let git_dirty = match git_output(&["status", "--porcelain"]) {
        Some(status) if status.is_empty() => "clean".to_string(),
        Some(_) => "dirty".to_string(),
        None => "unknown".to_string(),
    };

    println!("cargo:rustc-env=KAIN_GIT_SHA={}", git_sha);
    println!("cargo:rustc-env=KAIN_GIT_COMMIT_COUNT={}", git_commit_count);
    println!("cargo:rustc-env=KAIN_GIT_DIRTY={}", git_dirty);

    if let Some(git_dir) = git_output(&["rev-parse", "--git-dir"]) {
        let git_path = Path::new(&git_dir);
        let head_path = git_path.join("HEAD");
        if head_path.exists() {
            println!("cargo:rerun-if-changed={}", head_path.display());
        }
    }
}
