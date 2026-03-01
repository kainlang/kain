use gpu::generate_spirv;
use kain_core::comptime;
use kain_core::diagnostics::SpanMapper;
use kain_core::stdlib;
use kain_core::types;
use kain_core::{CompileTarget, Lexer, Parser, TypedProgram};

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

    let bytes = compile_spirv(src);
    assert!(bytes.len() > 16, "SPIR-V output too small");
    assert_eq!(&bytes[0..4], [0x03, 0x02, 0x23, 0x07], "invalid SPIR-V magic");
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

    let bytes = compile_spirv(src);
    assert!(bytes.len() > 16, "SPIR-V output too small");
    assert_eq!(&bytes[0..4], [0x03, 0x02, 0x23, 0x07], "invalid SPIR-V magic");
}
