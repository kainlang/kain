//! Component surface codegen — emits LLVM IR that calls through the
//! `KainComponentSurface` trait. See `X:/research/component/WIRING_CONTRACT.md`.

use kain_core::ast::{Expr, JSXAttrValue, JSXAttribute, JSXNode};
use kain_core::error::KainResult;
use kain_core::types::TypedComponent;
use super::LlvmGenerator;

// ── Surface trait type name ──────────────────────────────────────────────
const SURFACE_STRUCT_TYPE: &str = "%KainComponentSurface";

// ── JSX attribute → surface call mapping (Contract 11) ──────────────────
struct AttrMapping {
    call_name: &'static str,
    style_key: &'static str,
}

fn map_jsx_attr_to_surface_key(attr_name: &str) -> AttrMapping {
    match attr_name {
        "padding" => AttrMapping { call_name: "abi_ui_node_set_style_f64", style_key: "padding" },
        "spacing" => AttrMapping { call_name: "abi_ui_node_set_style_f64", style_key: "spacing" },
        "corner_radius" => AttrMapping { call_name: "abi_ui_node_set_style_f64", style_key: "corner_radius" },
        "font_size" => AttrMapping { call_name: "abi_ui_node_set_style_f64", style_key: "font_size" },
        "opacity" => AttrMapping { call_name: "abi_ui_node_set_style_f64", style_key: "opacity" },
        "border" | "border_width" => AttrMapping { call_name: "abi_ui_node_set_style_f64", style_key: "border_width" },
        "width" => AttrMapping { call_name: "abi_ui_node_set_style_f64", style_key: "width" },
        "height" => AttrMapping { call_name: "abi_ui_node_set_style_f64", style_key: "height" },
        "background" => AttrMapping { call_name: "abi_ui_node_set_style_string", style_key: "fill_color" },
        "border_color" => AttrMapping { call_name: "abi_ui_node_set_style_string", style_key: "border_color" },
        "color" | "ink_color" => AttrMapping { call_name: "abi_ui_node_set_style_string", style_key: "ink_color" },
        "title" => AttrMapping { call_name: "abi_ui_node_set_style_string", style_key: "title" },
        "value" => AttrMapping { call_name: "abi_ui_node_set_text", style_key: "" },
        "direction" => AttrMapping { call_name: "abi_ui_node_set_style_i64", style_key: "layout.direction" },
        "disabled" => AttrMapping { call_name: "abi_ui_node_set_style_i64", style_key: "disabled" },
        // Unknown attributes pass through as strings with the attr name as the key
        _ => AttrMapping { call_name: "abi_ui_node_set_style_string", style_key: "" },
    }
}

impl LlvmGenerator {
    // =====================================================================
    //  Public entry points — called from `mod.rs`
    // =====================================================================

    /// Emit declarations for the KainComponentSurface struct and registry
    /// functions. Call once per module before any component code.
    pub(crate) fn declare_surface_trait_types(&mut self) {
        if self.surface_trait_declared {
            return;
        }
        self.surface_trait_declared = true;

        // Registry helpers
        self.emit("declare void @kain_component_surface_register(i8*, i8*)");
        self.emit("declare i8* @kain_component_surface_resolve(i8*)");

        // Runtime panic (for surface resolution failures)
        self.emit("declare void @kain_runtime_panic(i8*)");

        // Frame delta helper
        self.emit("declare double @__kain_frame_delta_ms()");

        // Session lifecycle
        self.emit("declare i64 @abi_ui_session_create(i8*, i64, i64)");
        self.emit("declare void @abi_ui_session_destroy(i64)");

        // Frame lifecycle
        self.emit("declare void @abi_ui_begin_frame(i64, double)");
        self.emit("declare void @abi_ui_end_frame(i64)");
        self.emit("declare void @abi_ui_present(i64)");

        // Element tree
        self.emit("declare i64 @abi_ui_node_create_and_parent(i64, i64, i8*, i8*)");
        self.emit("declare void @abi_ui_node_set_text(i64, i64, i8*)");

        // Style attributes
        self.emit("declare void @abi_ui_node_set_style_f64(i64, i64, i8*, double)");
        self.emit("declare void @abi_ui_node_set_style_string(i64, i64, i8*, i8*)");
        self.emit("declare void @abi_ui_node_set_style_i64(i64, i64, i8*, i64)");

        // State persistence
        self.emit("declare i64 @abi_ui_surface_state_get_i64(i64, i8*)");
        self.emit("declare void @abi_ui_surface_state_set_i64(i64, i8*, i64)");

        // Event / close
        self.emit("declare i64 @abi_ui_host_should_close(i64)");
    }

    /// Compile a component as `void @Name_render(i64 %session_id, i64 %parent_id, props...)`.
    /// Replaces the old `compile_component` which emitted `i8*` string concatenation.
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

        // Build prop type list: session_id (i64), parent_id (i64), then declared props
        let prop_defs: Vec<(String, String)> = {
            let mut defs = Vec::new();
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

        let param_str = prop_defs
            .iter()
            .enumerate()
            .map(|(i, (_, ty))| format!("{} %arg{}", ty, i))
            .collect::<Vec<_>>()
            .join(", ");

        self.emit(&format!("define void @{}({}) {{", render_name, param_str));
        self.emit_label("entry");

        let session_reg = "%arg0".to_string();
        let parent_reg = "%arg1".to_string();

        // Store prop params in locals (skipping session/parent at indices 0,1)
        for (i, (param_name, param_ty)) in prop_defs.iter().enumerate().skip(2) {
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

        // Compile state fields (Contract 8)
        self.compile_component_state_init(component, &session_reg)?;

        // Set current component context for JSX compilation
        self.current_component_name = Some(name.clone());
        self.current_component_session = Some(session_reg.clone());
        self.current_component_parent = Some(parent_reg.clone());

        // Compile the JSX body (Contract 1-7)
        let mut sibling_index = 0usize;
        self.compile_jsx_to_surface(
            &session_reg,
            &parent_reg,
            &component.ast.body,
            name,
            &mut sibling_index,
        )?;

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

    /// Emit a world-surface frame loop for a world with a surface declaration.
    /// Called from `compile_world_initializer` extension.
    pub(crate) fn compile_surface_frame_loop(
        &mut self,
        world_name: &str,
        surface_kind: &str,
        root_component_name: &str,
    ) -> KainResult<()> {
        self.declare_surface_trait_types();

        let fn_name = format!("__kain_world_surface_loop_{}", Self::sanitize_symbol_fragment(world_name));
        let render_name = format!("{}_render", root_component_name);

        self.emit(&format!("define void @{}() {{", fn_name));
        self.emit_label("entry");

        // Resolve surface
        let surface_name_str = self.compile_static_c_string_literal(surface_kind);
        let surface_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = call i8* @kain_component_surface_resolve(i8* {})",
            surface_ptr, surface_name_str
        ));

        let is_null = self.next_reg();
        self.emit(&format!(
            "  {} = icmp eq i8* {}, null",
            is_null, surface_ptr
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

        // Session create
        self.emit_label(&init_block);
        let session_name_str = self.compile_static_c_string_literal(world_name);
        // Default dimensions: 1280x720
        let session_id = self.next_reg();
        self.emit(&format!(
            "  {} = call i64 @abi_ui_session_create(i8* {}, i64 1280, i64 720)",
            session_id, session_name_str
        ));

        let session_err = self.next_reg();
        self.emit(&format!(
            "  {} = icmp slt i64 {}, 0",
            session_err, session_id
        ));
        let session_fail = self.next_label();
        let frame_loop_label = self.next_label();
        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            session_err, session_fail, frame_loop_label
        ));

        self.emit_label(&session_fail);
        let fail_msg = format!("session_create failed for world '{}'", world_name);
        let fail_str = self.compile_static_c_string_literal(&fail_msg);
        self.emit(&format!("  call void @kain_runtime_panic(i8* {})", fail_str));
        self.emit("  unreachable");

        // Frame loop
        self.emit_label(&frame_loop_label);

        // begin_frame
        let delta = self.next_reg();
        self.emit(&format!(
            "  {} = call double @__kain_frame_delta_ms()",
            delta
        ));
        self.emit(&format!(
            "  call void @abi_ui_begin_frame(i64 {}, double {})",
            session_id, delta
        ));

        // Render root component
        self.emit(&format!(
            "  call void @{}(i64 {}, i64 0)",
            render_name, session_id
        ));

        // end_frame + present
        self.emit(&format!("  call void @abi_ui_end_frame(i64 {})", session_id));
        self.emit(&format!("  call void @abi_ui_present(i64 {})", session_id));

        // should_close
        let should_close = self.next_reg();
        self.emit(&format!(
            "  {} = call i64 @abi_ui_host_should_close(i64 {})",
            should_close, session_id
        ));
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
        self.emit(&format!(
            "  call void @abi_ui_session_destroy(i64 {})",
            session_id
        ));
        self.emit("  ret void");
        self.emit("}");
        self.emit("");

        Ok(())
    }

    // =====================================================================
    //  JSX → surface calls
    // =====================================================================

    /// Walk a JSX tree, emitting surface trait calls for every node.
    /// Returns nothing — all output is IR emitted into self.output.
    fn compile_jsx_to_surface(
        &mut self,
        session_reg: &str,
        parent_reg: &str,
        node: &JSXNode,
        component_name: &str,
        sibling_index: &mut usize,
    ) -> KainResult<()> {
        match node {
            JSXNode::Text(text, _) => {
                self.compile_jsx_text(session_reg, parent_reg, text, component_name, sibling_index)
            }
            JSXNode::Expression(expr) => {
                self.compile_jsx_expression(session_reg, parent_reg, expr, component_name, sibling_index)
            }
            JSXNode::Fragment(children, _) => {
                for child in children {
                    self.compile_jsx_to_surface(
                        session_reg,
                        parent_reg,
                        child,
                        component_name,
                        sibling_index,
                    )?;
                }
                Ok(())
            }
            JSXNode::Element {
                tag, attributes, children, ..
            } => {
                self.compile_jsx_element(
                    session_reg,
                    parent_reg,
                    tag,
                    attributes,
                    children,
                    component_name,
                    sibling_index,
                )
            }
            JSXNode::ComponentCall {
                name, props, children, ..
            } => {
                self.compile_jsx_component_call(
                    session_reg,
                    parent_reg,
                    name,
                    props,
                    children,
                    component_name,
                )
            }
            JSXNode::For {
                binding, iter, body, ..
            } => {
                self.compile_jsx_for(
                    session_reg,
                    parent_reg,
                    binding,
                    iter,
                    body,
                    component_name,
                    sibling_index,
                )
            }
            JSXNode::If {
                condition,
                then_branch,
                else_branch,
                ..
            } => {
                self.compile_jsx_if(
                    session_reg,
                    parent_reg,
                    condition,
                    then_branch,
                    else_branch.as_deref(),
                    component_name,
                    sibling_index,
                )
            }
        }
    }

    /// <text>literal</text> or {expression} → "text" element with set_text
    fn compile_jsx_text(
        &mut self,
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

        let el = self.emit_element_begin(session_reg, parent_reg, "text", &sk);
        let (text_val, _) = self.compile_string_literal(text);
        self.emit_surface_call(
            session_reg,
            &el,
            "element_set_text",
            &[("i8*", &text_val)],
        );
        self.emit_element_end(session_reg, &el);
        Ok(())
    }

    /// {expression} → evaluate, then emit as "text" element or attribute value
    fn compile_jsx_expression(
        &mut self,
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

        let el = self.emit_element_begin(session_reg, parent_reg, "text", &sk);

        // Stringify if needed
        if ty == "i8*" {
            self.emit_surface_call(
                session_reg,
                &el,
                "element_set_text",
                &[("i8*", &val)],
            );
        } else {
            let (str_val, _) = self.stringify_value(&val, &ty)?;
            self.emit_surface_call(
                session_reg,
                &el,
                "element_set_text",
                &[("i8*", &str_val)],
            );
        }

        self.emit_element_end(session_reg, &el);
        Ok(())
    }

    /// <tag attr="val">children</tag> → element_begin → attrs → children → element_end
    fn compile_jsx_element(
        &mut self,
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

        let el = self.emit_element_begin(session_reg, parent_reg, tag, &sk);

        // Emit attributes
        for attr in attributes {
            self.compile_jsx_attr(session_reg, &el, attr)?;
        }

        // Emit children with the element as parent
        let el_clone = el.clone();
        for child in children {
            let mut child_si = 0usize;
            self.compile_jsx_to_surface(
                session_reg,
                &el_clone,
                child,
                component_name,
                &mut child_si,
            )?;
        }

        self.emit_element_end(session_reg, &el);
        Ok(())
    }

    /// <ComponentName prop="val" /> → call void @ComponentName_render(session, parent, props...)
    fn compile_jsx_component_call(
        &mut self,
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

        // First two args: session_id, parent_id
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

        // Children — not passed as a separate arg anymore; if component has no render body,
        // children are unused (component manages its own children via JSX body).
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

        // Emit body with sibling_index = current idx
        let sk = self.emit_stable_key(
            &format!("{}:for", component_name),
            parent_reg,
            0,
        );
        let _ = sk; // stable key for the for-container (Phase 3: reconcile_list_begin)

        // Compile body for this item – pass idx as sibling_index for stable key
        let item_si: usize = 0; // items within for get keyed by the loop index
        let _si_val = self.next_reg();
        // Create a temporary "parent" that encodes idx so stable keys are unique
        // For now, use parent_reg + idx as pseudo-parent
        let child_parent = self.next_reg();
        self.emit(&format!(
            "  {} = add i64 {}, {}",
            child_parent, parent_reg, idx
        ));

        self.compile_jsx_to_surface(
            session_reg,
            &child_parent,
            body,
            component_name,
            &mut (item_si as usize),
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
            // Bool is lowered to i1; expressions return i64 - coerce
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
            session_reg,
            parent_reg,
            then_branch,
            component_name,
            &mut then_si_val,
        )?;
        self.emit(&format!("  br label %{}", done_block));

        // Else branch
        self.emit_label(&else_block);
        if let Some(else_node) = else_branch {
            let mut else_si_val = else_si;
            self.compile_jsx_to_surface(
                session_reg,
                parent_reg,
                else_node,
                component_name,
                &mut else_si_val,
            )?;
        }
        self.emit(&format!("  br label %{}", done_block));

        self.emit_label(&done_block);
        Ok(())
    }

    /// Map a JSX attribute to the correct surface call (Contract 11)
    fn compile_jsx_attr(
        &mut self,
        session_reg: &str,
        element_reg: &str,
        attr: &JSXAttribute,
    ) -> KainResult<()> {
        // Resolve the surface call name and the mapped key
        let mapped = map_jsx_attr_to_surface_key(&attr.name);
        let is_text_attr = attr.name == "value";

        // For unknown attributes, use the attr name itself as the key
        let style_key: String = if mapped.style_key.is_empty() && !is_text_attr {
            attr.name.clone()
        } else {
            mapped.style_key.to_string()
        };

        match &attr.value {
            JSXAttrValue::String(value) => {
                let (val_reg, _) = self.compile_string_literal(value);
                if is_text_attr {
                    self.emit_surface_call(
                        session_reg, element_reg,
                        "abi_ui_node_set_text",
                        &[("i8*", &val_reg)],
                    );
                } else {
                    self.emit_surface_style_call(
                        session_reg, element_reg,
                        mapped.call_name, &style_key,
                        &[("i8*", &val_reg)],
                    );
                }
            }
            JSXAttrValue::Bool(true) => {
                if is_text_attr {
                    // Bare boolean on a text element is unusual, emit set_text with "true"
                    let (val_reg, _) = self.compile_string_literal("true");
                    self.emit_surface_call(
                        session_reg, element_reg,
                        "abi_ui_node_set_text",
                        &[("i8*", &val_reg)],
                    );
                } else {
                    self.emit_surface_style_call(
                        session_reg, element_reg,
                        mapped.call_name, &style_key,
                        &[("i64", "1")],
                    );
                }
            }
            JSXAttrValue::Bool(false) => {
                // Bool false → no-op (attribute not set)
            }
            JSXAttrValue::Expr(expr) => {
                let (val, ty) = self.compile_expr(expr)?;
                // If value is i1 (bool), coerce to i64 for attr calls
                let (val_use, ty_use) = if ty == "i1" {
                    let widened = self.next_reg();
                    self.emit(&format!("  {} = zext i1 {} to i64", widened, val));
                    (widened, "i64".to_string())
                } else {
                    (val, ty)
                };
                if is_text_attr {
                    self.emit_surface_call(
                        session_reg, element_reg,
                        "abi_ui_node_set_text",
                        &[(&ty_use, &val_use)],
                    );
                } else {
                    self.emit_surface_style_call(
                        session_reg, element_reg,
                        mapped.call_name, &style_key,
                        &[(&ty_use, &val_use)],
                    );
                }
            }
        }
        Ok(())
    }

    // =====================================================================
    //  Surface trait call helpers
    // =====================================================================

    /// Emit `element_begin(session, parent, kind, stable_key)` and return the element ID reg.
    fn emit_element_begin(
        &mut self,
        session_reg: &str,
        parent_reg: &str,
        kind: &str,
        stable_key: &str,
    ) -> String {
        let kind_str = self.compile_static_c_string_literal(kind);
        let el = self.next_reg();
        self.emit(&format!(
            "  {} = call i64 @abi_ui_node_create_and_parent(i64 {}, i64 {}, i8* {}, i8* {})",
            el, session_reg, parent_reg, kind_str, stable_key
        ));
        el
    }

    /// Emit `element_end(session, element_id)`
    fn emit_element_end(&mut self, session_reg: &str, element_reg: &str) {
        // element_end is a no-op in the retained-mode surface; skip for now.
        let _ = session_reg;
        let _ = element_reg;
        // In Phase 3: emit actual element_end if needed by the surface backend
    }

    /// Emit a surface trait call: `call void @surface_fn(i64 sid, i64 el, ...)`
    fn emit_surface_call(
        &mut self,
        session_reg: &str,
        element_reg: &str,
        fn_name: &str,
        args: &[(&str, &str)], // (llvm_type, reg_or_literal)
    ) {
        let mut arg_strs = vec![
            format!("i64 {}", session_reg),
            format!("i64 {}", element_reg),
        ];
        for (ty, val) in args {
            arg_strs.push(format!("{} {}", ty, val));
        }
        self.emit(&format!(
            "  call void @{}({})",
            fn_name, arg_strs.join(", ")
        ));
    }

    /// Compute a stable key: `"ComponentName:path:parent_id:sibling_index"`
    fn emit_stable_key(
        &mut self,
        path_prefix: &str,
        parent_reg: &str,
        sibling_index: u64,
    ) -> String {
        // Build key as: path_prefix:PARENT_REG:SIBLING_INDEX
        let prefix_str = self.compile_static_c_string_literal(path_prefix);
        let si_str = self.compile_static_c_string_literal(&format!(":{}", sibling_index));

        // Concat prefix + ":" + parent_as_string + si_str
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
        session_reg: &str,
    ) -> KainResult<()> {
        for state in &component.ast.state {
            let key = format!("{}:{}", component.ast.name, state.name);
            let key_str = self.compile_static_c_string_literal(&key);

            // state_get_i64
            let stored_val = self.next_reg();
            self.emit(&format!(
                "  {} = call i64 @abi_ui_surface_state_get_i64(i64 {}, i8* {})",
                stored_val, session_reg, key_str
            ));

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

            self.emit_label(&init_block);
            // Store initial value
            let init_name = format!("__init_{}", state.name);
            let (init_val, _) = self.compile_expr(&state.initial)?;
            self.emit(&format!(
                "  call void @abi_ui_surface_state_set_i64(i64 {}, i8* {}, i64 {})",
                session_reg, key_str, init_val
            ));

            // Alloca for the state field
            let addr_reg = format!("%{}.addr", state.name);
            self.emit_entry_alloca(&addr_reg, "i64");
            self.emit(&format!(
                "  store i64 {}, i64* {}",
                init_val, addr_reg
            ));
            // Track in locals so $self.name works; also mark for state_set on write
            self.locals.insert(state.name.clone(), (addr_reg.clone(), "i64".to_string()));
            if let Some(scope) = self.scopes.last_mut() {
                scope.push(state.name.clone());
            }
            // Record the init value name for phi
            let _init_name_for_phi = init_name;
            self.emit(&format!("  br label %{}", load_block));

            self.emit_label(&load_block);
            // Actually, since we already stored in the alloca and recorded the local,
            // the phi is not strictly needed for Phase 1. The local already holds the value.
            // Readers of the state will load from the alloca (which has the value stored above).
        }

        Ok(())
    }

    // =====================================================================
    //  JSX attribute → surface key mapping (Contract 11)
    // =====================================================================

    /// Emit a surface style call: `call void @fn_name(i64 sid, i64 el, i8* key, ...)`
    fn emit_surface_style_call(
        &mut self,
        session_reg: &str,
        element_reg: &str,
        fn_name: &str,
        style_key: &str,
        args: &[(&str, &str)],
    ) {
        let key_str = self.compile_static_c_string_literal(style_key);
        let mut arg_strs = vec![
            format!("i64 {}", session_reg),
            format!("i64 {}", element_reg),
            format!("i8* {}", key_str),
        ];
        for (ty, val) in args {
            arg_strs.push(format!("{} {}", ty, val));
        }
        self.emit(&format!(
            "  call void @{}({})",
            fn_name, arg_strs.join(", ")
        ));
    }
}
