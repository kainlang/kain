//! Component surface codegen — emits LLVM IR that calls through the
//! `KainComponentSurface` trait vtable. All element creation, attribute
//! setting, state persistence, and frame lifecycle operations go through
//! indirect vtable calls — never direct `abi_ui_*` function calls.
//!
//! See `X:/research/component/WIRING_CONTRACT.md` for the full contract.
//! See `X:/runtime/native/include/component_surface.h` for the C trait layout.

use kain_core::ast::{Expr, JSXAttrValue, JSXAttribute, JSXNode};
use kain_core::error::{KainError, KainResult};
use kain_core::types::{ResolvedType, TypedComponent};
use super::LlvmGenerator;

// ── Vtable offset constants — must match KainComponentSurface field order ────
// See: runtime/native/include/component_surface.h
const OFF_SESSION_CREATE: u32 = 0;
const OFF_SESSION_DESTROY: u32 = 1;
const OFF_ELEMENT_BEGIN: u32 = 2;
const OFF_ELEMENT_END: u32 = 3;
const OFF_ELEMENT_SET_TEXT: u32 = 4;
const OFF_ELEMENT_SET_ATTR_I64: u32 = 5;
const OFF_ELEMENT_SET_ATTR_F64: u32 = 6;
const OFF_ELEMENT_SET_ATTR_STRING: u32 = 7;
const OFF_STATE_GET_I64: u32 = 8;
const OFF_STATE_SET_I64: u32 = 9;
const OFF_BEGIN_FRAME: u32 = 10;
const OFF_END_FRAME: u32 = 11;
const OFF_PRESENT: u32 = 12;
const OFF_POLL_EVENT: u32 = 13;
const OFF_SHOULD_CLOSE: u32 = 14;
const OFF_WINDOW_OPEN: u32 = 15;
const OFF_HOST_PUMP: u32 = 16;
const OFF_SESSION_ATTACH_PLATFORM: u32 = 17;
/// Slot 18: get_gpu_extension - returns KainGpuSurfaceExtension* or NULL
pub(crate) const OFF_GET_GPU_EXTENSION: u32 = 18;
/// Slot 19: state_get_f64 - read double-precision float state from session
pub(crate) const OFF_STATE_GET_F64: u32 = 19;
/// Slot 20: state_set_f64 - write double-precision float state to session
pub(crate) const OFF_STATE_SET_F64: u32 = 20;
/// Slot 21: state_get_string - read string state from session
pub(crate) const OFF_STATE_GET_STRING: u32 = 21;
/// Slot 22: state_set_string - write string state to session
pub(crate) const OFF_STATE_SET_STRING: u32 = 22;
/// Slot 23: element_set_callback - register event callback on an element
pub(crate) const OFF_ELEMENT_SET_CALLBACK: u32 = 23;

// ── JSX attribute → surface call mapping (Contract 11) ──────────────────
struct AttrMapping {
    vtable_offset: u32,
    fn_ptr_ty: &'static str,
    style_key: &'static str,
}

fn map_jsx_attr_to_surface_key(attr_name: &str) -> AttrMapping {
    match attr_name {
        // ── Existing: numeric (f64) attrs ────────────────────
        "padding" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_F64, fn_ptr_ty: "void (i64, i64, i8*, double)*", style_key: "padding" },
        "spacing" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_F64, fn_ptr_ty: "void (i64, i64, i8*, double)*", style_key: "spacing" },
        "corner_radius" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_F64, fn_ptr_ty: "void (i64, i64, i8*, double)*", style_key: "corner_radius" },
        "radius" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_F64, fn_ptr_ty: "void (i64, i64, i8*, double)*", style_key: "radius" },
        "font_size" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_F64, fn_ptr_ty: "void (i64, i64, i8*, double)*", style_key: "font_size" },
        "opacity" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_F64, fn_ptr_ty: "void (i64, i64, i8*, double)*", style_key: "opacity" },
        "border" | "border_width" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_F64, fn_ptr_ty: "void (i64, i64, i8*, double)*", style_key: "border_width" },
        "stroke_width" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_F64, fn_ptr_ty: "void (i64, i64, i8*, double)*", style_key: "border_width" },
        "width" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_F64, fn_ptr_ty: "void (i64, i64, i8*, double)*", style_key: "width" },
        "height" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_F64, fn_ptr_ty: "void (i64, i64, i8*, double)*", style_key: "height" },
        "min" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_F64, fn_ptr_ty: "void (i64, i64, i8*, double)*", style_key: "min" },
        "max" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_F64, fn_ptr_ty: "void (i64, i64, i8*, double)*", style_key: "max" },
        "step" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_F64, fn_ptr_ty: "void (i64, i64, i8*, double)*", style_key: "step" },

        // ── Existing: string attrs ───────────────────────────
        "background" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_STRING, fn_ptr_ty: "void (i64, i64, i8*, i8*)*", style_key: "fill_color" },
        "fill" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_STRING, fn_ptr_ty: "void (i64, i64, i8*, i8*)*", style_key: "fill_color" },
        "border_color" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_STRING, fn_ptr_ty: "void (i64, i64, i8*, i8*)*", style_key: "border_color" },
        "stroke" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_STRING, fn_ptr_ty: "void (i64, i64, i8*, i8*)*", style_key: "border_color" },
        "color" | "ink_color" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_STRING, fn_ptr_ty: "void (i64, i64, i8*, i8*)*", style_key: "ink_color" },
        "title" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_STRING, fn_ptr_ty: "void (i64, i64, i8*, i8*)*", style_key: "title" },
        "variant" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_STRING, fn_ptr_ty: "void (i64, i64, i8*, i8*)*", style_key: "variant" },
        "role" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_STRING, fn_ptr_ty: "void (i64, i64, i8*, i8*)*", style_key: "role" },
        "align" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_STRING, fn_ptr_ty: "void (i64, i64, i8*, i8*)*", style_key: "align" },
        "font_family" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_STRING, fn_ptr_ty: "void (i64, i64, i8*, i8*)*", style_key: "font_family" },
        "distribution" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_STRING, fn_ptr_ty: "void (i64, i64, i8*, i8*)*", style_key: "layout.distribution" },
        "axis" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_STRING, fn_ptr_ty: "void (i64, i64, i8*, i8*)*", style_key: "axis" },
        "placeholder" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_STRING, fn_ptr_ty: "void (i64, i64, i8*, i8*)*", style_key: "placeholder" },
        "tooltip" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_STRING, fn_ptr_ty: "void (i64, i64, i8*, i8*)*", style_key: "tooltip" },

        // ── Existing: i64 attrs ──────────────────────────────
        "direction" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_I64, fn_ptr_ty: "void (i64, i64, i8*, i64)*", style_key: "layout.direction" },
        "disabled" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_I64, fn_ptr_ty: "void (i64, i64, i8*, i64)*", style_key: "disabled" },
        "checked" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_I64, fn_ptr_ty: "void (i64, i64, i8*, i64)*", style_key: "checked" },
        "selected" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_I64, fn_ptr_ty: "void (i64, i64, i8*, i64)*", style_key: "selected" },
        "tab_index" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_I64, fn_ptr_ty: "void (i64, i64, i8*, i64)*", style_key: "tab_index" },
        "weight" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_I64, fn_ptr_ty: "void (i64, i64, i8*, i64)*", style_key: "weight" },

        // ── Existing: text attrs ─────────────────────────────
        "value" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_TEXT, fn_ptr_ty: "void (i64, i64, i8*)*", style_key: "" },

        // Unknown attributes pass through as strings with the attr name as the key
        _ => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_STRING, fn_ptr_ty: "void (i64, i64, i8*, i8*)*", style_key: "" },
    }
}

// ── State field tracking for type-generic persistence ───────
enum StateFieldType {
    I64,
    F64,
    String,
}

impl StateFieldType {
    fn from_resolved(ty: &ResolvedType) -> Self {
        match ty {
            ResolvedType::Float(_) => StateFieldType::F64,
            ResolvedType::String | ResolvedType::Char => StateFieldType::String,
            _ => StateFieldType::I64,
        }
    }
}

// ── Compile-time SPIR-V compilation gate ─────────────────
/// Returns true if the surface kind requires SPIR-V hex compilation.
/// This is a compile-time concern — the runtime vtable doesn't exist yet,
/// so slot 18 (`get_gpu_extension != NULL`) cannot answer this question.
///
/// This helper bridges the gap: it tells `collect_shader_spirv_hexes`
/// which surfaces need shader compilation, based on the surface kind
/// string. Extend this list when new GPU-capable surface kinds are
/// registered. Future: a `with shader_compile` annotation on the surface
/// declaration would eliminate this compile-time registry entirely.
pub(crate) fn surface_needs_shader_compilation(surface_kind: &str) -> bool {
    // Known shader-capable surface kinds.
    // Future: this could be driven by a build-time metadata file.
    matches!(surface_kind, "shader_canvas")
}

impl LlvmGenerator {
    // =====================================================================
    //  Public entry points — called from `mod.rs`
    // =====================================================================

    /// Emit declarations for the `KainComponentSurface` opaque struct type
    /// and registry/support functions. Call once per module before any
    /// component code.
    ///
    /// Does NOT declare any direct `abi_ui_*` functions — all surface
    /// operations go through the vtable.
    pub(crate) fn declare_surface_trait_types(&mut self) {
        if self.surface_trait_declared {
            return;
        }
        self.surface_trait_declared = true;

        // Sized trait type with 24 pointer-sized fields (one per vtable slot).
        // The exact function pointer types differ per slot; we use i8* as a
        // uniform placeholder and bitcast before loading the real fn pointer.
        self.emit("%KainComponentSurface = type { i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8* }");

        // Registry — resolve a named surface backend
        self.emit("%KainGpuSurfaceExtension = type { i8*, i8* }");
        self.emit("declare %KainComponentSurface* @kain_component_surface_resolve(i8*)");

        // Runtime panic — for surface resolution / session failures
        self.emit("declare void @kain_runtime_panic(i8*)");

        // Frame delta — high-resolution timer
        self.emit("declare double @__kain_frame_delta_ms()");

        // Callback function pointer type — void fn that receives session_id,
        // element_id, and an opaque event data pointer (i8*).
        self.emit("%KainComponentCallback = type void (i64, i64, i8*)*");

        // Note: @str_concat and @to_string are declared by the main module preamble.
        // They are NOT re-declared here to avoid duplicate-definition linker errors.
    }

    /// Compile a component as `void @Name_render(%KainComponentSurface* %surface, i64 %session_id, i64 %parent_id, props...)`.
    ///
    /// The `%surface` pointer is threaded through all render functions so
    /// every JSX node can call through the vtable.
    pub(crate) fn compile_component_render(
        &mut self,
        component: &TypedComponent,
    ) -> KainResult<()> {
        self.declare_surface_trait_types();

        self.reg_count = 0;
        self.locals.clear();
        self.ssa_locals.clear();
        self.authored_pointer_locals.clear();
        self.helper_owned_pointer_locals.clear();
        self.shattered_array_locals.clear();
        self.fixed_array_locals.clear();
        self.sealed_literal_map_locals.clear();
        self.borrowed_locals.clear();
        self.json_handle_locals.clear();
        self.json_passthrough_locals.clear();
        self.runtime_any_passthrough_locals.clear();
        self.string_locals.clear();
        self.runtime_array_locals.clear();
        self.string_length_values.clear();
        self.pooled_string_literal_slots.clear();
        self.scopes.clear();
        self.scopes.push(Vec::new());
        self.const_init_blocks.clear();
        self.entry_alloca_insert_offset = None;
        self.entry_preamble_insert_offset = None;
        self.entry_hoisted_const_inits.clear();

        let name = &component.ast.name;
        let render_name = format!("{}_render", name);

        // Build param type list:
        //   1. %KainComponentSurface* %surface   (arg0)
        //   2. i64 %session_id                    (arg1)
        //   3. i64 %parent_id                     (arg2)
        //   4..N props in declaration order
        let param_defs: Vec<(String, String)> = {
            let mut defs = Vec::new();
            defs.push(("surface".to_string(), "%KainComponentSurface*".to_string()));
            defs.push(("session_id".to_string(), "i64".to_string()));
            defs.push(("parent_id".to_string(), "i64".to_string()));
            for prop in &component.ast.props {
                let ty = component
                    .prop_types
                    .get(&prop.name)
                    .map(|ty| self.map_type(ty))
                    .unwrap_or_else(|| self.map_type_from_ast(&prop.ty));
                defs.push((prop.name.clone(), ty));
            }
            defs
        };

        let param_str = param_defs
            .iter()
            .enumerate()
            .map(|(i, (_, ty))| format!("{} %arg{}", ty, i))
            .collect::<Vec<_>>()
            .join(", ");

        // Emit stub pulse/resonate handler functions BEFORE the render function.
        // These handlers are referenced by kain_machine_pulse_start/abi_resonate_register
        // during registration. Top-level pulse/resonate codegen in mod.rs doesn't know
        // about component-inline pulses, so we emit minimal stubs here.
        for pulse in &component.pulse_types {
            let sym = Self::sanitize_symbol_fragment(&pulse.ast.name);
            self.emit(&format!("define internal void @__kain_pulse_fire_{}() {{ ret void }}", sym));
        }
        for resonate in &component.resonate_types {
            let sym = Self::sanitize_symbol_fragment(&resonate.ast.name);
            self.emit(&format!("define internal void @__kain_resonate_{}() {{ ret void }}", sym));
        }

        self.emit(&format!("define void @{}({}) {{", render_name, param_str));
        self.emit_label("entry");

        let surface_reg = "%arg0".to_string();
        let session_reg = "%arg1".to_string();
        let parent_reg = "%arg2".to_string();

        // Store prop params in locals (skipping surface/session/parent at indices 0,1,2)
        for (i, (param_name, param_ty)) in param_defs.iter().enumerate().skip(3) {
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

        // Compile state fields (Contract 8) — returns (key, addr_reg, state_ty) for write-back
        let state_fields = self.compile_component_state_init(component, &surface_reg, &session_reg)?;

        // ── Emit component-internal pulse/resonate registration ──
        // One-time guard: register pulses and resonates on first render,
        // skip on subsequent frames (idempotent at the runtime level).
        if !component.pulse_types.is_empty() || !component.resonate_types.is_empty() {
            self.emit_component_pulse_resonate_registration(
                component, &surface_reg, &session_reg,
            )?;
        }

        // Set current component context for JSX compilation
        self.current_component_name = Some(name.clone());
        self.current_component_methods = Some(component.ast.methods.clone());
        self.current_component_session = Some(session_reg.clone());
        self.current_component_parent = Some(parent_reg.clone());

        // Compile the JSX body (Contracts 1-7)
        let mut sibling_index = 0usize;
        self.compile_jsx_to_surface(
            &surface_reg,
            &session_reg,
            &parent_reg,
            &component.ast.body,
            name,
            &mut sibling_index,
        )?;

        // Write-back: persist current state field values to the surface
        // so mutations (self.count = self.count + 1) survive across frames.
        // Dispatch to the correct vtable slot based on state field type.
        for (key, addr_reg, field_ty) in &state_fields {
            let key_str = self.compile_static_c_string_literal(key);
            match field_ty {
                StateFieldType::F64 => {
                    let load_reg = self.next_reg();
                    self.emit(&format!("  {} = load double, double* {}", load_reg, addr_reg));
                    self.emit_vtable_call_void(
                        &surface_reg,
                        OFF_STATE_SET_F64,
                        "void (i64, i8*, double)*",
                        &[(&session_reg, "i64"), (&key_str, "i8*"), (&load_reg, "double")],
                    );
                }
                StateFieldType::String => {
                    let load_reg = self.next_reg();
                    self.emit(&format!("  {} = load i8*, i8** {}", load_reg, addr_reg));
                    self.emit_vtable_call_void(
                        &surface_reg,
                        OFF_STATE_SET_STRING,
                        "void (i64, i8*, i8*)*",
                        &[(&session_reg, "i64"), (&key_str, "i8*"), (&load_reg, "i8*")],
                    );
                }
                StateFieldType::I64 => {
                    let load_reg = self.next_reg();
                    self.emit(&format!("  {} = load i64, i64* {}", load_reg, addr_reg));
                    self.emit_vtable_call_void(
                        &surface_reg,
                        OFF_STATE_SET_I64,
                        "void (i64, i8*, i64)*",
                        &[(&session_reg, "i64"), (&key_str, "i8*"), (&load_reg, "i64")],
                    );
                }
            }
        }

        // Clear component context
        self.current_component_name = None;
        self.current_component_methods = None;
        self.current_component_session = None;
        self.current_component_parent = None;

        self.emit("  ret void");
        self.emit("}");
        self.emit("");
        self.current_return_type = None;
        Ok(())
    }


    /// Emit a call to set_uniform through the GPU extension struct.
    fn emit_gpu_set_uniform(
        &mut self,
        ext_reg: &str,
        session_reg: &str,
        binding: u32,
        data_ptr: &str,
        data_ty: &str,
        size_bytes: u32,
    ) {
        // gep into extension struct at offset 1 -> set_uniform fn ptr
        let gep_reg = self.next_reg();
        self.emit(&format!(
            "  {} = getelementptr inbounds %KainGpuSurfaceExtension, %KainGpuSurfaceExtension* {}, i32 0, i32 1",
            gep_reg, ext_reg
        ));
        let cast_reg = self.next_reg();
        self.emit(&format!(
            "  {} = bitcast i8** {} to i64 (i64, i32, i8*, i64)**",
            cast_reg, gep_reg
        ));
        let fn_reg = self.next_reg();
        self.emit(&format!(
            "  {} = load i64 (i64, i32, i8*, i64)*, i64 (i64, i32, i8*, i64)** {}",
            fn_reg, cast_reg
        ));
        // Bitcast data pointer to i8*
        let data_i8 = self.next_reg();
        self.emit(&format!(
            "  {} = bitcast {}* {} to i8*",
            data_i8, data_ty, data_ptr
        ));
        let call_reg = self.next_reg();
        self.emit(&format!(
            "  {} = call i64 {}(i64 {}, i32 {}, i8* {}, i64 {})",
            call_reg, fn_reg, session_reg, binding, data_i8, size_bytes
        ));
    }

    /// Emit a world-surface frame loop for a world with a surface declaration.
    /// Called from `compile_world_initializer`.
    ///
    /// Resolves the `%KainComponentSurface*` from the registry, creates a
    /// session, then loops: begin_frame → render root component → end_frame
    /// → present → should_close. All surface ops go through the vtable.
    pub(crate) fn compile_surface_frame_loop(
        &mut self,
        world_name: &str,
        surface_kind: &str,
        root_component_name: &str,
        width: i64,
        height: i64,
    ) -> KainResult<()> {
        self.declare_surface_trait_types();

        // The runtime branch on vtable slot 18 (get_gpu_extension != NULL)
        // selects the GPU vs component render path. No compile-time string
        // matching needed — LLVM LTO constant-folds this at -O2.

        let fn_name = format!("__kain_world_surface_loop_{}", Self::sanitize_symbol_fragment(world_name));
        let render_name = format!("{}_render", root_component_name);

        self.emit(&format!("define void @{}() {{", fn_name));
        self.emit_label("entry");

        // ── Resolve surface ────────────────────────────────────────
        let surface_name_str = self.compile_static_c_string_literal(surface_kind);
        let surface_reg = self.next_reg();
        self.emit(&format!(
            "  {} = call %KainComponentSurface* @kain_component_surface_resolve(i8* {})",
            surface_reg, surface_name_str
        ));

        let is_null = self.next_reg();
        self.emit(&format!(
            "  {} = icmp eq %KainComponentSurface* {}, null",
            is_null, surface_reg
        ));
        let null_block = self.next_label();
        let init_block = self.next_label();
        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            is_null, null_block, init_block
        ));

        // Error: surface not registered
        self.emit_label(&null_block);
        let err_msg = format!("surface '{}' not registered for world '{}'", surface_kind, world_name);
        let err_str = self.compile_static_c_string_literal(&err_msg);
        self.emit(&format!("  call void @kain_runtime_panic(i8* {})", err_str));
        self.emit("  unreachable");

        // ── Create session (vtable offset 0) ──────────────────────
        self.emit_label(&init_block);
        let session_name_str = self.compile_static_c_string_literal(world_name);
        // Use component dimensions or fall back to 1280x720
        let dim_width = width.to_string();
        let dim_height = height.to_string();
        let session_id = self.emit_vtable_call(
            &surface_reg,
            OFF_SESSION_CREATE,
            "i64 (i8*, i64, i64)*",
            &[
                (&session_name_str, "i8*"),
                (&dim_width, "i64"),
                (&dim_height, "i64"),
            ],
        );

        let session_err = self.next_reg();
        self.emit(&format!(
            "  {} = icmp slt i64 {}, 0",
            session_err, session_id
        ));
        let session_fail = self.next_label();
        let window_init_label = self.next_label();
        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            session_err, session_fail, window_init_label
        ));

        self.emit_label(&session_fail);
        let fail_msg = format!("session_create failed for world '{}'", world_name);
        let fail_str = self.compile_static_c_string_literal(&fail_msg);
        self.emit(&format!("  call void @kain_runtime_panic(i8* {})", fail_str));
        self.emit("  unreachable");

        // ── Attach platform (vtable offset 17) ────────────────────
        // Allocate a zero-initialized 8-byte platform handle on the stack.
        // Backends that own their window (Vulkan, D3D12) create it here
        // when hwnd is NULL. Backends that receive a host-created window
        // (native_ui via winit) have it set by the host adapter.
        self.emit_label(&window_init_label);

        let handle_reg = self.next_reg();
        self.emit(&format!("  {} = alloca [8 x i8], align 8", handle_reg));
        self.emit(&format!("  call void @llvm.memset.p0i8.i64(i8* {}, i8 0, i64 8, i1 false)", handle_reg));
        self.emit_vtable_call_void(
            &surface_reg,
            OFF_SESSION_ATTACH_PLATFORM,
            "void (i64, i8*)*",
            &[(&session_id, "i64"), (&handle_reg, "i8*")],
        );

        // window_open (vtable offset 15) — flag session as open.
        // The OS window was created by native_ui_session_create which
        // auto-attaches the winit host adapter (RegisterClassA + CreateWindowExA).
        // The rendering intent comes from the Kain source: `surface native_ui => Component`.
        let window_title_str = self.compile_static_c_string_literal(world_name);
        let _window_ok = self.emit_vtable_call(
            &surface_reg,
            OFF_WINDOW_OPEN,
            "i64 (i64, i8*, i64, i64)*",
            &[
                (&session_id, "i64"),
                (&window_title_str, "i8*"),
                (&dim_width, "i64"),
                (&dim_height, "i64"),
            ],
        );

        // LLVM LTO constant-folds this branch at -O2 because the vtable
        // is a link-time constant global. Zero runtime cost.
        // ── Declare shared shutdown label ─────────────────────────
        let shutdown = self.next_label();

        // ── Probe vtable slot 18: get_gpu_extension ──────────────
        let ext_reg = self.emit_vtable_call(
            &surface_reg,
            OFF_GET_GPU_EXTENSION,
            "i8* (i64)*",
            &[(&session_id, "i64")],
        );
        
        let has_gpu = self.next_reg();
        self.emit(&format!(
            "  {} = icmp ne i8* {}, null",
            has_gpu, ext_reg
        ));
        let gpu_init_block = self.next_label();
        let component_init_block = self.next_label();
        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            has_gpu, gpu_init_block, component_init_block
        ));

        // ═══════════════════════════════════════════════════════════
        //  GPU shader path (slot 18 != NULL)
        // ═══════════════════════════════════════════════════════════
        self.emit_label(&gpu_init_block);

        // Bitcast the extension pointer to KainGpuSurfaceExtension*
        let ext_typed = self.next_reg();
        self.emit(&format!(
            "  {} = bitcast i8* {} to %KainGpuSurfaceExtension*",
            ext_typed, ext_reg
        ));

        // gep into extension struct at offset 0 -> load_shader fn ptr
        let load_gep = self.next_reg();
        self.emit(&format!(
            "  {} = getelementptr inbounds %KainGpuSurfaceExtension, %KainGpuSurfaceExtension* {}, i32 0, i32 0",
            load_gep, ext_typed
        ));
        let load_cast = self.next_reg();
        self.emit(&format!(
            "  {} = bitcast i8** {} to i64 (i64, i8*)**",
            load_cast, load_gep
        ));
        let load_fn = self.next_reg();
        self.emit(&format!(
            "  {} = load i64 (i64, i8*)*, i64 (i64, i8*)** {}",
            load_fn, load_cast
        ));

        // Embed SPIR-V hex from the global emitted in the codegen preamble
        let spirv_ptr = if let Some(hex) = self.shader_spirv_hexes.get(root_component_name) {
            let global_name = format!("@__kain_spirv_{}", root_component_name);
            let byte_len = hex.len() + 1; // +1 for null terminator
            let gep = self.next_reg();
            self.emit(&format!(
                "  {} = getelementptr inbounds [{} x i8], [{} x i8]* {}, i64 0, i64 0",
                gep, byte_len, byte_len, global_name
            ));
            gep
        } else {
            self.compile_static_c_string_literal("")
        };
        let _load_result = self.next_reg();
        self.emit(&format!(
            "  {} = call i64 {}(i64 {}, i8* {})",
            _load_result, load_fn, session_id, spirv_ptr
        ));

        // GPU frame loop
        let gpu_frame_loop_label = self.next_label();
        self.emit(&format!("  br label %{}", gpu_frame_loop_label));
        self.emit_label(&gpu_frame_loop_label);

        // Alloca for time accumulator (Float, 4 bytes)
        let time_addr = self.next_reg();
        self.emit(&format!("  {} = alloca float, align 4", time_addr));
        self.emit(&format!("  store float 0.0, float* {}", time_addr));

        // host_pump (vtable offset 16)
        let _gpu_pump = self.emit_vtable_call(
            &surface_reg,
            OFF_HOST_PUMP,
            "i64 (i64)*",
            &[(&session_id, "i64")],
        );

        // begin_frame (vtable offset 10)
        let gpu_delta = self.next_reg();
        self.emit(&format!(
            "  {} = call double @__kain_frame_delta_ms()",
            gpu_delta
        ));
        let gpu_delta_float = self.next_reg();
        self.emit(&format!(
            "  {} = fptrunc double {} to float",
            gpu_delta_float, gpu_delta
        ));
        self.emit_vtable_call_void(
            &surface_reg,
            OFF_BEGIN_FRAME,
            "void (i64, double)*",
            &[(&session_id, "i64"), (&gpu_delta, "double")],
        );

        // Binding 0: time (Float, 4 bytes)
        self.emit_gpu_set_uniform(
            &ext_typed, &session_id, 0, &time_addr, "float", 4,
        );

        // Binding 1: resolution (Vec2, 8 bytes)
        let res_addr = self.next_reg();
        self.emit(&format!("  {} = alloca [2 x float], align 4", res_addr));
        let res_x_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = getelementptr inbounds [2 x float], [2 x float]* {}, i32 0, i32 0",
            res_x_ptr, res_addr
        ));
        self.emit(&format!("  store float 1280.0, float* {}", res_x_ptr));
        let res_y_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = getelementptr inbounds [2 x float], [2 x float]* {}, i32 0, i32 1",
            res_y_ptr, res_addr
        ));
        self.emit(&format!("  store float 720.0, float* {}", res_y_ptr));
        self.emit_gpu_set_uniform(
            &ext_typed, &session_id, 1, &res_addr, "[2 x float]", 8,
        );

        // Binding 2: mouse (Vec2, 8 bytes) - zero-initialized
        let mouse_addr = self.next_reg();
        self.emit(&format!("  {} = alloca [2 x float], align 4", mouse_addr));
        self.emit(&format!(
            "  call void @llvm.memset.p0i8.i64(i8* {}, i8 0, i64 8, i1 false)",
            mouse_addr
        ));
        self.emit_gpu_set_uniform(
            &ext_typed, &session_id, 2, &mouse_addr, "[2 x float]", 8,
        );

        // end_frame (vtable offset 11)
        self.emit_vtable_call_void(
            &surface_reg,
            OFF_END_FRAME,
            "void (i64)*",
            &[(&session_id, "i64")],
        );

        // present (vtable offset 12)
        self.emit_vtable_call_void(
            &surface_reg,
            OFF_PRESENT,
            "void (i64)*",
            &[(&session_id, "i64")],
        );

        // Update time accumulator: time += delta (in seconds)
        let gpu_delta_sec = self.next_reg();
        self.emit(&format!(
            "  {} = fdiv float {}, 1.0e+3",
            gpu_delta_sec, gpu_delta_float
        ));
        let gpu_old_time = self.next_reg();
        self.emit(&format!("  {} = load float, float* {}", gpu_old_time, time_addr));
        let gpu_new_time = self.next_reg();
        self.emit(&format!(
            "  {} = fadd float {}, {}",
            gpu_new_time, gpu_old_time, gpu_delta_sec
        ));
        self.emit(&format!("  store float {}, float* {}", gpu_new_time, time_addr));

        // should_close (vtable offset 14)
        let gpu_close = self.emit_vtable_call(
            &surface_reg,
            OFF_SHOULD_CLOSE,
            "i64 (i64)*",
            &[(&session_id, "i64")],
        );
        let gpu_keep = self.next_reg();
        self.emit(&format!(
            "  {} = icmp eq i64 {}, 0",
            gpu_keep, gpu_close
        ));
        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            gpu_keep, gpu_frame_loop_label, shutdown
        ));

        // ═══════════════════════════════════════════════════════════
        //  Component render path (slot 18 == NULL)
        // ═══════════════════════════════════════════════════════════
        self.emit_label(&component_init_block);

        // ── Emit component-internal pulse registration ────────────
        let component_pulses = self.component_pulses.get(root_component_name).cloned();
        if let Some(pulses) = component_pulses {
            for pulse in &pulses {
                let token_str = pulse.token.to_string();
                let interval_str = pulse.interval_ns.to_string();
                let jitter_str = pulse.jitter_ns.to_string();
                let fire_sym = format!("@__kain_pulse_fire_{}", pulse.name);
                let status = self.next_reg();
                self.emit(&format!(
                    "  {} = call i64 @kain_machine_pulse_start(i64 {}, i64 {}, i64 {}, void ()* {})",
                    status, token_str, interval_str, jitter_str, fire_sym
                ));
            }
        }

        // ── Emit component-internal resonate registration ─────────
        let component_resonates = self.component_resonates.get(root_component_name).cloned();
        if let Some(resonates) = component_resonates {
            for resonate in &resonates {
                let target_str = self.compile_static_c_string_literal(&resonate.target);
                let dampen_str = resonate.dampen_ns.to_string();
                let handler_sym = format!("@__kain_resonate_{}", resonate.handler_symbol);
                self.emit(&format!(
                    "  call void @abi_resonate_register(i8* {}, i64 {}, void ()* {})",
                    target_str, dampen_str, handler_sym
                ));
            }
        }

        // Fall through to frame loop
        let component_frame_loop_label = self.next_label();
        self.emit(&format!("  br label %{}", component_frame_loop_label));

        // ── Component frame loop ─────────────────────────────────
        self.emit_label(&component_frame_loop_label);

        // host_pump (vtable offset 16)
        let _comp_pump = self.emit_vtable_call(
            &surface_reg,
            OFF_HOST_PUMP,
            "i64 (i64)*",
            &[(&session_id, "i64")],
        );

        // begin_frame (vtable offset 10)
        let delta = self.next_reg();
        self.emit(&format!(
            "  {} = call double @__kain_frame_delta_ms()",
            delta
        ));
        self.emit_vtable_call_void(
            &surface_reg,
            OFF_BEGIN_FRAME,
            "void (i64, double)*",
            &[(&session_id, "i64"), (&delta, "double")],
        );

        // Render root component
        self.emit(&format!(
            "  call void @{}(%KainComponentSurface* {}, i64 {}, i64 0)",
            render_name, surface_reg, session_id
        ));

        // end_frame (vtable offset 11)
        self.emit_vtable_call_void(
            &surface_reg,
            OFF_END_FRAME,
            "void (i64)*",
            &[(&session_id, "i64")],
        );

        // present (vtable offset 12)
        self.emit_vtable_call_void(
            &surface_reg,
            OFF_PRESENT,
            "void (i64)*",
            &[(&session_id, "i64")],
        );

        // should_close (vtable offset 14)
        let should_close = self.emit_vtable_call(
            &surface_reg,
            OFF_SHOULD_CLOSE,
            "i64 (i64)*",
            &[(&session_id, "i64")],
        );
        let keep_going = self.next_reg();
        self.emit(&format!(
            "  {} = icmp eq i64 {}, 0",
            keep_going, should_close
        ));
        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            keep_going, component_frame_loop_label, shutdown
        ));

        // ── Shared Shutdown ───────────────────────────────────────
        self.emit_label(&shutdown);
        // session_destroy (vtable offset 1)
        self.emit_vtable_call_void(
            &surface_reg,
            OFF_SESSION_DESTROY,
            "void (i64)*",
            &[(&session_id, "i64")],
        );
        self.emit("  ret void");
        self.emit("}");
        self.emit("");

        Ok(())
    }

    // =====================================================================
    //  JSX → surface calls (all through vtable)
    // =====================================================================

    /// Walk a JSX tree, emitting surface trait vtable calls for every node.
    fn compile_jsx_to_surface(
        &mut self,
        surface_reg: &str,
        session_reg: &str,
        parent_reg: &str,
        node: &JSXNode,
        component_name: &str,
        sibling_index: &mut usize,
    ) -> KainResult<()> {
        match node {
            JSXNode::Text(text, _) => {
                self.compile_jsx_text(surface_reg, session_reg, parent_reg, text, component_name, sibling_index)
            }
            JSXNode::Expression(expr) => {
                self.compile_jsx_expression(surface_reg, session_reg, parent_reg, expr, component_name, sibling_index)
            }
            JSXNode::Fragment(children, _) => {
                for child in children {
                    self.compile_jsx_to_surface(
                        surface_reg, session_reg, parent_reg,
                        child, component_name, sibling_index,
                    )?;
                }
                Ok(())
            }
            JSXNode::Element {
                tag, attributes, children, ..
            } => {
                self.compile_jsx_element(
                    surface_reg, session_reg, parent_reg,
                    tag, attributes, children,
                    component_name, sibling_index,
                )
            }
            JSXNode::ComponentCall {
                name, props, children, ..
            } => {
                self.compile_jsx_component_call(
                    surface_reg, session_reg, parent_reg,
                    name, props, children,
                    component_name,
                )
            }
            JSXNode::For {
                binding, iter, body, ..
            } => {
                self.compile_jsx_for(
                    surface_reg, session_reg, parent_reg,
                    binding, iter, body,
                    component_name, sibling_index,
                )
            }
            JSXNode::If {
                condition,
                then_branch,
                else_branch,
                ..
            } => {
                self.compile_jsx_if(
                    surface_reg, session_reg, parent_reg,
                    condition, then_branch, else_branch.as_deref(),
                    component_name, sibling_index,
                )
            }
        }
    }

    /// `<text>literal</text>` or `{expression}` → `"text"` element with set_text
    fn compile_jsx_text(
        &mut self,
        surface_reg: &str,
        session_reg: &str,
        parent_reg: &str,
        text: &str,
        component_name: &str,
        sibling_index: &mut usize,
    ) -> KainResult<()> {
        let si = *sibling_index;
        *sibling_index += 1;

        let sk = self.emit_stable_key(
            &format!("{}:text", component_name),
            parent_reg,
            si as u64,
        );

        let el = self.emit_element_begin(surface_reg, session_reg, parent_reg, "text", &sk);
        let (text_val, _) = self.compile_string_literal(text);
        self.emit_vtable_call_void(
            surface_reg,
            OFF_ELEMENT_SET_TEXT,
            "void (i64, i64, i8*)*",
            &[(session_reg, "i64"), (&el, "i64"), (&text_val, "i8*")],
        );
        self.emit_element_end(surface_reg, session_reg, &el);
        Ok(())
    }

    /// Try to resolve and inline a call to a component method at the current
    /// expression site. Returns `Ok(Some((val, ty)))` if the expression was a
    /// component method call and was successfully inlined, or `Ok(None)` if it
    /// was not a component method call (fall through to normal `compile_expr`).
    fn try_inline_component_method(
        &mut self,
        expr: &Expr,
    ) -> KainResult<Option<(String, String)>> {
        // Extract method name and call args from the expression
        let (method_name, call_args) = match expr {
            // Direct call: `method_name(args...)`
            Expr::Call { callee, args, .. } => {
                if let Expr::Ident(name, _) = callee.as_ref() {
                    (name.as_str(), args.as_slice())
                } else {
                    return Ok(None);
                }
            }
            // Method call: `self.method_name(args...)`
            Expr::MethodCall { receiver, method, args, .. } => {
                if let Expr::Ident(receiver_name, _) = receiver.as_ref() {
                    if receiver_name == "self" {
                        (method.as_str(), args.as_slice())
                    } else {
                        return Ok(None);
                    }
                } else {
                    return Ok(None);
                }
            }
            _ => return Ok(None),
        };

        // Look up in the current component's methods
        let methods = match &self.current_component_methods {
            Some(m) => m.clone(),
            None => return Ok(None),
        };

        let method = match methods.iter().find(|m| m.name == method_name) {
            Some(m) => m,
            None => return Ok(None),
        };

        // Verify arg count matches param count
        if call_args.len() != method.params.len() {
            return Err(KainError::codegen(
                format!(
                    "Method '{}' expects {} arguments, got {}",
                    method_name,
                    method.params.len(),
                    call_args.len()
                ),
                expr.span(),
            ));
        }

        // Push a new scope for method parameters
        self.scopes.push(Vec::new());

        // Bind call arguments to method parameters as locals
        for (param, arg) in method.params.iter().zip(call_args.iter()) {
            let (arg_val, arg_ty) = self.compile_expr(&arg.value)?;
            let addr_reg = format!("%{}.addr", param.name);
            self.emit_entry_alloca(&addr_reg, &arg_ty);
            self.emit(&format!(
                "  store {} {}, {}* {}",
                arg_ty, arg_val, arg_ty, addr_reg
            ));
            self.locals
                .insert(param.name.clone(), (addr_reg.clone(), arg_ty.clone()));
            if let Some(scope) = self.scopes.last_mut() {
                scope.push(param.name.clone());
            }
        }

        // Compile the method body inline
        let result = self.compile_block_with_result(&method.body)?;

        // Pop the parameter scope
        if let Some(scope) = self.scopes.pop() {
            for name in scope {
                self.locals.remove(&name);
            }
        }

        Ok(result)
    }

    /// `{expression}` → evaluate, then emit as `"text"` element via vtable
    ///
    /// If the expression is a call to a component method (e.g., `label_font_size()`
    /// inside a render body), the method body is compiled inline instead of
    /// trying to resolve a global function symbol.
    fn compile_jsx_expression(
        &mut self,
        surface_reg: &str,
        session_reg: &str,
        parent_reg: &str,
        expr: &Expr,
        component_name: &str,
        sibling_index: &mut usize,
    ) -> KainResult<()> {
        // Check for component method calls and inline the method body.
        // Component methods are NOT compiled as global LLVM functions — they
        // must be inlined at the call site within the component's render function.
        let (val, ty) = if let Some(inlined) = self.try_inline_component_method(expr)? {
            inlined
        } else {
            self.compile_expr(expr)?
        };
        let si = *sibling_index;
        *sibling_index += 1;

        let sk = self.emit_stable_key(
            &format!("{}:text", component_name),
            parent_reg,
            si as u64,
        );

        let el = self.emit_element_begin(surface_reg, session_reg, parent_reg, "text", &sk);

        // Stringify if needed
        if ty == "i8*" {
            self.emit_vtable_call_void(
                surface_reg,
                OFF_ELEMENT_SET_TEXT,
                "void (i64, i64, i8*)*",
                &[(session_reg, "i64"), (&el, "i64"), (&val, "i8*")],
            );
        } else {
            let (str_val, _) = self.stringify_value(&val, &ty)?;
            self.emit_vtable_call_void(
                surface_reg,
                OFF_ELEMENT_SET_TEXT,
                "void (i64, i64, i8*)*",
                &[(session_reg, "i64"), (&el, "i64"), (&str_val, "i8*")],
            );
        }

        self.emit_element_end(surface_reg, session_reg, &el);
        Ok(())
    }

    /// `<tag attr="val">children</tag>` → element_begin → attrs → children → element_end
    fn compile_jsx_element(
        &mut self,
        surface_reg: &str,
        session_reg: &str,
        parent_reg: &str,
        tag: &str,
        attributes: &[JSXAttribute],
        children: &[JSXNode],
        component_name: &str,
        sibling_index: &mut usize,
    ) -> KainResult<()> {
        let si = *sibling_index;
        *sibling_index += 1;

        let sk = self.emit_stable_key(
            &format!("{}:{}", component_name, tag),
            parent_reg,
            si as u64,
        );

        let el = self.emit_element_begin(surface_reg, session_reg, parent_reg, tag, &sk);

        // Emit attributes through vtable
        for attr in attributes {
            self.compile_jsx_attr(surface_reg, session_reg, &el, attr)?;
        }

        // Emit children with the element as parent
        let el_clone = el.clone();
        let mut child_si = 0usize;
        for child in children {
            self.compile_jsx_to_surface(
                surface_reg, session_reg, &el_clone,
                child, component_name, &mut child_si,
            )?;
        }

        self.emit_element_end(surface_reg, session_reg, &el);
        Ok(())
    }

    /// `<ComponentName prop="val" />` → call void @ComponentName_render(surface, session, parent, props...)
    /// Children are compiled as siblings under the same parent after the component render call.
    fn compile_jsx_component_call(
        &mut self,
        surface_reg: &str,
        session_reg: &str,
        parent_reg: &str,
        name: &str,
        props: &[JSXAttribute],
        children: &[JSXNode],
        component_name: &str,
    ) -> KainResult<()> {
        let render_name = format!("{}_render", name);

        // Look up component prop definitions
        let defs = self.component_defs.get(name).cloned().unwrap_or_default();

        let mut compiled_args: Vec<(String, String)> = Vec::new();

        // First three args: surface (i8* => %KainComponentSurface*), session_id (i64), parent_id (i64)
        compiled_args.push((surface_reg.to_string(), "%KainComponentSurface*".to_string()));
        compiled_args.push((session_reg.to_string(), "i64".to_string()));
        compiled_args.push((parent_reg.to_string(), "i64".to_string()));

        // Props in declaration order
        for (prop_name, _prop_ty) in &defs {
            if let Some(prop) = props.iter().find(|p| p.name == *prop_name) {
                match &prop.value {
                    JSXAttrValue::String(value) => {
                        let (val, _) = self.compile_string_literal(value);
                        compiled_args.push((val, "i8*".to_string()));
                    }
                    JSXAttrValue::Bool(value) => {
                        compiled_args.push((
                            if *value { "1".into() } else { "0".into() },
                            "i64".to_string(),
                        ));
                    }
                    JSXAttrValue::Expr(expr) => {
                        let (val, ty) = self.compile_expr(expr)?;
                        compiled_args.push((val, ty));
                    }
                    JSXAttrValue::Callback(_, handler_expr) => {
                        let (val, ty) = self.compile_expr(handler_expr.as_ref())?;
                        compiled_args.push((val, ty));
                    }
                }
            } else {
                // Prop not provided → use zero/empty default
                let zero = self.zero_value_for_ty(_prop_ty);
                compiled_args.push((zero, _prop_ty.clone()));
            }
        }

        // Emit declare for cross-module component renders.
        // If the render function was compiled in a different module, the linker
        // needs a declaration. Skip if already defined in the current module.
        if !self.functions.contains_key(&render_name) {
            let mut declare_types: Vec<String> = Vec::new();
            declare_types.push("%KainComponentSurface*".to_string());
            declare_types.push("i64".to_string());
            declare_types.push("i64".to_string());
            for (_prop_name, _prop_ty) in &defs {
                declare_types.push(_prop_ty.clone());
            }
            let declare_str = declare_types.join(", ");
            self.emit(&format!("declare void @{}({})", render_name, declare_str));
        }

        let arg_str = compiled_args
            .iter()
            .map(|(val, ty)| format!("{} {}", ty, val))
            .collect::<Vec<_>>()
            .join(", ");

        self.emit(&format!(
            "  call void @{}({})",
            render_name, arg_str
        ));

        // Compile children as siblings under the same parent after the component
        // render call. This ensures parent/child node linkage across component
        // boundaries instead of silently ignoring nested children.
        if !children.is_empty() {
            let mut child_si = 0usize;
            for child in children {
                self.compile_jsx_to_surface(
                    surface_reg, session_reg, parent_reg,
                    child, component_name, &mut child_si,
                )?;
            }
        }

        Ok(())
    }

    /// `for item in items: <jsx>` → runtime loop with index-based stable keys
    fn compile_jsx_for(
        &mut self,
        surface_reg: &str,
        session_reg: &str,
        parent_reg: &str,
        binding: &str,
        iter: &Expr,
        body: &JSXNode,
        component_name: &str,
        _sibling_index: &mut usize,
    ) -> KainResult<()> {
        // Evaluate the iterable
        let (iter_val, _iter_ty) = self.compile_expr(iter)?;

        // Get length — assume runtime array (i8* handle)
        let len_reg = self.next_reg();
        self.emit(&format!(
            "  {} = call i64 @runtime_array_len(i8* {})",
            len_reg, iter_val
        ));

        // Alloca for loop index
        let idx_ptr = self.next_reg();
        self.emit_entry_alloca(&idx_ptr, "i64");
        self.emit(&format!("  store i64 0, i64* {}", idx_ptr));

        let loop_header = self.next_label();
        let loop_body = self.next_label();
        let loop_done = self.next_label();

        self.emit(&format!("  br label %{}", loop_header));

        self.emit_label(&loop_header);
        let idx = self.next_reg();
        self.emit(&format!(
            "  {} = load i64, i64* {}",
            idx, idx_ptr
        ));
        let done = self.next_reg();
        self.emit(&format!(
            "  {} = icmp sge i64 {}, {}",
            done, idx, len_reg
        ));
        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            done, loop_done, loop_body
        ));

        self.emit_label(&loop_body);

        // Get item: runtime_array_get(iter, idx)
        let item = self.next_reg();
        self.emit(&format!(
            "  {} = call i8* @runtime_array_get(i8* {}, i64 {})",
            item, iter_val, idx
        ));

        // Store item as local so the body can reference it by name
        let item_addr = format!("%{}.addr", binding);
        self.emit_entry_alloca(&item_addr, "i8*");
        self.emit(&format!("  store i8* {}, i8** {}", item, item_addr));
        self.locals.insert(binding.to_string(), (item_addr.clone(), "i8*".to_string()));
        if let Some(scope) = self.scopes.last_mut() {
            scope.push(binding.to_string());
        }

        // Create a temporary parent that encodes idx so stable keys are unique
        let child_parent = self.next_reg();
        self.emit(&format!(
            "  {} = add i64 {}, {}",
            child_parent, parent_reg, idx
        ));

        let item_si: usize = 0;
        self.compile_jsx_to_surface(
            surface_reg, session_reg, &child_parent,
            body, component_name, &mut (item_si as usize),
        )?;

        // Remove binding from scope
        self.locals.remove(binding);
        if let Some(scope) = self.scopes.last_mut() {
            scope.retain(|n| n != binding);
        }

        // Increment index
        let next_idx = self.next_reg();
        self.emit(&format!(
            "  {} = add i64 {}, 1",
            next_idx, idx
        ));
        self.emit(&format!("  store i64 {}, i64* {}", next_idx, idx_ptr));
        self.emit(&format!("  br label %{}", loop_header));

        self.emit_label(&loop_done);
        Ok(())
    }

    /// `if cond: <jsx> elif cond2: <jsx> else: <jsx>` → LLVM branches
    fn compile_jsx_if(
        &mut self,
        surface_reg: &str,
        session_reg: &str,
        parent_reg: &str,
        condition: &Expr,
        then_branch: &JSXNode,
        else_branch: Option<&JSXNode>,
        component_name: &str,
        sibling_index: &mut usize,
    ) -> KainResult<()> {
        let (cond_val, cond_ty) = self.compile_expr(condition)?;

        let is_true = if cond_ty == "i1" {
            cond_val
        } else {
            // Bool is lowered to i1; expressions return i64 — coerce
            let cmp_reg = self.next_reg();
            self.emit(&format!("  {} = icmp ne i64 {}, 0", cmp_reg, cond_val));
            cmp_reg
        };

        let then_block = self.next_label();
        let else_block = self.next_label();
        let done_block = self.next_label();

        // Use distinct suffix per branch for stable keys
        let then_si = *sibling_index;
        *sibling_index += 1;
        let else_si = *sibling_index;
        *sibling_index += 1;

        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            is_true, then_block, else_block
        ));

        // Then branch
        self.emit_label(&then_block);
        let mut then_si_val = then_si;
        self.compile_jsx_to_surface(
            surface_reg, session_reg, parent_reg,
            then_branch, component_name, &mut then_si_val,
        )?;
        self.emit(&format!("  br label %{}", done_block));

        // Else branch
        self.emit_label(&else_block);
        if let Some(else_node) = else_branch {
            let mut else_si_val = else_si;
            self.compile_jsx_to_surface(
                surface_reg, session_reg, parent_reg,
                else_node, component_name, &mut else_si_val,
            )?;
        }
        self.emit(&format!("  br label %{}", done_block));

        self.emit_label(&done_block);
        Ok(())
    }

    /// Compile an event callback attribute — emit element_set_callback vtable call.
    /// Called when JSXAttrValue::Callback is encountered.
    /// The callback is stored as a function pointer on the element for the given event.
    fn compile_jsx_callback(
        &mut self,
        surface_reg: &str,
        session_reg: &str,
        element_reg: &str,
        attr: &JSXAttribute,
    ) -> KainResult<()> {
        let (event_kind, fn_expr) = match &attr.value {
            JSXAttrValue::Callback(kind, expr) => (kind.clone(), expr.as_ref().clone()),
            _ => return Ok(()), // not a callback — no-op
        };

        // Emit the event kind string literal (e.g. "click", "change", "toggle")
        let event_str = self.compile_static_c_string_literal(&event_kind);

        // Compile the handler expression to get the function pointer.
        // The handler must be a function reference that yields a void (i64, i64, i8*)*
        // compatible function pointer.
        let (handler_val, handler_ty) = self.compile_expr(&fn_expr)?;

        // Bitcast the handler to the expected callback type: void (i64, i64, i8*)*
        let callback_reg = self.next_reg();
        self.emit(&format!(
            "  {} = bitcast {} {} to %KainComponentCallback",
            callback_reg, handler_ty, handler_val
        ));

        // Emit: element_set_callback(session_id, element_id, event_name, callback_fn)
        // vtable slot 23: void (i64, i64, i8*, void*)*
        // The callback is stored in the vtable as a void*; we pass the %KainComponentCallback ptr.
        let callback_i8 = self.next_reg();
        self.emit(&format!(
            "  {} = bitcast %KainComponentCallback {} to i8*",
            callback_i8, callback_reg
        ));
        self.emit_vtable_call_void(
            surface_reg,
            OFF_ELEMENT_SET_CALLBACK,
            "void (i64, i64, i8*, i8*)*",
            &[
                (session_reg, "i64"),
                (element_reg, "i64"),
                (&event_str, "i8*"),
                (&callback_i8, "i8*"),
            ],
        );
        Ok(())
    }

    /// Map a JSX attribute to the correct vtable call (Contract 11)
    fn compile_jsx_attr(
        &mut self,
        surface_reg: &str,
        session_reg: &str,
        element_reg: &str,
        attr: &JSXAttribute,
    ) -> KainResult<()> {
        // ── Event callbacks are handled separately through the vtable ──
        if matches!(&attr.value, JSXAttrValue::Callback(_, _)) {
            return self.compile_jsx_callback(surface_reg, session_reg, element_reg, attr);
        }

        // Resolve the vtable offset and mapped key
        let mapped = map_jsx_attr_to_surface_key(&attr.name);
        let is_text_attr = attr.name == "value";
        let expects_f64 = mapped.vtable_offset == OFF_ELEMENT_SET_ATTR_F64;

        // For unknown attributes, use the attr name itself as the key
        let style_key: String = if mapped.style_key.is_empty() && !is_text_attr {
            attr.name.clone()
        } else {
            mapped.style_key.to_string()
        };

        // Helper to coerce value types for the vtable call
        let coerce_for_surface = |gen: &mut Self, val: String, ty: String| -> (String, String) {
            if ty == "i1" {
                let widened = gen.next_reg();
                gen.emit(&format!("  {} = zext i1 {} to i64", widened, val));
                (widened, "i64".to_string())
            } else if expects_f64 && ty == "i64" {
                let promoted = gen.next_reg();
                gen.emit(&format!("  {} = sitofp i64 {} to double", promoted, val));
                (promoted, "double".to_string())
            } else {
                (val, ty)
            }
        };

        match &attr.value {
            JSXAttrValue::String(value) => {
                let (val_reg, _) = self.compile_string_literal(value);
                if is_text_attr {
                    // element_set_text: void (i64, i64, i8*)* — session, element, text
                    self.emit_vtable_call_void(
                        surface_reg,
                        OFF_ELEMENT_SET_TEXT,
                        "void (i64, i64, i8*)*",
                        &[(session_reg, "i64"), (element_reg, "i64"), (&val_reg, "i8*")],
                    );
                } else if mapped.vtable_offset == OFF_ELEMENT_SET_ATTR_I64 {
                    // String value for an i64 attribute — convert known keywords to integers.
                    // direction="vertical" → 1, direction="horizontal" → 0, etc.
                    let int_val: i64 = match value.as_str() {
                        "vertical" | "column" => 1,
                        "horizontal" | "row" => 0,
                        _ => 0, // unknown string → default to 0
                    };
                    let int_str = int_val.to_string();
                    let key_str = self.compile_static_c_string_literal(&style_key);
                    self.emit_vtable_call_void(
                        surface_reg,
                        mapped.vtable_offset,
                        mapped.fn_ptr_ty,
                        &[(session_reg, "i64"), (element_reg, "i64"), (&key_str, "i8*"), (&int_str, "i64")],
                    );
                } else {
                    // Emit style call: element_set_attr_* (i64, i64, i8*, value) — session, element, key, value
                    let key_str = self.compile_static_c_string_literal(&style_key);
                    self.emit_vtable_call_void(
                        surface_reg,
                        mapped.vtable_offset,
                        mapped.fn_ptr_ty,
                        &[(session_reg, "i64"), (element_reg, "i64"), (&key_str, "i8*"), (&val_reg, "i8*")],
                    );
                }
            }
            JSXAttrValue::Bool(true) => {
                if is_text_attr {
                    let (val_reg, _) = self.compile_string_literal("true");
                    self.emit_vtable_call_void(
                        surface_reg,
                        OFF_ELEMENT_SET_TEXT,
                        "void (i64, i64, i8*)*",
                        &[(session_reg, "i64"), (element_reg, "i64"), (&val_reg, "i8*")],
                    );
                } else if expects_f64 {
                    let key_str = self.compile_static_c_string_literal(&style_key);
                    self.emit_vtable_call_void(
                        surface_reg,
                        mapped.vtable_offset,
                        mapped.fn_ptr_ty,
                        &[(session_reg, "i64"), (element_reg, "i64"), (&key_str, "i8*"), ("1.0", "double")],
                    );
                } else {
                    let key_str = self.compile_static_c_string_literal(&style_key);
                    self.emit_vtable_call_void(
                        surface_reg,
                        mapped.vtable_offset,
                        mapped.fn_ptr_ty,
                        &[(session_reg, "i64"), (element_reg, "i64"), (&key_str, "i8*"), ("1", "i64")],
                    );
                }
            }
            JSXAttrValue::Bool(false) => {
                // Bool false → no-op (attribute not set)
            }
            JSXAttrValue::Expr(expr) => {
                let (val, ty) = self.compile_expr(expr)?;
                let (val_use, _ty_use) = coerce_for_surface(self, val, ty);
                if is_text_attr {
                    self.emit_vtable_call_void(
                        surface_reg,
                        OFF_ELEMENT_SET_TEXT,
                        "void (i64, i64, i8*)*",
                        &[(session_reg, "i64"), (element_reg, "i64"), (&val_use, "i8*")],
                    );
                } else {
                    let key_str = self.compile_static_c_string_literal(&style_key);
                    // Determine value type for the call
                    let val_ty: &str = if mapped.vtable_offset == OFF_ELEMENT_SET_ATTR_F64 {
                        "double"
                    } else if mapped.vtable_offset == OFF_ELEMENT_SET_ATTR_I64 {
                        "i64"
                    } else {
                        "i8*"
                    };
                    self.emit_vtable_call_void(
                        surface_reg,
                        mapped.vtable_offset,
                        mapped.fn_ptr_ty,
                        &[(session_reg, "i64"), (element_reg, "i64"), (&key_str, "i8*"), (&val_use, val_ty)],
                    );
                }
            }
            JSXAttrValue::Callback(_, _) => {
                // Handled at the top of compile_jsx_attr via early return.
                // This arm is unreachable but needed for exhaustive match.
                unreachable!("Callback should be routed before attribute mapping");
            }
        }
        Ok(())
    }

    // =====================================================================
    //  Vtable call helpers
    // =====================================================================

    /// Emit a vtable indirect call: `getelementptr` → `load` → `call`.
    ///
    /// Returns the result register name (empty string for void return).
    fn emit_vtable_call(
        &mut self,
        surface_reg: &str,
        offset: u32,
        fn_ptr_ty: &str,
        args: &[(&str, &str)], // (reg_or_literal, llvm_type)
    ) -> String {
        // Check if the function pointer type returns void
        let ret_is_void = fn_ptr_ty.trim_start().starts_with("void");

        // getelementptr into the vtable at the given offset (produces i8**)
        let gep_reg = self.next_reg();
        self.emit(&format!(
            "  {} = getelementptr inbounds %KainComponentSurface, %KainComponentSurface* {}, i32 0, i32 {}",
            gep_reg, surface_reg, offset
        ));

        // Bitcast i8** to the actual function-pointer-pointer type
        let cast_reg = self.next_reg();
        let fn_ptr_ptr_ty = format!("{}*", fn_ptr_ty);
        self.emit(&format!(
            "  {} = bitcast i8** {} to {}",
            cast_reg, gep_reg, fn_ptr_ptr_ty
        ));

        // Load the function pointer
        let fn_reg = self.next_reg();
        self.emit(&format!(
            "  {} = load {}, {}* {}",
            fn_reg, fn_ptr_ty, fn_ptr_ptr_ty, cast_reg
        ));

        let args_str = args
            .iter()
            .map(|(val, ty)| format!("{} {}", ty, val))
            .collect::<Vec<_>>()
            .join(", ");

        if ret_is_void {
            self.emit(&format!(
                "  call void {}({})",
                fn_reg, args_str
            ));
            String::new()
        } else {
            let call_reg = self.next_reg();
            // Extract return type: everything before the first '(' in fn_ptr_ty
            let ret_ty = fn_ptr_ty.split('(').next().unwrap_or("i64").trim();
            self.emit(&format!(
                "  {} = call {} {}({})",
                call_reg, ret_ty, fn_reg, args_str
            ));
            call_reg
        }
    }

    /// Convenience wrapper for `emit_vtable_call` when the return is void.
    /// Discards the (empty) result register name.
    fn emit_vtable_call_void(
        &mut self,
        surface_reg: &str,
        offset: u32,
        fn_ptr_ty: &str,
        args: &[(&str, &str)],
    ) {
        self.emit_vtable_call(surface_reg, offset, fn_ptr_ty, args);
    }

    // =====================================================================
    //  Element begin / end
    // =====================================================================

    /// Emit `element_begin(session, parent, kind, stable_key)` through the
    /// vtable (offset 2) and return the element ID register.
    fn emit_element_begin(
        &mut self,
        surface_reg: &str,
        session_reg: &str,
        parent_reg: &str,
        kind: &str,
        stable_key: &str,
    ) -> String {
        let kind_str = self.compile_static_c_string_literal(kind);
        self.emit_vtable_call(
            surface_reg,
            OFF_ELEMENT_BEGIN,
            "i64 (i64, i64, i8*, i8*)*",
            &[
                (session_reg, "i64"),
                (parent_reg, "i64"),
                (&kind_str, "i8*"),
                (stable_key, "i8*"),
            ],
        )
    }

    /// Emit `element_end(session, element_id)` through the vtable (offset 3).
    fn emit_element_end(
        &mut self,
        surface_reg: &str,
        session_reg: &str,
        element_reg: &str,
    ) {
        self.emit_vtable_call_void(
            surface_reg,
            OFF_ELEMENT_END,
            "void (i64, i64)*",
            &[(session_reg, "i64"), (element_reg, "i64")],
        );
    }

    // =====================================================================
    //  Stable key computation (Contract 10)
    // =====================================================================

    /// Compute a stable key: `"ComponentName:path:parent_id:sibling_index"`
    fn emit_stable_key(
        &mut self,
        path_prefix: &str,
        parent_reg: &str,
        sibling_index: u64,
    ) -> String {
        let prefix_str = self.compile_static_c_string_literal(path_prefix);
        let si_str = self.compile_static_c_string_literal(&format!(":{}", sibling_index));
        let colon = self.compile_static_c_string_literal(":");

        // concat prefix + ":"
        let step1 = self.next_reg();
        self.emit(&format!(
            "  {} = call i8* @str_concat(i8* {}, i8* {})",
            step1, prefix_str, colon
        ));

        // parent to string
        let parent_str = self.next_reg();
        self.emit(&format!(
            "  {} = call i8* @to_string(i64 {})",
            parent_str, parent_reg
        ));

        // concat step1 + parent_str
        let step2 = self.next_reg();
        self.emit(&format!(
            "  {} = call i8* @str_concat(i8* {}, i8* {})",
            step2, step1, parent_str
        ));

        // concat step2 + si_str
        let result = self.next_reg();
        self.emit(&format!(
            "  {} = call i8* @str_concat(i8* {}, i8* {})",
            result, step2, si_str
        ));

        result
    }

    // =====================================================================
    //  Component state persistence (Contract 8)
    // =====================================================================

    fn compile_component_state_init(
        &mut self,
        component: &TypedComponent,
        surface_reg: &str,
        session_reg: &str,
    ) -> KainResult<Vec<(String, String, StateFieldType)>> {
        let mut state_fields: Vec<(String, String, StateFieldType)> = Vec::new();
        for state in &component.ast.state {
            let key = format!("{}:{}", component.ast.name, state.name);
            let key_str = self.compile_static_c_string_literal(&key);

            // Determine state type from the typechecker's resolved types.
            // Falls back to I64 for untyped or unknown state.
            let state_ty = component.state_types.get(&state.name)
                .map(|t| StateFieldType::from_resolved(t))
                .unwrap_or(StateFieldType::I64);

            // Dispatch: get the stored value via the correct vtable slot.
            // Sentinel values: i64=-1, f64=NaN, string=null — distinct from valid data.
            let (stored_val, _stored_ty, is_first_check): (String, &str, String) = match &state_ty {
                StateFieldType::F64 => {
                    let v = self.emit_vtable_call(
                        surface_reg,
                        OFF_STATE_GET_F64,
                        "double (i64, i8*)*",
                        &[(session_reg, "i64"), (&key_str, "i8*")],
                    );
                    // First-frame check: NaN sentinel (fcmp uno returns true if either is NaN)
                    let is_first = self.next_reg();
                    self.emit(&format!(
                        "  {} = fcmp uno double {}, 0x{:X}",
                        is_first, v, f64::to_bits(f64::NAN)
                    ));
                    // Hoist alloca
                    let addr_reg = format!("%{}.addr", state.name);
                    self.emit_entry_alloca(&addr_reg, "double");
                    self.locals.insert(state.name.clone(), (addr_reg.clone(), "double".to_string()));
                    if let Some(scope) = self.scopes.last_mut() {
                        scope.push(state.name.clone());
                    }
                    (v, "double", is_first)
                }
                StateFieldType::String => {
                    let v = self.emit_vtable_call(
                        surface_reg,
                        OFF_STATE_GET_STRING,
                        "i8* (i64, i8*)*",
                        &[(session_reg, "i64"), (&key_str, "i8*")],
                    );
                    // First-frame check: null sentinel
                    let is_first = self.next_reg();
                    self.emit(&format!(
                        "  {} = icmp eq i8* {}, null",
                        is_first, v
                    ));
                    // Hoist alloca
                    let addr_reg = format!("%{}.addr", state.name);
                    self.emit_entry_alloca(&addr_reg, "i8*");
                    self.locals.insert(state.name.clone(), (addr_reg.clone(), "i8*".to_string()));
                    if let Some(scope) = self.scopes.last_mut() {
                        scope.push(state.name.clone());
                    }
                    (v, "i8*", is_first)
                }
                StateFieldType::I64 => {
                    let v = self.emit_vtable_call(
                        surface_reg,
                        OFF_STATE_GET_I64,
                        "i64 (i64, i8*)*",
                        &[(session_reg, "i64"), (&key_str, "i8*")],
                    );
                    // First-frame check: use -1 as sentinel instead of 0
                    // This avoids the bug where 0 (valid state value) == first-frame sentinel.
                    let is_first = self.next_reg();
                    self.emit(&format!(
                        "  {} = icmp eq i64 {}, -1",
                        is_first, v
                    ));
                    // Hoist alloca
                    let addr_reg = format!("%{}.addr", state.name);
                    self.emit_entry_alloca(&addr_reg, "i64");
                    self.locals.insert(state.name.clone(), (addr_reg.clone(), "i64".to_string()));
                    if let Some(scope) = self.scopes.last_mut() {
                        scope.push(state.name.clone());
                    }
                    (v, "i64", is_first)
                }
            };

            // Save predecessor block BEFORE the branch for correct PHI labels.
            // For the first state field this is "entry"; for subsequent fields it's
            // the previous iteration's load block.
            let pred_block = self.current_block.clone();

            let init_block = self.next_label();
            let load_block = self.next_label();
            self.emit(&format!(
                "  br i1 {}, label %{}, label %{}",
                is_first_check, init_block, load_block
            ));

            // Init block: compile initial value and store via the correct vtable set
            self.emit_label(&init_block);
            let (raw_init_val, raw_init_ty) = self.compile_expr(&state.initial)?;
            // Coerce to the target type if needed (e.g., literal i64 → f64 for Float state)
            let init_val = match &state_ty {
                StateFieldType::F64 if raw_init_ty == "i64" => {
                    let c = self.next_reg();
                    self.emit(&format!("  {} = sitofp i64 {} to double", c, raw_init_val));
                    (c, "double".to_string())
                }
                StateFieldType::F64 => (raw_init_val, "double".to_string()),
                _ => (raw_init_val, raw_init_ty),
            };
            match &state_ty {
                StateFieldType::F64 => {
                    self.emit_vtable_call_void(
                        surface_reg,
                        OFF_STATE_SET_F64,
                        "void (i64, i8*, double)*",
                        &[(session_reg, "i64"), (&key_str, "i8*"), (&init_val.0, "double")],
                    );
                }
                StateFieldType::String => {
                    self.emit_vtable_call_void(
                        surface_reg,
                        OFF_STATE_SET_STRING,
                        "void (i64, i8*, i8*)*",
                        &[(session_reg, "i64"), (&key_str, "i8*"), (&init_val.0, "i8*")],
                    );
                }
                StateFieldType::I64 => {
                    self.emit_vtable_call_void(
                        surface_reg,
                        OFF_STATE_SET_I64,
                        "void (i64, i8*, i64)*",
                        &[(session_reg, "i64"), (&key_str, "i8*"), (&init_val.0, "i64")],
                    );
                }
            }
            self.emit(&format!("  br label %{}", load_block));

            // Load block: PHI merge init_val (first frame) with stored_val (subsequent frames)
            self.emit_label(&load_block);
            let addr_reg = format!("%{}.addr", state.name);
            let phi_reg = self.next_reg();
            match &state_ty {
                StateFieldType::F64 => {
                    self.emit(&format!(
                        "  {} = phi double [ {}, %{} ], [ {}, %{} ]",
                        phi_reg, init_val.0, init_block, stored_val, pred_block
                    ));
                    self.emit(&format!("  store double {}, double* {}", phi_reg, addr_reg));
                }
                StateFieldType::String => {
                    self.emit(&format!(
                        "  {} = phi i8* [ {}, %{} ], [ {}, %{} ]",
                        phi_reg, init_val.0, init_block, stored_val, pred_block
                    ));
                    self.emit(&format!("  store i8* {}, i8** {}", phi_reg, addr_reg));
                }
                StateFieldType::I64 => {
                    self.emit(&format!(
                        "  {} = phi i64 [ {}, %{} ], [ {}, %{} ]",
                        phi_reg, init_val.0, init_block, stored_val, pred_block
                    ));
                    self.emit(&format!("  store i64 {}, i64* {}", phi_reg, addr_reg));
                }
            }

            // Track for write-back at end of render function
            state_fields.push((key, addr_reg, state_ty));
        }

        Ok(state_fields)
    }

    /// Emit one-time pulse and resonate registration for component-inline
    /// clocks and tripwires. Uses a sentinel key in the vtable state store
    /// to track whether registration has already occurred (avoids re-registering
    /// every frame which would reset the timers).
    fn emit_component_pulse_resonate_registration(
        &mut self,
        component: &TypedComponent,
        surface_reg: &str,
        session_reg: &str,
    ) -> KainResult<()> {
        let name = &component.ast.name;
        let init_key = format!("{}:__pulses_init", name);
        let init_key_str = self.compile_static_c_string_literal(&init_key);

        // Check the init flag via vtable state (slot 8: state_get_i64).
        // Sent -1 means "first render" (not yet registered).
        let init_flag = self.emit_vtable_call(
            surface_reg,
            OFF_STATE_GET_I64,
            "i64 (i64, i8*)*",
            &[(session_reg, "i64"), (&init_key_str, "i8*")],
        );
        let needs_init = self.next_reg();
        self.emit(&format!(
            "  {} = icmp eq i64 {}, -1",
            needs_init, init_flag
        ));

        let register_block = self.next_label();
        let skip_block = self.next_label();
        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            needs_init, register_block, skip_block
        ));

        self.emit_label(&register_block);

        // Mark as initialized so subsequent renders skip registration
        self.emit_vtable_call_void(
            surface_reg,
            OFF_STATE_SET_I64,
            "void (i64, i8*, i64)*",
            &[(session_reg, "i64"), (&init_key_str, "i8*"), ("1", "i64")],
        );

        // Emit pulse registration: one kain_machine_pulse_start per inline pulse.
        // The pulse handler symbols (@__kain_pulse_fire_{name}) are emitted by
        // the top-level pulse codegen in mod.rs.
        for pulse in &component.pulse_types {
            let pulse_sym = format!("@__kain_pulse_fire_{}", pulse.ast.name);
            // Derive a unique token from the component+pulse name string pointer
            let token_str = self.compile_static_c_string_literal(
                &format!("{}:{}", name, pulse.ast.name)
            );
            let token = self.next_reg();
            self.emit(&format!(
                "  {} = ptrtoint i8* {} to i64",
                token, token_str
            ));
            // interval_ns: parse from pulse.ast.interval (e.g. "16ms" → 16_000_000 ns)
            // For now, emit a fixed 16ms interval; future: resolve the actual duration.
            let interval_ns: u64 = 16_000_000; // 16ms in nanoseconds
            let status = self.next_reg();
            self.emit(&format!(
                "  {} = call i64 @kain_machine_pulse_start(i64 {}, i64 {}, i64 0, void ()* {})",
                status, token, interval_ns, pulse_sym
            ));
        }

        // Emit resonate registration: one abi_resonate_register per inline resonate.
        // The handler symbols (@__kain_resonate_{name}) are emitted by the top-level
        // resonate codegen in mod.rs.
        for resonate in &component.resonate_types {
            let target_str = self.compile_static_c_string_literal(&resonate.ast.name);
            let handler_sym = format!("@__kain_resonate_{}", resonate.ast.name);
            let dampen_ns: u64 = 16_000_000; // 16ms default dampening
            self.emit(&format!(
                "  call void @abi_resonate_register(i8* {}, i64 {}, void ()* {})",
                target_str, dampen_ns, handler_sym
            ));
        }

        self.emit(&format!("  br label %{}", skip_block));
        self.emit_label(&skip_block);
        Ok(())
    }
}
