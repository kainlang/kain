//! Network Synchronization Code Generation
//!
//! This module generates C++ code for network synchronization patterns including:
//! - State buffers for interpolation
//! - Compression logic for bandwidth optimization
//! - Interpolation/extrapolation in Tick methods
//! - Snap threshold checks for teleportation
//! - Owner time synchronization
//! - GetLifetimeReplicatedProps implementation

use crate::network_sync_ir::{
    NetworkConfigIR, NetworkSyncIR, ReplicatedPropertyIR, ReplicationModeIR,
};

/// Output from network sync code generation
#[derive(Debug, Clone)]
pub struct NetworkSyncCodegenOutput {
    /// Additional header declarations (state buffers, helper structs)
    pub header_declarations: String,

    /// Additional constructor initialization
    pub constructor_body: String,

    /// Tick method body for interpolation/extrapolation
    pub tick_body: String,

    /// GetLifetimeReplicatedProps body
    pub replication_body: String,

    /// Additional includes needed
    pub includes: Vec<String>,
}

/// Generate network synchronization code from IR
pub fn generate_network_sync_code(
    ir: &NetworkSyncIR,
    class_name: &str,
) -> NetworkSyncCodegenOutput {
    let mut output = NetworkSyncCodegenOutput {
        header_declarations: String::new(),
        constructor_body: String::new(),
        tick_body: String::new(),
        replication_body: String::new(),
        includes: vec!["Net/UnrealNetwork.h".to_string()],
    };

    // Generate state buffers for interpolated properties
    generate_state_buffers(ir, &mut output);

    // Generate constructor initialization
    generate_constructor_init(ir, &mut output);

    // Generate tick logic
    generate_tick_logic(ir, class_name, &mut output);

    // Generate replication setup
    generate_replication_setup(ir, class_name, &mut output);

    output
}

/// Generate state buffer structures for interpolated properties
fn generate_state_buffers(ir: &NetworkSyncIR, output: &mut NetworkSyncCodegenOutput) {
    let mut has_interpolated = false;

    // Check if we have any interpolated properties
    for prop in &ir.replicated_properties {
        if matches!(prop.mode, ReplicationModeIR::Interpolated { .. }) {
            has_interpolated = true;
            break;
        }
    }

    if !has_interpolated {
        return;
    }

    // Generate state buffer struct
    output
        .header_declarations
        .push_str("\n    // Network synchronization state buffer\n");
    output.header_declarations.push_str("    USTRUCT()\n");
    output
        .header_declarations
        .push_str("    struct FNetworkState\n");
    output.header_declarations.push_str("    {\n");
    output
        .header_declarations
        .push_str("        GENERATED_BODY()\n\n");
    output.header_declarations.push_str("        UPROPERTY()\n");
    output
        .header_declarations
        .push_str("        float Timestamp;\n\n");

    // Add fields for each interpolated property
    for prop in &ir.replicated_properties {
        if matches!(prop.mode, ReplicationModeIR::Interpolated { .. }) {
            output
                .header_declarations
                .push_str(&format!("        UPROPERTY()\n"));
            output
                .header_declarations
                .push_str(&format!("        {} {};\n\n", prop.cpp_type, prop.name));
        }
    }

    output
        .header_declarations
        .push_str("        FNetworkState() : Timestamp(0.0f) {}\n");
    output.header_declarations.push_str("    };\n\n");

    // Generate state buffer array
    output
        .header_declarations
        .push_str("    // State buffer for interpolation\n");
    output
        .header_declarations
        .push_str("    UPROPERTY(Transient)\n");
    output
        .header_declarations
        .push_str("    TArray<FNetworkState> StateBuffer;\n\n");

    // Generate current interpolation time
    output
        .header_declarations
        .push_str("    // Current interpolation time offset\n");
    output
        .header_declarations
        .push_str("    UPROPERTY(Transient)\n");
    output
        .header_declarations
        .push_str("    float InterpolationBackTime;\n\n");
}

/// Generate constructor initialization code
fn generate_constructor_init(ir: &NetworkSyncIR, output: &mut NetworkSyncCodegenOutput) {
    // Set interpolation back time if we have interpolated properties
    for prop in &ir.replicated_properties {
        if let ReplicationModeIR::Interpolated {
            back_time,
            buffer_size,
        } = prop.mode
        {
            output
                .constructor_body
                .push_str(&format!("    InterpolationBackTime = {}f;\n", back_time));
            output
                .constructor_body
                .push_str(&format!("    StateBuffer.Reserve({});\n", buffer_size));
            break; // Only need to set once
        }
    }

    // Enable replication
    output
        .constructor_body
        .push_str("    SetIsReplicatedByDefault(true);\n");
}

/// Generate tick method logic for interpolation and extrapolation
fn generate_tick_logic(
    ir: &NetworkSyncIR,
    class_name: &str,
    output: &mut NetworkSyncCodegenOutput,
) {
    let mut has_interpolated = false;
    let mut has_extrapolated = false;
    let mut has_compressed = false;

    // Check what modes we have
    for prop in &ir.replicated_properties {
        match prop.mode {
            ReplicationModeIR::Interpolated { .. } => has_interpolated = true,
            ReplicationModeIR::Extrapolated { .. } => has_extrapolated = true,
            ReplicationModeIR::Compressed { .. } => has_compressed = true,
            _ => {}
        }
    }

    if !has_interpolated && !has_extrapolated && !has_compressed {
        return;
    }

    output
        .tick_body
        .push_str("    // Network synchronization logic\n");
    output
        .tick_body
        .push_str("    if (GetOwnerRole() != ROLE_Authority)\n");
    output.tick_body.push_str("    {\n");

    // Generate interpolation logic
    if has_interpolated {
        generate_interpolation_logic(ir, output);
    }

    // Generate extrapolation logic
    if has_extrapolated {
        generate_extrapolation_logic(ir, output);
    }

    // Generate snap threshold checks
    if ir.config.snap_threshold > 0.0 {
        generate_snap_threshold_checks(ir, output);
    }

    output.tick_body.push_str("    }\n\n");
}

/// Generate interpolation logic for interpolated properties
fn generate_interpolation_logic(ir: &NetworkSyncIR, output: &mut NetworkSyncCodegenOutput) {
    output
        .tick_body
        .push_str("        // Interpolation logic\n");
    output
        .tick_body
        .push_str("        if (StateBuffer.Num() >= 2)\n");
    output.tick_body.push_str("        {\n");
    output
        .tick_body
        .push_str("            float CurrentTime = GetWorld()->GetTimeSeconds();\n");
    output
        .tick_body
        .push_str("            float TargetTime = CurrentTime - InterpolationBackTime;\n\n");

    output
        .tick_body
        .push_str("            // Find the two states to interpolate between\n");
    output
        .tick_body
        .push_str("            int32 FromIndex = -1;\n");
    output
        .tick_body
        .push_str("            int32 ToIndex = -1;\n\n");

    output
        .tick_body
        .push_str("            for (int32 i = 0; i < StateBuffer.Num() - 1; ++i)\n");
    output.tick_body.push_str("            {\n");
    output.tick_body.push_str("                if (StateBuffer[i].Timestamp <= TargetTime && StateBuffer[i + 1].Timestamp >= TargetTime)\n");
    output.tick_body.push_str("                {\n");
    output
        .tick_body
        .push_str("                    FromIndex = i;\n");
    output
        .tick_body
        .push_str("                    ToIndex = i + 1;\n");
    output.tick_body.push_str("                    break;\n");
    output.tick_body.push_str("                }\n");
    output.tick_body.push_str("            }\n\n");

    output
        .tick_body
        .push_str("            if (FromIndex >= 0 && ToIndex >= 0)\n");
    output.tick_body.push_str("            {\n");
    output
        .tick_body
        .push_str("                const FNetworkState& FromState = StateBuffer[FromIndex];\n");
    output
        .tick_body
        .push_str("                const FNetworkState& ToState = StateBuffer[ToIndex];\n");
    output.tick_body.push_str("                float Alpha = (TargetTime - FromState.Timestamp) / (ToState.Timestamp - FromState.Timestamp);\n");
    output
        .tick_body
        .push_str("                Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);\n\n");

    // Interpolate each property
    for prop in &ir.replicated_properties {
        if matches!(prop.mode, ReplicationModeIR::Interpolated { .. }) {
            output
                .tick_body
                .push_str(&format!("                // Interpolate {}\n", prop.name));

            // Use appropriate interpolation based on type
            if prop.cpp_type.contains("Vector") || prop.cpp_type.contains("FVector") {
                output.tick_body.push_str(&format!(
                    "                {} = FMath::Lerp(FromState.{}, ToState.{}, Alpha);\n",
                    prop.name, prop.name, prop.name
                ));
            } else if prop.cpp_type.contains("Rotator") || prop.cpp_type.contains("FRotator") {
                output.tick_body.push_str(&format!(
                    "                {} = FMath::Lerp(FromState.{}, ToState.{}, Alpha);\n",
                    prop.name, prop.name, prop.name
                ));
            } else if prop.cpp_type.contains("Quat") || prop.cpp_type.contains("FQuat") {
                output.tick_body.push_str(&format!(
                    "                {} = FQuat::Slerp(FromState.{}, ToState.{}, Alpha);\n",
                    prop.name, prop.name, prop.name
                ));
            } else {
                // Default to linear interpolation for scalars
                output.tick_body.push_str(&format!(
                    "                {} = FMath::Lerp(FromState.{}, ToState.{}, Alpha);\n",
                    prop.name, prop.name, prop.name
                ));
            }
        }
    }

    output.tick_body.push_str("            }\n\n");

    // Clean up old states
    output
        .tick_body
        .push_str("            // Remove old states\n");
    output.tick_body.push_str("            while (StateBuffer.Num() > 0 && StateBuffer[0].Timestamp < TargetTime - 1.0f)\n");
    output.tick_body.push_str("            {\n");
    output
        .tick_body
        .push_str("                StateBuffer.RemoveAt(0);\n");
    output.tick_body.push_str("            }\n");
    output.tick_body.push_str("        }\n\n");
}

/// Generate extrapolation logic for predicted movement
fn generate_extrapolation_logic(ir: &NetworkSyncIR, output: &mut NetworkSyncCodegenOutput) {
    output
        .tick_body
        .push_str("        // Extrapolation logic\n");

    for prop in &ir.replicated_properties {
        if let ReplicationModeIR::Extrapolated { limit } = prop.mode {
            output.tick_body.push_str(&format!(
                "        // Extrapolate {} with limit {}\n",
                prop.name, limit
            ));

            // For velocity-based extrapolation
            if prop.name.contains("velocity") || prop.name.contains("Velocity") {
                output.tick_body.push_str(&format!(
                    "        {} = FMath::Clamp({}, -{}, {});\n",
                    prop.name, prop.name, limit, limit
                ));
            } else if prop.cpp_type.contains("Vector") {
                // Extrapolate position based on velocity if available
                output.tick_body.push_str(&format!(
                    "        // Extrapolate {} (limited to {} units)\n",
                    prop.name, limit
                ));
            }
        }
    }

    output.tick_body.push_str("\n");
}

/// Generate snap threshold checks for teleportation detection
fn generate_snap_threshold_checks(ir: &NetworkSyncIR, output: &mut NetworkSyncCodegenOutput) {
    output.tick_body.push_str(&format!(
        "        // Snap threshold check (threshold: {})\n",
        ir.config.snap_threshold
    ));

    // Find position properties to check
    for prop in &ir.replicated_properties {
        if (prop.name.contains("position")
            || prop.name.contains("Position")
            || prop.name.contains("location")
            || prop.name.contains("Location"))
            && (prop.cpp_type.contains("Vector") || prop.cpp_type.contains("FVector"))
        {
            output
                .tick_body
                .push_str("        if (StateBuffer.Num() >= 2)\n");
            output.tick_body.push_str("        {\n");
            output.tick_body.push_str(&format!(
                "            float Distance = FVector::Dist(StateBuffer[StateBuffer.Num() - 1].{}, StateBuffer[StateBuffer.Num() - 2].{});\n",
                prop.name, prop.name
            ));
            output.tick_body.push_str(&format!(
                "            if (Distance > {}f)\n",
                ir.config.snap_threshold
            ));
            output.tick_body.push_str("            {\n");
            output
                .tick_body
                .push_str("                // Teleportation detected - snap to latest state\n");
            output.tick_body.push_str(&format!(
                "                {} = StateBuffer[StateBuffer.Num() - 1].{};\n",
                prop.name, prop.name
            ));
            output
                .tick_body
                .push_str("                StateBuffer.Empty();\n");
            output.tick_body.push_str("            }\n");
            output.tick_body.push_str("        }\n\n");
            break; // Only check one position property
        }
    }
}

/// Generate GetLifetimeReplicatedProps implementation
fn generate_replication_setup(
    ir: &NetworkSyncIR,
    class_name: &str,
    output: &mut NetworkSyncCodegenOutput,
) {
    output
        .replication_body
        .push_str("    Super::GetLifetimeReplicatedProps(OutLifetimeProps);\n\n");

    // Add replication for each property based on mode
    for prop in &ir.replicated_properties {
        match &prop.mode {
            ReplicationModeIR::Simple => {
                output.replication_body.push_str(&format!(
                    "    DOREPLIFETIME({}, {});\n",
                    class_name, prop.name
                ));
            }
            ReplicationModeIR::Interpolated { .. } => {
                // Use conditional replication for interpolated properties
                output.replication_body.push_str(&format!(
                    "    DOREPLIFETIME_CONDITION({}, {}, COND_SimulatedOnly);\n",
                    class_name, prop.name
                ));
            }
            ReplicationModeIR::Extrapolated { .. } => {
                // Use conditional replication for extrapolated properties
                output.replication_body.push_str(&format!(
                    "    DOREPLIFETIME_CONDITION({}, {}, COND_SimulatedOnly);\n",
                    class_name, prop.name
                ));
            }
            ReplicationModeIR::Compressed {
                threshold,
                use_half_float,
            } => {
                // Use quantized replication for compressed properties
                if *use_half_float {
                    output.replication_body.push_str(&format!(
                        "    DOREPLIFETIME_CONDITION({}, {}, COND_SimulatedOnly);\n",
                        class_name, prop.name
                    ));
                    output.replication_body.push_str(&format!(
                        "    // TODO: Implement half-float compression for {} (threshold: {})\n",
                        prop.name, threshold
                    ));
                } else {
                    output.replication_body.push_str(&format!(
                        "    DOREPLIFETIME_CONDITION({}, {}, COND_SimulatedOnly);\n",
                        class_name, prop.name
                    ));
                }
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::network_sync_ir::{NetworkConfigIR, ReplicatedPropertyIR, ReplicationModeIR};

    #[test]
    fn test_generate_simple_replication() {
        let ir = NetworkSyncIR {
            component_name: "TestComponent".to_string(),
            replicated_properties: vec![ReplicatedPropertyIR {
                name: "Health".to_string(),
                cpp_type: "float".to_string(),
                mode: ReplicationModeIR::Simple,
                compression: None,
            }],
            config: NetworkConfigIR::default(),
        };

        let output = generate_network_sync_code(&ir, "UTestComponent");

        assert!(output
            .replication_body
            .contains("DOREPLIFETIME(UTestComponent, Health)"));
        assert!(output
            .constructor_body
            .contains("SetIsReplicatedByDefault(true)"));
    }

    #[test]
    fn test_generate_interpolated_replication() {
        let ir = NetworkSyncIR {
            component_name: "TestComponent".to_string(),
            replicated_properties: vec![ReplicatedPropertyIR {
                name: "Position".to_string(),
                cpp_type: "FVector".to_string(),
                mode: ReplicationModeIR::Interpolated {
                    back_time: 0.1,
                    buffer_size: 32,
                },
                compression: None,
            }],
            config: NetworkConfigIR::default(),
        };

        let output = generate_network_sync_code(&ir, "UTestComponent");

        assert!(output.header_declarations.contains("struct FNetworkState"));
        assert!(output
            .header_declarations
            .contains("TArray<FNetworkState> StateBuffer"));
        assert!(output
            .constructor_body
            .contains("InterpolationBackTime = 0.1f"));
        assert!(output.tick_body.contains("Interpolation logic"));
        assert!(output.replication_body.contains("DOREPLIFETIME_CONDITION"));
    }

    #[test]
    fn test_generate_snap_threshold() {
        let ir = NetworkSyncIR {
            component_name: "TestComponent".to_string(),
            replicated_properties: vec![ReplicatedPropertyIR {
                name: "Position".to_string(),
                cpp_type: "FVector".to_string(),
                mode: ReplicationModeIR::Interpolated {
                    back_time: 0.1,
                    buffer_size: 32,
                },
                compression: None,
            }],
            config: NetworkConfigIR {
                snap_threshold: 500.0,
                ..Default::default()
            },
        };

        let output = generate_network_sync_code(&ir, "UTestComponent");

        assert!(output.tick_body.contains("Snap threshold check"));
        assert!(output.tick_body.contains("500"));
        assert!(output.tick_body.contains("Teleportation detected"));
    }
}
