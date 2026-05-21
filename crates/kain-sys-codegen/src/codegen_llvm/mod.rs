//! LLVM IR Generator
//!
//! Generates textual LLVM IR (Intermediate Representation) which can be compiled
//! by `clang` or `llc`. This approach is chosen for maximum portability and
//! reliability without requiring local LLVM library linking during the build.

use kain_actor::native::NATIVE_ACTOR_NAME_MAX_BYTES;
use kain_core::ast::{
    AxiomPredicate, BinaryOp, Block, ConvergeSelector, ElseBranch, Expr, JSXAttrValue, JSXNode,
    Pattern, PulseDuration, Stmt, Type, UnaryOp, VariantPatternFields,
};
use kain_core::error::{KainError, KainResult};
use kain_core::types::{
    ResolvedType, TypedComponent, TypedConst, TypedFunction, TypedItem, TypedProgram,
};
use kain_core::Span;
use kain_core::{
    lower_typed_program_memory_for_target, validate_typed_program_memory_support, CompileTarget,
};
use std::collections::{HashMap, HashSet};

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
enum LlvmTargetId {
    WindowsX64Msvc,
    LinuxX64Gnu,
    MacOsArm64,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
struct LlvmTargetDescriptor {
    id: LlvmTargetId,
    triple: &'static str,
    datalayout: &'static str,
}

#[derive(Clone, Debug)]
struct WorldGlobalInfo {
    global_symbol: String,
    init_flag_symbol: String,
    init_fn_name: String,
}

#[derive(Clone, Debug)]
struct ConstGlobalInfo {
    global_symbol: String,
    init_flag_symbol: String,
    init_fn_name: String,
    ty: String,
    requires_runtime_init: bool,
    is_known_string: bool,
    string_byte_len: Option<usize>,
    string_literal: Option<String>,
}

#[derive(Clone, Debug)]
struct NativeEntangleBinding {
    authority: String,
    mirror: String,
    policy: String,
    type_name: String,
}

#[derive(Clone, Debug)]
struct NativeMachineAxiomInfo {
    name: String,
}

#[derive(Clone, Debug)]
struct NativePulseInfo {
    name: String,
    token: u64,
    interval_ns: u64,
    jitter_ns: u64,
}

#[derive(Clone, Debug)]
enum ShatteredArrayBacking {
    RuntimeHandle,
    StackLaneBuffers,
}

#[derive(Clone, Debug)]
struct ShatteredArrayLocal {
    struct_name: String,
    element_count: usize,
    lane_base_values: Vec<String>,
    backing: ShatteredArrayBacking,
}

#[derive(Clone, Debug)]
struct FixedArrayLocal {
    storage_reg: String,
    array_ty: String,
    element_ty: String,
    element_count: usize,
}

#[derive(Clone, Debug, Default)]
struct LiteralMapLocal {
    entries: HashMap<String, i64>,
}

#[derive(Clone, Copy, Debug)]
struct LoopIndexBounds {
    lower_inclusive: i64,
    upper_exclusive: i64,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum OwnershipPointerProvenance {
    ImportedOrUnknown,
    HelperOwned,
    EphemeralLocal,
}

#[derive(Clone, Debug)]
struct EphemeralOwnershipLocalWitness {
    storage_reg: String,
    storage_llvm_ty: String,
    storage_element_ty: String,
    storage_byte_len: i64,
    storage_alignment: i64,
}

#[derive(Clone, Copy, Debug)]
struct HelperAllocStorageLayout {
    element_count: i64,
    stride_bytes: i64,
    byte_len: i64,
    zeroed: bool,
}

#[derive(Clone, Debug)]
struct ForwardedMemSlot {
    value_reg: String,
    value_ty: String,
    nonnegative_i64: bool,
}

#[derive(Clone, Debug)]
struct JsonAnyArgument {
    value: String,
    release_after_call: bool,
}

const LLVM_TARGET_WINDOWS_X64_MSVC: LlvmTargetDescriptor = LlvmTargetDescriptor {
    id: LlvmTargetId::WindowsX64Msvc,
    triple: "x86_64-pc-windows-msvc",
    datalayout: "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128",
};

const LLVM_TARGET_LINUX_X64_GNU: LlvmTargetDescriptor = LlvmTargetDescriptor {
    id: LlvmTargetId::LinuxX64Gnu,
    triple: "x86_64-unknown-linux-gnu",
    datalayout: "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128",
};

const LLVM_TARGET_MACOS_ARM64: LlvmTargetDescriptor = LlvmTargetDescriptor {
    id: LlvmTargetId::MacOsArm64,
    triple: "arm64-apple-darwin",
    datalayout: "e-m:o-i64:64-i128:128-n32:64-S128",
};

const LLVM_TARGET_DESCRIPTOR_REGISTRY: &[LlvmTargetDescriptor] = &[
    LLVM_TARGET_WINDOWS_X64_MSVC,
    LLVM_TARGET_LINUX_X64_GNU,
    LLVM_TARGET_MACOS_ARM64,
];
const ABI_TAGGED_HEADER_BYTES: i64 = 16;
const ABI_TAG_OPTION_NONE_LLVM: i64 = 0;
const ABI_TAG_OPTION_SOME_LLVM: i64 = 1;
const ABI_TAG_RESULT_OK_LLVM: i64 = 2;
const ABI_TAG_RESULT_ERR_LLVM: i64 = 3;
const ABI_TAGGED_IMMEDIATE_MASK_LLVM: i64 = 7;
const ABI_TAGGED_IMMEDIATE_INT_MIN_LLVM: i64 = -(1i64 << 60);
const ABI_TAGGED_IMMEDIATE_INT_MAX_LLVM: i64 = (1i64 << 60) - 1;
const JSON_ANY_TAG_INT_LLVM: i64 = 1;
const JSON_ANY_TAG_BOOL_LLVM: i64 = 2;
const JSON_ANY_TAG_STRING_LLVM: i64 = 3;
const JSON_ANY_TAG_NULL_LLVM: i64 = 4;
const ABI_DEFAULT_ASK_TIMEOUT_MS_LLVM: i64 = 30_000;
const ACTOR_REF_LLVM_TYPE: &str = "%KainActorRef";
const REPLY_PORT_ACTOR_NAME: &str = "KainReplyPort";
const REPLY_PORT_LLVM_TYPE: &str = "%KainReplyPort";
const KAIN_CONVERGE_LANE_MAX_LLVM: usize = 8;

fn runtime_symbol_for_stdlib_function(name: &str) -> &str {
    match name {
        "floor" => "kain_floor_i64",
        "ceil" => "kain_ceil_i64",
        "round" => "kain_round_i64",
        _ => name,
    }
}

fn stdlib_function_uses_borrowed_string_param(name: &str, index: usize) -> bool {
    matches!(
        (name, index),
        ("map_get", 1)
            | ("len", 0)
            | ("trim", 0)
            | ("to_upper", 0)
            | ("to_lower", 0)
            | ("contains", 0 | 1)
            | ("replace", 0 | 1 | 2)
            | ("starts_with", 0 | 1)
            | ("ends_with", 0 | 1)
            | ("substring", 0)
            | ("find_substring_from", 0 | 1)
            | ("char_at", 0)
            | ("byte_at", 0)
            | ("ord", 0)
    )
}

fn kain_map_codegen_mix_u64(mut value: u64) -> u64 {
    value ^= value >> 33;
    value = value.wrapping_mul(0xff51_afd7_ed55_8ccd);
    value ^= value >> 33;
    value = value.wrapping_mul(0xc4ce_b9fe_1a85_ec53);
    value ^= value >> 33;
    value
}

fn kain_map_codegen_hash_bytes(bytes: &[u8]) -> u64 {
    const SEED: u64 = 0x9e37_79b9_7f4a_7c15;
    const STEP: u64 = 0x94d0_49bb_1331_11eb;
    let mut hash = SEED ^ ((bytes.len() as u64).wrapping_mul(STEP));
    let mut offset = 0usize;

    while offset + 8 <= bytes.len() {
        let mut chunk_bytes = [0u8; 8];
        chunk_bytes.copy_from_slice(&bytes[offset..offset + 8]);
        let chunk = u64::from_le_bytes(chunk_bytes);
        hash ^= kain_map_codegen_mix_u64(chunk.wrapping_add(SEED));
        hash = hash.rotate_left(27).wrapping_mul(STEP).wrapping_add(SEED);
        offset += 8;
    }

    let remaining = bytes.len() - offset;
    if remaining > 0 {
        let mut tail_bytes = [0u8; 8];
        tail_bytes[..remaining].copy_from_slice(&bytes[offset..]);
        let tail = u64::from_le_bytes(tail_bytes);
        hash ^= kain_map_codegen_mix_u64(tail ^ ((remaining as u64) << 56));
        hash = hash.rotate_left(27).wrapping_mul(STEP).wrapping_add(SEED);
    }

    kain_map_codegen_mix_u64(hash)
}

fn kain_map_codegen_magic_prefix_state(
    word0: u64,
    word1: u64,
    word2: u64,
    word3: u64,
    length: u64,
) -> u64 {
    const MAGIC: u64 = 0x6417_0d35_8aa1_15a1;
    const LANE1: u64 = 0x9e37_79b9_7f4a_7c15;
    const LANE2: u64 = 0xbf58_476d_1ce4_e5b9;
    const LANE3: u64 = 0x94d0_49bb_1331_11eb;
    const LANE4: u64 = 0xd6e8_feb8_6659_fd93;
    let folded0 = (word0 ^ length).wrapping_mul(MAGIC);
    let folded1 = (word1 ^ MAGIC.rotate_left(13)).wrapping_mul(LANE1);
    let folded2 = (word2 ^ MAGIC.rotate_left(27)).wrapping_mul(LANE2);
    let folded3 = (word3 ^ (MAGIC ^ LANE3)).wrapping_mul(LANE4);
    let state = folded0 ^ folded1 ^ folded2 ^ folded3;
    ((state ^ (state >> 33)).wrapping_mul(0xff51_afd7_ed55_8ccd)) ^ (state >> 29)
}

fn kain_map_codegen_static_key_metadata(key: &str) -> (u64, u64, u64) {
    let bytes = key.as_bytes();
    let key_length = bytes.len() as u64;
    let prefix_length = bytes.len().min(32);
    let mut prefix_bytes = [0u8; 32];
    prefix_bytes[..prefix_length].copy_from_slice(&bytes[..prefix_length]);
    let word0 = u64::from_le_bytes(
        prefix_bytes[0..8]
            .try_into()
            .expect("slice length is exact"),
    );
    let word1 = u64::from_le_bytes(
        prefix_bytes[8..16]
            .try_into()
            .expect("slice length is exact"),
    );
    let word2 = u64::from_le_bytes(
        prefix_bytes[16..24]
            .try_into()
            .expect("slice length is exact"),
    );
    let word3 = u64::from_le_bytes(
        prefix_bytes[24..32]
            .try_into()
            .expect("slice length is exact"),
    );
    let key_hash = kain_map_codegen_hash_bytes(bytes);
    let key_prefix = kain_map_codegen_magic_prefix_state(word0, word1, word2, word3, key_length);
    (key_length, key_hash, key_prefix)
}

fn llvm_runtime_declaration_is_preemitted(name: &str) -> bool {
    matches!(
        name,
        "abi_cpu_feature_mask"
            | "abi_cpu_capability_mask_for_key"
            | "abi_entangle_register"
            | "abi_converge_select_lane_for_key"
            | "abi_converge_record_telemetry"
            | "kain_machine_pulse_total_fire_count"
    )
}

fn llvm_orchestrate_trace_enabled() -> bool {
    if let Ok(value) = std::env::var("KAIN_LLVM_ORCHESTRATE_TRACE") {
        let value = value.trim();
        if value.eq_ignore_ascii_case("1")
            || value.eq_ignore_ascii_case("true")
            || value.eq_ignore_ascii_case("yes")
            || value.eq_ignore_ascii_case("on")
        {
            return true;
        }
        if value.eq_ignore_ascii_case("0")
            || value.eq_ignore_ascii_case("false")
            || value.eq_ignore_ascii_case("no")
            || value.eq_ignore_ascii_case("off")
        {
            return false;
        }
    }

    !matches!(
        std::env::var("KAIN_NATIVE_PROFILE").as_deref(),
        Ok("benchmark-release")
    )
}

fn resolve_host_llvm_target_descriptor() -> &'static LlvmTargetDescriptor {
    let target_id = if cfg!(all(target_os = "windows", target_arch = "x86_64")) {
        LlvmTargetId::WindowsX64Msvc
    } else if cfg!(all(target_os = "linux", target_arch = "x86_64")) {
        LlvmTargetId::LinuxX64Gnu
    } else if cfg!(all(target_os = "macos", target_arch = "aarch64")) {
        LlvmTargetId::MacOsArm64
    } else {
        LlvmTargetId::WindowsX64Msvc
    };

    LLVM_TARGET_DESCRIPTOR_REGISTRY
        .iter()
        .find(|descriptor| descriptor.id == target_id)
        .unwrap_or(&LLVM_TARGET_WINDOWS_X64_MSVC)
}

pub fn generate(program: &TypedProgram) -> KainResult<Vec<u8>> {
    let lowered = lower_typed_program_memory_for_target(program, CompileTarget::Llvm)?;
    validate_typed_program_memory_support(&lowered, CompileTarget::Llvm)?;
    let mut gen = LlvmGenerator::new();
    gen.collect_original_pointer_let_type_hints(program);
    gen.compile_module(&lowered)?;
    Ok(gen.output.into_bytes())
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum ManualFindSubstringMissBehavior {
    NegativeOne,
    HaystackLength,
}

struct LlvmGenerator {
    output: String,
    reg_count: usize,
    label_count: usize,
    /// Maps variable names to (stack_ptr, type)
    locals: HashMap<String, (String, String)>,
    /// Pointer-like locals whose provenance is solver-proved helper-owned and may
    /// use the helper-header ownership fast path without imported registration.
    helper_owned_pointer_locals: HashMap<String, OwnershipPointerProvenance>,
    /// Fresh bounded helper locals whose runtime protocol can stay in the
    /// compiler because the buffer never escapes its local block.
    ephemeral_owned_pointer_locals: HashMap<String, EphemeralOwnershipLocalWitness>,
    /// Precomputed block-local candidates for ephemeral helper-buffer erasure.
    ephemeral_candidate_scopes: Vec<HashSet<String>>,
    /// Ephemeral locals whose fresh zero-fill is provably dead because the first
    /// ownership use is a full-width dominating store before any read.
    ephemeral_zero_init_elision_scopes: Vec<HashSet<String>>,
    /// Block-scoped i64 literals known to the compiler for nested ephemeral
    /// candidate nomination.
    known_i64_literal_scopes: Vec<HashMap<String, i64>>,
    /// Block-scoped integer locals whose current value is proven non-negative.
    /// This lets hot benchmark-style loops strength-reduce signed power-of-two
    /// div/rem into shift/mask without changing negative-number semantics.
    known_nonnegative_i64_scopes: Vec<HashSet<String>>,
    /// Block-scoped stack-buffer store/load forwarding for compiler-owned
    /// ephemeral memory. The SMT proof is in
    /// `crates/kain-sys-codegen/z3/proofs-experimental/packed_wire_store_load_forwarding.smt2`.
    forwarded_mem_slot_scopes: Vec<HashMap<String, ForwardedMemSlot>>,
    /// Locals whose array literal can stay as an LLVM fixed array because every
    /// remaining use is a len() query or an index load.
    fixed_array_candidate_scopes: Vec<HashSet<String>>,
    /// Locals whose shattered array literal never escapes field-projection /
    /// len-only use, so LLVM can keep each shatter lane as an entry-block stack
    /// buffer instead of routing through the runtime heap handle.
    stack_shatter_candidate_scopes: Vec<HashSet<String>>,
    /// Locals whose `map_new` seed and all remaining uses stay inside a closed
    /// literal-key / literal-value lane, so `map_get` can collapse to direct
    /// constants without changing runtime-visible semantics.
    literal_map_candidate_scopes: Vec<HashSet<String>>,
    /// Locals that are borrowed views and must not be released on scope exit.
    borrowed_locals: HashSet<String>,
    /// i64 locals that carry native JSON handles or JSON-tagged Any values.
    json_handle_locals: HashSet<String>,
    /// Maps function names to return type
    functions: HashMap<String, String>,
    /// Tracks function parameter LLVM types for call-site lowering.
    function_params: HashMap<String, Vec<String>>,
    /// Tracks which direct-call parameters are language-level strings and can be
    /// treated as borrowed aliases instead of refcount-owned call-frame locals.
    string_function_params: HashMap<String, Vec<bool>>,
    /// Functions that were emitted as extern declarations.
    extern_functions: HashSet<String>,
    /// Callable names defined by the lowered program itself, including target stdlib wrappers.
    defined_functions: HashSet<String>,
    /// User-authored substring-search helpers whose bodies match the canonical
    /// byte-search loop and can lower to the native length-aware search helper.
    manual_find_substring_functions: HashMap<String, ManualFindSubstringMissBehavior>,
    /// Zero-arg functions whose entire Future result is a compile-time visible
    /// immediate async payload, so `await f()` can inline the payload directly.
    immediate_ready_future_payloads: HashMap<String, Expr>,
    /// Maps string content to global variable name
    strings: HashMap<String, String>,
    string_counter: usize,
    /// Stack of (continue_label, break_label) for loops
    loop_stack: Vec<(String, String)>,
    /// Stack of scopes, each containing list of variable names declared in that scope
    scopes: Vec<Vec<String>>,
    /// Struct definitions: Name -> Vec<(FieldName, Type)>
    struct_defs: HashMap<String, Vec<(String, String)>>,
    /// Ordinary POD structs and POD tuples that can safely travel by value.
    value_aggregate_structs: HashSet<String>,
    component_defs: HashMap<String, Vec<(String, String)>>,
    /// Current basic block label (for Phi nodes)
    current_block: String,
    current_return_type: Option<String>,
    actor_return_label: Option<String>,
    actor_return_slot: Option<String>,
    target: &'static LlvmTargetDescriptor,
    world_globals: HashMap<String, WorldGlobalInfo>,
    const_globals: HashMap<String, ConstGlobalInfo>,
    string_locals: HashSet<String>,
    string_length_values: HashMap<String, String>,
    pooled_string_literal_slots: HashMap<String, String>,
    native_entanglements: Vec<NativeEntangleBinding>,
    native_machine_axioms: Vec<NativeMachineAxiomInfo>,
    native_pulses: Vec<NativePulseInfo>,
    shattered_structs: HashSet<String>,
    shattered_array_locals: HashMap<String, ShatteredArrayLocal>,
    fixed_array_locals: HashMap<String, FixedArrayLocal>,
    sealed_literal_map_locals: HashMap<String, LiteralMapLocal>,
    active_loop_index_bounds: Vec<HashMap<String, LoopIndexBounds>>,
    current_patch_name: Option<String>,
    const_init_blocks: HashMap<String, String>,
    /// Byte offset immediately after the current function's `entry:` label.
    ///
    /// LLVM permits `alloca` outside the entry block, but those allocations
    /// execute every time control reaches them. Long-running Kain loops such as
    /// native UI frame loops must reuse fixed local slots instead of consuming
    /// more stack every iteration.
    entry_alloca_insert_offset: Option<usize>,
    /// Byte offset after entry allocas where one-time entry preamble work such
    /// as hoisted const initializers can be inserted.
    entry_preamble_insert_offset: Option<usize>,
    /// Top-level const globals whose runtime init was already hoisted into the
    /// current function entry preamble.
    entry_hoisted_const_inits: HashSet<String>,
    /// Original authored pointer/ref let declarations keyed by let-statement
    /// span so LLVM-only post-lowering fast paths can recover pointee element
    /// intent after low-level memory normalization erases ptr<T> into Int.
    original_pointer_let_types: HashMap<Span, Type>,
}

impl LlvmGenerator {
    fn new() -> Self {
        Self {
            output: String::new(),
            reg_count: 0,
            label_count: 0,
            locals: HashMap::new(),
            helper_owned_pointer_locals: HashMap::new(),
            ephemeral_owned_pointer_locals: HashMap::new(),
            ephemeral_candidate_scopes: Vec::new(),
            ephemeral_zero_init_elision_scopes: Vec::new(),
            known_i64_literal_scopes: Vec::new(),
            known_nonnegative_i64_scopes: Vec::new(),
            forwarded_mem_slot_scopes: Vec::new(),
            fixed_array_candidate_scopes: Vec::new(),
            stack_shatter_candidate_scopes: Vec::new(),
            literal_map_candidate_scopes: Vec::new(),
            borrowed_locals: HashSet::new(),
            json_handle_locals: HashSet::new(),
            functions: HashMap::new(),
            function_params: HashMap::new(),
            string_function_params: HashMap::new(),
            extern_functions: HashSet::new(),
            defined_functions: HashSet::new(),
            manual_find_substring_functions: HashMap::new(),
            immediate_ready_future_payloads: HashMap::new(),
            strings: HashMap::new(),
            string_counter: 0,
            loop_stack: Vec::new(),
            scopes: Vec::new(),
            struct_defs: HashMap::new(),
            value_aggregate_structs: HashSet::new(),
            component_defs: HashMap::new(),
            current_block: "entry".to_string(),
            current_return_type: None,
            actor_return_label: None,
            actor_return_slot: None,
            target: resolve_host_llvm_target_descriptor(),
            world_globals: HashMap::new(),
            const_globals: HashMap::new(),
            string_locals: HashSet::new(),
            string_length_values: HashMap::new(),
            pooled_string_literal_slots: HashMap::new(),
            native_entanglements: Vec::new(),
            native_machine_axioms: Vec::new(),
            native_pulses: Vec::new(),
            shattered_structs: HashSet::new(),
            shattered_array_locals: HashMap::new(),
            fixed_array_locals: HashMap::new(),
            sealed_literal_map_locals: HashMap::new(),
            active_loop_index_bounds: Vec::new(),
            current_patch_name: None,
            const_init_blocks: HashMap::new(),
            entry_alloca_insert_offset: None,
            entry_preamble_insert_offset: None,
            entry_hoisted_const_inits: HashSet::new(),
            original_pointer_let_types: HashMap::new(),
        }
    }

    fn collect_original_pointer_let_type_hints(&mut self, program: &TypedProgram) {
        self.original_pointer_let_types.clear();
        for item in &program.items {
            self.collect_pointer_let_types_from_typed_item(item);
        }
    }

    fn collect_pointer_let_types_from_typed_item(&mut self, item: &TypedItem) {
        match item {
            TypedItem::Function(function) => {
                self.collect_pointer_let_types_from_block(&function.ast.body)
            }
            TypedItem::Patch(patch) => self.collect_pointer_let_types_from_block(&patch.ast.body),
            TypedItem::Law(law) => self.collect_pointer_let_types_from_block(&law.ast.body),
            TypedItem::Converge(converge) => {
                self.collect_pointer_let_types_from_block(&converge.ast.spec_lane.body);
                for lane in &converge.ast.fast_lanes {
                    self.collect_pointer_let_types_from_block(&lane.body);
                }
            }
            TypedItem::Orchestrate(orchestrate) => {
                self.collect_pointer_let_types_from_block(&orchestrate.ast.body);
            }
            TypedItem::Pulse(pulse) => self.collect_pointer_let_types_from_block(&pulse.ast.body),
            TypedItem::Component(component) => {
                for method in &component.ast.methods {
                    self.collect_pointer_let_types_from_block(&method.body);
                }
            }
            TypedItem::Shader(shader) => {
                self.collect_pointer_let_types_from_block(&shader.ast.body)
            }
            TypedItem::Actor(actor) => {
                for handler in &actor.ast.handlers {
                    self.collect_pointer_let_types_from_block(&handler.body);
                }
                for method in &actor.ast.methods {
                    self.collect_pointer_let_types_from_block(&method.body);
                }
            }
            TypedItem::Struct(struct_item) => {
                for method in &struct_item.ast.methods {
                    self.collect_pointer_let_types_from_block(&method.body);
                }
            }
            TypedItem::Impl(imp) => {
                for method in &imp.ast.methods {
                    self.collect_pointer_let_types_from_block(&method.body);
                }
            }
            TypedItem::Test(test) => self.collect_pointer_let_types_from_block(&test.ast.body),
            TypedItem::Mod(module) => {
                for child in &module.items {
                    self.collect_pointer_let_types_from_typed_item(child);
                }
            }
            _ => {}
        }
    }

    fn collect_pointer_let_types_from_item(&mut self, item: &kain_core::ast::Item) {
        match item {
            kain_core::ast::Item::Function(function) => {
                self.collect_pointer_let_types_from_block(&function.body);
            }
            kain_core::ast::Item::Patch(patch) => {
                self.collect_pointer_let_types_from_block(&patch.body)
            }
            kain_core::ast::Item::Law(law) => self.collect_pointer_let_types_from_block(&law.body),
            kain_core::ast::Item::Converge(converge) => {
                self.collect_pointer_let_types_from_block(&converge.spec_lane.body);
                for lane in &converge.fast_lanes {
                    self.collect_pointer_let_types_from_block(&lane.body);
                }
            }
            kain_core::ast::Item::Orchestrate(orchestrate) => {
                self.collect_pointer_let_types_from_block(&orchestrate.body);
            }
            kain_core::ast::Item::Pulse(pulse) => {
                self.collect_pointer_let_types_from_block(&pulse.body)
            }
            kain_core::ast::Item::Component(component) => {
                for method in &component.methods {
                    self.collect_pointer_let_types_from_block(&method.body);
                }
            }
            kain_core::ast::Item::Shader(shader) => {
                self.collect_pointer_let_types_from_block(&shader.body)
            }
            kain_core::ast::Item::Actor(actor) => {
                for handler in &actor.handlers {
                    self.collect_pointer_let_types_from_block(&handler.body);
                }
                for method in &actor.methods {
                    self.collect_pointer_let_types_from_block(&method.body);
                }
            }
            kain_core::ast::Item::Struct(struct_item) => {
                for method in &struct_item.methods {
                    self.collect_pointer_let_types_from_block(&method.body);
                }
            }
            kain_core::ast::Item::Impl(imp) => {
                for method in &imp.methods {
                    self.collect_pointer_let_types_from_block(&method.body);
                }
            }
            kain_core::ast::Item::Mod(module) => {
                if let Some(items) = &module.inline {
                    for child in items {
                        self.collect_pointer_let_types_from_item(child);
                    }
                }
            }
            kain_core::ast::Item::Comptime(comptime) => {
                self.collect_pointer_let_types_from_block(&comptime.body);
            }
            kain_core::ast::Item::Test(test) => {
                self.collect_pointer_let_types_from_block(&test.body)
            }
            _ => {}
        }
    }

    fn collect_pointer_let_types_from_block(&mut self, block: &Block) {
        for stmt in &block.stmts {
            self.collect_pointer_let_types_from_stmt(stmt);
        }
    }

    fn collect_pointer_let_types_from_stmt(&mut self, stmt: &Stmt) {
        match stmt {
            Stmt::Let {
                ty, value, span, ..
            } => {
                if let Some(original_ty) = ty {
                    if matches!(original_ty, Type::Ptr { .. } | Type::Ref { .. }) {
                        self.original_pointer_let_types
                            .insert(*span, original_ty.clone());
                    }
                }
                if let Some(value) = value {
                    self.collect_pointer_let_types_from_expr(value);
                }
            }
            Stmt::Expr(expr) => self.collect_pointer_let_types_from_expr(expr),
            Stmt::Return(value, _) | Stmt::Break(value, _) => {
                if let Some(value) = value {
                    self.collect_pointer_let_types_from_expr(value);
                }
            }
            Stmt::Continue(_) => {}
            Stmt::For { iter, body, .. } => {
                self.collect_pointer_let_types_from_expr(iter);
                self.collect_pointer_let_types_from_block(body);
            }
            Stmt::While {
                condition, body, ..
            } => {
                self.collect_pointer_let_types_from_expr(condition);
                self.collect_pointer_let_types_from_block(body);
            }
            Stmt::Loop { body, .. } => self.collect_pointer_let_types_from_block(body),
            Stmt::Item(item) => self.collect_pointer_let_types_from_item(item),
        }
    }

    fn collect_pointer_let_types_from_else_branch(&mut self, else_branch: &ElseBranch) {
        match else_branch {
            ElseBranch::Else(block) => self.collect_pointer_let_types_from_block(block),
            ElseBranch::ElseIf(condition, block, next) => {
                self.collect_pointer_let_types_from_expr(condition);
                self.collect_pointer_let_types_from_block(block);
                if let Some(next) = next.as_deref() {
                    self.collect_pointer_let_types_from_else_branch(next);
                }
            }
        }
    }

    fn collect_pointer_let_types_from_expr(&mut self, expr: &Expr) {
        match expr {
            Expr::FString(parts, _) | Expr::Array(parts, _) | Expr::Tuple(parts, _) => {
                for part in parts {
                    self.collect_pointer_let_types_from_expr(part);
                }
            }
            Expr::MacroCall { args, .. } => {
                for arg in args {
                    self.collect_pointer_let_types_from_expr(arg);
                }
            }
            Expr::Binary { left, right, .. } => {
                self.collect_pointer_let_types_from_expr(left);
                self.collect_pointer_let_types_from_expr(right);
            }
            Expr::Unary { operand, .. }
            | Expr::Deref(operand, _)
            | Expr::Try(operand, _)
            | Expr::Await(operand, _)
            | Expr::AsyncBlock(operand, _)
            | Expr::Comptime(operand, _)
            | Expr::Paren(operand, _) => self.collect_pointer_let_types_from_expr(operand),
            Expr::Call { callee, args, .. } => {
                self.collect_pointer_let_types_from_expr(callee);
                for arg in args {
                    self.collect_pointer_let_types_from_expr(&arg.value);
                }
            }
            Expr::StageCall { args, .. } => {
                for arg in args {
                    self.collect_pointer_let_types_from_expr(&arg.value);
                }
            }
            Expr::MethodCall { receiver, args, .. } => {
                self.collect_pointer_let_types_from_expr(receiver);
                for arg in args {
                    self.collect_pointer_let_types_from_expr(&arg.value);
                }
            }
            Expr::Field { object, .. } => self.collect_pointer_let_types_from_expr(object),
            Expr::Index { object, index, .. } => {
                self.collect_pointer_let_types_from_expr(object);
                self.collect_pointer_let_types_from_expr(index);
            }
            Expr::Assign { target, value, .. } => {
                self.collect_pointer_let_types_from_expr(target);
                self.collect_pointer_let_types_from_expr(value);
            }
            Expr::Struct { fields, rest, .. } => {
                for (_, value) in fields {
                    self.collect_pointer_let_types_from_expr(value);
                }
                if let Some(rest) = rest {
                    self.collect_pointer_let_types_from_expr(rest);
                }
            }
            Expr::AggregateInit { fields, .. } => {
                for (_, value) in fields {
                    self.collect_pointer_let_types_from_expr(value);
                }
            }
            Expr::EnumVariant { fields, .. } => match fields {
                kain_core::ast::EnumVariantFields::Unit => {}
                kain_core::ast::EnumVariantFields::Tuple(values) => {
                    for value in values {
                        self.collect_pointer_let_types_from_expr(value);
                    }
                }
                kain_core::ast::EnumVariantFields::Struct(fields) => {
                    for (_, value) in fields {
                        self.collect_pointer_let_types_from_expr(value);
                    }
                }
            },
            Expr::Range { start, end, .. } => {
                if let Some(start) = start.as_deref() {
                    self.collect_pointer_let_types_from_expr(start);
                }
                if let Some(end) = end.as_deref() {
                    self.collect_pointer_let_types_from_expr(end);
                }
            }
            Expr::If {
                condition,
                then_branch,
                else_branch,
                ..
            } => {
                self.collect_pointer_let_types_from_expr(condition);
                self.collect_pointer_let_types_from_block(then_branch);
                if let Some(else_branch) = else_branch.as_deref() {
                    self.collect_pointer_let_types_from_else_branch(else_branch);
                }
            }
            Expr::Match {
                scrutinee, arms, ..
            } => {
                self.collect_pointer_let_types_from_expr(scrutinee);
                for arm in arms {
                    if let Some(guard) = &arm.guard {
                        self.collect_pointer_let_types_from_expr(guard);
                    }
                    self.collect_pointer_let_types_from_expr(&arm.body);
                }
            }
            Expr::Lambda { body, .. } => self.collect_pointer_let_types_from_expr(body),
            Expr::Ref { value, .. } | Expr::AddrOf { value, .. } | Expr::Cast { value, .. } => {
                self.collect_pointer_let_types_from_expr(value)
            }
            Expr::PtrOffset {
                pointer, offset, ..
            } => {
                self.collect_pointer_let_types_from_expr(pointer);
                self.collect_pointer_let_types_from_expr(offset);
            }
            Expr::MemLoad { pointer, .. } => self.collect_pointer_let_types_from_expr(pointer),
            Expr::MemStore { pointer, value, .. } => {
                self.collect_pointer_let_types_from_expr(pointer);
                self.collect_pointer_let_types_from_expr(value);
            }
            Expr::Alloc { size, .. } => self.collect_pointer_let_types_from_expr(size),
            Expr::Realloc { pointer, size, .. } => {
                self.collect_pointer_let_types_from_expr(pointer);
                self.collect_pointer_let_types_from_expr(size);
            }
            Expr::Observe { target, body, .. } | Expr::Collapse { target, body, .. } => {
                self.collect_pointer_let_types_from_expr(target);
                self.collect_pointer_let_types_from_expr(body);
            }
            Expr::Decay { target, .. } => self.collect_pointer_let_types_from_expr(target),
            Expr::Teleport { value, .. } => self.collect_pointer_let_types_from_expr(value),
            Expr::Spawn { init, .. } | Expr::SendMsg { data: init, .. } => {
                if let Expr::SendMsg { target, .. } = expr {
                    self.collect_pointer_let_types_from_expr(target);
                }
                for (_, value) in init {
                    self.collect_pointer_let_types_from_expr(value);
                }
            }
            Expr::Block(block, _) => self.collect_pointer_let_types_from_block(block),
            Expr::JSX(_, _)
            | Expr::Int(_, _)
            | Expr::Float(_, _)
            | Expr::String(_, _)
            | Expr::Bool(_, _)
            | Expr::None(_)
            | Expr::Ident(_, _)
            | Expr::SizeOfType { .. }
            | Expr::AlignOfType { .. }
            | Expr::Alloca { .. }
            | Expr::Uninit { .. }
            | Expr::Continue(_) => {}
            Expr::Return(value, _) | Expr::Break(value, _) => {
                if let Some(value) = value.as_deref() {
                    self.collect_pointer_let_types_from_expr(value);
                }
            }
        }
    }

    fn emit(&mut self, s: &str) {
        self.output.push_str(s);
        self.output.push('\n');
    }

    fn emit_label(&mut self, label: &str) {
        self.emit(&format!("{}:", label));
        self.current_block = label.to_string();
        if label == "entry" {
            self.entry_alloca_insert_offset = Some(self.output.len());
            self.entry_preamble_insert_offset = Some(self.output.len());
        }
    }

    fn emit_entry_alloca(&mut self, target: &str, ty: &str) {
        let line = format!("  {} = alloca {}\n", target, ty);
        if let Some(offset) = self.entry_alloca_insert_offset {
            self.output.insert_str(offset, &line);
            self.entry_alloca_insert_offset = Some(offset + line.len());
            if let Some(preamble_offset) = self.entry_preamble_insert_offset {
                self.entry_preamble_insert_offset = Some(preamble_offset + line.len());
            }
        } else {
            self.output.push_str(&line);
        }
    }

    fn emit_entry_preamble_line(&mut self, line: &str) {
        let line = format!("  {}\n", line);
        if let Some(offset) = self.entry_preamble_insert_offset {
            self.output.insert_str(offset, &line);
            self.entry_preamble_insert_offset = Some(offset + line.len());
        } else {
            self.output.push_str(&line);
        }
    }

    fn next_reg(&mut self) -> String {
        let r = format!("%r{}", self.reg_count);
        self.reg_count += 1;
        r
    }

    fn next_label(&mut self) -> String {
        let l = format!("L{}", self.label_count);
        self.label_count += 1;
        l
    }

    fn sanitize_type_fragment(fragment: &str) -> String {
        fragment
            .chars()
            .map(|ch| if ch.is_ascii_alphanumeric() { ch } else { '_' })
            .collect()
    }

    fn sanitize_symbol_fragment(fragment: &str) -> String {
        let mut sanitized = String::new();
        for byte in fragment.bytes() {
            match byte {
                b'a'..=b'z' | b'A'..=b'Z' | b'0'..=b'9' | b'_' => sanitized.push(byte as char),
                _ => sanitized.push_str(&format!("_{:02X}", byte)),
            }
        }
        if sanitized.is_empty() {
            "unnamed".to_string()
        } else {
            sanitized
        }
    }

    fn stable_runtime_hash64(text: &str) -> u64 {
        let mut hash = 0xcbf29ce484222325u64;
        for byte in text.bytes() {
            hash ^= byte as u64;
            hash = hash.wrapping_mul(0x100000001b3u64);
        }
        hash
    }

    fn llvm_i64_literal_for_u64(value: u64) -> String {
        (value as i64).to_string()
    }

    fn machine_pulse_body_symbol(name: &str) -> String {
        format!("__kain_pulse_body_{}", Self::sanitize_symbol_fragment(name))
    }

    fn machine_pulse_fire_symbol(name: &str) -> String {
        format!("__kain_pulse_fire_{}", Self::sanitize_symbol_fragment(name))
    }

    fn machine_axiom_symbol(name: &str) -> String {
        format!(
            "__kain_axiom_accept_{}",
            Self::sanitize_symbol_fragment(name)
        )
    }

    fn machine_pulse_duration_ns(duration: &PulseDuration) -> u64 {
        let value = duration.value.max(0) as u64;
        match duration.unit.as_str() {
            "s" => value.saturating_mul(1_000_000_000),
            "ms" => value.saturating_mul(1_000_000),
            "us" => value.saturating_mul(1_000),
            "ns" | "tick" | "ticks" => value,
            _ => value,
        }
    }

    fn machine_axiom_capability_bit(capability: &str) -> u64 {
        match capability {
            "atomic.bitmask" => 1u64 << 0,
            "time.hardware-timer" | "time.pulse" => 1u64 << 1,
            "memory.shatter" => 1u64 << 2,
            "world.teleport" | "interop.zero-copy-handoff" => 1u64 << 3,
            "cpu.x86.sse2" | "x86.sse2" | "sse2" => 1u64 << 8,
            "cpu.x86.avx" | "x86.avx" | "avx" => 1u64 << 9,
            "cpu.x86.avx2" | "x86.avx2" | "avx2" => 1u64 << 10,
            "cpu.x86.avx512" | "cpu.x86.avx512f" | "x86.avx512" | "x86.avx512f" | "avx512"
            | "avx512f" => 1u64 << 11,
            _ => 0,
        }
    }

    fn converge_capability_is_cpu_selector(capability: &str) -> bool {
        capability.starts_with("cpu.")
            || capability.starts_with("x86.")
            || matches!(
                capability,
                "sse2"
                    | "avx"
                    | "avx2"
                    | "avx512"
                    | "avx512f"
                    | "avx512dq"
                    | "avx512bw"
                    | "avx512vl"
                    | "fma"
                    | "bmi2"
            )
    }

    fn converge_selector_static_eligibility(selector: Option<&ConvergeSelector>) -> Option<bool> {
        match selector {
            None => Some(true),
            Some(ConvergeSelector::Target(target)) => Some(matches!(
                target.as_str(),
                "llvm" | "native" | "native.llvm" | "compiled" | "kain.native"
            )),
            Some(ConvergeSelector::Capability(capability)) => {
                if Self::converge_capability_is_cpu_selector(capability) {
                    None
                } else {
                    Some(true)
                }
            }
        }
    }

    fn tuple_struct_name_from_types(field_tys: &[String]) -> String {
        let mut name = String::from("__kain_tuple");
        for field_ty in field_tys {
            name.push('_');
            name.push_str(&Self::sanitize_type_fragment(field_ty));
        }
        name
    }

    fn tuple_struct_storage_type_from_types(&self, field_tys: &[String]) -> String {
        let tuple_name = Self::tuple_struct_name_from_types(field_tys);
        if self.value_aggregate_structs.contains(&tuple_name) {
            format!("%{}", tuple_name)
        } else {
            format!("%{}*", tuple_name)
        }
    }

    fn builtin_named_tuple_storage_type(&self, name: &str) -> Option<String> {
        let lanes = match name {
            "Vec2" => 2usize,
            "Vec3" => 3usize,
            "Vec4" => 4usize,
            _ => return None,
        };
        let field_tys = vec!["double".to_string(); lanes];
        Some(self.tuple_struct_storage_type_from_types(&field_tys))
    }

    fn tuple_field_alias_index(field: &str) -> Option<usize> {
        match field {
            "x" | "r" => Some(0),
            "y" | "g" => Some(1),
            "z" | "b" => Some(2),
            "w" | "a" => Some(3),
            _ => field
                .strip_prefix("_")
                .or_else(|| field.strip_prefix("__kain_tuple_"))
                .and_then(|index| index.parse::<usize>().ok()),
        }
    }

    fn llvm_type_is_scalar_value_aggregate_pod(&self, ty: &str) -> bool {
        match ty {
            "i1" | "i8" | "i32" | "i64" | "double" => true,
            _ if ty.starts_with('%') && !ty.ends_with('*') => {
                let struct_name = ty.trim_start_matches('%');
                self.value_aggregate_structs.contains(struct_name)
            }
            _ => false,
        }
    }

    fn resolved_type_is_scalar_value_aggregate_pod(&self, ty: &ResolvedType) -> bool {
        match ty {
            ResolvedType::Int(_)
            | ResolvedType::Float(_)
            | ResolvedType::Bool
            | ResolvedType::Char => true,
            ResolvedType::Tuple(items) => items
                .iter()
                .all(|item| self.resolved_type_is_scalar_value_aggregate_pod(item)),
            ResolvedType::Struct(name, _) => self.value_aggregate_structs.contains(name),
            _ => false,
        }
    }

    fn struct_storage_type(&self, name: &str) -> String {
        if self.value_aggregate_structs.contains(name) {
            format!("%{}", name)
        } else {
            format!("%{}*", name)
        }
    }

    fn register_tuple_struct(&mut self, field_tys: Vec<String>) -> String {
        let name = Self::tuple_struct_name_from_types(&field_tys);
        if !self.struct_defs.contains_key(&name) {
            let fields = field_tys
                .iter()
                .enumerate()
                .map(|(index, ty)| (format!("_{}", index), ty.clone()))
                .collect::<Vec<_>>();
            self.struct_defs.insert(name.clone(), fields);
            if field_tys
                .iter()
                .all(|field_ty| self.llvm_type_is_scalar_value_aggregate_pod(field_ty))
            {
                self.value_aggregate_structs.insert(name.clone());
            }
            self.emit(&format!("%{} = type {{ {} }}", name, field_tys.join(", ")));
        }
        name
    }

    fn collect_tuple_types_from_ast(&mut self, ty: &kain_core::ast::Type) {
        match ty {
            kain_core::ast::Type::Tuple(items, _) => {
                for item in items {
                    self.collect_tuple_types_from_ast(item);
                }
                let field_tys = items
                    .iter()
                    .map(|item| self.map_type_from_ast(item))
                    .collect::<Vec<_>>();
                self.register_tuple_struct(field_tys);
            }
            kain_core::ast::Type::Array(inner, _, _)
            | kain_core::ast::Type::Slice(inner, _)
            | kain_core::ast::Type::Option(inner, _)
            | kain_core::ast::Type::Ref { inner, .. }
            | kain_core::ast::Type::Ptr { inner, .. } => self.collect_tuple_types_from_ast(inner),
            kain_core::ast::Type::Result(ok, err, _) => {
                self.collect_tuple_types_from_ast(ok);
                self.collect_tuple_types_from_ast(err);
            }
            _ => {}
        }
    }

    fn collect_tuple_types_from_resolved(&mut self, ty: &ResolvedType) {
        match ty {
            ResolvedType::Tuple(items) => {
                for item in items {
                    self.collect_tuple_types_from_resolved(item);
                }
                let field_tys = items
                    .iter()
                    .map(|item| self.map_type(item))
                    .collect::<Vec<_>>();
                self.register_tuple_struct(field_tys);
            }
            ResolvedType::Array(inner, _)
            | ResolvedType::Slice(inner)
            | ResolvedType::Option(inner)
            | ResolvedType::Ref { inner, .. }
            | ResolvedType::Ptr { inner, .. } => self.collect_tuple_types_from_resolved(inner),
            ResolvedType::Result(ok, err) => {
                self.collect_tuple_types_from_resolved(ok);
                self.collect_tuple_types_from_resolved(err);
            }
            ResolvedType::Function { params, ret, .. } => {
                for param in params {
                    self.collect_tuple_types_from_resolved(param);
                }
                self.collect_tuple_types_from_resolved(ret);
            }
            _ => {}
        }
    }

    fn collect_program_tuple_types(&mut self, program: &TypedProgram) {
        for item in &program.items {
            match item {
                TypedItem::Function(func) => {
                    self.collect_tuple_types_from_resolved(&func.resolved_type);
                    for param in &func.ast.params {
                        self.collect_tuple_types_from_ast(&param.ty);
                    }
                    if let Some(ret) = &func.ast.return_type {
                        self.collect_tuple_types_from_ast(ret);
                    }
                }
                TypedItem::Patch(patch) => {
                    self.collect_tuple_types_from_resolved(&patch.resolved_type);
                    for param in &patch.ast.params {
                        self.collect_tuple_types_from_ast(&param.ty);
                    }
                    if let Some(ret) = &patch.ast.return_type {
                        self.collect_tuple_types_from_ast(ret);
                    }
                }
                TypedItem::Law(law) => {
                    self.collect_tuple_types_from_resolved(&law.resolved_type);
                    for param in &law.ast.params {
                        self.collect_tuple_types_from_ast(&param.ty);
                    }
                    self.collect_tuple_types_from_ast(&law.ast.return_type);
                }
                TypedItem::Converge(converge) => {
                    self.collect_tuple_types_from_resolved(&converge.resolved_type);
                    for param in &converge.ast.params {
                        self.collect_tuple_types_from_ast(&param.ty);
                    }
                    if let Some(ret) = &converge.ast.return_type {
                        self.collect_tuple_types_from_ast(ret);
                    }
                }
                TypedItem::World(world) => {
                    for state in &world.ast.states {
                        self.collect_tuple_types_from_ast(&state.ty);
                    }
                }
                TypedItem::Orchestrate(orchestrate) => {
                    self.collect_tuple_types_from_resolved(&orchestrate.resolved_type);
                    for param in &orchestrate.ast.params {
                        self.collect_tuple_types_from_ast(&param.ty);
                    }
                    if let Some(ret) = &orchestrate.ast.return_type {
                        self.collect_tuple_types_from_ast(ret);
                    }
                }
                TypedItem::Struct(s) => {
                    for ty in s.field_types.values() {
                        self.collect_tuple_types_from_resolved(ty);
                    }
                }
                TypedItem::Component(component) => {
                    for ty in component.prop_types.values() {
                        self.collect_tuple_types_from_resolved(ty);
                    }
                    for prop in &component.ast.props {
                        self.collect_tuple_types_from_ast(&prop.ty);
                    }
                }
                TypedItem::Actor(actor) => {
                    for ty in actor.state_types.values() {
                        self.collect_tuple_types_from_resolved(ty);
                    }
                    for state in &actor.ast.state {
                        self.collect_tuple_types_from_ast(&state.ty);
                    }
                    for handler in &actor.ast.handlers {
                        for param in &handler.params {
                            self.collect_tuple_types_from_ast(&param.ty);
                        }
                    }
                }
                TypedItem::Enum(en) => {
                    for payload_types in en.variant_payload_types.values() {
                        for ty in payload_types {
                            self.collect_tuple_types_from_resolved(ty);
                        }
                    }
                }
                TypedItem::Impl(imp) => {
                    self.collect_tuple_types_from_ast(&imp.ast.target_type);
                    for method in &imp.ast.methods {
                        for param in &method.params {
                            self.collect_tuple_types_from_ast(&param.ty);
                        }
                        if let Some(ret) = &method.return_type {
                            self.collect_tuple_types_from_ast(ret);
                        }
                    }
                }
                TypedItem::Const(const_def) => {
                    self.collect_tuple_types_from_resolved(&const_def.ty);
                    self.collect_tuple_types_from_ast(&const_def.ast.ty);
                }
                TypedItem::Mod(module) => {
                    self.collect_program_tuple_types(&TypedProgram {
                        items: module.items.clone(),
                    });
                }
                _ => {}
            }
        }
    }

    fn register_builtin_tuple_structs(&mut self) {
        for lanes in [2usize, 3usize, 4usize] {
            self.register_tuple_struct(vec!["double".to_string(); lanes]);
        }
    }

    fn map_type_from_ast(&self, ty: &kain_core::ast::Type) -> String {
        match ty {
            kain_core::ast::Type::Named { name, generics, .. } => match name.as_str() {
                "Option" if generics.len() == 1 => "i8*".into(),
                "Result" if generics.len() == 2 => "i8*".into(),
                "Future" if generics.len() == 1 => "i8*".into(),
                "P" => REPLY_PORT_LLVM_TYPE.into(),
                _ => self.map_type_from_str(name),
            },
            kain_core::ast::Type::Tuple(items, _) => {
                let field_tys = items
                    .iter()
                    .map(|item| self.map_type_from_ast(item))
                    .collect::<Vec<_>>();
                self.tuple_struct_storage_type_from_types(&field_tys)
            }
            kain_core::ast::Type::Array(_, _, _) => "i8*".into(),
            kain_core::ast::Type::Slice(_, _) => "i8*".into(),
            kain_core::ast::Type::Ref { inner, .. } => {
                format!("{}*", self.map_type_from_ast(inner))
            }
            kain_core::ast::Type::Ptr { inner, .. } => {
                format!("{}*", self.map_type_from_ast(inner))
            }
            kain_core::ast::Type::Impl { trait_name, .. } if trait_name == "Future" => "i8*".into(),
            kain_core::ast::Type::Option(_, _) => "i8*".into(),
            kain_core::ast::Type::Result(_, _, _) => "i8*".into(),
            kain_core::ast::Type::Unit(_) => "void".into(),
            kain_core::ast::Type::Never(_) => "void".into(),
            _ => "i64".into(),
        }
    }

    fn map_type_from_str(&self, name: &str) -> String {
        match name {
            "Int" | "i64" => "i64".into(),
            "i32" => "i32".into(),
            "Float" | "f64" | "double" => "double".into(),
            "Bool" | "bool" => "i1".into(),
            "String" | "str" => "i8*".into(),
            "Unit" | "Void" | "()" | "void" => "void".into(),
            "P" => REPLY_PORT_LLVM_TYPE.into(),
            "KainActorId" => "i64".into(),
            "KainActorExitReason" => "i32".into(),
            "KainActorState" => "i32".into(),
            "KainSupervisionStrategy" => "i32".into(),
            "KainRestartPolicy" => "i32".into(),
            "KainActorMailbox" => "i8*".into(),
            "KainActorMessage" => "%KainActorMessage*".into(),
            "KainActorSpawnConfig" => "%KainActorSpawnConfig*".into(),
            "KainActorBootstrapFn" => "i32 (i64, i8*, i8*)*".into(),
            "KainActorTurnFn" => "i32 (i64, i8*, i8*, i32)*".into(),
            _ => {
                if let Some(tuple_ty) = self.builtin_named_tuple_storage_type(name) {
                    return tuple_ty;
                }
                // Check if it's a known struct/enum
                if self.struct_defs.contains_key(name) {
                    self.struct_storage_type(name)
                } else {
                    "i64".into()
                }
            }
        }
    }

    fn ast_type_is_self_alias(ty: &Type) -> bool {
        matches!(ty, Type::Named { name, .. } if name == "Self_" || name == "Self")
    }

    fn map_impl_type_from_ast(&self, target_name: &str, ty: &Type) -> String {
        if Self::ast_type_is_self_alias(ty) {
            self.struct_storage_type(target_name)
        } else {
            self.map_type_from_ast(ty)
        }
    }

    fn impl_method_has_authored_self_param(method: &kain_core::ast::Function) -> bool {
        method.params.first().is_some_and(|param| {
            Self::ast_type_is_self_alias(&param.ty)
                || matches!(param.name.as_str(), "self" | "_self")
        })
    }

    fn ast_type_is_string(ty: &Type) -> bool {
        matches!(
            ty,
            Type::Named { name, .. } if name == "String" || name == "str"
        )
    }

    fn ast_type_is_int(ty: &Type) -> bool {
        matches!(ty, Type::Named { name, .. } if name == "Int")
    }

    fn resolved_type_is_string(ty: &ResolvedType) -> bool {
        matches!(ty, ResolvedType::String)
    }

    fn record_helper_owned_pointer_local(
        &mut self,
        name: &str,
        provenance: OwnershipPointerProvenance,
    ) {
        self.ephemeral_owned_pointer_locals.remove(name);
        if provenance == OwnershipPointerProvenance::HelperOwned {
            self.helper_owned_pointer_locals
                .insert(name.to_string(), OwnershipPointerProvenance::HelperOwned);
        } else {
            self.helper_owned_pointer_locals.remove(name);
        }
    }

    fn ownership_pointer_provenance_for_expr(&self, expr: &Expr) -> OwnershipPointerProvenance {
        match expr {
            Expr::Ident(name, _) => {
                if self.ephemeral_owned_pointer_locals.contains_key(name) {
                    OwnershipPointerProvenance::EphemeralLocal
                } else {
                    self.helper_owned_pointer_locals
                        .get(name)
                        .copied()
                        .unwrap_or(OwnershipPointerProvenance::ImportedOrUnknown)
                }
            }
            Expr::Paren(inner, _) => self.ownership_pointer_provenance_for_expr(inner),
            Expr::Cast { value, .. } => self.ownership_pointer_provenance_for_expr(value),
            Expr::PtrOffset { pointer, .. } => self.ownership_pointer_provenance_for_expr(pointer),
            Expr::Call { callee, args, .. } => match callee.as_ref() {
                Expr::Ident(name, _) if name == "__kain_alloc" => {
                    OwnershipPointerProvenance::HelperOwned
                }
                Expr::Ident(name, _) if name == "__kain_realloc" => {
                    if args.is_empty() {
                        OwnershipPointerProvenance::ImportedOrUnknown
                    } else if matches!(args[0].value, Expr::None(_)) {
                        OwnershipPointerProvenance::HelperOwned
                    } else {
                        self.ownership_pointer_provenance_for_expr(&args[0].value)
                    }
                }
                Expr::Ident(name, _)
                    if (name == "__kain_ptr_offset" || name == "__kain_index_ptr")
                        && !args.is_empty() =>
                {
                    self.ownership_pointer_provenance_for_expr(&args[0].value)
                }
                _ => OwnershipPointerProvenance::ImportedOrUnknown,
            },
            _ => OwnershipPointerProvenance::ImportedOrUnknown,
        }
    }

    fn expr_needs_rc_retain(&self, expr: &Expr) -> bool {
        !self.is_new_object(expr)
            && self.ownership_pointer_provenance_for_expr(expr)
                == OwnershipPointerProvenance::ImportedOrUnknown
    }

    fn obvious_ast_type_byte_width(&self, ty: &Type) -> i64 {
        let mapped = self.map_type_from_ast(ty);
        Self::obvious_llvm_type_byte_width(&mapped).unwrap_or(8)
    }

    fn obvious_llvm_type_alignment(llvm_ty: &str) -> i64 {
        match Self::obvious_llvm_type_byte_width(llvm_ty) {
            Some(width @ 1) | Some(width @ 2) | Some(width @ 4) | Some(width @ 8) => width as i64,
            _ => 1,
        }
    }

    fn ptr_offset_stride_matches_llvm_type(
        &self,
        element_ty: Option<&Type>,
        llvm_ty: &str,
    ) -> bool {
        let Some(access_width) = Self::obvious_llvm_type_byte_width(llvm_ty) else {
            return false;
        };
        let stride_width = element_ty
            .map(|ty| self.obvious_ast_type_byte_width(ty))
            .unwrap_or(8);
        stride_width == access_width as i64
    }

    fn safe_memory_access_alignment(&self, pointer: &Expr, llvm_ty: &str) -> i64 {
        let natural = Self::obvious_llvm_type_alignment(llvm_ty);
        if natural <= 1 {
            return 1;
        }
        match self.ownership_pointer_provenance_for_expr(pointer) {
            OwnershipPointerProvenance::HelperOwned => natural,
            OwnershipPointerProvenance::EphemeralLocal
            | OwnershipPointerProvenance::ImportedOrUnknown => 1,
        }
    }

    fn coerce_pointer_value_to_typed_memory_pointer(
        &mut self,
        ptr: &str,
        ptr_ty: &str,
        llvm_ty: &str,
    ) -> String {
        let target_ptr_ty = format!("{}*", llvm_ty);
        if ptr_ty == target_ptr_ty {
            return ptr.to_string();
        }
        if ptr_ty.ends_with('*') {
            let typed_ptr = self.next_reg();
            self.emit(&format!(
                "  {} = bitcast {} {} to {}",
                typed_ptr, ptr_ty, ptr, target_ptr_ty
            ));
            return typed_ptr;
        }
        let ptr_i64 = self.coerce_to_i64_storage(ptr, ptr_ty);
        let typed_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = inttoptr i64 {} to {}",
            typed_ptr, ptr_i64, target_ptr_ty
        ));
        typed_ptr
    }

    fn compile_non_ephemeral_typed_memory_pointer(
        &mut self,
        pointer: &Expr,
        llvm_ty: &str,
    ) -> KainResult<(String, i64)> {
        match pointer {
            Expr::Paren(inner, _) | Expr::Cast { value: inner, .. } => {
                self.compile_non_ephemeral_typed_memory_pointer(inner, llvm_ty)
            }
            Expr::PtrOffset {
                pointer: base,
                offset,
                element_ty,
                ..
            } if self.ptr_offset_stride_matches_llvm_type(element_ty.as_ref(), llvm_ty) => {
                let (base_typed, alignment) =
                    self.compile_non_ephemeral_typed_memory_pointer(base, llvm_ty)?;
                let (offset_value, _) = self.compile_expr(offset)?;
                let derived_ptr = self.next_reg();
                self.emit(&format!(
                    "  {} = getelementptr {}, {}* {}, i64 {}",
                    derived_ptr, llvm_ty, llvm_ty, base_typed, offset_value
                ));
                Ok((derived_ptr, alignment))
            }
            Expr::Call { callee, args, .. }
                if matches!(callee.as_ref(), Expr::Ident(name, _) if name == "__kain_ptr_offset" || name == "__kain_index_ptr")
                    && args.len() == 3 =>
            {
                let known_i64_bindings = self.current_known_i64_literals();
                let stride_literal = Self::resolve_i64_literal(&args[2].value, &known_i64_bindings);
                let Some(access_width) = Self::obvious_llvm_type_byte_width(llvm_ty) else {
                    let (ptr, ptr_ty) = self.compile_expr(pointer)?;
                    let typed_ptr =
                        self.coerce_pointer_value_to_typed_memory_pointer(&ptr, &ptr_ty, llvm_ty);
                    let alignment = self.safe_memory_access_alignment(pointer, llvm_ty);
                    return Ok((typed_ptr, alignment));
                };
                if stride_literal == Some(access_width as i64) {
                    let (base_typed, alignment) =
                        self.compile_non_ephemeral_typed_memory_pointer(&args[0].value, llvm_ty)?;
                    let (offset_value, _) = self.compile_expr(&args[1].value)?;
                    let derived_ptr = self.next_reg();
                    self.emit(&format!(
                        "  {} = getelementptr {}, {}* {}, i64 {}",
                        derived_ptr, llvm_ty, llvm_ty, base_typed, offset_value
                    ));
                    Ok((derived_ptr, alignment))
                } else {
                    let (ptr, ptr_ty) = self.compile_expr(pointer)?;
                    let typed_ptr =
                        self.coerce_pointer_value_to_typed_memory_pointer(&ptr, &ptr_ty, llvm_ty);
                    let alignment = self.safe_memory_access_alignment(pointer, llvm_ty);
                    Ok((typed_ptr, alignment))
                }
            }
            _ => {
                let (ptr, ptr_ty) = self.compile_expr(pointer)?;
                let typed_ptr =
                    self.coerce_pointer_value_to_typed_memory_pointer(&ptr, &ptr_ty, llvm_ty);
                let alignment = self.safe_memory_access_alignment(pointer, llvm_ty);
                Ok((typed_ptr, alignment))
            }
        }
    }

    fn compile_ephemeral_storage_i8_pointer(
        &mut self,
        pointer: &Expr,
    ) -> KainResult<Option<(String, EphemeralOwnershipLocalWitness)>> {
        match pointer {
            Expr::Ident(name, _) => {
                let Some(witness) = self.ephemeral_owned_pointer_locals.get(name).cloned() else {
                    return Ok(None);
                };
                let storage_i8 = self.next_reg();
                if witness.storage_llvm_ty == witness.storage_element_ty {
                    self.emit(&format!(
                        "  {} = bitcast {}* {} to i8*",
                        storage_i8, witness.storage_element_ty, witness.storage_reg
                    ));
                } else {
                    let storage_base = self.next_reg();
                    self.emit(&format!(
                        "  {} = getelementptr inbounds {}, {}* {}, i32 0, i32 0",
                        storage_base,
                        witness.storage_llvm_ty,
                        witness.storage_llvm_ty,
                        witness.storage_reg
                    ));
                    self.emit(&format!(
                        "  {} = bitcast {}* {} to i8*",
                        storage_i8, witness.storage_element_ty, storage_base
                    ));
                }
                Ok(Some((storage_i8, witness)))
            }
            Expr::Paren(inner, _) | Expr::Cast { value: inner, .. } => {
                self.compile_ephemeral_storage_i8_pointer(inner)
            }
            Expr::PtrOffset {
                pointer: base,
                offset,
                element_ty,
                ..
            } => {
                let Some((base_i8, witness)) = self.compile_ephemeral_storage_i8_pointer(base)?
                else {
                    return Ok(None);
                };
                let stride_literal = element_ty
                    .as_ref()
                    .map(|ty| self.obvious_ast_type_byte_width(ty))
                    .unwrap_or(8);
                let (offset_value, _) = self.compile_expr(offset)?;
                let shift_safe_stride_literal =
                    Some(stride_literal).filter(|_| self.expr_is_proven_nonnegative_i64(offset));
                let byte_offset = self.emit_scaled_byte_offset(
                    &offset_value,
                    &stride_literal.to_string(),
                    shift_safe_stride_literal,
                );
                let derived_i8 = self.next_reg();
                self.emit(&format!(
                    "  {} = getelementptr i8, i8* {}, i64 {}",
                    derived_i8, base_i8, byte_offset
                ));
                Ok(Some((derived_i8, witness)))
            }
            Expr::Call { callee, args, .. }
                if matches!(callee.as_ref(), Expr::Ident(name, _) if name == "__kain_ptr_offset" || name == "__kain_index_ptr")
                    && args.len() == 3 =>
            {
                let Some((base_i8, witness)) =
                    self.compile_ephemeral_storage_i8_pointer(&args[0].value)?
                else {
                    return Ok(None);
                };
                let known_i64_bindings = self.current_known_i64_literals();
                let stride_literal = Self::resolve_i64_literal(&args[2].value, &known_i64_bindings);
                let (offset_value, _) = self.compile_expr(&args[1].value)?;
                let (stride_value, _) = if let Some(stride_literal) = stride_literal {
                    (stride_literal.to_string(), "i64".to_string())
                } else {
                    self.compile_expr(&args[2].value)?
                };
                let shift_safe_stride_literal =
                    stride_literal.filter(|_| self.expr_is_proven_nonnegative_i64(&args[1].value));
                let byte_offset = self.emit_scaled_byte_offset(
                    &offset_value,
                    &stride_value,
                    shift_safe_stride_literal,
                );
                let derived_i8 = self.next_reg();
                self.emit(&format!(
                    "  {} = getelementptr i8, i8* {}, i64 {}",
                    derived_i8, base_i8, byte_offset
                ));
                Ok(Some((derived_i8, witness)))
            }
            _ => Ok(None),
        }
    }

    fn compile_ephemeral_typed_memory_pointer(
        &mut self,
        pointer: &Expr,
        llvm_ty: &str,
    ) -> KainResult<Option<(String, i64)>> {
        match pointer {
            Expr::Ident(name, _) => {
                let Some(witness) = self.ephemeral_owned_pointer_locals.get(name).cloned() else {
                    return Ok(None);
                };
                if witness.storage_element_ty != llvm_ty {
                    return Ok(None);
                }
                let typed_ptr = if witness.storage_llvm_ty == witness.storage_element_ty {
                    witness.storage_reg
                } else {
                    let storage_base = self.next_reg();
                    self.emit(&format!(
                        "  {} = getelementptr inbounds {}, {}* {}, i32 0, i32 0",
                        storage_base,
                        witness.storage_llvm_ty,
                        witness.storage_llvm_ty,
                        witness.storage_reg
                    ));
                    storage_base
                };
                let alignment =
                    Self::obvious_llvm_type_alignment(llvm_ty).min(witness.storage_alignment);
                Ok(Some((typed_ptr, alignment)))
            }
            Expr::Paren(inner, _) | Expr::Cast { value: inner, .. } => {
                self.compile_ephemeral_typed_memory_pointer(inner, llvm_ty)
            }
            Expr::PtrOffset {
                pointer: base,
                offset,
                element_ty,
                ..
            } if self.ptr_offset_stride_matches_llvm_type(element_ty.as_ref(), llvm_ty) => {
                let Some((base_typed, alignment)) =
                    self.compile_ephemeral_typed_memory_pointer(base, llvm_ty)?
                else {
                    return Ok(None);
                };
                let (offset_value, _) = self.compile_expr(offset)?;
                let derived_ptr = self.next_reg();
                self.emit(&format!(
                    "  {} = getelementptr {}, {}* {}, i64 {}",
                    derived_ptr, llvm_ty, llvm_ty, base_typed, offset_value
                ));
                Ok(Some((derived_ptr, alignment)))
            }
            Expr::Call { callee, args, .. }
                if matches!(callee.as_ref(), Expr::Ident(name, _) if name == "__kain_ptr_offset" || name == "__kain_index_ptr")
                    && args.len() == 3 =>
            {
                let Some(access_width) = Self::obvious_llvm_type_byte_width(llvm_ty) else {
                    return Ok(None);
                };
                let known_i64_bindings = self.current_known_i64_literals();
                let stride_literal = Self::resolve_i64_literal(&args[2].value, &known_i64_bindings);
                if stride_literal != Some(access_width) {
                    return Ok(None);
                }
                let Some((base_typed, alignment)) =
                    self.compile_ephemeral_typed_memory_pointer(&args[0].value, llvm_ty)?
                else {
                    return Ok(None);
                };
                let (offset_value, _) = self.compile_expr(&args[1].value)?;
                let derived_ptr = self.next_reg();
                self.emit(&format!(
                    "  {} = getelementptr {}, {}* {}, i64 {}",
                    derived_ptr, llvm_ty, llvm_ty, base_typed, offset_value
                ));
                Ok(Some((derived_ptr, alignment)))
            }
            _ => Ok(None),
        }
    }

    fn scalar_forward_key(&self, expr: &Expr) -> Option<String> {
        match expr {
            Expr::Int(value, _) => Some(format!("i:{value}")),
            Expr::Bool(value, _) => Some(format!("b:{value}")),
            Expr::Ident(name, _) => Some(format!("v:{name}")),
            Expr::Paren(inner, _) | Expr::Cast { value: inner, .. } => {
                self.scalar_forward_key(inner)
            }
            Expr::Unary { op, operand, .. } => {
                Some(format!("u:{:?}({})", op, self.scalar_forward_key(operand)?))
            }
            Expr::Binary {
                left, op, right, ..
            } => Some(format!(
                "b:{:?}({},{})",
                op,
                self.scalar_forward_key(left)?,
                self.scalar_forward_key(right)?
            )),
            Expr::Call { callee, args, .. } => {
                let Expr::Ident(name, _) = callee.as_ref() else {
                    return None;
                };
                if name != "__kain_ptr_offset" && name != "__kain_index_ptr" {
                    return None;
                }
                let rendered_args = args
                    .iter()
                    .map(|arg| self.scalar_forward_key(&arg.value))
                    .collect::<Option<Vec<_>>>()?
                    .join(",");
                Some(format!("call:{name}({rendered_args})"))
            }
            _ => None,
        }
    }

    fn forwardable_mem_pointer_key(&self, pointer: &Expr) -> Option<String> {
        match pointer {
            Expr::Ident(name, _) if self.ephemeral_owned_pointer_locals.contains_key(name) => {
                Some(format!("ephemeral:{name}"))
            }
            Expr::Paren(inner, _) | Expr::Cast { value: inner, .. } => {
                self.forwardable_mem_pointer_key(inner)
            }
            Expr::PtrOffset {
                pointer: base,
                offset,
                element_ty,
                ..
            } => {
                let base_key = self.forwardable_mem_pointer_key(base)?;
                let offset_key = self.scalar_forward_key(offset)?;
                let stride = element_ty
                    .as_ref()
                    .map(|ty| self.obvious_ast_type_byte_width(ty))
                    .unwrap_or(8);
                Some(format!("offset:{base_key}:{offset_key}:stride:{stride}"))
            }
            Expr::Call { callee, args, .. }
                if matches!(callee.as_ref(), Expr::Ident(name, _) if name == "__kain_ptr_offset" || name == "__kain_index_ptr")
                    && args.len() == 3 =>
            {
                let base_key = self.forwardable_mem_pointer_key(&args[0].value)?;
                let offset_key = self.scalar_forward_key(&args[1].value)?;
                let known_i64_bindings = self.current_known_i64_literals();
                let stride_key = Self::resolve_i64_literal(&args[2].value, &known_i64_bindings)
                    .map(|literal| format!("stride:{literal}"))
                    .or_else(|| {
                        self.scalar_forward_key(&args[2].value)
                            .map(|key| format!("stride_expr:{key}"))
                    })?;
                Some(format!("offset:{base_key}:{offset_key}:{stride_key}"))
            }
            _ => None,
        }
    }

    fn forwarded_mem_load_slot(&self, expr: &Expr) -> Option<&ForwardedMemSlot> {
        let key = match expr {
            Expr::MemLoad { pointer, .. } => self.forwardable_mem_pointer_key(pointer)?,
            Expr::Call { callee, args, .. }
                if matches!(callee.as_ref(), Expr::Ident(name, _) if name == "__kain_mem_load")
                    && args.len() == 1 =>
            {
                self.forwardable_mem_pointer_key(&args[0].value)?
            }
            Expr::Cast { value, .. } | Expr::Paren(value, _) => {
                return self.forwarded_mem_load_slot(value)
            }
            _ => return None,
        };
        self.forwarded_mem_slot_scopes
            .iter()
            .rev()
            .find_map(|scope| scope.get(&key))
    }

    fn current_forwarded_mem_load_slot(&self, pointer: &Expr) -> Option<&ForwardedMemSlot> {
        let key = self.forwardable_mem_pointer_key(pointer)?;
        self.forwarded_mem_slot_scopes
            .iter()
            .rev()
            .find_map(|scope| scope.get(&key))
    }

    fn record_forwarded_mem_store(
        &mut self,
        pointer: &Expr,
        value_reg: &str,
        value_ty: &str,
        nonnegative_i64: bool,
    ) {
        let Some(key) = self.forwardable_mem_pointer_key(pointer) else {
            return;
        };
        if let Some(scope) = self.forwarded_mem_slot_scopes.last_mut() {
            scope.insert(
                key,
                ForwardedMemSlot {
                    value_reg: value_reg.to_string(),
                    value_ty: value_ty.to_string(),
                    nonnegative_i64,
                },
            );
        }
    }

    fn clear_current_forwarded_mem_slots(&mut self) {
        if let Some(scope) = self.forwarded_mem_slot_scopes.last_mut() {
            scope.clear();
        }
    }

    fn expr_is_mem_load_surface(expr: &Expr) -> bool {
        match expr {
            Expr::MemLoad { .. } => true,
            Expr::Call { callee, .. } if matches!(callee.as_ref(), Expr::Ident(name, _) if name == "__kain_mem_load") => {
                true
            }
            Expr::Cast { value, .. } | Expr::Paren(value, _) => {
                Self::expr_is_mem_load_surface(value)
            }
            _ => false,
        }
    }

    fn stmt_preserves_forwarded_mem_slots(stmt: &Stmt) -> bool {
        match stmt {
            Stmt::Expr(Expr::MemStore { .. }) => true,
            Stmt::Expr(Expr::Call { callee, .. }) if matches!(callee.as_ref(), Expr::Ident(name, _) if name == "__kain_mem_store") => {
                true
            }
            Stmt::Let {
                value: Some(value), ..
            } => Self::expr_is_mem_load_surface(value),
            _ => false,
        }
    }

    fn current_scope_marks_ephemeral_candidate(&self, name: &str) -> bool {
        self.ephemeral_candidate_scopes
            .last()
            .map(|candidates| candidates.contains(name))
            .unwrap_or(false)
    }

    fn current_scope_elides_ephemeral_zero_init(&self, name: &str) -> bool {
        self.ephemeral_zero_init_elision_scopes
            .last()
            .map(|candidates| candidates.contains(name))
            .unwrap_or(false)
    }

    fn current_known_i64_literals(&self) -> HashMap<String, i64> {
        let mut merged = HashMap::new();
        for scope in &self.known_i64_literal_scopes {
            for (name, literal) in scope {
                merged.insert(name.clone(), *literal);
            }
        }
        merged
    }

    fn current_known_nonnegative_i64s(&self) -> HashSet<String> {
        let mut merged = HashSet::new();
        for scope in &self.known_nonnegative_i64_scopes {
            for name in scope {
                merged.insert(name.clone());
            }
        }
        merged
    }

    fn current_known_llvm_types(&self) -> HashMap<String, String> {
        let mut merged = HashMap::new();
        for (name, (_, ty)) in &self.locals {
            merged.insert(name.clone(), ty.clone());
        }
        merged
    }

    fn current_scope_marks_fixed_array_candidate(&self, name: &str) -> bool {
        self.fixed_array_candidate_scopes
            .last()
            .map(|candidates| candidates.contains(name))
            .unwrap_or(false)
    }

    fn current_scope_marks_stack_shatter_candidate(&self, name: &str) -> bool {
        self.stack_shatter_candidate_scopes
            .last()
            .map(|candidates| candidates.contains(name))
            .unwrap_or(false)
    }

    fn current_scope_marks_literal_map_candidate(&self, name: &str) -> bool {
        self.literal_map_candidate_scopes
            .last()
            .map(|candidates| candidates.contains(name))
            .unwrap_or(false)
    }

    fn active_loop_bounds_for(&self, name: &str) -> Option<LoopIndexBounds> {
        self.active_loop_index_bounds
            .iter()
            .rev()
            .find_map(|scope| scope.get(name).copied())
    }

    fn helper_alloc_storage_layout_with_bindings(
        expr: &Expr,
        known_i64_bindings: &HashMap<String, i64>,
    ) -> Option<HelperAllocStorageLayout> {
        match expr {
            Expr::Call { callee, args, .. }
                if matches!(callee.as_ref(), Expr::Ident(name, _) if name == "__kain_alloc")
                    && args.len() == 3 =>
            {
                let element_count = Self::resolve_i64_literal(&args[0].value, known_i64_bindings)?;
                let stride_bytes = Self::resolve_i64_literal(&args[1].value, known_i64_bindings)?;
                let zeroed = Self::resolve_zeroed_literal(&args[2].value, known_i64_bindings)?;
                if element_count <= 0 || stride_bytes <= 0 {
                    return None;
                }
                let byte_len = element_count.checked_mul(stride_bytes)?;
                (byte_len > 0 && byte_len <= 65536).then_some(HelperAllocStorageLayout {
                    element_count,
                    stride_bytes,
                    byte_len,
                    zeroed,
                })
            }
            _ => None,
        }
    }

    fn helper_alloc_is_single_cell(expr: &Expr, known_i64_bindings: &HashMap<String, i64>) -> bool {
        matches!(
            Self::helper_alloc_storage_layout_with_bindings(expr, known_i64_bindings),
            Some(layout) if layout.element_count == 1
        )
    }

    fn helper_alloc_scalar_llvm_ty(stride_bytes: i64) -> Option<&'static str> {
        match stride_bytes {
            1 => Some("i8"),
            2 => Some("i16"),
            4 => Some("i32"),
            8 => Some("i64"),
            _ => None,
        }
    }

    fn preferred_ephemeral_storage_element_llvm_ty(
        &self,
        declared_ty: Option<&Type>,
        layout: HelperAllocStorageLayout,
    ) -> Option<String> {
        let pointee_ty = match declared_ty {
            Some(Type::Ptr { inner, .. }) | Some(Type::Ref { inner, .. }) => inner.as_ref(),
            _ => return None,
        };
        let llvm_ty = self.map_type_from_ast(pointee_ty);
        if !matches!(
            llvm_ty.as_str(),
            "i1" | "i8" | "i16" | "i32" | "i64" | "double"
        ) {
            return None;
        }
        (Self::obvious_llvm_type_byte_width(&llvm_ty) == Some(layout.stride_bytes))
            .then_some(llvm_ty)
    }

    fn preferred_ephemeral_storage_element_llvm_ty_for_let(
        &self,
        lowered_declared_ty: Option<&Type>,
        stmt_span: Span,
        layout: HelperAllocStorageLayout,
    ) -> Option<String> {
        let authored_declared_ty = match lowered_declared_ty {
            Some(Type::Ptr { .. }) | Some(Type::Ref { .. }) => lowered_declared_ty,
            _ => self.original_pointer_let_types.get(&stmt_span),
        };
        self.preferred_ephemeral_storage_element_llvm_ty(authored_declared_ty, layout)
    }

    fn helper_alloc_stack_storage_shape(
        layout: HelperAllocStorageLayout,
        preferred_scalar_llvm_ty: Option<&str>,
    ) -> (String, String, i64) {
        let scalar_ty = preferred_scalar_llvm_ty
            .filter(|ty| Self::obvious_llvm_type_byte_width(ty) == Some(layout.stride_bytes))
            .or_else(|| Self::helper_alloc_scalar_llvm_ty(layout.stride_bytes));
        if let Some(scalar_ty) = scalar_ty {
            if layout.element_count == 1 {
                (
                    scalar_ty.to_string(),
                    scalar_ty.to_string(),
                    Self::obvious_llvm_type_alignment(scalar_ty),
                )
            } else {
                (
                    format!("[{} x {}]", layout.element_count, scalar_ty),
                    scalar_ty.to_string(),
                    Self::obvious_llvm_type_alignment(scalar_ty),
                )
            }
        } else {
            (format!("[{} x i8]", layout.byte_len), "i8".to_string(), 1)
        }
    }

    fn resolve_i64_literal(expr: &Expr, known_i64_bindings: &HashMap<String, i64>) -> Option<i64> {
        match expr {
            Expr::Int(value, _) => Some(*value),
            Expr::Ident(name, _) => known_i64_bindings.get(name).copied(),
            Expr::Paren(inner, _) => Self::resolve_i64_literal(inner, known_i64_bindings),
            Expr::Cast { value, .. } => Self::resolve_i64_literal(value, known_i64_bindings),
            Expr::Binary {
                left, op, right, ..
            } => {
                let lhs = Self::resolve_i64_literal(left, known_i64_bindings)?;
                let rhs = Self::resolve_i64_literal(right, known_i64_bindings)?;
                match op {
                    BinaryOp::Add => lhs.checked_add(rhs),
                    BinaryOp::Sub => lhs.checked_sub(rhs),
                    BinaryOp::Mul => lhs.checked_mul(rhs),
                    BinaryOp::Div if rhs != 0 => lhs.checked_div(rhs),
                    BinaryOp::Mod if rhs != 0 => lhs.checked_rem(rhs),
                    BinaryOp::BitAnd => Some(lhs & rhs),
                    BinaryOp::BitOr => Some(lhs | rhs),
                    BinaryOp::BitXor => Some(lhs ^ rhs),
                    BinaryOp::Shl if (0..63).contains(&rhs) => lhs.checked_shl(rhs as u32),
                    BinaryOp::Shr if (0..63).contains(&rhs) => lhs.checked_shr(rhs as u32),
                    _ => None,
                }
            }
            _ => None,
        }
    }

    fn resolve_zeroed_literal(
        expr: &Expr,
        known_i64_bindings: &HashMap<String, i64>,
    ) -> Option<bool> {
        match expr {
            Expr::Bool(value, _) => Some(*value),
            Expr::Paren(inner, _) => Self::resolve_zeroed_literal(inner, known_i64_bindings),
            Expr::Cast { value, .. } => Self::resolve_zeroed_literal(value, known_i64_bindings),
            _ => match Self::resolve_i64_literal(expr, known_i64_bindings) {
                Some(0) => Some(false),
                Some(1) => Some(true),
                _ => None,
            },
        }
    }

    fn positive_power_of_two_shift(value: i64) -> Option<u32> {
        (value > 0 && (value & (value - 1)) == 0).then_some(value.trailing_zeros())
    }

    fn positive_i64_literal(expr: &Expr, known_i64_bindings: &HashMap<String, i64>) -> Option<i64> {
        let value = Self::resolve_i64_literal(expr, known_i64_bindings)?;
        (value > 0).then_some(value)
    }

    fn expr_is_proven_nonnegative_i64(&self, expr: &Expr) -> bool {
        let known_i64_bindings = self.current_known_i64_literals();
        let known_nonnegative = self.current_known_nonnegative_i64s();
        self.expr_is_proven_nonnegative_i64_with(expr, &known_i64_bindings, &known_nonnegative)
    }

    fn expr_is_proven_nonnegative_i64_with(
        &self,
        expr: &Expr,
        known_i64_bindings: &HashMap<String, i64>,
        known_nonnegative: &HashSet<String>,
    ) -> bool {
        match expr {
            Expr::Int(value, _) => *value >= 0,
            Expr::Bool(_, _) => true,
            Expr::Ident(name, _) => {
                known_i64_bindings
                    .get(name)
                    .map(|value| *value >= 0)
                    .unwrap_or(false)
                    || known_nonnegative.contains(name)
                    || self
                        .active_loop_bounds_for(name)
                        .map(|bounds| bounds.lower_inclusive >= 0)
                        .unwrap_or(false)
            }
            Expr::Paren(inner, _) | Expr::Cast { value: inner, .. } => self
                .expr_is_proven_nonnegative_i64_with(inner, known_i64_bindings, known_nonnegative),
            Expr::Binary {
                left, op, right, ..
            } => match op {
                BinaryOp::Add | BinaryOp::Mul | BinaryOp::BitOr | BinaryOp::BitXor => {
                    self.expr_is_proven_nonnegative_i64_with(
                        left,
                        known_i64_bindings,
                        known_nonnegative,
                    ) && self.expr_is_proven_nonnegative_i64_with(
                        right,
                        known_i64_bindings,
                        known_nonnegative,
                    )
                }
                BinaryOp::BitAnd => Self::resolve_i64_literal(right, known_i64_bindings)
                    .map(|mask| mask >= 0)
                    .unwrap_or_else(|| {
                        self.expr_is_proven_nonnegative_i64_with(
                            left,
                            known_i64_bindings,
                            known_nonnegative,
                        ) && self.expr_is_proven_nonnegative_i64_with(
                            right,
                            known_i64_bindings,
                            known_nonnegative,
                        )
                    }),
                BinaryOp::Div | BinaryOp::Mod => {
                    self.expr_is_proven_nonnegative_i64_with(
                        left,
                        known_i64_bindings,
                        known_nonnegative,
                    ) && Self::positive_i64_literal(right, known_i64_bindings).is_some()
                }
                BinaryOp::Shr => {
                    self.expr_is_proven_nonnegative_i64_with(
                        left,
                        known_i64_bindings,
                        known_nonnegative,
                    ) && Self::positive_i64_literal(right, known_i64_bindings).is_some()
                }
                BinaryOp::Shl => false,
                _ => false,
            },
            _ => false,
        }
    }

    fn debug_mentions_identifier<T: std::fmt::Debug>(node: &T, target: &str) -> bool {
        format!("{:?}", node).contains(&format!("\"{}\"", target))
    }

    fn else_branch_has_loop_that_mentions_identifier(
        else_branch: &ElseBranch,
        target: &str,
    ) -> bool {
        match else_branch {
            ElseBranch::Else(block) => Self::block_has_loop_that_mentions_identifier(block, target),
            ElseBranch::ElseIf(condition, block, nested) => {
                (Self::debug_mentions_identifier(condition, target)
                    || Self::block_has_loop_that_mentions_identifier(block, target))
                    || nested
                        .as_ref()
                        .map(|branch| {
                            Self::else_branch_has_loop_that_mentions_identifier(branch, target)
                        })
                        .unwrap_or(false)
            }
        }
    }

    fn expr_has_loop_that_mentions_identifier(expr: &Expr, target: &str) -> bool {
        match expr {
            Expr::Paren(inner, _)
            | Expr::Deref(inner, _)
            | Expr::Try(inner, _)
            | Expr::Await(inner, _)
            | Expr::AsyncBlock(inner, _)
            | Expr::Comptime(inner, _) => {
                Self::expr_has_loop_that_mentions_identifier(inner, target)
            }
            Expr::Block(block, _) => Self::block_has_loop_that_mentions_identifier(block, target),
            Expr::If {
                then_branch,
                else_branch,
                ..
            } => {
                Self::block_has_loop_that_mentions_identifier(then_branch, target)
                    || else_branch
                        .as_ref()
                        .map(|branch| {
                            Self::else_branch_has_loop_that_mentions_identifier(branch, target)
                        })
                        .unwrap_or(false)
            }
            Expr::Match { arms, .. } => arms.iter().any(|arm| {
                arm.guard
                    .as_ref()
                    .map(|guard| Self::expr_has_loop_that_mentions_identifier(guard, target))
                    .unwrap_or(false)
                    || Self::expr_has_loop_that_mentions_identifier(&arm.body, target)
            }),
            Expr::Lambda { body, .. } => Self::expr_has_loop_that_mentions_identifier(body, target),
            Expr::Call { callee, args, .. } => {
                Self::expr_has_loop_that_mentions_identifier(callee, target)
                    || args
                        .iter()
                        .any(|arg| Self::expr_has_loop_that_mentions_identifier(&arg.value, target))
            }
            Expr::StageCall { args, .. } => args
                .iter()
                .any(|arg| Self::expr_has_loop_that_mentions_identifier(&arg.value, target)),
            Expr::MethodCall { receiver, args, .. } => {
                Self::expr_has_loop_that_mentions_identifier(receiver, target)
                    || args
                        .iter()
                        .any(|arg| Self::expr_has_loop_that_mentions_identifier(&arg.value, target))
            }
            Expr::Binary { left, right, .. } => {
                Self::expr_has_loop_that_mentions_identifier(left, target)
                    || Self::expr_has_loop_that_mentions_identifier(right, target)
            }
            Expr::Unary { operand, .. } => {
                Self::expr_has_loop_that_mentions_identifier(operand, target)
            }
            Expr::Field { object, .. } => {
                Self::expr_has_loop_that_mentions_identifier(object, target)
            }
            Expr::Index { object, index, .. } => {
                Self::expr_has_loop_that_mentions_identifier(object, target)
                    || Self::expr_has_loop_that_mentions_identifier(index, target)
            }
            Expr::Assign {
                target: lhs, value, ..
            } => {
                Self::expr_has_loop_that_mentions_identifier(lhs, target)
                    || Self::expr_has_loop_that_mentions_identifier(value, target)
            }
            Expr::Struct { fields, rest, .. } => {
                fields
                    .iter()
                    .any(|(_, value)| Self::expr_has_loop_that_mentions_identifier(value, target))
                    || rest
                        .as_ref()
                        .map(|value| Self::expr_has_loop_that_mentions_identifier(value, target))
                        .unwrap_or(false)
            }
            Expr::AggregateInit { fields, .. } => fields
                .iter()
                .any(|(_, value)| Self::expr_has_loop_that_mentions_identifier(value, target)),
            Expr::EnumVariant { fields, .. } => match fields {
                kain_core::ast::EnumVariantFields::Unit => false,
                kain_core::ast::EnumVariantFields::Tuple(values) => values
                    .iter()
                    .any(|value| Self::expr_has_loop_that_mentions_identifier(value, target)),
                kain_core::ast::EnumVariantFields::Struct(fields) => fields
                    .iter()
                    .any(|(_, value)| Self::expr_has_loop_that_mentions_identifier(value, target)),
            },
            Expr::Array(items, _) | Expr::Tuple(items, _) | Expr::FString(items, _) => items
                .iter()
                .any(|item| Self::expr_has_loop_that_mentions_identifier(item, target)),
            Expr::Range { start, end, .. } => {
                start
                    .as_ref()
                    .map(|value| Self::expr_has_loop_that_mentions_identifier(value, target))
                    .unwrap_or(false)
                    || end
                        .as_ref()
                        .map(|value| Self::expr_has_loop_that_mentions_identifier(value, target))
                        .unwrap_or(false)
            }
            Expr::Ref { value, .. }
            | Expr::AddrOf { value, .. }
            | Expr::Cast { value, .. }
            | Expr::Observe { target: value, .. }
            | Expr::Collapse { target: value, .. }
            | Expr::Decay { target: value, .. } => {
                Self::expr_has_loop_that_mentions_identifier(value, target)
            }
            Expr::PtrOffset {
                pointer, offset, ..
            } => {
                Self::expr_has_loop_that_mentions_identifier(pointer, target)
                    || Self::expr_has_loop_that_mentions_identifier(offset, target)
            }
            Expr::MemLoad { pointer, .. } => {
                Self::expr_has_loop_that_mentions_identifier(pointer, target)
            }
            Expr::MemStore { pointer, value, .. } => {
                Self::expr_has_loop_that_mentions_identifier(pointer, target)
                    || Self::expr_has_loop_that_mentions_identifier(value, target)
            }
            Expr::Alloc { size, .. } => Self::expr_has_loop_that_mentions_identifier(size, target),
            Expr::Realloc { pointer, size, .. } => {
                Self::expr_has_loop_that_mentions_identifier(pointer, target)
                    || Self::expr_has_loop_that_mentions_identifier(size, target)
            }
            Expr::Teleport { value, .. }
            | Expr::Return(Some(value), _)
            | Expr::Break(Some(value), _) => {
                Self::expr_has_loop_that_mentions_identifier(value, target)
            }
            Expr::Spawn { init, .. } => init
                .iter()
                .any(|(_, value)| Self::expr_has_loop_that_mentions_identifier(value, target)),
            Expr::SendMsg {
                target: msg_target,
                data,
                ..
            } => {
                Self::expr_has_loop_that_mentions_identifier(msg_target, target)
                    || data.iter().any(|(_, value)| {
                        Self::expr_has_loop_that_mentions_identifier(value, target)
                    })
            }
            Expr::MacroCall { args, .. } => args
                .iter()
                .any(|arg| Self::expr_has_loop_that_mentions_identifier(arg, target)),
            Expr::Return(None, _)
            | Expr::Break(None, _)
            | Expr::Continue(_)
            | Expr::Int(..)
            | Expr::Float(..)
            | Expr::String(..)
            | Expr::Bool(..)
            | Expr::None(..)
            | Expr::Ident(..)
            | Expr::SizeOfType { .. }
            | Expr::AlignOfType { .. }
            | Expr::Alloca { .. }
            | Expr::Uninit { .. }
            | Expr::JSX(..) => false,
        }
    }

    fn stmt_has_loop_that_mentions_identifier(stmt: &Stmt, target: &str) -> bool {
        match stmt {
            Stmt::Expr(expr) | Stmt::Return(Some(expr), _) | Stmt::Break(Some(expr), _) => {
                Self::expr_has_loop_that_mentions_identifier(expr, target)
            }
            Stmt::Let {
                value: Some(expr), ..
            } => Self::expr_has_loop_that_mentions_identifier(expr, target),
            Stmt::While {
                condition, body, ..
            } => {
                Self::debug_mentions_identifier(condition, target)
                    || Self::debug_mentions_identifier(body, target)
                    || Self::block_has_loop_that_mentions_identifier(body, target)
            }
            Stmt::For { iter, body, .. } => {
                Self::debug_mentions_identifier(iter, target)
                    || Self::debug_mentions_identifier(body, target)
                    || Self::block_has_loop_that_mentions_identifier(body, target)
            }
            Stmt::Loop { body, .. } => {
                Self::debug_mentions_identifier(body, target)
                    || Self::block_has_loop_that_mentions_identifier(body, target)
            }
            Stmt::Item(_item) => false,
            Stmt::Let { value: None, .. }
            | Stmt::Return(None, _)
            | Stmt::Break(None, _)
            | Stmt::Continue(_) => false,
        }
    }

    fn block_has_loop_that_mentions_identifier(block: &Block, target: &str) -> bool {
        block
            .stmts
            .iter()
            .any(|stmt| Self::stmt_has_loop_that_mentions_identifier(stmt, target))
    }

    fn expr_is_exact_target_pointer(expr: &Expr, target: &str) -> bool {
        match expr {
            Expr::Ident(name, _) => name == target,
            Expr::Paren(inner, _) => Self::expr_is_exact_target_pointer(inner, target),
            Expr::Cast { value, .. } => Self::expr_is_exact_target_pointer(value, target),
            _ => false,
        }
    }

    fn expr_is_ephemeral_target_address(expr: &Expr, target: &str) -> bool {
        match expr {
            Expr::Ident(name, _) => name == target,
            Expr::Paren(inner, _) | Expr::Cast { value: inner, .. } => {
                Self::expr_is_ephemeral_target_address(inner, target)
            }
            Expr::PtrOffset {
                pointer, offset, ..
            } => {
                Self::expr_is_ephemeral_target_address(pointer, target)
                    && Self::expr_is_safe_for_ephemeral_local(offset, target)
            }
            Expr::Call { callee, args, .. }
                if matches!(callee.as_ref(), Expr::Ident(name, _) if name == "__kain_ptr_offset" || name == "__kain_index_ptr")
                    && args.len() == 3 =>
            {
                Self::expr_is_ephemeral_target_address(&args[0].value, target)
                    && Self::expr_is_safe_for_ephemeral_local(&args[1].value, target)
                    && Self::expr_is_safe_for_ephemeral_local(&args[2].value, target)
            }
            _ => false,
        }
    }

    fn stmt_binds_i64_literal(
        stmt: &Stmt,
        known_i64_bindings: &HashMap<String, i64>,
    ) -> Option<(String, i64)> {
        match stmt {
            Stmt::Let {
                pattern: Pattern::Binding { name, .. },
                value: Some(value),
                ..
            } => Self::resolve_i64_literal(value, known_i64_bindings)
                .map(|literal| (name.clone(), literal)),
            _ => None,
        }
    }

    fn stmt_assigned_identifier_name(stmt: &Stmt) -> Option<&str> {
        match stmt {
            Stmt::Expr(Expr::Assign { target, .. }) => match target.as_ref() {
                Expr::Ident(name, _) => Some(name.as_str()),
                _ => None,
            },
            _ => None,
        }
    }

    fn collect_expr_assigned_identifier_names(expr: &Expr, assigned: &mut HashSet<String>) {
        match expr {
            Expr::Assign { target, value, .. } => {
                if let Expr::Ident(name, _) = target.as_ref() {
                    assigned.insert(name.clone());
                }
                Self::collect_expr_assigned_identifier_names(value, assigned);
            }
            Expr::If {
                condition,
                then_branch,
                else_branch,
                ..
            } => {
                Self::collect_expr_assigned_identifier_names(condition, assigned);
                Self::collect_block_assigned_identifier_names(then_branch, assigned);
                if let Some(else_branch) = else_branch {
                    Self::collect_else_branch_assigned_identifier_names(else_branch, assigned);
                }
            }
            Expr::Match {
                scrutinee, arms, ..
            } => {
                Self::collect_expr_assigned_identifier_names(scrutinee, assigned);
                for arm in arms {
                    if let Some(guard) = &arm.guard {
                        Self::collect_expr_assigned_identifier_names(guard, assigned);
                    }
                    Self::collect_expr_assigned_identifier_names(&arm.body, assigned);
                }
            }
            Expr::Binary { left, right, .. } => {
                Self::collect_expr_assigned_identifier_names(left, assigned);
                Self::collect_expr_assigned_identifier_names(right, assigned);
            }
            Expr::Unary { operand, .. }
            | Expr::Deref(operand, _)
            | Expr::Try(operand, _)
            | Expr::Await(operand, _)
            | Expr::AsyncBlock(operand, _)
            | Expr::Comptime(operand, _)
            | Expr::Paren(operand, _)
            | Expr::Cast { value: operand, .. }
            | Expr::Ref { value: operand, .. }
            | Expr::AddrOf { value: operand, .. } => {
                Self::collect_expr_assigned_identifier_names(operand, assigned);
            }
            Expr::Call { callee, args, .. } => {
                Self::collect_expr_assigned_identifier_names(callee, assigned);
                for arg in args {
                    Self::collect_expr_assigned_identifier_names(&arg.value, assigned);
                }
            }
            Expr::StageCall { args, .. } => {
                for arg in args {
                    Self::collect_expr_assigned_identifier_names(&arg.value, assigned);
                }
            }
            Expr::MacroCall { args, .. } => {
                for arg in args {
                    Self::collect_expr_assigned_identifier_names(arg, assigned);
                }
            }
            Expr::MethodCall { receiver, args, .. } => {
                Self::collect_expr_assigned_identifier_names(receiver, assigned);
                for arg in args {
                    Self::collect_expr_assigned_identifier_names(&arg.value, assigned);
                }
            }
            Expr::Field { object, .. } => {
                Self::collect_expr_assigned_identifier_names(object, assigned);
            }
            Expr::Index { object, index, .. } => {
                Self::collect_expr_assigned_identifier_names(object, assigned);
                Self::collect_expr_assigned_identifier_names(index, assigned);
            }
            Expr::Struct { fields, rest, .. } => {
                for (_, value) in fields {
                    Self::collect_expr_assigned_identifier_names(value, assigned);
                }
                if let Some(rest) = rest {
                    Self::collect_expr_assigned_identifier_names(rest, assigned);
                }
            }
            Expr::AggregateInit { fields, .. } => {
                for (_, value) in fields {
                    Self::collect_expr_assigned_identifier_names(value, assigned);
                }
            }
            Expr::Array(items, _) | Expr::Tuple(items, _) | Expr::FString(items, _) => {
                for item in items {
                    Self::collect_expr_assigned_identifier_names(item, assigned);
                }
            }
            Expr::Range { start, end, .. } => {
                if let Some(start) = start {
                    Self::collect_expr_assigned_identifier_names(start, assigned);
                }
                if let Some(end) = end {
                    Self::collect_expr_assigned_identifier_names(end, assigned);
                }
            }
            Expr::Lambda { body, .. } => {
                Self::collect_expr_assigned_identifier_names(body, assigned);
            }
            Expr::Block(block, _) => {
                Self::collect_block_assigned_identifier_names(block, assigned);
            }
            Expr::Return(Some(value), _) | Expr::Break(Some(value), _) => {
                Self::collect_expr_assigned_identifier_names(value, assigned);
            }
            Expr::PtrOffset {
                pointer, offset, ..
            } => {
                Self::collect_expr_assigned_identifier_names(pointer, assigned);
                Self::collect_expr_assigned_identifier_names(offset, assigned);
            }
            Expr::MemLoad { pointer, .. }
            | Expr::Decay {
                target: pointer, ..
            } => {
                Self::collect_expr_assigned_identifier_names(pointer, assigned);
            }
            Expr::MemStore { pointer, value, .. } => {
                Self::collect_expr_assigned_identifier_names(pointer, assigned);
                Self::collect_expr_assigned_identifier_names(value, assigned);
            }
            Expr::Alloc { size, .. } => {
                Self::collect_expr_assigned_identifier_names(size, assigned);
            }
            Expr::Realloc { pointer, size, .. } => {
                Self::collect_expr_assigned_identifier_names(pointer, assigned);
                Self::collect_expr_assigned_identifier_names(size, assigned);
            }
            Expr::Observe { target, body, .. } | Expr::Collapse { target, body, .. } => {
                Self::collect_expr_assigned_identifier_names(target, assigned);
                Self::collect_expr_assigned_identifier_names(body, assigned);
            }
            Expr::Teleport { value, .. } => {
                Self::collect_expr_assigned_identifier_names(value, assigned);
            }
            Expr::Spawn { init, .. } => {
                for (_, value) in init {
                    Self::collect_expr_assigned_identifier_names(value, assigned);
                }
            }
            Expr::SendMsg { target, data, .. } => {
                Self::collect_expr_assigned_identifier_names(target, assigned);
                for (_, value) in data {
                    Self::collect_expr_assigned_identifier_names(value, assigned);
                }
            }
            Expr::EnumVariant { fields, .. } => match fields {
                kain_core::ast::EnumVariantFields::Unit => {}
                kain_core::ast::EnumVariantFields::Tuple(items) => {
                    for item in items {
                        Self::collect_expr_assigned_identifier_names(item, assigned);
                    }
                }
                kain_core::ast::EnumVariantFields::Struct(fields) => {
                    for (_, value) in fields {
                        Self::collect_expr_assigned_identifier_names(value, assigned);
                    }
                }
            },
            Expr::Int(_, _)
            | Expr::Float(_, _)
            | Expr::String(_, _)
            | Expr::Bool(_, _)
            | Expr::None(_)
            | Expr::Ident(_, _)
            | Expr::SizeOfType { .. }
            | Expr::AlignOfType { .. }
            | Expr::Alloca { .. }
            | Expr::Uninit { .. }
            | Expr::JSX(_, _)
            | Expr::Return(None, _)
            | Expr::Break(None, _)
            | Expr::Continue(_) => {}
        }
    }

    fn collect_else_branch_assigned_identifier_names(
        else_branch: &ElseBranch,
        assigned: &mut HashSet<String>,
    ) {
        match else_branch {
            ElseBranch::Else(block) => {
                Self::collect_block_assigned_identifier_names(block, assigned)
            }
            ElseBranch::ElseIf(condition, block, nested) => {
                Self::collect_expr_assigned_identifier_names(condition, assigned);
                Self::collect_block_assigned_identifier_names(block, assigned);
                if let Some(nested) = nested {
                    Self::collect_else_branch_assigned_identifier_names(nested, assigned);
                }
            }
        }
    }

    fn collect_block_assigned_identifier_names(block: &Block, assigned: &mut HashSet<String>) {
        for stmt in &block.stmts {
            match stmt {
                Stmt::Expr(expr) | Stmt::Return(Some(expr), _) | Stmt::Break(Some(expr), _) => {
                    Self::collect_expr_assigned_identifier_names(expr, assigned);
                }
                Stmt::For { iter, body, .. } => {
                    Self::collect_expr_assigned_identifier_names(iter, assigned);
                    Self::collect_block_assigned_identifier_names(body, assigned);
                }
                Stmt::While {
                    condition, body, ..
                } => {
                    Self::collect_expr_assigned_identifier_names(condition, assigned);
                    Self::collect_block_assigned_identifier_names(body, assigned);
                }
                Stmt::Loop { body, .. } => {
                    Self::collect_block_assigned_identifier_names(body, assigned);
                }
                Stmt::Let { value, .. } => {
                    if let Some(value) = value {
                        Self::collect_expr_assigned_identifier_names(value, assigned);
                    }
                }
                Stmt::Return(None, _)
                | Stmt::Break(None, _)
                | Stmt::Continue(_)
                | Stmt::Item(_) => {}
            }
        }
    }

    fn clear_loop_variant_literal_facts(&mut self, body: &Block) {
        let mut assigned = HashSet::new();
        Self::collect_block_assigned_identifier_names(body, &mut assigned);
        if assigned.is_empty() {
            return;
        }
        for scope in self.known_i64_literal_scopes.iter_mut() {
            for name in &assigned {
                scope.remove(name);
            }
        }
        for scope in self.known_nonnegative_i64_scopes.iter_mut() {
            for name in &assigned {
                scope.remove(name);
            }
        }
    }

    fn record_stmt_i64_literal_effects(&mut self, stmt: &Stmt) {
        if let Some(name) = Self::stmt_assigned_identifier_name(stmt) {
            for scope in self.known_i64_literal_scopes.iter_mut().rev() {
                if scope.remove(name).is_some() {
                    break;
                }
            }
        }

        let known_i64_bindings = self.current_known_i64_literals();
        if let Some((name, literal)) = Self::stmt_binds_i64_literal(stmt, &known_i64_bindings) {
            if let Some(scope) = self.known_i64_literal_scopes.last_mut() {
                scope.insert(name, literal);
            }
        }
    }

    fn record_stmt_literal_map_effects(&mut self, stmt: &Stmt) {
        match stmt {
            Stmt::Let {
                pattern: Pattern::Binding { name, .. },
                value: Some(value),
                ..
            } => {
                self.sealed_literal_map_locals.remove(name);
                if self.current_scope_marks_literal_map_candidate(name)
                    && Self::expr_is_literal_map_seed(value)
                {
                    self.sealed_literal_map_locals
                        .insert(name.clone(), LiteralMapLocal::default());
                }
            }
            Stmt::Expr(Expr::Call { callee, args, .. })
                if matches!(callee.as_ref(), Expr::Ident(name, _) if name == "map_set")
                    && args.len() == 3 =>
            {
                if let Expr::Ident(map_name, _) = &args[0].value {
                    if let Some(local) = self.sealed_literal_map_locals.get_mut(map_name) {
                        if let (Some(key), Some(value)) = (
                            Self::extract_string_literal(&args[1].value),
                            Self::resolve_i64_literal(&args[2].value, &HashMap::new()),
                        ) {
                            local.entries.insert(key, value);
                        }
                    }
                }
            }
            _ => {}
        }
    }

    fn record_stmt_nonnegative_i64_effects(&mut self, stmt: &Stmt) {
        let binding = match stmt {
            Stmt::Let {
                pattern: Pattern::Binding { name, .. },
                value: Some(value),
                ..
            } => Some((name.as_str(), value)),
            Stmt::Expr(Expr::Assign { target, value, .. }) => match target.as_ref() {
                Expr::Ident(name, _) => Some((name.as_str(), value.as_ref())),
                _ => None,
            },
            _ => None,
        };

        let Some((name, value)) = binding else {
            return;
        };
        let is_nonnegative = self.expr_is_proven_nonnegative_i64(value)
            || self
                .forwarded_mem_load_slot(value)
                .map(|slot| slot.nonnegative_i64)
                .unwrap_or(false);
        for scope in self.known_nonnegative_i64_scopes.iter_mut().rev() {
            scope.remove(name);
        }
        if is_nonnegative {
            if let Some(scope) = self.known_nonnegative_i64_scopes.last_mut() {
                scope.insert(name.to_string());
            }
        }
    }

    fn obvious_llvm_type_byte_width(llvm_ty: &str) -> Option<i64> {
        match llvm_ty {
            "i1" | "i8" => Some(1),
            "i32" => Some(4),
            "i64" | "double" => Some(8),
            ty if ty.ends_with('*') => Some(8),
            _ => None,
        }
    }

    fn expr_obvious_llvm_ty(
        &self,
        expr: &Expr,
        known_llvm_types: &HashMap<String, String>,
    ) -> Option<String> {
        match expr {
            Expr::Int(_, _) => Some("i64".to_string()),
            Expr::Float(_, _) => Some("double".to_string()),
            Expr::Bool(_, _) => Some("i1".to_string()),
            Expr::String(_, _) => Some("i8*".to_string()),
            Expr::Ident(name, _) => known_llvm_types.get(name).cloned(),
            Expr::Paren(inner, _)
            | Expr::Deref(inner, _)
            | Expr::Try(inner, _)
            | Expr::Await(inner, _)
            | Expr::AsyncBlock(inner, _)
            | Expr::Comptime(inner, _)
            | Expr::Ref { value: inner, .. }
            | Expr::AddrOf { value: inner, .. }
            | Expr::Cast { value: inner, .. } => self.expr_obvious_llvm_ty(inner, known_llvm_types),
            Expr::Unary { operand, .. } => self.expr_obvious_llvm_ty(operand, known_llvm_types),
            Expr::Binary { left, right, .. } => {
                let left_ty = self.expr_obvious_llvm_ty(left, known_llvm_types);
                let right_ty = self.expr_obvious_llvm_ty(right, known_llvm_types);
                match (left_ty.as_deref(), right_ty.as_deref()) {
                    (Some("double"), _) | (_, Some("double")) => Some("double".to_string()),
                    (Some(left), Some(right)) if left == right => Some(left.to_string()),
                    (Some("i64"), Some("i1")) | (Some("i1"), Some("i64")) => {
                        Some("i64".to_string())
                    }
                    _ => None,
                }
            }
            _ => None,
        }
    }

    fn stmt_binds_obvious_llvm_ty(
        &self,
        stmt: &Stmt,
        known_llvm_types: &HashMap<String, String>,
    ) -> Option<(String, String)> {
        match stmt {
            Stmt::Let {
                pattern: Pattern::Binding { name, .. },
                ty: Some(declared_ty),
                ..
            } => Some((name.clone(), self.map_type_from_ast(declared_ty))),
            Stmt::Let {
                pattern: Pattern::Binding { name, .. },
                value: Some(value),
                ..
            } => self
                .expr_obvious_llvm_ty(value, known_llvm_types)
                .map(|ty| (name.clone(), ty)),
            _ => None,
        }
    }

    fn expr_is_full_width_initial_store_on_target(
        &self,
        expr: &Expr,
        target: &str,
        storage_byte_len: i64,
        known_llvm_types: &HashMap<String, String>,
    ) -> bool {
        match expr {
            Expr::Call { callee, args, .. }
                if matches!(callee.as_ref(), Expr::Ident(name, _) if name == "__kain_mem_store")
                    && args.len() == 2
                    && Self::expr_is_exact_target_pointer(&args[0].value, target) =>
            {
                self.expr_obvious_llvm_ty(&args[1].value, known_llvm_types)
                    .as_deref()
                    .and_then(Self::obvious_llvm_type_byte_width)
                    .map(|width| width == storage_byte_len)
                    .unwrap_or(false)
            }
            Expr::MemStore { pointer, value, .. }
                if Self::expr_is_exact_target_pointer(pointer, target) =>
            {
                self.expr_obvious_llvm_ty(value, known_llvm_types)
                    .as_deref()
                    .and_then(Self::obvious_llvm_type_byte_width)
                    .map(|width| width == storage_byte_len)
                    .unwrap_or(false)
            }
            _ => false,
        }
    }

    fn stmt_is_full_width_initial_store_on_target(
        &self,
        stmt: &Stmt,
        target: &str,
        storage_byte_len: i64,
        known_llvm_types: &HashMap<String, String>,
    ) -> bool {
        match stmt {
            Stmt::Expr(expr) => self.expr_is_full_width_initial_store_on_target(
                expr,
                target,
                storage_byte_len,
                known_llvm_types,
            ),
            _ => false,
        }
    }

    fn collapse_body_begins_with_full_width_store(
        &self,
        body: &Expr,
        target: &str,
        storage_byte_len: i64,
        known_llvm_types: &HashMap<String, String>,
    ) -> bool {
        match body {
            Expr::Block(block, _) => block
                .stmts
                .first()
                .map(|stmt| {
                    self.stmt_is_full_width_initial_store_on_target(
                        stmt,
                        target,
                        storage_byte_len,
                        known_llvm_types,
                    )
                })
                .unwrap_or(false),
            _ => self.expr_is_full_width_initial_store_on_target(
                body,
                target,
                storage_byte_len,
                known_llvm_types,
            ),
        }
    }

    fn remaining_statements_allow_ephemeral_zero_init_elision(
        &self,
        stmts: &[Stmt],
        target: &str,
        storage_byte_len: i64,
        known_llvm_types: &HashMap<String, String>,
    ) -> bool {
        for stmt in stmts {
            match stmt {
                Stmt::Expr(Expr::Collapse {
                    target: ownership_target,
                    body,
                    ..
                }) if Self::expr_is_exact_target_pointer(ownership_target, target) => {
                    return self.collapse_body_begins_with_full_width_store(
                        body,
                        target,
                        storage_byte_len,
                        known_llvm_types,
                    );
                }
                Stmt::Let {
                    value:
                        Some(Expr::Collapse {
                            target: ownership_target,
                            body,
                            ..
                        }),
                    ..
                } if Self::expr_is_exact_target_pointer(ownership_target, target) => {
                    return self.collapse_body_begins_with_full_width_store(
                        body,
                        target,
                        storage_byte_len,
                        known_llvm_types,
                    );
                }
                Stmt::Expr(Expr::Observe {
                    target: ownership_target,
                    ..
                }) if Self::expr_is_exact_target_pointer(ownership_target, target) => {
                    return false;
                }
                Stmt::Let {
                    value:
                        Some(Expr::Observe {
                            target: ownership_target,
                            ..
                        }),
                    ..
                } if Self::expr_is_exact_target_pointer(ownership_target, target) => {
                    return false;
                }
                Stmt::Expr(Expr::Decay {
                    target: decay_target,
                    ..
                }) if Self::expr_is_exact_target_pointer(decay_target, target) => {
                    return false;
                }
                _ => {
                    if Self::debug_mentions_identifier(stmt, target) {
                        return false;
                    }
                }
            }
        }
        false
    }

    fn expr_is_safe_for_ephemeral_local(expr: &Expr, target: &str) -> bool {
        match expr {
            Expr::Int(_, _)
            | Expr::Float(_, _)
            | Expr::String(_, _)
            | Expr::Bool(_, _)
            | Expr::None(_)
            | Expr::SizeOfType { .. }
            | Expr::AlignOfType { .. }
            | Expr::Alloca { .. }
            | Expr::Uninit { .. }
            | Expr::Continue(_) => true,
            Expr::Ident(name, _) => name != target,
            Expr::FString(parts, _) | Expr::Array(parts, _) | Expr::Tuple(parts, _) => parts
                .iter()
                .all(|part| Self::expr_is_safe_for_ephemeral_local(part, target)),
            Expr::MacroCall { args, .. } => args
                .iter()
                .all(|arg| Self::expr_is_safe_for_ephemeral_local(arg, target)),
            Expr::Binary { left, right, .. } => {
                Self::expr_is_safe_for_ephemeral_local(left, target)
                    && Self::expr_is_safe_for_ephemeral_local(right, target)
            }
            Expr::Unary { operand, .. }
            | Expr::Paren(operand, _)
            | Expr::Deref(operand, _)
            | Expr::Try(operand, _)
            | Expr::Await(operand, _)
            | Expr::AsyncBlock(operand, _)
            | Expr::Comptime(operand, _)
            | Expr::Return(Some(operand), _)
            | Expr::Break(Some(operand), _) => {
                Self::expr_is_safe_for_ephemeral_local(operand, target)
            }
            Expr::Return(None, _) | Expr::Break(None, _) => true,
            Expr::Call { callee, args, .. } => {
                if matches!(callee.as_ref(), Expr::Ident(name, _) if name == "__kain_mem_load") {
                    args.len() == 1
                        && (Self::expr_is_ephemeral_target_address(&args[0].value, target)
                            || (!Self::expr_is_exact_target_pointer(&args[0].value, target)
                                && Self::expr_is_safe_for_ephemeral_local(&args[0].value, target)))
                } else if matches!(callee.as_ref(), Expr::Ident(name, _) if name == "__kain_mem_store")
                {
                    args.len() == 2
                        && (Self::expr_is_ephemeral_target_address(&args[0].value, target)
                            || (!Self::expr_is_exact_target_pointer(&args[0].value, target)
                                && Self::expr_is_safe_for_ephemeral_local(&args[0].value, target)))
                        && Self::expr_is_safe_for_ephemeral_local(&args[1].value, target)
                } else {
                    Self::expr_is_safe_for_ephemeral_local(callee, target)
                        && args
                            .iter()
                            .all(|arg| Self::expr_is_safe_for_ephemeral_local(&arg.value, target))
                }
            }
            Expr::StageCall { args, .. } => args
                .iter()
                .all(|arg| Self::expr_is_safe_for_ephemeral_local(&arg.value, target)),
            Expr::MethodCall { receiver, args, .. } => {
                Self::expr_is_safe_for_ephemeral_local(receiver, target)
                    && args
                        .iter()
                        .all(|arg| Self::expr_is_safe_for_ephemeral_local(&arg.value, target))
            }
            Expr::Field { object, .. } => Self::expr_is_safe_for_ephemeral_local(object, target),
            Expr::Index { object, index, .. } => {
                Self::expr_is_safe_for_ephemeral_local(object, target)
                    && Self::expr_is_safe_for_ephemeral_local(index, target)
            }
            Expr::Assign {
                target: assign_target,
                value,
                ..
            } => {
                Self::expr_is_safe_for_ephemeral_local(assign_target, target)
                    && Self::expr_is_safe_for_ephemeral_local(value, target)
            }
            Expr::Struct { fields, rest, .. } => {
                fields
                    .iter()
                    .all(|(_, value)| Self::expr_is_safe_for_ephemeral_local(value, target))
                    && rest
                        .as_ref()
                        .map(|value| Self::expr_is_safe_for_ephemeral_local(value, target))
                        .unwrap_or(true)
            }
            Expr::AggregateInit { fields, .. } => fields
                .iter()
                .all(|(_, value)| Self::expr_is_safe_for_ephemeral_local(value, target)),
            Expr::EnumVariant { fields, .. } => match fields {
                kain_core::ast::EnumVariantFields::Unit => true,
                kain_core::ast::EnumVariantFields::Tuple(values) => values
                    .iter()
                    .all(|value| Self::expr_is_safe_for_ephemeral_local(value, target)),
                kain_core::ast::EnumVariantFields::Struct(values) => values
                    .iter()
                    .all(|(_, value)| Self::expr_is_safe_for_ephemeral_local(value, target)),
            },
            Expr::Range { start, end, .. } => {
                start
                    .as_ref()
                    .map(|value| Self::expr_is_safe_for_ephemeral_local(value, target))
                    .unwrap_or(true)
                    && end
                        .as_ref()
                        .map(|value| Self::expr_is_safe_for_ephemeral_local(value, target))
                        .unwrap_or(true)
            }
            Expr::If {
                condition,
                then_branch,
                else_branch,
                ..
            } => {
                Self::expr_is_safe_for_ephemeral_local(condition, target)
                    && Self::block_is_safe_for_ephemeral_local(then_branch, target)
                    && else_branch
                        .as_ref()
                        .map(|branch| Self::else_branch_is_safe_for_ephemeral_local(branch, target))
                        .unwrap_or(true)
            }
            Expr::Lambda { body, .. } => Self::expr_is_safe_for_ephemeral_local(body, target),
            Expr::Ref { value, .. } | Expr::AddrOf { value, .. } | Expr::Cast { value, .. } => {
                Self::expr_is_safe_for_ephemeral_local(value, target)
            }
            Expr::PtrOffset {
                pointer, offset, ..
            } => {
                Self::expr_is_ephemeral_target_address(expr, target)
                    || (!Self::expr_is_exact_target_pointer(pointer, target)
                        && Self::expr_is_safe_for_ephemeral_local(pointer, target)
                        && Self::expr_is_safe_for_ephemeral_local(offset, target))
            }
            Expr::MemLoad { pointer, .. } => {
                Self::expr_is_ephemeral_target_address(pointer, target)
                    || (!Self::expr_is_exact_target_pointer(pointer, target)
                        && Self::expr_is_safe_for_ephemeral_local(pointer, target))
            }
            Expr::MemStore { pointer, value, .. } => {
                (Self::expr_is_ephemeral_target_address(pointer, target)
                    || (!Self::expr_is_exact_target_pointer(pointer, target)
                        && Self::expr_is_safe_for_ephemeral_local(pointer, target)))
                    && Self::expr_is_safe_for_ephemeral_local(value, target)
            }
            Expr::Alloc { size, .. } => Self::expr_is_safe_for_ephemeral_local(size, target),
            Expr::Realloc { pointer, size, .. } => {
                Self::expr_is_safe_for_ephemeral_local(pointer, target)
                    && Self::expr_is_safe_for_ephemeral_local(size, target)
            }
            Expr::Observe {
                target: observe_target,
                body,
                ..
            }
            | Expr::Collapse {
                target: observe_target,
                body,
                ..
            } => {
                !Self::expr_is_exact_target_pointer(observe_target, target)
                    && Self::expr_is_safe_for_ephemeral_local(observe_target, target)
                    && Self::expr_is_safe_for_ephemeral_local(body, target)
            }
            Expr::Decay {
                target: decay_target,
                ..
            } => {
                !Self::expr_is_exact_target_pointer(decay_target, target)
                    && Self::expr_is_safe_for_ephemeral_local(decay_target, target)
            }
            Expr::Teleport { value, .. } => Self::expr_is_safe_for_ephemeral_local(value, target),
            Expr::Spawn { init, .. } => init
                .iter()
                .all(|(_, value)| Self::expr_is_safe_for_ephemeral_local(value, target)),
            Expr::SendMsg {
                target: send_target,
                data,
                ..
            } => {
                Self::expr_is_safe_for_ephemeral_local(send_target, target)
                    && data
                        .iter()
                        .all(|(_, value)| Self::expr_is_safe_for_ephemeral_local(value, target))
            }
            Expr::Block(block, _) => Self::block_is_safe_for_ephemeral_local(block, target),
            Expr::Match { .. } | Expr::JSX(_, _) => !Self::debug_mentions_identifier(expr, target),
        }
    }

    fn stmt_is_safe_for_ephemeral_local(stmt: &Stmt, target: &str) -> bool {
        match stmt {
            Stmt::Let { pattern, value, .. } => {
                !Self::debug_mentions_identifier(pattern, target)
                    && value
                        .as_ref()
                        .map(|expr| Self::expr_is_safe_for_ephemeral_local(expr, target))
                        .unwrap_or(true)
            }
            Stmt::Expr(expr) => Self::expr_is_safe_for_ephemeral_local(expr, target),
            Stmt::Return(Some(expr), _) | Stmt::Break(Some(expr), _) => {
                Self::expr_is_safe_for_ephemeral_local(expr, target)
            }
            Stmt::Return(None, _) | Stmt::Break(None, _) | Stmt::Continue(_) => true,
            Stmt::For { iter, body, .. } => {
                Self::expr_is_safe_for_ephemeral_local(iter, target)
                    && Self::block_is_safe_for_ephemeral_local(body, target)
            }
            Stmt::While {
                condition, body, ..
            } => {
                Self::expr_is_safe_for_ephemeral_local(condition, target)
                    && Self::block_is_safe_for_ephemeral_local(body, target)
            }
            Stmt::Loop { body, .. } => Self::block_is_safe_for_ephemeral_local(body, target),
            Stmt::Item(item) => !Self::debug_mentions_identifier(item, target),
        }
    }

    fn block_is_safe_for_ephemeral_local(block: &Block, target: &str) -> bool {
        block
            .stmts
            .iter()
            .all(|stmt| Self::stmt_is_safe_for_ephemeral_local(stmt, target))
    }

    fn else_branch_is_safe_for_ephemeral_local(else_branch: &ElseBranch, target: &str) -> bool {
        match else_branch {
            ElseBranch::Else(block) => Self::block_is_safe_for_ephemeral_local(block, target),
            ElseBranch::ElseIf(condition, then_branch, nested_else) => {
                Self::expr_is_safe_for_ephemeral_local(condition, target)
                    && Self::block_is_safe_for_ephemeral_local(then_branch, target)
                    && nested_else
                        .as_ref()
                        .map(|branch| Self::else_branch_is_safe_for_ephemeral_local(branch, target))
                        .unwrap_or(true)
            }
        }
    }

    fn remaining_statements_preserve_ephemeral_contract(stmts: &[Stmt], target: &str) -> bool {
        let mut saw_decay = false;

        for stmt in stmts {
            if saw_decay {
                if Self::debug_mentions_identifier(stmt, target) {
                    return false;
                }
                continue;
            }

            match stmt {
                Stmt::Expr(Expr::Decay {
                    target: decay_target,
                    ..
                }) if Self::expr_is_exact_target_pointer(decay_target, target) => {
                    saw_decay = true;
                }
                Stmt::Expr(Expr::Collapse {
                    target: ownership_target,
                    body,
                    ..
                }) if Self::expr_is_exact_target_pointer(ownership_target, target) => {
                    if !Self::expr_is_safe_for_ephemeral_local(body, target) {
                        return false;
                    }
                }
                Stmt::Expr(Expr::Observe {
                    target: ownership_target,
                    body,
                    ..
                }) if Self::expr_is_exact_target_pointer(ownership_target, target) => {
                    if !Self::expr_is_safe_for_ephemeral_local(body, target) {
                        return false;
                    }
                }
                Stmt::Let {
                    value:
                        Some(Expr::Observe {
                            target: ownership_target,
                            body,
                            ..
                        }),
                    ..
                } if Self::expr_is_exact_target_pointer(ownership_target, target) => {
                    if !Self::expr_is_safe_for_ephemeral_local(body, target) {
                        return false;
                    }
                }
                Stmt::Let {
                    value:
                        Some(Expr::Collapse {
                            target: ownership_target,
                            body,
                            ..
                        }),
                    ..
                } if Self::expr_is_exact_target_pointer(ownership_target, target) => {
                    if !Self::expr_is_safe_for_ephemeral_local(body, target) {
                        return false;
                    }
                }
                _ => {
                    if !Self::stmt_is_safe_for_ephemeral_local(stmt, target) {
                        return false;
                    }
                }
            }
        }

        saw_decay
    }

    fn collect_block_ephemeral_candidate_names(
        block: &Block,
        inherited_known_i64_bindings: &HashMap<String, i64>,
    ) -> HashSet<String> {
        let mut candidates = HashSet::new();
        let mut known_i64_bindings = inherited_known_i64_bindings.clone();

        for (index, stmt) in block.stmts.iter().enumerate() {
            if let Stmt::Let {
                pattern: Pattern::Binding { name, .. },
                ty: Some(_),
                value: Some(value),
                ..
            } = stmt
            {
                let is_bounded_helper_alloc =
                    Self::helper_alloc_storage_layout_with_bindings(value, &known_i64_bindings)
                        .is_some();
                if is_bounded_helper_alloc
                    && Self::remaining_statements_preserve_ephemeral_contract(
                        &block.stmts[index + 1..],
                        name,
                    )
                {
                    candidates.insert(name.clone());
                }
            }

            if let Some((name, literal)) = Self::stmt_binds_i64_literal(stmt, &known_i64_bindings) {
                known_i64_bindings.insert(name, literal);
            }
        }

        candidates
    }

    fn collect_block_ephemeral_zero_init_elision_names(
        &self,
        block: &Block,
        inherited_known_i64_bindings: &HashMap<String, i64>,
        inherited_known_llvm_types: &HashMap<String, String>,
    ) -> HashSet<String> {
        let mut candidates = HashSet::new();
        let mut known_i64_bindings = inherited_known_i64_bindings.clone();
        let mut known_llvm_types = inherited_known_llvm_types.clone();

        for (index, stmt) in block.stmts.iter().enumerate() {
            if let Stmt::Let {
                pattern: Pattern::Binding { name, .. },
                ty: Some(_),
                value: Some(value),
                ..
            } = stmt
            {
                if let Some(layout) =
                    Self::helper_alloc_storage_layout_with_bindings(value, &known_i64_bindings)
                {
                    if layout.zeroed
                        && Self::helper_alloc_is_single_cell(value, &known_i64_bindings)
                        && Self::remaining_statements_preserve_ephemeral_contract(
                            &block.stmts[index + 1..],
                            name,
                        )
                        && self.remaining_statements_allow_ephemeral_zero_init_elision(
                            &block.stmts[index + 1..],
                            name,
                            layout.byte_len,
                            &known_llvm_types,
                        )
                    {
                        candidates.insert(name.clone());
                    }
                }
            }

            if let Some((name, literal)) = Self::stmt_binds_i64_literal(stmt, &known_i64_bindings) {
                known_i64_bindings.insert(name, literal);
            }
            if let Some((name, llvm_ty)) = self.stmt_binds_obvious_llvm_ty(stmt, &known_llvm_types)
            {
                known_llvm_types.insert(name, llvm_ty);
            }
        }

        candidates
    }

    fn expr_is_fixed_i64_array_literal(expr: &Expr) -> bool {
        let items = match expr {
            Expr::Array(items, _) => items,
            Expr::MacroCall { name, args, .. } if name == "vec" => args,
            _ => return false,
        };
        !items.is_empty()
            && items.iter().all(|item| {
                matches!(
                    item,
                    Expr::Int(_, _)
                        | Expr::Bool(_, _)
                        | Expr::Paren(_, _)
                        | Expr::Cast { .. }
                        | Expr::Binary { .. }
                )
            })
    }

    fn expr_is_safe_fixed_array_use(expr: &Expr, target: &str) -> bool {
        match expr {
            Expr::Ident(name, _) => name != target,
            Expr::Call { callee, args, .. } => {
                if matches!(callee.as_ref(), Expr::Ident(name, _) if name == "len")
                    && args.len() == 1
                    && Self::expr_is_exact_target_pointer(&args[0].value, target)
                {
                    return true;
                }
                Self::expr_is_safe_fixed_array_use(callee, target)
                    && args
                        .iter()
                        .all(|arg| Self::expr_is_safe_fixed_array_use(&arg.value, target))
            }
            Expr::Index { object, index, .. } => {
                if Self::expr_is_exact_target_pointer(object, target) {
                    !Self::debug_mentions_identifier(index, target)
                } else {
                    Self::expr_is_safe_fixed_array_use(object, target)
                        && Self::expr_is_safe_fixed_array_use(index, target)
                }
            }
            Expr::Assign {
                target: assign_target,
                value,
                ..
            } => {
                !Self::debug_mentions_identifier(assign_target, target)
                    && Self::expr_is_safe_fixed_array_use(assign_target, target)
                    && Self::expr_is_safe_fixed_array_use(value, target)
            }
            Expr::Binary { left, right, .. } => {
                Self::expr_is_safe_fixed_array_use(left, target)
                    && Self::expr_is_safe_fixed_array_use(right, target)
            }
            Expr::Unary { operand, .. }
            | Expr::Paren(operand, _)
            | Expr::Deref(operand, _)
            | Expr::Try(operand, _)
            | Expr::Await(operand, _)
            | Expr::AsyncBlock(operand, _)
            | Expr::Comptime(operand, _)
            | Expr::Return(Some(operand), _)
            | Expr::Break(Some(operand), _) => Self::expr_is_safe_fixed_array_use(operand, target),
            Expr::Return(None, _) | Expr::Break(None, _) => true,
            Expr::FString(parts, _) | Expr::Array(parts, _) | Expr::Tuple(parts, _) => parts
                .iter()
                .all(|part| Self::expr_is_safe_fixed_array_use(part, target)),
            Expr::MacroCall { args, .. } => args
                .iter()
                .all(|arg| Self::expr_is_safe_fixed_array_use(arg, target)),
            Expr::StageCall { args, .. } => args
                .iter()
                .all(|arg| Self::expr_is_safe_fixed_array_use(&arg.value, target)),
            Expr::MethodCall { receiver, args, .. } => {
                Self::expr_is_safe_fixed_array_use(receiver, target)
                    && args
                        .iter()
                        .all(|arg| Self::expr_is_safe_fixed_array_use(&arg.value, target))
            }
            Expr::Field { object, .. } => Self::expr_is_safe_fixed_array_use(object, target),
            Expr::Struct { fields, rest, .. } => {
                fields
                    .iter()
                    .all(|(_, value)| Self::expr_is_safe_fixed_array_use(value, target))
                    && rest
                        .as_ref()
                        .map(|value| Self::expr_is_safe_fixed_array_use(value, target))
                        .unwrap_or(true)
            }
            Expr::AggregateInit { fields, .. } => fields
                .iter()
                .all(|(_, value)| Self::expr_is_safe_fixed_array_use(value, target)),
            Expr::EnumVariant { fields, .. } => match fields {
                kain_core::ast::EnumVariantFields::Unit => true,
                kain_core::ast::EnumVariantFields::Tuple(values) => values
                    .iter()
                    .all(|value| Self::expr_is_safe_fixed_array_use(value, target)),
                kain_core::ast::EnumVariantFields::Struct(values) => values
                    .iter()
                    .all(|(_, value)| Self::expr_is_safe_fixed_array_use(value, target)),
            },
            Expr::If {
                condition,
                then_branch,
                else_branch,
                ..
            } => {
                Self::expr_is_safe_fixed_array_use(condition, target)
                    && Self::block_is_safe_fixed_array_use(then_branch, target)
                    && else_branch
                        .as_ref()
                        .map(|branch| match branch.as_ref() {
                            ElseBranch::Else(block) => {
                                Self::block_is_safe_fixed_array_use(block, target)
                            }
                            ElseBranch::ElseIf(condition, block, next) => {
                                Self::expr_is_safe_fixed_array_use(condition, target)
                                    && Self::block_is_safe_fixed_array_use(block, target)
                                    && next
                                        .as_ref()
                                        .map(|nested| match nested.as_ref() {
                                            ElseBranch::Else(block) => {
                                                Self::block_is_safe_fixed_array_use(block, target)
                                            }
                                            ElseBranch::ElseIf(..) => {
                                                !Self::debug_mentions_identifier(nested, target)
                                            }
                                        })
                                        .unwrap_or(true)
                            }
                        })
                        .unwrap_or(true)
            }
            Expr::Block(block, _) => Self::block_is_safe_fixed_array_use(block, target),
            other => !Self::debug_mentions_identifier(other, target),
        }
    }

    fn stmt_is_safe_fixed_array_use(stmt: &Stmt, target: &str) -> bool {
        match stmt {
            Stmt::Let { value, .. } => value
                .as_ref()
                .map(|expr| Self::expr_is_safe_fixed_array_use(expr, target))
                .unwrap_or(true),
            Stmt::Expr(expr) => Self::expr_is_safe_fixed_array_use(expr, target),
            Stmt::Return(Some(expr), _) => Self::expr_is_safe_fixed_array_use(expr, target),
            Stmt::Return(None, _) | Stmt::Break(None, _) | Stmt::Continue(_) => true,
            Stmt::Break(Some(expr), _) => Self::expr_is_safe_fixed_array_use(expr, target),
            Stmt::For { iter, body, .. } => {
                Self::expr_is_safe_fixed_array_use(iter, target)
                    && Self::block_is_safe_fixed_array_use(body, target)
            }
            Stmt::While {
                condition, body, ..
            } => {
                Self::expr_is_safe_fixed_array_use(condition, target)
                    && Self::block_is_safe_fixed_array_use(body, target)
            }
            Stmt::Loop { body, .. } => Self::block_is_safe_fixed_array_use(body, target),
            Stmt::Item(item) => !Self::debug_mentions_identifier(item, target),
        }
    }

    fn block_is_safe_fixed_array_use(block: &Block, target: &str) -> bool {
        block
            .stmts
            .iter()
            .all(|stmt| Self::stmt_is_safe_fixed_array_use(stmt, target))
    }

    fn collect_block_fixed_array_candidate_names(block: &Block) -> HashSet<String> {
        let mut candidates = HashSet::new();
        for (index, stmt) in block.stmts.iter().enumerate() {
            if let Stmt::Let {
                pattern: Pattern::Binding { name, .. },
                value: Some(value),
                ..
            } = stmt
            {
                if Self::expr_is_fixed_i64_array_literal(value)
                    && block.stmts[index + 1..]
                        .iter()
                        .all(|stmt| Self::stmt_is_safe_fixed_array_use(stmt, name))
                {
                    candidates.insert(name.clone());
                }
            }
        }
        candidates
    }

    fn expr_is_direct_shattered_array_literal(&self, expr: &Expr) -> bool {
        self.shattered_array_expr_struct_name(expr).is_some()
    }

    fn expr_matches_closed_shatter_field_projection(expr: &Expr, target: &str) -> bool {
        match expr {
            Expr::Field { object, .. } => match object.as_ref() {
                Expr::Index {
                    object: indexed_object,
                    index,
                    ..
                } if Self::expr_is_exact_target_pointer(indexed_object, target) => {
                    !Self::debug_mentions_identifier(index, target)
                }
                Expr::Paren(inner, _) | Expr::Cast { value: inner, .. } => {
                    Self::expr_matches_closed_shatter_field_projection(inner, target)
                }
                _ => false,
            },
            Expr::Paren(inner, _) | Expr::Cast { value: inner, .. } => {
                Self::expr_matches_closed_shatter_field_projection(inner, target)
            }
            _ => false,
        }
    }

    fn expr_is_safe_stack_shatter_use(expr: &Expr, target: &str) -> bool {
        if Self::expr_matches_closed_shatter_field_projection(expr, target) {
            return true;
        }
        match expr {
            Expr::Ident(name, _) => name != target,
            Expr::Call { callee, args, .. } => {
                if matches!(callee.as_ref(), Expr::Ident(name, _) if name == "len")
                    && args.len() == 1
                    && Self::expr_is_exact_target_pointer(&args[0].value, target)
                {
                    return true;
                }
                Self::expr_is_safe_stack_shatter_use(callee, target)
                    && args
                        .iter()
                        .all(|arg| Self::expr_is_safe_stack_shatter_use(&arg.value, target))
            }
            Expr::Index { object, index, .. } => {
                if Self::expr_is_exact_target_pointer(object, target) {
                    false
                } else {
                    Self::expr_is_safe_stack_shatter_use(object, target)
                        && Self::expr_is_safe_stack_shatter_use(index, target)
                }
            }
            Expr::Assign {
                target: assign_target,
                value,
                ..
            } => {
                !Self::debug_mentions_identifier(assign_target, target)
                    && Self::expr_is_safe_stack_shatter_use(assign_target, target)
                    && Self::expr_is_safe_stack_shatter_use(value, target)
            }
            Expr::Binary { left, right, .. } => {
                Self::expr_is_safe_stack_shatter_use(left, target)
                    && Self::expr_is_safe_stack_shatter_use(right, target)
            }
            Expr::Unary { operand, .. }
            | Expr::Paren(operand, _)
            | Expr::Deref(operand, _)
            | Expr::Try(operand, _)
            | Expr::Await(operand, _)
            | Expr::AsyncBlock(operand, _)
            | Expr::Comptime(operand, _)
            | Expr::Return(Some(operand), _)
            | Expr::Break(Some(operand), _) => {
                Self::expr_is_safe_stack_shatter_use(operand, target)
            }
            Expr::Return(None, _) | Expr::Break(None, _) => true,
            Expr::FString(parts, _) | Expr::Array(parts, _) | Expr::Tuple(parts, _) => parts
                .iter()
                .all(|part| Self::expr_is_safe_stack_shatter_use(part, target)),
            Expr::MacroCall { args, .. } => args
                .iter()
                .all(|arg| Self::expr_is_safe_stack_shatter_use(arg, target)),
            Expr::StageCall { args, .. } => args
                .iter()
                .all(|arg| Self::expr_is_safe_stack_shatter_use(&arg.value, target)),
            Expr::MethodCall { receiver, args, .. } => {
                Self::expr_is_safe_stack_shatter_use(receiver, target)
                    && args
                        .iter()
                        .all(|arg| Self::expr_is_safe_stack_shatter_use(&arg.value, target))
            }
            Expr::Field { object, .. } => Self::expr_is_safe_stack_shatter_use(object, target),
            Expr::Struct { fields, rest, .. } => {
                fields
                    .iter()
                    .all(|(_, value)| Self::expr_is_safe_stack_shatter_use(value, target))
                    && rest
                        .as_ref()
                        .map(|value| Self::expr_is_safe_stack_shatter_use(value, target))
                        .unwrap_or(true)
            }
            Expr::AggregateInit { fields, .. } => fields
                .iter()
                .all(|(_, value)| Self::expr_is_safe_stack_shatter_use(value, target)),
            Expr::EnumVariant { fields, .. } => match fields {
                kain_core::ast::EnumVariantFields::Unit => true,
                kain_core::ast::EnumVariantFields::Tuple(values) => values
                    .iter()
                    .all(|value| Self::expr_is_safe_stack_shatter_use(value, target)),
                kain_core::ast::EnumVariantFields::Struct(values) => values
                    .iter()
                    .all(|(_, value)| Self::expr_is_safe_stack_shatter_use(value, target)),
            },
            Expr::If {
                condition,
                then_branch,
                else_branch,
                ..
            } => {
                Self::expr_is_safe_stack_shatter_use(condition, target)
                    && Self::block_is_safe_stack_shatter_use(then_branch, target)
                    && else_branch
                        .as_ref()
                        .map(|branch| match branch.as_ref() {
                            ElseBranch::Else(block) => {
                                Self::block_is_safe_stack_shatter_use(block, target)
                            }
                            ElseBranch::ElseIf(condition, block, next) => {
                                Self::expr_is_safe_stack_shatter_use(condition, target)
                                    && Self::block_is_safe_stack_shatter_use(block, target)
                                    && next
                                        .as_ref()
                                        .map(|nested| match nested.as_ref() {
                                            ElseBranch::Else(block) => {
                                                Self::block_is_safe_stack_shatter_use(block, target)
                                            }
                                            ElseBranch::ElseIf(..) => {
                                                !Self::debug_mentions_identifier(nested, target)
                                            }
                                        })
                                        .unwrap_or(true)
                            }
                        })
                        .unwrap_or(true)
            }
            Expr::Block(block, _) => Self::block_is_safe_stack_shatter_use(block, target),
            other => !Self::debug_mentions_identifier(other, target),
        }
    }

    fn stmt_is_safe_stack_shatter_use(stmt: &Stmt, target: &str) -> bool {
        match stmt {
            Stmt::Let { value, .. } => value
                .as_ref()
                .map(|expr| Self::expr_is_safe_stack_shatter_use(expr, target))
                .unwrap_or(true),
            Stmt::Expr(expr) => Self::expr_is_safe_stack_shatter_use(expr, target),
            Stmt::Return(Some(expr), _) => Self::expr_is_safe_stack_shatter_use(expr, target),
            Stmt::Return(None, _) | Stmt::Break(None, _) | Stmt::Continue(_) => true,
            Stmt::Break(Some(expr), _) => Self::expr_is_safe_stack_shatter_use(expr, target),
            Stmt::For { iter, body, .. } => {
                Self::expr_is_safe_stack_shatter_use(iter, target)
                    && Self::block_is_safe_stack_shatter_use(body, target)
            }
            Stmt::While {
                condition, body, ..
            } => {
                Self::expr_is_safe_stack_shatter_use(condition, target)
                    && Self::block_is_safe_stack_shatter_use(body, target)
            }
            Stmt::Loop { body, .. } => Self::block_is_safe_stack_shatter_use(body, target),
            Stmt::Item(item) => !Self::debug_mentions_identifier(item, target),
        }
    }

    fn block_is_safe_stack_shatter_use(block: &Block, target: &str) -> bool {
        block
            .stmts
            .iter()
            .all(|stmt| Self::stmt_is_safe_stack_shatter_use(stmt, target))
    }

    fn collect_block_stack_shatter_candidate_names(&self, block: &Block) -> HashSet<String> {
        let mut candidates = HashSet::new();
        for (index, stmt) in block.stmts.iter().enumerate() {
            if let Stmt::Let {
                pattern: Pattern::Binding { name, .. },
                value: Some(value),
                ..
            } = stmt
            {
                if self.expr_is_direct_shattered_array_literal(value)
                    && block.stmts[index + 1..]
                        .iter()
                        .all(|stmt| Self::stmt_is_safe_stack_shatter_use(stmt, name))
                {
                    candidates.insert(name.clone());
                }
            }
        }
        candidates
    }

    fn expr_is_literal_map_seed(expr: &Expr) -> bool {
        matches!(
            expr,
            Expr::Call { callee, args, .. }
                if matches!(callee.as_ref(), Expr::Ident(name, _) if name == "map_new") && args.is_empty()
        )
    }

    fn expr_matches_literal_map_set(expr: &Expr, target: &str) -> bool {
        let Expr::Call { callee, args, .. } = expr else {
            return false;
        };
        matches!(callee.as_ref(), Expr::Ident(name, _) if name == "map_set")
            && args.len() == 3
            && Self::expr_is_exact_target_pointer(&args[0].value, target)
            && Self::extract_string_literal(&args[1].value).is_some()
            && matches!(args[2].value, Expr::Int(_, _))
    }

    fn expr_is_safe_literal_map_use(expr: &Expr, target: &str) -> bool {
        match expr {
            Expr::Ident(name, _) => name != target,
            Expr::Call { callee, args, .. } => {
                if matches!(callee.as_ref(), Expr::Ident(name, _) if name == "map_get")
                    && args.len() == 2
                    && Self::expr_is_exact_target_pointer(&args[0].value, target)
                    && Self::extract_string_literal(&args[1].value).is_some()
                {
                    return true;
                }
                Self::expr_is_safe_literal_map_use(callee, target)
                    && args
                        .iter()
                        .all(|arg| Self::expr_is_safe_literal_map_use(&arg.value, target))
            }
            Expr::Index { object, index, .. } => {
                Self::expr_is_safe_literal_map_use(object, target)
                    && Self::expr_is_safe_literal_map_use(index, target)
            }
            Expr::Assign {
                target: assign_target,
                value,
                ..
            } => {
                !Self::debug_mentions_identifier(assign_target, target)
                    && Self::expr_is_safe_literal_map_use(assign_target, target)
                    && Self::expr_is_safe_literal_map_use(value, target)
            }
            Expr::Binary { left, right, .. } => {
                Self::expr_is_safe_literal_map_use(left, target)
                    && Self::expr_is_safe_literal_map_use(right, target)
            }
            Expr::Unary { operand, .. }
            | Expr::Paren(operand, _)
            | Expr::Deref(operand, _)
            | Expr::Try(operand, _)
            | Expr::Await(operand, _)
            | Expr::AsyncBlock(operand, _)
            | Expr::Comptime(operand, _)
            | Expr::Return(Some(operand), _)
            | Expr::Break(Some(operand), _) => Self::expr_is_safe_literal_map_use(operand, target),
            Expr::Return(None, _) | Expr::Break(None, _) => true,
            Expr::FString(parts, _) | Expr::Array(parts, _) | Expr::Tuple(parts, _) => parts
                .iter()
                .all(|part| Self::expr_is_safe_literal_map_use(part, target)),
            Expr::MacroCall { args, .. } => args
                .iter()
                .all(|arg| Self::expr_is_safe_literal_map_use(arg, target)),
            Expr::StageCall { args, .. } => args
                .iter()
                .all(|arg| Self::expr_is_safe_literal_map_use(&arg.value, target)),
            Expr::MethodCall { receiver, args, .. } => {
                Self::expr_is_safe_literal_map_use(receiver, target)
                    && args
                        .iter()
                        .all(|arg| Self::expr_is_safe_literal_map_use(&arg.value, target))
            }
            Expr::Field { object, .. } => Self::expr_is_safe_literal_map_use(object, target),
            Expr::Struct { fields, rest, .. } => {
                fields
                    .iter()
                    .all(|(_, value)| Self::expr_is_safe_literal_map_use(value, target))
                    && rest
                        .as_ref()
                        .map(|value| Self::expr_is_safe_literal_map_use(value, target))
                        .unwrap_or(true)
            }
            Expr::EnumVariant { fields, .. } => match fields {
                kain_core::ast::EnumVariantFields::Unit => true,
                kain_core::ast::EnumVariantFields::Tuple(values) => values
                    .iter()
                    .all(|value| Self::expr_is_safe_literal_map_use(value, target)),
                kain_core::ast::EnumVariantFields::Struct(values) => values
                    .iter()
                    .all(|(_, value)| Self::expr_is_safe_literal_map_use(value, target)),
            },
            Expr::If {
                condition,
                then_branch,
                else_branch,
                ..
            } => {
                Self::expr_is_safe_literal_map_use(condition, target)
                    && Self::block_is_safe_literal_map_use(then_branch, target)
                    && else_branch
                        .as_ref()
                        .map(|branch| match branch.as_ref() {
                            ElseBranch::Else(block) => {
                                Self::block_is_safe_literal_map_use(block, target)
                            }
                            ElseBranch::ElseIf(condition, block, next) => {
                                Self::expr_is_safe_literal_map_use(condition, target)
                                    && Self::block_is_safe_literal_map_use(block, target)
                                    && next
                                        .as_ref()
                                        .map(|nested| match nested.as_ref() {
                                            ElseBranch::Else(block) => {
                                                Self::block_is_safe_literal_map_use(block, target)
                                            }
                                            ElseBranch::ElseIf(..) => {
                                                !Self::debug_mentions_identifier(nested, target)
                                            }
                                        })
                                        .unwrap_or(true)
                            }
                        })
                        .unwrap_or(true)
            }
            Expr::Block(block, _) => Self::block_is_safe_literal_map_use(block, target),
            other => !Self::debug_mentions_identifier(other, target),
        }
    }

    fn stmt_is_safe_literal_map_use(stmt: &Stmt, target: &str) -> bool {
        match stmt {
            Stmt::Let { value, .. } => value
                .as_ref()
                .map(|expr| Self::expr_is_safe_literal_map_use(expr, target))
                .unwrap_or(true),
            Stmt::Expr(expr) => {
                Self::expr_matches_literal_map_set(expr, target)
                    || Self::expr_is_safe_literal_map_use(expr, target)
            }
            Stmt::Return(Some(expr), _) => Self::expr_is_safe_literal_map_use(expr, target),
            Stmt::Return(None, _) | Stmt::Break(None, _) | Stmt::Continue(_) => true,
            Stmt::Break(Some(expr), _) => Self::expr_is_safe_literal_map_use(expr, target),
            Stmt::For { iter, body, .. } => {
                Self::expr_is_safe_literal_map_use(iter, target)
                    && Self::block_is_safe_literal_map_use(body, target)
            }
            Stmt::While {
                condition, body, ..
            } => {
                Self::expr_is_safe_literal_map_use(condition, target)
                    && Self::block_is_safe_literal_map_use(body, target)
            }
            Stmt::Loop { body, .. } => Self::block_is_safe_literal_map_use(body, target),
            Stmt::Item(item) => !Self::debug_mentions_identifier(item, target),
        }
    }

    fn block_is_safe_literal_map_use(block: &Block, target: &str) -> bool {
        block
            .stmts
            .iter()
            .all(|stmt| Self::stmt_is_safe_literal_map_use(stmt, target))
    }

    fn collect_block_literal_map_candidate_names(block: &Block) -> HashSet<String> {
        let mut candidates = HashSet::new();
        for (index, stmt) in block.stmts.iter().enumerate() {
            if let Stmt::Let {
                pattern: Pattern::Binding { name, .. },
                value: Some(value),
                ..
            } = stmt
            {
                if Self::expr_is_literal_map_seed(value)
                    && block.stmts[index + 1..]
                        .iter()
                        .all(|stmt| Self::stmt_is_safe_literal_map_use(stmt, name))
                {
                    candidates.insert(name.clone());
                }
            }
        }
        candidates
    }

    fn extract_string_literal(expr: &Expr) -> Option<String> {
        match expr {
            Expr::String(value, _) => Some(value.clone()),
            Expr::Paren(inner, _) => Self::extract_string_literal(inner),
            _ => None,
        }
    }

    fn extract_static_string_literal(&self, expr: &Expr) -> Option<String> {
        match expr {
            Expr::String(value, _) => Some(value.clone()),
            Expr::Paren(inner, _) => self.extract_static_string_literal(inner),
            Expr::Ident(name, _) => self
                .const_globals
                .get(name)
                .and_then(|info| info.string_literal.clone()),
            _ => None,
        }
    }

    fn is_known_string_ident(&self, name: &str) -> bool {
        self.string_locals.contains(name)
            || self
                .const_globals
                .get(name)
                .map(|info| info.is_known_string)
                .unwrap_or(false)
    }

    fn expr_is_known_string(&self, expr: &Expr) -> bool {
        match expr {
            Expr::String(..) | Expr::FString(..) => true,
            Expr::Paren(inner, _) => self.expr_is_known_string(inner),
            Expr::Ident(name, _) => self.is_known_string_ident(name),
            Expr::Binary {
                left, op, right, ..
            } => {
                *op == BinaryOp::Add
                    && (self.expr_is_known_string(left) || self.expr_is_known_string(right))
            }
            _ => false,
        }
    }

    fn expr_static_string_bytes(&self, expr: &Expr) -> Option<Vec<u8>> {
        match Self::expr_strip_parens(expr) {
            Expr::String(value, _) => Some(value.as_bytes().to_vec()),
            Expr::Ident(name, _) => self.const_globals.get(name).and_then(|info| {
                info.string_literal
                    .as_ref()
                    .map(|literal| literal.as_bytes().to_vec())
            }),
            _ => None,
        }
    }

    fn collect_string_concat_terms<'a>(&self, expr: &'a Expr, terms: &mut Vec<&'a Expr>) {
        match expr {
            Expr::Paren(inner, _) => self.collect_string_concat_terms(inner, terms),
            Expr::Binary {
                left, op, right, ..
            } if *op == BinaryOp::Add && self.expr_is_known_string(expr) => {
                if self.expr_is_known_string(left) {
                    self.collect_string_concat_terms(left, terms);
                } else {
                    terms.push(left);
                }
                if self.expr_is_known_string(right) {
                    self.collect_string_concat_terms(right, terms);
                } else {
                    terms.push(right);
                }
            }
            _ => terms.push(expr),
        }
    }

    fn emit_fixed_arity_string_concat_call(&mut self, values: &[String]) -> Option<String> {
        if !(3..=10).contains(&values.len()) {
            return None;
        }
        let res = self.next_reg();
        let args = values
            .iter()
            .map(|value| format!("i8* {}", value))
            .collect::<Vec<_>>()
            .join(", ");
        self.emit(&format!(
            "  {} = call i8* @str_concat{}({})",
            res,
            values.len(),
            args
        ));
        Some(res)
    }

    fn compile_string_concat_expression(
        &mut self,
        expr: &Expr,
    ) -> KainResult<Option<(String, String)>> {
        if !self.expr_is_known_string(expr) {
            return Ok(None);
        }
        let mut term_exprs = Vec::new();
        self.collect_string_concat_terms(expr, &mut term_exprs);
        if term_exprs.len() < 3 {
            return Ok(None);
        }

        let mut compiled_terms = Vec::with_capacity(term_exprs.len());
        for term in term_exprs {
            let (value, ty) = self.compile_expr(term)?;
            if ty == "i8*" {
                compiled_terms.push((value, self.is_new_object(term)));
            } else {
                let (string_value, string_ty) = self.stringify_value(&value, &ty)?;
                debug_assert_eq!(string_ty, "i8*");
                compiled_terms.push((string_value, true));
            }
        }

        let term_values = compiled_terms
            .iter()
            .map(|(value, _)| value.clone())
            .collect::<Vec<_>>();

        let result = if let Some(flattened) = self.emit_fixed_arity_string_concat_call(&term_values)
        {
            for (value, release_after_use) in &compiled_terms {
                if *release_after_use {
                    self.emit_rc_release_if_heap_i8(value);
                }
            }
            flattened
        } else {
            let (mut acc, mut acc_release_after_use) = compiled_terms[0].clone();
            for (value, value_release_after_use) in compiled_terms.iter().skip(1) {
                let previous_acc = acc.clone();
                let previous_acc_release_after_use = acc_release_after_use;
                acc = self.concat_strings(&previous_acc, value);
                if previous_acc_release_after_use {
                    self.emit_rc_release_if_heap_i8(&previous_acc);
                }
                if *value_release_after_use {
                    self.emit_rc_release_if_heap_i8(value);
                }
                acc_release_after_use = true;
            }
            acc
        };

        Ok(Some((result, "i8*".to_string())))
    }

    fn compile_string_data_pointer_for_byte_view(&mut self, expr: &Expr) -> KainResult<String> {
        match expr {
            Expr::String(value, _) => Ok(self.compile_static_c_string_literal(value)),
            Expr::Paren(inner, _) => self.compile_string_data_pointer_for_byte_view(inner),
            Expr::Ident(name, _) => {
                if let Some(literal) = self
                    .const_globals
                    .get(name)
                    .and_then(|info| info.string_literal.clone())
                {
                    return Ok(self.compile_static_c_string_literal(&literal));
                }
                let (value, value_ty) = self.compile_expr(expr)?;
                if value_ty != "i8*" {
                    return Err(KainError::codegen(
                        format!("expected string pointer for byte view, found {}", value_ty),
                        expr.span(),
                    ));
                }
                Ok(value)
            }
            _ => {
                let (value, value_ty) = self.compile_expr(expr)?;
                if value_ty != "i8*" {
                    return Err(KainError::codegen(
                        format!("expected string pointer for byte view, found {}", value_ty),
                        expr.span(),
                    ));
                }
                Ok(value)
            }
        }
    }

    fn compile_string_length_value(&mut self, expr: &Expr) -> KainResult<Option<String>> {
        match expr {
            Expr::String(value, _) => return Ok(Some(value.len().to_string())),
            Expr::Paren(inner, _) => return self.compile_string_length_value(inner),
            Expr::Ident(name, _) => {
                if let Some(length_reg) = self.string_length_values.get(name) {
                    return Ok(Some(length_reg.clone()));
                }
                if let Some(info) = self.const_globals.get(name) {
                    if let Some(string_len) = info.string_byte_len {
                        return Ok(Some(string_len.to_string()));
                    }
                }
            }
            _ => {}
        }

        if !self.expr_is_known_string(expr) {
            return Ok(None);
        }

        let ptr = self.compile_string_data_pointer_for_byte_view(expr)?;
        let len = self.next_reg();
        self.emit(&format!("  {} = call i64 @len(i8* {})", len, ptr));
        if let Expr::Ident(name, _) = expr {
            if self.string_locals.contains(name) {
                self.string_length_values.insert(name.clone(), len.clone());
            }
        }
        Ok(Some(len))
    }

    fn prime_string_param_length_cache(&mut self, param_name: &str, addr_reg: &str) {
        if self.string_length_values.contains_key(param_name) {
            return;
        }
        let ptr = self.next_reg();
        self.emit(&format!("  {} = load i8*, i8** {}", ptr, addr_reg));
        let len = self.next_reg();
        self.emit(&format!("  {} = call i64 @len(i8* {})", len, ptr));
        self.string_length_values
            .insert(param_name.to_string(), len);
    }

    fn compile_expr_as_i64(&mut self, expr: &Expr) -> KainResult<String> {
        let (value, value_ty) = self.compile_expr(expr)?;
        if value_ty == "i64" {
            return Ok(value);
        }
        self.cast_numeric_value(value, &value_ty, "i64")
    }

    fn decompose_char_at_call<'a>(&self, expr: &'a Expr) -> Option<(&'a Expr, &'a Expr)> {
        match expr {
            Expr::Paren(inner, _) => self.decompose_char_at_call(inner),
            Expr::Call { callee, args, .. } => match callee.as_ref() {
                Expr::Ident(name, _) if name == "char_at" && args.len() == 2 => {
                    Some((&args[0].value, &args[1].value))
                }
                _ => None,
            },
            _ => None,
        }
    }

    fn compile_char_at_string_equality_fast_path(
        &mut self,
        left: &Expr,
        right: &Expr,
    ) -> KainResult<Option<String>> {
        let Some((left_text, left_index_expr)) = self.decompose_char_at_call(left) else {
            return Ok(None);
        };
        let Some((right_text, right_index_expr)) = self.decompose_char_at_call(right) else {
            return Ok(None);
        };
        if !self.expr_is_known_string(left_text) || !self.expr_is_known_string(right_text) {
            return Ok(None);
        }

        let left_ptr = self.compile_string_data_pointer_for_byte_view(left_text)?;
        let right_ptr = self.compile_string_data_pointer_for_byte_view(right_text)?;
        let Some(left_len) = self.compile_string_length_value(left_text)? else {
            return Ok(None);
        };
        let Some(right_len) = self.compile_string_length_value(right_text)? else {
            return Ok(None);
        };
        let left_index = self.compile_expr_as_i64(left_index_expr)?;
        let right_index = self.compile_expr_as_i64(right_index_expr)?;

        let left_non_negative = self.next_reg();
        self.emit(&format!(
            "  {} = icmp sge i64 {}, 0",
            left_non_negative, left_index
        ));
        let left_below_len = self.next_reg();
        self.emit(&format!(
            "  {} = icmp slt i64 {}, {}",
            left_below_len, left_index, left_len
        ));
        let left_valid = self.next_reg();
        self.emit(&format!(
            "  {} = and i1 {}, {}",
            left_valid, left_non_negative, left_below_len
        ));

        let right_non_negative = self.next_reg();
        self.emit(&format!(
            "  {} = icmp sge i64 {}, 0",
            right_non_negative, right_index
        ));
        let right_below_len = self.next_reg();
        self.emit(&format!(
            "  {} = icmp slt i64 {}, {}",
            right_below_len, right_index, right_len
        ));
        let right_valid = self.next_reg();
        self.emit(&format!(
            "  {} = and i1 {}, {}",
            right_valid, right_non_negative, right_below_len
        ));

        let left_invalid = self.next_reg();
        self.emit(&format!("  {} = xor i1 {}, 1", left_invalid, left_valid));
        let right_invalid = self.next_reg();
        self.emit(&format!("  {} = xor i1 {}, 1", right_invalid, right_valid));
        let both_invalid = self.next_reg();
        self.emit(&format!(
            "  {} = and i1 {}, {}",
            both_invalid, left_invalid, right_invalid
        ));
        let both_valid = self.next_reg();
        self.emit(&format!(
            "  {} = and i1 {}, {}",
            both_valid, left_valid, right_valid
        ));

        let start_block = self.current_block.clone();
        let load_block = self.next_label();
        let merge_block = self.next_label();
        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            both_valid, load_block, merge_block
        ));

        self.emit_label(&load_block);
        let left_byte_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = getelementptr inbounds i8, i8* {}, i64 {}",
            left_byte_ptr, left_ptr, left_index
        ));
        let left_byte = self.next_reg();
        self.emit(&format!(
            "  {} = load i8, i8* {}, align 1",
            left_byte, left_byte_ptr
        ));
        let right_byte_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = getelementptr inbounds i8, i8* {}, i64 {}",
            right_byte_ptr, right_ptr, right_index
        ));
        let right_byte = self.next_reg();
        self.emit(&format!(
            "  {} = load i8, i8* {}, align 1",
            right_byte, right_byte_ptr
        ));
        let byte_eq = self.next_reg();
        self.emit(&format!(
            "  {} = icmp eq i8 {}, {}",
            byte_eq, left_byte, right_byte
        ));
        let load_end_block = self.current_block.clone();
        self.emit(&format!("  br label %{}", merge_block));

        self.emit_label(&merge_block);
        let result = self.next_reg();
        self.emit(&format!(
            "  {} = phi i1 [ {}, %{} ], [ {}, %{} ]",
            result, both_invalid, start_block, byte_eq, load_end_block
        ));
        Ok(Some(result))
    }

    fn compile_byte_at_fast_path(
        &mut self,
        text: &Expr,
        index_expr: &Expr,
    ) -> KainResult<Option<String>> {
        if !self.expr_is_known_string(text) {
            return Ok(None);
        }

        let text_ptr = self.compile_string_data_pointer_for_byte_view(text)?;
        let Some(text_len) = self.compile_string_length_value(text)? else {
            return Ok(None);
        };
        let index = self.compile_expr_as_i64(index_expr)?;

        let text_non_null = self.next_reg();
        self.emit(&format!(
            "  {} = icmp ne i8* {}, null",
            text_non_null, text_ptr
        ));
        let index_non_negative = self.next_reg();
        self.emit(&format!(
            "  {} = icmp sge i64 {}, 0",
            index_non_negative, index
        ));
        let index_below_len = self.next_reg();
        self.emit(&format!(
            "  {} = icmp slt i64 {}, {}",
            index_below_len, index, text_len
        ));
        let index_in_bounds = self.next_reg();
        self.emit(&format!(
            "  {} = and i1 {}, {}",
            index_in_bounds, index_non_negative, index_below_len
        ));
        let valid = self.next_reg();
        self.emit(&format!(
            "  {} = and i1 {}, {}",
            valid, text_non_null, index_in_bounds
        ));

        let start_block = self.current_block.clone();
        let load_block = self.next_label();
        let merge_block = self.next_label();
        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            valid, load_block, merge_block
        ));

        self.emit_label(&load_block);
        let byte_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = getelementptr inbounds i8, i8* {}, i64 {}",
            byte_ptr, text_ptr, index
        ));
        let byte_value = self.next_reg();
        self.emit(&format!(
            "  {} = load i8, i8* {}, align 1",
            byte_value, byte_ptr
        ));
        let byte_i64 = self.next_reg();
        self.emit(&format!("  {} = zext i8 {} to i64", byte_i64, byte_value));
        let load_end_block = self.current_block.clone();
        self.emit(&format!("  br label %{}", merge_block));

        self.emit_label(&merge_block);
        let result = self.next_reg();
        self.emit(&format!(
            "  {} = phi i64 [ -1, %{} ], [ {}, %{} ]",
            result, start_block, byte_i64, load_end_block
        ));
        Ok(Some(result))
    }

    fn compile_find_substring_from_fast_path(
        &mut self,
        text: &Expr,
        needle: &Expr,
        start_expr: &Expr,
    ) -> KainResult<Option<String>> {
        if !self.expr_is_known_string(text) || !self.expr_is_known_string(needle) {
            return Ok(None);
        }

        let text_ptr = self.compile_string_data_pointer_for_byte_view(text)?;
        let Some(text_len) = self.compile_string_length_value(text)? else {
            return Ok(None);
        };
        let needle_ptr = self.compile_string_data_pointer_for_byte_view(needle)?;
        let Some(needle_len) = self.compile_string_length_value(needle)? else {
            return Ok(None);
        };
        let start = self.compile_expr_as_i64(start_expr)?;
        let needle_static_bytes = self.expr_static_string_bytes(needle);
        Ok(Some(self.compile_known_length_find_substring_inline(
            &text_ptr,
            &text_len,
            &needle_ptr,
            &needle_len,
            &start,
            needle_static_bytes.as_deref(),
        )))
    }

    fn compile_known_length_find_substring_inline(
        &mut self,
        text_ptr: &str,
        text_len: &str,
        needle_ptr: &str,
        needle_len: &str,
        start: &str,
        needle_static_bytes: Option<&[u8]>,
    ) -> String {
        if let Some(bytes) = needle_static_bytes {
            if bytes.len() == 2 {
                // Proof-backed short-needle lane: a stride-1 packed compare is
                // cheaper than bouncing through memchr for tiny constant needles.
                return self.compile_known_length_find_substring_inline_static_two_byte_needle(
                    text_ptr, text_len, start, bytes[0], bytes[1],
                );
            }
        }
        let empty_check = self.next_label();
        let setup = self.next_label();
        let search = self.next_label();
        let compare = self.next_label();
        let continue_search = self.next_label();
        let tail_check = self.next_label();
        let found_match = self.next_label();
        let fail = self.next_label();
        let merge = self.next_label();
        let next_cursor = self.next_reg();
        let next_remaining = self.next_reg();

        let text_non_null = self.next_reg();
        self.emit(&format!(
            "  {} = icmp ne i8* {}, null",
            text_non_null, text_ptr
        ));
        let needle_non_null = self.next_reg();
        self.emit(&format!(
            "  {} = icmp ne i8* {}, null",
            needle_non_null, needle_ptr
        ));
        let non_null = self.next_reg();
        self.emit(&format!(
            "  {} = and i1 {}, {}",
            non_null, text_non_null, needle_non_null
        ));
        let start_non_negative = self.next_reg();
        self.emit(&format!(
            "  {} = icmp sge i64 {}, 0",
            start_non_negative, start
        ));
        let clamped_start = self.next_reg();
        self.emit(&format!(
            "  {} = select i1 {}, i64 {}, i64 0",
            clamped_start, start_non_negative, start
        ));
        let start_in_bounds = self.next_reg();
        self.emit(&format!(
            "  {} = icmp sle i64 {}, {}",
            start_in_bounds, clamped_start, text_len
        ));
        let start_valid = self.next_reg();
        self.emit(&format!(
            "  {} = and i1 {}, {}",
            start_valid, non_null, start_in_bounds
        ));
        let needle_empty = self.next_reg();
        self.emit(&format!(
            "  {} = icmp eq i64 {}, 0",
            needle_empty, needle_len
        ));
        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            start_valid, empty_check, fail
        ));

        self.emit_label(&empty_check);
        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            needle_empty, merge, setup
        ));

        self.emit_label(&setup);
        let remaining = self.next_reg();
        self.emit(&format!(
            "  {} = sub i64 {}, {}",
            remaining, text_len, clamped_start
        ));
        let needle_fits = self.next_reg();
        self.emit(&format!(
            "  {} = icmp sle i64 {}, {}",
            needle_fits, needle_len, remaining
        ));
        let text_base_int = self.next_reg();
        self.emit(&format!(
            "  {} = ptrtoint i8* {} to i64",
            text_base_int, text_ptr
        ));
        let start_cursor = self.next_reg();
        self.emit(&format!(
            "  {} = getelementptr inbounds i8, i8* {}, i64 {}",
            start_cursor, text_ptr, clamped_start
        ));
        let first_byte = self.next_reg();
        self.emit(&format!(
            "  {} = load i8, i8* {}, align 1",
            first_byte, needle_ptr
        ));
        let first_byte_i32 = self.next_reg();
        self.emit(&format!(
            "  {} = zext i8 {} to i32",
            first_byte_i32, first_byte
        ));
        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            needle_fits, search, fail
        ));

        self.emit_label(&search);
        let cursor = self.next_reg();
        self.emit(&format!(
            "  {} = phi i8* [ {}, %{} ], [ {}, %{} ]",
            cursor, start_cursor, setup, next_cursor, continue_search
        ));
        let remaining_phi = self.next_reg();
        self.emit(&format!(
            "  {} = phi i64 [ {}, %{} ], [ {}, %{} ]",
            remaining_phi, remaining, setup, next_remaining, continue_search
        ));
        let can_search = self.next_reg();
        self.emit(&format!(
            "  {} = icmp sge i64 {}, {}",
            can_search, remaining_phi, needle_len
        ));
        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            can_search, compare, fail
        ));

        self.emit_label(&compare);
        let search_window_minus_one = self.next_reg();
        self.emit(&format!(
            "  {} = sub i64 {}, {}",
            search_window_minus_one, remaining_phi, needle_len
        ));
        let search_window = self.next_reg();
        self.emit(&format!(
            "  {} = add i64 {}, 1",
            search_window, search_window_minus_one
        ));
        let found = self.next_reg();
        self.emit(&format!(
            "  {} = call i8* @memchr(i8* {}, i32 {}, i64 {})",
            found, cursor, first_byte_i32, search_window
        ));
        let found_non_null = self.next_reg();
        self.emit(&format!(
            "  {} = icmp ne i8* {}, null",
            found_non_null, found
        ));
        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            found_non_null, tail_check, fail
        ));

        self.emit_label(&tail_check);
        let tail_matches = if let Some(bytes) = needle_static_bytes {
            if bytes.len() <= 1 {
                "true".to_string()
            } else if bytes.len() <= 8 {
                let mut current_match = "true".to_string();
                for (offset, byte) in bytes.iter().enumerate().skip(1) {
                    let byte_ptr = self.next_reg();
                    self.emit(&format!(
                        "  {} = getelementptr inbounds i8, i8* {}, i64 {}",
                        byte_ptr, found, offset
                    ));
                    let loaded = self.next_reg();
                    self.emit(&format!(
                        "  {} = load i8, i8* {}, align 1",
                        loaded, byte_ptr
                    ));
                    let byte_matches = self.next_reg();
                    self.emit(&format!(
                        "  {} = icmp eq i8 {}, {}",
                        byte_matches, loaded, byte
                    ));
                    let all_match = self.next_reg();
                    self.emit(&format!(
                        "  {} = and i1 {}, {}",
                        all_match, current_match, byte_matches
                    ));
                    current_match = all_match;
                }
                current_match
            } else {
                let tail_len = self.next_reg();
                self.emit(&format!("  {} = sub i64 {}, 1", tail_len, needle_len));
                let found_tail = self.next_reg();
                self.emit(&format!(
                    "  {} = getelementptr inbounds i8, i8* {}, i64 1",
                    found_tail, found
                ));
                let needle_tail = self.next_reg();
                self.emit(&format!(
                    "  {} = getelementptr inbounds i8, i8* {}, i64 1",
                    needle_tail, needle_ptr
                ));
                let tail_cmp = self.next_reg();
                self.emit(&format!(
                    "  {} = call i32 @memcmp(i8* {}, i8* {}, i64 {})",
                    tail_cmp, found_tail, needle_tail, tail_len
                ));
                let cmp_ok = self.next_reg();
                self.emit(&format!("  {} = icmp eq i32 {}, 0", cmp_ok, tail_cmp));
                cmp_ok
            }
        } else {
            let tail_is_empty = self.next_reg();
            self.emit(&format!(
                "  {} = icmp eq i64 {}, 1",
                tail_is_empty, needle_len
            ));
            let found_tail = self.next_reg();
            self.emit(&format!(
                "  {} = getelementptr inbounds i8, i8* {}, i64 1",
                found_tail, found
            ));
            let needle_tail = self.next_reg();
            self.emit(&format!(
                "  {} = getelementptr inbounds i8, i8* {}, i64 1",
                needle_tail, needle_ptr
            ));
            let tail_len = self.next_reg();
            self.emit(&format!("  {} = sub i64 {}, 1", tail_len, needle_len));
            let tail_cmp = self.next_reg();
            self.emit(&format!(
                "  {} = call i32 @memcmp(i8* {}, i8* {}, i64 {})",
                tail_cmp, found_tail, needle_tail, tail_len
            ));
            let tail_cmp_ok = self.next_reg();
            self.emit(&format!("  {} = icmp eq i32 {}, 0", tail_cmp_ok, tail_cmp));
            let combined = self.next_reg();
            self.emit(&format!(
                "  {} = select i1 {}, i1 true, i1 {}",
                combined, tail_is_empty, tail_cmp_ok
            ));
            combined
        };
        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            tail_matches, found_match, continue_search
        ));

        self.emit_label(&continue_search);
        self.emit(&format!(
            "  {} = getelementptr inbounds i8, i8* {}, i64 1",
            next_cursor, found
        ));
        let cursor_next_int = self.next_reg();
        self.emit(&format!(
            "  {} = ptrtoint i8* {} to i64",
            cursor_next_int, next_cursor
        ));
        let next_offset = self.next_reg();
        self.emit(&format!(
            "  {} = sub i64 {}, {}",
            next_offset, cursor_next_int, text_base_int
        ));
        self.emit(&format!(
            "  {} = sub i64 {}, {}",
            next_remaining, text_len, next_offset
        ));
        self.emit(&format!("  br label %{}", search));

        self.emit_label(&fail);
        self.emit(&format!("  br label %{}", merge));

        self.emit_label(&found_match);
        let found_int = self.next_reg();
        self.emit(&format!("  {} = ptrtoint i8* {} to i64", found_int, found));
        let match_offset = self.next_reg();
        self.emit(&format!(
            "  {} = sub i64 {}, {}",
            match_offset, found_int, text_base_int
        ));
        self.emit(&format!("  br label %{}", merge));

        self.emit_label(&merge);
        let result = self.next_reg();
        self.emit(&format!(
            "  {} = phi i64 [ {}, %{} ], [ -1, %{} ], [ {}, %{} ]",
            result, clamped_start, empty_check, fail, match_offset, found_match
        ));
        result
    }

    fn compile_known_length_find_substring_inline_static_two_byte_needle(
        &mut self,
        text_ptr: &str,
        text_len: &str,
        start: &str,
        first: u8,
        second: u8,
    ) -> String {
        let setup = self.next_label();
        let search = self.next_label();
        let compare = self.next_label();
        let continue_search = self.next_label();
        let found_match = self.next_label();
        let fail = self.next_label();
        let merge = self.next_label();
        let next_cursor = self.next_reg();
        let next_offset = self.next_reg();
        let next_remaining = self.next_reg();

        let text_non_null = self.next_reg();
        self.emit(&format!(
            "  {} = icmp ne i8* {}, null",
            text_non_null, text_ptr
        ));
        let start_non_negative = self.next_reg();
        self.emit(&format!(
            "  {} = icmp sge i64 {}, 0",
            start_non_negative, start
        ));
        let clamped_start = self.next_reg();
        self.emit(&format!(
            "  {} = select i1 {}, i64 {}, i64 0",
            clamped_start, start_non_negative, start
        ));
        let start_in_bounds = self.next_reg();
        self.emit(&format!(
            "  {} = icmp sle i64 {}, {}",
            start_in_bounds, clamped_start, text_len
        ));
        let start_valid = self.next_reg();
        self.emit(&format!(
            "  {} = and i1 {}, {}",
            start_valid, text_non_null, start_in_bounds
        ));
        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            start_valid, setup, fail
        ));

        self.emit_label(&setup);
        let remaining = self.next_reg();
        self.emit(&format!(
            "  {} = sub i64 {}, {}",
            remaining, text_len, clamped_start
        ));
        let needle_fits = self.next_reg();
        self.emit(&format!(
            "  {} = icmp sge i64 {}, 2",
            needle_fits, remaining
        ));
        let start_cursor = self.next_reg();
        self.emit(&format!(
            "  {} = getelementptr inbounds i8, i8* {}, i64 {}",
            start_cursor, text_ptr, clamped_start
        ));
        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            needle_fits, search, fail
        ));

        self.emit_label(&search);
        let cursor = self.next_reg();
        self.emit(&format!(
            "  {} = phi i8* [ {}, %{} ], [ {}, %{} ]",
            cursor, start_cursor, setup, next_cursor, continue_search
        ));
        let cursor_offset = self.next_reg();
        self.emit(&format!(
            "  {} = phi i64 [ {}, %{} ], [ {}, %{} ]",
            cursor_offset, clamped_start, setup, next_offset, continue_search
        ));
        let remaining_phi = self.next_reg();
        self.emit(&format!(
            "  {} = phi i64 [ {}, %{} ], [ {}, %{} ]",
            remaining_phi, remaining, setup, next_remaining, continue_search
        ));
        let can_search = self.next_reg();
        self.emit(&format!(
            "  {} = icmp sge i64 {}, 2",
            can_search, remaining_phi
        ));
        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            can_search, compare, fail
        ));

        self.emit_label(&compare);
        let byte0 = self.next_reg();
        self.emit(&format!("  {} = load i8, i8* {}, align 1", byte0, cursor));
        let byte0_i16 = self.next_reg();
        self.emit(&format!("  {} = zext i8 {} to i16", byte0_i16, byte0));
        let byte1_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = getelementptr inbounds i8, i8* {}, i64 1",
            byte1_ptr, cursor
        ));
        let byte1 = self.next_reg();
        self.emit(&format!("  {} = load i8, i8* {}, align 1", byte1, byte1_ptr));
        let byte1_i16 = self.next_reg();
        self.emit(&format!("  {} = zext i8 {} to i16", byte1_i16, byte1));
        let byte1_shifted = self.next_reg();
        self.emit(&format!("  {} = shl i16 {}, 8", byte1_shifted, byte1_i16));
        let packed_window = self.next_reg();
        self.emit(&format!(
            "  {} = or i16 {}, {}",
            packed_window, byte0_i16, byte1_shifted
        ));
        let window_matches = self.next_reg();
        let packed_needle = u16::from(first) | (u16::from(second) << 8);
        self.emit(&format!(
            "  {} = icmp eq i16 {}, {}",
            window_matches, packed_window, packed_needle
        ));
        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            window_matches, found_match, continue_search
        ));

        self.emit_label(&continue_search);
        self.emit(&format!(
            "  {} = getelementptr inbounds i8, i8* {}, i64 1",
            next_cursor, cursor
        ));
        self.emit(&format!("  {} = add i64 {}, 1", next_offset, cursor_offset));
        self.emit(&format!(
            "  {} = sub i64 {}, 1",
            next_remaining, remaining_phi
        ));
        self.emit(&format!("  br label %{}", search));

        self.emit_label(&fail);
        self.emit(&format!("  br label %{}", merge));

        self.emit_label(&found_match);
        self.emit(&format!("  br label %{}", merge));

        self.emit_label(&merge);
        let result = self.next_reg();
        self.emit(&format!(
            "  {} = phi i64 [ -1, %{} ], [ {}, %{} ]",
            result, fail, cursor_offset, found_match
        ));
        result
    }

    fn expr_strip_parens<'a>(expr: &'a Expr) -> &'a Expr {
        match expr {
            Expr::Paren(inner, _) => Self::expr_strip_parens(inner),
            Expr::Cast { value, .. } => Self::expr_strip_parens(value),
            _ => expr,
        }
    }

    fn pattern_binding_name(pattern: &Pattern) -> Option<&str> {
        match pattern {
            Pattern::Binding { name, .. } => Some(name.as_str()),
            _ => None,
        }
    }

    fn expr_is_ident(expr: &Expr, expected: &str) -> bool {
        matches!(Self::expr_strip_parens(expr), Expr::Ident(name, _) if name == expected)
    }

    fn expr_int_literal(expr: &Expr) -> Option<i64> {
        match Self::expr_strip_parens(expr) {
            Expr::Int(value, _) => Some(*value),
            Expr::Unary {
                op: UnaryOp::Neg,
                operand,
                ..
            } => Self::expr_int_literal(operand).map(|value| -value),
            _ => None,
        }
    }

    fn expr_is_zero(expr: &Expr) -> bool {
        Self::expr_int_literal(expr) == Some(0)
    }

    fn expr_is_len_call_of(expr: &Expr, expected_ident: &str) -> bool {
        match Self::expr_strip_parens(expr) {
            Expr::Call { callee, args, .. }
                if matches!(callee.as_ref(), Expr::Ident(name, _) if name == "len")
                    && args.len() == 1 =>
            {
                Self::expr_is_ident(&args[0].value, expected_ident)
            }
            _ => false,
        }
    }

    fn expr_is_manual_substring_needle_len(
        expr: &Expr,
        needle_name: &str,
        needle_len_binding: Option<&str>,
    ) -> bool {
        needle_len_binding
            .map(|binding| Self::expr_is_ident(expr, binding))
            .unwrap_or(false)
            || Self::expr_is_len_call_of(expr, needle_name)
    }

    fn match_manual_substring_needle_len_binding(stmt: &Stmt, needle_name: &str) -> Option<String> {
        match stmt {
            Stmt::Let {
                pattern,
                value: Some(value),
                ..
            } => {
                let binding = Self::pattern_binding_name(pattern)?;
                if Self::expr_is_len_call_of(value, needle_name) {
                    Some(binding.to_string())
                } else {
                    None
                }
            }
            _ => None,
        }
    }

    fn stmt_is_manual_substring_empty_needle_guard(
        stmt: &Stmt,
        needle_name: &str,
        needle_len_binding: Option<&str>,
        start_name: &str,
    ) -> bool {
        let Stmt::Expr(Expr::If {
            condition,
            then_branch,
            else_branch,
            ..
        }) = stmt
        else {
            return false;
        };
        if else_branch.is_some() {
            return false;
        }
        let condition = Self::expr_strip_parens(condition);
        let Expr::Binary {
            left,
            op: BinaryOp::Eq,
            right,
            ..
        } = condition
        else {
            return false;
        };
        let empty_check =
            (Self::expr_is_manual_substring_needle_len(left, needle_name, needle_len_binding)
                && Self::expr_is_zero(right))
                || (Self::expr_is_manual_substring_needle_len(
                    right,
                    needle_name,
                    needle_len_binding,
                ) && Self::expr_is_zero(left));
        if !empty_check {
            return false;
        }
        matches!(
            then_branch.stmts.as_slice(),
            [Stmt::Return(Some(value), _)] if Self::expr_is_ident(value, start_name)
        )
    }

    fn match_manual_substring_index_init(stmt: &Stmt, start_name: &str) -> Option<String> {
        match stmt {
            Stmt::Let {
                pattern,
                value: Some(value),
                ..
            } => {
                let binding = Self::pattern_binding_name(pattern)?;
                if Self::expr_is_ident(value, start_name) {
                    Some(binding.to_string())
                } else {
                    None
                }
            }
            _ => None,
        }
    }

    fn expr_is_manual_substring_search_bound(
        expr: &Expr,
        text_name: &str,
        needle_name: &str,
        needle_len_binding: Option<&str>,
        index_name: &str,
    ) -> bool {
        let Expr::Binary {
            left,
            op: BinaryOp::Le,
            right,
            ..
        } = Self::expr_strip_parens(expr)
        else {
            return false;
        };
        if !Self::expr_is_len_call_of(right, text_name) {
            return false;
        }
        let Expr::Binary {
            left: sum_left,
            op: BinaryOp::Add,
            right: sum_right,
            ..
        } = Self::expr_strip_parens(left)
        else {
            return false;
        };
        let left_matches = Self::expr_is_ident(sum_left, index_name)
            && Self::expr_is_manual_substring_needle_len(
                sum_right,
                needle_name,
                needle_len_binding,
            );
        let right_matches = Self::expr_is_ident(sum_right, index_name)
            && Self::expr_is_manual_substring_needle_len(sum_left, needle_name, needle_len_binding);
        left_matches || right_matches
    }

    fn stmt_is_manual_substring_match_guard(
        stmt: &Stmt,
        text_name: &str,
        needle_name: &str,
        index_name: &str,
    ) -> bool {
        let Stmt::Expr(Expr::If {
            condition,
            then_branch,
            else_branch,
            ..
        }) = stmt
        else {
            return false;
        };
        if else_branch.is_some() {
            return false;
        }
        let Expr::Call { callee, args, .. } = Self::expr_strip_parens(condition) else {
            return false;
        };
        if !matches!(callee.as_ref(), Expr::Ident(name, _) if name == "starts_with_at")
            || args.len() != 3
        {
            return false;
        }
        if !Self::expr_is_ident(&args[0].value, text_name)
            || !Self::expr_is_ident(&args[1].value, index_name)
            || !Self::expr_is_ident(&args[2].value, needle_name)
        {
            return false;
        }
        matches!(
            then_branch.stmts.as_slice(),
            [Stmt::Return(Some(value), _)] if Self::expr_is_ident(value, index_name)
        )
    }

    fn stmt_is_manual_substring_increment(stmt: &Stmt, index_name: &str) -> bool {
        let Stmt::Expr(Expr::Assign { target, value, .. }) = stmt else {
            return false;
        };
        if !Self::expr_is_ident(target, index_name) {
            return false;
        }
        let Expr::Binary {
            left,
            op: BinaryOp::Add,
            right,
            ..
        } = Self::expr_strip_parens(value)
        else {
            return false;
        };
        let left_matches =
            Self::expr_is_ident(left, index_name) && Self::expr_int_literal(right) == Some(1);
        let right_matches =
            Self::expr_is_ident(right, index_name) && Self::expr_int_literal(left) == Some(1);
        left_matches || right_matches
    }

    fn stmt_is_manual_substring_search_loop(
        stmt: &Stmt,
        text_name: &str,
        needle_name: &str,
        needle_len_binding: Option<&str>,
        index_name: &str,
    ) -> bool {
        let Stmt::While {
            condition, body, ..
        } = stmt
        else {
            return false;
        };
        if !Self::expr_is_manual_substring_search_bound(
            condition,
            text_name,
            needle_name,
            needle_len_binding,
            index_name,
        ) {
            return false;
        }
        matches!(
            body.stmts.as_slice(),
            [match_guard, increment]
                if Self::stmt_is_manual_substring_match_guard(
                    match_guard,
                    text_name,
                    needle_name,
                    index_name,
                ) && Self::stmt_is_manual_substring_increment(increment, index_name)
        )
    }

    fn match_manual_substring_miss_return(
        stmt: &Stmt,
        text_name: &str,
    ) -> Option<ManualFindSubstringMissBehavior> {
        let Stmt::Return(Some(value), _) = stmt else {
            return None;
        };
        if Self::expr_is_len_call_of(value, text_name) {
            return Some(ManualFindSubstringMissBehavior::HaystackLength);
        }
        if Self::expr_int_literal(value) == Some(-1) {
            return Some(ManualFindSubstringMissBehavior::NegativeOne);
        }
        None
    }

    fn detect_manual_find_substring_function(
        func: &TypedFunction,
    ) -> Option<ManualFindSubstringMissBehavior> {
        if func.ast.params.len() != 3 {
            return None;
        }
        if !Self::ast_type_is_string(&func.ast.params[0].ty)
            || !Self::ast_type_is_string(&func.ast.params[1].ty)
            || !Self::ast_type_is_int(&func.ast.params[2].ty)
            || !func
                .ast
                .return_type
                .as_ref()
                .is_some_and(Self::ast_type_is_int)
        {
            return None;
        }
        let text_name = func.ast.params[0].name.as_str();
        let needle_name = func.ast.params[1].name.as_str();
        let start_name = func.ast.params[2].name.as_str();
        let mut stmts = func.ast.body.stmts.iter();
        let mut current = stmts.next()?;
        let mut needle_len_binding = None::<String>;
        if let Some(binding_name) =
            Self::match_manual_substring_needle_len_binding(current, needle_name)
        {
            needle_len_binding = Some(binding_name);
            current = stmts.next()?;
        }
        if !Self::stmt_is_manual_substring_empty_needle_guard(
            current,
            needle_name,
            needle_len_binding.as_deref(),
            start_name,
        ) {
            return None;
        }
        let index_name = Self::match_manual_substring_index_init(stmts.next()?, start_name)?;
        if !Self::stmt_is_manual_substring_search_loop(
            stmts.next()?,
            text_name,
            needle_name,
            needle_len_binding.as_deref(),
            &index_name,
        ) {
            return None;
        }
        let miss_behavior = Self::match_manual_substring_miss_return(stmts.next()?, text_name)?;
        if stmts.next().is_some() {
            return None;
        }
        Some(miss_behavior)
    }

    fn expr_is_direct_string_byte_view(expr: &Expr) -> bool {
        match Self::expr_strip_parens(expr) {
            Expr::String(..) | Expr::Ident(..) => true,
            _ => false,
        }
    }

    fn compile_direct_string_view_and_length(
        &mut self,
        expr: &Expr,
    ) -> KainResult<Option<(String, String)>> {
        if !Self::expr_is_direct_string_byte_view(expr) || !self.expr_is_known_string(expr) {
            return Ok(None);
        }
        let ptr = self.compile_string_data_pointer_for_byte_view(expr)?;
        let Some(len) = self.compile_string_length_value(expr)? else {
            return Ok(None);
        };
        Ok(Some((ptr, len)))
    }

    fn compile_manual_find_substring_call_fast_path(
        &mut self,
        func_name: &str,
        args: &[kain_core::ast::CallArg],
    ) -> KainResult<Option<(String, String)>> {
        let Some(miss_behavior) = self.manual_find_substring_functions.get(func_name).copied()
        else {
            return Ok(None);
        };
        if args.len() != 3 {
            return Ok(None);
        }
        let Some((text_ptr, text_len)) =
            self.compile_direct_string_view_and_length(&args[0].value)?
        else {
            return Ok(None);
        };
        let Some((needle_ptr, needle_len)) =
            self.compile_direct_string_view_and_length(&args[1].value)?
        else {
            return Ok(None);
        };
        let start = self.compile_expr_as_i64(&args[2].value)?;
        let empty = self.next_reg();
        self.emit(&format!("  {} = icmp eq i64 {}, 0", empty, needle_len));
        let needle_static_bytes = self.expr_static_string_bytes(&args[1].value);
        let search_result = self.compile_known_length_find_substring_inline(
            &text_ptr,
            &text_len,
            &needle_ptr,
            &needle_len,
            &start,
            needle_static_bytes.as_deref(),
        );
        let nonempty_result = match miss_behavior {
            ManualFindSubstringMissBehavior::NegativeOne => search_result,
            ManualFindSubstringMissBehavior::HaystackLength => {
                let miss = self.next_reg();
                self.emit(&format!("  {} = icmp slt i64 {}, 0", miss, search_result));
                let shaped = self.next_reg();
                self.emit(&format!(
                    "  {} = select i1 {}, i64 {}, i64 {}",
                    shaped, miss, text_len, search_result
                ));
                shaped
            }
        };
        let result = self.next_reg();
        self.emit(&format!(
            "  {} = select i1 {}, i64 {}, i64 {}",
            result, empty, start, nonempty_result
        ));
        Ok(Some((result, "i64".to_string())))
    }

    fn map_type(&self, ty: &kain_core::types::ResolvedType) -> String {
        use kain_core::types::ResolvedType;
        match ty {
            ResolvedType::Int(_) => "i64".into(),
            ResolvedType::Float(_) => "double".into(),
            ResolvedType::Bool => "i1".into(),
            ResolvedType::String => "i8*".into(),
            ResolvedType::Unit => "void".into(),
            ResolvedType::Char => "i8".into(),
            ResolvedType::Struct(name, _) => {
                if self.struct_defs.contains_key(name) {
                    self.struct_storage_type(name)
                } else {
                    self.map_type_from_str(name)
                }
            }
            ResolvedType::Enum(name, _) => format!("%{}*", name),
            ResolvedType::Array(_, _) => "i64".into(), // Arrays are opaque pointers for now
            ResolvedType::Slice(_) => "i64".into(),
            ResolvedType::Option(_) => "i8*".into(),
            ResolvedType::Result(_, _) => "i8*".into(),
            ResolvedType::Future(_) => "i8*".into(),
            ResolvedType::Function { .. } => "i64".into(), // Function pointers
            ResolvedType::Generic(name) => self.map_type_from_str(name),
            ResolvedType::Tuple(items) => {
                let field_tys = items
                    .iter()
                    .map(|item| self.map_type(item))
                    .collect::<Vec<_>>();
                self.tuple_struct_storage_type_from_types(&field_tys)
            }
            ResolvedType::Ref { inner, .. } => self.map_type(inner),
            ResolvedType::Ptr { inner, .. } => format!("{}*", self.map_type(inner)),
            ResolvedType::Never => "void".into(),
            ResolvedType::Unknown => "i64".into(),
        }
    }

    fn intern_string_global_name(&mut self, s: &str) -> String {
        if let Some(name) = self.strings.get(s) {
            name.clone()
        } else {
            let name = format!("@.str.{}", self.string_counter);
            self.string_counter += 1;
            self.strings.insert(s.to_string(), name.clone());
            name
        }
    }

    fn compile_string_literal(&mut self, s: &str) -> (String, String) {
        if self.entry_preamble_insert_offset.is_some() && !self.scopes.is_empty() {
            let slot = if let Some(slot) = self.pooled_string_literal_slots.get(s) {
                slot.clone()
            } else {
                let global_name = self.intern_string_global_name(s);
                let slot_index = self.pooled_string_literal_slots.len();
                let local_name = format!("__kain_pooled_literal_{}", slot_index);
                let slot = format!("%{}.addr", local_name);
                let len = s.len() + 1;
                let reg_static = self.next_reg();
                let reg_rc = self.next_reg();
                self.emit_entry_alloca(&slot, "i8*");
                self.emit_entry_preamble_line(&format!(
                    "{} = getelementptr inbounds [{} x i8], [{} x i8]* {}, i64 0, i64 0",
                    reg_static, len, len, global_name
                ));
                self.emit_entry_preamble_line(&format!(
                    "{} = call i8* @string_new(i8* {})",
                    reg_rc, reg_static
                ));
                self.emit_entry_preamble_line(&format!("store i8* {}, i8** {}", reg_rc, slot));
                self.locals
                    .insert(local_name.clone(), (slot.clone(), "i8*".to_string()));
                self.string_locals.insert(local_name.clone());
                if let Some(root_scope) = self.scopes.first_mut() {
                    root_scope.push(local_name);
                }
                self.pooled_string_literal_slots
                    .insert(s.to_string(), slot.clone());
                slot
            };

            let reg = self.next_reg();
            self.emit(&format!("  {} = load i8*, i8** {}", reg, slot));
            return (reg, "i8*".to_string());
        }

        let global_name = self.intern_string_global_name(s);
        let reg_static = self.next_reg();
        let len = s.len() + 1;
        self.emit(&format!(
            "  {} = getelementptr inbounds [{} x i8], [{} x i8]* {}, i64 0, i64 0",
            reg_static, len, len, global_name
        ));

        let reg_rc = self.next_reg();
        self.emit(&format!(
            "  {} = call i8* @string_new(i8* {})",
            reg_rc, reg_static
        ));
        (reg_rc, "i8*".to_string())
    }

    fn compile_static_c_string_literal(&mut self, s: &str) -> String {
        let global_name = self.intern_string_global_name(s);

        let reg_static = self.next_reg();
        let len = s.len() + 1;
        self.emit(&format!(
            "  {} = getelementptr inbounds [{} x i8], [{} x i8]* {}, i64 0, i64 0",
            reg_static, len, len, global_name
        ));
        reg_static
    }

    fn concat_strings(&mut self, lhs: &str, rhs: &str) -> String {
        let res = self.next_reg();
        self.emit(&format!(
            "  {} = call i8* @str_concat(i8* {}, i8* {})",
            res, lhs, rhs
        ));
        res
    }

    fn stringify_value(&mut self, val: &str, ty: &str) -> KainResult<(String, String)> {
        match ty {
            "i8*" => Ok((val.to_string(), "i8*".to_string())),
            "i64" | "i8" => {
                let widened = if ty == "i64" {
                    val.to_string()
                } else {
                    let reg = self.next_reg();
                    self.emit(&format!("  {} = sext i8 {} to i64", reg, val));
                    reg
                };
                let res = self.next_reg();
                self.emit(&format!("  {} = call i8* @to_string(i64 {})", res, widened));
                Ok((res, "i8*".to_string()))
            }
            "i1" => {
                let widened = self.next_reg();
                self.emit(&format!("  {} = zext i1 {} to i64", widened, val));
                let res = self.next_reg();
                self.emit(&format!("  {} = call i8* @to_string(i64 {})", res, widened));
                Ok((res, "i8*".to_string()))
            }
            "double" => {
                let narrowed = self.next_reg();
                self.emit(&format!("  {} = fptosi double {} to i64", narrowed, val));
                let res = self.next_reg();
                self.emit(&format!(
                    "  {} = call i8* @to_string(i64 {})",
                    res, narrowed
                ));
                Ok((res, "i8*".to_string()))
            }
            _ if ty.starts_with('%') => Ok(self.compile_string_literal("<value>")),
            _ => Ok(self.compile_string_literal("<value>")),
        }
    }

    fn zero_value_for_ty(&self, ty: &str) -> String {
        match ty {
            "double" => "0.0".into(),
            "i1" | "i8" | "i64" => "0".into(),
            "void" => "0".into(),
            _ if ty.ends_with('*') => "null".into(),
            _ if ty.starts_with('%') => "zeroinitializer".into(),
            _ => "0".into(),
        }
    }

    fn compile_expr_for_target_type(
        &mut self,
        expr: &Expr,
        target_ty: &str,
    ) -> KainResult<(String, String)> {
        match expr {
            Expr::Await(value, span) => {
                return self.compile_await_for_target_type(value, target_ty, *span);
            }
            Expr::Try(value, span) => {
                return self.compile_try_for_target_type(value, target_ty, *span);
            }
            Expr::MethodCall {
                receiver,
                method,
                args,
                span,
            } if matches!(method.as_str(), "unwrap" | "expect") => {
                let (boxed_value, boxed_ty) = self.compile_expr(receiver)?;
                if boxed_ty == "i8*" {
                    if method == "expect" && args.len() != 1 {
                        return Err(KainError::codegen(
                            "expect expects exactly one message argument",
                            *span,
                        ));
                    }
                    if method == "unwrap" && !args.is_empty() {
                        return Err(KainError::codegen("unwrap expects no arguments", *span));
                    }
                    let result = self.compile_tagged_value_payload_copy(&boxed_value, target_ty);
                    self.emit_release_if_new_object_expr(receiver, &boxed_value, &boxed_ty);
                    return Ok(result);
                }
            }
            Expr::MethodCall {
                receiver,
                method,
                args,
                span,
            } if method == "unwrap_or" => {
                if args.len() != 1 {
                    return Err(KainError::codegen(
                        "unwrap_or expects exactly one default argument",
                        *span,
                    ));
                }
                let (boxed_value, boxed_ty) = self.compile_expr(receiver)?;
                if boxed_ty == "i8*" {
                    let is_success = self.compile_tagged_value_is_tag(
                        &boxed_value,
                        &[ABI_TAG_OPTION_SOME_LLVM, ABI_TAG_RESULT_OK_LLVM],
                        false,
                    );
                    let payload_label = self.next_label();
                    let default_label = self.next_label();
                    let merge_label = self.next_label();
                    self.emit(&format!(
                        "  br i1 {}, label %{}, label %{}",
                        is_success, payload_label, default_label
                    ));

                    self.emit_label(&payload_label);
                    let (payload_value, payload_ty) =
                        self.compile_tagged_value_payload_copy(&boxed_value, target_ty);
                    let payload_block = self.current_block.clone();
                    self.emit(&format!("  br label %{}", merge_label));

                    self.emit_label(&default_label);
                    let (default_value, default_ty) =
                        self.compile_expr_for_target_type(&args[0].value, target_ty)?;
                    let default_block = self.current_block.clone();
                    self.emit(&format!("  br label %{}", merge_label));

                    if payload_ty != default_ty {
                        return Err(KainError::codegen(
                            "unwrap_or payload and default produced different LLVM types",
                            *span,
                        ));
                    }
                    self.emit_label(&merge_label);
                    let merged = self.next_reg();
                    self.emit(&format!(
                        "  {} = phi {} [ {}, %{} ], [ {}, %{} ]",
                        merged,
                        target_ty,
                        payload_value,
                        payload_block,
                        default_value,
                        default_block
                    ));
                    self.emit_release_if_new_object_expr(receiver, &boxed_value, &boxed_ty);
                    return Ok((merged, target_ty.to_string()));
                }
            }
            _ => {}
        }

        if target_ty != "void" {
            if let Expr::Call { callee, args, span } = expr {
                if let Expr::Ident(name, _) = callee.as_ref() {
                    if let Some(result) =
                        self.compile_actor_builtin_ask(name, args, *span, Some(target_ty))?
                    {
                        return Ok(result);
                    }
                }
            }
        }

        if matches!(expr, Expr::None(_)) {
            if target_ty == "i8*" {
                return Ok(("null".to_string(), "i8*".to_string()));
            }
            return Ok((self.zero_value_for_ty(target_ty), target_ty.to_string()));
        }

        let (val, src_ty) = self.compile_expr(expr)?;
        self.coerce_compiled_value_to_target_type(val, &src_ty, target_ty)
    }

    fn coerce_compiled_value_to_target_type(
        &mut self,
        val: String,
        src_ty: &str,
        target_ty: &str,
    ) -> KainResult<(String, String)> {
        if src_ty == target_ty {
            return Ok((val, target_ty.to_string()));
        }

        if target_ty == "void" {
            return Ok((self.zero_value_for_ty(target_ty), target_ty.to_string()));
        }

        if matches!(target_ty, "i64" | "i32" | "i8" | "i1" | "double") {
            let coerced = self.cast_numeric_value(val, src_ty, target_ty)?;
            return Ok((coerced, target_ty.to_string()));
        }

        if target_ty.ends_with('*') {
            if src_ty.ends_with('*') {
                let reg = self.next_reg();
                self.emit(&format!(
                    "  {} = bitcast {} {} to {}",
                    reg, src_ty, val, target_ty
                ));
                return Ok((reg, target_ty.to_string()));
            }

            let ptr_source = self.coerce_to_i64_storage(&val, src_ty);
            let reg = self.next_reg();
            self.emit(&format!(
                "  {} = inttoptr i64 {} to {}",
                reg, ptr_source, target_ty
            ));
            return Ok((reg, target_ty.to_string()));
        }

        if src_ty.ends_with('*') && target_ty == "i64" {
            let reg = self.next_reg();
            self.emit(&format!("  {} = ptrtoint {} {} to i64", reg, src_ty, val));
            return Ok((reg, target_ty.to_string()));
        }

        if src_ty.starts_with('%') && target_ty.starts_with('%') {
            let reg = self.next_reg();
            self.emit(&format!(
                "  {} = bitcast {} {} to {}",
                reg, src_ty, val, target_ty
            ));
            return Ok((reg, target_ty.to_string()));
        }

        Err(KainError::codegen(
            format!(
                "Unsupported LLVM value coercion from {} to {}",
                src_ty, target_ty
            ),
            kain_core::Span::default(),
        ))
    }

    fn align_abi_size(size: usize, align: usize) -> usize {
        if align <= 1 {
            size
        } else {
            size.div_ceil(align) * align
        }
    }

    fn abi_layout_for_ty(&self, ty: &str, span: Span) -> KainResult<(usize, usize)> {
        match ty {
            "i1" | "i8" => Ok((1, 1)),
            "i32" => Ok((4, 4)),
            "i64" | "double" => Ok((8, 8)),
            "void" => Err(KainError::codegen(
                "Cannot compute runtime memory layout for void",
                span,
            )),
            _ if ty.ends_with('*') => Ok((8, 8)),
            _ if ty.starts_with('%') => {
                let struct_name = ty.trim_start_matches('%');
                let fields = self.struct_defs.get(struct_name).ok_or_else(|| {
                    KainError::codegen(format!("Unknown LLVM struct layout: {}", struct_name), span)
                })?;
                let mut size = 0usize;
                let mut max_align = 1usize;
                for (_, field_ty) in fields {
                    let (field_size, field_align) = self.abi_layout_for_ty(field_ty, span)?;
                    size = Self::align_abi_size(size, field_align);
                    size += field_size;
                    max_align = max_align.max(field_align);
                }
                Ok((Self::align_abi_size(size, max_align), max_align))
            }
            _ => Err(KainError::codegen(
                format!("Unsupported LLVM runtime memory layout for type {}", ty),
                span,
            )),
        }
    }

    fn emit_tagged_value_handle_bits(&mut self, boxed_value: &str) -> String {
        let handle_bits = self.next_reg();
        self.emit(&format!(
            "  {} = ptrtoint i8* {} to i64",
            handle_bits, boxed_value
        ));
        handle_bits
    }

    fn emit_tagged_immediate_tag_bits_from_handle_bits(&mut self, handle_bits: &str) -> String {
        let immediate_tag = self.next_reg();
        self.emit(&format!(
            "  {} = and i64 {}, {}",
            immediate_tag, handle_bits, ABI_TAGGED_IMMEDIATE_MASK_LLVM
        ));
        immediate_tag
    }

    fn compile_tagged_immediate_integer_handle_from_i64(
        &mut self,
        tag: i64,
        value_i64: &str,
    ) -> (String, String) {
        let shifted_payload = self.next_reg();
        self.emit(&format!("  {} = shl i64 {}, 3", shifted_payload, value_i64));
        let tagged_bits = self.next_reg();
        self.emit(&format!(
            "  {} = or i64 {}, {}",
            tagged_bits, shifted_payload, tag
        ));
        let handle = self.next_reg();
        self.emit(&format!(
            "  {} = inttoptr i64 {} to i8*",
            handle, tagged_bits
        ));
        (handle, "i8*".to_string())
    }

    fn compile_tagged_immediate_integer_payload_from_i64_bits(
        &mut self,
        handle_bits: &str,
        target_ty: &str,
    ) -> KainResult<(String, String)> {
        let payload_i64 = self.next_reg();
        self.emit(&format!("  {} = ashr i64 {}, 3", payload_i64, handle_bits));
        if target_ty == "i64" {
            Ok((payload_i64, "i64".to_string()))
        } else {
            let coerced = self.cast_numeric_value(payload_i64, "i64", target_ty)?;
            Ok((coerced, target_ty.to_string()))
        }
    }

    fn compile_tagged_immediate_borrowed_pointer_handle(
        &mut self,
        tag: i64,
        pointer: &str,
    ) -> (String, String) {
        let pointer_bits = self.next_reg();
        self.emit(&format!(
            "  {} = ptrtoint i8* {} to i64",
            pointer_bits, pointer
        ));
        let tagged_bits = self.next_reg();
        self.emit(&format!(
            "  {} = or i64 {}, {}",
            tagged_bits, pointer_bits, tag
        ));
        let handle = self.next_reg();
        self.emit(&format!(
            "  {} = inttoptr i64 {} to i8*",
            handle, tagged_bits
        ));
        (handle, "i8*".to_string())
    }

    fn emit_tagged_value_tag_load(&mut self, boxed_value: &str) -> String {
        let tag_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = bitcast i8* {} to i64*",
            tag_ptr, boxed_value
        ));
        let tag = self.next_reg();
        self.emit(&format!("  {} = load i64, i64* {}, align 1", tag, tag_ptr));
        tag
    }

    fn emit_tagged_value_payload_ptr(&mut self, boxed_value: &str) -> String {
        let payload_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = getelementptr inbounds i8, i8* {}, i64 {}",
            payload_ptr, boxed_value, ABI_TAGGED_HEADER_BYTES
        ));
        payload_ptr
    }

    fn compile_tagged_value_is_tag(
        &mut self,
        boxed_value: &str,
        match_tags: &[i64],
        null_value: bool,
    ) -> String {
        let is_null = self.next_reg();
        self.emit(&format!(
            "  {} = icmp eq i8* {}, null",
            is_null, boxed_value
        ));

        let null_label = self.next_label();
        let tagged_label = self.next_label();
        let merge_label = self.next_label();
        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            is_null, null_label, tagged_label
        ));

        self.emit_label(&null_label);
        self.emit(&format!("  br label %{}", merge_label));

        self.emit_label(&tagged_label);
        let handle_bits = self.emit_tagged_value_handle_bits(boxed_value);
        let immediate_tag = self.emit_tagged_immediate_tag_bits_from_handle_bits(&handle_bits);
        let is_immediate = self.next_reg();
        self.emit(&format!(
            "  {} = icmp ne i64 {}, 0",
            is_immediate, immediate_tag
        ));
        let immediate_label = self.next_label();
        let boxed_label = self.next_label();
        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            is_immediate, immediate_label, boxed_label
        ));

        self.emit_label(&immediate_label);
        let mut immediate_result: Option<String> = None;
        for expected in match_tags {
            let cmp = self.next_reg();
            self.emit(&format!(
                "  {} = icmp eq i64 {}, {}",
                cmp, immediate_tag, expected
            ));
            immediate_result = Some(match immediate_result {
                Some(previous) => {
                    let combined = self.next_reg();
                    self.emit(&format!("  {} = or i1 {}, {}", combined, previous, cmp));
                    combined
                }
                None => cmp,
            });
        }
        let immediate_result = immediate_result.unwrap_or_else(|| "0".to_string());
        let immediate_block = self.current_block.clone();
        self.emit(&format!("  br label %{}", merge_label));

        self.emit_label(&boxed_label);
        let tag = self.emit_tagged_value_tag_load(boxed_value);
        let mut boxed_result: Option<String> = None;
        for expected in match_tags {
            let cmp = self.next_reg();
            self.emit(&format!("  {} = icmp eq i64 {}, {}", cmp, tag, expected));
            boxed_result = Some(match boxed_result {
                Some(previous) => {
                    let combined = self.next_reg();
                    self.emit(&format!("  {} = or i1 {}, {}", combined, previous, cmp));
                    combined
                }
                None => cmp,
            });
        }
        let boxed_result = boxed_result.unwrap_or_else(|| "0".to_string());
        let boxed_block = self.current_block.clone();
        self.emit(&format!("  br label %{}", merge_label));

        self.emit_label(&merge_label);
        let result = self.next_reg();
        self.emit(&format!(
            "  {} = phi i1 [ {}, %{} ], [ {}, %{} ], [ {}, %{} ]",
            result,
            if null_value { "1" } else { "0" },
            null_label,
            immediate_result,
            immediate_block,
            boxed_result,
            boxed_block
        ));
        result
    }

    fn compile_tagged_value_payload_copy(
        &mut self,
        boxed_value: &str,
        target_ty: &str,
    ) -> (String, String) {
        if target_ty == "void" {
            return ("0".to_string(), "i64".to_string());
        }

        let handle_bits = self.emit_tagged_value_handle_bits(boxed_value);
        let immediate_tag = self.emit_tagged_immediate_tag_bits_from_handle_bits(&handle_bits);
        let is_immediate = self.next_reg();
        self.emit(&format!(
            "  {} = icmp ne i64 {}, 0",
            is_immediate, immediate_tag
        ));
        let immediate_label = self.next_label();
        let boxed_label = self.next_label();
        let merge_label = self.next_label();
        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            is_immediate, immediate_label, boxed_label
        ));

        self.emit_label(&immediate_label);
        let (immediate_value, immediate_ty) = if matches!(target_ty, "i64" | "i32" | "i8" | "i1") {
            self.compile_tagged_immediate_integer_payload_from_i64_bits(&handle_bits, target_ty)
                .expect("tagged immediate integer decode should be representable")
        } else if target_ty == "i8*" {
            let untagged_bits = self.next_reg();
            self.emit(&format!(
                "  {} = and i64 {}, -8",
                untagged_bits, handle_bits
            ));
            let pointer_value = self.next_reg();
            self.emit(&format!(
                "  {} = inttoptr i64 {} to i8*",
                pointer_value, untagged_bits
            ));
            (pointer_value, "i8*".to_string())
        } else {
            (self.zero_value_for_ty(target_ty), target_ty.to_string())
        };
        let immediate_block = self.current_block.clone();
        self.emit(&format!("  br label %{}", merge_label));

        self.emit_label(&boxed_label);
        let payload_ptr = self.emit_tagged_value_payload_ptr(boxed_value);
        let typed_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = bitcast i8* {} to {}*",
            typed_ptr, payload_ptr, target_ty
        ));
        let loaded = self.next_reg();
        self.emit(&format!(
            "  {} = load {}, {}* {}, align 1",
            loaded, target_ty, target_ty, typed_ptr
        ));
        let boxed_block = self.current_block.clone();
        self.emit(&format!("  br label %{}", merge_label));

        self.emit_label(&merge_label);
        let merged = self.next_reg();
        self.emit(&format!(
            "  {} = phi {} [ {}, %{} ], [ {}, %{} ]",
            merged, target_ty, immediate_value, immediate_block, loaded, boxed_block
        ));
        debug_assert_eq!(immediate_ty, target_ty);
        (merged, target_ty.to_string())
    }

    fn compile_tagged_value_from_compiled_payload(
        &mut self,
        tag: i64,
        payload_value: &str,
        payload_ty: &str,
        span: Span,
    ) -> KainResult<(String, String)> {
        match payload_ty {
            "i1" | "i8" | "i32" => {
                let payload_i64 =
                    self.cast_numeric_value(payload_value.to_string(), payload_ty, "i64")?;
                return Ok(self.compile_tagged_immediate_integer_handle_from_i64(tag, &payload_i64));
            }
            "i64" => {
                let within_min = self.next_reg();
                self.emit(&format!(
                    "  {} = icmp sge i64 {}, {}",
                    within_min, payload_value, ABI_TAGGED_IMMEDIATE_INT_MIN_LLVM
                ));
                let within_max = self.next_reg();
                self.emit(&format!(
                    "  {} = icmp sle i64 {}, {}",
                    within_max, payload_value, ABI_TAGGED_IMMEDIATE_INT_MAX_LLVM
                ));
                let in_range = self.next_reg();
                self.emit(&format!(
                    "  {} = and i1 {}, {}",
                    in_range, within_min, within_max
                ));
                let immediate_label = self.next_label();
                let boxed_label = self.next_label();
                let merge_label = self.next_label();
                self.emit(&format!(
                    "  br i1 {}, label %{}, label %{}",
                    in_range, immediate_label, boxed_label
                ));

                self.emit_label(&immediate_label);
                let immediate_value =
                    self.compile_tagged_immediate_integer_handle_from_i64(tag, payload_value);
                let immediate_block = self.current_block.clone();
                self.emit(&format!("  br label %{}", merge_label));

                self.emit_label(&boxed_label);
                let (payload_ptr, payload_size) =
                    self.compile_payload_pointer_from_value(payload_value, payload_ty, span)?;
                let boxed_value = self.compile_tagged_box_from_payload(
                    tag,
                    Some(&payload_ptr),
                    Some(payload_ty),
                    payload_size,
                );
                let boxed_block = self.current_block.clone();
                self.emit(&format!("  br label %{}", merge_label));

                self.emit_label(&merge_label);
                let merged = self.next_reg();
                self.emit(&format!(
                    "  {} = phi i8* [ {}, %{} ], [ {}, %{} ]",
                    merged, immediate_value.0, immediate_block, boxed_value.0, boxed_block
                ));
                return Ok((merged, "i8*".to_string()));
            }
            _ => {}
        }

        let (payload_ptr, payload_size) =
            self.compile_payload_pointer_from_value(payload_value, payload_ty, span)?;
        Ok(self.compile_tagged_box_from_payload(
            tag,
            Some(&payload_ptr),
            Some(payload_ty),
            payload_size,
        ))
    }

    fn compile_tagged_box_from_payload(
        &mut self,
        tag: i64,
        payload_ptr: Option<&str>,
        payload_ty: Option<&str>,
        payload_size: usize,
    ) -> (String, String) {
        let allocation_size = ABI_TAGGED_HEADER_BYTES as usize + payload_size;
        let boxed_value = self.next_reg();
        self.emit(&format!(
            "  {} = call i8* @KAIN_alloc(i64 {})",
            boxed_value, allocation_size
        ));

        let tag_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = bitcast i8* {} to i64*",
            tag_ptr, boxed_value
        ));
        self.emit(&format!("  store i64 {}, i64* {}, align 1", tag, tag_ptr));

        let payload_size_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = getelementptr inbounds i8, i8* {}, i64 8",
            payload_size_ptr, boxed_value
        ));
        let payload_size_i64_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = bitcast i8* {} to i64*",
            payload_size_i64_ptr, payload_size_ptr
        ));
        self.emit(&format!(
            "  store i64 {}, i64* {}, align 1",
            payload_size as i64, payload_size_i64_ptr
        ));

        if payload_size > 0 {
            let Some(payload_ptr) = payload_ptr else {
                return (boxed_value, "i8*".to_string());
            };
            let Some(payload_ty) = payload_ty else {
                return (boxed_value, "i8*".to_string());
            };
            let payload_dst = self.emit_tagged_value_payload_ptr(&boxed_value);
            let payload_src_typed = self.next_reg();
            self.emit(&format!(
                "  {} = bitcast i8* {} to {}*",
                payload_src_typed, payload_ptr, payload_ty
            ));
            let payload_value = self.next_reg();
            self.emit(&format!(
                "  {} = load {}, {}* {}, align 1",
                payload_value, payload_ty, payload_ty, payload_src_typed
            ));
            let payload_dst_typed = self.next_reg();
            self.emit(&format!(
                "  {} = bitcast i8* {} to {}*",
                payload_dst_typed, payload_dst, payload_ty
            ));
            self.emit(&format!(
                "  store {} {}, {}* {}, align 1",
                payload_ty, payload_value, payload_ty, payload_dst_typed
            ));
        }

        (boxed_value, "i8*".to_string())
    }

    fn compile_tagged_box_from_value(
        &mut self,
        tag: i64,
        payload_value: &str,
        payload_ty: &str,
        payload_size: usize,
    ) -> KainResult<(String, String)> {
        if payload_size > 0 {
            return self.compile_tagged_value_from_compiled_payload(
                tag,
                payload_value,
                payload_ty,
                Span::default(),
            );
        }
        let allocation_size = ABI_TAGGED_HEADER_BYTES as usize + payload_size;
        let boxed_value = self.next_reg();
        self.emit(&format!(
            "  {} = call i8* @KAIN_alloc(i64 {})",
            boxed_value, allocation_size
        ));

        let tag_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = bitcast i8* {} to i64*",
            tag_ptr, boxed_value
        ));
        self.emit(&format!("  store i64 {}, i64* {}, align 1", tag, tag_ptr));

        let payload_size_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = getelementptr inbounds i8, i8* {}, i64 8",
            payload_size_ptr, boxed_value
        ));
        let payload_size_i64_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = bitcast i8* {} to i64*",
            payload_size_i64_ptr, payload_size_ptr
        ));
        self.emit(&format!(
            "  store i64 {}, i64* {}, align 1",
            payload_size as i64, payload_size_i64_ptr
        ));

        if payload_size > 0 {
            let payload_dst = self.emit_tagged_value_payload_ptr(&boxed_value);
            let payload_dst_typed = self.next_reg();
            self.emit(&format!(
                "  {} = bitcast i8* {} to {}*",
                payload_dst_typed, payload_dst, payload_ty
            ));
            self.emit(&format!(
                "  store {} {}, {}* {}, align 1",
                payload_ty, payload_value, payload_ty, payload_dst_typed
            ));
        }

        Ok((boxed_value, "i8*".to_string()))
    }

    fn compile_runtime_mem_load(
        &mut self,
        pointer: &Expr,
        load_ty: &str,
        span: Span,
    ) -> KainResult<(String, String)> {
        if let Some(slot) = self.current_forwarded_mem_load_slot(pointer).cloned() {
            if slot.value_ty == load_ty {
                return Ok((slot.value_reg, slot.value_ty));
            }
        }
        if let Some((typed_ptr, alignment)) =
            self.compile_ephemeral_typed_memory_pointer(pointer, load_ty)?
        {
            let loaded = self.next_reg();
            self.emit(&format!(
                "  {} = load {}, {}* {}, align {}",
                loaded, load_ty, load_ty, typed_ptr, alignment
            ));
            return Ok((loaded, load_ty.to_string()));
        }
        if let Some((ptr_i8, witness)) = self.compile_ephemeral_storage_i8_pointer(pointer)? {
            let (load_size, _) = self.abi_layout_for_ty(load_ty, span)?;
            if load_size as i64 <= witness.storage_byte_len {
                let alignment =
                    Self::obvious_llvm_type_alignment(load_ty).min(witness.storage_alignment);
                let typed_ptr = self.next_reg();
                self.emit(&format!(
                    "  {} = bitcast i8* {} to {}*",
                    typed_ptr, ptr_i8, load_ty
                ));
                let loaded = self.next_reg();
                self.emit(&format!(
                    "  {} = load {}, {}* {}, align {}",
                    loaded, load_ty, load_ty, typed_ptr, alignment
                ));
                return Ok((loaded, load_ty.to_string()));
            }
        }
        let (typed_ptr, alignment) =
            self.compile_non_ephemeral_typed_memory_pointer(pointer, load_ty)?;
        let loaded = self.next_reg();
        self.emit(&format!(
            "  {} = load {}, {}* {}, align {}",
            loaded, load_ty, load_ty, typed_ptr, alignment
        ));
        Ok((loaded, load_ty.to_string()))
    }

    fn compile_runtime_mem_store(
        &mut self,
        pointer: &Expr,
        value: &Expr,
        span: Span,
    ) -> KainResult<(String, String)> {
        let stored_value_nonnegative = self.expr_is_proven_nonnegative_i64(value);
        let (stored_value, stored_ty) = self.compile_expr(value)?;
        if let Some((typed_ptr, alignment)) =
            self.compile_ephemeral_typed_memory_pointer(pointer, &stored_ty)?
        {
            self.emit(&format!(
                "  store {} {}, {}* {}, align {}",
                stored_ty, stored_value, stored_ty, typed_ptr, alignment
            ));
            self.record_forwarded_mem_store(
                pointer,
                &stored_value,
                &stored_ty,
                stored_value_nonnegative,
            );
            return Ok((stored_value, stored_ty));
        }
        if let Some((ptr_i8, witness)) = self.compile_ephemeral_storage_i8_pointer(pointer)? {
            let (store_size, _) = self.abi_layout_for_ty(&stored_ty, span)?;
            if store_size as i64 <= witness.storage_byte_len {
                let alignment =
                    Self::obvious_llvm_type_alignment(&stored_ty).min(witness.storage_alignment);
                let typed_ptr = self.next_reg();
                self.emit(&format!(
                    "  {} = bitcast i8* {} to {}*",
                    typed_ptr, ptr_i8, stored_ty
                ));
                self.emit(&format!(
                    "  store {} {}, {}* {}, align {}",
                    stored_ty, stored_value, stored_ty, typed_ptr, alignment
                ));
                self.record_forwarded_mem_store(
                    pointer,
                    &stored_value,
                    &stored_ty,
                    stored_value_nonnegative,
                );
                return Ok((stored_value, stored_ty));
            }
        }
        let (typed_ptr, alignment) =
            self.compile_non_ephemeral_typed_memory_pointer(pointer, &stored_ty)?;
        self.emit(&format!(
            "  store {} {}, {}* {}, align {}",
            stored_ty, stored_value, stored_ty, typed_ptr, alignment
        ));

        Ok((stored_value, stored_ty))
    }

    fn emit_scaled_byte_offset(
        &mut self,
        offset: &str,
        stride: &str,
        stride_literal: Option<i64>,
    ) -> String {
        match stride_literal {
            Some(1) => offset.to_string(),
            Some(value) => {
                if let Some(shift) = Self::positive_power_of_two_shift(value) {
                    let scaled = self.next_reg();
                    self.emit(&format!("  {} = shl i64 {}, {}", scaled, offset, shift));
                    scaled
                } else {
                    let scaled = self.next_reg();
                    self.emit(&format!("  {} = mul i64 {}, {}", scaled, offset, value));
                    scaled
                }
            }
            None => {
                let scaled = self.next_reg();
                self.emit(&format!("  {} = mul i64 {}, {}", scaled, offset, stride));
                scaled
            }
        }
    }

    fn compile_raw_ptr_offset_i64(
        &mut self,
        base_expr: &Expr,
        offset_expr: &Expr,
        stride_expr: &Expr,
    ) -> KainResult<(String, String)> {
        let known_i64_bindings = self.current_known_i64_literals();
        let stride_literal = Self::resolve_i64_literal(stride_expr, &known_i64_bindings);
        let (base, base_ty) = self.compile_expr(base_expr)?;
        let base_i64 = self.coerce_to_i64_storage(&base, &base_ty);
        let (offset, _) = self.compile_expr(offset_expr)?;
        let (stride, _) = if stride_literal.is_some() {
            (stride_literal.unwrap().to_string(), "i64".to_string())
        } else {
            self.compile_expr(stride_expr)?
        };
        let shift_safe_stride_literal =
            stride_literal.filter(|_| self.expr_is_proven_nonnegative_i64(offset_expr));
        let byte_offset = self.emit_scaled_byte_offset(&offset, &stride, shift_safe_stride_literal);
        let base_ptr = if base_ty.ends_with('*') {
            let cast = self.next_reg();
            self.emit(&format!("  {} = bitcast {} {} to i8*", cast, base_ty, base));
            cast
        } else {
            let cast = self.next_reg();
            self.emit(&format!("  {} = inttoptr i64 {} to i8*", cast, base_i64));
            cast
        };
        let raw_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = getelementptr i8, i8* {}, i64 {}",
            raw_ptr, base_ptr, byte_offset
        ));
        let res = self.next_reg();
        self.emit(&format!("  {} = ptrtoint i8* {} to i64", res, raw_ptr));
        Ok((res, "i64".to_string()))
    }

    fn compile_ownership_pointer(
        &mut self,
        target: &Expr,
    ) -> KainResult<(String, OwnershipPointerProvenance)> {
        let provenance = self.ownership_pointer_provenance_for_expr(target);
        let (ptr, ptr_ty) = self.compile_expr(target)?;
        let ptr_i8 = if ptr_ty == "i8*" {
            ptr
        } else if ptr_ty.ends_with('*') {
            let cast = self.next_reg();
            self.emit(&format!("  {} = bitcast {} {} to i8*", cast, ptr_ty, ptr));
            cast
        } else {
            let ptr_i64 = self.coerce_to_i64_storage(&ptr, &ptr_ty);
            let cast = self.next_reg();
            self.emit(&format!("  {} = inttoptr i64 {} to i8*", cast, ptr_i64));
            cast
        };
        if provenance == OwnershipPointerProvenance::ImportedOrUnknown {
            self.emit_lazy_import_ownership_region(&ptr_i8);
        }
        Ok((ptr_i8, provenance))
    }

    fn emit_lazy_import_ownership_region(&mut self, ptr_i8: &str) {
        let prepare_status = self.next_reg();
        self.emit(&format!(
            "  {} = call i32 @__kain_ownership_ensure_imported(i8* {})",
            prepare_status, ptr_i8
        ));
        let prepare_ok = self.next_reg();
        self.emit(&format!(
            "  {} = icmp eq i32 {}, 0",
            prepare_ok, prepare_status
        ));
        let continue_label = self.next_label();
        let abort_label = self.next_label();
        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            prepare_ok, continue_label, abort_label
        ));

        self.emit_label(&abort_label);
        self.emit("  call void @abort()");
        self.emit("  unreachable");
        self.emit_label(&continue_label);
    }

    fn emit_checked_ownership_call(&mut self, function_name: &str, ptr_i8: &str) {
        let status = self.next_reg();
        self.emit(&format!(
            "  {} = call i32 @{}(i8* {})",
            status, function_name, ptr_i8
        ));
        let ok = self.next_reg();
        self.emit(&format!("  {} = icmp eq i32 {}, 0", ok, status));
        let continue_label = self.next_label();
        let abort_label = self.next_label();
        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            ok, continue_label, abort_label
        ));
        self.emit_label(&abort_label);
        self.emit("  call void @abort()");
        self.emit("  unreachable");
        self.emit_label(&continue_label);
    }

    fn emit_helper_owned_local_decay_cleanup(&mut self, addr: &str) {
        let ptr_i8 = self.next_reg();
        self.emit(&format!("  {} = load i8*, i8** {}", ptr_i8, addr));
        let is_non_null = self.next_reg();
        self.emit(&format!("  {} = icmp ne i8* {}, null", is_non_null, ptr_i8));
        let decay_label = self.next_label();
        let merge_label = self.next_label();
        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            is_non_null, decay_label, merge_label
        ));
        self.emit_label(&decay_label);
        self.emit_checked_ownership_call("__kain_ownership_decay_helper", &ptr_i8);
        self.emit(&format!("  br label %{}", merge_label));
        self.emit_label(&merge_label);
    }

    fn clear_decayed_helper_owned_local(&mut self, target: &Expr) {
        let Expr::Ident(name, _) = target else {
            return;
        };
        if !self.helper_owned_pointer_locals.contains_key(name) {
            return;
        }
        let Some((addr, ty)) = self.locals.get(name).cloned() else {
            return;
        };
        if !ty.ends_with('*') {
            return;
        }
        self.emit(&format!("  store {} null, {}* {}", ty, ty, addr));
    }

    fn compile_scoped_ownership_expr(
        &mut self,
        target: &Expr,
        body: &Expr,
        imported_begin_fn: &str,
        helper_begin_fn: &str,
        imported_end_fn: &str,
        helper_end_fn: &str,
    ) -> KainResult<(String, String)> {
        if self.ownership_pointer_provenance_for_expr(target)
            == OwnershipPointerProvenance::EphemeralLocal
        {
            return self.compile_expr(body);
        }
        let (ptr_i8, provenance) = self.compile_ownership_pointer(target)?;
        let begin_fn = if provenance == OwnershipPointerProvenance::HelperOwned {
            helper_begin_fn
        } else {
            imported_begin_fn
        };
        let end_fn = if provenance == OwnershipPointerProvenance::HelperOwned {
            helper_end_fn
        } else {
            imported_end_fn
        };
        self.emit_checked_ownership_call(begin_fn, &ptr_i8);
        let body_result = self.compile_expr(body)?;
        self.emit_checked_ownership_call(end_fn, &ptr_i8);
        Ok(body_result)
    }

    fn compile_decay_expr(&mut self, target: &Expr) -> KainResult<(String, String)> {
        if self.ownership_pointer_provenance_for_expr(target)
            == OwnershipPointerProvenance::EphemeralLocal
        {
            return Ok(("0".to_string(), "void".to_string()));
        }
        let (ptr_i8, provenance) = self.compile_ownership_pointer(target)?;
        let decay_fn = if provenance == OwnershipPointerProvenance::HelperOwned {
            "__kain_ownership_decay_helper"
        } else {
            "__kain_ownership_decay"
        };
        self.emit_checked_ownership_call(decay_fn, &ptr_i8);
        if provenance == OwnershipPointerProvenance::HelperOwned {
            self.clear_decayed_helper_owned_local(target);
        }
        Ok(("0".to_string(), "void".to_string()))
    }

    fn compile_payload_pointer_from_value(
        &mut self,
        value: &str,
        value_ty: &str,
        span: Span,
    ) -> KainResult<(String, usize)> {
        if value_ty == "void" {
            return Ok(("null".to_string(), 0));
        }

        let (payload_size, _) = self.abi_layout_for_ty(value_ty, span)?;
        if value_ty == "i8*" {
            self.emit_rc_retain_if_heap_i8(value);
        } else if value_ty.starts_with('%') && value_ty.ends_with('*') {
            let retained = self.next_reg();
            self.emit(&format!(
                "  {} = bitcast {} {} to i8*",
                retained, value_ty, value
            ));
            self.emit_rc_retain_if_heap_i8(&retained);
        }
        let stack_slot = self.next_reg();
        self.emit_entry_alloca(&stack_slot, value_ty);
        self.emit(&format!(
            "  store {} {}, {}* {}",
            value_ty, value, value_ty, stack_slot
        ));
        let payload_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = bitcast {}* {} to i8*",
            payload_ptr, value_ty, stack_slot
        ));
        Ok((payload_ptr, payload_size))
    }

    fn compile_tagged_payload_copy(
        &mut self,
        boxed_value: &str,
        target_ty: &str,
        copy_function: &str,
        span: Span,
    ) -> KainResult<(String, String)> {
        if target_ty == "void" {
            return Ok(("0".to_string(), "i64".to_string()));
        }

        let (payload_size, _) = self.abi_layout_for_ty(target_ty, span)?;
        let out_slot = self.next_reg();
        self.emit_entry_alloca(&out_slot, target_ty);
        let out_i8 = self.next_reg();
        self.emit(&format!(
            "  {} = bitcast {}* {} to i8*",
            out_i8, target_ty, out_slot
        ));
        let status = self.next_reg();
        self.emit(&format!(
            "  {} = call i64 @{}(i8* {}, i8* {}, i64 {})",
            status, copy_function, boxed_value, out_i8, payload_size
        ));
        let loaded = self.next_reg();
        self.emit(&format!(
            "  {} = load {}, {}* {}",
            loaded, target_ty, target_ty, out_slot
        ));
        Ok((loaded, target_ty.to_string()))
    }

    fn compile_native_option_or_result_variant(
        &mut self,
        enum_name: &str,
        variant: &str,
        fields: &kain_core::ast::EnumVariantFields,
        span: Span,
    ) -> KainResult<Option<(String, String)>> {
        match (enum_name, variant) {
            ("Option", "None") => {
                return Ok(Some(("null".to_string(), "i8*".to_string())));
            }
            ("Option", "Some") | ("Result", "Ok") | ("Result", "Err") => {}
            _ => return Ok(None),
        };

        let values = match fields {
            kain_core::ast::EnumVariantFields::Tuple(values) if values.len() == 1 => values,
            _ => {
                return Err(KainError::codegen(
                    format!("{}::{} expects exactly one payload", enum_name, variant),
                    span,
                ))
            }
        };

        let tag = match (enum_name, variant) {
            ("Option", "Some") => ABI_TAG_OPTION_SOME_LLVM,
            ("Result", "Ok") => ABI_TAG_RESULT_OK_LLVM,
            ("Result", "Err") => ABI_TAG_RESULT_ERR_LLVM,
            _ => unreachable!(),
        };
        if let Some(literal) = self.extract_static_string_literal(&values[0]) {
            let literal_ptr = self.compile_static_c_string_literal(&literal);
            return Ok(Some(self.compile_tagged_immediate_borrowed_pointer_handle(
                tag,
                &literal_ptr,
            )));
        }
        let (payload_value, payload_ty) = self.compile_expr(&values[0])?;
        Ok(Some(self.compile_tagged_value_from_compiled_payload(
            tag,
            &payload_value,
            &payload_ty,
            span,
        )?))
    }

    fn compile_native_variant_function_call(
        &mut self,
        func_name: &str,
        args: &[kain_core::ast::CallArg],
        span: Span,
    ) -> KainResult<Option<(String, String)>> {
        match func_name {
            "Some" => {
                if args.len() != 1 {
                    return Err(KainError::codegen(
                        "Some expects exactly one argument",
                        span,
                    ));
                }
                if let Some(literal) = self.extract_static_string_literal(&args[0].value) {
                    let literal_ptr = self.compile_static_c_string_literal(&literal);
                    return Ok(Some(self.compile_tagged_immediate_borrowed_pointer_handle(
                        ABI_TAG_OPTION_SOME_LLVM,
                        &literal_ptr,
                    )));
                }
                let (payload_value, payload_ty) = self.compile_expr(&args[0].value)?;
                Ok(Some(self.compile_tagged_value_from_compiled_payload(
                    ABI_TAG_OPTION_SOME_LLVM,
                    &payload_value,
                    &payload_ty,
                    span,
                )?))
            }
            "Ok" | "Err" => {
                if args.len() != 1 {
                    return Err(KainError::codegen(
                        format!("{} expects exactly one argument", func_name),
                        span,
                    ));
                }
                let tag = if func_name == "Ok" {
                    ABI_TAG_RESULT_OK_LLVM
                } else {
                    ABI_TAG_RESULT_ERR_LLVM
                };
                if let Some(literal) = self.extract_static_string_literal(&args[0].value) {
                    let literal_ptr = self.compile_static_c_string_literal(&literal);
                    return Ok(Some(self.compile_tagged_immediate_borrowed_pointer_handle(
                        tag,
                        &literal_ptr,
                    )));
                }
                let (payload_value, payload_ty) = self.compile_expr(&args[0].value)?;
                Ok(Some(self.compile_tagged_value_from_compiled_payload(
                    tag,
                    &payload_value,
                    &payload_ty,
                    span,
                )?))
            }
            _ => Ok(None),
        }
    }

    fn compile_async_block(&mut self, body: &Expr, span: Span) -> KainResult<(String, String)> {
        let (payload_value, payload_ty) = self.compile_expr(body)?;
        let (payload_ptr, payload_size) =
            self.compile_payload_pointer_from_value(&payload_value, &payload_ty, span)?;
        let future = self.next_reg();
        self.emit(&format!(
            "  {} = call i8* @abi_future_ready_from_value(i8* {}, i64 {})",
            future, payload_ptr, payload_size
        ));
        Ok((future, "i8*".to_string()))
    }

    fn extract_immediate_ready_future_payload_expr<'a>(expr: &'a Expr) -> Option<&'a Expr> {
        match expr {
            Expr::AsyncBlock(payload, _) => Some(payload.as_ref()),
            Expr::Paren(inner, _) => Self::extract_immediate_ready_future_payload_expr(inner),
            Expr::Return(Some(inner), _) => {
                Self::extract_immediate_ready_future_payload_expr(inner)
            }
            _ => None,
        }
    }

    fn extract_zero_arg_immediate_ready_future_payload(func: &TypedFunction) -> Option<Expr> {
        if !func.ast.params.is_empty() {
            return None;
        }

        let returns_future = match &func.resolved_type {
            ResolvedType::Function { ret, .. } => matches!(ret.as_ref(), ResolvedType::Future(_)),
            _ => false,
        } || matches!(
            func.ast.return_type.as_ref(),
            Some(Type::Named { name, generics, .. }) if name == "Future" && generics.len() == 1
        ) || matches!(
            func.ast.return_type.as_ref(),
            Some(Type::Impl { trait_name, .. }) if trait_name == "Future"
        );
        if !returns_future {
            return None;
        }

        match func.ast.body.stmts.as_slice() {
            [Stmt::Return(Some(expr), _)] => {
                Self::extract_immediate_ready_future_payload_expr(expr)
            }
            [Stmt::Expr(expr)] => Self::extract_immediate_ready_future_payload_expr(expr),
            _ => None,
        }
        .cloned()
    }

    fn compile_immediate_ready_future_for_target_type(
        &mut self,
        future_expr: &Expr,
        target_ty: &str,
    ) -> KainResult<Option<(String, String)>> {
        match future_expr {
            Expr::AsyncBlock(body, _) => {
                Ok(Some(self.compile_expr_for_target_type(body, target_ty)?))
            }
            Expr::Paren(inner, _) => {
                self.compile_immediate_ready_future_for_target_type(inner, target_ty)
            }
            Expr::Call { callee, args, .. } if args.is_empty() => {
                let Expr::Ident(name, _) = callee.as_ref() else {
                    return Ok(None);
                };
                let Some(payload) = self.immediate_ready_future_payloads.get(name).cloned() else {
                    return Ok(None);
                };
                Ok(Some(
                    self.compile_expr_for_target_type(&payload, target_ty)?,
                ))
            }
            _ => Ok(None),
        }
    }

    fn compile_await_for_target_type(
        &mut self,
        future_expr: &Expr,
        target_ty: &str,
        span: Span,
    ) -> KainResult<(String, String)> {
        if let Some(inlined_result) =
            self.compile_immediate_ready_future_for_target_type(future_expr, target_ty)?
        {
            return Ok(inlined_result);
        }
        let (future_value, future_ty) = self.compile_expr(future_expr)?;
        if future_ty != "i8*" {
            return Err(KainError::codegen(
                format!("await expected native Future handle, found {}", future_ty),
                span,
            ));
        }
        let result = self.compile_tagged_payload_copy(
            &future_value,
            target_ty,
            "abi_future_await_payload_copy",
            span,
        )?;
        self.emit_release_if_new_object_expr(future_expr, &future_value, &future_ty);
        Ok(result)
    }

    fn compile_try_for_target_type(
        &mut self,
        value_expr: &Expr,
        target_ty: &str,
        span: Span,
    ) -> KainResult<(String, String)> {
        let (boxed_value, boxed_ty) = self.compile_expr(value_expr)?;
        if boxed_ty != "i8*" {
            return Err(KainError::codegen(
                format!(
                    "'?' expected native Option or Result handle, found {}",
                    boxed_ty
                ),
                span,
            ));
        }

        let success = self.compile_tagged_value_is_tag(
            &boxed_value,
            &[ABI_TAG_OPTION_SOME_LLVM, ABI_TAG_RESULT_OK_LLVM],
            false,
        );

        let payload_label = self.next_label();
        let residual_label = self.next_label();
        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            success, payload_label, residual_label
        ));

        self.emit_label(&residual_label);
        if self.current_return_type.as_deref() == Some("i8*") {
            self.emit_all_scopes_cleanup();
            self.emit(&format!("  ret i8* {}", boxed_value));
        } else {
            return Err(KainError::codegen(
                "'?' residual propagation requires an Option or Result return type in LLVM",
                span,
            ));
        }

        self.emit_label(&payload_label);
        let result = self.compile_tagged_value_payload_copy(&boxed_value, target_ty);
        self.emit_release_if_new_object_expr(value_expr, &boxed_value, &boxed_ty);
        Ok(result)
    }

    fn coerce_to_i64_storage(&mut self, val: &str, ty: &str) -> String {
        match ty {
            "i64" => val.to_string(),
            "i32" => {
                let reg = self.next_reg();
                self.emit(&format!("  {} = sext i32 {} to i64", reg, val));
                reg
            }
            "i1" => {
                let reg = self.next_reg();
                self.emit(&format!("  {} = zext i1 {} to i64", reg, val));
                reg
            }
            "i8" => {
                let reg = self.next_reg();
                self.emit(&format!("  {} = sext i8 {} to i64", reg, val));
                reg
            }
            "double" => {
                let reg = self.next_reg();
                self.emit(&format!("  {} = fptosi double {} to i64", reg, val));
                reg
            }
            _ if ty.ends_with('*') => {
                let reg = self.next_reg();
                self.emit(&format!("  {} = ptrtoint {} {} to i64", reg, ty, val));
                reg
            }
            _ => val.to_string(),
        }
    }

    fn expr_returns_json_handle(&self, expr: &Expr) -> bool {
        match expr {
            Expr::Ident(name, _) => self.json_handle_locals.contains(name),
            Expr::Paren(inner, _) => self.expr_returns_json_handle(inner),
            Expr::Cast { value, .. } => self.expr_returns_json_handle(value),
            Expr::Call { callee, .. } => matches!(
                callee.as_ref(),
                Expr::Ident(name, _)
                    if matches!(
                        name.as_str(),
                        "json_object_new"
                            | "json_array_new"
                            | "json_parse"
                            | "json_get"
                            | "json_array_get"
                    )
            ),
            _ => false,
        }
    }

    fn encode_json_any_i64_payload(&mut self, value: &str, tag: i64) -> String {
        let shifted = self.next_reg();
        self.emit(&format!("  {} = shl i64 {}, 3", shifted, value));
        let tagged = self.next_reg();
        self.emit(&format!("  {} = or i64 {}, {}", tagged, shifted, tag));
        tagged
    }

    fn compile_json_any_argument(&mut self, expr: &Expr) -> KainResult<JsonAnyArgument> {
        if matches!(expr, Expr::None(_)) {
            return Ok(JsonAnyArgument {
                value: JSON_ANY_TAG_NULL_LLVM.to_string(),
                release_after_call: false,
            });
        }
        if matches!(expr, Expr::Ident(name, _) if name == "None") {
            return Ok(JsonAnyArgument {
                value: JSON_ANY_TAG_NULL_LLVM.to_string(),
                release_after_call: false,
            });
        }

        let is_json_handle = self.expr_returns_json_handle(expr);
        let (value, ty) = self.compile_expr(expr)?;
        if is_json_handle {
            return Ok(JsonAnyArgument {
                value: self.coerce_to_i64_storage(&value, &ty),
                release_after_call: false,
            });
        }

        let any = match ty.as_str() {
            "i64" => JsonAnyArgument {
                value: self.encode_json_any_i64_payload(&value, JSON_ANY_TAG_INT_LLVM),
                release_after_call: false,
            },
            "i32" | "i8" => {
                let widened = self.cast_numeric_value(value, &ty, "i64")?;
                JsonAnyArgument {
                    value: self.encode_json_any_i64_payload(&widened, JSON_ANY_TAG_INT_LLVM),
                    release_after_call: false,
                }
            }
            "i1" => {
                let widened = self.cast_numeric_value(value, "i1", "i64")?;
                JsonAnyArgument {
                    value: self.encode_json_any_i64_payload(&widened, JSON_ANY_TAG_BOOL_LLVM),
                    release_after_call: false,
                }
            }
            "double" => {
                let boxed = self.next_reg();
                self.emit(&format!(
                    "  {} = call i64 @json_box_float(double {})",
                    boxed, value
                ));
                JsonAnyArgument {
                    value: boxed,
                    release_after_call: true,
                }
            }
            "i8*" => {
                let pointer_bits = self.next_reg();
                self.emit(&format!(
                    "  {} = ptrtoint i8* {} to i64",
                    pointer_bits, value
                ));
                let tagged = self.next_reg();
                self.emit(&format!(
                    "  {} = or i64 {}, {}",
                    tagged, pointer_bits, JSON_ANY_TAG_STRING_LLVM
                ));
                JsonAnyArgument {
                    value: tagged,
                    release_after_call: false,
                }
            }
            _ if ty.ends_with('*') => JsonAnyArgument {
                value: self.coerce_to_i64_storage(&value, &ty),
                release_after_call: false,
            },
            _ => JsonAnyArgument {
                value: self.coerce_to_i64_storage(&value, &ty),
                release_after_call: false,
            },
        };
        Ok(any)
    }

    fn compile_json_builtin_call(
        &mut self,
        func_name: &str,
        args: &[kain_core::ast::CallArg],
    ) -> KainResult<Option<(String, String)>> {
        match (func_name, args.len()) {
            ("json_object_set", 3) => {
                let (object, object_ty) = self.compile_expr(&args[0].value)?;
                let object_i64 = self.coerce_to_i64_storage(&object, &object_ty);
                let (key, key_ty) = self.compile_expr_for_target_type(&args[1].value, "i8*")?;
                let value_any = self.compile_json_any_argument(&args[2].value)?;
                self.emit(&format!(
                    "  call void @json_object_set(i64 {}, i8* {}, i64 {})",
                    object_i64, key, value_any.value
                ));
                if value_any.release_after_call {
                    self.emit(&format!(
                        "  call void @json_release(i64 {})",
                        value_any.value
                    ));
                }
                self.emit_release_if_new_object_expr(&args[1].value, &key, &key_ty);
                Ok(Some(("0".to_string(), "i64".to_string())))
            }
            ("json_array_push", 2) => {
                let (array, array_ty) = self.compile_expr(&args[0].value)?;
                let array_i64 = self.coerce_to_i64_storage(&array, &array_ty);
                let value_any = self.compile_json_any_argument(&args[1].value)?;
                self.emit(&format!(
                    "  call void @json_array_push(i64 {}, i64 {})",
                    array_i64, value_any.value
                ));
                if value_any.release_after_call {
                    self.emit(&format!(
                        "  call void @json_release(i64 {})",
                        value_any.value
                    ));
                }
                Ok(Some(("0".to_string(), "i64".to_string())))
            }
            ("json_string", 1) => {
                let value_any = self.compile_json_any_argument(&args[0].value)?;
                let result = self.next_reg();
                self.emit(&format!(
                    "  {} = call i8* @json_string(i64 {})",
                    result, value_any.value
                ));
                if value_any.release_after_call {
                    self.emit(&format!(
                        "  call void @json_release(i64 {})",
                        value_any.value
                    ));
                }
                Ok(Some((result, "i8*".to_string())))
            }
            _ => Ok(None),
        }
    }

    fn cast_numeric_value(
        &mut self,
        val: String,
        src_ty: &str,
        dst_ty: &str,
    ) -> KainResult<String> {
        if src_ty == dst_ty {
            return Ok(val);
        }

        if src_ty.ends_with('*') && matches!(dst_ty, "i64" | "i32" | "i8" | "i1") {
            let ptr_bits = self.next_reg();
            self.emit(&format!(
                "  {} = ptrtoint {} {} to i64",
                ptr_bits, src_ty, val
            ));
            return if dst_ty == "i64" {
                Ok(ptr_bits)
            } else {
                self.cast_numeric_value(ptr_bits, "i64", dst_ty)
            };
        }

        let reg = self.next_reg();
        match (src_ty, dst_ty) {
            ("i64", "double") => {
                self.emit(&format!("  {} = sitofp i64 {} to double", reg, val));
                Ok(reg)
            }
            ("i64", "i32") => {
                self.emit(&format!("  {} = trunc i64 {} to i32", reg, val));
                Ok(reg)
            }
            ("i64", "i8") => {
                self.emit(&format!("  {} = trunc i64 {} to i8", reg, val));
                Ok(reg)
            }
            ("i64", "i1") => {
                self.emit(&format!("  {} = icmp ne i64 {}, 0", reg, val));
                Ok(reg)
            }
            ("i32", "double") => {
                self.emit(&format!("  {} = sitofp i32 {} to double", reg, val));
                Ok(reg)
            }
            ("i32", "i64") => {
                self.emit(&format!("  {} = sext i32 {} to i64", reg, val));
                Ok(reg)
            }
            ("i32", "i8") => {
                self.emit(&format!("  {} = trunc i32 {} to i8", reg, val));
                Ok(reg)
            }
            ("i32", "i1") => {
                self.emit(&format!("  {} = icmp ne i32 {}, 0", reg, val));
                Ok(reg)
            }
            ("i1", "double") => {
                self.emit(&format!("  {} = uitofp i1 {} to double", reg, val));
                Ok(reg)
            }
            ("i1", "i32") => {
                self.emit(&format!("  {} = zext i1 {} to i32", reg, val));
                Ok(reg)
            }
            ("i8", "double") => {
                self.emit(&format!("  {} = sitofp i8 {} to double", reg, val));
                Ok(reg)
            }
            ("i1", "i64") => {
                self.emit(&format!("  {} = zext i1 {} to i64", reg, val));
                Ok(reg)
            }
            ("i8", "i32") => {
                self.emit(&format!("  {} = sext i8 {} to i32", reg, val));
                Ok(reg)
            }
            ("i8", "i64") => {
                self.emit(&format!("  {} = sext i8 {} to i64", reg, val));
                Ok(reg)
            }
            ("double", "i64") => {
                self.emit(&format!("  {} = fptosi double {} to i64", reg, val));
                Ok(reg)
            }
            ("double", "i32") => {
                self.emit(&format!("  {} = fptosi double {} to i32", reg, val));
                Ok(reg)
            }
            ("double", "i8") => {
                self.emit(&format!("  {} = fptosi double {} to i8", reg, val));
                Ok(reg)
            }
            ("double", "i1") => {
                self.emit(&format!("  {} = fcmp one double {}, 0.0", reg, val));
                Ok(reg)
            }
            _ => Err(KainError::codegen(
                format!("Unsupported numeric cast from {} to {}", src_ty, dst_ty),
                kain_core::Span::default(),
            )),
        }
    }

    fn coerce_binary_operands(
        &mut self,
        lhs: String,
        lhs_ty: String,
        rhs: String,
        rhs_ty: String,
    ) -> KainResult<(String, String, String, String)> {
        if lhs_ty == rhs_ty {
            return Ok((lhs, lhs_ty, rhs, rhs_ty));
        }

        if lhs_ty == "double" {
            let rhs_cast = self.cast_numeric_value(rhs, &rhs_ty, "double")?;
            return Ok((lhs, lhs_ty, rhs_cast, "double".to_string()));
        }

        if rhs_ty == "double" {
            let lhs_cast = self.cast_numeric_value(lhs, &lhs_ty, "double")?;
            return Ok((lhs_cast, "double".to_string(), rhs, rhs_ty));
        }

        if lhs_ty == "i64" {
            let rhs_cast = self.cast_numeric_value(rhs, &rhs_ty, "i64")?;
            return Ok((lhs, lhs_ty, rhs_cast, "i64".to_string()));
        }

        if rhs_ty == "i64" {
            let lhs_cast = self.cast_numeric_value(lhs, &lhs_ty, "i64")?;
            return Ok((lhs_cast, "i64".to_string(), rhs, rhs_ty));
        }

        Ok((lhs, lhs_ty, rhs, rhs_ty))
    }

    fn compile_value_eq(
        &mut self,
        lhs: &str,
        lhs_ty: &str,
        rhs: &str,
        rhs_ty: &str,
        span: kain_core::Span,
    ) -> KainResult<String> {
        let (lhs, lhs_ty, rhs, rhs_ty) = self.coerce_binary_operands(
            lhs.to_string(),
            lhs_ty.to_string(),
            rhs.to_string(),
            rhs_ty.to_string(),
        )?;

        let res = self.next_reg();
        if lhs_ty == "i8*" || rhs_ty == "i8*" {
            self.emit(&format!(
                "  {} = call i1 @deep_eq(i8* {}, i8* {})",
                res, lhs, rhs
            ));
            return Ok(res);
        }

        match lhs_ty.as_str() {
            "double" => self.emit(&format!("  {} = fcmp oeq double {}, {}", res, lhs, rhs)),
            "i1" | "i8" | "i64" => {
                self.emit(&format!("  {} = icmp eq {} {}, {}", res, lhs_ty, lhs, rhs))
            }
            _ if lhs_ty.ends_with('*') => {
                self.emit(&format!("  {} = icmp eq {} {}, {}", res, lhs_ty, lhs, rhs))
            }
            _ => {
                return Err(KainError::codegen(
                    format!(
                        "Unsupported equality comparison between {} and {}",
                        lhs_ty, rhs_ty
                    ),
                    span,
                ));
            }
        }

        Ok(res)
    }

    fn compile_range_check(
        &mut self,
        val: &str,
        val_ty: &str,
        start: &Option<Box<Expr>>,
        end: &Option<Box<Expr>>,
        inclusive: bool,
        span: kain_core::Span,
    ) -> KainResult<String> {
        let mut checks = Vec::new();

        if let Some(lo) = start {
            let (lo_val, lo_ty) = self.compile_expr(lo)?;
            let (lhs, lhs_ty, rhs, _) =
                self.coerce_binary_operands(val.to_string(), val_ty.to_string(), lo_val, lo_ty)?;
            let cmp = self.next_reg();
            match lhs_ty.as_str() {
                "double" => self.emit(&format!("  {} = fcmp oge double {}, {}", cmp, lhs, rhs)),
                "i1" | "i8" | "i64" => {
                    self.emit(&format!("  {} = icmp sge {} {}, {}", cmp, lhs_ty, lhs, rhs))
                }
                _ => {
                    return Err(KainError::codegen(
                        format!("Unsupported range lower bound type {}", lhs_ty),
                        span,
                    ))
                }
            }
            checks.push(cmp);
        }

        if let Some(hi) = end {
            let (hi_val, hi_ty) = self.compile_expr(hi)?;
            let (lhs, lhs_ty, rhs, _) =
                self.coerce_binary_operands(val.to_string(), val_ty.to_string(), hi_val, hi_ty)?;
            let cmp = self.next_reg();
            match lhs_ty.as_str() {
                "double" => {
                    let op = if inclusive { "fcmp ole" } else { "fcmp olt" };
                    self.emit(&format!("  {} = {} double {}, {}", cmp, op, lhs, rhs));
                }
                "i1" | "i8" | "i64" => {
                    let op = if inclusive { "icmp sle" } else { "icmp slt" };
                    self.emit(&format!("  {} = {} {} {}, {}", cmp, op, lhs_ty, lhs, rhs));
                }
                _ => {
                    return Err(KainError::codegen(
                        format!("Unsupported range upper bound type {}", lhs_ty),
                        span,
                    ))
                }
            }
            checks.push(cmp);
        }

        if checks.is_empty() {
            return Ok("1".to_string());
        }

        let mut current = checks[0].clone();
        for check in checks.iter().skip(1) {
            let combined = self.next_reg();
            self.emit(&format!("  {} = and i1 {}, {}", combined, current, check));
            current = combined;
        }

        Ok(current)
    }

    fn compile_pattern_condition(
        &mut self,
        pattern: &Pattern,
        scrutinee_val: &str,
        scrutinee_ty: &str,
        enum_name: Option<&str>,
        span: kain_core::Span,
    ) -> KainResult<String> {
        match pattern {
            Pattern::Wildcard(_) | Pattern::Binding { .. } => Ok("1".to_string()),
            Pattern::Literal(expr) => {
                let (rhs, rhs_ty) = self.compile_expr(expr)?;
                self.compile_value_eq(scrutinee_val, scrutinee_ty, &rhs, &rhs_ty, span)
            }
            Pattern::Range {
                start,
                end,
                inclusive,
                ..
            } => {
                self.compile_range_check(scrutinee_val, scrutinee_ty, start, end, *inclusive, span)
            }
            Pattern::Or(items, _) => {
                let mut regs = Vec::new();
                for item in items {
                    regs.push(self.compile_pattern_condition(
                        item,
                        scrutinee_val,
                        scrutinee_ty,
                        enum_name,
                        span,
                    )?);
                }
                if regs.is_empty() {
                    return Ok("0".to_string());
                }
                let mut current = regs[0].clone();
                for reg in regs.iter().skip(1) {
                    let merged = self.next_reg();
                    self.emit(&format!("  {} = or i1 {}, {}", merged, current, reg));
                    current = merged;
                }
                Ok(current)
            }
            Pattern::Variant { variant, .. } => {
                if scrutinee_ty == "i8*" {
                    let native_tag = match variant.as_str() {
                        "None" => Some(0),
                        "Some" => Some(1),
                        "Ok" => Some(2),
                        "Err" => Some(3),
                        _ => None,
                    };
                    if let Some(tag) = native_tag {
                        return Ok(self.compile_tagged_value_is_tag(
                            scrutinee_val,
                            &[tag],
                            tag == ABI_TAG_OPTION_NONE_LLVM,
                        ));
                    }
                }

                let enum_name = enum_name.ok_or_else(|| {
                    KainError::codegen("Variant pattern requires an enum scrutinee", span)
                })?;
                let tag_ptr = self.next_reg();
                self.emit(&format!(
                    "  {} = getelementptr inbounds %{}, {} {}, i32 0, i32 0",
                    tag_ptr, enum_name, scrutinee_ty, scrutinee_val
                ));
                let tag = self.next_reg();
                self.emit(&format!("  {} = load i64, i64* {}", tag, tag_ptr));
                let cmp = self.next_reg();
                self.emit(&format!(
                    "  {} = icmp eq i64 {}, {}",
                    cmp,
                    tag,
                    self.hash_message_tag(enum_name, variant)
                ));
                Ok(cmp)
            }
            other => Err(KainError::codegen(
                format!("Unsupported LLVM pattern condition: {:?}", other),
                span,
            )),
        }
    }

    fn bind_local_pattern_value(
        &mut self,
        pattern: &Pattern,
        val: String,
        ty: String,
    ) -> KainResult<()> {
        match pattern {
            Pattern::Wildcard(_) => Ok(()),
            Pattern::Binding { name, .. } => {
                let addr_reg = format!("%{}.addr_{}", name, self.reg_count);
                self.reg_count += 1;
                self.emit_entry_alloca(&addr_reg, &ty);
                self.emit(&format!("  store {} {}, {}* {}", ty, val, ty, addr_reg));

                self.locals.insert(name.clone(), (addr_reg, ty));
                if let Some(scope) = self.scopes.last_mut() {
                    scope.push(name.clone());
                }
                Ok(())
            }
            Pattern::Tuple(patterns, span) => {
                let struct_name = ty
                    .strip_prefix('%')
                    .map(|name| name.trim_end_matches('*'))
                    .ok_or_else(|| {
                        KainError::codegen(
                            format!("Tuple pattern requires tuple aggregate value, got {}", ty),
                            *span,
                        )
                    })?
                    .to_string();
                let (pattern_val, pattern_ty) = if ty.ends_with('*') {
                    (val, ty)
                } else {
                    let addr_reg = format!("%tuple.pattern.addr_{}", self.reg_count);
                    self.reg_count += 1;
                    self.emit_entry_alloca(&addr_reg, &ty);
                    self.emit(&format!("  store {} {}, {}* {}", ty, val, ty, addr_reg));
                    (addr_reg, format!("{}*", ty))
                };

                let field_defs = self.struct_defs.get(&struct_name).cloned().ok_or_else(|| {
                    KainError::codegen(
                        format!("Unknown tuple storage type for pattern: {}", struct_name),
                        *span,
                    )
                })?;

                for (index, sub_pattern) in patterns.iter().enumerate() {
                    let field_ty = field_defs
                        .get(index)
                        .map(|(_, ty)| ty.clone())
                        .unwrap_or_else(|| "i64".to_string());
                    let field_ptr = self.next_reg();
                    self.emit(&format!(
                        "  {} = getelementptr inbounds %{}, {} {}, i32 0, i32 {}",
                        field_ptr, struct_name, pattern_ty, pattern_val, index
                    ));
                    let field_val = self.next_reg();
                    self.emit(&format!("  {} = load {}, {}* {}", field_val, field_ty, field_ty, field_ptr));
                    if field_ty == "i8*" {
                        self.emit_rc_retain_if_heap_i8(&field_val);
                    }
                    self.bind_local_pattern_value(sub_pattern, field_val, field_ty)?;
                }
                Ok(())
            }
            Pattern::Struct { name, fields, span, .. } => {
                let struct_name = ty
                    .strip_prefix('%')
                    .map(|name| name.trim_end_matches('*'))
                    .ok_or_else(|| {
                        KainError::codegen(
                            format!("Struct pattern requires struct aggregate value, got {}", ty),
                            *span,
                        )
                    })?
                    .to_string();
                let (pattern_val, pattern_ty) = if ty.ends_with('*') {
                    (val, ty)
                } else {
                    let addr_reg = format!("%struct.pattern.addr_{}", self.reg_count);
                    self.reg_count += 1;
                    self.emit_entry_alloca(&addr_reg, &ty);
                    self.emit(&format!("  store {} {}, {}* {}", ty, val, ty, addr_reg));
                    (addr_reg, format!("{}*", ty))
                };

                if &struct_name != name {
                    return Err(KainError::codegen(
                        format!("Struct pattern expected {}, got {}", name, struct_name),
                        *span,
                    ));
                }

                let field_defs = self.struct_defs.get(&struct_name).cloned().ok_or_else(|| {
                    KainError::codegen(
                        format!("Unknown struct storage type for pattern: {}", struct_name),
                        *span,
                    )
                })?;

                for (field_name, sub_pattern) in fields {
                    let (index, field_ty) = field_defs
                        .iter()
                        .enumerate()
                        .find(|(_, (name, _))| name == field_name)
                        .map(|(index, (_, ty))| (index, ty.clone()))
                        .ok_or_else(|| {
                            KainError::codegen(
                                format!("Unknown struct field '{}' on {}", field_name, struct_name),
                                *span,
                            )
                        })?;

                    let field_ptr = self.next_reg();
                    self.emit(&format!(
                        "  {} = getelementptr inbounds %{}, {} {}, i32 0, i32 {}",
                        field_ptr, struct_name, pattern_ty, pattern_val, index
                    ));
                    let field_val = self.next_reg();
                    self.emit(&format!("  {} = load {}, {}* {}", field_val, field_ty, field_ty, field_ptr));
                    if field_ty == "i8*" {
                        self.emit_rc_retain_if_heap_i8(&field_val);
                    }
                    self.bind_local_pattern_value(sub_pattern, field_val, field_ty)?;
                }
                Ok(())
            }
            _ => Err(KainError::codegen(
                "Local pattern binding currently supports wildcard, binding, tuple, and struct patterns",
                kain_core::Span::default(),
            )),
        }
    }

    fn bind_variant_pattern_fields(
        &mut self,
        enum_name: &str,
        variant: &str,
        fields: &VariantPatternFields,
        scrutinee_val: &str,
        scrutinee_ty: &str,
        span: kain_core::Span,
    ) -> KainResult<()> {
        let payload_struct_name = format!("{}_{}", enum_name, variant);
        if !self.struct_defs.contains_key(&payload_struct_name) {
            return Ok(());
        }

        let payload_ty = format!("%{}", payload_struct_name);
        let payload_ptr_ty = format!("{}*", payload_ty);
        let payload_ptr_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = getelementptr inbounds %{}, {} {}, i32 0, i32 1",
            payload_ptr_ptr, enum_name, scrutinee_ty, scrutinee_val
        ));
        let payload_void = self.next_reg();
        self.emit(&format!(
            "  {} = load i8*, i8** {}",
            payload_void, payload_ptr_ptr
        ));
        let payload_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = bitcast i8* {} to {}",
            payload_ptr, payload_void, payload_ptr_ty
        ));

        match fields {
            VariantPatternFields::Unit => Ok(()),
            VariantPatternFields::Tuple(patterns) => {
                let field_defs = self
                    .struct_defs
                    .get(&payload_struct_name)
                    .cloned()
                    .unwrap_or_default();
                for (index, pattern) in patterns.iter().enumerate() {
                    let field_ty = field_defs
                        .get(index)
                        .map(|(_, ty)| ty.clone())
                        .unwrap_or_else(|| "i64".to_string());
                    let field_ptr = self.next_reg();
                    self.emit(&format!(
                        "  {} = getelementptr inbounds {}, {} {}, i32 0, i32 {}",
                        field_ptr, payload_ty, payload_ptr_ty, payload_ptr, index
                    ));
                    let field_val = self.next_reg();
                    self.emit(&format!(
                        "  {} = load {}, {}* {}",
                        field_val, field_ty, field_ty, field_ptr
                    ));
                    self.bind_local_pattern_value(pattern, field_val, field_ty)?;
                }
                Ok(())
            }
            VariantPatternFields::Struct(named_patterns) => {
                let field_defs = self
                    .struct_defs
                    .get(&payload_struct_name)
                    .cloned()
                    .unwrap_or_default();
                for (field_name, pattern) in named_patterns {
                    let (index, field_ty) = field_defs
                        .iter()
                        .enumerate()
                        .find(|(_, (name, _))| name == field_name)
                        .map(|(index, (_, ty))| (index, ty.clone()))
                        .ok_or_else(|| {
                            KainError::codegen(
                                format!(
                                    "Unknown payload field '{}' for {}::{}",
                                    field_name, enum_name, variant
                                ),
                                span,
                            )
                        })?;
                    let field_ptr = self.next_reg();
                    self.emit(&format!(
                        "  {} = getelementptr inbounds {}, {} {}, i32 0, i32 {}",
                        field_ptr, payload_ty, payload_ptr_ty, payload_ptr, index
                    ));
                    let field_val = self.next_reg();
                    self.emit(&format!(
                        "  {} = load {}, {}* {}",
                        field_val, field_ty, field_ty, field_ptr
                    ));
                    self.bind_local_pattern_value(pattern, field_val, field_ty)?;
                }
                Ok(())
            }
        }
    }

    fn bind_match_pattern(
        &mut self,
        pattern: &Pattern,
        scrutinee_val: &str,
        scrutinee_ty: &str,
        enum_name: Option<&str>,
        span: kain_core::Span,
    ) -> KainResult<()> {
        match pattern {
            Pattern::Wildcard(_) => Ok(()),
            Pattern::Binding { .. } | Pattern::Tuple(_, _) | Pattern::Struct { .. } => self
                .bind_local_pattern_value(
                    pattern,
                    scrutinee_val.to_string(),
                    scrutinee_ty.to_string(),
                ),
            Pattern::Variant {
                variant, fields, ..
            } => {
                if scrutinee_ty == "i8*" {
                    match fields {
                        VariantPatternFields::Unit => return Ok(()),
                        VariantPatternFields::Tuple(patterns) if patterns.len() == 1 => {
                            let target_ty = "i64";
                            let (payload_value, payload_ty) =
                                self.compile_tagged_value_payload_copy(scrutinee_val, target_ty);
                            return self.bind_local_pattern_value(
                                &patterns[0],
                                payload_value,
                                payload_ty,
                            );
                        }
                        _ => {
                            return Err(KainError::codegen(
                                format!("Unsupported native tagged pattern fields for {}", variant),
                                span,
                            ))
                        }
                    }
                }

                let enum_name = enum_name.ok_or_else(|| {
                    KainError::codegen("Variant pattern requires an enum scrutinee", span)
                })?;
                self.bind_variant_pattern_fields(
                    enum_name,
                    variant,
                    fields,
                    scrutinee_val,
                    scrutinee_ty,
                    span,
                )
            }
            Pattern::Or(_, _) | Pattern::Literal(_) | Pattern::Range { .. } => Ok(()),
            other => Err(KainError::codegen(
                format!("Unsupported LLVM match binding pattern: {:?}", other),
                span,
            )),
        }
    }

    fn ptr_struct_name<'a>(&self, ty: &'a str) -> Option<&'a str> {
        if ty.starts_with('%') && ty.ends_with('*') {
            Some(&ty[1..ty.len() - 1])
        } else {
            None
        }
    }

    fn field_index(&self, struct_name: &str, field: &str) -> Option<usize> {
        let fields = self.struct_defs.get(struct_name)?;
        fields
            .iter()
            .position(|(name, _)| name == field)
            .or_else(|| {
                if struct_name.starts_with("__kain_tuple") {
                    let index = Self::tuple_field_alias_index(field)?;
                    (index < fields.len()).then_some(index)
                } else {
                    None
                }
            })
    }

    fn native_world_field_path(&self, struct_name: &str, field: &str) -> Option<String> {
        if self.world_globals.contains_key(struct_name) {
            Some(format!("{}.{}", struct_name, field))
        } else {
            None
        }
    }

    fn native_entangle_authority_binding(&self, path: &str) -> Option<NativeEntangleBinding> {
        self.native_entanglements
            .iter()
            .find(|binding| binding.authority == path)
            .cloned()
    }

    fn native_entangle_mirror_binding(&self, path: &str) -> Option<NativeEntangleBinding> {
        self.native_entanglements
            .iter()
            .find(|binding| binding.mirror == path)
            .cloned()
    }

    fn direct_struct_literal_name<'a>(&self, expr: &'a Expr) -> Option<&'a str> {
        match expr {
            Expr::Struct { name, .. } if self.shattered_structs.contains(name) => Some(name),
            Expr::Paren(inner, _) => self.direct_struct_literal_name(inner),
            _ => None,
        }
    }

    fn shattered_array_expr_struct_name(&self, expr: &Expr) -> Option<String> {
        let Expr::Array(items, _) = expr else {
            return None;
        };
        let first_name = self.direct_struct_literal_name(items.first()?)?;
        if items
            .iter()
            .all(|item| self.direct_struct_literal_name(item) == Some(first_name))
        {
            Some(first_name.to_string())
        } else {
            None
        }
    }

    fn emit_shatter_lane_bases(&mut self, handle: &str, lane_count: usize) -> Vec<String> {
        (0..lane_count)
            .map(|lane_index| {
                let base = self.next_reg();
                self.emit(&format!(
                    "  {} = call i8* @kain_machine_shatter_lane_base(i8* {}, i64 {})",
                    base, handle, lane_index
                ));
                base
            })
            .collect()
    }

    fn emit_stack_shatter_lane_bases(
        &mut self,
        local_name: &str,
        lane_count: usize,
        element_count: usize,
    ) -> Vec<String> {
        let lane_storage_ty = format!("[{} x i64]", element_count);
        (0..lane_count)
            .map(|lane_index| {
                let storage_reg = format!(
                    "%{}.shatter_lane{}_{}",
                    local_name, lane_index, self.reg_count
                );
                self.reg_count += 1;
                self.emit_entry_alloca(&storage_reg, &lane_storage_ty);
                let base = self.next_reg();
                self.emit(&format!(
                    "  {} = bitcast {}* {} to i8*",
                    base, lane_storage_ty, storage_reg
                ));
                base
            })
            .collect()
    }

    fn populate_shattered_array_literal_lanes(
        &mut self,
        struct_name: &str,
        fields: &[(String, String)],
        items: &[Expr],
        lane_bases: &[String],
    ) -> KainResult<()> {
        for (element_index, item) in items.iter().enumerate() {
            let Expr::Struct {
                name,
                fields: authored_fields,
                ..
            } = item
            else {
                return Err(KainError::codegen(
                    "shatter arrays require direct struct literals in LLVM lowering",
                    item.span(),
                ));
            };
            if name != struct_name {
                return Err(KainError::codegen(
                    format!("shatter array expected {}, found {}", struct_name, name),
                    item.span(),
                ));
            }
            for (lane_index, (field_name, field_ty)) in fields.iter().enumerate() {
                let (value, value_ty) = if let Some((_, authored_value)) =
                    authored_fields.iter().find(|(name, _)| name == field_name)
                {
                    self.compile_expr_for_target_type(authored_value, field_ty)?
                } else {
                    (self.zero_value_for_ty(field_ty), field_ty.clone())
                };
                let raw_ptr = self.next_reg();
                self.emit(&format!(
                    "  {} = getelementptr inbounds i8, i8* {}, i64 {}",
                    raw_ptr,
                    lane_bases
                        .get(lane_index)
                        .map(String::as_str)
                        .unwrap_or("null"),
                    element_index * 8
                ));
                let typed_ptr = self.next_reg();
                self.emit(&format!(
                    "  {} = bitcast i8* {} to {}*",
                    typed_ptr, raw_ptr, field_ty
                ));
                self.emit(&format!(
                    "  store {} {}, {}* {}",
                    value_ty, value, field_ty, typed_ptr
                ));
            }
        }
        Ok(())
    }

    fn shattered_index_is_proven_in_bounds(&self, index: &Expr, element_count: usize) -> bool {
        let element_count = element_count as i64;
        let known_i64_bindings = self.current_known_i64_literals();
        if let Some(index) = Self::resolve_i64_literal(index, &known_i64_bindings) {
            return index >= 0 && index < element_count;
        }
        match index {
            Expr::Ident(name, _) => self
                .active_loop_bounds_for(name)
                .map(|bounds| {
                    bounds.lower_inclusive >= 0
                        && bounds.lower_inclusive <= bounds.upper_exclusive
                        && bounds.upper_exclusive <= element_count
                })
                .unwrap_or(false),
            Expr::Paren(inner, _) | Expr::Cast { value: inner, .. } => {
                self.shattered_index_is_proven_in_bounds(inner, element_count as usize)
            }
            _ => false,
        }
    }

    fn shattered_literal_byte_offset(&self, index: &Expr) -> Option<i64> {
        let known_i64_bindings = self.current_known_i64_literals();
        let index = Self::resolve_i64_literal(index, &known_i64_bindings)?;
        if index < 0 {
            return None;
        }
        index.checked_mul(8)
    }

    fn compile_shattered_field_ptr(
        &mut self,
        object: &Expr,
        field: &str,
        span: Span,
    ) -> Option<KainResult<(String, String)>> {
        let Expr::Index {
            object: indexed_object,
            index,
            ..
        } = object
        else {
            return None;
        };
        let Expr::Ident(array_name, _) = indexed_object.as_ref() else {
            return None;
        };
        let local = self.shattered_array_locals.get(array_name).cloned()?;
        let field_index = match self.field_index(&local.struct_name, field) {
            Some(index) => index,
            None => {
                return Some(Err(KainError::codegen(
                    format!(
                        "Unknown shattered field '{}' on {}",
                        field, local.struct_name
                    ),
                    span,
                )))
            }
        };
        let field_ty = self
            .struct_defs
            .get(&local.struct_name)
            .and_then(|fields| fields.get(field_index))
            .map(|(_, ty)| ty.clone())
            .unwrap_or_else(|| "i64".to_string());
        let (index_value, _) = match self.compile_expr(index) {
            Ok(value) => value,
            Err(error) => return Some(Err(error)),
        };
        if let Some(lane_base) = local.lane_base_values.get(field_index) {
            let index_is_proven_in_bounds =
                self.shattered_index_is_proven_in_bounds(index, local.element_count);
            let can_use_direct_lane_base = index_is_proven_in_bounds
                || matches!(local.backing, ShatteredArrayBacking::StackLaneBuffers);
            if can_use_direct_lane_base {
                let byte_offset =
                    if let Some(literal_offset) = self.shattered_literal_byte_offset(index) {
                        literal_offset.to_string()
                    } else {
                        let scaled = self.next_reg();
                        self.emit(&format!("  {} = shl i64 {}, 3", scaled, index_value));
                        scaled
                    };
                let raw_ptr = self.next_reg();
                let gep_opcode = if index_is_proven_in_bounds {
                    "getelementptr inbounds"
                } else {
                    "getelementptr"
                };
                self.emit(&format!(
                    "  {} = {} i8, i8* {}, i64 {}",
                    raw_ptr, gep_opcode, lane_base, byte_offset
                ));
                let typed_ptr = self.next_reg();
                self.emit(&format!(
                    "  {} = bitcast i8* {} to {}*",
                    typed_ptr, raw_ptr, field_ty
                ));
                return Some(Ok((typed_ptr, field_ty)));
            }
        }

        if matches!(local.backing, ShatteredArrayBacking::StackLaneBuffers) {
            return Some(Err(KainError::codegen(
                format!(
                    "stack-backed shattered local '{}' escaped closed field-projection lowering",
                    array_name
                ),
                span,
            )));
        }

        let (handle_addr, handle_ty) = match self.locals.get(array_name).cloned() {
            Some(local) => local,
            None => {
                return Some(Err(KainError::codegen(
                    format!("Undefined shattered array local: {}", array_name),
                    span,
                )))
            }
        };
        if handle_ty != "i8*" {
            return Some(Err(KainError::codegen(
                format!(
                    "Shattered array '{}' lowered to unexpected LLVM type {}",
                    array_name, handle_ty
                ),
                span,
            )));
        }
        let handle = self.next_reg();
        self.emit(&format!("  {} = load i8*, i8** {}", handle, handle_addr));
        let raw_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = call i8* @kain_machine_shatter_lane_ptr(i8* {}, i64 {}, i64 {})",
            raw_ptr, handle, field_index, index_value
        ));
        let typed_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = bitcast i8* {} to {}*",
            typed_ptr, raw_ptr, field_ty
        ));
        Some(Ok((typed_ptr, field_ty)))
    }

    fn emit_patch_record_i64(&mut self, path: &str, old_value: &str, new_value: &str) {
        if let Some(patch_name) = self.current_patch_name.clone() {
            let patch_name_ptr = self.compile_static_c_string_literal(&patch_name);
            let path_ptr = self.compile_static_c_string_literal(path);
            let status = self.next_reg();
            self.emit(&format!(
                "  {} = call i64 @abi_patch_record_i64(i8* {}, i8* {}, i64 {}, i64 {})",
                status, patch_name_ptr, path_ptr, old_value, new_value
            ));
        }
    }

    fn emit_entangle_i64_propagation(
        &mut self,
        binding: &NativeEntangleBinding,
        propagated_value: &str,
    ) -> KainResult<()> {
        let Some((mirror_world, mirror_field)) = binding.mirror.split_once('.') else {
            return Ok(());
        };
        let Some(world_info) = self.world_globals.get(mirror_world).cloned() else {
            return Ok(());
        };
        let Some(field_index) = self.field_index(mirror_world, mirror_field) else {
            return Ok(());
        };

        self.emit(&format!("  call void @{}()", world_info.init_fn_name));
        let mirror_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = getelementptr inbounds %{}, %{}* {}, i32 0, i32 {}",
            mirror_ptr, mirror_world, mirror_world, world_info.global_symbol, field_index
        ));
        self.emit(&format!(
            "  store i64 {}, i64* {}",
            propagated_value, mirror_ptr
        ));

        let authority = self.compile_static_c_string_literal(&binding.authority);
        let mirror = self.compile_static_c_string_literal(&binding.mirror);
        let status = self.next_reg();
        self.emit(&format!(
            "  {} = call i64 @abi_entangle_record_i64(i8* {}, i8* {}, i64 {})",
            status, authority, mirror, propagated_value
        ));
        Ok(())
    }

    fn compile_temporary_address(&mut self, expr: &Expr) -> KainResult<(String, String)> {
        let (val, ty) = self.compile_expr(expr)?;
        let addr = format!("%tmp.addr.{}", self.reg_count);
        self.reg_count += 1;
        self.emit_entry_alloca(&addr, &ty);
        self.emit(&format!("  store {} {}, {}* {}", ty, val, ty, addr));
        Ok((addr, ty))
    }

    fn compile_index_address_from_compiled(
        &mut self,
        obj_val: &str,
        obj_ty: &str,
        idx_val: &str,
        span: kain_core::Span,
    ) -> KainResult<(String, String)> {
        if let Some(pointee_ty) = obj_ty.strip_suffix('*') {
            let field_ptr = self.next_reg();
            self.emit(&format!(
                "  {} = getelementptr inbounds {}, {} {}, i64 {}",
                field_ptr, pointee_ty, obj_ty, obj_val, idx_val
            ));
            Ok((field_ptr, pointee_ty.to_string()))
        } else if obj_ty == "i64" {
            let base_ptr = self.next_reg();
            self.emit(&format!(
                "  {} = inttoptr i64 {} to i64*",
                base_ptr, obj_val
            ));
            let field_ptr = self.next_reg();
            self.emit(&format!(
                "  {} = getelementptr inbounds i64, i64* {}, i64 {}",
                field_ptr, base_ptr, idx_val
            ));
            Ok((field_ptr, "i64".to_string()))
        } else {
            Err(KainError::codegen(
                format!("Indexing is not supported for LLVM type {}", obj_ty),
                span,
            ))
        }
    }

    fn compile_shattered_array_literal(
        &mut self,
        struct_name: &str,
        items: &[Expr],
        span: Span,
    ) -> KainResult<(String, String)> {
        let fields = self.struct_defs.get(struct_name).cloned().ok_or_else(|| {
            KainError::codegen(format!("Unknown shattered struct: {}", struct_name), span)
        })?;
        let handle = self.next_reg();
        self.emit(&format!(
            "  {} = call i8* @kain_machine_shatter_alloc(i64 {}, i64 {})",
            handle,
            fields.len(),
            items.len()
        ));
        let lane_bases = self.emit_shatter_lane_bases(&handle, fields.len());
        self.populate_shattered_array_literal_lanes(struct_name, &fields, items, &lane_bases)?;

        Ok((handle, "i8*".to_string()))
    }

    fn compile_teleport_expr(
        &mut self,
        value: &Expr,
        source_world: &str,
        target_world: &str,
        channel: Option<&str>,
    ) -> KainResult<(String, String)> {
        let (value_reg, value_ty) = self.compile_expr(value)?;
        let source_ptr = self.compile_static_c_string_literal(source_world);
        let target_ptr = self.compile_static_c_string_literal(target_world);
        let channel_ptr = self.compile_static_c_string_literal(channel.unwrap_or(""));

        if value_ty.ends_with('*') {
            let raw_ptr = if value_ty == "i8*" {
                value_reg.clone()
            } else {
                let casted = self.next_reg();
                self.emit(&format!(
                    "  {} = bitcast {} {} to i8*",
                    casted, value_ty, value_reg
                ));
                casted
            };
            let handed_off = self.next_reg();
            self.emit(&format!(
                "  {} = call i8* @kain_machine_teleport_ptr(i8* {}, i8* {}, i8* {}, i8* {})",
                handed_off, raw_ptr, source_ptr, target_ptr, channel_ptr
            ));
            if value_ty == "i8*" {
                Ok((handed_off, value_ty))
            } else {
                let restored = self.next_reg();
                self.emit(&format!(
                    "  {} = bitcast i8* {} to {}",
                    restored, handed_off, value_ty
                ));
                Ok((restored, value_ty))
            }
        } else {
            self.emit(&format!(
                "  call void @kain_machine_teleport_note(i8* {}, i8* {}, i8* {})",
                source_ptr, target_ptr, channel_ptr
            ));
            Ok((value_reg, value_ty))
        }
    }

    fn compile_addressable_ptr(&mut self, expr: &Expr) -> KainResult<(String, String)> {
        match expr {
            Expr::Ident(name, span) => {
                if let Some((addr, ty)) = self.locals.get(name).cloned() {
                    Ok((addr, ty))
                } else if let Some(info) = self.const_globals.get(name).cloned() {
                    self.emit_const_init_call_if_needed(&info);
                    Ok((info.global_symbol, info.ty))
                } else {
                    Err(KainError::codegen(
                        format!("Undefined variable: {}", name),
                        *span,
                    ))
                }
            }
            Expr::Field {
                object,
                field,
                span,
            } => {
                if let Some(result) = self.compile_shattered_field_ptr(object, field, *span) {
                    return result;
                }
                if let Expr::Ident(name, _) = object.as_ref() {
                    if let Some((addr, obj_ty)) = self.locals.get(name).cloned() {
                        if obj_ty.starts_with('%') && !obj_ty.ends_with('*') {
                            let struct_name = obj_ty[1..].to_string();
                            let field_index =
                                self.field_index(&struct_name, field).ok_or_else(|| {
                                    KainError::codegen(
                                        format!("Unknown field '{}' on {}", field, struct_name),
                                        *span,
                                    )
                                })?;
                            let field_ty = self
                                .struct_defs
                                .get(&struct_name)
                                .and_then(|fields| fields.get(field_index))
                                .map(|(_, ty)| ty.clone())
                                .unwrap_or_else(|| "i64".to_string());
                            let field_ptr = self.next_reg();
                            self.emit(&format!(
                                "  {} = getelementptr inbounds %{}, %{}* {}, i32 0, i32 {}",
                                field_ptr, struct_name, struct_name, addr, field_index
                            ));
                            return Ok((field_ptr, field_ty));
                        }
                    }
                }
                let (obj_val, obj_ty) = self.compile_expr(object)?;
                let (struct_name, struct_ptr, field_index) = if let Some(struct_name) =
                    self.ptr_struct_name(&obj_ty)
                {
                    let index = self.field_index(struct_name, field).ok_or_else(|| {
                        KainError::codegen(
                            format!("Unknown field '{}' on {}", field, struct_name),
                            *span,
                        )
                    })?;
                    (struct_name.to_string(), obj_val, index)
                } else if obj_ty.starts_with('%') {
                    let struct_name = obj_ty[1..].to_string();
                    let index = self.field_index(&struct_name, field).ok_or_else(|| {
                        KainError::codegen(
                            format!("Unknown field '{}' on {}", field, struct_name),
                            *span,
                        )
                    })?;
                    let tmp_addr = self.next_reg();
                    self.emit_entry_alloca(&tmp_addr, &obj_ty);
                    self.emit(&format!(
                        "  store {} {}, {}* {}",
                        obj_ty, obj_val, obj_ty, tmp_addr
                    ));
                    (struct_name, tmp_addr, index)
                } else {
                    return Err(KainError::codegen(
                            format!(
                                "Field address for .{} requires a struct or struct pointer, but LLVM lowered {:?} to {}",
                                field, object, obj_ty
                            ),
                            *span,
                        ));
                };
                let field_ty = self
                    .struct_defs
                    .get(&struct_name)
                    .and_then(|fields| fields.get(field_index))
                    .map(|(_, ty)| ty.clone())
                    .unwrap_or_else(|| "i64".to_string());
                let field_ptr = self.next_reg();
                self.emit(&format!(
                    "  {} = getelementptr inbounds %{}, %{}* {}, i32 0, i32 {}",
                    field_ptr, struct_name, struct_name, struct_ptr, field_index
                ));
                Ok((field_ptr, field_ty))
            }
            Expr::Index {
                object,
                index,
                span,
            } => {
                let (obj_val, obj_ty) = self.compile_expr(object)?;
                let (idx_val, _) = self.compile_expr(index)?;
                if obj_ty == "i8*" {
                    Err(KainError::codegen(
                        "Runtime array indexing is not addressable in LLVM",
                        *span,
                    ))
                } else {
                    self.compile_index_address_from_compiled(&obj_val, &obj_ty, &idx_val, *span)
                }
            }
            _ => self.compile_temporary_address(expr),
        }
    }

    fn compile_lowered_helper_call(
        &mut self,
        func_name: &str,
        args: &[kain_core::ast::CallArg],
        span: kain_core::Span,
    ) -> Option<KainResult<(String, String)>> {
        match func_name {
            "__kain_bind_local" => {
                // Canonical ABI: i8* __kain_bind_local(i8* ptr)
                // Requirements: 1.4, 3.2
                if args.len() != 1 {
                    return Some(Err(KainError::codegen(
                        "__kain_bind_local expects 1 argument",
                        span,
                    )));
                }
                let (addr, ty) = match &args[0].value {
                    Expr::Ident(name, arg_span) => match self.locals.get(name).cloned() {
                        Some(pair) => pair,
                        None => {
                            return Some(Err(KainError::codegen(
                                format!("Undefined variable: {}", name),
                                *arg_span,
                            )))
                        }
                    },
                    other => match self.compile_temporary_address(other) {
                        Ok(pair) => pair,
                        Err(err) => return Some(Err(err)),
                    },
                };
                // Cast typed pointer to i8*
                let ptr_i8 = self.next_reg();
                self.emit(&format!("  {} = bitcast {}* {} to i8*", ptr_i8, ty, addr));
                // Call canonical helper
                let result = self.next_reg();
                self.emit(&format!(
                    "  {} = call i8* @__kain_bind_local(i8* {})",
                    result, ptr_i8
                ));
                // Convert back to i64 for compatibility with existing codegen
                let res = self.next_reg();
                self.emit(&format!("  {} = ptrtoint i8* {} to i64", res, result));
                Some(Ok((res, "i64".to_string())))
            }
            "__kain_addr_of" => {
                // Canonical ABI: i8* __kain_addr_of(i8* ptr, i64 size)
                // Requirements: 1.4, 3.2
                if args.len() < 1 {
                    return Some(Err(KainError::codegen(
                        "__kain_addr_of expects at least 1 argument",
                        span,
                    )));
                }
                let (addr, ty) = match self.compile_addressable_ptr(&args[0].value) {
                    Ok(pair) => pair,
                    Err(err) => return Some(Err(err)),
                };
                // Cast typed pointer to i8*
                let ptr_i8 = self.next_reg();
                self.emit(&format!("  {} = bitcast {}* {} to i8*", ptr_i8, ty, addr));
                // Get size (if provided, otherwise use 8 as default)
                let size = if args.len() > 1 {
                    match self.compile_expr(&args[1].value) {
                        Ok((val, _)) => val,
                        Err(err) => return Some(Err(err)),
                    }
                } else {
                    "8".to_string()
                };
                // Call canonical helper
                let result = self.next_reg();
                self.emit(&format!(
                    "  {} = call i8* @__kain_addr_of(i8* {}, i64 {})",
                    result, ptr_i8, size
                ));
                // Convert to i64
                let res = self.next_reg();
                self.emit(&format!("  {} = ptrtoint i8* {} to i64", res, result));
                Some(Ok((res, "i64".to_string())))
            }
            "__kain_mem_load" => {
                // Canonical ABI: void __kain_mem_load(i8* ptr, i8* out, i64 size)
                // Requirements: 1.4, 3.2
                if args.len() != 1 {
                    return Some(Err(KainError::codegen(
                        "__kain_mem_load expects 1 argument",
                        span,
                    )));
                }
                Some(self.compile_runtime_mem_load(&args[0].value, "i64", span))
            }
            "__kain_mem_store" => {
                // Canonical ABI: void __kain_mem_store(i8* ptr, i8* value, i64 size)
                // Requirements: 1.4, 3.2
                if args.len() != 2 {
                    return Some(Err(KainError::codegen(
                        "__kain_mem_store expects 2 arguments",
                        span,
                    )));
                }
                Some(self.compile_runtime_mem_store(&args[0].value, &args[1].value, span))
            }
            "__kain_field_ptr" => {
                // Canonical ABI: i8* __kain_field_ptr(i8* ptr, const char* field, size_t offset)
                // Requirements: 1.4, 3.2
                if args.len() != 3 {
                    return Some(Err(KainError::codegen(
                        "__kain_field_ptr expects 3 arguments (ptr, field_name, offset)",
                        span,
                    )));
                }
                let compiled_base = match self.compile_expr(&args[0].value) {
                    Ok(pair) => pair,
                    Err(err) => return Some(Err(err)),
                };
                let base_i64 = self.coerce_to_i64_storage(&compiled_base.0, &compiled_base.1);

                // Get field name (for diagnostics, not used in calculation)
                let field_name = match &args[1].value {
                    Expr::String(s, _) => s.clone(),
                    _ => "unknown".to_string(),
                };
                let (field_str, _) = self.compile_string_literal(&field_name);

                let (offset, _) = match self.compile_expr(&args[2].value) {
                    Ok(pair) => pair,
                    Err(err) => return Some(Err(err)),
                };

                // Cast base to i8*
                let base_ptr = self.next_reg();
                self.emit(&format!(
                    "  {} = inttoptr i64 {} to i8*",
                    base_ptr, base_i64
                ));

                // Call canonical helper
                let result = self.next_reg();
                self.emit(&format!(
                    "  {} = call i8* @__kain_field_ptr(i8* {}, i8* {}, i64 {})",
                    result, base_ptr, field_str, offset
                ));

                // Convert to i64
                let res = self.next_reg();
                self.emit(&format!("  {} = ptrtoint i8* {} to i64", res, result));
                Some(Ok((res, "i64".to_string())))
            }
            "__kain_index_ptr" => {
                // Canonical ABI: i8* __kain_index_ptr(i8* ptr, i64 index, i64 stride)
                // Requirements: 1.4, 3.2
                if args.len() != 3 {
                    return Some(Err(KainError::codegen(
                        "__kain_index_ptr expects 3 arguments (ptr, index, stride)",
                        span,
                    )));
                }
                Some(self.compile_raw_ptr_offset_i64(
                    &args[0].value,
                    &args[1].value,
                    &args[2].value,
                ))
            }
            "__kain_ptr_offset" => {
                // Canonical ABI: i8* __kain_ptr_offset(i8* ptr, i64 offset, i64 stride)
                // Requirements: 1.4, 3.2
                if args.len() != 3 {
                    return Some(Err(KainError::codegen(
                        "__kain_ptr_offset expects 3 arguments (ptr, offset, stride)",
                        span,
                    )));
                }
                Some(self.compile_raw_ptr_offset_i64(
                    &args[0].value,
                    &args[1].value,
                    &args[2].value,
                ))
            }
            "__kain_alloc" => {
                // Canonical ABI: i8* __kain_alloc(i64 size, i64 stride, i32 zeroed)
                // Requirements: 1.4, 3.6
                if args.len() != 3 {
                    return Some(Err(KainError::codegen(
                        "__kain_alloc expects 3 arguments (size, stride, zeroed)",
                        span,
                    )));
                }
                let (size, _) = match self.compile_expr(&args[0].value) {
                    Ok(pair) => pair,
                    Err(err) => return Some(Err(err)),
                };
                let (stride, _) = match self.compile_expr(&args[1].value) {
                    Ok(pair) => pair,
                    Err(err) => return Some(Err(err)),
                };
                let (zeroed, _) = match self.compile_expr(&args[2].value) {
                    Ok(pair) => pair,
                    Err(err) => return Some(Err(err)),
                };

                // Call canonical helper
                let result = self.next_reg();
                self.emit(&format!(
                    "  {} = call i8* @__kain_alloc(i64 {}, i64 {}, i32 {})",
                    result, size, stride, zeroed
                ));
                Some(Ok((result, "i8*".to_string())))
            }
            "__kain_realloc" => {
                // Canonical ABI: i8* __kain_realloc(i8* ptr, i64 size, i64 stride, i32 zeroed_new)
                // Requirements: 1.4, 3.6
                if args.len() != 4 {
                    return Some(Err(KainError::codegen(
                        "__kain_realloc expects 4 arguments (ptr, size, stride, zeroed_new)",
                        span,
                    )));
                }
                let (ptr, ptr_ty) = match self.compile_expr(&args[0].value) {
                    Ok(pair) => pair,
                    Err(err) => return Some(Err(err)),
                };
                let (size, _) = match self.compile_expr(&args[1].value) {
                    Ok(pair) => pair,
                    Err(err) => return Some(Err(err)),
                };
                let (stride, _) = match self.compile_expr(&args[2].value) {
                    Ok(pair) => pair,
                    Err(err) => return Some(Err(err)),
                };
                let (zeroed_new, _) = match self.compile_expr(&args[3].value) {
                    Ok(pair) => pair,
                    Err(err) => return Some(Err(err)),
                };

                // Cast to i8*
                let ptr_i8 = if ptr_ty.ends_with('*') {
                    let cast = self.next_reg();
                    self.emit(&format!("  {} = bitcast {} {} to i8*", cast, ptr_ty, ptr));
                    cast
                } else {
                    let ptr_i64 = self.coerce_to_i64_storage(&ptr, &ptr_ty);
                    let cast = self.next_reg();
                    self.emit(&format!("  {} = inttoptr i64 {} to i8*", cast, ptr_i64));
                    cast
                };

                // Call canonical helper
                let result = self.next_reg();
                self.emit(&format!(
                    "  {} = call i8* @__kain_realloc(i8* {}, i64 {}, i64 {}, i32 {})",
                    result, ptr_i8, size, stride, zeroed_new
                ));
                Some(Ok((result, "i8*".to_string())))
            }
            _ => None,
        }
    }

    fn jsx_span(&self, node: &JSXNode) -> kain_core::Span {
        match node {
            JSXNode::Element { span, .. }
            | JSXNode::Text(_, span)
            | JSXNode::ComponentCall { span, .. }
            | JSXNode::For { span, .. }
            | JSXNode::If { span, .. }
            | JSXNode::Fragment(_, span) => *span,
            JSXNode::Expression(expr) => expr.span(),
        }
    }

    fn compile_jsx(&mut self, node: &JSXNode) -> KainResult<(String, String)> {
        match node {
            JSXNode::Text(text, _) => Ok(self.compile_string_literal(text)),
            JSXNode::Expression(expr) => {
                let (val, ty) = self.compile_expr(expr)?;
                self.stringify_value(&val, &ty)
            }
            JSXNode::Fragment(children, _) => {
                let (mut acc, _) = self.compile_string_literal("");
                for child in children {
                    let (child_val, _) = self.compile_jsx(child)?;
                    acc = self.concat_strings(&acc, &child_val);
                }
                Ok((acc, "i8*".to_string()))
            }
            JSXNode::Element {
                tag,
                attributes,
                children,
                ..
            } => {
                let (mut acc, _) = self.compile_string_literal(&format!("<{}", tag));
                for attr in attributes {
                    match &attr.value {
                        JSXAttrValue::String(value) => {
                            let (piece, _) = self
                                .compile_string_literal(&format!(" {}=\"{}\"", attr.name, value));
                            acc = self.concat_strings(&acc, &piece);
                        }
                        JSXAttrValue::Bool(true) => {
                            let (piece, _) =
                                self.compile_string_literal(&format!(" {}", attr.name));
                            acc = self.concat_strings(&acc, &piece);
                        }
                        JSXAttrValue::Bool(false) => {}
                        JSXAttrValue::Expr(expr) => {
                            let (prefix, _) =
                                self.compile_string_literal(&format!(" {}=\"", attr.name));
                            acc = self.concat_strings(&acc, &prefix);
                            let (value, ty) = self.compile_expr(expr)?;
                            let (value_str, _) = self.stringify_value(&value, &ty)?;
                            acc = self.concat_strings(&acc, &value_str);
                            let (suffix, _) = self.compile_string_literal("\"");
                            acc = self.concat_strings(&acc, &suffix);
                        }
                    }
                }
                let (open_end, _) = self.compile_string_literal(">");
                acc = self.concat_strings(&acc, &open_end);
                for child in children {
                    let (child_val, _) = self.compile_jsx(child)?;
                    acc = self.concat_strings(&acc, &child_val);
                }
                let (close, _) = self.compile_string_literal(&format!("</{}>", tag));
                acc = self.concat_strings(&acc, &close);
                Ok((acc, "i8*".to_string()))
            }
            JSXNode::ComponentCall {
                name,
                props,
                children,
                span,
            } => {
                let defs = self.component_defs.get(name).cloned().unwrap_or_default();
                let mut compiled_args = Vec::new();
                let mut arg_types = Vec::new();
                for (prop_name, prop_ty) in defs {
                    if let Some(prop) = props.iter().find(|prop| prop.name == prop_name) {
                        match &prop.value {
                            JSXAttrValue::String(value) => {
                                let (val, ty) = self.compile_string_literal(value);
                                compiled_args.push(val);
                                arg_types.push(if prop_ty == "i8*" {
                                    ty
                                } else {
                                    prop_ty.clone()
                                });
                            }
                            JSXAttrValue::Bool(value) => {
                                compiled_args.push(if *value { "1".into() } else { "0".into() });
                                arg_types.push(prop_ty.clone());
                            }
                            JSXAttrValue::Expr(expr) => {
                                let (val, ty) = self.compile_expr(expr)?;
                                compiled_args.push(val);
                                arg_types.push(ty);
                            }
                        }
                    } else {
                        compiled_args.push(self.zero_value_for_ty(&prop_ty));
                        arg_types.push(prop_ty.clone());
                    }
                }
                let (children_val, children_ty) =
                    self.compile_jsx(&JSXNode::Fragment(children.clone(), *span))?;
                compiled_args.push(children_val);
                arg_types.push(children_ty);
                let res = self.next_reg();
                let arg_str = compiled_args
                    .iter()
                    .zip(arg_types.iter())
                    .map(|(val, ty)| format!("{} {}", ty, val))
                    .collect::<Vec<_>>()
                    .join(", ");
                self.emit(&format!("  {} = call i8* @{}({})", res, name, arg_str));
                Ok((res, "i8*".to_string()))
            }
            JSXNode::For {
                binding,
                iter,
                body,
                ..
            } => {
                let (iter_val, iter_ty) = self.compile_expr(iter)?;
                let _ = binding;
                let _ = body;
                self.stringify_value(&iter_val, &iter_ty)
            }
            JSXNode::If {
                condition,
                then_branch,
                else_branch,
                ..
            } => {
                let then_span = self.jsx_span(then_branch);
                let expr = Expr::If {
                    condition: condition.clone(),
                    then_branch: Block {
                        stmts: vec![Stmt::Expr(Expr::JSX((**then_branch).clone(), then_span))],
                        span: then_span,
                    },
                    else_branch: else_branch.as_ref().map(|branch| {
                        let branch_span = self.jsx_span(branch);
                        Box::new(ElseBranch::Else(Block {
                            stmts: vec![Stmt::Expr(Expr::JSX((**branch).clone(), branch_span))],
                            span: branch_span,
                        }))
                    }),
                    span: then_span,
                };
                self.compile_expr(&expr)
            }
        }
    }

    fn hash_message_tag(&self, actor: &str, msg: &str) -> i64 {
        let s = format!("{}_{}", actor, msg);
        let mut hash: i64 = 5381;
        for c in s.bytes() {
            hash = ((hash << 5).wrapping_add(hash)) ^ (c as i64);
        }
        hash
    }

    fn llvm_type_is_reply_port(&self, ty: &str) -> bool {
        ty == REPLY_PORT_LLVM_TYPE
    }

    fn actor_name_for_handle_type(&self, handle_ty: &str) -> Option<String> {
        if self.llvm_type_is_reply_port(handle_ty) {
            return Some(REPLY_PORT_ACTOR_NAME.to_string());
        }
        if handle_ty.starts_with('%') && handle_ty.ends_with('*') {
            return Some(
                handle_ty
                    .trim_start_matches('%')
                    .trim_end_matches('*')
                    .to_string(),
            );
        }
        None
    }

    fn compile_actor_handle_ref_value(
        &mut self,
        handle_val: &str,
        handle_ty: &str,
        span: Span,
    ) -> KainResult<String> {
        if self.llvm_type_is_reply_port(handle_ty) {
            let actor_ref = self.next_reg();
            self.emit(&format!(
                "  {} = extractvalue {} {}, 0",
                actor_ref, handle_ty, handle_val
            ));
            return Ok(actor_ref);
        }

        let actor_name = self.actor_name_for_handle_type(handle_ty).ok_or_else(|| {
            KainError::codegen(
                format!("Cannot use non-actor type {} as an actor handle", handle_ty),
                span,
            )
        })?;
        let actor_id_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = getelementptr inbounds %{}, {} {}, i32 0, i32 0",
            actor_id_ptr, actor_name, handle_ty, handle_val
        ));
        let actor_ref = self.next_reg();
        self.emit(&format!(
            "  {} = load {}, {}* {}",
            actor_ref, ACTOR_REF_LLVM_TYPE, ACTOR_REF_LLVM_TYPE, actor_id_ptr
        ));
        Ok(actor_ref)
    }

    fn compile_actor_handle_id(
        &mut self,
        handle_val: &str,
        handle_ty: &str,
        span: Span,
    ) -> KainResult<String> {
        let actor_ref = self.compile_actor_handle_ref_value(handle_val, handle_ty, span)?;
        let actor_id = self.next_reg();
        self.emit(&format!(
            "  {} = extractvalue {} {}, 0",
            actor_id, ACTOR_REF_LLVM_TYPE, actor_ref
        ));
        Ok(actor_id)
    }

    fn compile_actor_builtin_ask(
        &mut self,
        builtin_name: &str,
        args: &[kain_core::ast::CallArg],
        span: Span,
        reply_target_ty: Option<&str>,
    ) -> KainResult<Option<(String, String)>> {
        if builtin_name != "ask" && builtin_name != "ask_timeout" {
            return Ok(None);
        }

        let expected_args = if builtin_name == "ask" { 3 } else { 4 };
        if args.len() != expected_args {
            return Err(KainError::codegen(
                format!(
                    "{} expects {} arguments (actor, message, request{})",
                    builtin_name,
                    expected_args,
                    if builtin_name == "ask_timeout" {
                        ", timeout_ms"
                    } else {
                        ""
                    }
                ),
                span,
            ));
        }

        let (target_val, target_ty) = self.compile_expr(&args[0].value)?;
        let actor_name = self.actor_name_for_handle_type(&target_ty).ok_or_else(|| {
            KainError::codegen(
                format!(
                    "{} expects an actor handle as its first argument, got {}",
                    builtin_name, target_ty
                ),
                span,
            )
        })?;
        if actor_name == REPLY_PORT_ACTOR_NAME {
            return Err(KainError::codegen(
                format!("{} cannot target a reply port handle", builtin_name),
                span,
            ));
        }
        let target_ref = self.compile_actor_handle_ref_value(&target_val, &target_ty, span)?;
        let target_ref_ptr = self.next_reg();
        self.emit_entry_alloca(&target_ref_ptr, ACTOR_REF_LLVM_TYPE);
        self.emit(&format!(
            "  store {} {}, {}* {}",
            ACTOR_REF_LLVM_TYPE, target_ref, ACTOR_REF_LLVM_TYPE, target_ref_ptr
        ));

        let message_name = match &args[1].value {
            Expr::String(value, _) => value.clone(),
            _ => {
                return Err(KainError::codegen(
                    format!(
                    "{} currently requires a literal actor message name under the native LLVM lane",
                    builtin_name
                ),
                    span,
                ))
            }
        };

        let request_payload_name = format!("{}_{}", actor_name, message_name);
        let request_fields = self
            .struct_defs
            .get(&request_payload_name)
            .cloned()
            .ok_or_else(|| {
                KainError::codegen(
                    format!(
                        "Cannot lower {} for unknown actor message '{}.{}'",
                        builtin_name, actor_name, message_name
                    ),
                    span,
                )
            })?;

        if request_fields.len() != 2 || request_fields[0].1 != REPLY_PORT_LLVM_TYPE {
            return Err(KainError::codegen(
                format!(
                    "{} currently requires actor message '{}.{}' to start with reply_to: P",
                    builtin_name, actor_name, message_name
                ),
                span,
            ));
        }

        let wait_target_ty = match reply_target_ty {
            Some("void") | None => "i64",
            Some(target_ty) => target_ty,
        };
        let use_i64_wait = wait_target_ty == "i64";
        let zero_reply_value = self.zero_value_for_ty(wait_target_ty);

        let reply_port_handle = self.next_reg();
        self.emit(&format!(
            "  {} = call i8* @kain_actor_reply_port_new()",
            reply_port_handle
        ));
        let reply_port_ref_ptr = self.next_reg();
        self.emit_entry_alloca(&reply_port_ref_ptr, ACTOR_REF_LLVM_TYPE);
        self.emit(&format!(
            "  call void @kain_actor_reply_port_actor_ref(i8* {}, {}* {})",
            reply_port_handle, ACTOR_REF_LLVM_TYPE, reply_port_ref_ptr
        ));
        let reply_port_actor_ref = self.next_reg();
        self.emit(&format!(
            "  {} = load {}, {}* {}",
            reply_port_actor_ref, ACTOR_REF_LLVM_TYPE, ACTOR_REF_LLVM_TYPE, reply_port_ref_ptr
        ));
        let reply_port_value = self.next_reg();
        self.emit(&format!(
            "  {} = insertvalue {} zeroinitializer, {} {}, 0",
            reply_port_value, REPLY_PORT_LLVM_TYPE, ACTOR_REF_LLVM_TYPE, reply_port_actor_ref
        ));

        let request_payload_ty = format!("%{}", request_payload_name);
        let request_payload_ptr_ty = format!("{}*", request_payload_ty);
        let request_payload_ptr = self.next_reg();
        self.emit_entry_alloca(&request_payload_ptr, &request_payload_ty);

        for (index, (_, field_ty)) in request_fields.iter().enumerate() {
            let field_ptr = self.next_reg();
            self.emit(&format!(
                "  {} = getelementptr inbounds {}, {} {}, i32 0, i32 {}",
                field_ptr, request_payload_ty, request_payload_ptr_ty, request_payload_ptr, index
            ));
            if index == 0 {
                self.emit(&format!(
                    "  store {} {}, {}* {}",
                    field_ty, reply_port_value, field_ty, field_ptr
                ));
                continue;
            }
            let (request_value, request_value_ty) =
                self.compile_expr_for_target_type(&args[2].value, field_ty)?;
            self.emit(&format!(
                "  store {} {}, {}* {}",
                request_value_ty, request_value, field_ty, field_ptr
            ));
        }

        let request_payload_i8 = self.next_reg();
        self.emit(&format!(
            "  {} = bitcast {}* {} to i8*",
            request_payload_i8, request_payload_ty, request_payload_ptr
        ));
        let request_payload_size_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = getelementptr {}, {}* null, i32 1",
            request_payload_size_ptr, request_payload_ty, request_payload_ty
        ));
        let request_payload_size = self.next_reg();
        self.emit(&format!(
            "  {} = ptrtoint {}* {} to i64",
            request_payload_size, request_payload_ty, request_payload_size_ptr
        ));

        let outbound_message_ptr = self.next_reg();
        self.emit_entry_alloca(&outbound_message_ptr, "%KainActorMessage");

        let outbound_type_tag_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = getelementptr inbounds %KainActorMessage, %KainActorMessage* {}, i32 0, i32 0",
            outbound_type_tag_ptr, outbound_message_ptr
        ));
        self.emit(&format!(
            "  store i64 {}, i64* {}",
            self.hash_message_tag(&actor_name, &message_name),
            outbound_type_tag_ptr
        ));

        let outbound_data_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = getelementptr inbounds %KainActorMessage, %KainActorMessage* {}, i32 0, i32 1",
            outbound_data_ptr, outbound_message_ptr
        ));
        self.emit(&format!(
            "  store i8* {}, i8** {}",
            request_payload_i8, outbound_data_ptr
        ));

        let outbound_size_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = getelementptr inbounds %KainActorMessage, %KainActorMessage* {}, i32 0, i32 2",
            outbound_size_ptr, outbound_message_ptr
        ));
        self.emit(&format!(
            "  store i64 {}, i64* {}",
            request_payload_size, outbound_size_ptr
        ));

        let outbound_sender_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = getelementptr inbounds %KainActorMessage, %KainActorMessage* {}, i32 0, i32 3",
            outbound_sender_ptr, outbound_message_ptr
        ));
        self.emit(&format!("  store i64 0, i64* {}", outbound_sender_ptr));

        let send_status = self.next_reg();
        self.emit(&format!(
            "  {} = call i32 @kain_actor_ask_send_ref({}* {}, %KainActorMessage* {}, i8* null)",
            send_status, ACTOR_REF_LLVM_TYPE, target_ref_ptr, outbound_message_ptr
        ));

        let timeout_value = if builtin_name == "ask" {
            ABI_DEFAULT_ASK_TIMEOUT_MS_LLVM.to_string()
        } else {
            let (timeout_value, timeout_ty) =
                self.compile_expr_for_target_type(&args[3].value, "i64")?;
            debug_assert_eq!(timeout_ty, "i64");
            timeout_value
        };

        let send_ok = self.next_reg();
        self.emit(&format!("  {} = icmp eq i32 {}, 0", send_ok, send_status));
        let label_wait = self.next_label();
        let label_fail = self.next_label();
        let label_merge = self.next_label();
        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            send_ok, label_wait, label_fail
        ));

        self.emit_label(&label_wait);
        let reply_value = if use_i64_wait {
            let reply_value = self.next_reg();
            self.emit(&format!(
                "  {} = call i64 @kain_actor_reply_port_wait_i64(i8* {}, i64 {})",
                reply_value, reply_port_handle, timeout_value
            ));
            reply_value
        } else {
            let reply_slot = self.next_reg();
            self.emit_entry_alloca(&reply_slot, wait_target_ty);
            self.emit(&format!(
                "  store {} {}, {}* {}",
                wait_target_ty, zero_reply_value, wait_target_ty, reply_slot
            ));
            let reply_slot_i8 = self.next_reg();
            self.emit(&format!(
                "  {} = bitcast {}* {} to i8*",
                reply_slot_i8, wait_target_ty, reply_slot
            ));
            let reply_size_out_ptr = self.next_reg();
            self.emit_entry_alloca(&reply_size_out_ptr, "i64");
            self.emit(&format!("  store i64 0, i64* {}", reply_size_out_ptr));
            let reply_capacity = self.abi_layout_for_ty(wait_target_ty, span)?.0;
            let wait_status = self.next_reg();
            self.emit(&format!(
                "  {} = call i32 @kain_actor_reply_port_wait(i8* {}, i64 {}, i8* {}, i64 {}, i64* {})",
                wait_status,
                reply_port_handle,
                timeout_value,
                reply_slot_i8,
                reply_capacity,
                reply_size_out_ptr
            ));
            let wait_ok = self.next_reg();
            self.emit(&format!("  {} = icmp eq i32 {}, 0", wait_ok, wait_status));
            let label_wait_value = self.next_label();
            let label_wait_timeout = self.next_label();
            let label_wait_merge = self.next_label();
            self.emit(&format!(
                "  br i1 {}, label %{}, label %{}",
                wait_ok, label_wait_value, label_wait_timeout
            ));

            self.emit_label(&label_wait_value);
            let loaded_reply = self.next_reg();
            self.emit(&format!(
                "  {} = load {}, {}* {}",
                loaded_reply, wait_target_ty, wait_target_ty, reply_slot
            ));
            let wait_value_block = self.current_block.clone();
            self.emit(&format!("  br label %{}", label_wait_merge));

            self.emit_label(&label_wait_timeout);
            let wait_timeout_block = self.current_block.clone();
            self.emit(&format!("  br label %{}", label_wait_merge));

            self.emit_label(&label_wait_merge);
            let merged_reply = self.next_reg();
            self.emit(&format!(
                "  {} = phi {} [{}, %{}], [{}, %{}]",
                merged_reply,
                wait_target_ty,
                loaded_reply,
                wait_value_block,
                zero_reply_value,
                wait_timeout_block
            ));
            merged_reply
        };
        let reply_block = self.current_block.clone();
        self.emit(&format!("  br label %{}", label_merge));

        self.emit_label(&label_fail);
        self.emit(&format!(
            "  call void @kain_actor_reply_port_destroy(i8* {})",
            reply_port_handle
        ));
        self.emit(&format!("  br label %{}", label_merge));

        self.emit_label(&label_merge);
        let result = self.next_reg();
        self.emit(&format!(
            "  {} = phi {} [{}, %{}], [{}, %{}]",
            result, wait_target_ty, reply_value, reply_block, zero_reply_value, label_fail
        ));
        Ok(Some((result, wait_target_ty.into())))
    }

    fn callable_signature(
        &self,
        resolved_type: &ResolvedType,
        callable_name: &str,
        span: Span,
    ) -> KainResult<(Vec<ResolvedType>, String)> {
        let ResolvedType::Function { params, ret, .. } = resolved_type else {
            return Err(KainError::codegen(
                format!("{} has non-function type", callable_name),
                span,
            ));
        };

        let mut ret_type = self.map_type(ret);
        if ret_type == "void" && callable_name != "main" {
            ret_type = "i64".to_string();
        }

        Ok((params.clone(), ret_type))
    }

    fn extern_callable_signature(
        &self,
        resolved_type: &ResolvedType,
        callable_name: &str,
        span: Span,
    ) -> KainResult<(Vec<ResolvedType>, String)> {
        let ResolvedType::Function { params, ret, .. } = resolved_type else {
            return Err(KainError::codegen(
                format!("{} has non-function type", callable_name),
                span,
            ));
        };

        Ok((params.clone(), self.map_type(ret)))
    }

    fn ast_param_codegen_types(
        &self,
        params: &[kain_core::ast::Param],
        resolved_params: &[ResolvedType],
    ) -> KainResult<Vec<String>> {
        params
            .iter()
            .enumerate()
            .map(|(index, param)| {
                if matches!(param.ty, Type::Infer(_)) {
                    Ok(resolved_params
                        .get(index)
                        .map(|ty| self.map_type(ty))
                        .unwrap_or_else(|| "i64".to_string()))
                } else {
                    Ok(self.map_type_from_ast(&param.ty))
                }
            })
            .collect()
    }

    fn function_codegen_signature(
        &self,
        func: &TypedFunction,
    ) -> KainResult<(Vec<String>, String)> {
        let is_extern = Self::function_is_extern(func);
        let (resolved_params, mut ret_ty) = if is_extern {
            self.extern_callable_signature(&func.resolved_type, &func.ast.name, func.ast.span)?
        } else {
            self.callable_signature(&func.resolved_type, &func.ast.name, func.ast.span)?
        };
        let param_tys = self.ast_param_codegen_types(&func.ast.params, &resolved_params)?;
        if let Some(return_type) = &func.ast.return_type {
            ret_ty = self.map_type_from_ast(return_type);
        }
        Ok((param_tys, ret_ty))
    }

    fn function_is_extern(func: &TypedFunction) -> bool {
        func.ast.attributes.iter().any(|attr| attr.name == "extern")
    }

    fn register_callable_signature(
        &mut self,
        name: &str,
        resolved_type: &ResolvedType,
        span: Span,
    ) -> KainResult<()> {
        let (params, ret_ty) = self.callable_signature(resolved_type, name, span)?;
        self.functions.insert(name.to_string(), ret_ty);
        self.function_params.insert(
            name.to_string(),
            params
                .into_iter()
                .map(|param| self.map_type(&param))
                .collect(),
        );
        Ok(())
    }

    fn prescan_item_signatures(&mut self, items: &[TypedItem]) -> KainResult<()> {
        for item in items {
            match item {
                TypedItem::Function(func) => {
                    let (params, ret_ty) = self.function_codegen_signature(func)?;
                    let (resolved_params, _) = if Self::function_is_extern(func) {
                        self.extern_callable_signature(
                            &func.resolved_type,
                            &func.ast.name,
                            func.ast.span,
                        )?
                    } else {
                        self.callable_signature(&func.resolved_type, &func.ast.name, func.ast.span)?
                    };
                    self.functions.insert(func.ast.name.clone(), ret_ty);
                    self.function_params.insert(func.ast.name.clone(), params);
                    self.string_function_params.insert(
                        func.ast.name.clone(),
                        func.ast
                            .params
                            .iter()
                            .enumerate()
                            .map(|(index, param)| {
                                (!matches!(param.ty, Type::Infer(_))
                                    && Self::ast_type_is_string(&param.ty))
                                    || (matches!(param.ty, Type::Infer(_))
                                        && resolved_params
                                            .get(index)
                                            .map(Self::resolved_type_is_string)
                                            .unwrap_or(false))
                            })
                            .collect(),
                    );
                    if Self::function_is_extern(func) {
                        self.extern_functions.insert(func.ast.name.clone());
                    } else {
                        self.defined_functions.insert(func.ast.name.clone());
                        if let Some(payload) =
                            Self::extract_zero_arg_immediate_ready_future_payload(func)
                        {
                            self.immediate_ready_future_payloads
                                .insert(func.ast.name.clone(), payload);
                        }
                        if let Some(miss_behavior) =
                            Self::detect_manual_find_substring_function(func)
                        {
                            self.manual_find_substring_functions
                                .insert(func.ast.name.clone(), miss_behavior);
                        }
                    }
                }
                TypedItem::Patch(patch) => {
                    self.register_callable_signature(
                        &patch.ast.name,
                        &patch.resolved_type,
                        patch.ast.span,
                    )?;
                }
                TypedItem::Law(law) => {
                    self.register_callable_signature(
                        &law.ast.name,
                        &law.resolved_type,
                        law.ast.span,
                    )?;
                }
                TypedItem::Converge(converge) => {
                    self.register_callable_signature(
                        &converge.ast.name,
                        &converge.resolved_type,
                        converge.ast.span,
                    )?;
                }
                TypedItem::Orchestrate(orchestrate) => {
                    self.register_callable_signature(
                        &orchestrate.ast.name,
                        &orchestrate.resolved_type,
                        orchestrate.ast.span,
                    )?;
                }
                TypedItem::Impl(imp) => {
                    if let kain_core::ast::Type::Named { name, .. } = &imp.ast.target_type {
                        for method in &imp.ast.methods {
                            let mut ret_ty = method
                                .return_type
                                .as_ref()
                                .map(|ty| self.map_impl_type_from_ast(name, ty))
                                .unwrap_or_else(|| "void".to_string());
                            if ret_ty == "void" {
                                ret_ty = "i64".to_string();
                            }
                            self.functions
                                .insert(format!("{}_{}", name, method.name), ret_ty);
                        }
                    }
                }
                TypedItem::Mod(module) => {
                    self.prescan_item_signatures(&module.items)?;
                }
                _ => {}
            }
        }
        Ok(())
    }

    fn register_type_definitions_recursive(&mut self, items: &[TypedItem]) -> KainResult<()> {
        for item in items {
            match item {
                TypedItem::Struct(s) => {
                    let struct_is_value_aggregate = s
                        .field_types
                        .values()
                        .all(|ty| self.resolved_type_is_scalar_value_aggregate_pod(ty));
                    let mut fields = Vec::new();
                    for field in &s.ast.fields {
                        if let Some(res_ty) = s.field_types.get(&field.name) {
                            fields.push((field.name.clone(), self.map_type(res_ty)));
                        } else {
                            fields.push((field.name.clone(), "i64".into()));
                        }
                    }
                    self.struct_defs.insert(s.ast.name.clone(), fields.clone());
                    if struct_is_value_aggregate {
                        self.value_aggregate_structs.insert(s.ast.name.clone());
                    }

                    let field_types: Vec<String> = fields.iter().map(|(_, t)| t.clone()).collect();
                    self.emit(&format!(
                        "%{} = type {{ {} }}",
                        s.ast.name,
                        field_types.join(", ")
                    ));
                }
                TypedItem::World(world) => {
                    self.register_world_type_and_global(world)?;
                }
                TypedItem::Component(component) => {
                    let mut props = Vec::new();
                    for prop in &component.ast.props {
                        if let Some(res_ty) = component.prop_types.get(&prop.name) {
                            props.push((prop.name.clone(), self.map_type(res_ty)));
                        } else {
                            props.push((prop.name.clone(), self.map_type_from_ast(&prop.ty)));
                        }
                    }
                    props.push(("children".to_string(), "i8*".to_string()));
                    self.component_defs
                        .insert(component.ast.name.clone(), props.clone());
                    self.functions
                        .insert(component.ast.name.clone(), "i8*".to_string());
                }
                TypedItem::Actor(a) => {
                    let mut fields = Vec::new();
                    fields.push(("__actor_ref".to_string(), ACTOR_REF_LLVM_TYPE.into()));

                    for state in &a.ast.state {
                        if let Some(res_ty) = a.state_types.get(&state.name) {
                            fields.push((state.name.clone(), self.map_type(res_ty)));
                        } else {
                            fields.push((state.name.clone(), "i64".into()));
                        }
                    }
                    self.struct_defs.insert(a.ast.name.clone(), fields.clone());

                    let field_types: Vec<String> = fields.iter().map(|(_, t)| t.clone()).collect();
                    self.emit(&format!(
                        "%{} = type {{ {} }}",
                        a.ast.name,
                        field_types.join(", ")
                    ));

                    for handler in &a.ast.handlers {
                        let mut payload_fields = Vec::new();
                        let mut field_defs = Vec::new();
                        for param in &handler.params {
                            let p_ty = self.map_type_from_ast(&param.ty);
                            payload_fields.push(p_ty.clone());
                            field_defs.push((param.name.clone(), p_ty));
                        }
                        let msg_struct_name = format!("{}_{}", a.ast.name, handler.message_type);
                        self.struct_defs.insert(msg_struct_name.clone(), field_defs);
                        self.emit(&format!(
                            "%{} = type {{ {} }}",
                            msg_struct_name,
                            payload_fields.join(", ")
                        ));
                    }
                }
                TypedItem::Enum(e) => {
                    self.emit(&format!("%{} = type {{ i64, i8* }}", e.ast.name));
                    self.struct_defs.insert(
                        e.ast.name.clone(),
                        vec![
                            ("tag".to_string(), "i64".to_string()),
                            ("payload".to_string(), "i8*".to_string()),
                        ],
                    );

                    for (variant_name, payload_types) in &e.variant_payload_types {
                        if !payload_types.is_empty() {
                            let field_types: Vec<String> =
                                payload_types.iter().map(|t| self.map_type(t)).collect();
                            let struct_name = format!("{}_{}", e.ast.name, variant_name);
                            self.emit(&format!(
                                "%{} = type {{ {} }}",
                                struct_name,
                                field_types.join(", ")
                            ));

                            let mut fields = Vec::new();
                            for (i, ty) in field_types.iter().enumerate() {
                                fields.push((format!("_{}", i), ty.clone()));
                            }
                            self.struct_defs.insert(struct_name, fields);
                        }
                    }
                }
                TypedItem::Mod(module) => {
                    self.register_type_definitions_recursive(&module.items)?;
                }
                _ => {}
            }
        }

        Ok(())
    }

    fn compile_typed_items(&mut self, items: &[TypedItem]) -> KainResult<()> {
        for item in items {
            match item {
                TypedItem::Function(func) => self.compile_function(func)?,
                TypedItem::Patch(patch) => self.compile_patch(patch)?,
                TypedItem::Law(law) => self.compile_law(law)?,
                TypedItem::Axiom(axiom) => self.compile_axiom(axiom)?,
                TypedItem::Converge(converge) => self.compile_converge(converge)?,
                TypedItem::Pulse(pulse) => self.compile_pulse(pulse)?,
                TypedItem::World(world) => self.compile_world_initializer(world)?,
                TypedItem::Orchestrate(orchestrate) => self.compile_orchestrate(orchestrate)?,
                TypedItem::Component(component) => self.compile_component(component)?,
                TypedItem::Impl(imp) => self.compile_impl(imp)?,
                TypedItem::Actor(actor) => self.compile_actor(actor)?,
                TypedItem::Const(constant) => self.compile_const_initializer(constant)?,
                TypedItem::Mod(module) => self.compile_typed_items(&module.items)?,
                _ => {}
            }
        }
        Ok(())
    }

    fn collect_native_entanglements(&mut self, items: &[TypedItem]) {
        for item in items {
            match item {
                TypedItem::Entangle(entangle) => {
                    self.native_entanglements.push(NativeEntangleBinding {
                        authority: entangle.ast.left.authored_path(),
                        mirror: entangle.ast.right.authored_path(),
                        policy: entangle.ast.policy.as_str().to_string(),
                        type_name: entangle.endpoint_type_name.clone(),
                    });
                }
                TypedItem::Mod(module) => self.collect_native_entanglements(&module.items),
                _ => {}
            }
        }
    }

    fn collect_machine_stone_metadata(&mut self, items: &[TypedItem]) {
        for item in items {
            match item {
                TypedItem::Axiom(axiom) => {
                    self.native_machine_axioms.push(NativeMachineAxiomInfo {
                        name: axiom.ast.name.clone(),
                    });
                }
                TypedItem::Pulse(pulse) => self.native_pulses.push(NativePulseInfo {
                    name: pulse.ast.name.clone(),
                    token: Self::stable_runtime_hash64(&pulse.ast.name),
                    interval_ns: Self::machine_pulse_duration_ns(&pulse.ast.interval),
                    jitter_ns: pulse
                        .ast
                        .jitter
                        .as_ref()
                        .map(Self::machine_pulse_duration_ns)
                        .unwrap_or(0),
                }),
                TypedItem::Struct(struct_def) if struct_def.ast.is_shattered() => {
                    self.shattered_structs.insert(struct_def.ast.name.clone());
                }
                TypedItem::Mod(module) => self.collect_machine_stone_metadata(&module.items),
                _ => {}
            }
        }
    }

    fn register_world_type_and_global(
        &mut self,
        world: &kain_core::types::TypedWorld,
    ) -> KainResult<()> {
        let mut fields = Vec::new();
        for state in &world.ast.states {
            fields.push((state.name.clone(), self.map_type_from_ast(&state.ty)));
        }

        self.struct_defs
            .insert(world.ast.name.clone(), fields.clone());

        let field_types: Vec<String> = fields.iter().map(|(_, ty)| ty.clone()).collect();
        self.emit(&format!(
            "%{} = type {{ {} }}",
            world.ast.name,
            field_types.join(", ")
        ));

        let global_symbol = format!("@__kain_world_{}", world.ast.name);
        let init_flag_symbol = format!("@__kain_world_init_flag_{}", world.ast.name);
        let init_fn_name = format!("__kain_init_world_{}", world.ast.name);

        self.world_globals.insert(
            world.ast.name.clone(),
            WorldGlobalInfo {
                global_symbol: global_symbol.clone(),
                init_flag_symbol: init_flag_symbol.clone(),
                init_fn_name,
            },
        );

        self.emit(&format!(
            "{} = internal global %{} zeroinitializer",
            global_symbol, world.ast.name
        ));
        self.emit(&format!("{} = internal global i1 0", init_flag_symbol));
        self.emit("");

        Ok(())
    }

    fn llvm_constant_initializer_for_expr(&self, expr: &Expr, ty: &str) -> Option<String> {
        match expr {
            Expr::Int(value, _) if matches!(ty, "i64" | "i32" | "i8") => Some(value.to_string()),
            Expr::Float(value, _) if ty == "double" => Some(format!("{:.6}", value)),
            Expr::Bool(value, _) if ty == "i1" => Some(if *value {
                "1".to_string()
            } else {
                "0".to_string()
            }),
            Expr::None(_) if ty.ends_with('*') => Some("null".to_string()),
            Expr::Paren(inner, _) => self.llvm_constant_initializer_for_expr(inner, ty),
            _ => None,
        }
    }

    fn register_const_global(&mut self, constant: &TypedConst) {
        let name = &constant.ast.name;
        let llvm_ty = self.map_type(&constant.ty);
        let symbol_name = Self::sanitize_symbol_fragment(name);
        let initializer = self.llvm_constant_initializer_for_expr(&constant.ast.value, &llvm_ty);
        let requires_runtime_init = initializer.is_none();
        let string_literal = if Self::resolved_type_is_string(&constant.ty) {
            Self::extract_string_literal(&constant.ast.value)
        } else {
            None
        };

        let info = ConstGlobalInfo {
            global_symbol: format!("@__kain_const_{}", symbol_name),
            init_flag_symbol: format!("@__kain_const_init_flag_{}", symbol_name),
            init_fn_name: format!("__kain_init_const_{}", symbol_name),
            ty: llvm_ty.clone(),
            requires_runtime_init,
            is_known_string: Self::resolved_type_is_string(&constant.ty),
            string_byte_len: string_literal.as_ref().map(|value| value.len()),
            string_literal,
        };

        if let Some(initializer) = initializer {
            self.emit(&format!(
                "{} = internal constant {} {}",
                info.global_symbol, llvm_ty, initializer
            ));
        } else {
            self.emit(&format!(
                "{} = internal global {} zeroinitializer",
                info.global_symbol, llvm_ty
            ));
            self.emit(&format!("{} = internal global i1 0", info.init_flag_symbol));
        }

        self.const_globals.insert(name.clone(), info);
    }

    fn register_const_globals(&mut self, items: &[TypedItem]) {
        if self.register_const_globals_recursive(items) {
            self.emit("");
        }
    }

    fn register_const_globals_recursive(&mut self, items: &[TypedItem]) -> bool {
        let mut saw_const = false;
        for item in items {
            match item {
                TypedItem::Const(constant) => {
                    self.register_const_global(constant);
                    saw_const = true;
                }
                TypedItem::Mod(module) => {
                    saw_const |= self.register_const_globals_recursive(&module.items);
                }
                _ => {}
            }
        }
        saw_const
    }

    fn compile_const_initializer(&mut self, constant: &TypedConst) -> KainResult<()> {
        let Some(info) = self.const_globals.get(&constant.ast.name).cloned() else {
            return Err(KainError::codegen(
                format!("Missing LLVM const registration for {}", constant.ast.name),
                constant.ast.span,
            ));
        };

        if !info.requires_runtime_init {
            return Ok(());
        }

        self.reg_count = 0;
        self.locals.clear();
        self.helper_owned_pointer_locals.clear();
        self.shattered_array_locals.clear();
        self.fixed_array_locals.clear();
        self.sealed_literal_map_locals.clear();
        self.borrowed_locals.clear();
        self.json_handle_locals.clear();
        self.string_locals.clear();
        self.string_length_values.clear();
        self.pooled_string_literal_slots.clear();
        self.scopes.clear();
        self.const_init_blocks.clear();
        self.entry_alloca_insert_offset = None;
        self.entry_preamble_insert_offset = None;
        self.entry_hoisted_const_inits.clear();
        self.current_return_type = Some("void".to_string());

        self.emit(&format!("define void @{}() {{", info.init_fn_name));
        self.emit_label("entry");

        let initialized = self.next_reg();
        self.emit(&format!(
            "  {} = load i1, i1* {}",
            initialized, info.init_flag_symbol
        ));

        let init_block = self.next_label();
        let already_init_block = self.next_label();
        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            initialized, already_init_block, init_block
        ));

        self.emit_label(&init_block);
        let (initial_value, initial_ty) =
            self.compile_expr_for_target_type(&constant.ast.value, &info.ty)?;
        if initial_ty == "i8*" && self.expr_needs_rc_retain(&constant.ast.value) {
            self.emit_rc_retain_if_heap_i8(&initial_value);
        }
        self.emit(&format!(
            "  store {} {}, {}* {}",
            initial_ty, initial_value, info.ty, info.global_symbol
        ));
        self.emit(&format!("  store i1 1, i1* {}", info.init_flag_symbol));
        self.emit(&format!("  br label %{}", already_init_block));

        self.emit_label(&already_init_block);
        self.emit("  ret void");
        self.emit("}");
        self.emit("");
        self.current_return_type = None;
        self.entry_alloca_insert_offset = None;
        Ok(())
    }

    fn emit_const_init_call_if_needed(&mut self, info: &ConstGlobalInfo) {
        if info.requires_runtime_init {
            if self.entry_preamble_insert_offset.is_some() {
                if self
                    .entry_hoisted_const_inits
                    .insert(info.global_symbol.clone())
                {
                    self.emit_entry_preamble_line(&format!("call void @{}()", info.init_fn_name));
                }
                self.const_init_blocks
                    .insert(info.global_symbol.clone(), "__entry_preamble".to_string());
                return;
            }
            if self
                .const_init_blocks
                .get(&info.global_symbol)
                .map(|block| block == &self.current_block)
                .unwrap_or(false)
            {
                return;
            }
            self.emit(&format!("  call void @{}()", info.init_fn_name));
            self.const_init_blocks
                .insert(info.global_symbol.clone(), self.current_block.clone());
        }
    }

    fn compile_const_load(&mut self, info: &ConstGlobalInfo) -> (String, String) {
        self.emit_const_init_call_if_needed(info);
        let loaded = self.next_reg();
        self.emit(&format!(
            "  {} = load {}, {}* {}",
            loaded, info.ty, info.ty, info.global_symbol
        ));
        (loaded, info.ty.clone())
    }

    fn compile_module(&mut self, program: &TypedProgram) -> KainResult<()> {
        // 1. Emit Header
        self.emit("; ModuleID = 'KAIN'");
        self.emit("source_filename = \"KAIN\"");
        self.emit(&format!(
            "target datalayout = \"{}\"",
            self.target.datalayout
        ));
        self.emit(&format!("target triple = \"{}\"", self.target.triple));
        self.emit("");

        self.collect_native_entanglements(&program.items);
        self.collect_machine_stone_metadata(&program.items);
        self.collect_program_tuple_types(program);
        self.register_builtin_tuple_structs();
        self.emit_runtime_abi_types();
        self.struct_defs.insert(
            REPLY_PORT_ACTOR_NAME.to_string(),
            vec![("__actor_ref".to_string(), ACTOR_REF_LLVM_TYPE.to_string())],
        );

        // 2a. Pre-scan Structs and other type definitions, including nested modules.
        self.register_type_definitions_recursive(&program.items)?;

        self.register_const_globals(&program.items);

        // 2b. Pre-scan functions to register return types.
        self.prescan_item_signatures(&program.items)?;

        // 2c. Register StdLib functions
        let stdlib = kain_core::stdlib::StdLib::new();
        for (name, func) in stdlib.functions {
            let ret_ty = self.map_type_from_str(func.return_type);
            self.functions.insert(name, ret_ty);
        }

        // 3. Emit External Declarations (stdlib)
        self.emit_externs();
        self.emit_runtime();
        self.compile_entangle_registration_function();

        // 4. Compile Items
        self.compile_typed_items(&program.items)?;

        // 5. Emit String Constants
        // Clone strings to avoid borrow issues
        let strings: Vec<(String, String)> = self
            .strings
            .iter()
            .map(|(k, v)| (k.clone(), v.clone()))
            .collect();
        for (content, name) in strings {
            let len = content.len() + 1;
            // Escape string content for LLVM (simplified)
            // LLVM expects \xx for hex bytes.
            let mut escaped = String::new();
            for b in content.bytes() {
                if b >= 32 && b < 127 && b != b'"' && b != b'\\' {
                    escaped.push(b as char);
                } else {
                    escaped.push_str(&format!("\\{:02X}", b));
                }
            }
            escaped.push_str("\\00"); // Null terminator

            self.emit(&format!(
                "{} = private unnamed_addr constant [{} x i8] c\"{}\", align 8",
                name, len, escaped
            ));
        }

        // 6. Emit Struct Destructors
        self.emit_struct_destructors();

        Ok(())
    }

    fn compile_actor(&mut self, actor: &kain_core::types::TypedActor) -> KainResult<()> {
        let name = &actor.ast.name;
        let struct_ty = format!("%{}", name);

        self.reg_count = 0;
        self.locals.clear();
        self.helper_owned_pointer_locals.clear();
        self.shattered_array_locals.clear();
        self.fixed_array_locals.clear();
        self.sealed_literal_map_locals.clear();
        self.borrowed_locals.clear();
        self.json_handle_locals.clear();
        self.string_locals.clear();
        self.string_length_values.clear();
        self.pooled_string_literal_slots.clear();
        self.scopes.clear();
        self.const_init_blocks.clear();
        self.entry_alloca_insert_offset = None;
        self.entry_preamble_insert_offset = None;
        self.entry_hoisted_const_inits.clear();
        self.current_return_type = Some("i32".to_string());
        self.actor_return_label = None;
        self.actor_return_slot = None;

        // Generate scheduler-owned microcell turn function.
        self.emit(&format!(
            "define i32 @{}_turn(i64 %actor_id, i8* %mailbox, i8* %user_data, i32 %budget) {{",
            name
        ));
        self.emit_label("entry");

        // Bind the compiler-owned actor state and publish the runtime actor id.
        let self_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = bitcast i8* %user_data to {}*",
            self_ptr, struct_ty
        ));
        let actor_ref_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = getelementptr inbounds {}, {}* {}, i32 0, i32 0",
            actor_ref_ptr, struct_ty, struct_ty, self_ptr
        ));
        self.emit(&format!(
            "  call void @kain_actor_ref_from_id(i64 %actor_id, {}* {})",
            ACTOR_REF_LLVM_TYPE, actor_ref_ptr
        ));
        self.locals
            .insert("self".to_string(), (self_ptr.clone(), struct_ty.clone()));
        self.borrowed_locals.insert("self".to_string());

        // Receive loop setup.
        let message_ptr = self.next_reg();
        self.emit_entry_alloca(&message_ptr, "%KainActorMessage");
        let budget_ptr = self.next_reg();
        self.emit_entry_alloca(&budget_ptr, "i32");
        self.emit(&format!("  store i32 %budget, i32* {}", budget_ptr));
        let label_loop = self.next_label();
        self.emit(&format!("  br label %{}", label_loop));
        self.emit_label(&label_loop);
        let remaining_budget = self.next_reg();
        self.emit(&format!(
            "  {} = load i32, i32* {}",
            remaining_budget, budget_ptr
        ));
        let has_budget = self.next_reg();
        self.emit(&format!(
            "  {} = icmp ugt i32 {}, 0",
            has_budget, remaining_budget
        ));
        let label_try_receive = self.next_label();
        let label_yielded = self.next_label();
        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            has_budget, label_try_receive, label_yielded
        ));
        self.emit_label(&label_yielded);
        self.emit("  ret i32 1");
        self.emit_label(&label_try_receive);
        let receive_status = self.next_reg();
        self.emit(&format!(
            "  {} = call i32 @kain_actor_try_receive(i8* %mailbox, %KainActorMessage* {}, i8* null)",
            receive_status, message_ptr
        ));
        let has_message = self.next_reg();
        self.emit(&format!(
            "  {} = icmp eq i32 {}, 0",
            has_message, receive_status
        ));

        let label_idle = self.next_label();
        let label_dispatch = self.next_label();
        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            has_message, label_dispatch, label_idle
        ));
        self.emit_label(&label_idle);
        self.emit("  ret i32 0");
        self.emit_label(&label_dispatch);

        let message_type_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = getelementptr inbounds %KainActorMessage, %KainActorMessage* {}, i32 0, i32 0",
            message_type_ptr, message_ptr
        ));
        let message_tag = self.next_reg();
        self.emit(&format!(
            "  {} = load i64, i64* {}",
            message_tag, message_type_ptr
        ));

        let message_data_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = getelementptr inbounds %KainActorMessage, %KainActorMessage* {}, i32 0, i32 1",
            message_data_ptr, message_ptr
        ));
        let message_data = self.next_reg();
        self.emit(&format!(
            "  {} = load i8*, i8** {}",
            message_data, message_data_ptr
        ));

        let label_unknown = self.next_label();
        let mut handler_labels = Vec::new();
        for _ in &actor.ast.handlers {
            handler_labels.push(self.next_label());
        }

        let mut switch_cases = String::new();
        for (i, handler) in actor.ast.handlers.iter().enumerate() {
            let tag = self.hash_message_tag(name, &handler.message_type);
            switch_cases.push_str(&format!("i64 {}, label %{} ", tag, handler_labels[i]));
        }
        self.emit(&format!(
            "  switch i64 {}, label %{} [ {} ]",
            message_tag, label_unknown, switch_cases
        ));

        // Unknown messages are dropped after payload cleanup.
        self.emit_label(&label_unknown);
        self.emit(&format!(
            "  call void @kain_actor_message_release(i8* {})",
            message_data
        ));
        let unknown_remaining = self.next_reg();
        self.emit(&format!(
            "  {} = load i32, i32* {}",
            unknown_remaining, budget_ptr
        ));
        let unknown_next = self.next_reg();
        self.emit(&format!(
            "  {} = sub i32 {}, 1",
            unknown_next, unknown_remaining
        ));
        self.emit(&format!(
            "  store i32 {}, i32* {}",
            unknown_next, budget_ptr
        ));
        self.emit(&format!("  br label %{}", label_loop));

        // Generate Handler Bodies
        for (i, handler) in actor.ast.handlers.iter().enumerate() {
            self.emit_label(&handler_labels[i]);

            // Extract payload as the handler-specific message struct.
            let msg_struct_name = format!("{}_{}", name, handler.message_type);
            let msg_struct_ty = format!("%{}", msg_struct_name);
            let payload = self.next_reg();
            self.emit(&format!(
                "  {} = bitcast i8* {} to {}*",
                payload, message_data, msg_struct_ty
            ));

            // Setup scope for handler locals.
            self.scopes.push(Vec::new());
            self.locals.clear();
            self.helper_owned_pointer_locals.clear();
            self.shattered_array_locals.clear();
            self.fixed_array_locals.clear();
            self.sealed_literal_map_locals.clear();
            self.locals
                .insert("self".to_string(), (self_ptr.clone(), struct_ty.clone()));

            let return_slot = self.next_reg();
            self.emit_entry_alloca(&return_slot, "i32");
            self.emit(&format!("  store i32 0, i32* {}", return_slot));
            let handler_return_label = self.next_label();
            self.actor_return_label = Some(handler_return_label.clone());
            self.actor_return_slot = Some(return_slot.clone());

            // Map params.
            for (j, param) in handler.params.iter().enumerate() {
                let p_ty = self.map_type_from_ast(&param.ty);
                let field_ptr = self.next_reg();
                self.emit(&format!(
                    "  {} = getelementptr inbounds {}, {}* {}, i32 0, i32 {}",
                    field_ptr, msg_struct_ty, msg_struct_ty, payload, j
                ));
                let val = self.next_reg();
                self.emit(&format!(
                    "  {} = load {}, {}* {}",
                    val, p_ty, p_ty, field_ptr
                ));

                let addr_reg = format!("%{}.addr", param.name);
                self.emit_entry_alloca(&addr_reg, &p_ty);
                self.emit(&format!("  store {} {}, {}* {}", p_ty, val, p_ty, addr_reg));
                self.locals.insert(param.name.clone(), (addr_reg, p_ty));
                if let Some(scope) = self.scopes.last_mut() {
                    scope.push(param.name.clone());
                }
            }

            // Compile body.
            self.compile_block(&handler.body)?;

            // Normal fallthrough returns to the receive loop.
            self.emit(&format!(
                "  call void @kain_actor_message_release(i8* {})",
                message_data
            ));
            let handler_remaining = self.next_reg();
            self.emit(&format!(
                "  {} = load i32, i32* {}",
                handler_remaining, budget_ptr
            ));
            let handler_next = self.next_reg();
            self.emit(&format!(
                "  {} = sub i32 {}, 1",
                handler_next, handler_remaining
            ));
            self.emit(&format!(
                "  store i32 {}, i32* {}",
                handler_next, budget_ptr
            ));
            self.emit(&format!("  br label %{}", label_loop));

            // Explicit returns from the handler branch here.
            self.emit_label(&handler_return_label);
            self.emit(&format!(
                "  call void @kain_actor_message_release(i8* {})",
                message_data
            ));
            let handler_ret = self.next_reg();
            self.emit(&format!(
                "  {} = load i32, i32* {}",
                handler_ret, return_slot
            ));
            self.emit(&format!("  ret i32 {}", handler_ret));

            self.scopes.pop();
            self.actor_return_label = None;
            self.actor_return_slot = None;
        }

        self.emit("}");
        self.emit("");
        self.current_return_type = None;
        Ok(())
    }

    fn emit_runtime_abi_types(&mut self) {
        self.emit("; Canonical native runtime ABI types");
        self.emit("%KainActorRef = type { i64, i32, i32, i32 }");
        self.emit("%KainActorMessage = type { i64, i8*, i64, i64 }");
        self.emit("%KainReplyPort = type { %KainActorRef }");
        self.emit(&format!(
            "%KainActorSpawnConfig = type {{ i32 (i64, i8*, i8*)*, i8*, i64, i32, i32, i64, i32, [{} x i8], i32, i32 (i64, i8*, i8*, i32)*, i32, i32, i32, i32 }}",
            NATIVE_ACTOR_NAME_MAX_BYTES
        ));
        self.emit("");
    }

    fn emit_runtime(&mut self) {
        // Runtime implemented by the manifest-driven native C bundle under runtime/native.
    }

    fn emit_externs(&mut self) {
        // Core Runtime
        self.emit("declare void @print_i64(i64)");
        self.emit("declare void @print_f64(double)");
        self.emit("declare void @print_bool(i1)");
        self.emit("declare void @print_str(i8*, i64)");
        self.emit("declare i8* @to_string(i64)");
        self.emit("declare i8* @str_concat(i8*, i8*)");
        self.emit("declare i8* @str_concat3(i8*, i8*, i8*)");
        self.emit("declare i8* @str_concat4(i8*, i8*, i8*, i8*)");
        self.emit("declare i8* @str_concat5(i8*, i8*, i8*, i8*, i8*)");
        self.emit("declare i8* @str_concat6(i8*, i8*, i8*, i8*, i8*, i8*)");
        self.emit("declare i8* @str_concat7(i8*, i8*, i8*, i8*, i8*, i8*, i8*)");
        self.emit("declare i8* @str_concat8(i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*)");
        self.emit("declare i8* @str_concat9(i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*)");
        self.emit("declare i8* @str_concat10(i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*)");
        self.emit("declare i64 @clock_wrapper()");
        self.emit("declare i64 @strlen(i8*)");
        self.emit("declare noalias i8* @KAIN_alloc(i64) allocsize(0)");
        self.emit("declare void @rc_retain(i8*)");
        self.emit("declare void @rc_release(i8*)");
        self.emit("declare i8* @string_new(i8*)");
        self.emit("declare i64 @json_box_float(double)");
        self.emit("declare void @json_release(i64)");
        self.emit("declare void @map_set_static_prehashed(i64, i8*, i64, i64, i64, i64)");
        self.emit("declare i64 @map_get_prehashed(i64, i8*, i64, i64, i64)");
        self.emit("declare i64 @find_substring_from_known_lengths(i8*, i64, i8*, i64, i64)");
        self.emit("declare i8* @memchr(i8*, i32, i64)");
        self.emit("declare i32 @memcmp(i8*, i8*, i64)");
        self.emit("declare i8* @array_new(i64)");
        self.emit("declare void @array_push(i8*, i64)");
        self.emit("declare i64 @array_get(i8*, i64)");
        self.emit("declare void @array_set(i8*, i64, i64)");
        self.emit("declare i64 @array_len(i8*)");
        self.emit("declare i8* @abi_option_none()");
        self.emit("declare i8* @abi_option_some(i8*, i64)");
        self.emit("declare i64 @abi_option_is_some(i8*)");
        self.emit("declare i64 @abi_option_is_none(i8*)");
        self.emit("declare i64 @abi_option_payload_copy(i8*, i8*, i64)");
        self.emit("declare i8* @abi_result_ok(i8*, i64)");
        self.emit("declare i8* @abi_result_err(i8*, i64)");
        self.emit("declare i64 @abi_result_is_ok(i8*)");
        self.emit("declare i64 @abi_result_is_err(i8*)");
        self.emit("declare i8* @abi_result_ok_option(i8*)");
        self.emit("declare i64 @abi_result_payload_copy(i8*, i8*, i64)");
        self.emit("declare i64 @abi_tagged_is_success(i8*)");
        self.emit("declare i64 @abi_tagged_matches(i8*, i64)");
        self.emit("declare i64 @abi_tagged_payload_copy(i8*, i8*, i64)");
        self.emit("declare i8* @abi_future_ready_from_value(i8*, i64)");
        self.emit("declare i64 @abi_future_state(i8*)");
        self.emit("declare i64 @abi_future_await_payload_copy(i8*, i8*, i64)");
        self.emit("declare i8* @abi_async_sleep_future(i64)");
        self.emit("declare i64 @abi_patch_begin(i8*)");
        self.emit("declare i64 @abi_patch_record_i64(i8*, i8*, i64, i64)");
        self.emit("declare i64 @abi_patch_commit(i8*)");
        self.emit("declare i64 @abi_entangle_record_i64(i8*, i8*, i64)");
        self.emit("declare i64 @abi_converge_record_i64(i8*, i8*, i64, i64)");
        self.emit("declare i64 @abi_converge_record_bool(i8*, i8*, i32)");
        self.emit("declare i64 @abi_cpu_feature_mask()");
        self.emit("declare i64 @abi_cpu_capability_mask_for_key(i8*)");
        self.emit("declare i64 @abi_converge_select_lane_for_key(i64, i64, i64, i64)");
        self.emit("declare i64 @abi_converge_record_telemetry(i64, i64, i64, i64, i64)");
        self.emit("declare i64 @abi_orchestrate_stage_begin(i8*, i8*)");
        self.emit("declare i64 @abi_orchestrate_stage_end_i64(i8*, i8*, i64)");
        self.emit("declare i64 @kain_machine_axiom_accept(i8*, i8*, i64)");
        self.emit("declare void @kain_machine_pulse_snapshot(i64, i64, i64, i64*, i64*, i64*)");
        self.emit("declare i64 @kain_machine_pulse_start(i64, i64, i64, void ()*)");
        self.emit("declare void @kain_machine_pulse_stop_all()");
        self.emit("declare i64 @kain_machine_pulse_total_fire_count()");
        self.emit("declare i8* @kain_machine_teleport_ptr(i8*, i8*, i8*, i8*)");
        self.emit("declare void @kain_machine_teleport_note(i8*, i8*, i8*)");
        self.emit("declare i8* @kain_machine_shatter_alloc(i64, i64)");
        self.emit("declare i8* @kain_machine_shatter_lane_ptr(i8*, i64, i64)");
        self.emit("declare i8* @kain_machine_shatter_lane_base(i8*, i64)");
        self.emit("declare void @kain_machine_shatter_free(i8*)");

        // Canonical actor runtime ABI
        self.emit("declare void @kain_actor_spawn_config_init(%KainActorSpawnConfig*)");
        self.emit("declare i64 @kain_actor_spawn(%KainActorSpawnConfig*, i8*)");
        self.emit("declare i32 @kain_actor_send(i64, %KainActorMessage*, i8*)");
        self.emit("declare i32 @kain_actor_ask_send_ref(%KainActorRef*, %KainActorMessage*, i8*)");
        self.emit("declare i32 @kain_actor_receive(i8*, %KainActorMessage*, i8*)");
        self.emit("declare i32 @kain_actor_try_receive(i8*, %KainActorMessage*, i8*)");
        self.emit("declare void @kain_actor_message_release(i8*)");
        self.emit("declare void @kain_actor_ref_from_id(i64, %KainActorRef*)");
        self.emit("declare i32 @kain_actor_ref_is_live(%KainActorRef*)");
        self.emit("declare i8* @kain_actor_reply_port_new()");
        self.emit("declare i64 @kain_actor_reply_port_actor_id(i8*)");
        self.emit("declare void @kain_actor_reply_port_actor_ref(i8*, %KainActorRef*)");
        self.emit("declare void @kain_actor_reply_port_destroy(i8*)");
        self.emit("declare i32 @kain_actor_reply_port_send(i64, i8*, i64)");
        self.emit("declare i32 @kain_actor_reply_port_send_ref(%KainActorRef*, i8*, i64)");
        self.emit("declare i32 @kain_actor_reply_port_wait(i8*, i64, i8*, i64, i64*)");
        self.emit("declare i64 @kain_actor_reply_port_wait_i64(i8*, i64)");
        self.emit("declare void @KAIN_set_destructor(i8*, void(i8*)*)");
        self.emit("declare void @free(i8*)");
        self.emit("declare void @abort()");
        self.emit("declare i1 @deep_eq(i8*, i8*)");

        self.emit("");
        self.emit("; Compiler-owned entangle runtime registration");
        self.emit("declare i64 @abi_entangle_register(i8*, i8*, i8*, i8*)");

        // Low-Level Memory Helpers (Canonical ABI)
        // Source: runtime/native/include/memory.h
        // Requirements: 1.4, 3.1, 3.4, 3.5
        self.emit("");
        self.emit("; Low-Level Memory Helper Surface");
        self.emit("; Category 1: Pointer and Address Operations");
        self.emit("declare i8* @__kain_bind_local(i8*)");
        self.emit("declare i8* @__kain_addr_of(i8*, i64)");
        self.emit("declare i8* @__kain_ptr_offset(i8*, i64, i64)");
        self.emit("declare i8* @__kain_field_ptr(i8*, i8*, i64)");
        self.emit("declare i8* @__kain_index_ptr(i8*, i64, i64)");
        self.emit("");
        self.emit("; Category 2: Memory Load/Store Operations");
        self.emit("declare void @__kain_mem_load(i8*, i8*, i64)");
        self.emit("declare void @__kain_mem_store(i8*, i8*, i64)");
        self.emit("");
        self.emit("; Category 3: Allocation Operations");
        self.emit("declare noalias i8* @__kain_alloc(i64, i64, i32) allocsize(0,1)");
        self.emit("declare i8* @__kain_realloc(i8*, i64, i64, i32)");
        self.emit("");
        self.emit("; Category 4: Ownership State Operations");
        self.emit("declare i32 @__kain_ownership_register(i8*, i64, i64)");
        self.emit("declare i32 @__kain_ownership_register_imported(i8*, i64)");
        self.emit("declare i32 @__kain_ownership_ensure_imported(i8*)");
        self.emit("declare i32 @__kain_ownership_update(i8*, i8*, i64)");
        self.emit("declare i32 @__kain_ownership_begin_observe(i8*)");
        self.emit("declare i32 @__kain_ownership_end_observe(i8*)");
        self.emit("declare i32 @__kain_ownership_begin_collapse(i8*)");
        self.emit("declare i32 @__kain_ownership_end_collapse(i8*)");
        self.emit("declare i32 @__kain_ownership_decay(i8*)");
        self.emit("declare i32 @__kain_ownership_begin_observe_helper(i8*)");
        self.emit("declare i32 @__kain_ownership_end_observe_helper(i8*)");
        self.emit("declare i32 @__kain_ownership_begin_collapse_helper(i8*)");
        self.emit("declare i32 @__kain_ownership_end_collapse_helper(i8*)");
        self.emit("declare i32 @__kain_ownership_decay_helper(i8*)");
        self.emit("declare i32 @__kain_ownership_state(i8*)");

        // LLVM math intrinsics used by compiler-owned numeric fast paths.
        self.emit("declare double @llvm.floor.f64(double)");

        // StdLib
        self.emit_stdlib_externs();
        self.emit("");
        self.emit("; Inline hint for tiny hot helpers");
        self.emit("attributes #0 = { alwaysinline }");
    }

    fn emit_stdlib_externs(&mut self) {
        let stdlib = kain_core::stdlib::StdLib::new();
        // Skip functions that conflict with manual runtime declarations or are handled specially
        let skip_list = ["print", "println", "to_string"];

        for (name, func) in stdlib.functions {
            if skip_list.contains(&name.as_str()) || llvm_runtime_declaration_is_preemitted(&name) {
                continue;
            }
            if self.defined_functions.contains(&name) {
                continue;
            }

            let ret_ty = self.map_type_from_str(func.return_type);
            let mut param_tys = Vec::new();
            for (_, p_ty) in func.params {
                param_tys.push(self.map_type_from_str(p_ty));
            }
            let runtime_symbol = runtime_symbol_for_stdlib_function(&name);
            self.emit(&format!(
                "declare {} @{}({})",
                ret_ty,
                runtime_symbol,
                param_tys.join(", ")
            ));
        }
    }

    fn emit_struct_destructors(&mut self) {
        let structs: Vec<(String, Vec<(String, String)>)> = self
            .struct_defs
            .iter()
            .map(|(k, v)| (k.clone(), v.clone()))
            .collect();

        for (name, fields) in structs {
            // Only generate if there are RC fields
            let has_rc_fields = fields
                .iter()
                .any(|(_, ty)| ty == "i8*" || ty.starts_with("%"));
            if !has_rc_fields {
                continue;
            }

            let struct_ty = format!("%{}", name);
            let dtor_name = format!("dtor_{}", name);

            self.emit(&format!("define void @{}(i8* %ptr_void) {{", dtor_name));
            self.emit_label("entry");

            let ptr_typed = self.next_reg();
            self.emit(&format!(
                "  {} = bitcast i8* %ptr_void to {}*",
                ptr_typed, struct_ty
            ));

            for (i, (_, field_ty)) in fields.iter().enumerate() {
                if field_ty == "i8*" || field_ty.starts_with("%") {
                    let field_ptr = self.next_reg();
                    self.emit(&format!(
                        "  {} = getelementptr inbounds {}, {}* {}, i32 0, i32 {}",
                        field_ptr, struct_ty, struct_ty, ptr_typed, i
                    ));
                    let loaded = self.next_reg();
                    self.emit(&format!(
                        "  {} = load {}, {}* {}",
                        loaded, field_ty, field_ty, field_ptr
                    ));

                    self.emit_release(&loaded, field_ty);
                }
            }

            self.emit("  ret void");
            self.emit("}");
        }
    }

    fn compile_component(&mut self, component: &TypedComponent) -> KainResult<()> {
        self.reg_count = 0;
        self.locals.clear();
        self.helper_owned_pointer_locals.clear();
        self.shattered_array_locals.clear();
        self.fixed_array_locals.clear();
        self.sealed_literal_map_locals.clear();
        self.borrowed_locals.clear();
        self.json_handle_locals.clear();
        self.string_locals.clear();
        self.string_length_values.clear();
        self.pooled_string_literal_slots.clear();
        self.scopes.clear();
        self.scopes.push(Vec::new());
        self.const_init_blocks.clear();
        self.entry_alloca_insert_offset = None;
        self.entry_preamble_insert_offset = None;
        self.entry_hoisted_const_inits.clear();

        let name = &component.ast.name;
        let defs = self.component_defs.get(name).cloned().unwrap_or_else(|| {
            let mut props = component
                .ast
                .props
                .iter()
                .map(|prop| {
                    let ty = component
                        .prop_types
                        .get(&prop.name)
                        .map(|ty| self.map_type(ty))
                        .unwrap_or_else(|| self.map_type_from_ast(&prop.ty));
                    (prop.name.clone(), ty)
                })
                .collect::<Vec<_>>();
            props.push(("children".to_string(), "i8*".to_string()));
            props
        });

        let param_str = defs
            .iter()
            .enumerate()
            .map(|(i, (_, ty))| format!("{} %arg{}", ty, i))
            .collect::<Vec<_>>()
            .join(", ");

        self.emit(&format!("define i8* @{}({}) {{", name, param_str));
        self.emit_label("entry");

        for (i, (param_name, param_ty)) in defs.iter().enumerate() {
            let addr_reg = format!("%{}.addr", param_name);
            self.emit_entry_alloca(&addr_reg, param_ty);
            self.emit(&format!(
                "  store {} %arg{}, {}* {}",
                param_ty, i, param_ty, addr_reg
            ));
            self.locals
                .insert(param_name.clone(), (addr_reg, param_ty.clone()));
            if let Some(scope) = self.scopes.last_mut() {
                scope.push(param_name.clone());
            }
        }

        for method in &component.ast.methods {
            let _ = method;
        }

        let (result, _) = self.compile_jsx(&component.ast.body)?;
        self.emit(&format!("  ret i8* {}", result));
        self.emit("}");
        self.emit("");
        self.current_return_type = None;
        Ok(())
    }

    fn compile_impl(&mut self, imp: &kain_core::types::TypedImpl) -> KainResult<()> {
        let target_name = match &imp.ast.target_type {
            kain_core::ast::Type::Named { name, .. } => name.as_str(),
            _ => return Ok(()),
        };

        for method in &imp.ast.methods {
            self.compile_impl_method(target_name, method)?;
        }

        Ok(())
    }

    fn compile_impl_method(
        &mut self,
        target_name: &str,
        method: &kain_core::ast::Function,
    ) -> KainResult<()> {
        self.reg_count = 0;
        self.locals.clear();
        self.helper_owned_pointer_locals.clear();
        self.shattered_array_locals.clear();
        self.fixed_array_locals.clear();
        self.sealed_literal_map_locals.clear();
        self.borrowed_locals.clear();
        self.json_handle_locals.clear();
        self.string_locals.clear();
        self.string_length_values.clear();
        self.pooled_string_literal_slots.clear();
        self.scopes.clear();
        self.scopes.push(Vec::new());
        self.const_init_blocks.clear();
        self.entry_alloca_insert_offset = None;
        self.entry_preamble_insert_offset = None;
        self.entry_hoisted_const_inits.clear();

        let self_ty = format!("%{}*", target_name);
        let mut ret_type = method
            .return_type
            .as_ref()
            .map(|ty| self.map_impl_type_from_ast(target_name, ty))
            .unwrap_or_else(|| "void".to_string());
        if ret_type == "void" {
            ret_type = "i64".to_string();
        }
        self.current_return_type = Some(ret_type.clone());

        let mut params = Vec::new();
        params.push(format!("{} %arg0", self_ty));
        let authored_self_param = Self::impl_method_has_authored_self_param(method);
        for (i, param) in method
            .params
            .iter()
            .skip(if authored_self_param { 1 } else { 0 })
            .enumerate()
        {
            params.push(format!(
                "{} %arg{}",
                self.map_impl_type_from_ast(target_name, &param.ty),
                i + 1
            ));
        }

        let inline_attr = if self.should_force_inline_callable(&method.name, &method.body) {
            " #0"
        } else {
            ""
        };
        self.emit(&format!(
            "define {} @{}_{}({}){} {{",
            ret_type,
            target_name,
            method.name,
            params.join(", "),
            inline_attr
        ));
        self.emit_label("entry");

        let self_addr = "%self.addr".to_string();
        self.emit_entry_alloca(&self_addr, &self_ty);
        self.emit(&format!(
            "  store {} %arg0, {}* {}",
            self_ty, self_ty, self_addr
        ));
        self.locals
            .insert("self".to_string(), (self_addr, self_ty.clone()));
        self.borrowed_locals.insert("self".to_string());
        if let Some(scope) = self.scopes.last_mut() {
            scope.push("self".to_string());
        }
        if authored_self_param {
            let self_name = method
                .params
                .first()
                .map(|param| param.name.as_str())
                .unwrap_or("_self");
            if self_name != "self" {
                self.locals.insert(
                    self_name.to_string(),
                    ("%self.addr".to_string(), self_ty.clone()),
                );
                self.borrowed_locals.insert(self_name.to_string());
                if let Some(scope) = self.scopes.last_mut() {
                    scope.push(self_name.to_string());
                }
            }
        }

        for (i, param) in method
            .params
            .iter()
            .skip(if authored_self_param { 1 } else { 0 })
            .enumerate()
        {
            let p_ty = self.map_impl_type_from_ast(target_name, &param.ty);
            let addr_reg = format!("%{}.addr", param.name);
            self.emit_entry_alloca(&addr_reg, &p_ty);
            self.emit(&format!(
                "  store {} %arg{}, {}* {}",
                p_ty,
                i + 1,
                p_ty,
                addr_reg
            ));
            self.locals
                .insert(param.name.clone(), (addr_reg.clone(), p_ty));
            if Self::ast_type_is_string(&param.ty) {
                self.string_locals.insert(param.name.clone());
                if Self::block_has_loop_that_mentions_identifier(&method.body, &param.name) {
                    self.prime_string_param_length_cache(&param.name, &addr_reg);
                }
            }
            if let Some(scope) = self.scopes.last_mut() {
                scope.push(param.name.clone());
            }
        }

        self.compile_block(&method.body)?;
        self.emit_scope_exit();

        if ret_type == "void" {
            self.emit("  ret void");
        } else {
            self.emit("  unreachable");
        }

        self.emit("}");
        self.emit("");
        self.current_return_type = None;
        Ok(())
    }

    fn compile_entangle_registration_function(&mut self) {
        if self.native_entanglements.is_empty() {
            return;
        }

        self.reg_count = 0;
        self.string_length_values.clear();
        self.entry_alloca_insert_offset = None;
        self.entry_preamble_insert_offset = None;
        self.entry_hoisted_const_inits.clear();
        self.current_return_type = Some("void".to_string());
        self.emit("define void @__kain_register_entanglements() {");
        self.emit_label("entry");

        for binding in self.native_entanglements.clone() {
            self.emit(&format!(
                "  ; entangle {} <-> {} with {}",
                binding.authority, binding.mirror, binding.policy
            ));
            let authority = self.compile_static_c_string_literal(&binding.authority);
            let mirror = self.compile_static_c_string_literal(&binding.mirror);
            let policy = self.compile_static_c_string_literal(&binding.policy);
            let type_name = self.compile_static_c_string_literal(&binding.type_name);
            let status = self.next_reg();
            self.emit(&format!(
                "  {} = call i64 @abi_entangle_register(i8* {}, i8* {}, i8* {}, i8* {})",
                status, authority, mirror, policy, type_name
            ));
        }

        self.emit("  ret void");
        self.emit("}");
        self.emit("");
        self.current_return_type = None;
    }

    fn should_force_inline_callable(&self, callable_name: &str, body: &Block) -> bool {
        callable_name != "main"
            && body.stmts.len() <= 12
            && !body.stmts.iter().any(|stmt| {
                matches!(
                    stmt,
                    Stmt::While { .. } | Stmt::For { .. } | Stmt::Loop { .. } | Stmt::Item(_)
                )
            })
    }

    fn compile_named_callable(
        &mut self,
        callable_name: &str,
        params: &[kain_core::ast::Param],
        explicit_return_type: Option<&Type>,
        body: &Block,
        resolved_type: &ResolvedType,
        span: Span,
    ) -> KainResult<()> {
        self.reg_count = 0;
        self.locals.clear();
        self.helper_owned_pointer_locals.clear();
        self.shattered_array_locals.clear();
        self.fixed_array_locals.clear();
        self.sealed_literal_map_locals.clear();
        self.borrowed_locals.clear();
        self.json_handle_locals.clear();
        self.string_locals.clear();
        self.string_length_values.clear();
        self.pooled_string_literal_slots.clear();
        self.scopes.clear();
        self.scopes.push(Vec::new());
        self.const_init_blocks.clear();
        self.entry_alloca_insert_offset = None;
        self.entry_preamble_insert_offset = None;
        self.entry_hoisted_const_inits.clear();

        let (param_types, mut ret_type) =
            self.callable_signature(resolved_type, callable_name, span)?;
        if let Some(return_type) = explicit_return_type {
            ret_type = self.map_type_from_ast(return_type);
        }
        let borrowed_string_params = self
            .string_function_params
            .get(callable_name)
            .cloned()
            .unwrap_or_default();
        let param_type_strings = self.ast_param_codegen_types(params, &param_types)?;
        self.current_return_type = Some(ret_type.clone());

        let (llvm_name, is_main) = if callable_name == "main" {
            if ret_type == "void" {
                ret_type = "i64".to_string();
            }
            ("main", true)
        } else {
            (callable_name, false)
        };
        let callable_linkage = if is_main { "" } else { " internal" };

        let mut param_str = String::new();
        for (index, _) in params.iter().enumerate() {
            if index > 0 {
                param_str.push_str(", ");
            }
            let param_ty = &param_type_strings[index];
            param_str.push_str(&format!("{} %arg{}", param_ty, index));
        }

        let inline_attr = if self.should_force_inline_callable(callable_name, body) {
            " #0"
        } else {
            ""
        };
        self.emit(&format!(
            "define{} {} @{}({}){} {{",
            callable_linkage, ret_type, llvm_name, param_str, inline_attr
        ));
        self.emit_label("entry");

        if is_main && !self.native_entanglements.is_empty() {
            self.emit("  call void @__kain_register_entanglements()");
        }
        if is_main {
            self.emit_machine_stones_entry_preamble();
        }

        if let Some(patch_name) = self.current_patch_name.clone() {
            let patch_name_ptr = self.compile_static_c_string_literal(&patch_name);
            let status = self.next_reg();
            self.emit(&format!(
                "  {} = call i64 @abi_patch_begin(i8* {})",
                status, patch_name_ptr
            ));
        }

        for (index, param) in params.iter().enumerate() {
            let param_ty = param_type_strings[index].clone();
            let addr_reg = format!("%{}.addr", param.name);
            self.emit_entry_alloca(&addr_reg, &param_ty);
            self.emit(&format!(
                "  store {} %arg{}, {}* {}",
                param_ty, index, param_ty, addr_reg
            ));
            self.locals
                .insert(param.name.clone(), (addr_reg.clone(), param_ty));
            if (!matches!(param.ty, Type::Infer(_)) && Self::ast_type_is_string(&param.ty))
                || (matches!(param.ty, Type::Infer(_))
                    && param_types
                        .get(index)
                        .map(Self::resolved_type_is_string)
                        .unwrap_or(false))
            {
                self.string_locals.insert(param.name.clone());
                if borrowed_string_params.get(index).copied().unwrap_or(false) {
                    self.borrowed_locals.insert(param.name.clone());
                }
                if Self::block_has_loop_that_mentions_identifier(body, &param.name) {
                    self.prime_string_param_length_cache(&param.name, &addr_reg);
                }
            }
            if let Some(scope) = self.scopes.last_mut() {
                scope.push(param.name.clone());
            }
        }

        let body_result = match self.compile_block_with_result(body) {
            Ok(result) => result,
            Err(error) => {
                return Err(match error {
                    KainError::Codegen { message, span } => KainError::codegen(
                        format!("while compiling '{}': {}", callable_name, message),
                        span,
                    ),
                    other => other,
                });
            }
        };
        self.emit_scope_exit();

        let final_result = if let Some((value, value_ty)) = body_result {
            if ret_type == "void" {
                self.emit_release(&value, &value_ty);
                None
            } else {
                Some(self.coerce_compiled_value_to_target_type(value, &value_ty, &ret_type)?)
            }
        } else {
            None
        };

        if let Some(patch_name) = self.current_patch_name.clone() {
            let patch_name_ptr = self.compile_static_c_string_literal(&patch_name);
            let status = self.next_reg();
            self.emit(&format!(
                "  {} = call i64 @abi_patch_commit(i8* {})",
                status, patch_name_ptr
            ));
        }

        if let Some((value, value_ty)) = final_result {
            self.emit(&format!("  ret {} {}", value_ty, value));
            self.emit("}");
            self.emit("");
            self.current_return_type = None;
            return Ok(());
        }

        if ret_type == "void" {
            self.emit("  ret void");
        } else if is_main {
            self.emit("  ret i64 0");
        } else {
            self.emit("  unreachable");
        }

        self.emit("}");
        self.emit("");
        self.current_return_type = None;
        Ok(())
    }

    fn compile_patch(&mut self, patch: &kain_core::types::TypedPatch) -> KainResult<()> {
        let previous_patch = self.current_patch_name.replace(patch.ast.name.clone());
        let result = self.compile_named_callable(
            &patch.ast.name,
            &patch.ast.params,
            patch.ast.return_type.as_ref(),
            &patch.ast.body,
            &patch.resolved_type,
            patch.ast.span,
        );
        self.current_patch_name = previous_patch;
        result
    }

    fn compile_law(&mut self, law: &kain_core::types::TypedLaw) -> KainResult<()> {
        self.compile_named_callable(
            &law.ast.name,
            &law.ast.params,
            Some(&law.ast.return_type),
            &law.ast.body,
            &law.resolved_type,
            law.ast.span,
        )
    }

    fn compile_axiom(&mut self, axiom: &kain_core::types::TypedAxiom) -> KainResult<()> {
        self.reg_count = 0;
        self.locals.clear();
        self.helper_owned_pointer_locals.clear();
        self.shattered_array_locals.clear();
        self.fixed_array_locals.clear();
        self.sealed_literal_map_locals.clear();
        self.borrowed_locals.clear();
        self.json_handle_locals.clear();
        self.string_locals.clear();
        self.string_length_values.clear();
        self.pooled_string_literal_slots.clear();
        self.scopes.clear();
        self.scopes.push(Vec::new());
        self.const_init_blocks.clear();
        self.entry_alloca_insert_offset = None;
        self.entry_preamble_insert_offset = None;
        self.entry_hoisted_const_inits.clear();
        self.current_return_type = Some("i64".to_string());

        let mut target = String::new();
        let mut arch = String::new();
        let mut capability_mask = 0u64;
        for predicate in &axiom.ast.predicates {
            match predicate {
                AxiomPredicate::Target(value) => target = value.clone(),
                AxiomPredicate::Arch(value) => arch = value.clone(),
                AxiomPredicate::Capability(value) => {
                    capability_mask |= Self::machine_axiom_capability_bit(value);
                }
            }
        }

        self.emit(&format!(
            "define i64 @{}() {{",
            Self::machine_axiom_symbol(&axiom.ast.name)
        ));
        self.emit_label("entry");
        let target_ptr = self.compile_static_c_string_literal(&target);
        let arch_ptr = self.compile_static_c_string_literal(&arch);
        let accepted = self.next_reg();
        self.emit(&format!(
            "  {} = call i64 @kain_machine_axiom_accept(i8* {}, i8* {}, i64 {})",
            accepted,
            target_ptr,
            arch_ptr,
            Self::llvm_i64_literal_for_u64(capability_mask)
        ));
        self.emit(&format!("  ret i64 {}", accepted));
        self.emit("}");
        self.emit("");
        self.current_return_type = None;
        Ok(())
    }

    fn compile_pulse(&mut self, pulse: &kain_core::types::TypedPulse) -> KainResult<()> {
        self.reg_count = 0;
        self.locals.clear();
        self.helper_owned_pointer_locals.clear();
        self.shattered_array_locals.clear();
        self.fixed_array_locals.clear();
        self.sealed_literal_map_locals.clear();
        self.borrowed_locals.clear();
        self.json_handle_locals.clear();
        self.string_locals.clear();
        self.string_length_values.clear();
        self.pooled_string_literal_slots.clear();
        self.scopes.clear();
        self.scopes.push(Vec::new());
        self.const_init_blocks.clear();
        self.entry_alloca_insert_offset = None;
        self.entry_preamble_insert_offset = None;
        self.entry_hoisted_const_inits.clear();
        self.current_return_type = Some("void".to_string());

        let body_symbol = Self::machine_pulse_body_symbol(&pulse.ast.name);
        self.emit(&format!(
            "define void @{}(i64 %pulse_tick_arg, i64 %pulse_dt_ms_arg, i64 %pulse_missed_arg) {{",
            body_symbol
        ));
        self.emit_label("entry");
        for (name, arg) in [
            ("pulse_tick", "%pulse_tick_arg"),
            ("pulse_dt_ms", "%pulse_dt_ms_arg"),
            ("pulse_missed", "%pulse_missed_arg"),
        ] {
            let addr = format!("%{}.addr", name);
            self.emit_entry_alloca(&addr, "i64");
            self.emit(&format!("  store i64 {}, i64* {}", arg, addr));
            self.locals
                .insert(name.to_string(), (addr, "i64".to_string()));
            if let Some(scope) = self.scopes.last_mut() {
                scope.push(name.to_string());
            }
        }
        self.compile_block(&pulse.ast.body)?;
        self.emit_scope_exit();
        self.emit("  ret void");
        self.emit("}");
        self.emit("");

        self.reg_count = 0;
        self.locals.clear();
        self.helper_owned_pointer_locals.clear();
        self.shattered_array_locals.clear();
        self.fixed_array_locals.clear();
        self.sealed_literal_map_locals.clear();
        self.borrowed_locals.clear();
        self.json_handle_locals.clear();
        self.string_locals.clear();
        self.string_length_values.clear();
        self.scopes.clear();
        self.scopes.push(Vec::new());
        self.const_init_blocks.clear();
        self.entry_alloca_insert_offset = None;
        self.entry_preamble_insert_offset = None;
        self.entry_hoisted_const_inits.clear();
        self.current_return_type = Some("void".to_string());

        let fire_symbol = Self::machine_pulse_fire_symbol(&pulse.ast.name);
        let token = Self::stable_runtime_hash64(&pulse.ast.name);
        let interval_ns = Self::machine_pulse_duration_ns(&pulse.ast.interval);
        let jitter_ns = pulse
            .ast
            .jitter
            .as_ref()
            .map(Self::machine_pulse_duration_ns)
            .unwrap_or(0);
        self.emit(&format!("define void @{}() {{", fire_symbol));
        self.emit_label("entry");
        let tick_ptr = "%pulse.tick.out";
        let dt_ptr = "%pulse.dt.out";
        let missed_ptr = "%pulse.missed.out";
        self.emit_entry_alloca(tick_ptr, "i64");
        self.emit_entry_alloca(dt_ptr, "i64");
        self.emit_entry_alloca(missed_ptr, "i64");
        self.emit(&format!(
            "  call void @kain_machine_pulse_snapshot(i64 {}, i64 {}, i64 {}, i64* {}, i64* {}, i64* {})",
            Self::llvm_i64_literal_for_u64(token),
            interval_ns,
            jitter_ns,
            tick_ptr,
            dt_ptr,
            missed_ptr
        ));
        let tick = self.next_reg();
        let dt = self.next_reg();
        let missed = self.next_reg();
        self.emit(&format!("  {} = load i64, i64* {}", tick, tick_ptr));
        self.emit(&format!("  {} = load i64, i64* {}", dt, dt_ptr));
        self.emit(&format!("  {} = load i64, i64* {}", missed, missed_ptr));
        self.emit(&format!(
            "  call void @{}(i64 {}, i64 {}, i64 {})",
            body_symbol, tick, dt, missed
        ));
        self.emit("  ret void");
        self.emit("}");
        self.emit("");
        self.current_return_type = None;
        Ok(())
    }

    fn compile_converge(&mut self, converge: &kain_core::types::TypedConverge) -> KainResult<()> {
        if converge.ast.fast_lanes.is_empty() {
            return self.compile_named_callable(
                &converge.ast.name,
                &converge.ast.params,
                converge.ast.return_type.as_ref(),
                &converge.ast.spec_lane.body,
                &converge.resolved_type,
                converge.ast.span,
            );
        };
        if converge.ast.fast_lanes.len() > KAIN_CONVERGE_LANE_MAX_LLVM {
            return Err(KainError::codegen(
                format!(
                    "converge '{}' has {} fast lanes; LLVM currently supports at most {} selector lanes",
                    converge.ast.name,
                    converge.ast.fast_lanes.len(),
                    KAIN_CONVERGE_LANE_MAX_LLVM
                ),
                converge.ast.span,
            ));
        }

        let spec_name = format!("{}__spec", converge.ast.name);
        let mut fast_names = Vec::new();
        let mut used_fast_names = HashSet::new();
        for (index, lane) in converge.ast.fast_lanes.iter().enumerate() {
            let fragment = Self::sanitize_type_fragment(&lane.lane_name);
            let mut fast_name = format!("{}__fast_{}", converge.ast.name, fragment);
            if !used_fast_names.insert(fast_name.clone()) {
                fast_name = format!("{}__fast_{}_{}", converge.ast.name, index, fragment);
                used_fast_names.insert(fast_name.clone());
            }
            fast_names.push(fast_name);
        }

        let cached_lane_global = format!(
            "@__kain_converge_cached_lane_{}",
            Self::sanitize_symbol_fragment(&converge.ast.name)
        );
        self.emit(&format!("{} = internal global i64 -2", cached_lane_global));
        self.emit("");

        self.compile_named_callable(
            &spec_name,
            &converge.ast.params,
            converge.ast.return_type.as_ref(),
            &converge.ast.spec_lane.body,
            &converge.resolved_type,
            converge.ast.span,
        )?;
        for (lane, fast_name) in converge.ast.fast_lanes.iter().zip(fast_names.iter()) {
            self.compile_named_callable(
                fast_name,
                &converge.ast.params,
                converge.ast.return_type.as_ref(),
                &lane.body,
                &converge.resolved_type,
                converge.ast.span,
            )?;
        }

        self.reg_count = 0;
        self.locals.clear();
        self.helper_owned_pointer_locals.clear();
        self.shattered_array_locals.clear();
        self.fixed_array_locals.clear();
        self.sealed_literal_map_locals.clear();
        self.borrowed_locals.clear();
        self.json_handle_locals.clear();
        self.string_locals.clear();
        self.string_length_values.clear();
        self.scopes.clear();
        self.scopes.push(Vec::new());
        self.const_init_blocks.clear();
        self.entry_alloca_insert_offset = None;
        self.entry_preamble_insert_offset = None;
        self.entry_hoisted_const_inits.clear();

        let (param_types, mut ret_type) = self.callable_signature(
            &converge.resolved_type,
            &converge.ast.name,
            converge.ast.span,
        )?;
        if let Some(return_type) = &converge.ast.return_type {
            ret_type = self.map_type_from_ast(return_type);
        }
        self.current_return_type = Some(ret_type.clone());
        let param_type_strings =
            self.ast_param_codegen_types(&converge.ast.params, &param_types)?;
        let params = param_type_strings
            .iter()
            .enumerate()
            .map(|(index, ty)| format!("{} %arg{}", ty, index))
            .collect::<Vec<_>>()
            .join(", ");
        let args = param_type_strings
            .iter()
            .enumerate()
            .map(|(index, ty)| format!("{} %arg{}", ty, index))
            .collect::<Vec<_>>()
            .join(", ");

        let mut static_eligible_mask = 0u64;
        let mut dynamic_cpu_lanes = Vec::new();
        for (index, lane) in converge.ast.fast_lanes.iter().enumerate() {
            let lane_bit = 1u64 << index;
            match Self::converge_selector_static_eligibility(lane.selector.as_ref()) {
                Some(true) => static_eligible_mask |= lane_bit,
                Some(false) => {}
                None => {
                    if let Some(ConvergeSelector::Capability(capability)) = lane.selector.as_ref() {
                        dynamic_cpu_lanes.push((index, capability.clone()));
                    }
                }
            }
        }

        self.emit(&format!(
            "define {} @{}({}) #0 {{",
            ret_type, converge.ast.name, params
        ));
        self.emit_label("entry");

        if dynamic_cpu_lanes.is_empty() {
            let selected_name = if static_eligible_mask == 0 {
                &spec_name
            } else {
                let selected_index = static_eligible_mask.trailing_zeros() as usize;
                &fast_names[selected_index]
            };
            if ret_type == "void" {
                self.emit(&format!("  call void @{}({})", selected_name, args));
                self.emit("  ret void");
            } else {
                let value = self.next_reg();
                self.emit(&format!(
                    "  {} = call {} @{}({})",
                    value, ret_type, selected_name, args
                ));
                self.emit(&format!("  ret {} {}", ret_type, value));
            }
            self.emit("}");
            self.emit("");
            self.current_return_type = None;
            return Ok(());
        }

        let cached_lane = self.next_reg();
        self.emit(&format!(
            "  {} = load i64, i64* {}",
            cached_lane, cached_lane_global
        ));
        let cached_valid = self.next_reg();
        self.emit(&format!(
            "  {} = icmp ne i64 {}, -2",
            cached_valid, cached_lane
        ));
        let tune_block = self.next_label();
        let dispatch_block = self.next_label();
        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            cached_valid, dispatch_block, tune_block
        ));

        self.emit_label(&tune_block);

        let mut eligible_mask = static_eligible_mask.to_string();
        for (index, capability) in dynamic_cpu_lanes {
            let capability_key = self.compile_static_c_string_literal(&capability);
            let required_mask = self.next_reg();
            self.emit(&format!(
                "  {} = call i64 @abi_cpu_capability_mask_for_key(i8* {})",
                required_mask, capability_key
            ));
            let cpu_mask = self.next_reg();
            self.emit(&format!(
                "  {} = call i64 @abi_cpu_feature_mask()",
                cpu_mask
            ));
            let present_mask = self.next_reg();
            self.emit(&format!(
                "  {} = and i64 {}, {}",
                present_mask, cpu_mask, required_mask
            ));
            let has_required = self.next_reg();
            self.emit(&format!(
                "  {} = icmp eq i64 {}, {}",
                has_required, present_mask, required_mask
            ));
            let required_nonzero = self.next_reg();
            self.emit(&format!(
                "  {} = icmp ne i64 {}, 0",
                required_nonzero, required_mask
            ));
            let eligible_bool = self.next_reg();
            self.emit(&format!(
                "  {} = and i1 {}, {}",
                eligible_bool, has_required, required_nonzero
            ));
            let lane_mask = self.next_reg();
            self.emit(&format!(
                "  {} = select i1 {}, i64 {}, i64 0",
                lane_mask,
                eligible_bool,
                1u64 << index
            ));
            let merged_mask = self.next_reg();
            self.emit(&format!(
                "  {} = or i64 {}, {}",
                merged_mask, eligible_mask, lane_mask
            ));
            eligible_mask = merged_mask;
        }

        let converge_key =
            Self::llvm_i64_literal_for_u64(Self::stable_runtime_hash64(&converge.ast.name));
        let selected_lane = self.next_reg();
        self.emit(&format!(
            "  {} = call i64 @abi_converge_select_lane_for_key(i64 {}, i64 0, i64 {}, i64 -1)",
            selected_lane, converge_key, eligible_mask
        ));
        self.emit(&format!(
            "  store i64 {}, i64* {}",
            selected_lane, cached_lane_global
        ));
        self.emit(&format!("  br label %{}", dispatch_block));

        let spec_block = self.next_label();
        let merge_block = self.next_label();
        let fast_blocks = (0..fast_names.len())
            .map(|_| self.next_label())
            .collect::<Vec<_>>();
        let switch_cases = fast_blocks
            .iter()
            .enumerate()
            .map(|(index, label)| format!("i64 {}, label %{}", index, label))
            .collect::<Vec<_>>()
            .join(" ");
        self.emit_label(&dispatch_block);
        let dispatch_lane = self.next_reg();
        self.emit(&format!(
            "  {} = phi i64 [{}, %entry], [{}, %{}]",
            dispatch_lane, cached_lane, selected_lane, tune_block
        ));
        self.emit(&format!(
            "  switch i64 {}, label %{} [ {} ]",
            dispatch_lane, spec_block, switch_cases
        ));

        let mut incoming_values = Vec::new();
        for (index, fast_name) in fast_names.iter().enumerate() {
            let fast_block = &fast_blocks[index];
            self.emit_label(fast_block);
            if ret_type == "void" {
                self.emit(&format!("  call void @{}({})", fast_name, args));
                self.emit(&format!("  br label %{}", merge_block));
            } else {
                let value = self.next_reg();
                self.emit(&format!(
                    "  {} = call {} @{}({})",
                    value, ret_type, fast_name, args
                ));
                self.emit(&format!("  br label %{}", merge_block));
                incoming_values.push((value, fast_block.clone()));
            }
        }

        self.emit_label(&spec_block);
        if ret_type == "void" {
            self.emit(&format!("  call void @{}({})", spec_name, args));
            self.emit(&format!("  br label %{}", merge_block));
        } else {
            let spec_value = self.next_reg();
            self.emit(&format!(
                "  {} = call {} @{}({})",
                spec_value, ret_type, spec_name, args
            ));
            self.emit(&format!("  br label %{}", merge_block));
            incoming_values.push((spec_value, spec_block.clone()));
        }

        self.emit_label(&merge_block);
        if ret_type == "void" {
            self.emit("  ret void");
        } else {
            let selected_value = self.next_reg();
            let phi_values = incoming_values
                .iter()
                .map(|(value, block)| format!("[{}, %{}]", value, block))
                .collect::<Vec<_>>()
                .join(", ");
            self.emit(&format!(
                "  {} = phi {} {}",
                selected_value, ret_type, phi_values
            ));
            self.emit(&format!("  ret {} {}", ret_type, selected_value));
        }
        self.emit("}");
        self.emit("");
        self.current_return_type = None;
        Ok(())
    }

    fn compile_world_initializer(
        &mut self,
        world: &kain_core::types::TypedWorld,
    ) -> KainResult<()> {
        self.reg_count = 0;
        self.locals.clear();
        self.helper_owned_pointer_locals.clear();
        self.shattered_array_locals.clear();
        self.fixed_array_locals.clear();
        self.sealed_literal_map_locals.clear();
        self.borrowed_locals.clear();
        self.json_handle_locals.clear();
        self.string_locals.clear();
        self.string_length_values.clear();
        self.scopes.clear();
        self.const_init_blocks.clear();
        self.entry_alloca_insert_offset = None;
        self.entry_preamble_insert_offset = None;
        self.entry_hoisted_const_inits.clear();
        self.current_return_type = Some("void".to_string());

        let Some(world_info) = self.world_globals.get(&world.ast.name).cloned() else {
            return Err(KainError::codegen(
                format!("Missing LLVM world registration for {}", world.ast.name),
                world.ast.span,
            ));
        };

        self.emit(&format!("define void @{}() {{", world_info.init_fn_name));
        self.emit_label("entry");

        let init_loaded = self.next_reg();
        self.emit(&format!(
            "  {} = load i1, i1* {}",
            init_loaded, world_info.init_flag_symbol
        ));

        let init_block = self.next_label();
        let already_init_block = self.next_label();
        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            init_loaded, already_init_block, init_block
        ));

        self.emit_label(&init_block);
        let world_ptr_type = format!("%{}*", world.ast.name);
        for (index, state) in world.ast.states.iter().enumerate() {
            let field_ty = self.map_type_from_ast(&state.ty);
            let field_ptr = self.next_reg();
            self.emit(&format!(
                "  {} = getelementptr inbounds %{}, {} {}, i32 0, i32 {}",
                field_ptr, world.ast.name, world_ptr_type, world_info.global_symbol, index
            ));
            let (initial_value, initial_ty) =
                self.compile_expr_for_target_type(&state.initial, &field_ty)?;
            self.emit(&format!(
                "  store {} {}, {}* {}",
                initial_ty, initial_value, field_ty, field_ptr
            ));
        }
        self.emit(&format!(
            "  store i1 1, i1* {}",
            world_info.init_flag_symbol
        ));
        self.emit(&format!("  br label %{}", already_init_block));

        self.emit_label(&already_init_block);
        self.emit("  ret void");
        self.emit("}");
        self.emit("");
        self.current_return_type = None;
        Ok(())
    }

    fn compile_orchestrate(
        &mut self,
        orchestrate: &kain_core::types::TypedOrchestrate,
    ) -> KainResult<()> {
        self.compile_named_callable(
            &orchestrate.ast.name,
            &orchestrate.ast.params,
            orchestrate.ast.return_type.as_ref(),
            &orchestrate.ast.body,
            &orchestrate.resolved_type,
            orchestrate.ast.span,
        )
    }

    fn emit_machine_stones_entry_preamble(&mut self) {
        let axioms = self.native_machine_axioms.clone();
        for axiom in axioms {
            let status = self.next_reg();
            self.emit(&format!(
                "  {} = call i64 @{}()",
                status,
                Self::machine_axiom_symbol(&axiom.name)
            ));
        }

        let pulses = self.native_pulses.clone();
        for pulse in pulses {
            let status = self.next_reg();
            self.emit(&format!(
                "  {} = call i64 @kain_machine_pulse_start(i64 {}, i64 {}, i64 {}, void ()* @{})",
                status,
                Self::llvm_i64_literal_for_u64(pulse.token),
                pulse.interval_ns,
                pulse.jitter_ns,
                Self::machine_pulse_fire_symbol(&pulse.name)
            ));
        }
    }

    fn compile_function(&mut self, func: &TypedFunction) -> KainResult<()> {
        if Self::function_is_extern(func) {
            return self.compile_extern_function(func);
        }
        self.compile_named_callable(
            &func.ast.name,
            &func.ast.params,
            func.ast.return_type.as_ref(),
            &func.ast.body,
            &func.resolved_type,
            func.ast.span,
        )
    }

    fn compile_extern_function(&mut self, func: &TypedFunction) -> KainResult<()> {
        let (param_types, ret_type) = self.function_codegen_signature(func)?;

        let mut param_str = String::new();
        let mut emitted_index = 0usize;
        for (index, _) in func.ast.params.iter().enumerate() {
            let param_ty = &param_types[index];
            if param_ty == "void" {
                continue;
            }
            if emitted_index > 0 {
                param_str.push_str(", ");
            }
            param_str.push_str(&format!("{} %arg{}", param_ty, emitted_index));
            emitted_index += 1;
        }

        self.extern_functions.insert(func.ast.name.clone());
        self.functions
            .insert(func.ast.name.clone(), ret_type.clone());

        if llvm_runtime_declaration_is_preemitted(&func.ast.name) {
            return Ok(());
        }

        self.emit(&format!(
            "declare {} @{}({})",
            ret_type, func.ast.name, param_str
        ));
        self.emit("");
        Ok(())
    }

    fn compile_block(&mut self, block: &Block) -> KainResult<()> {
        self.scopes.push(Vec::new());
        let inherited_known_i64_bindings = self.current_known_i64_literals();
        let inherited_known_llvm_types = self.current_known_llvm_types();
        self.ephemeral_candidate_scopes
            .push(Self::collect_block_ephemeral_candidate_names(
                block,
                &inherited_known_i64_bindings,
            ));
        self.ephemeral_zero_init_elision_scopes.push(
            self.collect_block_ephemeral_zero_init_elision_names(
                block,
                &inherited_known_i64_bindings,
                &inherited_known_llvm_types,
            ),
        );
        self.fixed_array_candidate_scopes
            .push(Self::collect_block_fixed_array_candidate_names(block));
        self.stack_shatter_candidate_scopes
            .push(self.collect_block_stack_shatter_candidate_names(block));
        self.literal_map_candidate_scopes
            .push(Self::collect_block_literal_map_candidate_names(block));
        self.known_i64_literal_scopes.push(HashMap::new());
        self.known_nonnegative_i64_scopes.push(HashSet::new());
        self.forwarded_mem_slot_scopes.push(HashMap::new());
        for (index, stmt) in block.stmts.iter().enumerate() {
            if let Err(err) = self.compile_stmt(stmt, &block.stmts[index + 1..]) {
                self.forwarded_mem_slot_scopes.pop();
                self.known_nonnegative_i64_scopes.pop();
                self.known_i64_literal_scopes.pop();
                self.literal_map_candidate_scopes.pop();
                self.stack_shatter_candidate_scopes.pop();
                self.fixed_array_candidate_scopes.pop();
                self.ephemeral_zero_init_elision_scopes.pop();
                self.ephemeral_candidate_scopes.pop();
                return Err(err);
            }
            self.record_stmt_literal_map_effects(stmt);
            self.record_stmt_i64_literal_effects(stmt);
            self.record_stmt_nonnegative_i64_effects(stmt);
            if !Self::stmt_preserves_forwarded_mem_slots(stmt) {
                self.clear_current_forwarded_mem_slots();
            }
        }
        self.emit_scope_exit();
        self.forwarded_mem_slot_scopes.pop();
        self.known_nonnegative_i64_scopes.pop();
        self.known_i64_literal_scopes.pop();
        self.literal_map_candidate_scopes.pop();
        self.stack_shatter_candidate_scopes.pop();
        self.fixed_array_candidate_scopes.pop();
        self.ephemeral_zero_init_elision_scopes.pop();
        self.ephemeral_candidate_scopes.pop();
        Ok(())
    }

    fn compile_block_with_result(&mut self, block: &Block) -> KainResult<Option<(String, String)>> {
        self.scopes.push(Vec::new());
        let inherited_known_i64_bindings = self.current_known_i64_literals();
        let inherited_known_llvm_types = self.current_known_llvm_types();
        self.ephemeral_candidate_scopes
            .push(Self::collect_block_ephemeral_candidate_names(
                block,
                &inherited_known_i64_bindings,
            ));
        self.ephemeral_zero_init_elision_scopes.push(
            self.collect_block_ephemeral_zero_init_elision_names(
                block,
                &inherited_known_i64_bindings,
                &inherited_known_llvm_types,
            ),
        );
        self.fixed_array_candidate_scopes
            .push(Self::collect_block_fixed_array_candidate_names(block));
        self.stack_shatter_candidate_scopes
            .push(self.collect_block_stack_shatter_candidate_names(block));
        self.literal_map_candidate_scopes
            .push(Self::collect_block_literal_map_candidate_names(block));
        self.known_i64_literal_scopes.push(HashMap::new());
        self.known_nonnegative_i64_scopes.push(HashSet::new());
        self.forwarded_mem_slot_scopes.push(HashMap::new());
        let mut last_res = None;
        let mut last_is_new = false;

        for (i, stmt) in block.stmts.iter().enumerate() {
            if i == block.stmts.len() - 1 {
                if let Stmt::Expr(expr) = stmt {
                    let (val, ty) = self.compile_expr(expr)?;
                    last_res = Some((val, ty));
                    last_is_new = self.is_new_object(expr);
                    self.record_stmt_literal_map_effects(stmt);
                    self.record_stmt_i64_literal_effects(stmt);
                    self.record_stmt_nonnegative_i64_effects(stmt);
                    if !Self::stmt_preserves_forwarded_mem_slots(stmt) {
                        self.clear_current_forwarded_mem_slots();
                    }
                } else {
                    if let Err(err) = self.compile_stmt(stmt, &block.stmts[i + 1..]) {
                        self.forwarded_mem_slot_scopes.pop();
                        self.known_nonnegative_i64_scopes.pop();
                        self.known_i64_literal_scopes.pop();
                        self.literal_map_candidate_scopes.pop();
                        self.stack_shatter_candidate_scopes.pop();
                        self.fixed_array_candidate_scopes.pop();
                        self.ephemeral_zero_init_elision_scopes.pop();
                        self.ephemeral_candidate_scopes.pop();
                        return Err(err);
                    }
                    self.record_stmt_literal_map_effects(stmt);
                    self.record_stmt_i64_literal_effects(stmt);
                    self.record_stmt_nonnegative_i64_effects(stmt);
                    if !Self::stmt_preserves_forwarded_mem_slots(stmt) {
                        self.clear_current_forwarded_mem_slots();
                    }
                }
            } else {
                if let Err(err) = self.compile_stmt(stmt, &block.stmts[i + 1..]) {
                    self.forwarded_mem_slot_scopes.pop();
                    self.known_nonnegative_i64_scopes.pop();
                    self.known_i64_literal_scopes.pop();
                    self.literal_map_candidate_scopes.pop();
                    self.stack_shatter_candidate_scopes.pop();
                    self.fixed_array_candidate_scopes.pop();
                    self.ephemeral_zero_init_elision_scopes.pop();
                    self.ephemeral_candidate_scopes.pop();
                    return Err(err);
                }
                self.record_stmt_literal_map_effects(stmt);
                self.record_stmt_i64_literal_effects(stmt);
                self.record_stmt_nonnegative_i64_effects(stmt);
                if !Self::stmt_preserves_forwarded_mem_slots(stmt) {
                    self.clear_current_forwarded_mem_slots();
                }
            }
        }

        // If we are returning a value from the block, we must retain it before scope exit
        // destroys the local variables it might depend on.
        // Optimization: If the value is already a "new object" (owned with RC=1), we don't need to retain it
        // because no local variable owns it yet, so scope exit won't destroy it.
        if let Some((val, ty)) = &last_res {
            if ty == "i8*" && !last_is_new {
                self.emit_rc_retain_if_heap_i8(&val);
            }
        }

        self.emit_scope_exit();
        self.forwarded_mem_slot_scopes.pop();
        self.known_nonnegative_i64_scopes.pop();
        self.known_i64_literal_scopes.pop();
        self.literal_map_candidate_scopes.pop();
        self.stack_shatter_candidate_scopes.pop();
        self.fixed_array_candidate_scopes.pop();
        self.ephemeral_zero_init_elision_scopes.pop();
        self.ephemeral_candidate_scopes.pop();
        Ok(last_res)
    }

    fn emit_heap_owned_i8_guard(&mut self, val: &str) -> Option<String> {
        if val == "null" {
            return None;
        }
        let is_non_null = self.next_reg();
        self.emit(&format!("  {} = icmp ne i8* {}, null", is_non_null, val));
        let handle_bits = self.next_reg();
        self.emit(&format!("  {} = ptrtoint i8* {} to i64", handle_bits, val));
        let low_bits = self.next_reg();
        self.emit(&format!(
            "  {} = and i64 {}, {}",
            low_bits, handle_bits, ABI_TAGGED_IMMEDIATE_MASK_LLVM
        ));
        let low_bits_clear = self.next_reg();
        self.emit(&format!(
            "  {} = icmp eq i64 {}, 0",
            low_bits_clear, low_bits
        ));
        let should_call = self.next_reg();
        self.emit(&format!(
            "  {} = and i1 {}, {}",
            should_call, is_non_null, low_bits_clear
        ));
        Some(should_call)
    }

    fn emit_rc_retain_if_heap_i8(&mut self, val: &str) {
        let Some(should_call) = self.emit_heap_owned_i8_guard(val) else {
            return;
        };
        let retain_label = self.next_label();
        let merge_label = self.next_label();
        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            should_call, retain_label, merge_label
        ));
        self.emit_label(&retain_label);
        self.emit(&format!("  call void @rc_retain(i8* {})", val));
        self.emit(&format!("  br label %{}", merge_label));
        self.emit_label(&merge_label);
    }

    fn emit_rc_release_if_heap_i8(&mut self, val: &str) {
        let Some(should_call) = self.emit_heap_owned_i8_guard(val) else {
            return;
        };
        let release_label = self.next_label();
        let merge_label = self.next_label();
        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            should_call, release_label, merge_label
        ));
        self.emit_label(&release_label);
        self.emit(&format!("  call void @rc_release(i8* {})", val));
        self.emit(&format!("  br label %{}", merge_label));
        self.emit_label(&merge_label);
    }

    fn emit_release(&mut self, val: &str, ty: &str) {
        if ty == "i8*" {
            self.emit_rc_release_if_heap_i8(val);
        } else if ty.starts_with("%") {
            let struct_name = &ty[1..];
            // Clone fields to avoid borrowing self while emitting
            if let Some(fields) = self.struct_defs.get(struct_name).cloned() {
                for (i, (_, field_ty)) in fields.iter().enumerate() {
                    if field_ty == "i8*" || field_ty.starts_with("%") {
                        let field_val = self.next_reg();
                        self.emit(&format!(
                            "  {} = extractvalue {} {}, {}",
                            field_val, ty, val, i
                        ));
                        self.emit_release(&field_val, field_ty);
                    }
                }
            }
        }
    }

    fn emit_release_if_new_object_expr(&mut self, expr: &Expr, val: &str, ty: &str) {
        if (ty == "i8*" || ty.starts_with("%")) && self.is_new_object(expr) {
            self.emit_release(val, ty);
        }
    }

    fn emit_scope_cleanup_for_vars(&mut self, vars: &[String]) {
        for var_name in vars.iter().rev() {
            self.sealed_literal_map_locals.remove(var_name);
            if let Some((addr, ty)) = self.locals.get(var_name).cloned() {
                if self.borrowed_locals.contains(var_name) {
                    continue;
                }
                if let Some(local) = self.shattered_array_locals.remove(var_name) {
                    if matches!(local.backing, ShatteredArrayBacking::RuntimeHandle) {
                        let tmp = self.next_reg();
                        self.emit(&format!("  {} = load i8*, i8** {}", tmp, addr));
                        self.emit(&format!(
                            "  call void @kain_machine_shatter_free(i8* {})",
                            tmp
                        ));
                    }
                    continue;
                }
                if self.fixed_array_locals.remove(var_name).is_some() {
                    continue;
                }
                if ty == "i64" && self.json_handle_locals.contains(var_name) {
                    let tmp = self.next_reg();
                    self.emit(&format!("  {} = load i64, i64* {}", tmp, addr));
                    self.emit(&format!("  call void @json_release(i64 {})", tmp));
                }
                if ty == "i8*" && self.helper_owned_pointer_locals.contains_key(var_name) {
                    self.emit_helper_owned_local_decay_cleanup(&addr);
                    self.json_handle_locals.remove(var_name);
                    continue;
                }
                // Release if it's a pointer or struct
                if ty == "i8*" || ty.starts_with("%") {
                    let tmp = self.next_reg();
                    self.emit(&format!("  {} = load {}, {}* {}", tmp, ty, ty, addr));
                    self.emit_release(&tmp, &ty);
                }
                self.json_handle_locals.remove(var_name);
            }
        }
    }

    fn emit_scope_exit(&mut self) {
        if let Some(vars) = self.scopes.pop() {
            self.emit_scope_cleanup_for_vars(&vars);
        }
    }

    fn emit_all_scopes_cleanup(&mut self) {
        let mut vars_to_release = Vec::new();
        for scope in self.scopes.iter().rev() {
            for var in scope.iter().rev() {
                vars_to_release.push(var.clone());
            }
        }

        for var_name in vars_to_release {
            self.sealed_literal_map_locals.remove(&var_name);
            if let Some((addr, ty)) = self.locals.get(&var_name).cloned() {
                if self.borrowed_locals.contains(&var_name) {
                    continue;
                }
                if let Some(local) = self.shattered_array_locals.get(&var_name) {
                    if matches!(local.backing, ShatteredArrayBacking::RuntimeHandle) {
                        let tmp = self.next_reg();
                        self.emit(&format!("  {} = load i8*, i8** {}", tmp, addr));
                        self.emit(&format!(
                            "  call void @kain_machine_shatter_free(i8* {})",
                            tmp
                        ));
                    }
                    continue;
                }
                if self.fixed_array_locals.contains_key(&var_name) {
                    continue;
                }
                if ty == "i64" && self.json_handle_locals.contains(&var_name) {
                    let tmp = self.next_reg();
                    self.emit(&format!("  {} = load i64, i64* {}", tmp, addr));
                    self.emit(&format!("  call void @json_release(i64 {})", tmp));
                }
                if ty == "i8*" && self.helper_owned_pointer_locals.contains_key(&var_name) {
                    self.emit_helper_owned_local_decay_cleanup(&addr);
                    self.json_handle_locals.remove(&var_name);
                    continue;
                }
                if ty == "i8*" || ty.starts_with("%") {
                    let tmp = self.next_reg();
                    self.emit(&format!("  {} = load {}, {}* {}", tmp, ty, ty, addr));
                    self.emit_release(&tmp, &ty);
                }
                self.json_handle_locals.remove(&var_name);
            }
        }
    }

    fn match_fallback_value_for_type(&self, ty: &str) -> Option<String> {
        if ty == "double" {
            Some("0.0".to_string())
        } else if ty.ends_with('*') {
            Some("null".to_string())
        } else if ty.starts_with('i') {
            Some("0".to_string())
        } else if ty.starts_with('%') || ty.starts_with('[') || ty.starts_with('{') {
            Some("zeroinitializer".to_string())
        } else {
            None
        }
    }

    fn is_new_object(&self, expr: &Expr) -> bool {
        match expr {
            Expr::String(..) => self.entry_preamble_insert_offset.is_none(),
            Expr::None(..) => true,
            Expr::Ident(name, _) if name == "None" => true,
            Expr::Array(..) => true,
            Expr::Tuple(..) => true,
            Expr::Struct { .. } => true,
            Expr::EnumVariant { .. } => true,
            Expr::Call { .. } => true, // Function calls return owned values
            Expr::Binary { op, .. } => *op == BinaryOp::Add, // String concat
            Expr::If { .. } => true,   // If expressions return new objects (Phi result)
            _ => false,
        }
    }

    fn compile_if_statement(
        &mut self,
        condition: &Expr,
        then_branch: &Block,
        else_branch: Option<&ElseBranch>,
    ) -> KainResult<()> {
        let (cond_val, _) = self.compile_expr(condition)?;
        let label_then = self.next_label();
        let label_merge = self.next_label();
        let label_else = else_branch.map(|_| self.next_label());

        if let Some(label_else) = &label_else {
            self.emit(&format!(
                "  br i1 {}, label %{}, label %{}",
                cond_val, label_then, label_else
            ));
        } else {
            self.emit(&format!(
                "  br i1 {}, label %{}, label %{}",
                cond_val, label_then, label_merge
            ));
        }

        self.emit_label(&label_then);
        self.compile_block(then_branch)?;
        self.emit(&format!("  br label %{}", label_merge));

        if let Some(else_branch) = else_branch {
            let label_else = label_else.expect("else label must exist when else branch is present");
            self.emit_label(&label_else);
            match else_branch {
                ElseBranch::Else(block) => self.compile_block(block)?,
                ElseBranch::ElseIf(condition, then_branch, else_branch) => {
                    self.compile_if_statement(condition, then_branch, else_branch.as_deref())?
                }
            }
            self.emit(&format!("  br label %{}", label_merge));
        }

        self.emit_label(&label_merge);
        Ok(())
    }

    fn compile_stmt(&mut self, stmt: &Stmt, remaining_stmts: &[Stmt]) -> KainResult<()> {
        match stmt {
            Stmt::Let {
                pattern,
                value,
                ty,
                span,
            } => {
                if let Some(val_expr) = value {
                    // Allocate and Store
                    if let kain_core::ast::Pattern::Binding { name, .. } = pattern {
                        if self.current_scope_marks_fixed_array_candidate(name) {
                            let fixed_items = match val_expr {
                                Expr::Array(items, _) => Some(items),
                                Expr::MacroCall {
                                    name: macro_name,
                                    args,
                                    ..
                                } if macro_name == "vec" => Some(args),
                                _ => None,
                            };
                            if let Some(items) = fixed_items {
                                let element_ty = "i64".to_string();
                                let array_ty = format!("[{} x {}]", items.len(), element_ty);
                                let storage_reg =
                                    format!("%{}.fixed_array_{}", name, self.reg_count);
                                self.reg_count += 1;
                                self.emit_entry_alloca(&storage_reg, &array_ty);
                                for (index, item) in items.iter().enumerate() {
                                    let (item_value, item_ty) =
                                        self.compile_expr_for_target_type(item, &element_ty)?;
                                    let element_ptr = self.next_reg();
                                    self.emit(&format!(
                                        "  {} = getelementptr inbounds {}, {}* {}, i32 0, i64 {}",
                                        element_ptr, array_ty, array_ty, storage_reg, index
                                    ));
                                    self.emit(&format!(
                                        "  store {} {}, {}* {}",
                                        item_ty, item_value, element_ty, element_ptr
                                    ));
                                }
                                self.string_length_values.remove(name);
                                self.locals
                                    .insert(name.clone(), (storage_reg.clone(), array_ty.clone()));
                                self.helper_owned_pointer_locals.remove(name);
                                self.ephemeral_owned_pointer_locals.remove(name);
                                self.json_handle_locals.remove(name);
                                self.shattered_array_locals.remove(name);
                                self.fixed_array_locals.insert(
                                    name.clone(),
                                    FixedArrayLocal {
                                        storage_reg,
                                        array_ty,
                                        element_ty,
                                        element_count: items.len(),
                                    },
                                );
                                if let Some(scope) = self.scopes.last_mut() {
                                    scope.push(name.clone());
                                }
                                return Ok(());
                            }
                        }

                        if self.current_scope_marks_stack_shatter_candidate(name) {
                            let shattered_items = match val_expr {
                                Expr::Array(items, _) => Some(items),
                                _ => None,
                            };
                            if let Some(items) = shattered_items {
                                if let Some(struct_name) =
                                    self.shattered_array_expr_struct_name(val_expr)
                                {
                                    let fields =
                                        self.struct_defs.get(&struct_name).cloned().ok_or_else(
                                            || {
                                                KainError::codegen(
                                                    format!(
                                                        "Unknown shattered struct: {}",
                                                        struct_name
                                                    ),
                                                    val_expr.span(),
                                                )
                                            },
                                        )?;
                                    let addr_reg = format!("%{}.addr_{}", name, self.reg_count);
                                    self.reg_count += 1;
                                    self.emit_entry_alloca(&addr_reg, "i8*");
                                    self.emit(&format!("  store i8* null, i8** {}", addr_reg));
                                    let lane_base_values = self.emit_stack_shatter_lane_bases(
                                        name,
                                        fields.len(),
                                        items.len(),
                                    );
                                    self.populate_shattered_array_literal_lanes(
                                        &struct_name,
                                        &fields,
                                        items,
                                        &lane_base_values,
                                    )?;
                                    self.string_length_values.remove(name);
                                    self.locals.insert(
                                        name.clone(),
                                        (addr_reg.clone(), "i8*".to_string()),
                                    );
                                    self.helper_owned_pointer_locals.remove(name);
                                    self.ephemeral_owned_pointer_locals.remove(name);
                                    self.json_handle_locals.remove(name);
                                    self.fixed_array_locals.remove(name);
                                    self.shattered_array_locals.insert(
                                        name.clone(),
                                        ShatteredArrayLocal {
                                            struct_name,
                                            element_count: items.len(),
                                            lane_base_values,
                                            backing: ShatteredArrayBacking::StackLaneBuffers,
                                        },
                                    );
                                    if let Some(scope) = self.scopes.last_mut() {
                                        scope.push(name.clone());
                                    }
                                    return Ok(());
                                }
                            }
                        }

                        let known_i64_bindings = self.current_known_i64_literals();
                        if let Some(layout) = Self::helper_alloc_storage_layout_with_bindings(
                            val_expr,
                            &known_i64_bindings,
                        ) {
                            let should_use_ephemeral_local = self
                                .current_scope_marks_ephemeral_candidate(name)
                                || Self::remaining_statements_preserve_ephemeral_contract(
                                    remaining_stmts,
                                    name,
                                );
                            if should_use_ephemeral_local {
                                let known_llvm_types = self.current_known_llvm_types();
                                let single_cell = layout.element_count == 1;
                                let emit_initial_zero = layout.zeroed
                                    && !(self.current_scope_elides_ephemeral_zero_init(name)
                                        || (single_cell
                                            && self
                                                .remaining_statements_allow_ephemeral_zero_init_elision(
                                                    remaining_stmts,
                                                    name,
                                                    layout.byte_len,
                                                    &known_llvm_types,
                                                )));
                                let addr_reg = format!("%{}.addr_{}", name, self.reg_count);
                                self.reg_count += 1;
                                self.emit_entry_alloca(&addr_reg, "i64");

                                let preferred_storage_element_ty = self
                                    .preferred_ephemeral_storage_element_llvm_ty_for_let(
                                        ty.as_ref(),
                                        *span,
                                        layout,
                                    );
                                let (storage_llvm_ty, storage_element_ty, storage_alignment) =
                                    Self::helper_alloc_stack_storage_shape(
                                        layout,
                                        preferred_storage_element_ty.as_deref(),
                                    );
                                let storage_reg = self.next_reg();
                                self.emit_entry_alloca(&storage_reg, &storage_llvm_ty);
                                if emit_initial_zero {
                                    if storage_llvm_ty == storage_element_ty {
                                        let zero_value = if storage_element_ty == "double" {
                                            "0.000000e+00"
                                        } else {
                                            "0"
                                        };
                                        self.emit(&format!(
                                            "  store {} {}, {}* {}, align {}",
                                            storage_element_ty,
                                            zero_value,
                                            storage_element_ty,
                                            storage_reg,
                                            storage_alignment
                                        ));
                                    } else {
                                        self.emit(&format!(
                                            "  store {} {}, {}* {}, align {}",
                                            storage_llvm_ty,
                                            "zeroinitializer",
                                            storage_llvm_ty,
                                            storage_reg,
                                            storage_alignment
                                        ));
                                    }
                                }

                                let storage_ptr_i64 = self.next_reg();
                                self.emit(&format!(
                                    "  {} = ptrtoint {}* {} to i64",
                                    storage_ptr_i64, storage_llvm_ty, storage_reg
                                ));
                                self.emit(&format!(
                                    "  store i64 {}, i64* {}",
                                    storage_ptr_i64, addr_reg
                                ));

                                self.string_length_values.remove(name);
                                self.locals
                                    .insert(name.clone(), (addr_reg, "i64".to_string()));
                                self.helper_owned_pointer_locals.remove(name);
                                self.json_handle_locals.remove(name);
                                self.ephemeral_owned_pointer_locals.insert(
                                    name.clone(),
                                    EphemeralOwnershipLocalWitness {
                                        storage_reg,
                                        storage_llvm_ty,
                                        storage_element_ty,
                                        storage_byte_len: layout.byte_len,
                                        storage_alignment,
                                    },
                                );
                                if let Some(scope) = self.scopes.last_mut() {
                                    scope.push(name.clone());
                                }
                                return Ok(());
                            }
                        }

                        let target_ty =
                            ty.as_ref().map(|declared| self.map_type_from_ast(declared));
                        let preserve_lowered_pointer_helper_result =
                            matches!(target_ty.as_deref(), Some("i64"))
                                && matches!(
                                    val_expr,
                                    Expr::Call { callee, .. }
                                        if matches!(
                                            callee.as_ref(),
                                            Expr::Ident(name, _)
                                                if name == "__kain_alloc"
                                                    || name == "__kain_realloc"
                                        )
                                );
                        let (val_reg, val_ty) = if preserve_lowered_pointer_helper_result {
                            self.compile_expr(val_expr)?
                        } else if let Some(target_ty) = target_ty.as_deref() {
                            self.compile_expr_for_target_type(val_expr, target_ty)?
                        } else {
                            self.compile_expr(val_expr)?
                        };
                        self.string_length_values.remove(name);
                        let addr_reg = format!("%{}.addr_{}", name, self.reg_count);
                        self.reg_count += 1;

                        self.emit_entry_alloca(&addr_reg, &val_ty);
                        self.emit(&format!(
                            "  store {} {}, {}* {}",
                            val_ty, val_reg, val_ty, addr_reg
                        ));

                        // Retain if RC type AND it's not a new object (which already has RC=1)
                        if val_ty == "i8*" {
                            if self.expr_needs_rc_retain(val_expr) {
                                self.emit_rc_retain_if_heap_i8(&val_reg);
                            }
                        }

                        let local_pointer_provenance =
                            self.ownership_pointer_provenance_for_expr(val_expr);
                        self.locals.insert(name.clone(), (addr_reg, val_ty));
                        if self.expr_returns_json_handle(val_expr) {
                            self.json_handle_locals.insert(name.clone());
                        } else {
                            self.json_handle_locals.remove(name);
                        }
                        self.record_helper_owned_pointer_local(name, local_pointer_provenance);
                        self.fixed_array_locals.remove(name);
                        if let Some(struct_name) = self.shattered_array_expr_struct_name(val_expr) {
                            let field_count = self
                                .struct_defs
                                .get(&struct_name)
                                .map(|fields| fields.len())
                                .unwrap_or(0);
                            let element_count = match val_expr {
                                Expr::Array(items, _) => items.len(),
                                _ => 0,
                            };
                            let lane_base_values =
                                self.emit_shatter_lane_bases(&val_reg, field_count);
                            self.shattered_array_locals.insert(
                                name.clone(),
                                ShatteredArrayLocal {
                                    struct_name,
                                    element_count,
                                    lane_base_values,
                                    backing: ShatteredArrayBacking::RuntimeHandle,
                                },
                            );
                        } else {
                            self.shattered_array_locals.remove(name);
                        }
                        if let Some(scope) = self.scopes.last_mut() {
                            scope.push(name.clone());
                        }
                    } else {
                        let (val_reg, val_ty) = self.compile_expr(val_expr)?;
                        self.bind_local_pattern_value(pattern, val_reg, val_ty)?;
                    }
                }
            }
            Stmt::Expr(expr) => {
                if let Expr::If {
                    condition,
                    then_branch,
                    else_branch,
                    ..
                } = expr
                {
                    self.compile_if_statement(condition, then_branch, else_branch.as_deref())?;
                } else {
                    let (val, ty) = self.compile_expr(expr)?;
                    // If it is a new object, and we are ignoring the result, release it.
                    if (ty == "i8*" || ty.starts_with("%")) && self.is_new_object(expr) {
                        self.emit_release(&val, &ty);
                    }
                }
            }
            Stmt::Return(expr, _) => {
                let actor_return_label = self.actor_return_label.clone();
                let actor_return_slot = self.actor_return_slot.clone();

                if let Some(e) = expr {
                    let (val, ty) = if let Some(target_ty) = self.current_return_type.clone() {
                        self.compile_expr_for_target_type(e, &target_ty)?
                    } else {
                        self.compile_expr(e)?
                    };

                    if ty == "i8*" && self.expr_needs_rc_retain(e) {
                        self.emit_rc_retain_if_heap_i8(&val);
                    }

                    self.emit_all_scopes_cleanup();
                    if let Some(patch_name) = self.current_patch_name.clone() {
                        let patch_name_ptr = self.compile_static_c_string_literal(&patch_name);
                        let status = self.next_reg();
                        self.emit(&format!(
                            "  {} = call i64 @abi_patch_commit(i8* {})",
                            status, patch_name_ptr
                        ));
                    }

                    if let Some(return_slot) = actor_return_slot {
                        self.emit(&format!("  store {} {}, {}* {}", ty, val, ty, return_slot));
                        if let Some(return_label) = actor_return_label {
                            self.emit(&format!("  br label %{}", return_label));
                        } else {
                            self.emit(&format!("  ret {} {}", ty, val));
                        }
                    } else {
                        self.emit(&format!("  ret {} {}", ty, val));
                    }
                } else {
                    self.emit_all_scopes_cleanup();
                    if let Some(patch_name) = self.current_patch_name.clone() {
                        let patch_name_ptr = self.compile_static_c_string_literal(&patch_name);
                        let status = self.next_reg();
                        self.emit(&format!(
                            "  {} = call i64 @abi_patch_commit(i8* {})",
                            status, patch_name_ptr
                        ));
                    }
                    if let Some(return_label) = actor_return_label {
                        self.emit(&format!("  br label %{}", return_label));
                    } else {
                        self.emit("  ret void");
                    }
                }
                // Terminate block to keep LLVM happy if there's dead code
                let dead_label = self.next_label();
                self.emit_label(&dead_label);
            }
            Stmt::Break(_, _) => {
                if let Some((_, break_label)) = self.loop_stack.last() {
                    self.emit(&format!("  br label %{}", break_label));
                    let dead_label = self.next_label();
                    self.emit_label(&dead_label);
                }
            }
            Stmt::Continue(_) => {
                if let Some((continue_label, _)) = self.loop_stack.last() {
                    self.emit(&format!("  br label %{}", continue_label));
                    let dead_label = self.next_label();
                    self.emit_label(&dead_label);
                }
            }
            Stmt::While {
                condition, body, ..
            } => {
                self.clear_loop_variant_literal_facts(body);
                let label_cond = self.next_label();
                let label_body = self.next_label();
                let label_end = self.next_label();

                self.emit(&format!("  br label %{}", label_cond));
                self.emit_label(&label_cond);

                let (cond_val, _) = self.compile_expr(condition)?;
                self.emit(&format!(
                    "  br i1 {}, label %{}, label %{}",
                    cond_val, label_body, label_end
                ));

                self.emit_label(&label_body);

                self.loop_stack
                    .push((label_cond.clone(), label_end.clone()));
                self.compile_block(body)?;
                self.loop_stack.pop();

                self.emit(&format!("  br label %{}", label_cond));

                self.emit_label(&label_end);
            }
            Stmt::Loop { body, .. } => {
                self.clear_loop_variant_literal_facts(body);
                let label_body = self.next_label();
                let label_end = self.next_label();

                self.emit(&format!("  br label %{}", label_body));
                self.emit_label(&label_body);

                self.loop_stack
                    .push((label_body.clone(), label_end.clone()));
                self.compile_block(body)?;
                self.loop_stack.pop();

                self.emit(&format!("  br label %{}", label_body));
                self.emit_label(&label_end);
            }
            Stmt::For {
                binding,
                iter,
                body,
                span,
            } => {
                let known_i64_bindings = self.current_known_i64_literals();
                let literal_bounds = match iter {
                    Expr::Call { callee, args, .. }
                        if matches!(callee.as_ref(), Expr::Ident(name, _) if name == "range")
                            && args.len() == 2 =>
                    {
                        let start = Self::resolve_i64_literal(&args[0].value, &known_i64_bindings);
                        let end = Self::resolve_i64_literal(&args[1].value, &known_i64_bindings);
                        start.zip(end)
                    }
                    Expr::Range {
                        start,
                        end,
                        inclusive,
                        ..
                    } => {
                        let start = start
                            .as_ref()
                            .and_then(|value| Self::resolve_i64_literal(value, &known_i64_bindings))
                            .unwrap_or(0);
                        let end = end
                            .as_ref()
                            .and_then(|value| Self::resolve_i64_literal(value, &known_i64_bindings))
                            .unwrap_or(i64::MAX);
                        let upper = if *inclusive {
                            end.checked_add(1)
                        } else {
                            Some(end)
                        };
                        upper.map(|upper| (start, upper))
                    }
                    _ => None,
                };

                // Determine start, end
                let (start_val, end_val) = match iter {
                    Expr::Call { callee, args, .. } => {
                        if let Expr::Ident(name, _) = callee.as_ref() {
                            if name == "range" && args.len() == 2 {
                                let (s, _) = self.compile_expr(&args[0].value)?;
                                let (e, _) = self.compile_expr(&args[1].value)?;
                                (s, e)
                            } else {
                                return Err(KainError::codegen(
                                    "Unsupported call in for loop",
                                    *span,
                                ));
                            }
                        } else {
                            return Err(KainError::codegen("Unsupported call in for loop", *span));
                        }
                    }
                    Expr::Range {
                        start,
                        end,
                        inclusive,
                        ..
                    } => {
                        let s = if let Some(e) = start {
                            self.compile_expr(e)?.0
                        } else {
                            "0".into()
                        };
                        let mut e = if let Some(e) = end {
                            self.compile_expr(e)?.0
                        } else {
                            "9223372036854775807".into()
                        };
                        if *inclusive {
                            let tmp = self.next_reg();
                            self.emit(&format!("  {} = add i64 {}, 1", tmp, e));
                            e = tmp;
                        }
                        (s, e)
                    }
                    _ => {
                        return Err(KainError::codegen(
                            "Unsupported iterator in for loop",
                            *span,
                        ))
                    }
                };

                self.clear_loop_variant_literal_facts(body);

                // Allocate loop variable
                let loop_var = if let kain_core::ast::Pattern::Binding { name, .. } = binding {
                    name
                } else {
                    "it"
                };
                let var_addr = format!("%{}.addr_{}", loop_var, self.reg_count);
                self.reg_count += 1;
                self.emit_entry_alloca(&var_addr, "i64");
                self.emit(&format!("  store i64 {}, i64* {}", start_val, var_addr));
                self.locals
                    .insert(loop_var.to_string(), (var_addr.clone(), "i64".into()));

                let label_cond = self.next_label();
                let label_body = self.next_label();
                let label_step = self.next_label();
                let label_end = self.next_label();

                self.emit(&format!("  br label %{}", label_cond));
                self.emit_label(&label_cond);

                // Check condition: var < end
                let curr_val = self.next_reg();
                self.emit(&format!("  {} = load i64, i64* {}", curr_val, var_addr));
                let cond_res = self.next_reg();
                self.emit(&format!(
                    "  {} = icmp slt i64 {}, {}",
                    cond_res, curr_val, end_val
                ));
                self.emit(&format!(
                    "  br i1 {}, label %{}, label %{}",
                    cond_res, label_body, label_end
                ));

                self.emit_label(&label_body);

                self.loop_stack
                    .push((label_step.clone(), label_end.clone()));
                let loop_bounds_were_pushed = if let (
                    kain_core::ast::Pattern::Binding { name, .. },
                    Some((lower_inclusive, upper_exclusive)),
                ) = (binding, literal_bounds)
                {
                    let mut scope = HashMap::new();
                    scope.insert(
                        name.clone(),
                        LoopIndexBounds {
                            lower_inclusive,
                            upper_exclusive,
                        },
                    );
                    self.active_loop_index_bounds.push(scope);
                    true
                } else {
                    false
                };
                let body_result = self.compile_block(body);
                if loop_bounds_were_pushed {
                    self.active_loop_index_bounds.pop();
                }
                self.loop_stack.pop();
                body_result?;

                self.emit(&format!("  br label %{}", label_step));
                self.emit_label(&label_step);

                // Increment
                let val_before_inc = self.next_reg();
                self.emit(&format!(
                    "  {} = load i64, i64* {}",
                    val_before_inc, var_addr
                ));
                let val_after_inc = self.next_reg();
                self.emit(&format!(
                    "  {} = add i64 {}, 1",
                    val_after_inc, val_before_inc
                ));
                self.emit(&format!("  store i64 {}, i64* {}", val_after_inc, var_addr));

                self.emit(&format!("  br label %{}", label_cond));
                self.emit_label(&label_end);
            }
            _ => {}
        }
        Ok(())
    }

    fn compile_numeric_floor_builtin(&mut self, arg: &Expr) -> KainResult<(String, String)> {
        let (value, value_ty) = self.compile_expr(arg)?;
        if !matches!(value_ty.as_str(), "double" | "i64" | "i32" | "i8" | "i1") {
            return Err(KainError::codegen(
                format!("floor expects numeric argument, found {}", value_ty),
                arg.span(),
            ));
        }

        let float_value = if value_ty == "double" {
            value
        } else {
            self.cast_numeric_value(value, &value_ty, "double")?
        };
        let floored_float = self.next_reg();
        self.emit(&format!(
            "  {} = call double @llvm.floor.f64(double {})",
            floored_float, float_value
        ));
        let floored_int = self.next_reg();
        self.emit(&format!(
            "  {} = fptosi double {} to i64",
            floored_int, floored_float
        ));
        Ok((floored_int, "i64".to_string()))
    }

    fn compile_numeric_abs_builtin(&mut self, arg: &Expr) -> KainResult<(String, String)> {
        let (value, value_ty) = self.compile_expr(arg)?;
        if value_ty == "double" {
            let is_non_negative = self.next_reg();
            self.emit(&format!(
                "  {} = fcmp oge double {}, 0.000000",
                is_non_negative, value
            ));
            let negated = self.next_reg();
            self.emit(&format!("  {} = fsub double 0.000000, {}", negated, value));
            let selected = self.next_reg();
            self.emit(&format!(
                "  {} = select i1 {}, double {}, double {}",
                selected, is_non_negative, value, negated
            ));
            return Ok((selected, "double".to_string()));
        }

        if matches!(value_ty.as_str(), "i64" | "i32" | "i8" | "i1") {
            let widened = if value_ty == "i64" {
                value
            } else {
                self.cast_numeric_value(value, &value_ty, "i64")?
            };
            let is_non_negative = self.next_reg();
            self.emit(&format!(
                "  {} = icmp sge i64 {}, 0",
                is_non_negative, widened
            ));
            let negated = self.next_reg();
            self.emit(&format!("  {} = sub i64 0, {}", negated, widened));
            let selected = self.next_reg();
            self.emit(&format!(
                "  {} = select i1 {}, i64 {}, i64 {}",
                selected, is_non_negative, widened, negated
            ));
            return Ok((selected, "i64".to_string()));
        }

        Err(KainError::codegen(
            format!("abs expects numeric argument, found {}", value_ty),
            arg.span(),
        ))
    }

    fn compile_numeric_min_or_max_builtin(
        &mut self,
        func_name: &str,
        lhs_expr: &Expr,
        rhs_expr: &Expr,
    ) -> KainResult<(String, String)> {
        let (lhs, lhs_ty) = self.compile_expr(lhs_expr)?;
        let (rhs, rhs_ty) = self.compile_expr(rhs_expr)?;
        let (lhs, common_ty, rhs, _) = self.coerce_binary_operands(lhs, lhs_ty, rhs, rhs_ty)?;
        let compare = self.next_reg();
        let select = self.next_reg();

        match common_ty.as_str() {
            "double" => {
                let predicate = if func_name == "min" { "ole" } else { "oge" };
                self.emit(&format!(
                    "  {} = fcmp {} double {}, {}",
                    compare, predicate, lhs, rhs
                ));
                self.emit(&format!(
                    "  {} = select i1 {}, double {}, double {}",
                    select, compare, lhs, rhs
                ));
                Ok((select, "double".to_string()))
            }
            "i64" | "i32" | "i8" | "i1" => {
                let predicate = if func_name == "min" { "sle" } else { "sge" };
                self.emit(&format!(
                    "  {} = icmp {} {} {}, {}",
                    compare, predicate, common_ty, lhs, rhs
                ));
                self.emit(&format!(
                    "  {} = select i1 {}, {} {}, {} {}",
                    select, compare, common_ty, lhs, common_ty, rhs
                ));
                Ok((select, common_ty))
            }
            _ => Err(KainError::codegen(
                format!("{func_name} expects numeric arguments, found {}", common_ty),
                lhs_expr.span(),
            )),
        }
    }

    fn compile_numeric_clamp_builtin(
        &mut self,
        value_expr: &Expr,
        lo_expr: &Expr,
        hi_expr: &Expr,
    ) -> KainResult<(String, String)> {
        let (value, value_ty) = self.compile_expr(value_expr)?;
        let (lo, lo_ty) = self.compile_expr(lo_expr)?;
        let (hi, hi_ty) = self.compile_expr(hi_expr)?;
        let (mut value, mut value_common_ty, lo, lo_common_ty) =
            self.coerce_binary_operands(value, value_ty, lo, lo_ty)?;
        let (lo, final_ty, hi, _) = self.coerce_binary_operands(lo, lo_common_ty, hi, hi_ty)?;
        if value_common_ty != final_ty {
            value = self.cast_numeric_value(value, &value_common_ty, &final_ty)?;
            value_common_ty = final_ty.clone();
        }

        let (lower_bounded, lower_ty) = self.compile_numeric_min_or_max_builtin_from_values(
            "max",
            value,
            value_common_ty,
            lo,
            value_expr.span(),
        )?;
        self.compile_numeric_min_or_max_builtin_from_values(
            "min",
            lower_bounded,
            lower_ty,
            hi,
            value_expr.span(),
        )
    }

    fn compile_numeric_min_or_max_builtin_from_values(
        &mut self,
        func_name: &str,
        lhs: String,
        lhs_ty: String,
        rhs: String,
        span: kain_core::Span,
    ) -> KainResult<(String, String)> {
        let compare = self.next_reg();
        let select = self.next_reg();

        match lhs_ty.as_str() {
            "double" => {
                let predicate = if func_name == "min" { "ole" } else { "oge" };
                self.emit(&format!(
                    "  {} = fcmp {} double {}, {}",
                    compare, predicate, lhs, rhs
                ));
                self.emit(&format!(
                    "  {} = select i1 {}, double {}, double {}",
                    select, compare, lhs, rhs
                ));
                Ok((select, "double".to_string()))
            }
            "i64" | "i32" | "i8" | "i1" => {
                let predicate = if func_name == "min" { "sle" } else { "sge" };
                self.emit(&format!(
                    "  {} = icmp {} {} {}, {}",
                    compare, predicate, lhs_ty, lhs, rhs
                ));
                self.emit(&format!(
                    "  {} = select i1 {}, {} {}, {} {}",
                    select, compare, lhs_ty, lhs, lhs_ty, rhs
                ));
                Ok((select, lhs_ty))
            }
            _ => Err(KainError::codegen(
                format!("{func_name} expects numeric arguments, found {}", lhs_ty),
                span,
            )),
        }
    }

    fn compile_direct_call(
        &mut self,
        func_name: &str,
        args: &[kain_core::ast::CallArg],
    ) -> KainResult<(String, String)> {
        if args.len() == 1 {
            let constructor_target_ty = match func_name {
                "Int" | "i64" => Some("i64"),
                "i32" => Some("i32"),
                "i8" => Some("i8"),
                "Float" | "f64" | "double" => Some("double"),
                "Bool" | "bool" => Some("i1"),
                _ => None,
            };
            if let Some(target_ty) = constructor_target_ty {
                let (value, value_ty) = self.compile_expr(&args[0].value)?;
                return self.coerce_compiled_value_to_target_type(value, &value_ty, target_ty);
            }
        }

        if let Some(result) = self.compile_manual_find_substring_call_fast_path(func_name, args)? {
            return Ok(result);
        }

        if func_name == "len" && args.len() == 1 {
            if let Expr::Ident(name, _) = &args[0].value {
                if let Some(local) = self.shattered_array_locals.get(name) {
                    return Ok((local.element_count.to_string(), "i64".to_string()));
                }
                if let Some(local) = self.fixed_array_locals.get(name) {
                    return Ok((local.element_count.to_string(), "i64".to_string()));
                }
            }
            if let Some(length_value) = self.compile_string_length_value(&args[0].value)? {
                return Ok((length_value, "i64".to_string()));
            }
        }

        if func_name == "map_get" && args.len() == 2 {
            if let Some(literal) = self.extract_static_string_literal(&args[1].value) {
                if let Expr::Ident(map_name, _) = &args[0].value {
                    if let Some(local) = self.sealed_literal_map_locals.get(map_name) {
                        if let Some(value) = local.entries.get(&literal) {
                            return Ok((value.to_string(), "i64".to_string()));
                        }
                    }
                }
                let (map_value, map_ty) = self.compile_expr(&args[0].value)?;
                if map_ty != "i64" {
                    return Err(KainError::codegen(
                        format!("map_get expects native map handle as i64, found {}", map_ty),
                        args[0].value.span(),
                    ));
                }
                let key_ptr = self.compile_static_c_string_literal(&literal);
                let (key_length, key_hash, key_prefix) =
                    kain_map_codegen_static_key_metadata(&literal);
                let result = self.next_reg();
                self.emit(&format!(
                    "  {} = call i64 @map_get_prehashed(i64 {}, i8* {}, i64 {}, i64 {}, i64 {})",
                    result, map_value, key_ptr, key_length, key_hash, key_prefix
                ));
                return Ok((result, "i64".to_string()));
            }
        }

        if func_name == "map_set" && args.len() == 3 {
            if let Some(literal) = self.extract_static_string_literal(&args[1].value) {
                let (map_value, map_ty) = self.compile_expr(&args[0].value)?;
                let (key_length, key_hash, key_prefix) =
                    kain_map_codegen_static_key_metadata(&literal);
                let key_ptr = self.compile_static_c_string_literal(&literal);
                let (mut value, mut value_ty) = self.compile_expr(&args[2].value)?;
                if matches!(value_ty.as_str(), "i32" | "i8" | "i1" | "double") {
                    value = self.cast_numeric_value(value, &value_ty, "i64")?;
                    value_ty = "i64".to_string();
                }
                if value_ty == "i8*" && self.expr_needs_rc_retain(&args[2].value) {
                    self.emit_rc_retain_if_heap_i8(&value);
                }
                if value_ty == "i8*" || value_ty.starts_with('%') {
                    let int_val = self.next_reg();
                    self.emit(&format!(
                        "  {} = ptrtoint {} {} to i64",
                        int_val, value_ty, value
                    ));
                    value = int_val;
                    value_ty = "i64".to_string();
                }
                if map_ty != "i64" || value_ty != "i64" {
                    return Err(KainError::codegen(
                        format!(
                            "map_set expects native map/value handles as i64, found {} and {}",
                            map_ty, value_ty
                        ),
                        args[0].value.span(),
                    ));
                }
                self.emit(&format!(
                    "  call void @map_set_static_prehashed(i64 {}, i8* {}, i64 {}, i64 {}, i64 {}, i64 {})",
                    map_value, key_ptr, key_length, key_hash, key_prefix, value
                ));
                return Ok(("0".into(), "i64".into()));
            }
        }

        if func_name == "byte_at" && args.len() == 2 {
            if let Some(result) = self.compile_byte_at_fast_path(&args[0].value, &args[1].value)? {
                return Ok((result, "i64".to_string()));
            }
        }

        if func_name == "find_substring_from" && args.len() == 3 {
            if let Some(result) = self.compile_find_substring_from_fast_path(
                &args[0].value,
                &args[1].value,
                &args[2].value,
            )? {
                return Ok((result, "i64".to_string()));
            }
        }

        if func_name == "floor" && args.len() == 1 {
            return self.compile_numeric_floor_builtin(&args[0].value);
        }

        if func_name == "abs" && args.len() == 1 {
            return self.compile_numeric_abs_builtin(&args[0].value);
        }

        if matches!(func_name, "min" | "max") && args.len() == 2 {
            return self.compile_numeric_min_or_max_builtin(
                func_name,
                &args[0].value,
                &args[1].value,
            );
        }

        if func_name == "clamp" && args.len() == 3 {
            return self.compile_numeric_clamp_builtin(
                &args[0].value,
                &args[1].value,
                &args[2].value,
            );
        }

        if let Some(result) = self.compile_json_builtin_call(func_name, args)? {
            return Ok(result);
        }

        let mut compiled_args = Vec::new();
        let mut arg_types = Vec::new();
        let is_extern = self.extern_functions.contains(func_name);
        let param_types = self.function_params.get(func_name).cloned();
        let borrowed_string_params = self.string_function_params.get(func_name).cloned();

        for (index, arg) in args.iter().enumerate() {
            let param_ty = param_types
                .as_ref()
                .and_then(|types| types.get(index))
                .cloned()
                .unwrap_or_default();
            let inferred_borrowed_string = borrowed_string_params
                .as_ref()
                .and_then(|flags| flags.get(index))
                .copied()
                .unwrap_or(false);
            let runtime_borrowed_string =
                stdlib_function_uses_borrowed_string_param(func_name, index);
            let passes_borrowed_string = inferred_borrowed_string || runtime_borrowed_string;
            let can_lower_static_literal_as_borrowed =
                runtime_borrowed_string || (is_extern && inferred_borrowed_string);

            if is_extern && param_ty == "void" {
                continue;
            }

            let (mut val, mut ty) = if can_lower_static_literal_as_borrowed {
                if let Some(literal) = self.extract_static_string_literal(&arg.value) {
                    (
                        self.compile_static_c_string_literal(&literal),
                        "i8*".to_string(),
                    )
                } else {
                    self.compile_expr(&arg.value)?
                }
            } else {
                self.compile_expr(&arg.value)?
            };

            if matches!(param_ty.as_str(), "i64" | "i32" | "i8" | "i1" | "double")
                && matches!(ty.as_str(), "i64" | "i32" | "i8" | "i1" | "double")
                && ty != param_ty
            {
                val = self.cast_numeric_value(val, &ty, &param_ty)?;
                ty = param_ty.clone();
            }

            if !is_extern
                && ty == "i8*"
                && self.expr_needs_rc_retain(&arg.value)
                && !passes_borrowed_string
            {
                self.emit_rc_retain_if_heap_i8(&val);
            }

            let needs_cast_to_i64 = (ty == "i8*" || ty.starts_with('%'))
                && ((func_name == "push" && index == 1)
                    || (func_name == "array_push" && index == 1)
                    || (func_name == "array_set" && index == 2)
                    || (func_name == "map_set" && index == 2));

            if needs_cast_to_i64 {
                let int_val = self.next_reg();
                self.emit(&format!("  {} = ptrtoint {} {} to i64", int_val, ty, val));
                compiled_args.push(int_val);
                arg_types.push("i64".to_string());
                continue;
            }

            compiled_args.push(val);
            arg_types.push(ty);
        }

        let ret_ty = self
            .functions
            .get(func_name)
            .cloned()
            .unwrap_or_else(|| "i64".to_string());
        let arg_str = compiled_args
            .iter()
            .zip(arg_types.iter())
            .map(|(val, ty)| format!("{} {}", ty, val))
            .collect::<Vec<_>>()
            .join(", ");
        let callee_symbol = runtime_symbol_for_stdlib_function(func_name);

        if ret_ty == "void" {
            self.emit(&format!("  call void @{}({})", callee_symbol, arg_str));
            Ok(("0".into(), "i64".into()))
        } else {
            let res = self.next_reg();
            self.emit(&format!(
                "  {} = call {} @{}({})",
                res, ret_ty, callee_symbol, arg_str
            ));
            Ok((res, ret_ty))
        }
    }

    fn compile_stage_call(
        &mut self,
        runtime: &kain_core::ast::OrchestrateStageRuntime,
        function: &str,
        args: &[kain_core::ast::CallArg],
    ) -> KainResult<(String, String)> {
        self.emit(&format!(
            "  ; orchestrate stage {} -> {}",
            runtime.as_str(),
            function
        ));
        if !llvm_orchestrate_trace_enabled() {
            self.emit("  ; benchmark-release elides orchestrate telemetry wrapper");
            return self.compile_direct_call(function, args);
        }
        let runtime_name = self.compile_static_c_string_literal(runtime.as_str());
        let function_name = self.compile_static_c_string_literal(function);
        let begin_status = self.next_reg();
        self.emit(&format!(
            "  {} = call i64 @abi_orchestrate_stage_begin(i8* {}, i8* {})",
            begin_status, runtime_name, function_name
        ));
        let (value, ty) = self.compile_direct_call(function, args)?;
        if ty == "i64" {
            let end_status = self.next_reg();
            self.emit(&format!(
                "  {} = call i64 @abi_orchestrate_stage_end_i64(i8* {}, i8* {}, i64 {})",
                end_status, runtime_name, function_name, value
            ));
        }
        Ok((value, ty))
    }

    fn compile_expr_as_string_value(&mut self, expr: &Expr) -> KainResult<(String, bool)> {
        let (val, ty) = self.compile_expr(expr)?;
        let (text, _) = self.stringify_value(&val, &ty)?;
        let release_after_use = ty != "i8*" || self.is_new_object(expr);
        Ok((text, release_after_use))
    }

    fn compile_stdout_write_call(
        &mut self,
        intrinsic_name: &str,
        args: &[kain_core::ast::CallArg],
        span: kain_core::Span,
    ) -> KainResult<(String, String)> {
        if args.len() != 1 {
            return Err(KainError::codegen(
                format!("{intrinsic_name} expects exactly one argument"),
                span,
            ));
        }

        let (mut text, mut release_text) = self.compile_expr_as_string_value(&args[0].value)?;
        if intrinsic_name == "println" {
            let (newline, _) = self.compile_string_literal("\n");
            let with_newline = self.concat_strings(&text, &newline);
            if release_text {
                self.emit_release(&text, "i8*");
            }
            self.emit_release(&newline, "i8*");
            text = with_newline;
            release_text = true;
        }

        self.emit(&format!("  call void @stdout_write(i8* {})", text));
        if release_text {
            self.emit_release(&text, "i8*");
        }
        Ok(("0".into(), "i64".into()))
    }

    fn compile_macro_call(
        &mut self,
        name: &str,
        args: &[Expr],
        span: kain_core::Span,
    ) -> KainResult<(String, String)> {
        match name {
            "vec" => {
                let arr = self.next_reg();
                self.emit(&format!(
                    "  {} = call i8* @array_new(i64 {})",
                    arr,
                    args.len().max(4)
                ));
                for item in args {
                    let (val, ty) = self.compile_expr(item)?;
                    let stored = self.coerce_to_i64_storage(&val, &ty);
                    self.emit(&format!(
                        "  call void @array_push(i8* {}, i64 {})",
                        arr, stored
                    ));
                }
                Ok((arr, "i8*".into()))
            }
            "format" | "__kain_write_fmt" | "__kain_writeln_fmt" => {
                let format_args = if matches!(name, "__kain_write_fmt" | "__kain_writeln_fmt") {
                    if args.len() != 2 {
                        return Err(KainError::codegen(
                            format!("{name}! expects destination and message"),
                            span,
                        ));
                    }
                    &args[1..]
                } else {
                    args
                };

                let (mut acc, _) = self.compile_string_literal("");
                for arg in format_args {
                    let (text, release_text) = self.compile_expr_as_string_value(arg)?;
                    let next = self.concat_strings(&acc, &text);
                    self.emit_release(&acc, "i8*");
                    if release_text {
                        self.emit_release(&text, "i8*");
                    }
                    acc = next;
                }

                if name == "__kain_writeln_fmt" {
                    let (newline, _) = self.compile_string_literal("\n");
                    let next = self.concat_strings(&acc, &newline);
                    self.emit_release(&acc, "i8*");
                    self.emit_release(&newline, "i8*");
                    acc = next;
                }

                Ok((acc, "i8*".into()))
            }
            "type_name" => Err(KainError::codegen(
                "LLVM backend does not lower type_name! faithfully yet",
                span,
            )),
            "panic" => Err(KainError::codegen(
                "LLVM backend does not lower panic! faithfully yet",
                span,
            )),
            _ => Err(KainError::codegen(
                format!("Unsupported LLVM macro call: {name}!"),
                span,
            )),
        }
    }

    fn compile_expr(&mut self, expr: &Expr) -> KainResult<(String, String)> {
        match expr {
            Expr::Int(n, _) => Ok((format!("{}", n), "i64".to_string())),
            Expr::Float(f, _) => Ok((format!("{:.6}", f), "double".to_string())),
            Expr::Bool(b, _) => Ok((if *b { "1".into() } else { "0".into() }, "i1".to_string())),
            Expr::String(s, _) => Ok(self.compile_string_literal(s)),
            Expr::FString(parts, _) => {
                let (mut acc, _) = self.compile_string_literal("");
                for part in parts {
                    let (text, release_text) = self.compile_expr_as_string_value(part)?;
                    let next = self.concat_strings(&acc, &text);
                    self.emit_release(&acc, "i8*");
                    if release_text {
                        self.emit_release(&text, "i8*");
                    }
                    acc = next;
                }
                Ok((acc, "i8*".to_string()))
            }
            Expr::MacroCall { name, args, span } => self.compile_macro_call(name, args, *span),
            Expr::None(_) => {
                Ok(("null".to_string(), "i8*".to_string()))
            }
            Expr::Try(value, span) => self.compile_try_for_target_type(value, "i64", *span),
            Expr::Await(value, span) => self.compile_await_for_target_type(value, "i64", *span),
            Expr::AsyncBlock(body, span) => self.compile_async_block(body, *span),
            Expr::JSX(node, _) => self.compile_jsx(node),
            Expr::Paren(inner, _) => self.compile_expr(inner),
            Expr::Block(block, _) => self
                .compile_block_with_result(block)
                .map(|res| res.unwrap_or(("0".into(), "i64".into()))),
            Expr::Cast { value, target, .. } => {
                let dst_ty = self.map_type_from_ast(target);
                if let Expr::Call { callee, args, span } = value.as_ref() {
                    if let Expr::Ident(name, _) = callee.as_ref() {
                        if name == "__kain_mem_load" && args.len() == 1 {
                            return self.compile_runtime_mem_load(&args[0].value, &dst_ty, *span);
                        }
                    }
                }
                let (val, src_ty) = self.compile_expr(value)?;
                if src_ty == dst_ty {
                    Ok((val, dst_ty))
                } else if src_ty.ends_with('*') && dst_ty == "i64" {
                    let res = self.next_reg();
                    self.emit(&format!("  {} = ptrtoint {} {} to i64", res, src_ty, val));
                    Ok((res, dst_ty))
                } else if src_ty == "i64" && dst_ty == "double" {
                    let res = self.next_reg();
                    self.emit(&format!("  {} = sitofp i64 {} to double", res, val));
                    Ok((res, dst_ty))
                } else if src_ty == "double" && dst_ty == "i64" {
                    let res = self.next_reg();
                    self.emit(&format!("  {} = fptosi double {} to i64", res, val));
                    Ok((res, dst_ty))
                } else if src_ty == "i1" && dst_ty == "i64" {
                    let res = self.next_reg();
                    self.emit(&format!("  {} = zext i1 {} to i64", res, val));
                    Ok((res, dst_ty))
                } else if src_ty == "i64" && dst_ty == "i1" {
                    let res = self.next_reg();
                    self.emit(&format!("  {} = icmp ne i64 {}, 0", res, val));
                    Ok((res, dst_ty))
                } else if src_ty == "i64" && dst_ty.ends_with('*') {
                    let res = self.next_reg();
                    self.emit(&format!("  {} = inttoptr i64 {} to {}", res, val, dst_ty));
                    Ok((res, dst_ty))
                } else {
                    Ok((val, dst_ty))
                }
            }
            Expr::Ref { value, .. } => {
                let (addr, ty) = self.compile_addressable_ptr(value)?;
                Ok((addr, format!("{}*", ty)))
            }
            Expr::AddrOf { value, .. } => {
                let (addr, ty) = self.compile_addressable_ptr(value)?;
                Ok((addr, format!("{}*", ty)))
            }
            Expr::Deref(inner, span) => {
                let (val, ty) = self.compile_expr(inner)?;
                if let Some(pointee_ty) = ty.strip_suffix('*') {
                    let res = self.next_reg();
                    self.emit(&format!("  {} = load {}, {} {}", res, pointee_ty, ty, val));
                    Ok((res, pointee_ty.to_string()))
                } else {
                    Err(KainError::codegen(
                        "Cannot dereference non-pointer value",
                        *span,
                    ))
                }
            }
            Expr::PtrOffset {
                pointer,
                offset,
                element_ty,
                ..
            } => {
                let (base, base_ty) = self.compile_expr(pointer)?;
                let (off, _) = self.compile_expr(offset)?;
                let stride = element_ty
                    .as_ref()
                    .map(|ty| self.map_type_from_ast(ty))
                    .map(|ty| {
                        if ty == "double" {
                            8
                        } else if ty == "i8" {
                            1
                        } else if ty == "i1" {
                            1
                        } else {
                            8
                        }
                    })
                    .unwrap_or(8);
                let base_i64 = self.coerce_to_i64_storage(&base, &base_ty);
                let stride_literal =
                    Some(stride).filter(|_| self.expr_is_proven_nonnegative_i64(offset));
                let scaled =
                    self.emit_scaled_byte_offset(&off, &stride.to_string(), stride_literal);
                let res = self.next_reg();
                self.emit(&format!("  {} = add i64 {}, {}", res, base_i64, scaled));
                Ok((res, "i64".into()))
            }
            Expr::MemLoad {
                pointer,
                load_ty,
                span,
                ..
            } => {
                let target_ty = load_ty
                    .as_ref()
                    .map(|ty| self.map_type_from_ast(ty))
                    .unwrap_or_else(|| "i64".to_string());
                self.compile_runtime_mem_load(pointer, &target_ty, *span)
            }
            Expr::MemStore {
                pointer,
                value,
                span,
                ..
            } => self.compile_runtime_mem_store(pointer, value, *span),
            Expr::SizeOfType { target, .. } => {
                let mapped = self.map_type_from_ast(target);
                let size = if mapped == "double" {
                    8
                } else if mapped == "i8" {
                    1
                } else if mapped == "i1" {
                    1
                } else {
                    8
                };
                Ok((size.to_string(), "i64".into()))
            }
            Expr::AlignOfType { target, .. } => {
                let mapped = self.map_type_from_ast(target);
                let align = if mapped == "double" {
                    8
                } else if mapped == "i8" {
                    1
                } else if mapped == "i1" {
                    1
                } else {
                    8
                };
                Ok((align.to_string(), "i64".into()))
            }
            Expr::Alloca { ty, .. } => {
                let ty_str = self.map_type_from_ast(ty);
                let addr = self.next_reg();
                self.emit_entry_alloca(&addr, &ty_str);
                Ok((addr, format!("{}*", ty_str)))
            }
            Expr::Uninit { ty, .. } => Ok((
                self.zero_value_for_ty(&self.map_type_from_ast(ty)),
                self.map_type_from_ast(ty),
            )),
            Expr::Alloc { .. } => Err(KainError::codegen(
                "LLVM backend expected alloc to be lowered into a canonical __kain_alloc helper call",
                expr.span(),
            )),
            Expr::Realloc { .. } => Err(KainError::codegen(
                "LLVM backend expected realloc_mem to be lowered into a canonical __kain_realloc helper call",
                expr.span(),
            )),
            Expr::Observe { target, body, .. } => self.compile_scoped_ownership_expr(
                target,
                body,
                "__kain_ownership_begin_observe",
                "__kain_ownership_begin_observe_helper",
                "__kain_ownership_end_observe",
                "__kain_ownership_end_observe_helper",
            ),
            Expr::Collapse { target, body, .. } => self.compile_scoped_ownership_expr(
                target,
                body,
                "__kain_ownership_begin_collapse",
                "__kain_ownership_begin_collapse_helper",
                "__kain_ownership_end_collapse",
                "__kain_ownership_end_collapse_helper",
            ),
            Expr::Decay { target, .. } => self.compile_decay_expr(target),
            Expr::Teleport {
                value,
                source_world,
                target_world,
                channel,
                ..
            } => self.compile_teleport_expr(
                value,
                source_world,
                target_world,
                channel.as_deref(),
            ),
            Expr::Unary { op, operand, span } => {
                let (val, ty) = self.compile_expr(operand)?;
                match op {
                    UnaryOp::Neg => {
                        let res = self.next_reg();
                        if ty == "double" {
                            self.emit(&format!("  {} = fneg double {}", res, val));
                        } else {
                            self.emit(&format!("  {} = sub {} 0, {}", res, ty, val));
                        }
                        Ok((res, ty))
                    }
                    UnaryOp::Not => {
                        let res = self.next_reg();
                        self.emit(&format!("  {} = xor i1 {}, 1", res, val));
                        Ok((res, "i1".into()))
                    }
                    UnaryOp::BitNot => {
                        let res = self.next_reg();
                        self.emit(&format!("  {} = xor {} {}, -1", res, ty, val));
                        Ok((res, ty))
                    }
                    UnaryOp::Deref => {
                        if let Some(pointee_ty) = ty.strip_suffix('*') {
                            let res = self.next_reg();
                            self.emit(&format!("  {} = load {}, {} {}", res, pointee_ty, ty, val));
                            Ok((res, pointee_ty.to_string()))
                        } else {
                            Err(KainError::codegen(
                                "Cannot dereference non-pointer value",
                                *span,
                            ))
                        }
                    }
                    UnaryOp::Ref | UnaryOp::RefMut => Ok((val, format!("{}*", ty))),
                }
            }
            Expr::Field {
                ..
            } => {
                let (field_ptr, field_ty) = self.compile_addressable_ptr(expr)?;
                let loaded = self.next_reg();
                self.emit(&format!(
                    "  {} = load {}, {}* {}",
                    loaded, field_ty, field_ty, field_ptr
                ));
                Ok((loaded, field_ty))
            }
            Expr::Assign {
                target,
                value,
                span,
            } => match target.as_ref() {
                Expr::Ident(name, _) => {
                    if let Some((addr, ty)) = self.locals.get(name).cloned() {
                        let (rhs, rhs_ty) = self.compile_expr_for_target_type(value, &ty)?;
                        self.string_length_values.remove(name);
                        let was_borrowed_local = self.borrowed_locals.remove(name);
                        if ty == "i8*" {
                            if self.expr_needs_rc_retain(value) {
                                self.emit_rc_retain_if_heap_i8(&rhs);
                            }
                            if !was_borrowed_local {
                                let previous_value = self.next_reg();
                                self.emit(&format!(
                                    "  {} = load i8*, i8** {}",
                                    previous_value, addr
                                ));
                                self.emit_rc_release_if_heap_i8(&previous_value);
                            }
                        }
                        self.emit(&format!("  store {} {}, {}* {}", rhs_ty, rhs, ty, addr));
                        self.record_helper_owned_pointer_local(
                            name,
                            self.ownership_pointer_provenance_for_expr(value),
                        );
                        Ok((rhs, rhs_ty))
                    } else {
                        Err(KainError::codegen(
                            format!("Undefined assignment target: {}", name),
                            *span,
                        ))
                    }
                }
                Expr::Field { object, field, .. } => {
                    let (obj_val, obj_ty) = self.compile_expr(object)?;
                    let (struct_name, struct_ptr, field_index) =
                        if let Some(struct_name) = self.ptr_struct_name(&obj_ty) {
                            let index = self.field_index(struct_name, field).ok_or_else(|| {
                                KainError::codegen(
                                    format!("Unknown field '{}' on {}", field, struct_name),
                                    *span,
                                )
                            })?;
                            (struct_name.to_string(), obj_val, index)
                        } else if obj_ty.starts_with('%') {
                            let struct_name = obj_ty[1..].to_string();
                            let index = self.field_index(&struct_name, field).ok_or_else(|| {
                                KainError::codegen(
                                    format!("Unknown field '{}' on {}", field, struct_name),
                                    *span,
                                )
                            })?;
                            let tmp_addr = self.next_reg();
                            self.emit_entry_alloca(&tmp_addr, &obj_ty);
                            self.emit(&format!(
                                "  store {} {}, {}* {}",
                                obj_ty, obj_val, obj_ty, tmp_addr
                            ));
                            (struct_name, tmp_addr, index)
                        } else {
                            return Err(KainError::codegen(
                                "Field assignment requires a struct or struct pointer",
                                *span,
                            ));
                        };
                    let field_ty = self
                        .struct_defs
                        .get(&struct_name)
                        .and_then(|fields| fields.get(field_index))
                        .map(|(_, ty)| ty.clone())
                        .unwrap_or_else(|| "i64".to_string());
                    let field_path = self.native_world_field_path(&struct_name, field);
                    if let Some(path) = field_path.as_ref() {
                        if let Some(binding) = self.native_entangle_mirror_binding(path) {
                            return Err(KainError::codegen(
                                format!(
                                    "cannot write entangle mirror '{}' directly; write authority '{}'",
                                    binding.mirror, binding.authority
                                ),
                                *span,
                            ));
                        }
                    }
                    let field_ptr = self.next_reg();
                    self.emit(&format!(
                        "  {} = getelementptr inbounds %{}, %{}* {}, i32 0, i32 {}",
                        field_ptr, struct_name, struct_name, struct_ptr, field_index
                    ));
                    let old_value = if self.current_patch_name.is_some() && field_ty == "i64" {
                        let loaded = self.next_reg();
                        self.emit(&format!("  {} = load i64, i64* {}", loaded, field_ptr));
                        Some(loaded)
                    } else {
                        None
                    };
                    let (rhs, rhs_ty) = self.compile_expr_for_target_type(value, &field_ty)?;
                    if let (Some(path), Some(old_value)) = (field_path.as_ref(), old_value.as_ref())
                    {
                        self.emit_patch_record_i64(path, old_value, &rhs);
                    }
                    self.emit(&format!(
                        "  store {} {}, {}* {}",
                        rhs_ty, rhs, field_ty, field_ptr
                    ));
                    if field_ty == "i64" {
                        if let Some(path) = field_path.as_ref() {
                            if let Some(binding) = self.native_entangle_authority_binding(path) {
                                self.emit_entangle_i64_propagation(&binding, &rhs)?;
                            }
                        }
                    }
                    Ok((rhs, rhs_ty))
                }
                Expr::Index { object, index, .. } => {
                    let (obj_val, obj_ty) = self.compile_expr(object)?;
                    let (idx_val, _) = self.compile_expr(index)?;
                    if obj_ty == "i8*" {
                        let (rhs, rhs_ty) = self.compile_expr(value)?;
                        let stored = self.coerce_to_i64_storage(&rhs, &rhs_ty);
                        self.emit(&format!(
                            "  call void @array_set(i8* {}, i64 {}, i64 {})",
                            obj_val, idx_val, stored
                        ));
                        Ok((rhs, rhs_ty))
                    } else {
                        let (field_ptr, field_ty) = self.compile_index_address_from_compiled(
                            &obj_val, &obj_ty, &idx_val, *span,
                        )?;
                        let (rhs, rhs_ty) = self.compile_expr_for_target_type(value, &field_ty)?;
                        self.emit(&format!(
                            "  store {} {}, {}* {}",
                            rhs_ty, rhs, field_ty, field_ptr
                        ));
                        Ok((rhs, rhs_ty))
                    }
                }
                _ => Err(KainError::codegen("Unsupported assignment target", *span)),
            },
            Expr::Struct {
                name,
                fields,
                rest,
                span,
            } => {
                if rest.is_some() {
                    return Err(KainError::codegen(
                        "Struct update syntax is not yet supported by LLVM codegen",
                        *span,
                    ));
                }
                let def = self.struct_defs.get(name).cloned().ok_or_else(|| {
                    KainError::codegen(format!("Unknown struct: {}", name), *span)
                })?;
                let struct_ty = format!("%{}", name);
                if self.value_aggregate_structs.contains(name) {
                    let mut aggregate_value = "zeroinitializer".to_string();
                    let mut provided: HashMap<String, Expr> = fields.iter().cloned().collect();
                    for (index, (field_name, field_ty)) in def.iter().enumerate() {
                        let (field_value, _) = if let Some(expr) = provided.remove(field_name) {
                            self.compile_expr_for_target_type(&expr, field_ty)?
                        } else {
                            (self.zero_value_for_ty(field_ty), field_ty.clone())
                        };
                        let next_aggregate = self.next_reg();
                        self.emit(&format!(
                            "  {} = insertvalue {} {}, {} {}, {}",
                            next_aggregate,
                            struct_ty,
                            aggregate_value,
                            field_ty,
                            field_value,
                            index
                        ));
                        aggregate_value = next_aggregate;
                    }
                    return Ok((aggregate_value, struct_ty));
                }
                let ptr_ty = format!("{}*", struct_ty);
                let null_ptr = format!("{} null", ptr_ty);
                let size_ptr_reg = self.next_reg();
                self.emit(&format!(
                    "  {} = getelementptr {}, {}, i32 1",
                    size_ptr_reg, struct_ty, null_ptr
                ));
                let size_reg = self.next_reg();
                self.emit(&format!(
                    "  {} = ptrtoint {} {} to i64",
                    size_reg, ptr_ty, size_ptr_reg
                ));
                let mem_reg = self.next_reg();
                self.emit(&format!(
                    "  {} = call i8* @KAIN_alloc(i64 {})",
                    mem_reg, size_reg
                ));
                let struct_ptr = self.next_reg();
                self.emit(&format!(
                    "  {} = bitcast i8* {} to {}",
                    struct_ptr, mem_reg, ptr_ty
                ));
                if def
                    .iter()
                    .any(|(_, field_ty)| field_ty == "i8*" || field_ty.starts_with('%'))
                {
                    let dtor_name = format!("dtor_{}", name);
                    self.emit(&format!(
                        "  call void @KAIN_set_destructor(i8* {}, void (i8*)* @{})",
                        mem_reg, dtor_name
                    ));
                }
                let mut provided: HashMap<String, Expr> = fields.iter().cloned().collect();
                for (i, (field_name, field_ty)) in def.iter().enumerate() {
                    let (val, val_ty) = if let Some(expr) = provided.remove(field_name) {
                        self.compile_expr(&expr)?
                    } else {
                        (self.zero_value_for_ty(field_ty), field_ty.clone())
                    };
                    let field_ptr = self.next_reg();
                    self.emit(&format!(
                        "  {} = getelementptr inbounds {}, {} {}, i32 0, i32 {}",
                        field_ptr, struct_ty, ptr_ty, struct_ptr, i
                    ));
                    self.emit(&format!(
                        "  store {} {}, {}* {}",
                        val_ty, val, val_ty, field_ptr
                    ));
                }
                Ok((struct_ptr, ptr_ty))
            }
            Expr::AggregateInit {
                ty, fields, span, ..
            } => match ty {
                kain_core::ast::Type::Named { name, .. } => self.compile_expr(&Expr::Struct {
                    name: name.clone(),
                    fields: fields.clone(),
                    rest: None,
                    span: *span,
                }),
                kain_core::ast::Type::Tuple(_, _) => self.compile_expr(&Expr::Tuple(
                    fields.iter().map(|(_, value)| value.clone()).collect(),
                    *span,
                )),
                _ => Err(KainError::codegen(
                    format!("Unsupported LLVM aggregate init type: {:?}", ty),
                    *span,
                )),
            },
            Expr::Array(items, _) => {
                if let Some(struct_name) = self.shattered_array_expr_struct_name(expr) {
                    return self.compile_shattered_array_literal(
                        &struct_name,
                        items,
                        expr.span(),
                    );
                }
                let arr = self.next_reg();
                self.emit(&format!(
                    "  {} = call i8* @array_new(i64 {})",
                    arr,
                    items.len().max(4)
                ));
                for item in items {
                    let (val, ty) = self.compile_expr(item)?;
                    let stored = self.coerce_to_i64_storage(&val, &ty);
                    self.emit(&format!(
                        "  call void @array_push(i8* {}, i64 {})",
                        arr, stored
                    ));
                }
                Ok((arr, "i8*".into()))
            }
            Expr::Tuple(items, span) => {
                let mut compiled_fields = Vec::new();
                let mut field_tys = Vec::new();
                for item in items {
                    let (val, ty) = self.compile_expr(item)?;
                    compiled_fields.push((val, ty.clone()));
                    field_tys.push(ty);
                }

                let tuple_name = Self::tuple_struct_name_from_types(&field_tys);
                let tuple_ty = format!("%{}", tuple_name);
                let tuple_ptr_ty = format!("{}*", tuple_ty);
                if !self.struct_defs.contains_key(&tuple_name) {
                    return Err(KainError::codegen(
                        format!(
                            "Tuple LLVM type '{}' was not registered before codegen",
                            tuple_name
                        ),
                        *span,
                    ));
                }

                if self.value_aggregate_structs.contains(&tuple_name) {
                    let mut aggregate_value = "zeroinitializer".to_string();
                    for (index, (field_val, field_ty)) in compiled_fields.iter().enumerate() {
                        let next_aggregate = self.next_reg();
                        self.emit(&format!(
                            "  {} = insertvalue {} {}, {} {}, {}",
                            next_aggregate,
                            tuple_ty,
                            aggregate_value,
                            field_ty,
                            field_val,
                            index
                        ));
                        aggregate_value = next_aggregate;
                    }
                    return Ok((aggregate_value, tuple_ty));
                }

                let null_ptr = format!("{} null", tuple_ptr_ty);
                let size_ptr_reg = self.next_reg();
                self.emit(&format!(
                    "  {} = getelementptr %{}, {}, i32 1",
                    size_ptr_reg, tuple_name, null_ptr
                ));
                let size_reg = self.next_reg();
                self.emit(&format!(
                    "  {} = ptrtoint {} {} to i64",
                    size_reg, tuple_ptr_ty, size_ptr_reg
                ));
                let mem_reg = self.next_reg();
                self.emit(&format!(
                    "  {} = call i8* @KAIN_alloc(i64 {})",
                    mem_reg, size_reg
                ));
                let tuple_ptr = self.next_reg();
                self.emit(&format!(
                    "  {} = bitcast i8* {} to {}",
                    tuple_ptr, mem_reg, tuple_ptr_ty
                ));

                for (index, (field_val, field_ty)) in compiled_fields.iter().enumerate() {
                    let field_ptr = self.next_reg();
                    self.emit(&format!(
                        "  {} = getelementptr inbounds %{}, {} {}, i32 0, i32 {}",
                        field_ptr, tuple_name, tuple_ptr_ty, tuple_ptr, index
                    ));
                    self.emit(&format!(
                        "  store {} {}, {}* {}",
                        field_ty, field_val, field_ty, field_ptr
                    ));
                }

                Ok((tuple_ptr, tuple_ptr_ty))
            }
            Expr::Index {
                object,
                index,
                span: _,
            } => {
                if let Expr::Ident(name, _) = object.as_ref() {
                    if let Some(local) = self.fixed_array_locals.get(name).cloned() {
                        let (idx_val, _) = self.compile_expr(index)?;
                        let element_ptr = self.next_reg();
                        self.emit(&format!(
                            "  {} = getelementptr inbounds {}, {}* {}, i32 0, i64 {}",
                            element_ptr, local.array_ty, local.array_ty, local.storage_reg, idx_val
                        ));
                        let loaded = self.next_reg();
                        self.emit(&format!(
                            "  {} = load {}, {}* {}",
                            loaded, local.element_ty, local.element_ty, element_ptr
                        ));
                        return Ok((loaded, local.element_ty));
                    }
                }
                let (obj_val, obj_ty) = self.compile_expr(object)?;
                let (idx_val, _) = self.compile_expr(index)?;
                if obj_ty == "i8*" {
                    let res = self.next_reg();
                    self.emit(&format!(
                        "  {} = call i64 @array_get(i8* {}, i64 {})",
                        res, obj_val, idx_val
                    ));
                    Ok((res, "i64".into()))
                } else {
                    let (field_ptr, field_ty) = self.compile_index_address_from_compiled(
                        &obj_val,
                        &obj_ty,
                        &idx_val,
                        index.span(),
                    )?;
                    let loaded = self.next_reg();
                    self.emit(&format!(
                        "  {} = load {}, {}* {}",
                        loaded, field_ty, field_ty, field_ptr
                    ));
                    Ok((loaded, field_ty))
                }
            }
            Expr::Spawn { actor, init, span } => {
                let def = self
                    .struct_defs
                    .get(actor)
                    .cloned()
                    .ok_or(KainError::codegen(
                        format!("Unknown actor: {}", actor),
                        *span,
                ))?;

                let struct_ty = format!("%{}", actor);
                let turn_fn_ty = "i32 (i64, i8*, i8*, i32)";

                // Allocate the compiler-owned actor state on the heap.
                let null_ptr = format!("{}* null", struct_ty);
                let size_ptr_reg = self.next_reg();
                self.emit(&format!(
                    "  {} = getelementptr {}, {}, i32 1",
                    size_ptr_reg, struct_ty, null_ptr
                ));
                let size_reg = self.next_reg();
                self.emit(&format!(
                    "  {} = ptrtoint {}* {} to i64",
                    size_reg, struct_ty, size_ptr_reg
                ));

                let mem_reg = self.next_reg();
                self.emit(&format!(
                    "  {} = call i8* @KAIN_alloc(i64 {})",
                    mem_reg, size_reg
                ));

                let struct_ptr = self.next_reg();
                self.emit(&format!(
                    "  {} = bitcast i8* {} to {}*",
                    struct_ptr, mem_reg, struct_ty
                ));

                // Initialize the runtime spawn config on the stack.
                let config_ptr = self.next_reg();
                self.emit_entry_alloca(&config_ptr, "%KainActorSpawnConfig");
                self.emit(&format!(
                    "  call void @kain_actor_spawn_config_init(%KainActorSpawnConfig* {})",
                    config_ptr
                ));

                let user_data_ptr = self.next_reg();
                self.emit(&format!(
                    "  {} = getelementptr inbounds %KainActorSpawnConfig, %KainActorSpawnConfig* {}, i32 0, i32 1",
                    user_data_ptr, config_ptr
                ));
                self.emit(&format!("  store i8* {}, i8** {}", mem_reg, user_data_ptr));

                let retain_user_data_ptr = self.next_reg();
                self.emit(&format!(
                    "  {} = getelementptr inbounds %KainActorSpawnConfig, %KainActorSpawnConfig* {}, i32 0, i32 6",
                    retain_user_data_ptr, config_ptr
                ));
                self.emit(&format!("  store i32 1, i32* {}", retain_user_data_ptr));

                let entry_kind_ptr = self.next_reg();
                self.emit(&format!(
                    "  {} = getelementptr inbounds %KainActorSpawnConfig, %KainActorSpawnConfig* {}, i32 0, i32 8",
                    entry_kind_ptr, config_ptr
                ));
                self.emit(&format!("  store i32 2, i32* {}", entry_kind_ptr));

                let turn_ptr = self.next_reg();
                self.emit(&format!(
                    "  {} = getelementptr inbounds %KainActorSpawnConfig, %KainActorSpawnConfig* {}, i32 0, i32 9",
                    turn_ptr, config_ptr
                ));
                self.emit(&format!(
                    "  store {}* @{}_turn, {}** {}",
                    turn_fn_ty, actor, turn_fn_ty, turn_ptr
                ));

                let inline_ask_policy_ptr = self.next_reg();
                self.emit(&format!(
                    "  {} = getelementptr inbounds %KainActorSpawnConfig, %KainActorSpawnConfig* {}, i32 0, i32 13",
                    inline_ask_policy_ptr, config_ptr
                ));
                self.emit(&format!("  store i32 1, i32* {}", inline_ask_policy_ptr));

                // Initialize fields.
                let mut provided: HashMap<String, Expr> = init.iter().cloned().collect();
                for (i, (field_name, field_ty)) in def.iter().enumerate() {
                    if field_name == "__actor_ref" {
                        continue;
                    }

                    let (val, val_ty) = if let Some(expr) = provided.remove(field_name) {
                        self.compile_expr_for_target_type(&expr, field_ty)?
                    } else {
                        (self.zero_value_for_ty(field_ty), field_ty.clone())
                    };

                    let field_ptr = self.next_reg();
                    self.emit(&format!(
                        "  {} = getelementptr inbounds {}, {}* {}, i32 0, i32 {}",
                        field_ptr, struct_ty, struct_ty, struct_ptr, i
                    ));
                    self.emit(&format!(
                        "  store {} {}, {}* {}",
                        val_ty, val, val_ty, field_ptr
                    ));
                }

                // Register a destructor for owned actor state when RC fields exist.
                let has_rc_fields = def.iter().any(|(_, ty)| ty == "i8*" || ty.starts_with("%"));
                if has_rc_fields {
                    let dtor_name = format!("dtor_{}", actor);
                    self.emit(&format!(
                        "  call void @KAIN_set_destructor(i8* {}, void (i8*)* @{})",
                        mem_reg, dtor_name
                    ));
                }

                let actor_id_reg = self.next_reg();
                self.emit(&format!(
                    "  {} = call i64 @kain_actor_spawn(%KainActorSpawnConfig* {}, i8* null)",
                    actor_id_reg, config_ptr
                ));

                let actor_ref_ptr = self.next_reg();
                self.emit(&format!(
                    "  {} = getelementptr inbounds {}, {}* {}, i32 0, i32 0",
                    actor_ref_ptr, struct_ty, struct_ty, struct_ptr
                ));
                self.emit(&format!(
                    "  call void @kain_actor_ref_from_id(i64 {}, {}* {})",
                    actor_id_reg, ACTOR_REF_LLVM_TYPE, actor_ref_ptr
                ));

                Ok((struct_ptr, format!("%{}*", actor)))
            }
            Expr::SendMsg {
                target,
                message,
                data,
                span,
            } => {
                let (target_val, target_ty) = self.compile_expr(target)?;
                let actor_name = self.actor_name_for_handle_type(&target_ty).ok_or_else(|| {
                    KainError::codegen(
                        format!(
                            "Cannot send message '{}' to non-actor type {}",
                            message, target_ty
                        ),
                        *span,
                    )
                })?;
                if actor_name == REPLY_PORT_ACTOR_NAME {
                    let target_ref =
                        self.compile_actor_handle_ref_value(&target_val, &target_ty, *span)?;
                    let (payload_mem, message_size) = if message != "Reply" {
                        return Err(KainError::codegen(
                            format!(
                                "Reply port handles only accept the synthetic 'Reply' message, found '{}'",
                                message
                            ),
                            *span,
                        ));
                    } else if data.is_empty() {
                        ("null".to_string(), "0".to_string())
                    } else {
                        if data.len() != 1 {
                            return Err(KainError::codegen(
                                "Reply port messages accept at most one payload field named 'value'",
                                *span,
                            ));
                        }
                        let (field_name, payload_expr) = &data[0];
                        if field_name != "value" {
                            return Err(KainError::codegen(
                                "Reply port payload field must be named 'value'",
                                *span,
                            ));
                        }
                        let (payload_value, payload_ty) = self.compile_expr(payload_expr)?;
                        let (payload_ptr, payload_size) = self
                            .compile_payload_pointer_from_value(&payload_value, &payload_ty, *span)?;
                        (payload_ptr, payload_size.to_string())
                    };
                    let target_ref_ptr = self.next_reg();
                    self.emit_entry_alloca(&target_ref_ptr, ACTOR_REF_LLVM_TYPE);
                    self.emit(&format!(
                        "  store {} {}, {}* {}",
                        ACTOR_REF_LLVM_TYPE,
                        target_ref,
                        ACTOR_REF_LLVM_TYPE,
                        target_ref_ptr
                    ));
                    let send_status = self.next_reg();
                    self.emit(&format!(
                        "  {} = call i32 @kain_actor_reply_port_send_ref({}* {}, i8* {}, i64 {})",
                        send_status,
                        ACTOR_REF_LLVM_TYPE,
                        target_ref_ptr,
                        payload_mem,
                        message_size
                    ));
                    return Ok(("0".into(), "i64".into()));
                }

                let target_id = self.compile_actor_handle_id(&target_val, &target_ty, *span)?;

                let sender_id = if let Some((self_addr, self_ty)) = self.locals.get("self").cloned()
                {
                    if self_ty.starts_with('%') {
                        self.compile_actor_handle_id(&self_addr, &self_ty, *span)?
                    } else {
                        "0".to_string()
                    }
                } else {
                    "0".to_string()
                };

                let message_ptr = self.next_reg();
                self.emit_entry_alloca(&message_ptr, "%KainActorMessage");

                let (payload_mem, message_size) = if let Some(field_defs) = self
                    .struct_defs
                    .get(&format!("{}_{}", actor_name, message))
                    .cloned()
                {
                    if field_defs.is_empty() {
                        ("null".to_string(), "0".to_string())
                    } else {
                        let payload_struct_name = format!("{}_{}", actor_name, message);
                        let payload_ty = format!("%{}", payload_struct_name);
                        let payload_ptr_ty = format!("{}*", payload_ty);
                        let payload_ptr = self.next_reg();
                        self.emit_entry_alloca(&payload_ptr, &payload_ty);

                        let named_args: std::collections::HashMap<String, Expr> =
                            data.iter().cloned().collect();
                        for (i, (field_name, field_ty)) in field_defs.iter().enumerate() {
                            let expr = named_args.get(field_name).ok_or_else(|| {
                                KainError::codegen(
                                    format!(
                                        "Missing field '{}' for actor message '{}.{}'",
                                        field_name, actor_name, message
                                    ),
                                    *span,
                                )
                            })?;
                            let (val, val_ty) =
                                self.compile_expr_for_target_type(expr, field_ty)?;
                            let field_ptr = self.next_reg();
                            self.emit(&format!(
                                "  {} = getelementptr inbounds {}, {}* {}, i32 0, i32 {}",
                                field_ptr, payload_ty, payload_ptr_ty, payload_ptr, i
                            ));
                            self.emit(&format!(
                                "  store {} {}, {}* {}",
                                val_ty, val, field_ty, field_ptr
                            ));
                        }

                        let null_ptr = format!("{}* null", payload_ty);
                        let size_ptr_reg = self.next_reg();
                        self.emit(&format!(
                            "  {} = getelementptr {}, {}, i32 1",
                            size_ptr_reg, payload_ty, null_ptr
                        ));
                        let size_reg = self.next_reg();
                        self.emit(&format!(
                            "  {} = ptrtoint {}* {} to i64",
                            size_reg, payload_ptr_ty, size_ptr_reg
                        ));
                        let payload_i8 = self.next_reg();
                        self.emit(&format!(
                            "  {} = bitcast {}* {} to i8*",
                            payload_i8, payload_ty, payload_ptr
                        ));
                        (payload_i8, size_reg)
                    }
                } else {
                    ("null".to_string(), "0".to_string())
                };

                let message_tag = self.hash_message_tag(&actor_name, message);
                let message_tag_ptr = self.next_reg();
                self.emit(&format!(
                    "  {} = getelementptr inbounds %KainActorMessage, %KainActorMessage* {}, i32 0, i32 0",
                    message_tag_ptr, message_ptr
                ));
                self.emit(&format!(
                    "  store i64 {}, i64* {}",
                    message_tag, message_tag_ptr
                ));

                let message_data_ptr = self.next_reg();
                self.emit(&format!(
                    "  {} = getelementptr inbounds %KainActorMessage, %KainActorMessage* {}, i32 0, i32 1",
                    message_data_ptr, message_ptr
                ));
                self.emit(&format!(
                    "  store i8* {}, i8** {}",
                    payload_mem, message_data_ptr
                ));

                let message_size_ptr = self.next_reg();
                self.emit(&format!(
                    "  {} = getelementptr inbounds %KainActorMessage, %KainActorMessage* {}, i32 0, i32 2",
                    message_size_ptr, message_ptr
                ));
                self.emit(&format!(
                    "  store i64 {}, i64* {}",
                    message_size, message_size_ptr
                ));

                let message_sender_ptr = self.next_reg();
                self.emit(&format!(
                    "  {} = getelementptr inbounds %KainActorMessage, %KainActorMessage* {}, i32 0, i32 3",
                    message_sender_ptr, message_ptr
                ));
                self.emit(&format!(
                    "  store i64 {}, i64* {}",
                    sender_id, message_sender_ptr
                ));

                let send_status = self.next_reg();
                self.emit(&format!(
                    "  {} = call i32 @kain_actor_send(i64 {}, %KainActorMessage* {}, i8* null)",
                    send_status, target_id, message_ptr
                ));
                Ok(("0".into(), "i64".into()))
            }
            Expr::If {
                condition,
                then_branch,
                else_branch,
                span,
            } => {
                let start_block = self.current_block.clone();
                let (cond_val, _) = self.compile_expr(condition)?;

                let label_then = self.next_label();
                let label_else = self.next_label();
                let label_merge = self.next_label();

                let has_else = else_branch.is_some();
                let target_else = if has_else { &label_else } else { &label_merge };

                self.emit(&format!(
                    "  br i1 {}, label %{}, label %{}",
                    cond_val, label_then, target_else
                ));

                let mut incoming = Vec::new();

                // Then Block
                self.emit_label(&label_then);
                let then_res = self.compile_block_with_result(then_branch)?;
                let then_end_block = self.current_block.clone();
                self.emit(&format!("  br label %{}", label_merge));

                if let Some((val, ty)) = then_res {
                    incoming.push((val, ty, then_end_block));
                } else {
                    incoming.push(("0".into(), "i64".into(), then_end_block));
                }

                // Else Block
                if let Some(else_branch) = else_branch {
                    self.emit_label(&label_else);
                    let else_res = match else_branch.as_ref() {
                        kain_core::ast::ElseBranch::Else(b) => self.compile_block_with_result(b)?,
                        kain_core::ast::ElseBranch::ElseIf(cond, then, el) => {
                            let nested = Expr::If {
                                condition: cond.clone(),
                                then_branch: then.clone(),
                                else_branch: el.clone(),
                                span: *span,
                            };
                            Some(self.compile_expr(&nested)?)
                        }
                    };

                    let else_end_block = self.current_block.clone();
                    self.emit(&format!("  br label %{}", label_merge));

                    if let Some((val, ty)) = else_res {
                        incoming.push((val, ty, else_end_block));
                    } else {
                        incoming.push(("0".into(), "i64".into(), else_end_block));
                    }
                } else {
                    // No else branch: path comes from start_block with value 0
                    incoming.push(("0".into(), "i64".into(), start_block));
                }

                self.emit_label(&label_merge);

                // Generate Phi
                let res_ty = incoming[0].1.clone();
                let res_reg = self.next_reg();

                // Check consistency (simple check)
                let consistent = incoming.iter().all(|(_, ty, _)| *ty == res_ty);

                if consistent {
                    let phi_args = incoming
                        .iter()
                        .map(|(val, _, block)| format!("[ {}, %{} ]", val, block))
                        .collect::<Vec<_>>()
                        .join(", ");

                    self.emit(&format!("  {} = phi {} {}", res_reg, res_ty, phi_args));
                    Ok((res_reg, res_ty))
                } else {
                    Err(KainError::codegen(
                        "LLVM if-expression branches produced inconsistent result types",
                        *span,
                    ))
                }
            }
            Expr::Ident(name, span) => {
                if name == "None" {
                    Ok(("null".to_string(), "i8*".to_string()))
                } else if let Some((ptr, ty)) = self.locals.get(name).cloned() {
                    let reg = self.next_reg();
                    self.emit(&format!("  {} = load {}, {}* {}", reg, ty, ty, ptr));
                    Ok((reg, ty))
                } else if let Some(info) = self.const_globals.get(name).cloned() {
                    Ok(self.compile_const_load(&info))
                } else if let Some(world_info) = self.world_globals.get(name).cloned() {
                    self.emit(&format!("  call void @{}()", world_info.init_fn_name));
                    Ok((world_info.global_symbol.clone(), format!("%{}*", name)))
                } else {
                    Err(KainError::codegen(
                        format!("Undefined variable: {}", name),
                        *span,
                    ))
                }
            }
            expr @ Expr::Binary {
                left, op, right, ..
            } => {
                if *op == BinaryOp::Eq || *op == BinaryOp::Ne {
                    if let Some(result) =
                        self.compile_char_at_string_equality_fast_path(left, right)?
                    {
                        if *op == BinaryOp::Ne {
                            let inverted = self.next_reg();
                            self.emit(&format!("  {} = xor i1 {}, 1", inverted, result));
                            return Ok((inverted, "i1".to_string()));
                        }
                        return Ok((result, "i1".to_string()));
                    }
                }

                if *op == BinaryOp::Add && self.expr_is_known_string(expr) {
                    if let Some(result) = self.compile_string_concat_expression(expr)? {
                        return Ok(result);
                    }
                }

                let (lhs, lhs_ty) = self.compile_expr(left)?;
                let (rhs, rhs_ty) = self.compile_expr(right)?;
                if *op == BinaryOp::Add && (lhs_ty == "i8*" || rhs_ty == "i8*") {
                    let lhs_release_after_use = if lhs_ty == "i8*" {
                        self.is_new_object(left)
                    } else {
                        true
                    };
                    let rhs_release_after_use = if rhs_ty == "i8*" {
                        self.is_new_object(right)
                    } else {
                        true
                    };
                    let (lhs, _) = self.stringify_value(&lhs, &lhs_ty)?;
                    let (rhs, _) = self.stringify_value(&rhs, &rhs_ty)?;
                    let res = self.next_reg();
                    self.emit(&format!(
                        "  {} = call i8* @str_concat(i8* {}, i8* {})",
                        res, lhs, rhs
                    ));
                    if lhs_release_after_use {
                        self.emit_rc_release_if_heap_i8(&lhs);
                    }
                    if rhs_release_after_use {
                        self.emit_rc_release_if_heap_i8(&rhs);
                    }
                    return Ok((res, "i8*".into()));
                }

                if (*op == BinaryOp::Eq || *op == BinaryOp::Ne)
                    && (lhs_ty == "i8*" || rhs_ty == "i8*")
                {
                    let (lhs, _) = self.stringify_value(&lhs, &lhs_ty)?;
                    let (rhs, _) = self.stringify_value(&rhs, &rhs_ty)?;
                    let res = self.next_reg();
                    self.emit(&format!(
                        "  {} = call i1 @deep_eq(i8* {}, i8* {})",
                        res, lhs, rhs
                    ));

                    if *op == BinaryOp::Ne {
                        let inv = self.next_reg();
                        self.emit(&format!("  {} = xor i1 {}, 1", inv, res));
                        return Ok((inv, "i1".into()));
                    }
                    return Ok((res, "i1".into()));
                }

                let (lhs, ty, rhs, rhs_ty) =
                    self.coerce_binary_operands(lhs, lhs_ty, rhs, rhs_ty)?;

                let is_float = ty == "double" && rhs_ty == "double";
                let res = self.next_reg();
                let rhs_literal = Self::resolve_i64_literal(right, &self.current_known_i64_literals());
                let lhs_nonnegative = self.expr_is_proven_nonnegative_i64(left);

                match op {
                    BinaryOp::Add => {
                        if is_float {
                            self.emit(&format!("  {} = fadd double {}, {}", res, lhs, rhs));
                        } else {
                            self.emit(&format!("  {} = add {} {}, {}", res, ty, lhs, rhs));
                        }
                        Ok((res, ty))
                    }
                    BinaryOp::Sub => {
                        if is_float {
                            self.emit(&format!("  {} = fsub double {}, {}", res, lhs, rhs));
                        } else {
                            self.emit(&format!("  {} = sub {} {}, {}", res, ty, lhs, rhs));
                        }
                        Ok((res, ty))
                    }
                    BinaryOp::Mul => {
                        if is_float {
                            self.emit(&format!("  {} = fmul double {}, {}", res, lhs, rhs));
                        } else {
                            self.emit(&format!("  {} = mul {} {}, {}", res, ty, lhs, rhs));
                        }
                        Ok((res, ty))
                    }
                    BinaryOp::Div => {
                        if is_float {
                            self.emit(&format!("  {} = fdiv double {}, {}", res, lhs, rhs));
                        } else if ty == "i64" && rhs_ty == "i64" && lhs_nonnegative {
                            if let Some(shift) =
                                rhs_literal.and_then(Self::positive_power_of_two_shift)
                            {
                                self.emit(&format!("  {} = lshr i64 {}, {}", res, lhs, shift));
                            } else {
                                self.emit(&format!("  {} = sdiv {} {}, {}", res, ty, lhs, rhs));
                            }
                        } else {
                            self.emit(&format!("  {} = sdiv {} {}, {}", res, ty, lhs, rhs));
                        }
                        Ok((res, ty))
                    }
                    BinaryOp::Mod => {
                        if is_float {
                            self.emit(&format!("  {} = frem double {}, {}", res, lhs, rhs));
                        } else if ty == "i64" && rhs_ty == "i64" && lhs_nonnegative {
                            if let Some(mask) = rhs_literal
                                .and_then(Self::positive_power_of_two_shift)
                                .map(|shift| (1i64 << shift) - 1)
                            {
                                self.emit(&format!("  {} = and i64 {}, {}", res, lhs, mask));
                            } else {
                                self.emit(&format!("  {} = srem {} {}, {}", res, ty, lhs, rhs));
                            }
                        } else {
                            self.emit(&format!("  {} = srem {} {}, {}", res, ty, lhs, rhs));
                        }
                        Ok((res, ty))
                    }
                    BinaryOp::Pow => {
                        if is_float {
                            self.emit(&format!(
                                "  {} = call double @pow(double {}, double {})",
                                res, lhs, rhs
                            ));
                            Ok((res, "double".to_string()))
                        } else {
                            let lhs_cast = self.cast_numeric_value(lhs, &ty, "double")?;
                            let rhs_cast = self.cast_numeric_value(rhs, &rhs_ty, "double")?;
                            let pow_res = self.next_reg();
                            self.emit(&format!(
                                "  {} = call double @pow(double {}, double {})",
                                pow_res, lhs_cast, rhs_cast
                            ));
                            let int_res = self.next_reg();
                            self.emit(&format!("  {} = fptosi double {} to i64", int_res, pow_res));
                            Ok((int_res, "i64".to_string()))
                        }
                    }
                    BinaryOp::Eq => {
                        if is_float {
                            self.emit(&format!("  {} = fcmp oeq double {}, {}", res, lhs, rhs));
                        } else {
                            self.emit(&format!("  {} = icmp eq {} {}, {}", res, ty, lhs, rhs));
                        }
                        Ok((res, "i1".to_string()))
                    }
                    BinaryOp::Ne => {
                        if is_float {
                            self.emit(&format!("  {} = fcmp one double {}, {}", res, lhs, rhs));
                        } else {
                            self.emit(&format!("  {} = icmp ne {} {}, {}", res, ty, lhs, rhs));
                        }
                        Ok((res, "i1".to_string()))
                    }
                    BinaryOp::Lt => {
                        if is_float {
                            self.emit(&format!("  {} = fcmp olt double {}, {}", res, lhs, rhs));
                        } else {
                            self.emit(&format!("  {} = icmp slt {} {}, {}", res, ty, lhs, rhs));
                        }
                        Ok((res, "i1".to_string()))
                    }
                    BinaryOp::Gt => {
                        if is_float {
                            self.emit(&format!("  {} = fcmp ogt double {}, {}", res, lhs, rhs));
                        } else {
                            self.emit(&format!("  {} = icmp sgt {} {}, {}", res, ty, lhs, rhs));
                        }
                        Ok((res, "i1".to_string()))
                    }
                    BinaryOp::Le => {
                        if is_float {
                            self.emit(&format!("  {} = fcmp ole double {}, {}", res, lhs, rhs));
                        } else {
                            self.emit(&format!("  {} = icmp sle {} {}, {}", res, ty, lhs, rhs));
                        }
                        Ok((res, "i1".to_string()))
                    }
                    BinaryOp::Ge => {
                        if is_float {
                            self.emit(&format!("  {} = fcmp oge double {}, {}", res, lhs, rhs));
                        } else {
                            self.emit(&format!("  {} = icmp sge {} {}, {}", res, ty, lhs, rhs));
                        }
                        Ok((res, "i1".to_string()))
                    }
                    BinaryOp::And => {
                        self.emit(&format!("  {} = and {} {}, {}", res, ty, lhs, rhs));
                        Ok((res, ty))
                    }
                    BinaryOp::Or => {
                        self.emit(&format!("  {} = or {} {}, {}", res, ty, lhs, rhs));
                        Ok((res, ty))
                    }
                    BinaryOp::BitAnd => {
                        self.emit(&format!("  {} = and {} {}, {}", res, ty, lhs, rhs));
                        Ok((res, ty))
                    }
                    BinaryOp::BitOr => {
                        self.emit(&format!("  {} = or {} {}, {}", res, ty, lhs, rhs));
                        Ok((res, ty))
                    }
                    BinaryOp::BitXor => {
                        self.emit(&format!("  {} = xor {} {}, {}", res, ty, lhs, rhs));
                        Ok((res, ty))
                    }
                    BinaryOp::Shl => {
                        self.emit(&format!("  {} = shl {} {}, {}", res, ty, lhs, rhs));
                        Ok((res, ty))
                    }
                    BinaryOp::Shr => {
                        self.emit(&format!("  {} = ashr {} {}, {}", res, ty, lhs, rhs));
                        Ok((res, ty))
                    }
                    _ => Err(KainError::codegen(
                        format!("Unsupported LLVM binary operator: {:?}", op),
                        expr.span(),
                    )),
                }
            }
            Expr::MethodCall {
                receiver,
                method,
                args,
                span,
            } => {
                // LLVM doesn't have native method dispatch.
                // We resolve methods by checking the type of the receiver.

                let (obj_val, obj_ty) = self.compile_expr(receiver)?;

                // 1. Struct Methods: Call Struct_method(obj, args...)
                if obj_ty == "i8*" {
                    match method.as_str() {
                        "is_some" => {
                            if !args.is_empty() {
                                return Err(KainError::codegen("is_some expects no arguments", *span));
                            }
                            let result = self.compile_tagged_value_is_tag(
                                &obj_val,
                                &[ABI_TAG_OPTION_SOME_LLVM],
                                false,
                            );
                            self.emit_release_if_new_object_expr(receiver, &obj_val, &obj_ty);
                            return Ok((result, "i1".to_string()));
                        }
                        "is_none" => {
                            if !args.is_empty() {
                                return Err(KainError::codegen("is_none expects no arguments", *span));
                            }
                            let result = self.compile_tagged_value_is_tag(
                                &obj_val,
                                &[ABI_TAG_OPTION_NONE_LLVM],
                                true,
                            );
                            self.emit_release_if_new_object_expr(receiver, &obj_val, &obj_ty);
                            return Ok((result, "i1".to_string()));
                        }
                        "is_ok" => {
                            if !args.is_empty() {
                                return Err(KainError::codegen("is_ok expects no arguments", *span));
                            }
                            let result = self.compile_tagged_value_is_tag(
                                &obj_val,
                                &[ABI_TAG_RESULT_OK_LLVM],
                                false,
                            );
                            self.emit_release_if_new_object_expr(receiver, &obj_val, &obj_ty);
                            return Ok((result, "i1".to_string()));
                        }
                        "is_err" => {
                            if !args.is_empty() {
                                return Err(KainError::codegen("is_err expects no arguments", *span));
                            }
                            let result = self.compile_tagged_value_is_tag(
                                &obj_val,
                                &[ABI_TAG_RESULT_ERR_LLVM],
                                false,
                            );
                            self.emit_release_if_new_object_expr(receiver, &obj_val, &obj_ty);
                            return Ok((result, "i1".to_string()));
                        }
                        "ok" => {
                            if !args.is_empty() {
                                return Err(KainError::codegen("Result.ok expects no arguments", *span));
                            }
                            let is_ok = self.compile_tagged_value_is_tag(
                                &obj_val,
                                &[ABI_TAG_RESULT_OK_LLVM],
                                false,
                            );
                            let ok_label = self.next_label();
                            let none_label = self.next_label();
                            let merge_label = self.next_label();
                            self.emit(&format!(
                                "  br i1 {}, label %{}, label %{}",
                                is_ok, ok_label, none_label
                            ));

                            self.emit_label(&none_label);
                            let none_value = ("null".to_string(), "i8*".to_string());
                            let none_block = self.current_block.clone();
                            self.emit(&format!("  br label %{}", merge_label));

                            self.emit_label(&ok_label);
                            let (payload, payload_ty) =
                                self.compile_tagged_value_payload_copy(&obj_val, "i8*");
                            let some_value = self.compile_tagged_box_from_value(
                                ABI_TAG_OPTION_SOME_LLVM,
                                &payload,
                                &payload_ty,
                                8,
                            )?;
                            let some_block = self.current_block.clone();
                            self.emit(&format!("  br label %{}", merge_label));

                            self.emit_label(&merge_label);
                            let result = self.next_reg();
                            self.emit(&format!(
                                "  {} = phi i8* [ {}, %{} ], [ {}, %{} ]",
                                result,
                                none_value.0,
                                none_block,
                                some_value.0,
                                some_block
                            ));
                            self.emit_release_if_new_object_expr(receiver, &obj_val, &obj_ty);
                            return Ok((result, "i8*".to_string()));
                        }
                        "unwrap" | "expect" => {
                            if method == "expect" && args.len() != 1 {
                                return Err(KainError::codegen(
                                    "expect expects exactly one message argument",
                                    *span,
                                ));
                            }
                            if method == "unwrap" && !args.is_empty() {
                                return Err(KainError::codegen("unwrap expects no arguments", *span));
                            }
                            let result = self.compile_tagged_value_payload_copy(&obj_val, "i64");
                            self.emit_release_if_new_object_expr(receiver, &obj_val, &obj_ty);
                            return Ok(result);
                        }
                        _ => {}
                    }
                }

                if obj_ty.starts_with("%") {
                    let struct_name = obj_ty
                        .trim_start_matches('%')
                        .trim_end_matches('*');
                    let func_name = format!("{}_{}", struct_name, method);

                    if self.functions.contains_key(&func_name) {
                        let mut compiled_args = Vec::new();
                        let mut arg_types = Vec::new();
                        let self_arg_ty = format!("%{}*", struct_name);
                        let self_arg_val = if obj_ty.ends_with('*') {
                            obj_val
                        } else {
                            let receiver_addr = self.next_reg();
                            self.emit_entry_alloca(&receiver_addr, &obj_ty);
                            self.emit(&format!(
                                "  store {} {}, {}* {}",
                                obj_ty, obj_val, obj_ty, receiver_addr
                            ));
                            receiver_addr
                        };

                        // Pass 'self' as first argument
                        compiled_args.push(self_arg_val);
                        arg_types.push(self_arg_ty);

                        for arg in args {
                            let (val, ty) = self.compile_expr(&arg.value)?;
                            compiled_args.push(val);
                            arg_types.push(ty);
                        }

                        let ret_ty = self.functions.get(&func_name).unwrap().clone();
                        let res = self.next_reg();

                        let arg_str = compiled_args
                            .iter()
                            .zip(arg_types.iter())
                            .map(|(val, ty)| format!("{} {}", ty, val))
                            .collect::<Vec<_>>()
                            .join(", ");

                        if ret_ty == "void" {
                            self.emit(&format!("  call void @{}({})", func_name, arg_str));
                            return Ok(("0".into(), "i64".into()));
                        }

                        self.emit(&format!(
                            "  {} = call {} @{}({})",
                            res, ret_ty, func_name, arg_str
                        ));
                        return Ok((res, ret_ty));
                    }
                }

                return Err(KainError::codegen(
                    format!("Method {} not found on type {}", method, obj_ty),
                    *span,
                ));
            }
            Expr::Call { callee, args, span } => {
                if let Expr::Ident(name, _) = callee.as_ref() {
                    if let Some(result) = self.compile_lowered_helper_call(name, args, *span) {
                        return result;
                    }
                    if let Some(result) =
                        self.compile_actor_builtin_ask(name, args, *span, None)?
                    {
                        return Ok(result);
                    }
                    if let Some(result) =
                        self.compile_native_variant_function_call(name, args, *span)?
                    {
                        return Ok(result);
                    }
                }

                // Handle print intrinsic
                if let Expr::Ident(name, _) = callee.as_ref() {
                    if (name == "to_string" || name == "str") && args.len() == 1 {
                        let (val, ty) = self.compile_expr(&args[0].value)?;
                        return self.stringify_value(&val, &ty);
                    }

                    if name == "now" {
                        let res = self.next_reg();
                        self.emit(&format!("  {} = call i64 @clock_wrapper()", res));
                        return Ok((res, "i64".into()));
                    }

                    if name == "print" || name == "println" {
                        return self.compile_stdout_write_call(name, args, *span);
                    }
                }

                // Normal call - extract function name
                let func_name = match callee.as_ref() {
                    Expr::Ident(name, _) => name.clone(),
                    _ => {
                        return Err(KainError::codegen(
                            "Only direct function calls supported",
                            *span,
                        ))
                    }
                };
                self.compile_direct_call(&func_name, args)
            }
            Expr::StageCall {
                runtime,
                function,
                args,
                ..
            } => self.compile_stage_call(runtime, function, args),
            Expr::EnumVariant {
                enum_name,
                variant,
                fields,
                span,
            } => {
                if let Some(result) =
                    self.compile_native_option_or_result_variant(enum_name, variant, fields, *span)?
                {
                    return Ok(result);
                }

                let struct_ty = format!("%{}", enum_name);
                let ptr_ty = format!("{}*", struct_ty);

                // Allocate Enum struct
                let null_ptr = format!("{} null", ptr_ty);
                let size_ptr_reg = self.next_reg();
                self.emit(&format!(
                    "  {} = getelementptr {}, {}, i32 1",
                    size_ptr_reg, struct_ty, null_ptr
                ));
                let size_reg = self.next_reg();
                self.emit(&format!(
                    "  {} = ptrtoint {} {} to i64",
                    size_reg, ptr_ty, size_ptr_reg
                ));

                let mem_reg = self.next_reg();
                self.emit(&format!(
                    "  {} = call i8* @KAIN_alloc(i64 {})",
                    mem_reg, size_reg
                ));

                let enum_ptr = self.next_reg();
                self.emit(&format!(
                    "  {} = bitcast i8* {} to {}",
                    enum_ptr, mem_reg, ptr_ty
                ));
                let dtor_name = format!("dtor_{}", enum_name);
                self.emit(&format!(
                    "  call void @KAIN_set_destructor(i8* {}, void (i8*)* @{})",
                    mem_reg, dtor_name
                ));

                // Store Tag
                let tag = self.hash_message_tag(enum_name, variant);
                let tag_ptr = self.next_reg();
                self.emit(&format!(
                    "  {} = getelementptr inbounds {}, {} {}, i32 0, i32 0",
                    tag_ptr, struct_ty, ptr_ty, enum_ptr
                ));
                self.emit(&format!("  store i64 {}, i64* {}", tag, tag_ptr));

                // Handle Payload
                let payload_struct_name = format!("{}_{}", enum_name, variant);
                let payload_ty = format!("%{}", payload_struct_name);
                let payload_ptr_ty = format!("{}*", payload_ty);

                // Check if payload struct exists (implies non-empty payload)
                if self.struct_defs.contains_key(&payload_struct_name) {
                    // Allocate Payload
                    let p_null_ptr = format!("{} null", payload_ptr_ty);
                    let p_size_ptr = self.next_reg();
                    self.emit(&format!(
                        "  {} = getelementptr {}, {}, i32 1",
                        p_size_ptr, payload_ty, p_null_ptr
                    ));
                    let p_size = self.next_reg();
                    self.emit(&format!(
                        "  {} = ptrtoint {} {} to i64",
                        p_size, payload_ptr_ty, p_size_ptr
                    ));

                    let p_mem = self.next_reg();
                    self.emit(&format!(
                        "  {} = call i8* @KAIN_alloc(i64 {})",
                        p_mem, p_size
                    ));

                    let p_ptr = self.next_reg();
                    self.emit(&format!(
                        "  {} = bitcast i8* {} to {}",
                        p_ptr, p_mem, payload_ptr_ty
                    ));

                    // Store Fields
                    match fields {
                        kain_core::ast::EnumVariantFields::Tuple(exprs) => {
                            for (i, expr) in exprs.iter().enumerate() {
                                let (val, val_ty) = self.compile_expr(expr)?;
                                let field_ptr = self.next_reg();
                                self.emit(&format!(
                                    "  {} = getelementptr inbounds {}, {} {}, i32 0, i32 {}",
                                    field_ptr, payload_ty, payload_ptr_ty, p_ptr, i
                                ));
                                self.emit(&format!(
                                    "  store {} {}, {}* {}",
                                    val_ty, val, val_ty, field_ptr
                                ));
                            }
                        }
                        kain_core::ast::EnumVariantFields::Struct(named_fields) => {
                            for (i, (_, expr)) in named_fields.iter().enumerate() {
                                let (val, val_ty) = self.compile_expr(expr)?;
                                let field_ptr = self.next_reg();
                                self.emit(&format!(
                                    "  {} = getelementptr inbounds {}, {} {}, i32 0, i32 {}",
                                    field_ptr, payload_ty, payload_ptr_ty, p_ptr, i
                                ));
                                self.emit(&format!(
                                    "  store {} {}, {}* {}",
                                    val_ty, val, val_ty, field_ptr
                                ));
                            }
                        }
                        _ => {}
                    }

                    // Store Payload Pointer in Enum
                    let payload_ptr_ptr = self.next_reg();
                    self.emit(&format!(
                        "  {} = getelementptr inbounds {}, {} {}, i32 0, i32 1",
                        payload_ptr_ptr, struct_ty, ptr_ty, enum_ptr
                    ));
                    self.emit(&format!("  store i8* {}, i8** {}", p_mem, payload_ptr_ptr));
                } else {
                    // Store Null
                    let payload_ptr_ptr = self.next_reg();
                    self.emit(&format!(
                        "  {} = getelementptr inbounds {}, {} {}, i32 0, i32 1",
                        payload_ptr_ptr, struct_ty, ptr_ty, enum_ptr
                    ));
                    self.emit(&format!("  store i8* null, i8** {}", payload_ptr_ptr));
                }

                Ok((enum_ptr, ptr_ty))
            }
            Expr::Match {
                scrutinee,
                arms,
                span: _,
            } => {
                let (val, val_ty) = self.compile_expr(scrutinee)?;
                let enum_name = if val_ty.starts_with('%') && val_ty.ends_with('*') {
                    Some(
                        val_ty
                            .trim_start_matches('%')
                            .trim_end_matches('*')
                            .to_string(),
                    )
                } else {
                    None
                };

                let label_end = self.next_label();
                let label_no_match = self.next_label();
                let mut condition_labels = Vec::new();
                let mut arm_labels = Vec::new();
                for _ in 0..arms.len() {
                    condition_labels.push(self.next_label());
                    arm_labels.push(self.next_label());
                }

                if arms.is_empty() {
                    self.emit(&format!("  br label %{}", label_no_match));
                } else {
                    self.emit(&format!("  br label %{}", condition_labels[0]));
                }

                let mut incoming = Vec::new();

                for (i, arm) in arms.iter().enumerate() {
                    self.emit_label(&condition_labels[i]);
                    let cond = self.compile_pattern_condition(
                        &arm.pattern,
                        &val,
                        &val_ty,
                        enum_name.as_deref(),
                        arm.span,
                    )?;

                    let branch_true = arm_labels[i].clone();
                    let branch_false = if i + 1 < arms.len() {
                        condition_labels[i + 1].clone()
                    } else {
                        label_no_match.clone()
                    };
                    self.emit(&format!(
                        "  br i1 {}, label %{}, label %{}",
                        cond, branch_true, branch_false
                    ));

                    self.emit_label(&arm_labels[i]);
                    self.scopes.push(Vec::new());

                    self.bind_match_pattern(
                        &arm.pattern,
                        &val,
                        &val_ty,
                        enum_name.as_deref(),
                        arm.span,
                    )?;

                    let bound_vars = self.scopes.last().cloned().unwrap_or_default();

                    if let Some(guard) = &arm.guard {
                        let (guard_val, guard_ty) = self.compile_expr(guard)?;
                        if guard_ty != "i1" {
                            return Err(KainError::codegen(
                                format!("Match guard must compile to bool/i1, got {}", guard_ty),
                                arm.span,
                            ));
                        }
                        let guard_pass = self.next_label();
                        let guard_fail = self.next_label();
                        let guard_false_target = if i + 1 < arms.len() {
                            condition_labels[i + 1].clone()
                        } else {
                            label_no_match.clone()
                        };
                        self.emit(&format!(
                            "  br i1 {}, label %{}, label %{}",
                            guard_val, guard_pass, guard_fail
                        ));
                        self.emit_label(&guard_fail);
                        self.emit_scope_cleanup_for_vars(&bound_vars);
                        self.emit(&format!("  br label %{}", guard_false_target));
                        self.emit_label(&guard_pass);
                    }

                    let (res_val, res_ty) = self.compile_expr(&arm.body)?;
                    let arm_end_block = self.current_block.clone();

                    self.emit_scope_exit();
                    self.emit(&format!("  br label %{}", label_end));
                    incoming.push((res_val, res_ty, arm_end_block));
                }

                let res_ty = if let Some((_, ty, _)) = incoming.first() {
                    let res_ty = ty.clone();
                    let consistent = incoming.iter().all(|(_, candidate_ty, _)| *candidate_ty == res_ty);
                    if !consistent {
                        return Err(KainError::codegen(
                            "LLVM match arms produced inconsistent result types",
                            scrutinee.span(),
                        ));
                    }
                    Some(res_ty)
                } else {
                    None
                };

                self.emit_label(&label_no_match);
                if let Some(res_ty) = res_ty.clone() {
                    if let Some(default_value) = self.match_fallback_value_for_type(&res_ty) {
                        incoming.push((default_value, res_ty, label_no_match.clone()));
                        self.emit(&format!("  br label %{}", label_end));
                    } else {
                        self.emit("  unreachable");
                    }
                } else {
                    incoming.push(("0".into(), "i64".into(), label_no_match.clone()));
                    self.emit(&format!("  br label %{}", label_end));
                }

                self.emit_label(&label_end);

                // Phi
                if incoming.is_empty() {
                    Ok(("0".into(), "i64".into()))
                } else {
                    let res_ty = incoming[0].1.clone();
                    let res_reg = self.next_reg();

                    let phi_args = incoming
                        .iter()
                        .map(|(val, _, block)| format!("[ {}, %{} ]", val, block))
                        .collect::<Vec<_>>()
                        .join(", ");

                    self.emit(&format!("  {} = phi {} {}", res_reg, res_ty, phi_args));
                    Ok((res_reg, res_ty))
                }
            }
            // Catch-all for unsupported expressions
            other => Err(KainError::codegen(
                format!("Unsupported LLVM expression: {:?}", other),
                other.span(),
            )),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::{generate, runtime_symbol_for_stdlib_function};
    use kain_core::diagnostics::SpanMapper;
    use kain_core::lexer::Lexer;
    use kain_core::parser::Parser;
    use kain_core::types;

    #[test]
    fn remaps_rounding_builtins_to_runtime_wrappers() {
        assert_eq!(
            runtime_symbol_for_stdlib_function("floor"),
            "kain_floor_i64"
        );
        assert_eq!(runtime_symbol_for_stdlib_function("ceil"), "kain_ceil_i64");
        assert_eq!(
            runtime_symbol_for_stdlib_function("round"),
            "kain_round_i64"
        );
        assert_eq!(runtime_symbol_for_stdlib_function("sqrt"), "sqrt");
    }

    #[test]
    fn lowers_extern_cffi_declarations_without_void_parameters() {
        let source = r#"
@extern fn piano_audio_status(arg1: Void) -> String
@extern fn piano_audio_note_on(midi_note: Int) -> Int

fn main() -> Int:
    let status = piano_audio_status(())
    return piano_audio_note_on(60)
"#;
        let tokens = Lexer::new(source).tokenize().expect("tokens");
        let mapper = SpanMapper::new(source);
        let ast = Parser::new(&tokens, &mapper, "<llvm-extern-test>")
            .parse()
            .expect("parse");
        let typed = types::check(&ast, &mapper, "<llvm-extern-test>").expect("typecheck");
        let llvm = String::from_utf8(generate(&typed).expect("llvm generation"))
            .expect("utf8 llvm output");

        assert!(llvm.contains("declare i8* @piano_audio_status()"));
        assert!(llvm.contains("declare i64 @piano_audio_note_on(i64 %arg0)"));
        assert!(llvm.contains("call i8* @piano_audio_status()"));
        assert!(llvm.contains("call i64 @piano_audio_note_on(i64 60)"));
    }

    #[test]
    fn lowers_extern_cffi_declarations_inside_generated_modules() {
        let source = r#"
mod c:
    mod piano_audio:
        @extern fn piano_audio_status(arg1: Void) -> String
        @extern fn piano_audio_note_on(midi_note: Int) -> Int

use c::piano_audio

fn main() -> Int:
    let status = piano_audio_status(())
    return piano_audio_note_on(60)
"#;
        let tokens = Lexer::new(source).tokenize().expect("tokens");
        let mapper = SpanMapper::new(source);
        let ast = Parser::new(&tokens, &mapper, "<llvm-extern-module-test>")
            .parse()
            .expect("parse");
        let typed = types::check(&ast, &mapper, "<llvm-extern-module-test>").expect("typecheck");
        let llvm = String::from_utf8(generate(&typed).expect("llvm generation"))
            .expect("utf8 llvm output");

        assert!(llvm.contains("declare i8* @piano_audio_status()"));
        assert!(llvm.contains("declare i64 @piano_audio_note_on(i64 %arg0)"));
        assert!(llvm.contains("call i8* @piano_audio_status()"));
        assert!(llvm.contains("call i64 @piano_audio_note_on(i64 60)"));
    }

    #[test]
    fn lowers_impl_self_builder_methods_without_extra_self_parameter() {
        let source = r#"
struct ButtonBuilder:
    label: String
    key: String

impl ButtonBuilder:
    fn key(_self: Self_, key: String) -> Self_:
        return ButtonBuilder { label: _self.label, key: key }

fn main() -> Int:
    let b = ButtonBuilder { label: "Save", key: "" }.key("save")
    return len(b.key)
"#;
        let tokens = Lexer::new(source).tokenize().expect("tokens");
        let mapper = SpanMapper::new(source);
        let ast = Parser::new(&tokens, &mapper, "<llvm-impl-self-builder-test>")
            .parse()
            .expect("parse");
        let typed =
            types::check(&ast, &mapper, "<llvm-impl-self-builder-test>").expect("typecheck");
        let llvm = String::from_utf8(generate(&typed).expect("llvm generation"))
            .expect("utf8 llvm output");

        assert!(llvm.contains(
            "define %ButtonBuilder* @ButtonBuilder_key(%ButtonBuilder* %arg0, i8* %arg1)"
        ));
        assert!(llvm.contains("call %ButtonBuilder* @ButtonBuilder_key(%ButtonBuilder*"));
        assert!(!llvm.contains("@ButtonBuilder_key(%ButtonBuilder* %arg0, %ButtonBuilder* %arg1"));
    }

    #[test]
    fn retains_borrowed_string_arguments_before_non_extern_calls() {
        let source = r#"
fn sink(path: String) -> Int:
    return 0

fn main() -> Int:
    let dir = "root"
    let status = sink(dir)
    if dir == "root":
        return status
    return 1
"#;
        let tokens = Lexer::new(source).tokenize().expect("tokens");
        let mapper = SpanMapper::new(source);
        let ast = Parser::new(&tokens, &mapper, "<llvm-owned-call-arg-test>")
            .parse()
            .expect("parse");
        let typed = types::check(&ast, &mapper, "<llvm-owned-call-arg-test>").expect("typecheck");
        let llvm = String::from_utf8(generate(&typed).expect("llvm generation"))
            .expect("utf8 llvm output");
        let call_index = llvm
            .find("call i64 @sink(i8*")
            .expect("sink call should be present in LLVM");
        let window_start = call_index.saturating_sub(160);
        let window = &llvm[window_start..call_index];

        assert!(window.contains("call void @rc_retain(i8*"));
    }
}
