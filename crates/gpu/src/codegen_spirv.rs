//! SPIR-V Code Generation for GPU shaders

use kain_core::types::{TypedProgram, TypedItem, TypedShader};
use kain_core::error::{KainResult, KainError};
use kain_core::ast::{Type, ShaderStage, Expr, Stmt, Block, BinaryOp, UnaryOp, ElseBranch, Pattern};
use rspirv::binary::Assemble;
use rspirv::dr::{Builder, Operand};
use rspirv::spirv::{Capability, AddressingModel, MemoryModel, ExecutionModel, ExecutionMode, StorageClass, Decoration};
use std::collections::{HashMap, HashSet};

pub fn generate(program: &TypedProgram) -> KainResult<Vec<u8>> {
    let mut builder = Builder::new();
    
    // Set capabilities and memory model
    builder.capability(Capability::Shader);
    // Add VulkanMemoryModel if targeting Vulkan, but GLSL450 is standard for now
    builder.memory_model(AddressingModel::Logical, MemoryModel::GLSL450);
    
    for item in &program.items {
        if let TypedItem::Shader(shader) = item {
            emit_shader(&mut builder, shader)?;
        }
    }
    
    let module = builder.module();
    let bytes: Vec<u8> = module.assemble().iter().flat_map(|w| w.to_le_bytes()).collect();
    Ok(bytes)
}

struct ShaderContext<'a> {
    b: &'a mut Builder,
    // Name -> (SPIR-V ID, AST Type, IsPointer)
    vars: HashMap<String, VarBinding>,
    output_var: Option<u32>,
    // Track which variables are struct-wrapped uniforms (need AccessChain)
    struct_uniforms: HashSet<String>,
    // Track storage-buffer uniforms to emit runtime array indexing loads/stores.
    storage_buffers: HashSet<String>,
    // Cache GLSL extension import
    glsl_ext: Option<u32>,
    loop_continue_targets: Vec<u32>,
    loop_break_targets: Vec<u32>,
}

#[derive(Clone)]
struct VarBinding {
    id: u32,
    ty: Type,
    is_ptr: bool,
}

fn emit_shader(b: &mut Builder, shader: &TypedShader) -> KainResult<()> {
    let exec_model = match shader.ast.stage {
        ShaderStage::Vertex => ExecutionModel::Vertex,
        ShaderStage::Fragment => ExecutionModel::Fragment,
        ShaderStage::Compute => ExecutionModel::GLCompute,
        ShaderStage::Surface => ExecutionModel::Fragment, // Surface shaders compile to fragment
    };
    
    // 1. Define Basic Types
    let void = b.type_void();
    
    // 2. Define Entry Point Function Type
    let fn_void_void = b.type_function(void, vec![]);
    
    // 3. Declare Variables (Global Interface)
    let mut interface_vars = vec![];
    let mut ctx_vars = HashMap::new();
    let mut struct_uniforms = HashSet::new();
    let mut storage_buffers = HashSet::new();
    let mut local_size_spec_ids: Option<[u32; 3]> = None;

    // Inputs
    for (i, param) in shader.ast.inputs.iter().enumerate() {
        let ty = map_ast_type(b, &param.ty);
        let ptr_ty = b.type_pointer(None, StorageClass::Input, ty);
        let var = b.variable(ptr_ty, None, StorageClass::Input, None);
        b.decorate(var, Decoration::Location, vec![Operand::LiteralBit32(i as u32)]);
        interface_vars.push(var);
        ctx_vars.insert(param.name.clone(), VarBinding { id: var, ty: param.ty.clone(), is_ptr: true });
    }

    // Outputs
    let output_var = if !is_void(&shader.ast.outputs) {
         let output_ty = map_ast_type(b, &shader.ast.outputs);
         let ptr_ty = b.type_pointer(None, StorageClass::Output, output_ty);
         let var = b.variable(ptr_ty, None, StorageClass::Output, None);
         
         // Vertex shader output is @builtin(position) for Vec4, otherwise use Location
         if exec_model == ExecutionModel::Vertex && is_vec4(&shader.ast.outputs) {
             b.decorate(var, Decoration::BuiltIn, vec![Operand::BuiltIn(rspirv::spirv::BuiltIn::Position)]);
         } else {
             b.decorate(var, Decoration::Location, vec![Operand::LiteralBit32(0)]);
         }
         
         interface_vars.push(var);
         Some(var)
    } else {
        None
    };

    // Uniforms
    for uniform in &shader.ast.uniforms {
        if matches!(shader.ast.stage, ShaderStage::Compute) && is_local_size_param(&uniform.name) {
            let slot = match uniform.name.as_str() {
                "LOCAL_SIZE_X" => 0usize,
                "LOCAL_SIZE_Y" => 1usize,
                "LOCAL_SIZE_Z" => 2usize,
                _ => 0usize,
            };
            let default_value = match slot {
                0 | 1 => 8,
                _ => 1,
            };
            let spec = emit_u32_spec_constant(b, default_value, uniform.binding);
            let mut ids = local_size_spec_ids.unwrap_or([0, 0, 0]);
            ids[slot] = spec;
            local_size_spec_ids = Some(ids);
            ctx_vars.insert(
                uniform.name.clone(),
                VarBinding {
                    id: spec,
                    ty: Type::Named { name: "UInt".into(), generics: vec![], span: uniform.span },
                    is_ptr: false,
                },
            );
            continue;
        }
        if is_permutation_param(&uniform.name) {
            if let Some(spec_id) = emit_permutation_spec_constant(b, &uniform.ty, uniform.binding) {
                ctx_vars.insert(
                    uniform.name.clone(),
                    VarBinding {
                        id: spec_id,
                        ty: uniform.ty.clone(),
                        is_ptr: false,
                    },
                );
                continue;
            }
        }

        let inner_ty = map_ast_type(b, &uniform.ty);
        
        // Check if this is a sampler type (uses UniformConstant) or data type (uses Uniform with struct)
        let is_sampler = matches!(&uniform.ty, Type::Named { name, .. } if name == "Sampler2D");
        
        if is_sampler {
            // Samplers use UniformConstant storage class directly
            let ptr_ty = b.type_pointer(None, StorageClass::UniformConstant, inner_ty);
            let var = b.variable(ptr_ty, None, StorageClass::UniformConstant, None);
            b.decorate(var, Decoration::DescriptorSet, vec![Operand::LiteralBit32(0)]);
            b.decorate(var, Decoration::Binding, vec![Operand::LiteralBit32(uniform.binding)]);
            ctx_vars.insert(uniform.name.clone(), VarBinding { id: var, ty: uniform.ty.clone(), is_ptr: true });
        } else {
            let is_storage_buffer = is_storage_buffer(&uniform.ty);
            // Data uniforms (matrices, vectors, etc.) need a struct wrapper with Block decoration.
            let struct_ty = b.type_struct(vec![inner_ty]);
            b.decorate(struct_ty, Decoration::Block, vec![]);
            // Offset decoration for the first (and only) member
            b.member_decorate(struct_ty, 0, Decoration::Offset, vec![Operand::LiteralBit32(0)]);

            // For matrices, we need ColMajor and MatrixStride decorations
            if matches!(&uniform.ty, Type::Named { name, .. } if name == "Mat4") {
                b.member_decorate(struct_ty, 0, Decoration::ColMajor, vec![]);
                b.member_decorate(struct_ty, 0, Decoration::MatrixStride, vec![Operand::LiteralBit32(16)]);
            }

            let storage_class = if is_storage_buffer {
                StorageClass::StorageBuffer
            } else {
                StorageClass::Uniform
            };
            let ptr_ty = b.type_pointer(None, storage_class, struct_ty);
            let var = b.variable(ptr_ty, None, storage_class, None);
            b.decorate(var, Decoration::DescriptorSet, vec![Operand::LiteralBit32(0)]);
            b.decorate(var, Decoration::Binding, vec![Operand::LiteralBit32(uniform.binding)]);
            ctx_vars.insert(uniform.name.clone(), VarBinding { id: var, ty: uniform.ty.clone(), is_ptr: true });
            if is_storage_buffer {
                storage_buffers.insert(uniform.name.clone());
            } else {
                struct_uniforms.insert(uniform.name.clone());
            }
        }
    }

    if exec_model == ExecutionModel::GLCompute {
        let uint = b.type_int(32, 0);
        let uvec3 = b.type_vector(uint, 3);
        let uvec3_ptr = b.type_pointer(None, StorageClass::Input, uvec3);
        let uint_ptr = b.type_pointer(None, StorageClass::Input, uint);

        let gid = b.variable(uvec3_ptr, None, StorageClass::Input, None);
        b.decorate(gid, Decoration::BuiltIn, vec![Operand::BuiltIn(rspirv::spirv::BuiltIn::GlobalInvocationId)]);
        interface_vars.push(gid);

        let lid = b.variable(uvec3_ptr, None, StorageClass::Input, None);
        b.decorate(lid, Decoration::BuiltIn, vec![Operand::BuiltIn(rspirv::spirv::BuiltIn::LocalInvocationId)]);
        interface_vars.push(lid);

        let wid = b.variable(uvec3_ptr, None, StorageClass::Input, None);
        b.decorate(wid, Decoration::BuiltIn, vec![Operand::BuiltIn(rspirv::spirv::BuiltIn::WorkgroupId)]);
        interface_vars.push(wid);

        let lindex = b.variable(uint_ptr, None, StorageClass::Input, None);
        b.decorate(lindex, Decoration::BuiltIn, vec![Operand::BuiltIn(rspirv::spirv::BuiltIn::LocalInvocationIndex)]);
        interface_vars.push(lindex);

        let uvec3_ty = Type::Named { name: "UVec3".into(), generics: vec![], span: shader.ast.span };
        let uint_ty = Type::Named { name: "UInt".into(), generics: vec![], span: shader.ast.span };
        ctx_vars.insert("global_invocation_id".into(), VarBinding { id: gid, ty: uvec3_ty.clone(), is_ptr: true });
        ctx_vars.insert("local_invocation_id".into(), VarBinding { id: lid, ty: uvec3_ty.clone(), is_ptr: true });
        ctx_vars.insert("workgroup_id".into(), VarBinding { id: wid, ty: uvec3_ty, is_ptr: true });
        ctx_vars.insert("local_invocation_index".into(), VarBinding { id: lindex, ty: uint_ty.clone(), is_ptr: true });
        // Friendly aliases matching the HLSL backend naming.
        ctx_vars.insert("dispatch_thread_id".into(), VarBinding { id: gid, ty: Type::Named { name: "UVec3".into(), generics: vec![], span: shader.ast.span }, is_ptr: true });
        ctx_vars.insert("group_thread_id".into(), VarBinding { id: lid, ty: Type::Named { name: "UVec3".into(), generics: vec![], span: shader.ast.span }, is_ptr: true });
        ctx_vars.insert("group_id".into(), VarBinding { id: wid, ty: Type::Named { name: "UVec3".into(), generics: vec![], span: shader.ast.span }, is_ptr: true });
        ctx_vars.insert("group_index".into(), VarBinding { id: lindex, ty: uint_ty, is_ptr: true });
    }

    // 4. Function Body
    let main_fn = b.begin_function(void, None, rspirv::spirv::FunctionControl::NONE, fn_void_void).unwrap();
    b.begin_block(None).unwrap();

    let mut ctx = ShaderContext {
        b,
        vars: ctx_vars,
        output_var,
        struct_uniforms,
        storage_buffers,
        glsl_ext: None,
        loop_continue_targets: vec![],
        loop_break_targets: vec![],
    };

    emit_block(&mut ctx, &shader.ast.body)?;

    // Ensure we always have a return
    if shader.ast.body.stmts.last().map_or(true, |s| !matches!(s, Stmt::Return(_, _))) {
        ctx.b.ret().unwrap();
    }
    
    ctx.b.end_function().unwrap();

    // 5. Entry Point
    b.entry_point(exec_model, main_fn, &shader.ast.name, interface_vars);
    
    if exec_model == ExecutionModel::Fragment {
        b.execution_mode(main_fn, ExecutionMode::OriginUpperLeft, vec![]);
    } else if exec_model == ExecutionModel::GLCompute {
        if let Some([sx, sy, sz]) = local_size_spec_ids {
            if sx != 0 && sy != 0 && sz != 0 {
                b.execution_mode_id(main_fn, ExecutionMode::LocalSizeId, vec![sx, sy, sz]);
            } else {
                b.execution_mode(main_fn, ExecutionMode::LocalSize, vec![8, 8, 1]);
            }
        } else {
            b.execution_mode(main_fn, ExecutionMode::LocalSize, vec![8, 8, 1]);
        }
    }
    
    Ok(())
}

impl<'a> ShaderContext<'a> {
    fn get_glsl_ext(&mut self) -> u32 {
        if let Some(ext) = self.glsl_ext {
            ext
        } else {
            let ext = self.b.ext_inst_import("GLSL.std.450");
            self.glsl_ext = Some(ext);
            ext
        }
    }
}

fn emit_block(ctx: &mut ShaderContext, block: &Block) -> KainResult<()> {
    for stmt in &block.stmts {
        match stmt {
            Stmt::Return(expr, _) => {
                if let Some(expr) = expr {
                    if let Some(out_var) = ctx.output_var {
                        let (val, _) = emit_expr(ctx, expr)?;
                        ctx.b.store(out_var, val, None, vec![]).unwrap();
                    }
                }
                ctx.b.ret().unwrap();
            },
            Stmt::Let { pattern, value, .. } => {
                if let Some(value) = value {
                    let (val, ty) = emit_expr(ctx, value)?;
                    if let kain_core::ast::Pattern::Binding { name, .. } = pattern {
                        // Function-scope locals are pointers to support mutation/assignments.
                        let type_id = map_ast_type(ctx.b, &ty);
                        let ptr_ty = ctx.b.type_pointer(None, StorageClass::Function, type_id);
                        let local_var = ctx.b.variable(ptr_ty, None, StorageClass::Function, None);
                        ctx.b.store(local_var, val, None, std::iter::empty()).unwrap();
                        ctx.vars.insert(name.clone(), VarBinding { id: local_var, ty, is_ptr: true });
                    }
                }
            },
            Stmt::Expr(expr) => {
                if let Expr::If { condition, then_branch, else_branch, .. } = expr {
                    emit_if_statement(ctx, condition, then_branch, else_branch.as_deref())?;
                } else {
                    emit_expr(ctx, expr)?;
                }
            },
            Stmt::While { condition, body, .. } => {
                emit_while_statement(ctx, condition, body)?;
            },
            Stmt::For { binding, iter, body, span } => {
                emit_for_statement(ctx, binding, iter, body, *span)?;
            },
            Stmt::Break(_, span) => {
                let break_target = ctx
                    .loop_break_targets
                    .last()
                    .copied()
                    .ok_or_else(|| KainError::codegen("break outside loop", *span))?;
                terminate_with_branch(ctx, break_target)?;
                let cont_label = ctx.b.id();
                ctx.b.begin_block(Some(cont_label)).unwrap();
            },
            Stmt::Continue(span) => {
                let continue_target = ctx
                    .loop_continue_targets
                    .last()
                    .copied()
                    .ok_or_else(|| KainError::codegen("continue outside loop", *span))?;
                terminate_with_branch(ctx, continue_target)?;
                let cont_label = ctx.b.id();
                ctx.b.begin_block(Some(cont_label)).unwrap();
            },
            _ => {} // Ignore others for now
        }
    }
    Ok(())
}

fn emit_expr(ctx: &mut ShaderContext, expr: &Expr) -> KainResult<(u32, Type)> {
    match expr {
        Expr::Ident(name, span) => {
            if let Some(binding) = ctx.vars.get(name).cloned() {
                if binding.is_ptr {
                    if is_storage_buffer(&binding.ty) {
                        // Storage buffers are pointer-backed structs and must be consumed via indexing.
                        return Ok((binding.id, binding.ty));
                    }
                    // Need to load from pointer
                    let type_id = map_ast_type(ctx.b, &binding.ty);
                    
                    // Check if this is a struct-wrapped uniform
                    if ctx.struct_uniforms.contains(name) {
                        // Use AccessChain to get pointer to member 0 of the struct
                        let ptr_ty = ctx.b.type_pointer(None, StorageClass::Uniform, type_id);
                        let int_ty = ctx.b.type_int(32, 0);
                        let zero = ctx.b.constant_bit32(int_ty, 0);
                        let member_ptr = ctx.b.access_chain(ptr_ty, None, binding.id, vec![zero]).unwrap();
                        let val_id = ctx.b.load(type_id, None, member_ptr, None, std::iter::empty()).unwrap();
                        Ok((val_id, binding.ty))
                    } else {
                        // Direct load for inputs and non-wrapped uniforms
                        let val_id = ctx.b.load(type_id, None, binding.id, None, std::iter::empty()).unwrap();
                        Ok((val_id, binding.ty))
                    }
                } else {
                    Ok((binding.id, binding.ty))
                }
            } else {
                 Err(KainError::codegen(format!("Unknown variable: {}", name), *span))
            }
        },
        Expr::Binary { left, op, right, .. } => {
            let (lhs, lhs_ty) = emit_expr(ctx, left)?;
            let (rhs, rhs_ty) = emit_expr(ctx, right)?;
            
            // Map types to SPIR-V types
            let res_ty_id = map_ast_type(ctx.b, &lhs_ty);
            
            let res_id = match op {
                BinaryOp::Mul => {
                    if is_mat4(&lhs_ty) && is_mat4(&rhs_ty) {
                        ctx.b.matrix_times_matrix(res_ty_id, None, lhs, rhs).unwrap()
                    } else if is_mat4(&lhs_ty) && is_vec4(&rhs_ty) {
                        // Mat4 * Vec4 -> Vec4
                         let vec4_ty = map_ast_type(ctx.b, &rhs_ty);
                         ctx.b.matrix_times_vector(vec4_ty, None, lhs, rhs).unwrap()
                    } else if is_vec4(&lhs_ty) && is_mat4(&rhs_ty) {
                        // Vec4 * Mat4 -> Vec4
                         let vec4_ty = map_ast_type(ctx.b, &lhs_ty);
                         ctx.b.vector_times_matrix(vec4_ty, None, lhs, rhs).unwrap()
                    } else if is_float(&lhs_ty) && is_float(&rhs_ty) {
                        ctx.b.f_mul(res_ty_id, None, lhs, rhs).unwrap()
                    } else if is_int(&lhs_ty) && is_int(&rhs_ty) {
                        ctx.b.i_mul(res_ty_id, None, lhs, rhs).unwrap()
                    } else {
                        // Fallback for mixed vectors/scalars.
                        ctx.b.f_mul(res_ty_id, None, lhs, rhs).unwrap()
                    }
                },
                BinaryOp::Add => {
                    if is_int(&lhs_ty) && is_int(&rhs_ty) {
                        ctx.b.i_add(res_ty_id, None, lhs, rhs).unwrap()
                    } else {
                        ctx.b.f_add(res_ty_id, None, lhs, rhs).unwrap()
                    }
                },
                BinaryOp::Sub => {
                    if is_int(&lhs_ty) && is_int(&rhs_ty) {
                        ctx.b.i_sub(res_ty_id, None, lhs, rhs).unwrap()
                    } else {
                        ctx.b.f_sub(res_ty_id, None, lhs, rhs).unwrap()
                    }
                },
                BinaryOp::Div => {
                    if is_uint(&lhs_ty) && is_uint(&rhs_ty) {
                        ctx.b.u_div(res_ty_id, None, lhs, rhs).unwrap()
                    } else if is_int(&lhs_ty) && is_int(&rhs_ty) {
                        ctx.b.s_div(res_ty_id, None, lhs, rhs).unwrap()
                    } else {
                        ctx.b.f_div(res_ty_id, None, lhs, rhs).unwrap()
                    }
                },
                BinaryOp::Mod => {
                    if is_uint(&lhs_ty) && is_uint(&rhs_ty) {
                        ctx.b.u_mod(res_ty_id, None, lhs, rhs).unwrap()
                    } else if is_int(&lhs_ty) && is_int(&rhs_ty) {
                        ctx.b.s_mod(res_ty_id, None, lhs, rhs).unwrap()
                    } else {
                        ctx.b.f_mod(res_ty_id, None, lhs, rhs).unwrap()
                    }
                }
                BinaryOp::Pow => {
                    let glsl = ctx.get_glsl_ext();
                    ctx.b.ext_inst(res_ty_id, None, glsl, 26, vec![Operand::IdRef(lhs), Operand::IdRef(rhs)]).unwrap()
                }
                BinaryOp::Eq => {
                    let bool_ty = ctx.b.type_bool();
                    if is_float(&lhs_ty) && is_float(&rhs_ty) {
                        ctx.b.f_ord_equal(bool_ty, None, lhs, rhs).unwrap()
                    } else {
                        ctx.b.i_equal(bool_ty, None, lhs, rhs).unwrap()
                    }
                }
                BinaryOp::Ne => {
                    let bool_ty = ctx.b.type_bool();
                    if is_float(&lhs_ty) && is_float(&rhs_ty) {
                        ctx.b.f_ord_not_equal(bool_ty, None, lhs, rhs).unwrap()
                    } else {
                        ctx.b.i_not_equal(bool_ty, None, lhs, rhs).unwrap()
                    }
                }
                BinaryOp::Lt => {
                    let bool_ty = ctx.b.type_bool();
                    if is_float(&lhs_ty) && is_float(&rhs_ty) {
                        ctx.b.f_ord_less_than(bool_ty, None, lhs, rhs).unwrap()
                    } else if is_uint(&lhs_ty) && is_uint(&rhs_ty) {
                        ctx.b.u_less_than(bool_ty, None, lhs, rhs).unwrap()
                    } else {
                        ctx.b.s_less_than(bool_ty, None, lhs, rhs).unwrap()
                    }
                }
                BinaryOp::Le => {
                    let bool_ty = ctx.b.type_bool();
                    if is_float(&lhs_ty) && is_float(&rhs_ty) {
                        ctx.b.f_ord_less_than_equal(bool_ty, None, lhs, rhs).unwrap()
                    } else if is_uint(&lhs_ty) && is_uint(&rhs_ty) {
                        ctx.b.u_less_than_equal(bool_ty, None, lhs, rhs).unwrap()
                    } else {
                        ctx.b.s_less_than_equal(bool_ty, None, lhs, rhs).unwrap()
                    }
                }
                BinaryOp::Gt => {
                    let bool_ty = ctx.b.type_bool();
                    if is_float(&lhs_ty) && is_float(&rhs_ty) {
                        ctx.b.f_ord_greater_than(bool_ty, None, lhs, rhs).unwrap()
                    } else if is_uint(&lhs_ty) && is_uint(&rhs_ty) {
                        ctx.b.u_greater_than(bool_ty, None, lhs, rhs).unwrap()
                    } else {
                        ctx.b.s_greater_than(bool_ty, None, lhs, rhs).unwrap()
                    }
                }
                BinaryOp::Ge => {
                    let bool_ty = ctx.b.type_bool();
                    if is_float(&lhs_ty) && is_float(&rhs_ty) {
                        ctx.b.f_ord_greater_than_equal(bool_ty, None, lhs, rhs).unwrap()
                    } else if is_uint(&lhs_ty) && is_uint(&rhs_ty) {
                        ctx.b.u_greater_than_equal(bool_ty, None, lhs, rhs).unwrap()
                    } else {
                        ctx.b.s_greater_than_equal(bool_ty, None, lhs, rhs).unwrap()
                    }
                }
                BinaryOp::And => {
                    let bool_ty = ctx.b.type_bool();
                    ctx.b.logical_and(bool_ty, None, lhs, rhs).unwrap()
                }
                BinaryOp::Or => {
                    let bool_ty = ctx.b.type_bool();
                    ctx.b.logical_or(bool_ty, None, lhs, rhs).unwrap()
                }
                BinaryOp::BitAnd => ctx.b.bitwise_and(res_ty_id, None, lhs, rhs).unwrap(),
                BinaryOp::BitOr => ctx.b.bitwise_or(res_ty_id, None, lhs, rhs).unwrap(),
                BinaryOp::BitXor => ctx.b.bitwise_xor(res_ty_id, None, lhs, rhs).unwrap(),
                BinaryOp::Shl => ctx.b.shift_left_logical(res_ty_id, None, lhs, rhs).unwrap(),
                BinaryOp::Shr => {
                    if is_uint(&lhs_ty) {
                        ctx.b.shift_right_logical(res_ty_id, None, lhs, rhs).unwrap()
                    } else {
                        ctx.b.shift_right_arithmetic(res_ty_id, None, lhs, rhs).unwrap()
                    }
                }
                _ => return Err(KainError::codegen("Unsupported binary op in shader", expr.span())),
            };
            
            // Result type inference
            let res_ty = if matches!(op, BinaryOp::Eq | BinaryOp::Ne | BinaryOp::Lt | BinaryOp::Le | BinaryOp::Gt | BinaryOp::Ge | BinaryOp::And | BinaryOp::Or) {
                Type::Named { name: "Bool".into(), generics: vec![], span: expr.span() }
            } else if is_mat4(&lhs_ty) && is_vec4(&rhs_ty) {
                rhs_ty
            } else {
                lhs_ty
            };
            
            Ok((res_id, res_ty))
        },
        Expr::Call { callee, args, .. } => {
            if let Expr::Ident(name, _) = &**callee {
                let float = ctx.b.type_float(32);
                let int = ctx.b.type_int(32, 1);
                let uint = ctx.b.type_int(32, 0);
                
                // Vector constructors
                match name.as_str() {
                    "vec2" | "Vec2" if args.len() == 2 => {
                        let vec2 = ctx.b.type_vector(float, 2);
                        let mut components = vec![];
                        for arg in args {
                            let (val, _) = emit_expr(ctx, &arg.value)?;
                            components.push(val);
                        }
                        let res_id = ctx.b.composite_construct(vec2, None, components).unwrap();
                        return Ok((res_id, Type::Named { name: "Vec2".into(), generics: vec![], span: expr.span() }));
                    },
                    "vec3" | "Vec3" if args.len() == 3 => {
                        let vec3 = ctx.b.type_vector(float, 3);
                        let mut components = vec![];
                        for arg in args {
                            let (val, _) = emit_expr(ctx, &arg.value)?;
                            components.push(val);
                        }
                        let res_id = ctx.b.composite_construct(vec3, None, components).unwrap();
                        return Ok((res_id, Type::Named { name: "Vec3".into(), generics: vec![], span: expr.span() }));
                    },
                    "vec4" | "Vec4" if args.len() == 4 => {
                        let vec4 = ctx.b.type_vector(float, 4);
                        let mut components = vec![];
                        for arg in args {
                            let (val, _) = emit_expr(ctx, &arg.value)?;
                            components.push(val);
                        }
                        let res_id = ctx.b.composite_construct(vec4, None, components).unwrap();
                        return Ok((res_id, Type::Named { name: "Vec4".into(), generics: vec![], span: expr.span() }));
                    },
                    "uvec2" | "UVec2" if args.len() == 2 => {
                        let uvec2 = ctx.b.type_vector(uint, 2);
                        let mut components = vec![];
                        for arg in args {
                            let (val, _) = emit_expr(ctx, &arg.value)?;
                            components.push(val);
                        }
                        let res_id = ctx.b.composite_construct(uvec2, None, components).unwrap();
                        return Ok((res_id, Type::Named { name: "UVec2".into(), generics: vec![], span: expr.span() }));
                    },
                    "uvec3" | "UVec3" if args.len() == 3 => {
                        let uvec3 = ctx.b.type_vector(uint, 3);
                        let mut components = vec![];
                        for arg in args {
                            let (val, _) = emit_expr(ctx, &arg.value)?;
                            components.push(val);
                        }
                        let res_id = ctx.b.composite_construct(uvec3, None, components).unwrap();
                        return Ok((res_id, Type::Named { name: "UVec3".into(), generics: vec![], span: expr.span() }));
                    },
                    "uvec4" | "UVec4" if args.len() == 4 => {
                        let uvec4 = ctx.b.type_vector(uint, 4);
                        let mut components = vec![];
                        for arg in args {
                            let (val, _) = emit_expr(ctx, &arg.value)?;
                            components.push(val);
                        }
                        let res_id = ctx.b.composite_construct(uvec4, None, components).unwrap();
                        return Ok((res_id, Type::Named { name: "UVec4".into(), generics: vec![], span: expr.span() }));
                    },
                    "ivec2" | "IVec2" if args.len() == 2 => {
                        let ivec2 = ctx.b.type_vector(int, 2);
                        let mut components = vec![];
                        for arg in args {
                            let (val, _) = emit_expr(ctx, &arg.value)?;
                            components.push(val);
                        }
                        let res_id = ctx.b.composite_construct(ivec2, None, components).unwrap();
                        return Ok((res_id, Type::Named { name: "IVec2".into(), generics: vec![], span: expr.span() }));
                    },
                    "ivec3" | "IVec3" if args.len() == 3 => {
                        let ivec3 = ctx.b.type_vector(int, 3);
                        let mut components = vec![];
                        for arg in args {
                            let (val, _) = emit_expr(ctx, &arg.value)?;
                            components.push(val);
                        }
                        let res_id = ctx.b.composite_construct(ivec3, None, components).unwrap();
                        return Ok((res_id, Type::Named { name: "IVec3".into(), generics: vec![], span: expr.span() }));
                    },
                    "ivec4" | "IVec4" if args.len() == 4 => {
                        let ivec4 = ctx.b.type_vector(int, 4);
                        let mut components = vec![];
                        for arg in args {
                            let (val, _) = emit_expr(ctx, &arg.value)?;
                            components.push(val);
                        }
                        let res_id = ctx.b.composite_construct(ivec4, None, components).unwrap();
                        return Ok((res_id, Type::Named { name: "IVec4".into(), generics: vec![], span: expr.span() }));
                    },

                    // Scalar constructors/casts (Float(x), Int(x), UInt(x), Bool(x))
                    "float" | "Float" if args.len() == 1 => {
                        let cast_expr = Expr::Cast {
                            value: Box::new(args[0].value.clone()),
                            target: Type::Named { name: "Float".into(), generics: vec![], span: expr.span() },
                            span: expr.span(),
                        };
                        return emit_expr(ctx, &cast_expr);
                    }
                    "int" | "Int" if args.len() == 1 => {
                        let cast_expr = Expr::Cast {
                            value: Box::new(args[0].value.clone()),
                            target: Type::Named { name: "Int".into(), generics: vec![], span: expr.span() },
                            span: expr.span(),
                        };
                        return emit_expr(ctx, &cast_expr);
                    }
                    "uint" | "UInt" if args.len() == 1 => {
                        let cast_expr = Expr::Cast {
                            value: Box::new(args[0].value.clone()),
                            target: Type::Named { name: "UInt".into(), generics: vec![], span: expr.span() },
                            span: expr.span(),
                        };
                        return emit_expr(ctx, &cast_expr);
                    }
                    "bool" | "Bool" if args.len() == 1 => {
                        let cast_expr = Expr::Cast {
                            value: Box::new(args[0].value.clone()),
                            target: Type::Named { name: "Bool".into(), generics: vec![], span: expr.span() },
                            span: expr.span(),
                        };
                        return emit_expr(ctx, &cast_expr);
                    }
                    
                    // Math functions (GLSL extended instructions)
                    "sin" if args.len() == 1 => {
                        let (val, ty) = emit_expr(ctx, &args[0].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.get_glsl_ext();
                        let res_id = ctx.b.ext_inst(res_ty, None, glsl, 13, vec![Operand::IdRef(val)]).unwrap(); // Sin = 13
                        return Ok((res_id, ty));
                    },
                    "cos" if args.len() == 1 => {
                        let (val, ty) = emit_expr(ctx, &args[0].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.get_glsl_ext();
                        let res_id = ctx.b.ext_inst(res_ty, None, glsl, 14, vec![Operand::IdRef(val)]).unwrap(); // Cos = 14
                        return Ok((res_id, ty));
                    },
                    "tan" if args.len() == 1 => {
                        let (val, ty) = emit_expr(ctx, &args[0].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.b.ext_inst_import("GLSL.std.450");
                        let res_id = ctx.b.ext_inst(res_ty, None, glsl, 15, vec![Operand::IdRef(val)]).unwrap(); // Tan = 15
                        return Ok((res_id, ty));
                    },
                    "pow" if args.len() == 2 => {
                        let (base, ty) = emit_expr(ctx, &args[0].value)?;
                        let (exp, _) = emit_expr(ctx, &args[1].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.b.ext_inst_import("GLSL.std.450");
                        let res_id = ctx.b.ext_inst(res_ty, None, glsl, 26, vec![Operand::IdRef(base), Operand::IdRef(exp)]).unwrap(); // Pow = 26
                        return Ok((res_id, ty));
                    },
                    "sqrt" if args.len() == 1 => {
                        let (val, ty) = emit_expr(ctx, &args[0].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.b.ext_inst_import("GLSL.std.450");
                        let res_id = ctx.b.ext_inst(res_ty, None, glsl, 31, vec![Operand::IdRef(val)]).unwrap(); // Sqrt = 31
                        return Ok((res_id, ty));
                    },
                    "abs" if args.len() == 1 => {
                        let (val, ty) = emit_expr(ctx, &args[0].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.b.ext_inst_import("GLSL.std.450");
                        let res_id = ctx.b.ext_inst(res_ty, None, glsl, 4, vec![Operand::IdRef(val)]).unwrap(); // FAbs = 4
                        return Ok((res_id, ty));
                    },
                    "floor" if args.len() == 1 => {
                        let (val, ty) = emit_expr(ctx, &args[0].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.b.ext_inst_import("GLSL.std.450");
                        let res_id = ctx.b.ext_inst(res_ty, None, glsl, 8, vec![Operand::IdRef(val)]).unwrap(); // Floor = 8
                        return Ok((res_id, ty));
                    },
                    "ceil" if args.len() == 1 => {
                        let (val, ty) = emit_expr(ctx, &args[0].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.b.ext_inst_import("GLSL.std.450");
                        let res_id = ctx.b.ext_inst(res_ty, None, glsl, 9, vec![Operand::IdRef(val)]).unwrap(); // Ceil = 9
                        return Ok((res_id, ty));
                    },
                    "fract" if args.len() == 1 => {
                        let (val, ty) = emit_expr(ctx, &args[0].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.b.ext_inst_import("GLSL.std.450");
                        let res_id = ctx.b.ext_inst(res_ty, None, glsl, 10, vec![Operand::IdRef(val)]).unwrap(); // Fract = 10
                        return Ok((res_id, ty));
                    },
                    "min" if args.len() == 2 => {
                        let (a, ty) = emit_expr(ctx, &args[0].value)?;
                        let (b, _) = emit_expr(ctx, &args[1].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.b.ext_inst_import("GLSL.std.450");
                        let res_id = ctx.b.ext_inst(res_ty, None, glsl, 37, vec![Operand::IdRef(a), Operand::IdRef(b)]).unwrap(); // FMin = 37
                        return Ok((res_id, ty));
                    },
                    "max" if args.len() == 2 => {
                        let (a, ty) = emit_expr(ctx, &args[0].value)?;
                        let (b, _) = emit_expr(ctx, &args[1].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.b.ext_inst_import("GLSL.std.450");
                        let res_id = ctx.b.ext_inst(res_ty, None, glsl, 40, vec![Operand::IdRef(a), Operand::IdRef(b)]).unwrap(); // FMax = 40
                        return Ok((res_id, ty));
                    },
                    "clamp" if args.len() == 3 => {
                        let (val, ty) = emit_expr(ctx, &args[0].value)?;
                        let (min_val, _) = emit_expr(ctx, &args[1].value)?;
                        let (max_val, _) = emit_expr(ctx, &args[2].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.b.ext_inst_import("GLSL.std.450");
                        let res_id = ctx.b.ext_inst(res_ty, None, glsl, 43, vec![Operand::IdRef(val), Operand::IdRef(min_val), Operand::IdRef(max_val)]).unwrap(); // FClamp = 43
                        return Ok((res_id, ty));
                    },
                    "mix" if args.len() == 3 => {
                        let (a, ty) = emit_expr(ctx, &args[0].value)?;
                        let (b, _) = emit_expr(ctx, &args[1].value)?;
                        let (t, _) = emit_expr(ctx, &args[2].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.b.ext_inst_import("GLSL.std.450");
                        let res_id = ctx.b.ext_inst(res_ty, None, glsl, 46, vec![Operand::IdRef(a), Operand::IdRef(b), Operand::IdRef(t)]).unwrap(); // FMix = 46
                        return Ok((res_id, ty));
                    },
                    "step" if args.len() == 2 => {
                        let (edge, ty) = emit_expr(ctx, &args[0].value)?;
                        let (x, _) = emit_expr(ctx, &args[1].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.b.ext_inst_import("GLSL.std.450");
                        let res_id = ctx.b.ext_inst(res_ty, None, glsl, 48, vec![Operand::IdRef(edge), Operand::IdRef(x)]).unwrap(); // Step = 48
                        return Ok((res_id, ty));
                    },
                    "smoothstep" if args.len() == 3 => {
                        let (edge0, ty) = emit_expr(ctx, &args[0].value)?;
                        let (edge1, _) = emit_expr(ctx, &args[1].value)?;
                        let (x, _) = emit_expr(ctx, &args[2].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.b.ext_inst_import("GLSL.std.450");
                        let res_id = ctx.b.ext_inst(res_ty, None, glsl, 49, vec![Operand::IdRef(edge0), Operand::IdRef(edge1), Operand::IdRef(x)]).unwrap(); // SmoothStep = 49
                        return Ok((res_id, ty));
                    },
                    "length" if args.len() == 1 => {
                        let (val, _) = emit_expr(ctx, &args[0].value)?;
                        let glsl = ctx.b.ext_inst_import("GLSL.std.450");
                        let res_id = ctx.b.ext_inst(float, None, glsl, 66, vec![Operand::IdRef(val)]).unwrap(); // Length = 66
                        return Ok((res_id, Type::Named { name: "Float".into(), generics: vec![], span: expr.span() }));
                    },
                    "normalize" if args.len() == 1 => {
                        let (val, ty) = emit_expr(ctx, &args[0].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.b.ext_inst_import("GLSL.std.450");
                        let res_id = ctx.b.ext_inst(res_ty, None, glsl, 69, vec![Operand::IdRef(val)]).unwrap(); // Normalize = 69
                        return Ok((res_id, ty));
                    },
                    "dot" if args.len() == 2 => {
                        let (a, _) = emit_expr(ctx, &args[0].value)?;
                        let (b, _) = emit_expr(ctx, &args[1].value)?;
                        let res_id = ctx.b.dot(float, None, a, b).unwrap();
                        return Ok((res_id, Type::Named { name: "Float".into(), generics: vec![], span: expr.span() }));
                    },
                    "cross" if args.len() == 2 => {
                        let (a, ty) = emit_expr(ctx, &args[0].value)?;
                        let (b, _) = emit_expr(ctx, &args[1].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.b.ext_inst_import("GLSL.std.450");
                        let res_id = ctx.b.ext_inst(res_ty, None, glsl, 68, vec![Operand::IdRef(a), Operand::IdRef(b)]).unwrap(); // Cross = 68
                        return Ok((res_id, ty));
                    },
                    "reflect" if args.len() == 2 => {
                        let (i, ty) = emit_expr(ctx, &args[0].value)?;
                        let (n, _) = emit_expr(ctx, &args[1].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.b.ext_inst_import("GLSL.std.450");
                        let res_id = ctx.b.ext_inst(res_ty, None, glsl, 71, vec![Operand::IdRef(i), Operand::IdRef(n)]).unwrap(); // Reflect = 71
                        return Ok((res_id, ty));
                    },
                    
                    // Texture sampling
                    "sample" if args.len() == 2 => {
                        let (sampler, _) = emit_expr(ctx, &args[0].value)?;
                        let (coords, _) = emit_expr(ctx, &args[1].value)?;
                        let vec4 = ctx.b.type_vector(float, 4);
                        let res_id = ctx.b.image_sample_implicit_lod(vec4, None, sampler, coords, None, std::iter::empty()).unwrap();
                        return Ok((res_id, Type::Named { name: "Vec4".into(), generics: vec![], span: expr.span() }));
                    },
                    "sample_lod" if args.len() == 3 => {
                        let (sampler, _) = emit_expr(ctx, &args[0].value)?;
                        let (coords, _) = emit_expr(ctx, &args[1].value)?;
                        let (lod, _) = emit_expr(ctx, &args[2].value)?;
                        let vec4 = ctx.b.type_vector(float, 4);
                        let res_id = ctx.b.image_sample_explicit_lod(vec4, None, sampler, coords, rspirv::spirv::ImageOperands::LOD, vec![Operand::IdRef(lod)]).unwrap();
                        return Ok((res_id, Type::Named { name: "Vec4".into(), generics: vec![], span: expr.span() }));
                    },
                    
                    _ => {}
                }
            }
            let callee_name = match &**callee {
                Expr::Ident(name, _) => name.clone(),
                _ => "<complex expression>".to_string(),
            };
            Err(KainError::codegen(format!("Unsupported function call in shader: '{}'", callee_name), expr.span()))
        },
        Expr::Float(f, span) => {
            let float = ctx.b.type_float(32);
            let val = ctx.b.constant_bit32(float, (*f as f32).to_bits());
            Ok((val, Type::Named { name: "Float".into(), generics: vec![], span: *span }))
        },
        Expr::Int(i, span) => {
            let int = ctx.b.type_int(32, 1);
            let val = ctx.b.constant_bit32(int, *i as u32);
            Ok((val, Type::Named { name: "Int".into(), generics: vec![], span: *span }))
        }
        Expr::Bool(v, span) => {
            let bool_ty = ctx.b.type_bool();
            let bool_id = if *v {
                ctx.b.constant_true(bool_ty)
            } else {
                ctx.b.constant_false(bool_ty)
            };
            Ok((bool_id, Type::Named { name: "Bool".into(), generics: vec![], span: *span }))
        }
        Expr::Paren(inner, _) => emit_expr(ctx, inner),
        Expr::Cast { value, target, span } => {
            let (src_val, src_ty) = emit_expr(ctx, value)?;
            let dst_ty = target.clone();
            let dst_ty_id = map_ast_type(ctx.b, &dst_ty);

            if is_same_type_family(&src_ty, &dst_ty) {
                return Ok((src_val, dst_ty));
            }

            if is_bool(&dst_ty) {
                let as_b = as_bool(ctx, src_val, &src_ty);
                return Ok((as_b, dst_ty));
            }

            if is_bool(&src_ty) {
                let src_b = as_bool(ctx, src_val, &src_ty);
                let out = if is_float(&dst_ty) || is_float_vector(&dst_ty) {
                    let one = float_one(ctx.b, &dst_ty);
                    let zero = float_zero(ctx.b, &dst_ty);
                    ctx.b.select(dst_ty_id, None, src_b, one, zero).unwrap()
                } else if is_uint_like(&dst_ty) {
                    let one = uint_one(ctx.b, &dst_ty);
                    let zero = uint_zero(ctx.b, &dst_ty);
                    ctx.b.select(dst_ty_id, None, src_b, one, zero).unwrap()
                } else if is_int_like(&dst_ty) {
                    let one = int_one(ctx.b, &dst_ty);
                    let zero = int_zero(ctx.b, &dst_ty);
                    ctx.b.select(dst_ty_id, None, src_b, one, zero).unwrap()
                } else {
                    return Err(KainError::codegen("Unsupported bool cast target in shader", *span));
                };
                return Ok((out, dst_ty));
            }

            let src_dim = numeric_dim(&src_ty);
            let dst_dim = numeric_dim(&dst_ty);
            if src_dim == 0 || dst_dim == 0 || src_dim != dst_dim {
                return Err(KainError::codegen(
                    "Invalid cast in shader: source/target dimensions or categories are incompatible",
                    *span,
                ));
            }

            let converted = if is_float_like(&src_ty) && is_int_like(&dst_ty) {
                ctx.b.convert_f_to_s(dst_ty_id, None, src_val).unwrap()
            } else if is_float_like(&src_ty) && is_uint_like(&dst_ty) {
                ctx.b.convert_f_to_u(dst_ty_id, None, src_val).unwrap()
            } else if is_int_like(&src_ty) && is_float_like(&dst_ty) {
                ctx.b.convert_s_to_f(dst_ty_id, None, src_val).unwrap()
            } else if is_uint_like(&src_ty) && is_float_like(&dst_ty) {
                ctx.b.convert_u_to_f(dst_ty_id, None, src_val).unwrap()
            } else if is_int_like(&src_ty) && is_uint_like(&dst_ty) {
                ctx.b.bitcast(dst_ty_id, None, src_val).unwrap()
            } else if is_uint_like(&src_ty) && is_int_like(&dst_ty) {
                ctx.b.bitcast(dst_ty_id, None, src_val).unwrap()
            } else if is_int_like(&src_ty) && is_int_like(&dst_ty) {
                ctx.b.s_convert(dst_ty_id, None, src_val).unwrap()
            } else if is_uint_like(&src_ty) && is_uint_like(&dst_ty) {
                ctx.b.u_convert(dst_ty_id, None, src_val).unwrap()
            } else {
                return Err(KainError::codegen("Unsupported numeric cast in shader", *span));
            };
            Ok((converted, dst_ty))
        }
        Expr::Assign { target, value, .. } => {
            let (next_val, next_ty) = emit_expr(ctx, value)?;
            match target.as_ref() {
                Expr::Ident(name, span) => {
                    if let Some(binding) = ctx.vars.get(name).cloned() {
                        if !binding.is_ptr {
                            return Err(KainError::codegen(format!("Cannot assign to immutable value '{}'", name), *span));
                        }
                        if is_storage_buffer(&binding.ty) {
                            return Err(KainError::codegen(format!("Assigning a whole storage buffer '{}' is not supported", name), *span));
                        }
                        ctx.b.store(binding.id, next_val, None, std::iter::empty()).unwrap();
                        Ok((next_val, next_ty))
                    } else {
                        Err(KainError::codegen(format!("Unknown assignment target: {}", name), *span))
                    }
                }
                Expr::Index { object, index, .. } => {
                    let elem_ptr = emit_index_lvalue_ptr(ctx, object, index, &next_ty, target.span())?;
                    ctx.b.store(elem_ptr, next_val, None, std::iter::empty()).unwrap();
                    Ok((next_val, next_ty))
                }
                _ => Err(KainError::codegen("Unsupported assignment target in shader", target.span())),
            }
        }
        Expr::Index { object, index, .. } => {
            // StorageBuffer<T> indexing: emit AccessChain to runtime array element.
            if let Expr::Ident(buffer_name, span) = object.as_ref() {
                if ctx.storage_buffers.contains(buffer_name) {
                    let binding = ctx
                        .vars
                        .get(buffer_name)
                        .cloned()
                        .ok_or_else(|| KainError::codegen(format!("Unknown storage buffer: {}", buffer_name), *span))?;
                    let (index_id, _) = emit_expr(ctx, index)?;
                    let elem_ty = storage_buffer_elem_type(&binding.ty, expr.span());
                    let elem_ty_id = map_ast_type(ctx.b, &elem_ty);
                    let elem_ptr_ty = ctx.b.type_pointer(None, StorageClass::StorageBuffer, elem_ty_id);
                    let uint_ty = ctx.b.type_int(32, 0);
                    let zero = ctx.b.constant_bit32(uint_ty, 0);
                    let elem_ptr = ctx
                        .b
                        .access_chain(elem_ptr_ty, None, binding.id, vec![zero, index_id])
                        .unwrap();
                    let loaded = ctx.b.load(elem_ty_id, None, elem_ptr, None, std::iter::empty()).unwrap();
                    return Ok((loaded, elem_ty));
                }
            }
            Err(KainError::codegen("Unsupported index expression in shader", expr.span()))
        }
        Expr::Field { object, field, span } => {
            let (obj_id, obj_ty) = emit_expr(ctx, object)?;
            if let Some(indices) = swizzle_indices(field) {
                let scalar_ty = vector_scalar_type(ctx, &obj_ty);
                let scalar_spv = map_ast_type(ctx.b, &scalar_ty);
                if indices.len() == 1 {
                    let res_id = ctx
                        .b
                        .composite_extract(scalar_spv, None, obj_id, vec![indices[0]])
                        .unwrap();
                    return Ok((res_id, scalar_ty));
                }
                let swizzle_len = indices.len();
                let out_vec = ctx.b.type_vector(scalar_spv, swizzle_len as u32);
                let res_id = ctx
                    .b
                    .vector_shuffle(out_vec, None, obj_id, obj_id, indices)
                    .unwrap();
                return Ok((res_id, vec_type_from_scalar(&scalar_ty, swizzle_len, *span)));
            }
            Err(KainError::codegen(format!("Unsupported field access: {}", field), *span))
        },
        Expr::Unary { op, operand, .. } => {
            let (val, ty) = emit_expr(ctx, operand)?;
            let ty_id = map_ast_type(ctx.b, &ty);
            let out = match op {
                UnaryOp::Neg => {
                    if is_float(&ty) {
                        ctx.b.f_negate(ty_id, None, val).unwrap()
                    } else {
                        ctx.b.s_negate(ty_id, None, val).unwrap()
                    }
                }
                UnaryOp::Not => {
                    let bool_ty = ctx.b.type_bool();
                    let b = as_bool(ctx, val, &ty);
                    return Ok((ctx.b.logical_not(bool_ty, None, b).unwrap(), Type::Named { name: "Bool".into(), generics: vec![], span: expr.span() }));
                }
                UnaryOp::BitNot => ctx.b.not(ty_id, None, val).unwrap(),
                UnaryOp::Ref | UnaryOp::RefMut | UnaryOp::Deref => val,
            };
            Ok((out, ty))
        }
        Expr::MethodCall { receiver, method, args, span } => {
            let (recv_val, recv_ty) = emit_expr(ctx, receiver)?;
            let float = ctx.b.type_float(32);
            match method.as_str() {
                "length" if args.is_empty() => {
                    let glsl = ctx.get_glsl_ext();
                    let id = ctx.b.ext_inst(float, None, glsl, 66, vec![Operand::IdRef(recv_val)]).unwrap();
                    Ok((id, Type::Named { name: "Float".into(), generics: vec![], span: *span }))
                }
                "normalize" if args.is_empty() => {
                    let res_ty = map_ast_type(ctx.b, &recv_ty);
                    let glsl = ctx.get_glsl_ext();
                    let id = ctx.b.ext_inst(res_ty, None, glsl, 69, vec![Operand::IdRef(recv_val)]).unwrap();
                    Ok((id, recv_ty))
                }
                "dot" if args.len() == 1 => {
                    let (rhs, _) = emit_expr(ctx, &args[0].value)?;
                    let id = ctx.b.dot(float, None, recv_val, rhs).unwrap();
                    Ok((id, Type::Named { name: "Float".into(), generics: vec![], span: *span }))
                }
                "cross" if args.len() == 1 => {
                    let (rhs, _) = emit_expr(ctx, &args[0].value)?;
                    let res_ty = map_ast_type(ctx.b, &recv_ty);
                    let glsl = ctx.get_glsl_ext();
                    let id = ctx.b.ext_inst(res_ty, None, glsl, 68, vec![Operand::IdRef(recv_val), Operand::IdRef(rhs)]).unwrap();
                    Ok((id, recv_ty))
                }
                "reflect" if args.len() == 1 => {
                    let (rhs, _) = emit_expr(ctx, &args[0].value)?;
                    let res_ty = map_ast_type(ctx.b, &recv_ty);
                    let glsl = ctx.get_glsl_ext();
                    let id = ctx.b.ext_inst(res_ty, None, glsl, 71, vec![Operand::IdRef(recv_val), Operand::IdRef(rhs)]).unwrap();
                    Ok((id, recv_ty))
                }
                _ => Err(KainError::codegen(format!("Unsupported method call in shader: {}", method), *span)),
            }
        }
        Expr::If { condition, then_branch, else_branch, span } => {
            let (cond_raw, cond_ty) = emit_expr(ctx, condition)?;
            let cond = as_bool(ctx, cond_raw, &cond_ty);

            let then_expr = single_expr_from_block(then_branch, *span)?;
            let (then_val, then_ty) = emit_expr(ctx, then_expr)?;

            let else_expr = match else_branch.as_deref() {
                Some(ElseBranch::Else(block)) => single_expr_from_block(block, *span)?,
                Some(ElseBranch::ElseIf(_, _, _)) => {
                    return Err(KainError::codegen(
                        "Else-if expression chains are not yet supported in SPIR-V value expressions",
                        *span,
                    ));
                }
                None => {
                    return Err(KainError::codegen(
                        "If expression requires an else branch in SPIR-V backend",
                        *span,
                    ));
                }
            };
            let (else_val, else_ty) = emit_expr(ctx, else_expr)?;
            if std::mem::discriminant(&then_ty) != std::mem::discriminant(&else_ty) {
                return Err(KainError::codegen(
                    "If expression branches must return compatible types",
                    *span,
                ));
            }
            let res_ty = map_ast_type(ctx.b, &then_ty);
            let selected = ctx.b.select(res_ty, None, cond, then_val, else_val).unwrap();
            Ok((selected, then_ty))
        }
        _ => Err(KainError::codegen(
            format!("Unsupported expression in shader: {}", expr_kind_name(expr)),
            expr.span(),
        )),
    }
}

fn expr_kind_name(expr: &Expr) -> &'static str {
    match expr {
        Expr::Int(..) => "Int",
        Expr::Float(..) => "Float",
        Expr::Bool(..) => "Bool",
        Expr::None(..) => "None",
        Expr::String(..) => "String",
        Expr::FString(..) => "FString",
        Expr::Ident(..) => "Ident",
        Expr::MacroCall { .. } => "MacroCall",
        Expr::Binary { .. } => "Binary",
        Expr::Unary { .. } => "Unary",
        Expr::Call { .. } => "Call",
        Expr::MethodCall { .. } => "MethodCall",
        Expr::Field { .. } => "Field",
        Expr::Index { .. } => "Index",
        Expr::Assign { .. } => "Assign",
        Expr::Struct { .. } => "Struct",
        Expr::AggregateInit { .. } => "AggregateInit",
        Expr::EnumVariant { .. } => "EnumVariant",
        Expr::Tuple(..) => "Tuple",
        Expr::Array(..) => "Array",
        Expr::Range { .. } => "Range",
        Expr::If { .. } => "If",
        Expr::Match { .. } => "Match",
        Expr::Lambda { .. } => "Lambda",
        Expr::Ref { .. } => "Ref",
        Expr::AddrOf { .. } => "AddrOf",
        Expr::Deref(..) => "Deref",
        Expr::PtrOffset { .. } => "PtrOffset",
        Expr::MemLoad { .. } => "MemLoad",
        Expr::MemStore { .. } => "MemStore",
        Expr::SizeOfType { .. } => "SizeOfType",
        Expr::AlignOfType { .. } => "AlignOfType",
        Expr::Alloca { .. } => "Alloca",
        Expr::Uninit { .. } => "Uninit",
        Expr::Alloc { .. } => "Alloc",
        Expr::Realloc { .. } => "Realloc",
        Expr::Cast { .. } => "Cast",
        Expr::Try(..) => "Try",
        Expr::Await(..) => "Await",
        Expr::Spawn { .. } => "Spawn",
        Expr::SendMsg { .. } => "SendMsg",
        Expr::Comptime(..) => "Comptime",
        Expr::Block(..) => "Block",
        Expr::JSX(..) => "JSX",
        Expr::Paren(..) => "Paren",
        Expr::Return(..) => "Return",
        Expr::Break(..) => "Break",
        Expr::Continue(..) => "Continue",
    }
}

fn single_expr_from_block<'a>(block: &'a Block, span: kain_core::span::Span) -> KainResult<&'a Expr> {
    if block.stmts.len() != 1 {
        return Err(KainError::codegen("Expected single-expression block", span));
    }
    match &block.stmts[0] {
        Stmt::Expr(expr) => Ok(expr),
        _ => Err(KainError::codegen("Expected expression statement in block", span)),
    }
}

fn as_bool(ctx: &mut ShaderContext, value: u32, ty: &Type) -> u32 {
    if matches!(ty, Type::Named { name, .. } if name == "Bool") {
        return value;
    }
    let bool_ty = ctx.b.type_bool();
    if is_uint(ty) {
        let uint_ty = map_ast_type(ctx.b, ty);
        let zero = ctx.b.constant_bit32(uint_ty, 0);
        return ctx.b.i_not_equal(bool_ty, None, value, zero).unwrap();
    }
    if is_int(ty) {
        let int_ty = map_ast_type(ctx.b, ty);
        let zero = ctx.b.constant_bit32(int_ty, 0);
        return ctx.b.i_not_equal(bool_ty, None, value, zero).unwrap();
    }
    let float_ty = ctx.b.type_float(32);
    let zero = ctx.b.constant_bit32(float_ty, 0f32.to_bits());
    ctx.b.f_ord_not_equal(bool_ty, None, value, zero).unwrap()
}

fn terminate_with_branch(ctx: &mut ShaderContext, target: u32) -> KainResult<()> {
    if ctx.b.selected_block().is_some() {
        ctx.b.branch(target).unwrap();
    }
    Ok(())
}

fn emit_if_statement(
    ctx: &mut ShaderContext,
    condition: &Expr,
    then_branch: &Block,
    else_branch: Option<&ElseBranch>,
) -> KainResult<()> {
    let (cond_raw, cond_ty) = emit_expr(ctx, condition)?;
    let cond = as_bool(ctx, cond_raw, &cond_ty);

    let then_label = ctx.b.id();
    let else_label = ctx.b.id();
    let merge_label = ctx.b.id();
    ctx.b.selection_merge(merge_label, rspirv::spirv::SelectionControl::NONE).unwrap();
    ctx.b.branch_conditional(cond, then_label, else_label, std::iter::empty::<u32>()).unwrap();

    ctx.b.begin_block(Some(then_label)).unwrap();
    emit_block(ctx, then_branch)?;
    terminate_with_branch(ctx, merge_label)?;

    ctx.b.begin_block(Some(else_label)).unwrap();
    if let Some(else_branch) = else_branch {
        emit_else_branch(ctx, else_branch)?;
    }
    terminate_with_branch(ctx, merge_label)?;

    ctx.b.begin_block(Some(merge_label)).unwrap();
    Ok(())
}

fn emit_else_branch(ctx: &mut ShaderContext, else_branch: &ElseBranch) -> KainResult<()> {
    match else_branch {
        ElseBranch::Else(block) => emit_block(ctx, block),
        ElseBranch::ElseIf(cond, then_block, nested) => {
            emit_if_statement(ctx, cond, then_block, nested.as_deref())
        }
    }
}

fn emit_while_statement(ctx: &mut ShaderContext, condition: &Expr, body: &Block) -> KainResult<()> {
    let header_label = ctx.b.id();
    let loop_body_label = ctx.b.id();
    let continue_label = ctx.b.id();
    let merge_label = ctx.b.id();

    terminate_with_branch(ctx, header_label)?;

    ctx.b.begin_block(Some(header_label)).unwrap();
    ctx.b.loop_merge(merge_label, continue_label, rspirv::spirv::LoopControl::NONE, std::iter::empty()).unwrap();
    let (cond_raw, cond_ty) = emit_expr(ctx, condition)?;
    let cond = as_bool(ctx, cond_raw, &cond_ty);
    ctx.b.branch_conditional(cond, loop_body_label, merge_label, std::iter::empty::<u32>()).unwrap();

    ctx.loop_continue_targets.push(continue_label);
    ctx.loop_break_targets.push(merge_label);

    ctx.b.begin_block(Some(loop_body_label)).unwrap();
    emit_block(ctx, body)?;
    terminate_with_branch(ctx, continue_label)?;

    ctx.loop_continue_targets.pop();
    ctx.loop_break_targets.pop();

    ctx.b.begin_block(Some(continue_label)).unwrap();
    terminate_with_branch(ctx, header_label)?;

    ctx.b.begin_block(Some(merge_label)).unwrap();
    Ok(())
}

fn emit_for_statement(
    ctx: &mut ShaderContext,
    binding: &Pattern,
    iter: &Expr,
    body: &Block,
    span: kain_core::span::Span,
) -> KainResult<()> {
    let (name, bind_span) = match binding {
        Pattern::Binding { name, span, .. } => (name.clone(), *span),
        _ => return Err(KainError::codegen("for loop requires binding pattern", span)),
    };

    let (start_expr, end_expr, inclusive) = match iter {
        Expr::Range { start, end, inclusive, .. } => (start.as_deref(), end.as_deref(), *inclusive),
        _ => return Err(KainError::codegen("SPIR-V for loops currently require range iteration", span)),
    };
    let end_expr = end_expr.ok_or_else(|| KainError::codegen("for range requires an end bound", span))?;

    let int_ty_ast = Type::Named { name: "Int".into(), generics: vec![], span: bind_span };
    let int_ty = map_ast_type(ctx.b, &int_ty_ast);
    let int_ptr_ty = ctx.b.type_pointer(None, StorageClass::Function, int_ty);

    let loop_var = ctx.b.variable(int_ptr_ty, None, StorageClass::Function, None);
    let init_val = if let Some(start_expr) = start_expr {
        let (v, _) = emit_expr(ctx, start_expr)?;
        v
    } else {
        ctx.b.constant_bit32(int_ty, 0)
    };
    ctx.b.store(loop_var, init_val, None, std::iter::empty()).unwrap();

    let range_end_var = ctx.b.variable(int_ptr_ty, None, StorageClass::Function, None);
    let (end_val, _) = emit_expr(ctx, end_expr)?;
    ctx.b.store(range_end_var, end_val, None, std::iter::empty()).unwrap();

    let previous = ctx.vars.insert(
        name.clone(),
        VarBinding {
            id: loop_var,
            ty: int_ty_ast.clone(),
            is_ptr: true,
        },
    );

    let header_label = ctx.b.id();
    let loop_body_label = ctx.b.id();
    let continue_label = ctx.b.id();
    let merge_label = ctx.b.id();

    terminate_with_branch(ctx, header_label)?;
    ctx.b.begin_block(Some(header_label)).unwrap();
    ctx.b.loop_merge(merge_label, continue_label, rspirv::spirv::LoopControl::NONE, std::iter::empty()).unwrap();

    let i_val = ctx.b.load(int_ty, None, loop_var, None, std::iter::empty()).unwrap();
    let end_loaded = ctx.b.load(int_ty, None, range_end_var, None, std::iter::empty()).unwrap();
    let bool_ty = ctx.b.type_bool();
    let cond = if inclusive {
        ctx.b.s_less_than_equal(bool_ty, None, i_val, end_loaded).unwrap()
    } else {
        ctx.b.s_less_than(bool_ty, None, i_val, end_loaded).unwrap()
    };
    ctx.b.branch_conditional(cond, loop_body_label, merge_label, std::iter::empty::<u32>()).unwrap();

    ctx.loop_continue_targets.push(continue_label);
    ctx.loop_break_targets.push(merge_label);

    ctx.b.begin_block(Some(loop_body_label)).unwrap();
    emit_block(ctx, body)?;
    terminate_with_branch(ctx, continue_label)?;

    ctx.loop_continue_targets.pop();
    ctx.loop_break_targets.pop();

    ctx.b.begin_block(Some(continue_label)).unwrap();
    let cur_i = ctx.b.load(int_ty, None, loop_var, None, std::iter::empty()).unwrap();
    let one = ctx.b.constant_bit32(int_ty, 1);
    let next_i = ctx.b.i_add(int_ty, None, cur_i, one).unwrap();
    ctx.b.store(loop_var, next_i, None, std::iter::empty()).unwrap();
    terminate_with_branch(ctx, header_label)?;

    ctx.b.begin_block(Some(merge_label)).unwrap();
    if let Some(prev) = previous {
        ctx.vars.insert(name, prev);
    } else {
        ctx.vars.remove(&name);
    }
    Ok(())
}

fn map_ast_type(b: &mut Builder, ty: &Type) -> u32 {
    let float = b.type_float(32);
    let int = b.type_int(32, 1);
    let uint = b.type_int(32, 0);
    match ty {
        Type::Named { name, generics, .. } => match name.as_str() {
            "Float" | "f32" => float,
            "Int" | "i32" => int,
            "UInt" | "u32" => uint,
            "Bool" => b.type_bool(),
            "Vec2" => b.type_vector(float, 2),
            "Vec3" => b.type_vector(float, 3),
            "Vec4" => b.type_vector(float, 4),
            "IVec2" => b.type_vector(int, 2),
            "IVec3" => b.type_vector(int, 3),
            "IVec4" => b.type_vector(int, 4),
            "UVec2" => b.type_vector(uint, 2),
            "UVec3" => b.type_vector(uint, 3),
            "UVec4" => b.type_vector(uint, 4),
            "Mat4" => {
                let v4 = b.type_vector(float, 4);
                b.type_matrix(v4, 4)
            },
            "Sampler2D" => {
                // Dim2D, NotDepth, Arrayed=False, MS=False, Sampled=1, Format=Unknown
                let image = b.type_image(float, rspirv::spirv::Dim::Dim2D, 0, 0, 0, 1, rspirv::spirv::ImageFormat::Unknown, None);
                b.type_sampled_image(image)
            },
            "StorageBuffer" => {
                // StorageBuffer<T> lowers to Block-wrapped runtime array of T.
                let elem_type_ast = if let Some(first) = generics.first() {
                    first.clone()
                } else {
                    Type::Named { name: "Float".into(), generics: vec![], span: kain_core::span::Span::default() }
                };
                let elem_ty = map_ast_type(b, &elem_type_ast);
                let rt_array = b.type_runtime_array(elem_ty);
                b.decorate(rt_array, Decoration::ArrayStride, vec![Operand::LiteralBit32(storage_buffer_stride(ty))]);
                let struct_ty = b.type_struct(vec![rt_array]);
                b.decorate(struct_ty, Decoration::Block, vec![]);
                struct_ty
            },
            "Void" => b.type_void(),
            _ => b.type_void(),
        },
        Type::Ref { inner, .. } => map_ast_type(b, inner),
        Type::Array(inner, _, _) | Type::Slice(inner, _) => map_ast_type(b, inner),
        _ => b.type_void(),
    }
}

fn is_void(ty: &Type) -> bool {
    matches!(ty, Type::Named { name, .. } if name == "Void")
}

fn is_vec4(ty: &Type) -> bool {
    matches!(ty, Type::Named { name, .. } if name == "Vec4")
}

fn is_mat4(ty: &Type) -> bool {
    matches!(ty, Type::Named { name, .. } if name == "Mat4")
}

fn is_float(ty: &Type) -> bool {
    matches!(ty, Type::Named { name, .. } if name == "Float" || name == "f32")
}

fn is_bool(ty: &Type) -> bool {
    matches!(ty, Type::Named { name, .. } if name == "Bool")
}

fn is_uint(ty: &Type) -> bool {
    matches!(ty, Type::Named { name, .. } if name == "UInt" || name == "u32")
}

fn is_int(ty: &Type) -> bool {
    matches!(ty, Type::Named { name, .. } if name == "Int" || name == "i32")
}

fn type_name(ty: &Type) -> Option<&str> {
    if let Type::Named { name, .. } = ty {
        Some(name.as_str())
    } else {
        None
    }
}

fn numeric_dim(ty: &Type) -> usize {
    match type_name(ty) {
        Some("Float") | Some("f32")
        | Some("Int") | Some("i32")
        | Some("UInt") | Some("u32")
        | Some("Bool") => 1,
        Some("Vec2") | Some("IVec2") | Some("UVec2") => 2,
        Some("Vec3") | Some("IVec3") | Some("UVec3") => 3,
        Some("Vec4") | Some("IVec4") | Some("UVec4") => 4,
        _ => 0,
    }
}

fn is_float_vector(ty: &Type) -> bool {
    matches!(type_name(ty), Some("Vec2" | "Vec3" | "Vec4"))
}

fn is_int_vector(ty: &Type) -> bool {
    matches!(type_name(ty), Some("IVec2" | "IVec3" | "IVec4"))
}

fn is_uint_vector(ty: &Type) -> bool {
    matches!(type_name(ty), Some("UVec2" | "UVec3" | "UVec4"))
}

fn is_float_like(ty: &Type) -> bool {
    is_float(ty) || is_float_vector(ty)
}

fn is_int_like(ty: &Type) -> bool {
    is_int(ty) || is_int_vector(ty)
}

fn is_uint_like(ty: &Type) -> bool {
    is_uint(ty) || is_uint_vector(ty)
}

fn is_same_type_family(a: &Type, b: &Type) -> bool {
    type_name(a) == type_name(b)
}

fn float_zero(b: &mut Builder, ty: &Type) -> u32 {
    let float = b.type_float(32);
    match numeric_dim(ty) {
        2 => {
            let vec = b.type_vector(float, 2);
            let z = b.constant_bit32(float, 0.0f32.to_bits());
            b.constant_composite(vec, vec![z, z])
        }
        3 => {
            let vec = b.type_vector(float, 3);
            let z = b.constant_bit32(float, 0.0f32.to_bits());
            b.constant_composite(vec, vec![z, z, z])
        }
        4 => {
            let vec = b.type_vector(float, 4);
            let z = b.constant_bit32(float, 0.0f32.to_bits());
            b.constant_composite(vec, vec![z, z, z, z])
        }
        _ => b.constant_bit32(float, 0.0f32.to_bits()),
    }
}

fn float_one(b: &mut Builder, ty: &Type) -> u32 {
    let float = b.type_float(32);
    match numeric_dim(ty) {
        2 => {
            let vec = b.type_vector(float, 2);
            let o = b.constant_bit32(float, 1.0f32.to_bits());
            b.constant_composite(vec, vec![o, o])
        }
        3 => {
            let vec = b.type_vector(float, 3);
            let o = b.constant_bit32(float, 1.0f32.to_bits());
            b.constant_composite(vec, vec![o, o, o])
        }
        4 => {
            let vec = b.type_vector(float, 4);
            let o = b.constant_bit32(float, 1.0f32.to_bits());
            b.constant_composite(vec, vec![o, o, o, o])
        }
        _ => b.constant_bit32(float, 1.0f32.to_bits()),
    }
}

fn int_zero(b: &mut Builder, ty: &Type) -> u32 {
    let int = b.type_int(32, 1);
    match numeric_dim(ty) {
        2 => {
            let vec = b.type_vector(int, 2);
            let z = b.constant_bit32(int, 0);
            b.constant_composite(vec, vec![z, z])
        }
        3 => {
            let vec = b.type_vector(int, 3);
            let z = b.constant_bit32(int, 0);
            b.constant_composite(vec, vec![z, z, z])
        }
        4 => {
            let vec = b.type_vector(int, 4);
            let z = b.constant_bit32(int, 0);
            b.constant_composite(vec, vec![z, z, z, z])
        }
        _ => b.constant_bit32(int, 0),
    }
}

fn int_one(b: &mut Builder, ty: &Type) -> u32 {
    let int = b.type_int(32, 1);
    match numeric_dim(ty) {
        2 => {
            let vec = b.type_vector(int, 2);
            let o = b.constant_bit32(int, 1);
            b.constant_composite(vec, vec![o, o])
        }
        3 => {
            let vec = b.type_vector(int, 3);
            let o = b.constant_bit32(int, 1);
            b.constant_composite(vec, vec![o, o, o])
        }
        4 => {
            let vec = b.type_vector(int, 4);
            let o = b.constant_bit32(int, 1);
            b.constant_composite(vec, vec![o, o, o, o])
        }
        _ => b.constant_bit32(int, 1),
    }
}

fn uint_zero(b: &mut Builder, ty: &Type) -> u32 {
    let uint = b.type_int(32, 0);
    match numeric_dim(ty) {
        2 => {
            let vec = b.type_vector(uint, 2);
            let z = b.constant_bit32(uint, 0);
            b.constant_composite(vec, vec![z, z])
        }
        3 => {
            let vec = b.type_vector(uint, 3);
            let z = b.constant_bit32(uint, 0);
            b.constant_composite(vec, vec![z, z, z])
        }
        4 => {
            let vec = b.type_vector(uint, 4);
            let z = b.constant_bit32(uint, 0);
            b.constant_composite(vec, vec![z, z, z, z])
        }
        _ => b.constant_bit32(uint, 0),
    }
}

fn uint_one(b: &mut Builder, ty: &Type) -> u32 {
    let uint = b.type_int(32, 0);
    match numeric_dim(ty) {
        2 => {
            let vec = b.type_vector(uint, 2);
            let o = b.constant_bit32(uint, 1);
            b.constant_composite(vec, vec![o, o])
        }
        3 => {
            let vec = b.type_vector(uint, 3);
            let o = b.constant_bit32(uint, 1);
            b.constant_composite(vec, vec![o, o, o])
        }
        4 => {
            let vec = b.type_vector(uint, 4);
            let o = b.constant_bit32(uint, 1);
            b.constant_composite(vec, vec![o, o, o, o])
        }
        _ => b.constant_bit32(uint, 1),
    }
}

fn is_storage_buffer(ty: &Type) -> bool {
    matches!(ty, Type::Named { name, .. } if name == "StorageBuffer")
}

fn is_local_size_param(name: &str) -> bool {
    matches!(name, "LOCAL_SIZE_X" | "LOCAL_SIZE_Y" | "LOCAL_SIZE_Z")
}

fn is_permutation_param(name: &str) -> bool {
    if name.starts_with("CFG_")
        || name.starts_with("ENABLE_")
        || name.starts_with("USE_")
        || name.starts_with("WITH_")
        || name.starts_with("HAS_")
        || name.starts_with("ALLOW_")
        || name.starts_with("SUPPORT_")
    {
        return true;
    }
    name.len() >= 4
        && name.contains('_')
        && name.chars().all(|c| c.is_uppercase() || c == '_' || c.is_ascii_digit())
}

fn emit_permutation_spec_constant(b: &mut Builder, ty: &Type, spec_id: u32) -> Option<u32> {
    let id = match ty {
        Type::Named { name, .. } if name == "Bool" => {
            let bool_ty = b.type_bool();
            b.spec_constant_false(bool_ty)
        }
        Type::Named { name, .. } if name == "UInt" || name == "u32" => {
            let uint_ty = b.type_int(32, 0);
            b.spec_constant_bit32(uint_ty, 0)
        }
        Type::Named { name, .. } if name == "Int" || name == "i32" => {
            let int_ty = b.type_int(32, 1);
            b.spec_constant_bit32(int_ty, 0)
        }
        Type::Named { name, .. } if name == "Float" || name == "f32" => {
            let float_ty = b.type_float(32);
            b.spec_constant_bit32(float_ty, 0f32.to_bits())
        }
        _ => return None,
    };
    b.decorate(id, Decoration::SpecId, vec![Operand::LiteralBit32(spec_id)]);
    Some(id)
}

fn emit_u32_spec_constant(b: &mut Builder, value: u32, spec_id: u32) -> u32 {
    let uint_ty = b.type_int(32, 0);
    let id = b.spec_constant_bit32(uint_ty, value);
    b.decorate(id, Decoration::SpecId, vec![Operand::LiteralBit32(spec_id)]);
    id
}

fn storage_buffer_elem_type(buffer_ty: &Type, span: kain_core::span::Span) -> Type {
    match buffer_ty {
        Type::Named { name, generics, .. } if name == "StorageBuffer" => {
            generics
                .first()
                .cloned()
                .unwrap_or(Type::Named { name: "Float".into(), generics: vec![], span })
        }
        _ => Type::Named { name: "Float".into(), generics: vec![], span },
    }
}

fn storage_buffer_stride(buffer_ty: &Type) -> u32 {
    let elem_ty = storage_buffer_elem_type(buffer_ty, kain_core::span::Span::default());
    match elem_ty {
        Type::Named { ref name, .. } if name == "Vec4" || name == "UVec4" => 16,
        Type::Named { ref name, .. } if name == "Vec3" || name == "UVec3" => 12,
        Type::Named { ref name, .. } if name == "Vec2" || name == "UVec2" => 8,
        _ => 4,
    }
}

fn vector_scalar_type(ctx: &mut ShaderContext, vec_ty: &Type) -> Type {
    if matches!(vec_ty, Type::Named { name, .. } if name.starts_with('U')) {
        Type::Named { name: "UInt".into(), generics: vec![], span: kain_core::span::Span::default() }
    } else if is_uint(vec_ty) {
        Type::Named { name: "UInt".into(), generics: vec![], span: kain_core::span::Span::default() }
    } else {
        let _ = ctx; // keep signature uniform for future extension
        Type::Named { name: "Float".into(), generics: vec![], span: kain_core::span::Span::default() }
    }
}

fn vec_type_from_scalar(scalar_ty: &Type, width: usize, span: kain_core::span::Span) -> Type {
    let is_u = matches!(scalar_ty, Type::Named { name, .. } if name == "UInt" || name == "u32");
    let name = if is_u {
        match width {
            2 => "UVec2",
            3 => "UVec3",
            _ => "UVec4",
        }
    } else {
        match width {
            2 => "Vec2",
            3 => "Vec3",
            _ => "Vec4",
        }
    };
    Type::Named { name: name.into(), generics: vec![], span }
}

fn swizzle_indices(field: &str) -> Option<Vec<u32>> {
    if field.is_empty() || field.len() > 4 {
        return None;
    }
    let mut out = Vec::with_capacity(field.len());
    for ch in field.chars() {
        let idx = match ch {
            'x' | 'r' => 0,
            'y' | 'g' => 1,
            'z' | 'b' => 2,
            'w' | 'a' => 3,
            _ => return None,
        };
        out.push(idx);
    }
    Some(out)
}

fn emit_index_lvalue_ptr(
    ctx: &mut ShaderContext,
    object: &Expr,
    index: &Expr,
    elem_ty: &Type,
    span: kain_core::span::Span,
) -> KainResult<u32> {
    if let Expr::Ident(buffer_name, _) = object {
        if ctx.storage_buffers.contains(buffer_name) {
            let binding = ctx
                .vars
                .get(buffer_name)
                .cloned()
                .ok_or_else(|| KainError::codegen(format!("Unknown storage buffer: {}", buffer_name), span))?;
            let (index_id, _) = emit_expr(ctx, index)?;
            let elem_ty_id = map_ast_type(ctx.b, elem_ty);
            let elem_ptr_ty = ctx.b.type_pointer(None, StorageClass::StorageBuffer, elem_ty_id);
            let uint_ty = ctx.b.type_int(32, 0);
            let zero = ctx.b.constant_bit32(uint_ty, 0);
            let elem_ptr = ctx
                .b
                .access_chain(elem_ptr_ty, None, binding.id, vec![zero, index_id])
                .unwrap();
            return Ok(elem_ptr);
        }
    }
    Err(KainError::codegen("Unsupported lvalue index target in shader", span))
}
