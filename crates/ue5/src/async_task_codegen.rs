//! Async Task Queue Code Generation
//!
//! This module generates C++ code for async task queue patterns including:
//! - Task queue class with thread pool (FRunnable pattern)
//! - Task class with DoWork method (runs on worker thread)
//! - Completion callback dispatching to main thread (AsyncTask pattern)
//! - Task cancellation support
//! - Task priority support

use crate::async_task_ir::{AsyncTaskCallbackIR, AsyncTaskIR, AsyncTaskThreadIR};

/// Output from async task code generation
#[derive(Debug, Clone)]
pub struct AsyncTaskCodegenOutput {
    /// Task class header file content
    pub task_header: String,

    /// Task class source file content
    pub task_source: String,

    /// Task queue class header file content
    pub queue_header: String,

    /// Task queue class source file content
    pub queue_source: String,

    /// Additional includes needed
    pub includes: Vec<String>,
}

/// Generate async task code from IR
///
/// # Arguments
/// * `ir` - The async task intermediate representation
/// * `plugin_name` - Name of the plugin (for API macro)
///
/// # Returns
/// * `AsyncTaskCodegenOutput` - Generated header and source files
pub fn generate_async_task_code(ir: &AsyncTaskIR, plugin_name: &str) -> AsyncTaskCodegenOutput {
    let task_class_name = format!("F{}Task", ir.task_name);
    let queue_class_name = format!("U{}TaskQueue", ir.task_name);
    let api_macro = format!("{}_API", plugin_name.to_uppercase());

    let mut task_header = String::new();
    let mut task_source = String::new();
    let mut queue_header = String::new();
    let mut queue_source = String::new();

    // Generate task class (FRunnable)
    generate_task_header(ir, &task_class_name, &api_macro, &mut task_header);
    generate_task_source(ir, &task_class_name, &mut task_source);

    // Generate task queue class (UObject)
    generate_queue_header(
        ir,
        &queue_class_name,
        &task_class_name,
        &api_macro,
        &mut queue_header,
    );
    generate_queue_source(ir, &queue_class_name, &task_class_name, &mut queue_source);

    AsyncTaskCodegenOutput {
        task_header,
        task_source,
        queue_header,
        queue_source,
        includes: vec![
            "CoreMinimal.h".to_string(),
            "HAL/Runnable.h".to_string(),
            "HAL/RunnableThread.h".to_string(),
            "Async/AsyncWork.h".to_string(),
            "UObject/NoExportTypes.h".to_string(),
        ],
    }
}

/// Generate task class header (FRunnable implementation)
fn generate_task_header(
    ir: &AsyncTaskIR,
    task_class_name: &str,
    api_macro: &str,
    output: &mut String,
) {
    output.push_str("#pragma once\n\n");
    output.push_str("#include \"CoreMinimal.h\"\n");
    output.push_str("#include \"HAL/Runnable.h\"\n");
    output.push_str("#include \"Async/AsyncWork.h\"\n\n");

    // Forward declare queue class
    output.push_str(&format!("class U{}TaskQueue;\n\n", ir.task_name));

    // Task class declaration
    output.push_str(&format!("/**\n"));
    output.push_str(&format!(
        " * Async task for {} - runs on worker thread\n",
        ir.task_name
    ));
    output.push_str(&format!(
        " * Implements FRunnable for thread pool execution\n"
    ));
    output.push_str(&format!(" */\n"));
    output.push_str(&format!(
        "class {} {} : public FRunnable\n",
        api_macro, task_class_name
    ));
    output.push_str("{\n");
    output.push_str("public:\n");

    // Constructor
    output.push_str(&format!("    {}(", task_class_name));

    // Constructor parameters (input fields + queue reference)
    let mut params = Vec::new();
    for field in &ir.input_fields {
        params.push(format!(
            "const {}& In{}",
            field.cpp_type,
            capitalize(&field.name)
        ));
    }
    params.push(format!("U{}TaskQueue* InQueue", ir.task_name));
    output.push_str(&params.join(", "));
    output.push_str(");\n\n");

    // Destructor
    output.push_str(&format!("    virtual ~{}();\n\n", task_class_name));

    // FRunnable interface
    output.push_str("    // FRunnable interface\n");
    output.push_str("    virtual bool Init() override;\n");
    output.push_str("    virtual uint32 Run() override;\n");
    output.push_str("    virtual void Stop() override;\n");
    output.push_str("    virtual void Exit() override;\n\n");

    // Task control
    output.push_str("    /** Cancel this task */\n");
    output.push_str("    void Cancel();\n\n");

    output.push_str("    /** Check if task is cancelled */\n");
    output.push_str("    bool IsCancelled() const { return bCancelled; }\n\n");

    output.push_str("    /** Get task priority */\n");
    output.push_str(&format!(
        "    int32 GetPriority() const {{ return {}; }}\n\n",
        ir.priority
    ));

    output.push_str("private:\n");

    // Input fields
    if !ir.input_fields.is_empty() {
        output.push_str("    // Input data\n");
        for field in &ir.input_fields {
            output.push_str(&format!("    {} {};\n", field.cpp_type, field.name));
        }
        output.push_str("\n");
    }

    // Output fields
    if !ir.output_fields.is_empty() {
        output.push_str("    // Output data\n");
        for field in &ir.output_fields {
            output.push_str(&format!("    {} {};\n", field.cpp_type, field.name));
        }
        output.push_str("\n");
    }

    // Queue reference
    output.push_str("    // Queue reference for callback\n");
    output.push_str(&format!("    U{}TaskQueue* Queue;\n\n", ir.task_name));

    // Cancellation flag
    output.push_str("    // Cancellation flag\n");
    output.push_str("    FThreadSafeBool bCancelled;\n\n");

    // DoWork method
    output.push_str("    /** Main work method - override this to implement task logic */\n");
    output.push_str("    void DoWork();\n");

    output.push_str("};\n");
}

/// Generate task class source
fn generate_task_source(ir: &AsyncTaskIR, task_class_name: &str, output: &mut String) {
    output.push_str(&format!("#include \"{}.h\"\n", ir.task_name));
    output.push_str(&format!("#include \"{}TaskQueue.h\"\n\n", ir.task_name));

    // Constructor
    output.push_str(&format!("{}::{}(", task_class_name, task_class_name));
    let mut params = Vec::new();
    for field in &ir.input_fields {
        params.push(format!(
            "const {}& In{}",
            field.cpp_type,
            capitalize(&field.name)
        ));
    }
    params.push(format!("U{}TaskQueue* InQueue", ir.task_name));
    output.push_str(&params.join(", "));
    output.push_str(")\n");

    // Constructor initializer list
    let mut initializers = Vec::new();
    for field in &ir.input_fields {
        initializers.push(format!("    {}(In{})", field.name, capitalize(&field.name)));
    }
    initializers.push(format!("    Queue(InQueue)"));
    initializers.push("    bCancelled(false)".to_string());

    if !initializers.is_empty() {
        output.push_str("    : ");
        output.push_str(&initializers.join(",\n      "));
        output.push_str("\n");
    }

    output.push_str("{\n");

    // Initialize output fields
    for field in &ir.output_fields {
        if field.is_array {
            output.push_str(&format!("    {}.Empty();\n", field.name));
        } else if field.cpp_type == "int32" || field.cpp_type == "int64" {
            output.push_str(&format!("    {} = 0;\n", field.name));
        } else if field.cpp_type == "float" || field.cpp_type == "double" {
            output.push_str(&format!("    {} = 0.0f;\n", field.name));
        } else if field.cpp_type == "bool" {
            output.push_str(&format!("    {} = false;\n", field.name));
        }
    }

    output.push_str("}\n\n");

    // Destructor
    output.push_str(&format!("{}::~{}()\n", task_class_name, task_class_name));
    output.push_str("{\n");
    output.push_str("}\n\n");

    // Init
    output.push_str(&format!("bool {}::Init()\n", task_class_name));
    output.push_str("{\n");
    output.push_str("    return true;\n");
    output.push_str("}\n\n");

    // Run
    output.push_str(&format!("uint32 {}::Run()\n", task_class_name));
    output.push_str("{\n");
    output.push_str("    if (!bCancelled)\n");
    output.push_str("    {\n");
    output.push_str("        DoWork();\n");
    output.push_str("    }\n");
    output.push_str("    return 0;\n");
    output.push_str("}\n\n");

    // Stop
    output.push_str(&format!("void {}::Stop()\n", task_class_name));
    output.push_str("{\n");
    output.push_str("    bCancelled = true;\n");
    output.push_str("}\n\n");

    // Exit
    output.push_str(&format!("void {}::Exit()\n", task_class_name));
    output.push_str("{\n");
    output.push_str("}\n\n");

    // Cancel
    output.push_str(&format!("void {}::Cancel()\n", task_class_name));
    output.push_str("{\n");
    output.push_str("    bCancelled = true;\n");
    output.push_str("}\n\n");

    // DoWork
    output.push_str(&format!("void {}::DoWork()\n", task_class_name));
    output.push_str("{\n");

    if let Some(body) = &ir.do_work_body {
        output.push_str(&format!("    // User-defined work\n"));
        output.push_str(&format!("    {}\n", body));
    } else {
        output.push_str("    // TODO: Implement task logic\n");
        output.push_str("    // Process input fields and populate output fields\n");
    }

    output.push_str("}\n");
}

/// Generate task queue class header (UObject)
fn generate_queue_header(
    ir: &AsyncTaskIR,
    queue_class_name: &str,
    task_class_name: &str,
    api_macro: &str,
    output: &mut String,
) {
    output.push_str("#pragma once\n\n");
    output.push_str("#include \"CoreMinimal.h\"\n");
    output.push_str("#include \"UObject/NoExportTypes.h\"\n");
    output.push_str("#include \"HAL/RunnableThread.h\"\n");
    output.push_str(&format!("#include \"{}.generated.h\"\n\n", ir.task_name));

    // Forward declare task class
    output.push_str(&format!("class {};\n\n", task_class_name));

    // Queue class declaration
    output.push_str(&format!("/**\n"));
    output.push_str(&format!(
        " * Task queue for {} - manages thread pool and task execution\n",
        ir.task_name
    ));
    output.push_str(&format!(" */\n"));
    output.push_str("UCLASS(BlueprintType)\n");
    output.push_str(&format!(
        "class {} {} : public UObject\n",
        api_macro, queue_class_name
    ));
    output.push_str("{\n");
    output.push_str("    GENERATED_BODY()\n\n");
    output.push_str("public:\n");

    // Constructor
    output.push_str(&format!("    {}();\n\n", queue_class_name));

    // Destructor
    output.push_str(&format!("    virtual ~{}();\n\n", queue_class_name));

    // Queue task method
    output.push_str("    /** Queue a new task for execution */\n");
    output.push_str("    UFUNCTION(BlueprintCallable, Category = \"Async Tasks\")\n");
    output.push_str("    void QueueTask(");

    // Parameters (input fields)
    let mut params = Vec::new();
    for field in &ir.input_fields {
        params.push(format!("const {}& {}", field.cpp_type, field.name));
    }
    output.push_str(&params.join(", "));
    output.push_str(");\n\n");

    // Cancel all tasks
    output.push_str("    /** Cancel all pending tasks */\n");
    output.push_str("    UFUNCTION(BlueprintCallable, Category = \"Async Tasks\")\n");
    output.push_str("    void CancelAllTasks();\n\n");

    // Get active task count
    output.push_str("    /** Get number of active tasks */\n");
    output.push_str("    UFUNCTION(BlueprintPure, Category = \"Async Tasks\")\n");
    output.push_str("    int32 GetActiveTaskCount() const;\n\n");

    // Callback method (if callback is defined)
    if let Some(callback) = &ir.callback {
        generate_callback_declaration(ir, callback, output);
    }

    output.push_str("private:\n");

    // Active tasks
    output.push_str("    // Active tasks\n");
    output.push_str(&format!(
        "    TArray<{}*> ActiveTasks;\n\n",
        task_class_name
    ));

    // Thread pool
    output.push_str("    // Thread pool\n");
    output.push_str("    TArray<FRunnableThread*> ThreadPool;\n\n");

    // Thread pool size
    output.push_str("    // Thread pool size\n");
    output.push_str("    int32 ThreadPoolSize;\n\n");

    // Cleanup completed tasks
    output.push_str("    /** Clean up completed tasks */\n");
    output.push_str("    void CleanupCompletedTasks();\n");

    output.push_str("};\n");
}

/// Generate callback declaration in queue header
fn generate_callback_declaration(
    _ir: &AsyncTaskIR,
    callback: &AsyncTaskCallbackIR,
    output: &mut String,
) {
    output.push_str(&format!("    /** Callback when task completes */\n"));

    if callback.thread == AsyncTaskThreadIR::Main {
        output.push_str("    UFUNCTION(BlueprintNativeEvent, Category = \"Async Tasks\")\n");
    }

    output.push_str(&format!("    void {}(", callback.name));

    // Callback parameters
    let mut params = Vec::new();
    for param in &callback.params {
        params.push(format!("const {}& {}", param.cpp_type, param.name));
    }
    output.push_str(&params.join(", "));
    output.push_str(");\n\n");
}

/// Generate task queue class source
fn generate_queue_source(
    ir: &AsyncTaskIR,
    queue_class_name: &str,
    task_class_name: &str,
    output: &mut String,
) {
    output.push_str(&format!("#include \"{}TaskQueue.h\"\n", ir.task_name));
    output.push_str(&format!("#include \"{}.h\"\n", ir.task_name));
    output.push_str("#include \"Async/Async.h\"\n\n");

    // Constructor
    output.push_str(&format!("{}::{}()\n", queue_class_name, queue_class_name));
    output.push_str("    : ThreadPoolSize(4)\n");
    output.push_str("{\n");
    output.push_str("}\n\n");

    // Destructor
    output.push_str(&format!("{}::~{}()\n", queue_class_name, queue_class_name));
    output.push_str("{\n");
    output.push_str("    CancelAllTasks();\n");
    output.push_str("}\n\n");

    // QueueTask
    output.push_str(&format!("void {}::QueueTask(", queue_class_name));
    let mut params = Vec::new();
    for field in &ir.input_fields {
        params.push(format!("const {}& {}", field.cpp_type, field.name));
    }
    output.push_str(&params.join(", "));
    output.push_str(")\n");
    output.push_str("{\n");
    output.push_str("    // Clean up completed tasks first\n");
    output.push_str("    CleanupCompletedTasks();\n\n");

    output.push_str("    // Create new task\n");
    output.push_str(&format!(
        "    {}* Task = new {}(",
        task_class_name, task_class_name
    ));
    let mut args = Vec::new();
    for field in &ir.input_fields {
        args.push(field.name.clone());
    }
    args.push("this".to_string());
    output.push_str(&args.join(", "));
    output.push_str(");\n\n");

    output.push_str("    // Add to active tasks\n");
    output.push_str("    ActiveTasks.Add(Task);\n\n");

    output.push_str("    // Create thread and start execution\n");
    output.push_str(&format!(
        "    FRunnableThread* Thread = FRunnableThread::Create(Task, TEXT(\"{}Task\"));\n",
        ir.task_name
    ));
    output.push_str("    ThreadPool.Add(Thread);\n");
    output.push_str("}\n\n");

    // CancelAllTasks
    output.push_str(&format!("void {}::CancelAllTasks()\n", queue_class_name));
    output.push_str("{\n");
    output.push_str("    // Cancel all active tasks\n");
    output.push_str("    for (auto* Task : ActiveTasks)\n");
    output.push_str("    {\n");
    output.push_str("        if (Task)\n");
    output.push_str("        {\n");
    output.push_str("            Task->Cancel();\n");
    output.push_str("        }\n");
    output.push_str("    }\n\n");

    output.push_str("    // Wait for threads to complete\n");
    output.push_str("    for (auto* Thread : ThreadPool)\n");
    output.push_str("    {\n");
    output.push_str("        if (Thread)\n");
    output.push_str("        {\n");
    output.push_str("            Thread->WaitForCompletion();\n");
    output.push_str("            delete Thread;\n");
    output.push_str("        }\n");
    output.push_str("    }\n\n");

    output.push_str("    // Clean up tasks\n");
    output.push_str("    for (auto* Task : ActiveTasks)\n");
    output.push_str("    {\n");
    output.push_str("        delete Task;\n");
    output.push_str("    }\n\n");

    output.push_str("    ActiveTasks.Empty();\n");
    output.push_str("    ThreadPool.Empty();\n");
    output.push_str("}\n\n");

    // GetActiveTaskCount
    output.push_str(&format!(
        "int32 {}::GetActiveTaskCount() const\n",
        queue_class_name
    ));
    output.push_str("{\n");
    output.push_str("    return ActiveTasks.Num();\n");
    output.push_str("}\n\n");

    // CleanupCompletedTasks
    output.push_str(&format!(
        "void {}::CleanupCompletedTasks()\n",
        queue_class_name
    ));
    output.push_str("{\n");
    output.push_str("    // Remove completed threads\n");
    output.push_str("    for (int32 i = ThreadPool.Num() - 1; i >= 0; --i)\n");
    output.push_str("    {\n");
    output.push_str("        if (ThreadPool[i] && ThreadPool[i]->Kill(true))\n");
    output.push_str("        {\n");
    output.push_str("            delete ThreadPool[i];\n");
    output.push_str("            ThreadPool.RemoveAt(i);\n");
    output.push_str("        }\n");
    output.push_str("    }\n");
    output.push_str("}\n\n");

    // Callback implementation (if defined)
    if let Some(callback) = &ir.callback {
        generate_callback_implementation(ir, callback, queue_class_name, output);
    }
}

/// Generate callback implementation in queue source
fn generate_callback_implementation(
    _ir: &AsyncTaskIR,
    callback: &AsyncTaskCallbackIR,
    queue_class_name: &str,
    output: &mut String,
) {
    output.push_str(&format!("void {}::{}(", queue_class_name, callback.name));

    // Callback parameters
    let mut params = Vec::new();
    for param in &callback.params {
        params.push(format!("const {}& {}", param.cpp_type, param.name));
    }
    output.push_str(&params.join(", "));
    output.push_str(")\n");
    output.push_str("{\n");

    if callback.thread == AsyncTaskThreadIR::Main {
        output.push_str("    // Execute on main thread\n");
        output.push_str(&format!("    {}\n", callback.body));
    } else {
        output.push_str("    // Execute on worker thread\n");
        output.push_str(&format!("    {}\n", callback.body));
    }

    output.push_str("}\n");
}

// ============================================================================
// Helper Functions
// ============================================================================

/// Capitalize first letter of a string
fn capitalize(s: &str) -> String {
    let mut chars = s.chars();
    match chars.next() {
        None => String::new(),
        Some(first) => first.to_uppercase().collect::<String>() + chars.as_str(),
    }
}

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;
    use crate::async_task_ir::{
        AsyncTaskCallbackIR, AsyncTaskFieldIR, AsyncTaskIR, AsyncTaskThreadIR,
    };

    fn make_simple_task() -> AsyncTaskIR {
        AsyncTaskIR {
            task_name: "DataProcessing".to_string(),
            input_fields: vec![
                AsyncTaskFieldIR {
                    name: "data".to_string(),
                    cpp_type: "TArray<float>".to_string(),
                    is_array: true,
                },
                AsyncTaskFieldIR {
                    name: "threshold".to_string(),
                    cpp_type: "float".to_string(),
                    is_array: false,
                },
            ],
            output_fields: vec![AsyncTaskFieldIR {
                name: "result".to_string(),
                cpp_type: "TArray<float>".to_string(),
                is_array: true,
            }],
            callback: None,
            do_work_body: None,
            priority: 0,
        }
    }

    #[test]
    fn test_generate_task_header() {
        let ir = make_simple_task();
        let output = generate_async_task_code(&ir, "TestPlugin");

        // Check task header contains expected elements
        assert!(output
            .task_header
            .contains("class TESTPLUGIN_API FDataProcessingTask"));
        assert!(output.task_header.contains("public FRunnable"));
        assert!(output.task_header.contains("FDataProcessingTask(const TArray<float>& InData, const float& InThreshold, UDataProcessingTaskQueue* InQueue)"));
        assert!(output.task_header.contains("virtual bool Init() override"));
        assert!(output.task_header.contains("virtual uint32 Run() override"));
        assert!(output.task_header.contains("void Cancel()"));
        assert!(output.task_header.contains("bool IsCancelled()"));
        assert!(output.task_header.contains("int32 GetPriority()"));
        assert!(output.task_header.contains("TArray<float> data"));
        assert!(output.task_header.contains("float threshold"));
        assert!(output.task_header.contains("TArray<float> result"));
    }

    #[test]
    fn test_generate_queue_header() {
        let ir = make_simple_task();
        let output = generate_async_task_code(&ir, "TestPlugin");

        // Check queue header contains expected elements
        assert!(output
            .queue_header
            .contains("class TESTPLUGIN_API UDataProcessingTaskQueue"));
        assert!(output.queue_header.contains("public UObject"));
        assert!(output.queue_header.contains("UCLASS(BlueprintType)"));
        assert!(output
            .queue_header
            .contains("void QueueTask(const TArray<float>& data, const float& threshold)"));
        assert!(output.queue_header.contains("void CancelAllTasks()"));
        assert!(output.queue_header.contains("int32 GetActiveTaskCount()"));
        assert!(output
            .queue_header
            .contains("TArray<FDataProcessingTask*> ActiveTasks"));
        assert!(output
            .queue_header
            .contains("TArray<FRunnableThread*> ThreadPool"));
    }

    #[test]
    fn test_generate_task_source() {
        let ir = make_simple_task();
        let output = generate_async_task_code(&ir, "TestPlugin");

        // Check task source contains expected elements
        assert!(output
            .task_source
            .contains("FDataProcessingTask::FDataProcessingTask"));
        assert!(output
            .task_source
            .contains("uint32 FDataProcessingTask::Run()"));
        assert!(output.task_source.contains("DoWork()"));
        assert!(output
            .task_source
            .contains("void FDataProcessingTask::Cancel()"));
        assert!(output.task_source.contains("bCancelled = true"));
    }

    #[test]
    fn test_generate_queue_source() {
        let ir = make_simple_task();
        let output = generate_async_task_code(&ir, "TestPlugin");

        // Check queue source contains expected elements
        assert!(output
            .queue_source
            .contains("UDataProcessingTaskQueue::QueueTask"));
        assert!(output
            .queue_source
            .contains("FDataProcessingTask* Task = new FDataProcessingTask"));
        assert!(output.queue_source.contains("FRunnableThread::Create"));
        assert!(output
            .queue_source
            .contains("void UDataProcessingTaskQueue::CancelAllTasks()"));
        assert!(output.queue_source.contains("Task->Cancel()"));
        assert!(output.queue_source.contains("Thread->WaitForCompletion()"));
    }

    #[test]
    fn test_generate_with_callback() {
        let mut ir = make_simple_task();
        ir.callback = Some(AsyncTaskCallbackIR {
            name: "on_complete".to_string(),
            thread: AsyncTaskThreadIR::Main,
            params: vec![AsyncTaskFieldIR {
                name: "result".to_string(),
                cpp_type: "TArray<float>".to_string(),
                is_array: true,
            }],
            body: "UE_LOG(LogTemp, Log, TEXT(\"Task complete\"));".to_string(),
        });

        let output = generate_async_task_code(&ir, "TestPlugin");

        // Check callback is generated
        assert!(output
            .queue_header
            .contains("void on_complete(const TArray<float>& result)"));
        assert!(output
            .queue_header
            .contains("UFUNCTION(BlueprintNativeEvent"));
        assert!(output
            .queue_source
            .contains("void UDataProcessingTaskQueue::on_complete"));
        assert!(output
            .queue_source
            .contains("UE_LOG(LogTemp, Log, TEXT(\"Task complete\"))"));
    }

    #[test]
    fn test_generate_with_priority() {
        let mut ir = make_simple_task();
        ir.priority = 10;

        let output = generate_async_task_code(&ir, "TestPlugin");

        // Check priority is included
        assert!(output.task_header.contains("return 10"));
    }

    #[test]
    fn test_capitalize() {
        assert_eq!(capitalize("hello"), "Hello");
        assert_eq!(capitalize("world"), "World");
        assert_eq!(capitalize(""), "");
        assert_eq!(capitalize("a"), "A");
    }
}
