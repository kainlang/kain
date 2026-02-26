use std::fs;
use std::process::Command;
use std::time::{SystemTime, UNIX_EPOCH};

#[test]
fn build_single_file_with_typescript_target_generates_ts_output() {
    let base = std::env::temp_dir().join(format!(
        "kain_ts_codegen_test_{}",
        SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("system time before UNIX_EPOCH")
            .as_nanos()
    ));
    fs::create_dir_all(&base).expect("failed to create temp test directory");

    let input_path = base.join("main.kn");
    fs::write(
        &input_path,
        r#"
fn add(a: Int, b: Int) -> Int:
    return a + b
"#,
    )
    .expect("failed to write test source");

    let output = Command::new(env!("CARGO_BIN_EXE_kain"))
        .arg("build")
        .arg(&input_path)
        .arg("--target")
        .arg("ts")
        .current_dir(&base)
        .output()
        .expect("failed to execute kain binary");

    assert!(
        output.status.success(),
        "kain build failed\nstdout:\n{}\nstderr:\n{}",
        String::from_utf8_lossy(&output.stdout),
        String::from_utf8_lossy(&output.stderr)
    );

    let generated_ts = input_path.with_extension("ts");
    assert!(
        generated_ts.exists(),
        "expected TypeScript output to exist at {}",
        generated_ts.display()
    );

    let generated = fs::read_to_string(&generated_ts).expect("failed to read generated ts output");
    assert!(
        generated.contains("export function add"),
        "expected generated TypeScript to include function signature, got:\n{}",
        generated
    );

    let _ = fs::remove_file(&generated_ts);
    let _ = fs::remove_file(&input_path);
    let _ = fs::remove_dir_all(&base);
}
