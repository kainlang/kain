pub mod codegen_ue5;
pub mod ue5;
pub mod network_sync_ir;
pub mod network_sync_codegen;
pub mod state_machine_ir;
pub mod state_machine_codegen;

pub use codegen_ue5::{
    generate, generate_with_context, generate_with_context_typed,
    generate_filtered, generate_filtered_typed, generate_from_typed,
    generate_stdlib_functions, Ue5Output
};

// Re-export ue5 module items for easier access
pub use ue5::*;

// Re-export network sync IR
pub use network_sync_ir::{
    NetworkSyncIR, ReplicatedPropertyIR, ReplicationModeIR,
    CompressionSettingsIR, NetworkConfigIR, convert_to_network_sync_ir
};

// Re-export state machine IR and codegen
pub use state_machine_ir::{
    StateMachineIR, StateIR, TransitionIR, convert_to_state_machine_ir
};
pub use state_machine_codegen::{
    generate_state_machine_code, StateMachineCodegenOutput
};
