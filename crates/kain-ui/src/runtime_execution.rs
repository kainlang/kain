//! Runtime execution model for `kain-ui`.
//!
//! This module is intentionally explicit: it defines the runtime-owned
//! mutation + invalidation + transaction contract that backends can consume
//! without reverse engineering host behavior.

use std::collections::{BTreeMap, BTreeSet};

use crate::{
    ui_step_animation_runtime, UiAnimationFrame, UiCommand, UiCommandBuffer, UiCommandRejection,
    UiComputed, UiEventPhase, UiInvalidationResult, UiNodeId, UiPatch, UiSchedulerEntry,
    UiSchedulerPhase, UiSignalId, UiSignalUpdate, UiTransaction, UiTree, UiValue,
    UiWorkspaceLayout,
};

/// Runtime entrypoint that owns:
/// - retained semantic graph (`UiTree`)
/// - runtime-owned state/systems (`UiRuntimeSystems` via fields on `UiRuntimeBundle.output.systems`)
/// - exact invalidation and scheduler decisions
/// - transaction-aware mutation + patch authority
///
/// This is the object that backends should treat as "the UI runtime" rather
/// than scattering state across host-local widget instances.
#[derive(Clone, Debug)]
pub struct UiRuntime {
    pub tree: UiTree,
    pub systems: crate::UiRuntimeSystems,
    indexes: UiRuntimeIndexes,
    next_transaction_id: u64,
}

impl UiRuntime {
    pub fn new(tree: UiTree, systems: crate::UiRuntimeSystems) -> Self {
        let mut runtime = Self {
            tree,
            systems,
            indexes: UiRuntimeIndexes::default(),
            next_transaction_id: 1,
        };
        runtime.rebuild_indexes();
        runtime
    }

    pub fn rebuild_indexes(&mut self) {
        self.indexes = UiRuntimeIndexes::build(&self.tree, &self.systems.computed);
    }

    /// Executes a runtime step:
    /// - routes events into commands (runtime-owned)
    /// - executes queued commands (mutating retained graph + runtime systems)
    /// - applies signal updates with exact invalidation and scheduler decisions
    /// - advances animation playback state (does not mutate the semantic tree)
    pub fn step(&mut self, input: &UiRuntimeStepInput) -> UiRuntimeStepOutput {
        let mut output = UiRuntimeStepOutput::default();

        for event in &input.events {
            let routed = self.route_event_to_commands(event);
            for command in routed {
                output.system_patches.push(UiRuntimeSystemPatch::CommandQueued {
                    command: command.clone(),
                });
                self.systems.command_buffer.pending.push(command);
            }
        }

        // Execute pending commands (including commands queued from events).
        let mut command_exec = UiCommandExecutionOutput::default();
        for command in std::mem::take(&mut self.systems.command_buffer.pending) {
            let exec_one = self.execute_command(command.clone());
            command_exec.merge(exec_one);
            let command_name = command.name.clone();
            output.system_patches.push(UiRuntimeSystemPatch::CommandExecuted {
                command,
                applied: !command_exec
                    .rejections
                    .iter()
                    .any(|rej| rej.command_name == command_name),
            });
        }
        self.systems.command_buffer.executed.extend(command_exec.executed);
        self.systems.command_buffer.rejections.extend(command_exec.rejections);
        output.tree_patches.extend(command_exec.tree_patches);

        if !input.signal_updates.is_empty() {
            let invalidation = self.apply_signal_updates(&input.signal_updates, input.transaction_label.as_deref());
            output.invalidation = Some(invalidation.clone());
            output.system_patches.push(UiRuntimeSystemPatch::SignalsUpdated {
                changed_signals: invalidation.changed_signals.clone(),
                invalidated_nodes: invalidation.invalidated_nodes.clone(),
            });
        }

        if input.delta_ms > 0 {
            let frames = ui_step_animation_runtime(&mut self.systems, input.delta_ms);
            if !frames.is_empty() {
                output.animation_frames = frames.clone();
                output.system_patches.push(UiRuntimeSystemPatch::AnimationAdvanced {
                    frames,
                });
            }
        }

        // Coalesce scheduler entries to keep the contract bounded and explainable.
        output.scheduler = Some(ui_coalesce_scheduler_entries(&mut self.systems.scheduler.pending));

        output
    }

    pub fn apply_signal_updates(
        &mut self,
        updates: &[UiSignalUpdate],
        transaction_label: Option<&str>,
    ) -> UiInvalidationResult {
        let mut result = UiInvalidationResult::default();

        // Track invalidated nodes in a set to preserve "exact once" invalidation.
        let mut invalidated = BTreeSet::new();

        for update in updates {
            let changed = self.systems.signal_values.get(&update.signal) != Some(&update.value);
            if !changed {
                continue;
            }

            self.systems
                .signal_values
                .insert(update.signal, update.value.clone());
            result.changed_signals.push(update.signal);

            // Direct node watchers (tree-owned dependency declaration).
            if let Some(nodes) = self.indexes.signal_watchers.get(&update.signal) {
                for node in nodes {
                    if invalidated.insert(*node) {
                        result.invalidated_nodes.push(*node);
                    }
                }
            }

            // Computed dependents (systems-owned dependency declaration).
            if let Some(computed_indices) = self.indexes.signal_to_computed.get(&update.signal) {
                for idx in computed_indices {
                    let Some(computed) = self.systems.computed.get(*idx) else {
                        continue;
                    };
                    for node in &computed.invalidates_nodes {
                        if invalidated.insert(*node) {
                            result.invalidated_nodes.push(*node);
                        }
                    }

                    let entry = UiSchedulerEntry {
                        phase: computed.scheduler_phase,
                        label: computed.label.clone(),
                        target_nodes: computed.invalidates_nodes.clone(),
                    };
                    self.systems.scheduler.pending.push(entry.clone());
                    result.scheduled.push(entry);
                }
            }
        }

        if !result.changed_signals.is_empty() {
            let mut transaction = UiTransaction {
                label: transaction_label
                    .map(|label| label.to_string())
                    .unwrap_or_else(|| format!("signals:{}", result.changed_signals.len())),
                touched_nodes: result.invalidated_nodes.clone(),
                changed_signals: result.changed_signals.clone(),
                ..UiTransaction::default_runtime(self.next_transaction_id)
            };
            self.next_transaction_id = self.next_transaction_id.saturating_add(1);
            transaction.patch_count = 0;
            self.systems.transactions.push(transaction.clone());
            result.transaction = Some(transaction);
        }

        result
    }

    fn route_event_to_commands(&self, event: &UiRuntimeEvent) -> Vec<UiCommand> {
        let mut commands = Vec::new();

        for route in &self.systems.event_routes {
            if route.event != event.event {
                continue;
            }
            if route.target != event.target {
                continue;
            }
            if route.phase != event.phase {
                continue;
            }

            if let Some(command_name) = route.dispatch_command.as_deref() {
                commands.push(UiCommand {
                    name: command_name.to_string(),
                    target: Some(event.target),
                    payload: event.payload.clone(),
                });
            } else if let Some(handler_id) = route.handler_id.as_deref() {
                commands.push(UiCommand {
                    name: handler_id.to_string(),
                    target: Some(event.target),
                    payload: event.payload.clone(),
                });
            }
        }

        commands
    }

    fn execute_command(&mut self, command: UiCommand) -> UiCommandExecutionOutput {
        let mut output = UiCommandExecutionOutput::default();

        // Always record the transaction, even if the command is rejected; that
        // keeps "patch authority" inspectable.
        let mut tx = UiTransaction {
            label: format!("cmd:{}", command.name),
            ..UiTransaction::default_runtime(self.next_transaction_id)
        };
        self.next_transaction_id = self.next_transaction_id.saturating_add(1);

        let mut mutator = UiTreeMutator::new(&mut self.tree);
        let applied = ui_apply_builtin_command(&mut mutator, &mut self.systems, &command, &mut tx);
        let (patches, touched_nodes) = mutator.finish();
        tx.touched_nodes = touched_nodes;
        tx.patch_count = patches.len();

        if applied {
            output.executed.push(command);
            output.tree_patches.extend(patches);
        } else {
            output.rejections.push(UiCommandRejection {
                command_name: command.name.clone(),
                reason: "unsupported_command_or_invalid_payload".to_string(),
            });
        }

        self.systems.transactions.push(tx);
        output
    }
}

#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
pub struct UiRuntimeStepInput {
    /// Optional label used by the runtime when generating `UiTransaction` entries.
    pub transaction_label: Option<String>,
    pub delta_ms: u32,
    #[serde(default)]
    pub events: Vec<UiRuntimeEvent>,
    #[serde(default)]
    pub signal_updates: Vec<UiSignalUpdate>,
}

#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
pub struct UiRuntimeStepOutput {
    #[serde(default)]
    pub tree_patches: Vec<UiPatch>,
    #[serde(default)]
    pub system_patches: Vec<UiRuntimeSystemPatch>,
    pub invalidation: Option<UiInvalidationResult>,
    pub scheduler: Option<UiSchedulerCoalesceReport>,
    #[serde(default)]
    pub animation_frames: Vec<UiAnimationFrame>,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiRuntimeEvent {
    pub event: String,
    pub target: UiNodeId,
    pub phase: UiEventPhase,
    #[serde(default)]
    pub payload: BTreeMap<String, UiValue>,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub enum UiRuntimeSystemPatch {
    SignalsUpdated {
        changed_signals: Vec<UiSignalId>,
        invalidated_nodes: Vec<UiNodeId>,
    },
    CommandQueued {
        command: UiCommand,
    },
    CommandExecuted {
        command: UiCommand,
        applied: bool,
    },
    WorkspaceActiveTabChanged {
        group_id: String,
        active_layout_id: String,
    },
    SelectionChanged {
        scope: String,
        primary: Option<UiNodeId>,
    },
    FocusChanged {
        scope: String,
        focused: Option<UiNodeId>,
    },
    AnimationAdvanced {
        frames: Vec<UiAnimationFrame>,
    },
}

#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
pub struct UiCommandExecutionOutput {
    #[serde(default)]
    pub executed: Vec<UiCommand>,
    #[serde(default)]
    pub tree_patches: Vec<UiPatch>,
    #[serde(default)]
    pub rejections: Vec<UiCommandRejection>,
}

impl UiCommandExecutionOutput {
    fn merge(&mut self, other: UiCommandExecutionOutput) {
        self.executed.extend(other.executed);
        self.tree_patches.extend(other.tree_patches);
        self.rejections.extend(other.rejections);
    }
}

#[derive(Debug)]
struct UiTreeMutator<'a> {
    tree: &'a mut UiTree,
    patches: Vec<UiPatch>,
    touched_nodes: BTreeSet<UiNodeId>,
}

impl<'a> UiTreeMutator<'a> {
    fn new(tree: &'a mut UiTree) -> Self {
        Self {
            tree,
            patches: Vec::new(),
            touched_nodes: BTreeSet::new(),
        }
    }

    fn set_prop(&mut self, id: UiNodeId, key: String, value: UiValue) -> bool {
        let Some(node) = self.tree.node_mut(id) else {
            return false;
        };
        let changed = node.props.get(&key) != Some(&value);
        if !changed {
            return true;
        }
        node.props.insert(key.clone(), value.clone());
        self.patches.push(UiPatch::SetProp { id, key, value });
        self.touched_nodes.insert(id);
        true
    }

    fn set_layout(&mut self, id: UiNodeId, layout: crate::UiLayoutSpec) -> bool {
        let Some(node) = self.tree.node_mut(id) else {
            return false;
        };
        if node.layout == layout {
            return true;
        }
        node.layout = layout.clone();
        self.patches.push(UiPatch::SetLayout { id, layout });
        self.touched_nodes.insert(id);
        true
    }

    fn finish(self) -> (Vec<UiPatch>, Vec<UiNodeId>) {
        (self.patches, self.touched_nodes.into_iter().collect())
    }
}

#[derive(Clone, Debug, Default)]
struct UiRuntimeIndexes {
    signal_watchers: BTreeMap<UiSignalId, Vec<UiNodeId>>,
    signal_to_computed: BTreeMap<UiSignalId, Vec<usize>>,
}

impl UiRuntimeIndexes {
    fn build(tree: &UiTree, computed: &[UiComputed]) -> Self {
        let mut indexes = Self::default();

        for node in tree.nodes.values() {
            for signal in &node.watches {
                indexes
                    .signal_watchers
                    .entry(*signal)
                    .or_default()
                    .push(node.id);
            }
        }

        for (idx, computed) in computed.iter().enumerate() {
            for signal in &computed.depends_on {
                indexes.signal_to_computed.entry(*signal).or_default().push(idx);
            }
        }

        for watchers in indexes.signal_watchers.values_mut() {
            watchers.sort();
            watchers.dedup();
        }
        for deps in indexes.signal_to_computed.values_mut() {
            deps.sort();
            deps.dedup();
        }

        indexes
    }
}

#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
pub struct UiSchedulerCoalesceReport {
    pub original_count: usize,
    pub coalesced_count: usize,
    #[serde(default)]
    pub entries: Vec<UiSchedulerEntry>,
}

fn ui_coalesce_scheduler_entries(pending: &mut Vec<UiSchedulerEntry>) -> UiSchedulerCoalesceReport {
    let original_count = pending.len();
    if pending.is_empty() {
        return UiSchedulerCoalesceReport {
            original_count,
            coalesced_count: 0,
            entries: Vec::new(),
        };
    }

    // Stable coalesce key: phase + label + target nodes.
    let mut seen = BTreeSet::<String>::new();
    let mut coalesced = Vec::new();

    for entry in pending.drain(..) {
        let mut key = format!("{:?}|{}", entry.phase, entry.label);
        for node in &entry.target_nodes {
            key.push('|');
            key.push_str(&node.0.to_string());
        }
        if seen.insert(key) {
            coalesced.push(entry);
        }
    }

    coalesced.sort_by_key(|entry| (phase_sort_key(entry.phase), entry.label.clone()));

    let coalesced_count = coalesced.len();
    UiSchedulerCoalesceReport {
        original_count,
        coalesced_count,
        entries: coalesced,
    }
}

fn phase_sort_key(phase: UiSchedulerPhase) -> u32 {
    match phase {
        UiSchedulerPhase::Signals => 0,
        UiSchedulerPhase::Resources => 1,
        UiSchedulerPhase::Layout => 2,
        UiSchedulerPhase::Animation => 3,
        UiSchedulerPhase::Patches => 4,
        UiSchedulerPhase::Effects => 5,
    }
}

fn ui_apply_builtin_command(
    mutator: &mut UiTreeMutator<'_>,
    systems: &mut crate::UiRuntimeSystems,
    command: &UiCommand,
    transaction: &mut UiTransaction,
) -> bool {
    match command.name.as_str() {
        // Workspace/tab intent is runtime-owned and backend-neutral. It is not a host-only "active tab" flag.
        "ui.tab.activate" => {
            let group_id = match command.payload.get("group_id") {
                Some(UiValue::String(value)) => value.clone(),
                _ => return false,
            };
            let layout_id = match command.payload.get("layout_id") {
                Some(UiValue::String(value)) => value.clone(),
                _ => return false,
            };

            if systems.workspace_layout.active_tabs.get(&group_id) == Some(&layout_id) {
                return true;
            }

            systems.workspace_layout.active_tabs.insert(group_id, layout_id);
            transaction.touched_nodes.clear();
            true
        }
        "ui.node.set_prop" => {
            let node_id = match command.payload.get("node_id") {
                Some(UiValue::Int(value)) if *value >= 0 => UiNodeId(*value as u64),
                Some(UiValue::String(value)) => match value.parse::<u64>() {
                    Ok(parsed) => UiNodeId(parsed),
                    Err(_) => return false,
                },
                _ => return false,
            };
            let key = match command.payload.get("key") {
                Some(UiValue::String(value)) => value.clone(),
                _ => return false,
            };
            let value = match command.payload.get("value") {
                Some(value) => value.clone(),
                None => return false,
            };
            mutator.set_prop(node_id, key, value)
        }
        "ui.node.set_dock" => {
            let node_id = match command.payload.get("node_id") {
                Some(UiValue::Int(value)) if *value >= 0 => UiNodeId(*value as u64),
                Some(UiValue::String(value)) => match value.parse::<u64>() {
                    Ok(parsed) => UiNodeId(parsed),
                    Err(_) => return false,
                },
                _ => return false,
            };
            let placement = match command.payload.get("placement") {
                Some(UiValue::String(value)) => value.as_str(),
                _ => return false,
            };

            let mut layout = match mutator.tree.node(node_id) {
                Some(node) => node.layout.clone(),
                None => return false,
            };
            layout.dock = Some(match placement {
                "left" => crate::UiDockPlacement::Left,
                "right" => crate::UiDockPlacement::Right,
                "top" => crate::UiDockPlacement::Top,
                "bottom" => crate::UiDockPlacement::Bottom,
                "center" => crate::UiDockPlacement::Center,
                "tab" => crate::UiDockPlacement::Tab,
                _ => return false,
            });
            if let Some(UiValue::Float(ratio)) = command.payload.get("split_ratio") {
                layout.split_ratio = Some(*ratio as f32);
            }
            mutator.set_layout(node_id, layout)
        }
        "ui.focus.set" => {
            let scope = match command.payload.get("scope") {
                Some(UiValue::String(value)) => value.clone(),
                _ => return false,
            };
            let node_id = match command.payload.get("node_id") {
                Some(UiValue::Int(value)) if *value >= 0 => Some(UiNodeId(*value as u64)),
                Some(UiValue::String(value)) => value.parse::<u64>().ok().map(UiNodeId),
                _ => None,
            };
            if let Some(node_id) = node_id {
                systems.focus_graph.focused.insert(scope, node_id);
            } else {
                systems.focus_graph.focused.remove(&scope);
            }
            true
        }
        "ui.selection.set_primary" => {
            let scope = match command.payload.get("scope") {
                Some(UiValue::String(value)) => value.clone(),
                _ => return false,
            };
            let node_id = match command.payload.get("node_id") {
                Some(UiValue::Int(value)) if *value >= 0 => Some(UiNodeId(*value as u64)),
                Some(UiValue::String(value)) => value.parse::<u64>().ok().map(UiNodeId),
                _ => None,
            };
            if let Some(node_id) = node_id {
                systems.selection_model.primary.insert(scope.clone(), node_id);
                systems
                    .selection_model
                    .selected
                    .entry(scope)
                    .or_default()
                    .insert(node_id);
            } else {
                systems.selection_model.primary.remove(&scope);
            }
            true
        }
        "ui.resource.set_state" => {
            let resource_id = match command.payload.get("id") {
                Some(UiValue::String(value)) => value.clone(),
                _ => return false,
            };
            let state = match command.payload.get("state") {
                Some(UiValue::String(value)) => value.as_str(),
                _ => return false,
            };
            let state = match state {
                "idle" => crate::UiResourceState::Idle,
                "loading" => crate::UiResourceState::Loading,
                "ready" => crate::UiResourceState::Ready,
                "failed" => crate::UiResourceState::Failed,
                _ => return false,
            };
            if let Some(resource) = systems.resources.iter_mut().find(|res| res.id == resource_id) {
                resource.state = state;
            } else {
                systems.resources.push(crate::UiResource {
                    id: resource_id,
                    kind: "unknown".to_string(),
                    owner: None,
                    state,
                });
            }
            true
        }
        _ => false,
    }
}

impl UiCommandBuffer {
    fn has_rejection_for_name(&self, command_name: &str) -> bool {
        self.rejections
            .iter()
            .any(|rej| rej.command_name == command_name)
    }
}

impl UiWorkspaceLayout {
    pub fn active_tab(&self, group_id: &str) -> Option<&str> {
        self.active_tabs.get(group_id).map(|s| s.as_str())
    }
}

// Note: We intentionally keep these serde types local to `kain-ui` so the
// runtime contract is explicit and backend-neutral. Any native-only "projection"
// formats should stay in adapter crates.

use serde::{Deserialize, Serialize};
