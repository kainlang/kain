use std::fs;
use std::process::Command;
use std::process::Stdio;

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

#[test]
fn kn_runs_inline_code_with_c_flag() {
    let output = Command::new(env!("CARGO_BIN_EXE_kn"))
        .arg("-c")
        .arg("fn main() -> Int:\n    return 11\n")
        .output()
        .expect("failed to execute kn binary");

    assert!(
        output.status.success(),
        "kn -c failed\nstdout:\n{}\nstderr:\n{}",
        String::from_utf8_lossy(&output.stdout),
        String::from_utf8_lossy(&output.stderr)
    );

    let stdout = String::from_utf8_lossy(&output.stdout);
    assert!(
        stdout.contains("11"),
        "expected inline interpreter result in stdout, got:\n{}",
        stdout
    );
}

#[test]
fn kn_runs_piped_stdin_and_ignores_shebang() {
    let mut child = Command::new(env!("CARGO_BIN_EXE_kn"))
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
        .expect("failed to spawn kn binary");

    {
        let stdin = child.stdin.as_mut().expect("expected piped stdin");
        use std::io::Write;
        stdin
            .write_all(b"#!/usr/bin/env kn\nfn main() -> Int:\n    return 13\n")
            .expect("failed to write piped source");
    }

    let output = child.wait_with_output().expect("failed to wait on kn");
    assert!(
        output.status.success(),
        "piped kn failed\nstdout:\n{}\nstderr:\n{}",
        String::from_utf8_lossy(&output.stdout),
        String::from_utf8_lossy(&output.stderr)
    );

    let stdout = String::from_utf8_lossy(&output.stdout);
    assert!(
        stdout.contains("13"),
        "expected piped interpreter result in stdout, got:\n{}",
        stdout
    );
}
