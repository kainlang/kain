//! Component surface codegen — emits LLVM IR that calls through the
//! `KainComponentSurface` trait vtable. All element creation, attribute
//! setting, state persistence, and frame lifecycle operations go through
//! indirect vtable calls — never direct `abi_ui_*` function calls.
//!
//! See `X:/research/component/WIRING_CONTRACT.md` for the full contract.
//! See `X:/runtime/native/include/component_surface.h` for the C trait layout.

use kain_core::ast::{Expr, JSXAttrValue, JSXAttribute, JSXNode};
use kain_core::error::KainResult;
use kain_core::types::TypedComponent;
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

// ── JSX attribute → surface call mapping (Contract 11) ──────────────────
struct AttrMapping {
    vtable_offset: u32,
    fn_ptr_ty: &'static str,
    style_key: &'static str,
}

fn map_jsx_attr_to_surface_key(attr_name: &str) -> AttrMapping {
    match attr_name {
        "padding" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_F64, fn_ptr_ty: "void (i64, i64, i8*, double)*", style_key: "padding" },
        "spacing" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_F64, fn_ptr_ty: "void (i64, i64, i8*, double)*", style_key: "spacing" },
        "corner_radius" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_F64, fn_ptr_ty: "void (i64, i64, i8*, double)*", style_key: "corner_radius" },
        "font_size" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_F64, fn_ptr_ty: "void (i64, i64, i8*, double)*", style_key: "font_size" },
        "opacity" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_F64, fn_ptr_ty: "void (i64, i64, i8*, double)*", style_key: "opacity" },
        "border" | "border_width" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_F64, fn_ptr_ty: "void (i64, i64, i8*, double)*", style_key: "border_width" },
        "width" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_F64, fn_ptr_ty: "void (i64, i64, i8*, double)*", style_key: "width" },
        "height" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_F64, fn_ptr_ty: "void (i64, i64, i8*, double)*", style_key: "height" },
        "background" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_STRING, fn_ptr_ty: "void (i64, i64, i8*, i8*)*", style_key: "fill_color" },
        "border_color" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_STRING, fn_ptr_ty: "void (i64, i64, i8*, i8*)*", style_key: "border_color" },
        "color" | "ink_color" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_STRING, fn_ptr_ty: "void (i64, i64, i8*, i8*)*", style_key: "ink_color" },
        "title" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_STRING, fn_ptr_ty: "void (i64, i64, i8*, i8*)*", style_key: "title" },
        "value" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_TEXT, fn_ptr_ty: "void (i64, i64, i8*)*", style_key: "" },
        "direction" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_I64, fn_ptr_ty: "void (i64, i64, i8*, i64)*", style_key: "layout.direction" },
        "disabled" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_I64, fn_ptr_ty: "void (i64, i64, i8*, i64)*", style_key: "disabled" },
        // Unknown attributes pass through as strings with the attr name as the key
        _ => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_STRING, fn_ptr_ty: "void (i64, i64, i8*, i8*)*", style_key: "" },
    }
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

        // Sized trait type with 19 pointer-sized fields (one per vtable slot).
        // The exact function pointer types differ per slot; we use i8* as a
        // uniform placeholder and bitcast before loading the real fn pointer.
        self.emit("%KainComponentSurface = type { i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8* }");

        // Registry — resolve a named surface backend
        self.emit("%KainGpuSurfaceExtension = type { i8*, i8* }");
        self.emit("declare %KainComponentSurface* @kain_component_surface_resolve(i8*)");

        // Runtime panic — for surface resolution / session failures
        self.emit("declare void @kain_runtime_panic(i8*)");

        // Frame delta — high-resolution timer
        self.emit("declare double @__kain_frame_delta_ms()");

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

        // Compile state fields (Contract 8) — returns (key, addr_reg) for write-back
        let state_fields = self.compile_component_state_init(component, &surface_reg, &session_reg)?;

        // Set current component context for JSX compilation
        self.current_component_name = Some(name.clone());
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
        for (key, addr_reg) in &state_fields {
            let load_reg = self.next_reg();
            self.emit(&format!("  {} = load i64, i64* {}", load_reg, addr_reg));
            let key_str = self.compile_static_c_string_literal(key);
            self.emit_vtable_call_void(
                &surface_reg,
                OFF_STATE_SET_I64,
                "void (i64, i8*, i64)*",
                &[(&session_reg, "i64"), (&key_str, "i8*"), (&load_reg, "i64")],
            );
        }

        // Clear component context
        self.current_component_name = None;
        self.current_component_session = None;
        self.current_component_parent = None;

        self.emit("  ret void");
        self.emit("}");
        self.emit("");
        self.current_return_type = None;
        Ok(())
    }

    /// Emit a GPU shader surface frame loop for worlds with surface shader => Fragment.
    /// Generates: resolve surface -> session -> attach platform -> get_gpu_extension ->
    /// load_shader -> frame loop (host_pump, begin_frame, set_uniform*3, end_frame,
    /// present, should_close) -> session_destroy.
    fn compile_shader_surface_loop(
        &mut self,
        world_name: &str,
        shader_fragment_name: &str,
    ) -> KainResult<()> {        let fn_name = format!("__kain_world_surface_loop_{}", Self::sanitize_symbol_fragment(world_name));

        self.emit(&format!("define void @{}() {{", fn_name));
        self.emit_label("entry");

        // Resolve surface
        let surface_name_str = self.compile_static_c_string_literal("shader");
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
        let err_msg = format!("surface shader not registered for world {}", world_name);
        let err_str = self.compile_static_c_string_literal(&err_msg);
        self.emit(&format!("  call void @kain_runtime_panic(i8* {})", err_str));
        self.emit("  unreachable");

        // Create session (vtable offset 0)
        self.emit_label(&init_block);
        let session_name_str = self.compile_static_c_string_literal(world_name);
        let session_id = self.emit_vtable_call(
            &surface_reg,
            OFF_SESSION_CREATE,
            "i64 (i8*, i64, i64)*",
            &[
                (&session_name_str, "i8*"),
                ("1280", "i64"),
                ("720", "i64"),
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
        let fail_msg = format!("session_create failed for world {}", world_name);
        let fail_str = self.compile_static_c_string_literal(&fail_msg);
        self.emit(&format!("  call void @kain_runtime_panic(i8* {})", fail_str));
        self.emit("  unreachable");

        // Attach platform (vtable offset 17)
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

        // window_open (vtable offset 15)
        let window_title_str = self.compile_static_c_string_literal(world_name);
        let _window_ok = self.emit_vtable_call(
            &surface_reg,
            OFF_WINDOW_OPEN,
            "i64 (i64, i8*, i64, i64)*",
            &[
                (&session_id, "i64"),
                (&window_title_str, "i8*"),
                ("1280", "i64"),
                ("720", "i64"),
            ],
        );

        // Get GPU extension (vtable slot 18)
        let ext_reg = self.emit_vtable_call(
            &surface_reg,
            OFF_GET_GPU_EXTENSION,
            "i8* (i64)*",
            &[(&session_id, "i64")],
        );

        let ext_is_null = self.next_reg();
        self.emit(&format!(
            "  {} = icmp eq i8* {}, null",
            ext_is_null, ext_reg
        ));
        let gpu_panic = self.next_label();
        let gpu_ok = self.next_label();
        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            ext_is_null, gpu_panic, gpu_ok
        ));

        self.emit_label(&gpu_panic);
        let gpu_err = self.compile_static_c_string_literal(
            "shader surface requires GPU backend (set RENDERER_BACKEND=vulkan|d3d12|webgpu)"
        );
        self.emit(&format!("  call void @kain_runtime_panic(i8* {})", gpu_err));
        self.emit("  unreachable");

        // Load shader (extension offset 0)
        self.emit_label(&gpu_ok);
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
        // TODO: embed SPIR-V hex as a static global for the shader fragment
        let _shader_name_str = self.compile_static_c_string_literal(shader_fragment_name);
        let _load_result = self.next_reg();
        let spirv_placeholder = self.compile_static_c_string_literal("");
        self.emit(&format!(
            "  {} = call i64 {}(i64 {}, i8* {}) ; SPIR-V placeholder (TODO: embed real hex)",
            _load_result, load_fn, session_id, spirv_placeholder
        ));

        // Frame loop
        let frame_loop_label = self.next_label();
        self.emit(&format!("  br label %{}", frame_loop_label));
        self.emit_label(&frame_loop_label);

        // Alloca for time accumulator (Float, 4 bytes)
        let time_addr = self.next_reg();
        self.emit(&format!("  {} = alloca float, align 4", time_addr));
        self.emit(&format!("  store float 0.0, float* {}", time_addr));

        // host_pump (vtable offset 16)
        let _pump_ok = self.emit_vtable_call(
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
        let delta_float = self.next_reg();
        self.emit(&format!(
            "  {} = fptrunc double {} to float",
            delta_float, delta
        ));
        self.emit_vtable_call_void(
            &surface_reg,
            OFF_BEGIN_FRAME,
            "void (i64, double)*",
            &[(&session_id, "i64"), (&delta, "double")],
        );

        // Update uniforms via extension
        // Binding 0: time (Float, 4 bytes)
        self.emit_gpu_set_uniform(
            &ext_typed, &session_id, 0, &time_addr, "float", 4,
        );

        // Binding 1: resolution (Vec2, 8 bytes) - alloca two floats
        let res_addr = self.next_reg();
        self.emit(&format!("  {} = alloca [2 x float], align 4", res_addr));
        let res_x_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = getelementptr inbounds [2 x float], [2 x float]* {}, i32 0, i32 0",
            res_x_ptr, res_addr
        ));
        self.emit(&format!("  store float 0x1.4p+10, float* {}", res_x_ptr));
        let res_y_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = getelementptr inbounds [2 x float], [2 x float]* {}, i32 0, i32 1",
            res_y_ptr, res_addr
        ));
        self.emit(&format!("  store float 0x1.68p+9, float* {}", res_y_ptr));
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
        let delta_sec = self.next_reg();
        self.emit(&format!(
            "  {} = fdiv float {}, 1.0e+3",
            delta_sec, delta_float
        ));
        let old_time = self.next_reg();
        self.emit(&format!("  {} = load float, float* {}", old_time, time_addr));
        let new_time = self.next_reg();
        self.emit(&format!(
            "  {} = fadd float {}, {}",
            new_time, old_time, delta_sec
        ));
        self.emit(&format!("  store float {}, float* {}", new_time, time_addr));

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
        let shutdown = self.next_label();
        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            keep_going, frame_loop_label, shutdown
        ));

        // Shutdown
        self.emit_label(&shutdown);
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
    ) -> KainResult<()> {
        self.declare_surface_trait_types();

        // Branch: shader surfaces emit a GPU shader loop (no component render)
        if surface_kind == "shader" {
            return self.compile_shader_surface_loop(world_name, root_component_name);
        }

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
        // Default dimensions: 1280x720
        let session_id = self.emit_vtable_call(
            &surface_reg,
            OFF_SESSION_CREATE,
            "i64 (i8*, i64, i64)*",
            &[
                (&session_name_str, "i8*"),
                ("1280", "i64"),
                ("720", "i64"),
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
                ("1280", "i64"),
                ("720", "i64"),
            ],
        );

        // Fall through to frame loop
        let frame_loop_label = self.next_label();
        self.emit(&format!("  br label %{}", frame_loop_label));

        // ── Frame loop ─────────────────────────────────────────────
        self.emit_label(&frame_loop_label);

        // host_pump (vtable offset 16) — process OS messages
        // On Win32: PeekMessageA → TranslateMessage → DispatchMessageA
        // Keeps the window responsive (close, resize, input).
        let _pump_ok = self.emit_vtable_call(
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

        // Render root component — pass surface, session, and root parent (0)
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
        let shutdown = self.next_label();
        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            keep_going, frame_loop_label, shutdown
        ));

        // ── Shutdown ───────────────────────────────────────────────
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

    /// `{expression}` → evaluate, then emit as `"text"` element via vtable
    fn compile_jsx_expression(
        &mut self,
        surface_reg: &str,
        session_reg: &str,
        parent_reg: &str,
        expr: &Expr,
        component_name: &str,
        sibling_index: &mut usize,
    ) -> KainResult<()> {
        let (val, ty) = self.compile_expr(expr)?;
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
    fn compile_jsx_component_call(
        &mut self,
        surface_reg: &str,
        session_reg: &str,
        parent_reg: &str,
        name: &str,
        props: &[JSXAttribute],
        children: &[JSXNode],
        _component_name: &str,
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
                }
            } else {
                // Prop not provided → use zero/empty default
                let zero = self.zero_value_for_ty(_prop_ty);
                compiled_args.push((zero, _prop_ty.clone()));
            }
        }

        // Children — not passed as a separate arg; component manages its own
        // children via JSX body.
        let _ = children;

        let arg_str = compiled_args
            .iter()
            .map(|(val, ty)| format!("{} {}", ty, val))
            .collect::<Vec<_>>()
            .join(", ");

        self.emit(&format!(
            "  call void @{}({})",
            render_name, arg_str
        ));
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

    /// Map a JSX attribute to the correct vtable call (Contract 11)
    fn compile_jsx_attr(
        &mut self,
        surface_reg: &str,
        session_reg: &str,
        element_reg: &str,
        attr: &JSXAttribute,
    ) -> KainResult<()> {
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
    ) -> KainResult<Vec<(String, String)>> {
        let mut state_fields: Vec<(String, String)> = Vec::new();
        for state in &component.ast.state {
            let key = format!("{}:{}", component.ast.name, state.name);
            let key_str = self.compile_static_c_string_literal(&key);

            // state_get_i64 through vtable (offset 8)
            let stored_val = self.emit_vtable_call(
                surface_reg,
                OFF_STATE_GET_I64,
                "i64 (i64, i8*)*",
                &[(session_reg, "i64"), (&key_str, "i8*")],
            );

            // Check if first frame (get returns 0)
            let is_first = self.next_reg();
            self.emit(&format!(
                "  {} = icmp eq i64 {}, 0",
                is_first, stored_val
            ));

            let init_block = self.next_label();
            let load_block = self.next_label();
            self.emit(&format!(
                "  br i1 {}, label %{}, label %{}",
                is_first, init_block, load_block
            ));

            // Alloca for the state field (local fast access) — hoisted to entry
            let addr_reg = format!("%{}.addr", state.name);
            self.emit_entry_alloca(&addr_reg, "i64");
            // Track in locals so $self.name works BEFORE init_block emits
            self.locals.insert(state.name.clone(), (addr_reg.clone(), "i64".to_string()));
            if let Some(scope) = self.scopes.last_mut() {
                scope.push(state.name.clone());
            }

            self.emit_label(&init_block);
            // Store initial value through vtable (offset 9)
            let (init_val, _) = self.compile_expr(&state.initial)?;
            self.emit_vtable_call_void(
                surface_reg,
                OFF_STATE_SET_I64,
                "void (i64, i8*, i64)*",
                &[(session_reg, "i64"), (&key_str, "i8*"), (&init_val, "i64")],
            );
            self.emit(&format!("  br label %{}", load_block));

            self.emit_label(&load_block);
            // PHI: merge init_val (first frame) with stored_val (subsequent frames)
            // NOTE: sentinel 0 means "first frame" — initial value of 0 would
            // cause re-init every frame. Future: use a separate init-flag key.
            let phi_reg = self.next_reg();
            self.emit(&format!(
                "  {} = phi i64 [ {}, %{} ], [ {}, %entry ]",
                phi_reg, init_val, init_block, stored_val
            ));
            self.emit(&format!("  store i64 {}, i64* {}", phi_reg, addr_reg));

            // Track for write-back at end of render function
            state_fields.push((key, addr_reg));
        }

        Ok(state_fields)
    }
}
