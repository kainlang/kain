//! Async Task Queue Intermediate Representation
//!
//! This module defines the IR structures for async task queue patterns
//! and provides conversion from AST to IR with proper type mapping.
//!
//! The AsyncTask system supports:
//! - Task queue class with thread pool (FRunnable pattern)
//! - Task class with DoWork method (runs on worker thread)
//! - Completion callback dispatching to main thread (AsyncTask pattern)
//! - Task cancellation support
//! - Task priority support

use crate::ue5::context::Ue5Context;
use crate::ue5::types::TypeMapper;
use kain_core::ast::{AsyncTaskCallback, AsyncTaskDef, AsyncTaskThread, Block, Field};

/// Async task intermediate representation
/// Represents a task that can be executed on worker threads with completion callbacks
#[derive(Debug, Clone)]
pub struct AsyncTaskIR {
    /// Name of the task (without U prefix)
    pub task_name: String,

    /// Input fields (data passed to task)
    pub input_fields: Vec<AsyncTaskFieldIR>,

    /// Output fields (data returned from task)
    pub output_fields: Vec<AsyncTaskFieldIR>,

    /// Optional completion callback
    pub callback: Option<AsyncTaskCallbackIR>,

    /// Optional DoWork implementation
    pub do_work_body: Option<String>,

    /// Task priority (higher = executed first)
    pub priority: i32,
}

/// A single input or output field for the task
#[derive(Debug, Clone)]
pub struct AsyncTaskFieldIR {
    /// Field name (KAIN identifier)
    pub name: String,

    /// C++ type string (e.g., "TArray<float>", "FVector", "int32")
    pub cpp_type: String,

    /// Whether this field is an array
    pub is_array: bool,
}

/// Completion callback for async task
#[derive(Debug, Clone)]
pub struct AsyncTaskCallbackIR {
    /// Callback method name
    pub name: String,

    /// Thread to execute callback on
    pub thread: AsyncTaskThreadIR,

    /// Callback parameters (typically output fields)
    pub params: Vec<AsyncTaskFieldIR>,

    /// Callback body (C++ code)
    pub body: String,
}

/// Thread specification for callback execution
#[derive(Debug, Clone, PartialEq)]
pub enum AsyncTaskThreadIR {
    /// Execute callback on main game thread (via AsyncTask)
    Main,

    /// Execute callback on worker thread (same thread as DoWork)
    Worker,
}

impl Default for AsyncTaskIR {
    fn default() -> Self {
        Self {
            task_name: String::new(),
            input_fields: Vec::new(),
            output_fields: Vec::new(),
            callback: None,
            do_work_body: None,
            priority: 0,
        }
    }
}

/// Convert an async task definition from AST to AsyncTaskIR
///
/// # Arguments
/// * `task_def` - The async task definition from AST
/// * `ctx` - UE5 compilation context for type mapping
///
/// # Returns
/// * `Ok(AsyncTaskIR)` - Successfully converted IR
/// * `Err(String)` - Conversion error with description
pub fn convert_to_async_task_ir(
    task_def: &AsyncTaskDef,
    ctx: &Ue5Context,
) -> Result<AsyncTaskIR, String> {
    // Create type mapper with context knowledge
    let mut type_mapper = TypeMapper::with_knowledge(ctx.knowledge.clone());

    // Register all known types from context
    for enum_name in &ctx.enum_names {
        type_mapper.register_enum(enum_name.clone());
    }
    for struct_name in &ctx.struct_names {
        type_mapper.register_struct(struct_name.clone());
    }
    for component_name in &ctx.component_names {
        type_mapper.register_component(component_name.clone());
    }
    for actor_name in &ctx.actor_names {
        type_mapper.register_actor(actor_name.clone());
    }
    for delegate_name in &ctx.delegate_names {
        type_mapper.register_delegate(delegate_name.clone());
    }

    // Convert input fields
    let input_fields = task_def
        .input_fields
        .iter()
        .map(|field| convert_field(field, &type_mapper))
        .collect::<Result<Vec<_>, _>>()?;

    // Convert output fields
    let output_fields = task_def
        .output_fields
        .iter()
        .map(|field| convert_field(field, &type_mapper))
        .collect::<Result<Vec<_>, _>>()?;

    // Convert callback if present
    let callback = if let Some(cb) = &task_def.callback {
        Some(convert_callback(cb, &type_mapper, ctx)?)
    } else {
        None
    };

    // Convert do_work body if present
    let do_work_body = if let Some(block) = &task_def.do_work {
        Some(convert_block_to_cpp(block, ctx))
    } else {
        None
    };

    // Extract priority from attributes or use default
    let priority = task_def.priority.unwrap_or(0);

    Ok(AsyncTaskIR {
        task_name: task_def.name.clone(),
        input_fields,
        output_fields,
        callback,
        do_work_body,
        priority,
    })
}

/// Convert a field to AsyncTaskFieldIR
fn convert_field(field: &Field, type_mapper: &TypeMapper) -> Result<AsyncTaskFieldIR, String> {
    // Map KAIN type to C++ type
    let cpp_type = type_mapper.map_type_string(&field.ty);

    // Check if this is an array type
    let is_array = cpp_type.starts_with("TArray<");

    Ok(AsyncTaskFieldIR {
        name: field.name.clone(),
        cpp_type,
        is_array,
    })
}

/// Convert callback definition to AsyncTaskCallbackIR
fn convert_callback(
    callback: &AsyncTaskCallback,
    type_mapper: &TypeMapper,
    ctx: &Ue5Context,
) -> Result<AsyncTaskCallbackIR, String> {
    // Convert thread specification
    let thread = match callback.thread {
        AsyncTaskThread::Main => AsyncTaskThreadIR::Main,
        AsyncTaskThread::Worker => AsyncTaskThreadIR::Worker,
    };

    // Convert parameters
    let params = callback
        .params
        .iter()
        .map(|param| {
            let cpp_type = type_mapper.map_type_string(&param.ty);
            let is_array = cpp_type.starts_with("TArray<");
            Ok(AsyncTaskFieldIR {
                name: param.name.clone(),
                cpp_type,
                is_array,
            })
        })
        .collect::<Result<Vec<_>, String>>()?;

    // Convert body to C++ code
    let body = convert_block_to_cpp(&callback.body, ctx);

    Ok(AsyncTaskCallbackIR {
        name: callback.name.clone(),
        thread,
        params,
        body,
    })
}

/// Convert a KAIN block to C++ code
///
/// This is a placeholder implementation that will be replaced with proper
/// expression codegen when the full codegen pipeline is integrated.
fn convert_block_to_cpp(block: &Block, _ctx: &Ue5Context) -> String {
    // For now, return a placeholder comment
    // TODO: Integrate with expression codegen from ue5 crate
    format!("/* Block with {} statements */", block.stmts.len())
}

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;
    use kain_core::ast::{
        AsyncTaskCallback, AsyncTaskDef, AsyncTaskThread, Attribute, Block, Field, Param, Type,
        Visibility,
    };
    use kain_core::span::Span;

    fn dummy_span() -> Span {
        Span::new(0, 0)
    }

    fn make_simple_field(name: &str, ty: Type) -> Field {
        Field {
            name: name.to_string(),
            ty,
            attributes: vec![],
            visibility: Visibility::Public,
            default: None,
            weak: false,
            span: dummy_span(),
        }
    }

    fn make_simple_param(name: &str, ty: Type) -> Param {
        Param {
            name: name.to_string(),
            ty,
            mutable: false,
            default: None,
            span: dummy_span(),
        }
    }

    #[test]
    fn test_convert_simple_async_task() {
        let ctx = Ue5Context::new("TestPlugin", None);

        let task_def = AsyncTaskDef {
            name: "DataProcessingTask".to_string(),
            input_fields: vec![
                make_simple_field(
                    "data",
                    Type::Named {
                        name: "Array".to_string(),
                        generics: vec![Type::Named {
                            name: "Float".to_string(),
                            generics: vec![],
                            span: dummy_span(),
                        }],
                        span: dummy_span(),
                    },
                ),
                make_simple_field(
                    "threshold",
                    Type::Named {
                        name: "Float".to_string(),
                        generics: vec![],
                        span: dummy_span(),
                    },
                ),
            ],
            output_fields: vec![make_simple_field(
                "result",
                Type::Named {
                    name: "Array".to_string(),
                    generics: vec![Type::Named {
                        name: "Float".to_string(),
                        generics: vec![],
                        span: dummy_span(),
                    }],
                    span: dummy_span(),
                },
            )],
            callback: None,
            do_work: None,
            priority: None,
            attributes: vec![],
            span: dummy_span(),
        };

        let ir = convert_to_async_task_ir(&task_def, &ctx).unwrap();

        assert_eq!(ir.task_name, "DataProcessingTask");
        assert_eq!(ir.input_fields.len(), 2);
        assert_eq!(ir.input_fields[0].name, "data");
        assert_eq!(ir.input_fields[0].cpp_type, "TArray<float>");
        assert_eq!(ir.input_fields[0].is_array, true);
        assert_eq!(ir.input_fields[1].name, "threshold");
        assert_eq!(ir.input_fields[1].cpp_type, "float");
        assert_eq!(ir.output_fields.len(), 1);
        assert_eq!(ir.output_fields[0].name, "result");
        assert_eq!(ir.output_fields[0].cpp_type, "TArray<float>");
        assert_eq!(ir.priority, 0);
    }

    #[test]
    fn test_convert_async_task_with_callback() {
        let ctx = Ue5Context::new("TestPlugin", None);

        let task_def = AsyncTaskDef {
            name: "TestTask".to_string(),
            input_fields: vec![],
            output_fields: vec![make_simple_field(
                "result",
                Type::Named {
                    name: "Int".to_string(),
                    generics: vec![],
                    span: dummy_span(),
                },
            )],
            callback: Some(AsyncTaskCallback {
                name: "on_complete".to_string(),
                thread: AsyncTaskThread::Main,
                params: vec![make_simple_param(
                    "result",
                    Type::Named {
                        name: "Int".to_string(),
                        generics: vec![],
                        span: dummy_span(),
                    },
                )],
                body: Block {
                    stmts: vec![],
                    span: dummy_span(),
                },
                attributes: vec![],
                span: dummy_span(),
            }),
            do_work: None,
            priority: Some(10),
            attributes: vec![],
            span: dummy_span(),
        };

        let ir = convert_to_async_task_ir(&task_def, &ctx).unwrap();

        assert_eq!(ir.task_name, "TestTask");
        assert_eq!(ir.priority, 10);
        assert!(ir.callback.is_some());

        let callback = ir.callback.unwrap();
        assert_eq!(callback.name, "on_complete");
        assert_eq!(callback.thread, AsyncTaskThreadIR::Main);
        assert_eq!(callback.params.len(), 1);
        assert_eq!(callback.params[0].name, "result");
        assert_eq!(callback.params[0].cpp_type, "int64");
    }

    #[test]
    fn test_convert_field_with_array() {
        let type_mapper = TypeMapper::new();

        let field = make_simple_field(
            "values",
            Type::Named {
                name: "Array".to_string(),
                generics: vec![Type::Named {
                    name: "Float".to_string(),
                    generics: vec![],
                    span: dummy_span(),
                }],
                span: dummy_span(),
            },
        );

        let field_ir = convert_field(&field, &type_mapper).unwrap();

        assert_eq!(field_ir.name, "values");
        assert_eq!(field_ir.cpp_type, "TArray<float>");
        assert_eq!(field_ir.is_array, true);
    }

    #[test]
    fn test_convert_field_scalar() {
        let type_mapper = TypeMapper::new();

        let field = make_simple_field(
            "count",
            Type::Named {
                name: "Int".to_string(),
                generics: vec![],
                span: dummy_span(),
            },
        );

        let field_ir = convert_field(&field, &type_mapper).unwrap();

        assert_eq!(field_ir.name, "count");
        assert_eq!(field_ir.cpp_type, "int64");
        assert_eq!(field_ir.is_array, false);
    }

    #[test]
    fn test_callback_thread_conversion() {
        let ctx = Ue5Context::new("TestPlugin", None);
        let type_mapper = TypeMapper::new();

        let main_callback = AsyncTaskCallback {
            name: "on_complete".to_string(),
            thread: AsyncTaskThread::Main,
            params: vec![],
            body: Block {
                stmts: vec![],
                span: dummy_span(),
            },
            attributes: vec![],
            span: dummy_span(),
        };

        let callback_ir = convert_callback(&main_callback, &type_mapper, &ctx).unwrap();
        assert_eq!(callback_ir.thread, AsyncTaskThreadIR::Main);

        let worker_callback = AsyncTaskCallback {
            name: "on_complete".to_string(),
            thread: AsyncTaskThread::Worker,
            params: vec![],
            body: Block {
                stmts: vec![],
                span: dummy_span(),
            },
            attributes: vec![],
            span: dummy_span(),
        };

        let callback_ir = convert_callback(&worker_callback, &type_mapper, &ctx).unwrap();
        assert_eq!(callback_ir.thread, AsyncTaskThreadIR::Worker);
    }
}
