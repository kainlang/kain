use crate::graph::{AccessKind, OrchestrateGraphPlan, OrchestrateStageKind, ResourceStage};
use serde::{Deserialize, Serialize};

// ---------------------------------------------------------------------------
// Planner Policy
// ---------------------------------------------------------------------------

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum OrchestratePlannerPolicy {
    Static,
    TelemetryPreferGpu,
    TelemetryPreferCpu,
    TelemetryBalanceLatency,
    /// Route this stage to an async compute queue when available.
    PreferAsyncCompute,
}

impl Default for OrchestratePlannerPolicy {
    fn default() -> Self {
        Self::Static
    }
}

impl OrchestratePlannerPolicy {
    pub fn from_name(name: &str) -> Option<Self> {
        match name {
            "static" => Some(Self::Static),
            "telemetry_prefer_gpu" => Some(Self::TelemetryPreferGpu),
            "telemetry_prefer_cpu" => Some(Self::TelemetryPreferCpu),
            "telemetry_balance_latency" => Some(Self::TelemetryBalanceLatency),
            "prefer_async_compute" => Some(Self::PreferAsyncCompute),
            _ => None,
        }
    }

    pub fn as_str(self) -> &'static str {
        match self {
            Self::Static => "static",
            Self::TelemetryPreferGpu => "telemetry_prefer_gpu",
            Self::TelemetryPreferCpu => "telemetry_prefer_cpu",
            Self::TelemetryBalanceLatency => "telemetry_balance_latency",
            Self::PreferAsyncCompute => "prefer_async_compute",
        }
    }

    /// Returns true for all policies except `Static`.
    pub fn adaptive(self) -> bool {
        !matches!(self, Self::Static)
    }
}

// ---------------------------------------------------------------------------
// Barrier Inference — precise GPU pipeline barriers from the orchestrate DAG
// ---------------------------------------------------------------------------

/// A single memory barrier between two GPU pipeline stages.
///
/// JSON schema consumed by the C runtime / GPU executor (Stream ECHO):
/// ```json
/// {
///   "from_stage": "compute_pass",
///   "to_stage": "gfx_pass",
///   "src_stage_mask": 2048,
///   "dst_stage_mask": 128,
///   "src_access_mask": 64,
///   "dst_access_mask": 32
/// }
/// ```
/// The bitmask fields map directly to Vulkan `VkPipelineStageFlagBits` and
/// `VkAccessFlagBits` values as documented in the Vulkan 1.3 specification.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BarrierSpec {
    /// Name of the stage that produces the data (the dependency).
    pub from_stage: String,
    /// Name of the stage that consumes the data.
    pub to_stage: String,
    /// VkPipelineStageFlags for the source stage.
    pub src_stage_mask: u32,
    /// VkPipelineStageFlags for the destination stage.
    pub dst_stage_mask: u32,
    /// VkAccessFlags for the source access.
    pub src_access_mask: u32,
    /// VkAccessFlags for the destination access.
    pub dst_access_mask: u32,
}

/// Map a Kain `ResourceStage` to the corresponding Vulkan `VkPipelineStageFlagBits`.
///
/// Reference: Vulkan 1.3 Specification, Table 2. "Pipeline stages and access types".
fn shader_stage_to_pipeline_stage(stage: ResourceStage) -> u32 {
    match stage {
        ResourceStage::Vertex => 0x00000001,   // VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
        ResourceStage::Fragment => 0x00000080, // VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
        ResourceStage::Compute => 0x00000800,  // VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
        ResourceStage::Surface => 0x00000080,  // treat as fragment
    }
}

/// Map an `AccessKind` to the corresponding Vulkan `VkAccessFlagBits`.
///
/// Reference: Vulkan 1.3 Specification.
fn access_kind_to_access_flags(kind: AccessKind) -> u32 {
    match kind {
        AccessKind::Read => 0x00000020,     // VK_ACCESS_SHADER_READ_BIT
        AccessKind::Write => 0x00000040,    // VK_ACCESS_SHADER_WRITE_BIT
        AccessKind::ReadWrite => 0x00000060, // VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
    }
}

/// Infer precise GPU pipeline barriers from the orchestrate stage graph.
///
/// Algorithm:
/// 1. For each edge (from → to) in the dependency graph where both stages are GPU:
///    - Collect `ResourceAccess` for `from` stage.
///    - Collect `ResourceAccess` for `to` stage.
///    - For any resource R that `from` WRITES and `to` READS:
///        src_stage = shader stage of `from`
///        dst_stage = shader stage of `to`
///        src_access = write access of `from` on R
///        dst_access = read access of `to` on R
/// 2. If stages are parallel (no dependency edge): no barrier needed.
/// 3. Merge barriers for the same (from, to) pair into one barrier.
/// 4. If one stage is on an async compute queue and the other is on a graphics
///    queue, use `VK_PIPELINE_STAGE_ALL_COMMANDS_BIT` (0x00010000) for both masks.
pub fn infer_barrier_metadata(plan: &OrchestrateGraphPlan) -> Vec<BarrierSpec> {
    let access = plan.collect_access_map();
    let mut barriers = Vec::new();

    for stage in &plan.stages {
        // Skip non-GPU stages
        if !matches!(
            stage.kind,
            OrchestrateStageKind::Gpu | OrchestrateStageKind::Dispatch
        ) {
            continue;
        }

        for dep_name in &stage.metadata.dependencies {
            // Find the dependency stage
            let dep_stage = plan.stages.iter().find(|s| s.binding_name == *dep_name);
            let dep_stage = match dep_stage {
                Some(s) => s,
                None => continue,
            };

            // Skip non-GPU dependency stages
            if !matches!(
                dep_stage.kind,
                OrchestrateStageKind::Gpu | OrchestrateStageKind::Dispatch
            ) {
                continue;
            }

            let dep_accesses = access.get(dep_name);
            let stage_accesses = access.get(&stage.binding_name);

            if let (Some(dep_acc), Some(stage_acc)) = (dep_accesses, stage_accesses) {
                let mut src_stage_mask: u32 = 0;
                let mut dst_stage_mask: u32 = 0;
                let mut src_access_mask: u32 = 0;
                let mut dst_access_mask: u32 = 0;

                // Check if one stage uses async compute and the other does not.
                // When stages straddle different queue families, we need a full
                // pipeline drain: VK_PIPELINE_STAGE_ALL_COMMANDS_BIT = 0x00010000.
                let dep_policy = dep_stage.metadata.policy.unwrap_or_default();
                let stage_policy = stage.metadata.policy.unwrap_or_default();
                let cross_queue = (dep_policy == OrchestratePlannerPolicy::PreferAsyncCompute)
                    != (stage_policy == OrchestratePlannerPolicy::PreferAsyncCompute);

                if cross_queue {
                    // All-commands barrier across queue families.
                    barriers.push(BarrierSpec {
                        from_stage: dep_name.clone(),
                        to_stage: stage.binding_name.clone(),
                        src_stage_mask: 0x00010000, // VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
                        dst_stage_mask: 0x00010000, // VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
                        src_access_mask: 0x00000060, // SHADER_READ | SHADER_WRITE
                        dst_access_mask: 0x00000060,
                    });
                    continue;
                }

                // Compute per-resource barriers for write→read dependencies.
                for r_dep in dep_acc {
                    if !r_dep.access_kind.writes() {
                        continue; // only write→read matters for barrier placement
                    }
                    for r_stage in stage_acc {
                        if r_stage.binding_name == r_dep.binding_name
                            && r_stage.access_kind.reads()
                        {
                            src_stage_mask |=
                                shader_stage_to_pipeline_stage(r_dep.shader_stage);
                            dst_stage_mask |=
                                shader_stage_to_pipeline_stage(r_stage.shader_stage);
                            src_access_mask |= access_kind_to_access_flags(r_dep.access_kind);
                            dst_access_mask |=
                                access_kind_to_access_flags(r_stage.access_kind);
                        }
                    }
                }

                if src_stage_mask != 0 {
                    barriers.push(BarrierSpec {
                        from_stage: dep_name.clone(),
                        to_stage: stage.binding_name.clone(),
                        src_stage_mask,
                        dst_stage_mask,
                        src_access_mask,
                        dst_access_mask,
                    });
                }
            }
        }
    }

    barriers
}

// ---------------------------------------------------------------------------
// Push Constant Size Classification
// ---------------------------------------------------------------------------

/// Determine whether a set of shader uniforms should be lowered to
/// Vulkan push constants (`StorageClass::PushConstant`) instead of
/// descriptor-uniform buffers.
///
/// Conditions:
/// 1. Total uniform size ≤ 128 bytes (Vulkan `maxPushConstantsSize` minimum).
/// 2. All uniforms are accessed by a single shader stage only.
///
/// Returns `Some(total_bytes)` if eligible, `None` if descriptor binding
/// should be used instead.
pub fn infer_push_constant_eligibility(
    uniforms: &[(String, u32)], // (type_name, type_size_bytes)
    stages: &[ResourceStage],
) -> Option<u32> {
    let total_size: u32 = uniforms.iter().map(|(_, size)| *size).sum();

    // Vulkan minimum guaranteed maxPushConstantsSize = 128 bytes.
    const MAX_PUSH_CONSTANT_SIZE: u32 = 128;

    if total_size > MAX_PUSH_CONSTANT_SIZE {
        return None;
    }

    // All uniforms must be accessed by a single stage.
    // Push constants are per-stage; they cannot be shared across stages.
    if stages.len() != 1 {
        return None;
    }

    Some(total_size)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::graph::OrchestrateStageGraphMetadata;
    use crate::stage::{OrchestrateStageKind, OrchestrateStagePlan};

    #[test]
    fn test_infer_barrier_write_to_read() {
        let mut plan = OrchestrateGraphPlan::new("test_pipeline");

        // Stage A: compute shader writes to "output_buf"
        let mut meta_a = OrchestrateStageGraphMetadata::default();
        meta_a.access_map.insert(
            "output_buf".to_string(),
            vec![ResourceAccess {
                binding_name: "output_buf".to_string(),
                shader_stage: ResourceStage::Compute,
                access_kind: AccessKind::Write,
            }],
        );
        let stage_a = OrchestrateStagePlan {
            binding_name: "stage_a".to_string(),
            kind: OrchestrateStageKind::Gpu,
            function: "compute_a".to_string(),
            selector: None,
            metadata: meta_a,
        };

        // Stage B: compute shader reads from "output_buf", depends on A
        let mut meta_b = OrchestrateStageGraphMetadata::default();
        meta_b.dependencies = vec!["stage_a".to_string()];
        meta_b.access_map.insert(
            "output_buf".to_string(),
            vec![ResourceAccess {
                binding_name: "output_buf".to_string(),
                shader_stage: ResourceStage::Compute,
                access_kind: AccessKind::Read,
            }],
        );
        let stage_b = OrchestrateStagePlan {
            binding_name: "stage_b".to_string(),
            kind: OrchestrateStageKind::Gpu,
            function: "compute_b".to_string(),
            selector: None,
            metadata: meta_b,
        };

        plan.push_stage(stage_a);
        plan.push_stage(stage_b);

        let barriers = infer_barrier_metadata(&plan);
        assert_eq!(barriers.len(), 1);
        assert_eq!(barriers[0].from_stage, "stage_a");
        assert_eq!(barriers[0].to_stage, "stage_b");
        assert_eq!(
            barriers[0].src_stage_mask,
            0x00000800 // VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
        );
        assert_eq!(
            barriers[0].dst_stage_mask,
            0x00000800 // VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
        );
        assert_eq!(
            barriers[0].src_access_mask,
            0x00000040 // VK_ACCESS_SHADER_WRITE_BIT
        );
        assert_eq!(
            barriers[0].dst_access_mask,
            0x00000020 // VK_ACCESS_SHADER_READ_BIT
        );
    }

    #[test]
    fn test_infer_barrier_no_shared_resources() {
        let mut plan = OrchestrateGraphPlan::new("test_pipeline");

        let mut meta_a = OrchestrateStageGraphMetadata::default();
        meta_a.access_map.insert(
            "buf_a".to_string(),
            vec![ResourceAccess {
                binding_name: "buf_a".to_string(),
                shader_stage: ResourceStage::Compute,
                access_kind: AccessKind::Write,
            }],
        );
        let stage_a = OrchestrateStagePlan {
            binding_name: "stage_a".to_string(),
            kind: OrchestrateStageKind::Gpu,
            function: "compute_a".to_string(),
            selector: None,
            metadata: meta_a,
        };

        let mut meta_b = OrchestrateStageGraphMetadata::default();
        meta_b.dependencies = vec!["stage_a".to_string()];
        meta_b.access_map.insert(
            "buf_b".to_string(),
            vec![ResourceAccess {
                binding_name: "buf_b".to_string(),
                shader_stage: ResourceStage::Compute,
                access_kind: AccessKind::Read,
            }],
        );
        let stage_b = OrchestrateStagePlan {
            binding_name: "stage_b".to_string(),
            kind: OrchestrateStageKind::Gpu,
            function: "compute_b".to_string(),
            selector: None,
            metadata: meta_b,
        };

        plan.push_stage(stage_a);
        plan.push_stage(stage_b);

        let barriers = infer_barrier_metadata(&plan);
        // No shared resources → no barrier needed
        assert!(barriers.is_empty());
    }

    #[test]
    fn test_infer_barrier_parallel_no_dependency() {
        let mut plan = OrchestrateGraphPlan::new("test_pipeline");

        let mut meta_a = OrchestrateStageGraphMetadata::default();
        meta_a.access_map.insert(
            "shared_buf".to_string(),
            vec![ResourceAccess {
                binding_name: "shared_buf".to_string(),
                shader_stage: ResourceStage::Compute,
                access_kind: AccessKind::Write,
            }],
        );
        let stage_a = OrchestrateStagePlan {
            binding_name: "stage_a".to_string(),
            kind: OrchestrateStageKind::Gpu,
            function: "compute_a".to_string(),
            selector: None,
            metadata: meta_a,
        };

        let mut meta_b = OrchestrateStageGraphMetadata::default();
        // No dependency on stage_a
        meta_b.access_map.insert(
            "shared_buf".to_string(),
            vec![ResourceAccess {
                binding_name: "shared_buf".to_string(),
                shader_stage: ResourceStage::Compute,
                access_kind: AccessKind::Read,
            }],
        );
        let stage_b = OrchestrateStagePlan {
            binding_name: "stage_b".to_string(),
            kind: OrchestrateStageKind::Gpu,
            function: "compute_b".to_string(),
            selector: None,
            metadata: meta_b,
        };

        plan.push_stage(stage_a);
        plan.push_stage(stage_b);

        let barriers = infer_barrier_metadata(&plan);
        // Parallel stages → no barrier
        assert!(barriers.is_empty());
    }

    #[test]
    fn test_infer_barrier_cross_queue() {
        let mut plan = OrchestrateGraphPlan::new("test_pipeline");

        let mut meta_a = OrchestrateStageGraphMetadata::default();
        meta_a.policy = Some(OrchestratePlannerPolicy::PreferAsyncCompute);
        meta_a.access_map.insert(
            "shared_buf".to_string(),
            vec![ResourceAccess {
                binding_name: "shared_buf".to_string(),
                shader_stage: ResourceStage::Compute,
                access_kind: AccessKind::Write,
            }],
        );
        let stage_a = OrchestrateStagePlan {
            binding_name: "async_stage".to_string(),
            kind: OrchestrateStageKind::Gpu,
            function: "async_compute".to_string(),
            selector: None,
            metadata: meta_a,
        };

        let mut meta_b = OrchestrateStageGraphMetadata::default();
        meta_b.dependencies = vec!["async_stage".to_string()];
        meta_b.policy = Some(OrchestratePlannerPolicy::Static); // graphics queue
        meta_b.access_map.insert(
            "shared_buf".to_string(),
            vec![ResourceAccess {
                binding_name: "shared_buf".to_string(),
                shader_stage: ResourceStage::Vertex,
                access_kind: AccessKind::Read,
            }],
        );
        let stage_b = OrchestrateStagePlan {
            binding_name: "gfx_stage".to_string(),
            kind: OrchestrateStageKind::Gpu,
            function: "gfx_render".to_string(),
            selector: None,
            metadata: meta_b,
        };

        plan.push_stage(stage_a);
        plan.push_stage(stage_b);

        let barriers = infer_barrier_metadata(&plan);
        assert_eq!(barriers.len(), 1);
        // Cross-queue → ALL_COMMANDS barrier
        assert_eq!(barriers[0].src_stage_mask, 0x00010000);
        assert_eq!(barriers[0].dst_stage_mask, 0x00010000);
    }

    #[test]
    fn test_barrier_metadata_json_single_stage_returns_none() {
        let mut plan = OrchestrateGraphPlan::new("single_stage");
        let mut meta = OrchestrateStageGraphMetadata::default();
        meta.access_map.insert(
            "buf".to_string(),
            vec![ResourceAccess {
                binding_name: "buf".to_string(),
                shader_stage: ResourceStage::Compute,
                access_kind: AccessKind::ReadWrite,
            }],
        );
        let stage = OrchestrateStagePlan {
            binding_name: "only_stage".to_string(),
            kind: OrchestrateStageKind::Gpu,
            function: "single_compute".to_string(),
            selector: None,
            metadata: meta,
        };
        plan.push_stage(stage);

        assert!(plan.barrier_metadata_json().is_none());
    }

    #[test]
    fn test_infer_push_constant_eligible() {
        let uniforms = vec![("Vec4".to_string(), 16u32)];
        let stages = vec![ResourceStage::Compute];
        assert_eq!(
            infer_push_constant_eligibility(&uniforms, &stages),
            Some(16)
        );
    }

    #[test]
    fn test_infer_push_constant_too_large() {
        // 9 × Vec4 = 144 bytes > 128
        let uniforms: Vec<(String, u32)> = (0..9)
            .map(|i| (format!("u{}", i), 16u32))
            .collect();
        let stages = vec![ResourceStage::Compute];
        assert_eq!(infer_push_constant_eligibility(&uniforms, &stages), None);
    }

    #[test]
    fn test_infer_push_constant_multi_stage() {
        let uniforms = vec![("Vec4".to_string(), 16u32)];
        let stages = vec![ResourceStage::Compute, ResourceStage::Vertex];
        assert_eq!(infer_push_constant_eligibility(&uniforms, &stages), None);
    }

    #[test]
    fn test_policy_prefer_async_compute_roundtrip() {
        let policy = OrchestratePlannerPolicy::PreferAsyncCompute;
        assert_eq!(policy.as_str(), "prefer_async_compute");
        assert_eq!(
            OrchestratePlannerPolicy::from_name("prefer_async_compute"),
            Some(policy)
        );
        assert!(policy.adaptive());
    }
}
