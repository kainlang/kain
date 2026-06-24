use std::fs;
use std::process::Command;
use std::process::Stdio;

use tempfile::tempdir;

fn kn_command() -> Command {
    let mut cmd = Command::new(env!("CARGO_BIN_EXE_kain"));
    cmd.env("KAIN_LAUNCHER_KIND", "kn");
    cmd
}

#[test]
fn kn_without_args_shows_quick_start_menu() {
    let output = kn_command()
        .output()
        .expect("failed to execute kn alias (kain binary)");

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
        stdout.contains("std::python"),
        "expected Python FFI hint in stdout, got:\n{}",
        stdout
    );
}

#[test]
fn kn_runs_input_file_by_default() {
    let temp = tempdir().expect("failed to create temp dir");
    let input_path = temp.path().join("main.kn");
    fs::write(
        &input_path,
        "use std::fs\nfn main() -> Int:\n    fs_write_text(\"kn_file_default.txt\", \"hello from kn\")\n    return 0\n",
    )
    .expect("failed to write Kain source");

    let output = kn_command()
        .arg(&input_path)
        .current_dir(temp.path())
        .output()
        .expect("failed to execute kn alias (kain binary)");

    assert!(
        output.status.success(),
        "kn run failed\nstdout:\n{}\nstderr:\n{}",
        String::from_utf8_lossy(&output.stdout),
        String::from_utf8_lossy(&output.stderr)
    );

    assert!(
        temp.path().join("kn_file_default.txt").exists(),
        "expected native script side effect file to exist"
    );
    assert_eq!(
        fs::read_to_string(temp.path().join("kn_file_default.txt"))
            .expect("failed to read script side effect"),
        "hello from kn"
    );
}

#[test]
fn kn_runs_inline_code_with_c_flag() {
    let temp = tempdir().expect("failed to create temp dir");
    let output = kn_command()
        .arg("-c")
        .arg("use std::fs\nfn main() -> Int:\n    fs_write_text(\"kn_inline.txt\", \"hello from inline\")\n    return 0\n")
        .current_dir(temp.path())
        .output()
        .expect("failed to execute kn alias (kain binary)");

    assert!(
        output.status.success(),
        "kn -c failed\nstdout:\n{}\nstderr:\n{}",
        String::from_utf8_lossy(&output.stdout),
        String::from_utf8_lossy(&output.stderr)
    );
    assert!(
        temp.path().join("kn_inline.txt").exists(),
        "expected inline native script to create its output file"
    );
    assert_eq!(
        fs::read_to_string(temp.path().join("kn_inline.txt"))
            .expect("failed to read inline output"),
        "hello from inline"
    );
}

#[test]
fn kn_runs_piped_stdin_and_ignores_shebang() {
    let temp = tempdir().expect("failed to create temp dir");
    let mut child = kn_command()
        .current_dir(temp.path())
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
        .expect("failed to spawn kn alias (kain binary)");

    {
        let stdin = child.stdin.as_mut().expect("expected piped stdin");
        use std::io::Write;
        stdin
            .write_all(
                b"#!/usr/bin/env kn\nuse std::fs\nfn main() -> Int:\n    fs_write_text(\"kn_stdin.txt\", \"hello from stdin\")\n    return 0\n",
            )
            .expect("failed to write piped source");
    }

    let output = child.wait_with_output().expect("failed to wait on kn");
    assert!(
        output.status.success(),
        "piped kn failed\nstdout:\n{}\nstderr:\n{}",
        String::from_utf8_lossy(&output.stdout),
        String::from_utf8_lossy(&output.stderr)
    );
    assert!(
        temp.path().join("kn_stdin.txt").exists(),
        "expected stdin native script to create its output file"
    );
    assert_eq!(
        fs::read_to_string(temp.path().join("kn_stdin.txt")).expect("failed to read stdin output"),
        "hello from stdin"
    );
}

#[test]
fn kain_runs_inline_code_with_full_native_stdlib_flow() {
    let temp = tempdir().expect("failed to create temp dir");

    let output = Command::new(env!("CARGO_BIN_EXE_kain"))
        .arg("-c")
        .arg("use std::fs\nfn main() -> Int:\n    fs_write_text(\"kain_inline.txt\", \"hello from kain\")\n    return 0\n")
        .current_dir(temp.path())
        .output()
        .expect("failed to execute kain binary");

    assert!(
        output.status.success(),
        "kain -c failed\nstdout:\n{}\nstderr:\n{}",
        String::from_utf8_lossy(&output.stdout),
        String::from_utf8_lossy(&output.stderr)
    );
    assert_eq!(
        fs::read_to_string(temp.path().join("kain_inline.txt"))
            .expect("failed to read kain inline output"),
        "hello from kain"
    );
}

#[test]
fn repl_executes_native_stdlib_scripts() {
    let temp = tempdir().expect("failed to create temp dir");
    let script_path = temp.path().join("repl_native.txt");

    let mut child = Command::new(env!("CARGO_BIN_EXE_kain"))
        .arg("repl")
        .current_dir(temp.path())
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
        .expect("failed to spawn kain repl");

    {
        let stdin = child.stdin.as_mut().expect("expected repl stdin");
        use std::io::Write;
        stdin
            .write_all(
                b"use std::fs\nfn main() -> Int:\n    fs_write_text(\"repl_native.txt\", \"hello from repl\")\n    return 0\n\n.exit\n",
            )
            .expect("failed to drive repl session");
    }

    let output = child
        .wait_with_output()
        .expect("failed to wait on kain repl");
    assert!(
        output.status.success(),
        "kain repl failed\nstdout:\n{}\nstderr:\n{}",
        String::from_utf8_lossy(&output.stdout),
        String::from_utf8_lossy(&output.stderr)
    );
    assert_eq!(
        fs::read_to_string(&script_path).expect("failed to read repl output"),
        "hello from repl"
    );
    let stdout = String::from_utf8_lossy(&output.stdout);
    assert!(
        stdout.contains("Execution complete"),
        "expected repl success marker in stdout, got:\n{}",
        stdout
    );
}
