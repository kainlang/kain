use gpu::generate_spirv;
use kain_core::comptime;
use kain_core::diagnostics::SpanMapper;
use kain_core::stdlib;
use kain_core::types;
use kain_core::{CompileTarget, Lexer, Parser, TypedProgram};
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;
use std::time::{SystemTime, UNIX_EPOCH};

fn typed_program_for_spirv(source: &str) -> TypedProgram {
    let stdlib_src = stdlib::load_stdlib_for_target(CompileTarget::Spirv);
    let full_source = format!("{}\n{}", stdlib_src, source);
    let span_mapper = SpanMapper::new(&full_source);
    let tokens = Lexer::new(&full_source)
        .tokenize()
        .expect("tokenize failed");
    let mut ast = Parser::new(&tokens, &span_mapper, "<spirv-layout>")
        .parse()
        .expect("parse failed");
    comptime::eval_program(&mut ast).expect("comptime failed");
    types::check(&ast, &span_mapper, "<spirv-layout>").expect("typecheck failed")
}

fn compile_spirv(source: &str) -> Vec<u8> {
    let typed = typed_program_for_spirv(source);
    generate_spirv(&typed).expect("spirv generation failed")
}

fn resolve_spirv_val() -> Option<PathBuf> {
    if let Ok(explicit) = std::env::var("SPIRV_VAL_EXE") {
        let path = PathBuf::from(explicit);
        if path.exists() {
            return Some(path);
        }
    }

    let known = PathBuf::from(r"C:\VulkanSDK\1.4.341.1\Bin\spirv-val.exe");
    if known.exists() {
        return Some(known);
    }

    let output = Command::new("where").arg("spirv-val").output().ok()?;
    if !output.status.success() {
        return None;
    }

    let stdout = String::from_utf8_lossy(&output.stdout);
    let first = stdout
        .lines()
        .map(str::trim)
        .find(|line| !line.is_empty())?;
    Some(PathBuf::from(first))
}

fn write_temp_spv(case_name: &str, bytes: &[u8]) -> PathBuf {
    let stamp = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .expect("clock before unix epoch")
        .as_nanos();
    let sanitized = case_name
        .chars()
        .map(|ch| if ch.is_ascii_alphanumeric() { ch } else { '_' })
        .collect::<String>();
    let path = std::env::temp_dir().join(format!("kain_spirv_layout_{}_{}.spv", sanitized, stamp));
    fs::write(&path, bytes).expect("failed to write temp spv file");
    path
}

fn run_spirv_val(case_name: &str, spv_path: &Path) {
    let Some(spirv_val) = resolve_spirv_val() else {
        eprintln!(
            "[spirv-layout] spirv-val not found; skipping external validation for {case_name}"
        );
        return;
    };

    let output = Command::new(&spirv_val)
        .args(["--target-env", "vulkan1.3"])
        .arg(spv_path)
        .output()
        .expect("failed to run spirv-val");

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        let stdout = String::from_utf8_lossy(&output.stdout);
        panic!(
            "spirv-val rejected case {case_name}\nstdout:\n{}\nstderr:\n{}",
            stdout, stderr,
        );
    }
}

fn assert_valid_spirv_case(case_name: &str, source: &str) {
    let bytes = compile_spirv(source);
    assert!(bytes.len() > 16, "SPIR-V output too small");
    assert_eq!(
        &bytes[0..4],
        [0x03, 0x02, 0x23, 0x07],
        "invalid SPIR-V magic"
    );
    let spv_path = write_temp_spv(case_name, &bytes);
    run_spirv_val(case_name, &spv_path);
    let _ = fs::remove_file(spv_path);
}

#[test]
fn spirv_layout_vec3_storage_buffers_validate_for_vulkan() {
    let src = r#"
shader compute vec3_stride_layout_smoke(id: UVec3) -> Void:
    uniform src: StorageBuffer<Vec3> @0
    uniform dst: StorageBuffer<Vec3> @1
    uniform count: UInt @2
    uniform LOCAL_SIZE_X: UInt @100
    uniform LOCAL_SIZE_Y: UInt @101
    uniform LOCAL_SIZE_Z: UInt @102

    let idx = id.x
    if idx >= count:
        return

    let value = src[idx]
    dst[idx] = value
    return
"#;

    assert_valid_spirv_case("vec3_stride_layout_smoke", src);
}
