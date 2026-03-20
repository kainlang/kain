use std::fs;
use std::process::Command;

use tempfile::tempdir;

#[test]
fn kn_without_args_shows_quick_start_menu() {
    let output = Command::new(env!("CARGO_BIN_EXE_kn"))
        .output()
        .expect("failed to execute kn binary");

    assert!(
        output.status.success(),
        "kn without args failed\nstdout:\n{}\nstderr:\n{}",
        String::from_utf8_lossy(&output.stdout),
        String::from_utf8_lossy(&output.stderr)
    );

    let stdout = String::from_utf8_lossy(&output.stdout);
    assert!(
        stdout.contains("kn Quick Start"),
        "expected quick-start menu in stdout, got:\n{}",
        stdout
    );
    assert!(
        stdout.contains("std::python::bridge"),
        "expected Python FFI hint in stdout, got:\n{}",
        stdout
    );
}

#[test]
fn kn_runs_input_file_by_default() {
    let temp = tempdir().expect("failed to create temp dir");
    let input_path = temp.path().join("main.kn");
    fs::write(&input_path, "fn main() -> Int:\n    return 7\n")
        .expect("failed to write Kain source");

    let output = Command::new(env!("CARGO_BIN_EXE_kn"))
        .arg(&input_path)
        .current_dir(temp.path())
        .output()
        .expect("failed to execute kn binary");

    assert!(
        output.status.success(),
        "kn run failed\nstdout:\n{}\nstderr:\n{}",
        String::from_utf8_lossy(&output.stdout),
        String::from_utf8_lossy(&output.stderr)
    );

    let stdout = String::from_utf8_lossy(&output.stdout);
    assert!(
        stdout.contains("7"),
        "expected interpreter result in stdout, got:\n{}",
        stdout
    );
    assert!(
        stdout.contains("Execution complete"),
        "expected interpret completion marker in stdout, got:\n{}",
        stdout
    );
}
