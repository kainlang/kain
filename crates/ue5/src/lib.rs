pub mod async_task_codegen;
pub mod async_task_ir;
pub mod blueprint_codegen;
pub mod blueprint_ir;
pub mod codegen_ue5;
pub mod network_sync_codegen;
pub mod network_sync_ir;
pub mod state_machine_codegen;
pub mod state_machine_ir;
pub mod ue5;

pub use codegen_ue5::{
    generate, generate_filtered, generate_filtered_typed, generate_from_typed,
    generate_stdlib_functions, generate_with_context, generate_with_context_typed, Ue5Output,
};

// Re-export ue5 module items for easier access
pub use ue5::*;

// Re-export network sync IR
pub use network_sync_ir::{
    convert_to_network_sync_ir, CompressionSettingsIR, NetworkConfigIR, NetworkSyncIR,
    ReplicatedPropertyIR, ReplicationModeIR,
};

// Re-export state machine IR and codegen
pub use state_machine_codegen::{generate_state_machine_code, StateMachineCodegenOutput};
pub use state_machine_ir::{convert_to_state_machine_ir, StateIR, StateMachineIR, TransitionIR};

// Re-export async task IR and codegen
pub use async_task_codegen::{generate_async_task_code, AsyncTaskCodegenOutput};
pub use async_task_ir::{
    convert_to_async_task_ir, AsyncTaskCallbackIR, AsyncTaskFieldIR, AsyncTaskIR, AsyncTaskThreadIR,
};

// Re-export Blueprint integration IR and codegen
pub use blueprint_codegen::{
    generate_async_blueprint_code, generate_blueprint_event_code, generate_k2node_code,
    AsyncBlueprintCodegenOutput, BlueprintEventCodegenOutput, K2NodeCodegenOutput,
};
pub use blueprint_ir::{
    convert_to_blueprint_event_ir, AsyncBlueprintIR, AsyncOutputPinIR, BlueprintEventIR,
    BlueprintParamIR, K2NodeIR, K2PinIR, K2PinType,
};
