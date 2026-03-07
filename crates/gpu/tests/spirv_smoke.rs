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
    let tokens = Lexer::new(&full_source).tokenize().expect("tokenize failed");
    let mut ast = Parser::new(&tokens, &span_mapper, "<spirv-smoke>")
        .parse()
        .expect("parse failed");
    comptime::eval_program(&mut ast).expect("comptime failed");
    types::check(&ast, &span_mapper, "<spirv-smoke>").expect("typecheck failed")
}

fn compile_spirv(source: &str) -> Vec<u8> {
    let typed = typed_program_for_spirv(source);
    generate_spirv(&typed).expect("spirv generation failed")
}

fn assert_basic_spirv_shape(bytes: &[u8]) {
    assert!(bytes.len() > 16, "SPIR-V output too small");
    assert_eq!(&bytes[0..4], [0x03, 0x02, 0x23, 0x07], "invalid SPIR-V magic");
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
    let path = std::env::temp_dir().join(format!("kain_spirv_test_{}_{}.spv", sanitized, stamp));
    fs::write(&path, bytes).expect("failed to write temp spv file");
    path
}

fn run_spirv_val(case_name: &str, spv_path: &Path) {
    let Some(spirv_val) = resolve_spirv_val() else {
        eprintln!("[spirv-smoke] spirv-val not found; skipping external validation for {case_name}");
        return;
    };

    let output = Command::new(&spirv_val)
        .arg(spv_path)
        .output()
        .expect("failed to run spirv-val");

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        let stdout = String::from_utf8_lossy(&output.stdout);
        panic!(
            "spirv-val rejected case {case_name}\nstdout:\n{}\nstderr:\n{}",
            stdout,
            stderr,
        );
    }
}

fn assert_valid_spirv_case(case_name: &str, source: &str) {
    let bytes = compile_spirv(source);
    assert_basic_spirv_shape(&bytes);
    let spv_path = write_temp_spv(case_name, &bytes);
    run_spirv_val(case_name, &spv_path);
    let _ = fs::remove_file(spv_path);
}

#[test]
fn spirv_smoke_scalar_ctors_and_casts() {
    let src = r#"
shader compute scalar_ctor_smoke(id: UVec3) -> Vec4:
    uniform src: StorageBuffer<Float> @0
    uniform dst: StorageBuffer<Float> @1
    uniform LOCAL_SIZE_X: UInt @100
    uniform LOCAL_SIZE_Y: UInt @101
    uniform LOCAL_SIZE_Z: UInt @102

    let i = id.x
    let a = src[i]
    let ai = Int(a)
    let au = UInt(ai)
    let af = Float(au)
    let mask = Bool(af)
    let mf = Float(mask)
    dst[i] = af + mf
    return vec4(af, mf, 0.0, 1.0)
"#;

    assert_valid_spirv_case("scalar_ctor_smoke", src);
}

#[test]
fn spirv_smoke_complex_compute_flow() {
    let src = r#"
shader compute complex_flow_smoke(id: UVec3) -> Vec4:
    uniform in_a: StorageBuffer<Float> @0
    uniform in_b: StorageBuffer<Float> @1
    uniform out_main: StorageBuffer<Float> @2
    uniform out_debug: StorageBuffer<Float> @3
    uniform joint_count: UInt @4
    uniform damping: Float @5
    uniform gain: Float @6
    uniform floor_y: Float @7
    uniform dt: Float @8
    uniform eps: Float @9
    uniform LOCAL_SIZE_X: UInt @100
    uniform LOCAL_SIZE_Y: UInt @101
    uniform LOCAL_SIZE_Z: UInt @102

    let joint = id.x
    let base = joint * 4
    if joint >= joint_count:
        return vec4(0.0, 0.0, 0.0, 0.0)

    let cur = vec3(in_a[base + 0], in_a[base + 1], in_a[base + 2])
    let prev = vec3(in_b[base + 0], in_b[base + 1], in_b[base + 2])
    let vel = (cur - prev) / max(dt, eps)

    let mut accum = vec3(0.0, 0.0, 0.0)
    let mut weight = 0.0
    let mut k = 0
    while k < 3:
        let phase = k as Float * 0.5 + joint as Float * 0.01
        let wobble = vec3(sin(phase + cur.x), cos(phase + cur.y), sin(phase + cur.z))
        let mixed = normalize(vel + wobble + vec3(eps, eps, eps))
        let tap = 1.0 / (1.0 + k as Float)
        accum = accum + mixed * tap
        weight = weight + tap
        k = k + 1

    let blended = accum / max(weight, eps)
    let tangent = normalize(cross(vec3(0.0, 1.0, 0.0), blended) + vec3(eps, 0.0, 0.0))
    let bitangent = normalize(cross(blended, tangent))
    let axis_energy = abs(dot(tangent, bitangent))

    let damped = cur * damping + blended * gain
    let y_clamped = max(damped.y, floor_y)
    let y_lock = if length(vel) < 0.02: y_clamped else: damped.y
    let packed = vec4(damped.x, y_lock, damped.z, in_a[base + 3])

    out_main[base + 0] = packed.x
    out_main[base + 1] = packed.y
    out_main[base + 2] = packed.z
    out_main[base + 3] = packed.w

    // Exercise generalized swizzle lowering
    let dbg = vec4(blended.z, blended.x, blended.y, axis_energy)
    out_debug[base + 0] = dbg.x
    out_debug[base + 1] = dbg.y
    out_debug[base + 2] = dbg.z
    out_debug[base + 3] = dbg.w
    return dbg
"#;

    assert_valid_spirv_case("complex_flow_smoke", src);
}

#[test]
fn spirv_edge_case_if_expression_and_shadowed_locals() {
    let src = r#"
shader compute if_shadow_smoke(id: UVec3) -> Vec4:
    uniform src: StorageBuffer<Vec4> @0
    uniform dst: StorageBuffer<Vec4> @1
    uniform limit: UInt @2
    uniform eps: Float @3
    uniform gain: Float @4
    uniform LOCAL_SIZE_X: UInt @100
    uniform LOCAL_SIZE_Y: UInt @101
    uniform LOCAL_SIZE_Z: UInt @102

    let i = id.x
    if i >= limit:
        return vec4(0.0, 0.0, 0.0, 0.0)

    let sample = src[i].xyz
    let rotated = vec2(sample.x, sample.z)
    let rotated = if length(sample) > eps:
        normalize(sample + vec3(eps, 0.0, 0.0))
    else:
        vec3(0.0, 0.0, 0.0)

    let final_v = rotated * gain + vec3(rotated.x, 0.0, rotated.y)
    dst[i] = vec4(final_v.x, final_v.y, final_v.z, 1.0)
    return vec4(final_v.x, final_v.y, final_v.z, 1.0)
"#;

    assert_valid_spirv_case("if_shadow_smoke", src);
}

#[test]
fn spirv_edge_case_compute_builtin_aliases_and_uint_indexing() {
    let src = r#"
shader compute builtin_alias_smoke() -> Void:
    uniform src: StorageBuffer<Float> @0
    uniform dst: StorageBuffer<Float> @1
    uniform joint_count: UInt @2
    uniform LOCAL_SIZE_X: UInt @100
    uniform LOCAL_SIZE_Y: UInt @101
    uniform LOCAL_SIZE_Z: UInt @102

    let joint = dispatch_thread_id.x
    let lane = group_index
    let base = joint * 4
    if joint >= joint_count:
        return

    dst[base + 0] = src[base + 0]
    dst[base + 1] = src[base + 1] + lane as Float
    dst[base + 2] = src[base + 2]
    dst[base + 3] = src[base + 3]
    return
"#;

    assert_valid_spirv_case("builtin_alias_smoke", src);
}

#[test]
fn spirv_edge_case_storage_buffers_and_vector_scalar_builtins() {
    let src = r#"
shader compute storage_mix_smoke(id: UVec3) -> Vec4:
    uniform positions_a: StorageBuffer<Vec4> @0
    uniform positions_b: StorageBuffer<Vec4> @1
    uniform out_positions: StorageBuffer<Vec4> @2
    uniform alpha_per_joint: StorageBuffer<Float> @3
    uniform joint_count: UInt @4
    uniform alpha_bias: Float @5
    uniform eps: Float @6
    uniform LOCAL_SIZE_X: UInt @100
    uniform LOCAL_SIZE_Y: UInt @101
    uniform LOCAL_SIZE_Z: UInt @102

    let joint = id.x
    if joint >= joint_count:
        return vec4(0.0, 0.0, 0.0, 0.0)

    let a = positions_a[joint]
    let b = positions_b[joint]
    let raw_alpha = alpha_per_joint[joint] + alpha_bias
    let t = clamp(raw_alpha, 0.0, 1.0)
    let p = mix(a.xyz, b.xyz, t)
    let w = mix(a.w, b.w, t)
    let edge = step(eps, length(p))
    let boosted = smoothstep(0.0, 1.0, edge)
    let out_v = vec4(p.x, p.y, p.z, max(w, boosted))

    out_positions[joint] = out_v
    return out_v
"#;

    assert_valid_spirv_case("storage_mix_smoke", src);
}

#[test]
fn spirv_edge_case_loop_cfg_and_continue_paths() {
    let src = r#"
shader compute loop_cfg_smoke(id: UVec3) -> Vec4:
    uniform src: StorageBuffer<Vec4> @0
    uniform dst: StorageBuffer<Vec4> @1
    uniform count: UInt @2
    uniform eps: Float @3
    uniform LOCAL_SIZE_X: UInt @100
    uniform LOCAL_SIZE_Y: UInt @101
    uniform LOCAL_SIZE_Z: UInt @102

    let idx = id.x
    if idx >= count:
        return vec4(0.0, 0.0, 0.0, 0.0)

    let seed = src[idx].xyz
    let mut accum = vec3(0.0, 0.0, 0.0)
    let mut angle = 0.0
    while angle < 6.2831853:
        let dir = vec3(cos(angle), 0.0, sin(angle))
        accum = accum + normalize(seed + dir + vec3(eps, 0.0, 0.0))
        angle = angle + 1.5707963

    let mut total = vec3(0.0, 0.0, 0.0)
    for k in range(3):
        let wobble = vec3(k as Float, 0.0, 1.0)
        total = total + accum + wobble

    let out_v = vec4(total.x, total.y, total.z, 1.0)
    dst[idx] = out_v
    return out_v
"#;

    assert_valid_spirv_case("loop_cfg_smoke", src);
}

#[test]
fn spirv_edge_case_nested_conditionals_and_parent_indices() {
    let src = r#"
shader compute parent_chain_smoke() -> Void:
    uniform in_joints: StorageBuffer<Float> @0
    uniform parents: StorageBuffer<Int> @1
    uniform out_rotations: StorageBuffer<Float> @2
    uniform joint_count: UInt @3
    uniform eps: Float @4
    uniform LOCAL_SIZE_X: UInt @100
    uniform LOCAL_SIZE_Y: UInt @101
    uniform LOCAL_SIZE_Z: UInt @102

    let joint = dispatch_thread_id.x
    if joint >= joint_count:
        return

    let base = joint * 4
    let parent = parents[joint]
    if parent < 0:
        out_rotations[base + 0] = 0.0
        out_rotations[base + 1] = 0.0
        out_rotations[base + 2] = 0.0
        out_rotations[base + 3] = 1.0
        return

    let pidx = parent as UInt
    let pbase = pidx * 4
    let dx = in_joints[base + 0] - in_joints[pbase + 0]
    let dy = in_joints[base + 1] - in_joints[pbase + 1]
    let dz = in_joints[base + 2] - in_joints[pbase + 2]
    let forward = normalize(vec3(dx, dy, dz) + vec3(eps, 0.0, 0.0))
    let align = abs(dot(forward, vec3(0.0, 1.0, 0.0)))
    let up_ref = if align > 0.95: vec3(1.0, 0.0, 0.0) else: vec3(0.0, 1.0, 0.0)
    let right = normalize(cross(up_ref, forward) + vec3(eps, 0.0, 0.0))

    out_rotations[base + 0] = right.x
    out_rotations[base + 1] = right.y
    out_rotations[base + 2] = right.z
    out_rotations[base + 3] = 1.0
    return
"#;

    assert_valid_spirv_case("parent_chain_smoke", src);
}
