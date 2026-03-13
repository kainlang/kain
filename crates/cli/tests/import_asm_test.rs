use std::fs;
use std::process::Command;
use std::time::{SystemTime, UNIX_EPOCH};

#[test]
fn import_asm_command_generates_expected_artifacts() {
    let base = std::env::temp_dir().join(format!(
        "kain_import_asm_cmd_{}",
        SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("clock before epoch")
            .as_nanos()
    ));
    fs::create_dir_all(&base).expect("failed to create test directory");

    let input = base.join("furby_source.asm");
    fs::write(&input, "Start:\nLDA #10\nSTA PortA\nTable1: DB 10,20,30\n")
        .expect("failed to write input");

    let output = Command::new(env!("CARGO_BIN_EXE_kain"))
        .arg("import-asm")
        .arg(&input)
        .arg("--format")
        .arg("6502-furby")
        .current_dir(&base)
        .output()
        .expect("failed to run kain import-asm");

    assert!(
        output.status.success(),
        "import-asm failed\nstdout:\n{}\nstderr:\n{}",
        String::from_utf8_lossy(&output.stdout),
        String::from_utf8_lossy(&output.stderr)
    );

    assert!(base
        .join("Research")
        .join("furby")
        .join("furby_canonical.asm")
        .exists());
    assert!(base
        .join("Research")
        .join("furby")
        .join("furby_recovery_report.json")
        .exists());
    assert!(base.join("generated").join("furby_firmware.kn").exists());
    assert!(base.join("generated").join("furby_map.json").exists());
}
