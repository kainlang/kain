//! Runtime execution model for `kain-ui`.
//!
//! This module is intentionally explicit: it defines the runtime-owned
//! mutation + invalidation + transaction contract that backends can consume
//! without reverse engineering host behavior.

use std::collections::{BTreeMap, BTreeSet};

use serde::{Deserialize, Serialize};

use crate::{
    ui_solve_workspace_layout, ui_step_animation_runtime, ui_transfer_hot_reload_state,
    UiAnimationFrame, UiBuildOutput, UiCommand, UiCommandRejection, UiComputed, UiDerivedExpr,
    UiEventPhase, UiHotReloadTransferReport, UiInvalidationResult, UiLayoutKind, UiNodeId,
    UiOverflowBehavior, UiPatch, UiRect, UiResolvedLayout, UiSchedulerEntry, UiSchedulerPhase,
    UiSignalId, UiSignalUpdate, UiTransaction, UiTree, UiValue, UiWorkspaceLayout,
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

    /// Applies a newly compiled output using the explicit hot-reload transfer contract.
    ///
    /// This keeps state preservation, invalidation, and reconciliation visible in runtime-owned
    /// data rather than collapsing into a backend-local "just swap the tree" shortcut.
    pub fn reload(&mut self, mut next: UiBuildOutput) -> UiRuntimeReloadOutput {
        let previous_output = UiBuildOutput {
            tree: self.tree.clone(),
            patches: Vec::new(),
            systems: self.systems.clone(),
        };
        let pre_active_tabs = self.systems.workspace_layout.active_tabs.clone();
        let pre_focus = self.systems.focus_graph.focused.clone();
        let pre_selection_primary = self.systems.selection_model.primary.clone();

        let report = ui_transfer_hot_reload_state(&previous_output, &mut next);

        self.tree = next.tree;
        self.systems = next.systems;
        self.rebuild_indexes();

        let mut output = UiRuntimeReloadOutput {
            report: report.clone(),
            ..UiRuntimeReloadOutput::default()
        };
        output
            .system_patches
            .push(UiRuntimeSystemPatch::HotReloadApplied {
                report: report.clone(),
            });
        output.system_patches.extend(runtime_state_delta_patches(
            &pre_active_tabs,
            &pre_focus,
            &pre_selection_primary,
            &self.systems.workspace_layout.active_tabs,
            &self.systems.focus_graph.focused,
            &self.systems.selection_model.primary,
        ));

        if !report.invalidated_nodes.is_empty() {
            let scheduler_entry = UiSchedulerEntry {
                phase: UiSchedulerPhase::Layout,
                label: "hot_reload".to_string(),
                target_nodes: report.invalidated_nodes.clone(),
            };
            self.systems.scheduler.pending.push(scheduler_entry.clone());

            let transaction = UiTransaction {
                label: "hot_reload".to_string(),
                touched_nodes: report.invalidated_nodes.clone(),
                ..UiTransaction::default_runtime(self.next_transaction_id)
            };
            self.next_transaction_id = self.next_transaction_id.saturating_add(1);
            self.systems.transactions.push(transaction.clone());

            let invalidation = UiInvalidationResult {
                changed_signals: Vec::new(),
                invalidated_nodes: report.invalidated_nodes.clone(),
                scheduled: vec![scheduler_entry],
                transaction: Some(transaction),
            };
            output.invalidation = Some(invalidation);
            output
                .system_patches
                .push(UiRuntimeSystemPatch::HotReloadInvalidated {
                    invalidated_nodes: report.invalidated_nodes.clone(),
                });
        }

        output.scheduler = Some(ui_coalesce_scheduler_entries(
            &mut self.systems.scheduler.pending,
        ));
        output
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
                output
                    .system_patches
                    .push(UiRuntimeSystemPatch::CommandQueued {
                        command: command.clone(),
                    });
                self.systems.command_buffer.pending.push(command);
            }
        }

        // Execute pending commands (including commands queued from events).
        let mut command_exec = UiCommandExecutionOutput::default();
        for command in std::mem::take(&mut self.systems.command_buffer.pending) {
            let exec_one = self.execute_command(command.clone());
            let applied = exec_one.rejections.is_empty();
            command_exec.merge(exec_one);
            output
                .system_patches
                .push(UiRuntimeSystemPatch::CommandExecuted { command, applied });
        }
        self.systems
            .command_buffer
            .executed
            .extend(command_exec.executed);
        self.systems
            .command_buffer
            .rejections
            .extend(command_exec.rejections);
        output.tree_patches.extend(command_exec.tree_patches);
        output.system_patches.extend(command_exec.system_patches);

        if !input.signal_updates.is_empty() {
            let invalidation = self
                .apply_signal_updates(&input.signal_updates, input.transaction_label.as_deref());
            output.invalidation = Some(invalidation.clone());
            output
                .system_patches
                .push(UiRuntimeSystemPatch::SignalsUpdated {
                    changed_signals: invalidation.changed_signals.clone(),
                    invalidated_nodes: invalidation.invalidated_nodes.clone(),
                });
        }

        if input.delta_ms > 0 {
            self.systems.motion_policy.recompute_capacitor();
            if self.systems.motion_policy.should_animate() {
                let frames = ui_step_animation_runtime(&mut self.systems, input.delta_ms);
                if !frames.is_empty() {
                    output.animation_frames = frames.clone();
                    output
                        .system_patches
                        .push(UiRuntimeSystemPatch::AnimationAdvanced { frames });
                }
            }
        }

        // Coalesce scheduler entries to keep the contract bounded and explainable.
        output.scheduler = Some(ui_coalesce_scheduler_entries(
            &mut self.systems.scheduler.pending,
        ));

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
        let mut changed = BTreeSet::<UiSignalId>::new();
        let mut worklist = Vec::<UiSignalId>::new();
        let mut scheduled = BTreeSet::<String>::new();

        // Seed worklist with direct updates.
        for update in updates {
            let changed_value =
                self.systems.signal_values.get(&update.signal) != Some(&update.value);
            if !changed_value {
                continue;
            }
            self.systems
                .signal_values
                .insert(update.signal, update.value.clone());
            if changed.insert(update.signal) {
                result.changed_signals.push(update.signal);
                worklist.push(update.signal);
            }
        }

        // Propagate invalidation and derived recompute until stable.
        //
        // This is the runtime-side "derived values" contract: derived signals are explicit
        // (`UiComputed { writes_signal, expr }`) and are recomputed here, not in the backend.
        let mut steps = 0usize;
        while let Some(signal) = worklist.pop() {
            steps += 1;
            if steps > 10_000 {
                // Defensive bound against cycles. Cycles are an authoring/compiler error, but
                // the runtime must stay bounded.
                break;
            }

            // Direct node watchers (tree-owned dependency declaration).
            if let Some(nodes) = self.indexes.signal_watchers.get(&signal) {
                for node in nodes {
                    if invalidated.insert(*node) {
                        result.invalidated_nodes.push(*node);
                    }
                }
            }

            // Computed dependents (systems-owned dependency declaration).
            if let Some(computed_indices) = self.indexes.signal_to_computed.get(&signal) {
                for idx in computed_indices {
                    let Some(computed) = self.systems.computed.get(*idx).cloned() else {
                        continue;
                    };

                    // Derived-signal recompute path.
                    if let (Some(out_signal), Some(expr)) =
                        (computed.writes_signal, computed.expr.clone())
                    {
                        let next_value = ui_eval_derived_expr(&expr, &self.systems.signal_values);
                        let changed_value =
                            self.systems.signal_values.get(&out_signal) != Some(&next_value);
                        if changed_value {
                            self.systems.signal_values.insert(out_signal, next_value);
                            if changed.insert(out_signal) {
                                result.changed_signals.push(out_signal);
                                worklist.push(out_signal);
                            }

                            // Only invalidate/schedule when the derived output actually changed.
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
                            let key = format!("computed:{}:{:?}", computed.id, entry.phase);
                            if scheduled.insert(key) {
                                self.systems.scheduler.pending.push(entry.clone());
                                result.scheduled.push(entry);
                            }
                        }
                        continue;
                    }

                    // Plain computed invalidation path.
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
                    let key = format!("computed:{}:{:?}", computed.id, entry.phase);
                    if scheduled.insert(key) {
                        self.systems.scheduler.pending.push(entry.clone());
                        result.scheduled.push(entry);
                    }
                }
            }
        }

        if !result.changed_signals.is_empty() {
            let transaction = UiTransaction {
                label: transaction_label
                    .map(|label| label.to_string())
                    .unwrap_or_else(|| format!("signals:{}", result.changed_signals.len())),
                touched_nodes: result.invalidated_nodes.clone(),
                changed_signals: result.changed_signals.clone(),
                ..UiTransaction::default_runtime(self.next_transaction_id)
            };
            self.next_transaction_id = self.next_transaction_id.saturating_add(1);
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
                    transaction_label: route.transaction_label.clone(),
                });
            } else if let Some(handler_id) = route.handler_id.as_deref() {
                commands.push(UiCommand {
                    name: handler_id.to_string(),
                    target: Some(event.target),
                    payload: event.payload.clone(),
                    transaction_label: route.transaction_label.clone(),
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
            label: command
                .transaction_label
                .clone()
                .unwrap_or_else(|| format!("cmd:{}", command.name)),
            ..UiTransaction::default_runtime(self.next_transaction_id)
        };
        self.next_transaction_id = self.next_transaction_id.saturating_add(1);

        // If the command is a registered external effect, dispatch it without
        // inventing backend-local semantics in the runtime.
        if let Some(desc) = self
            .systems
            .command_registry
            .snapshot
            .iter()
            .find(|desc| desc.name == command.name)
        {
            if desc.effect == crate::UiCommandEffectKind::ExternalEffect {
                tx.dispatched_commands.push(command.name.clone());
                output.executed.push(command.clone());
                output
                    .system_patches
                    .push(UiRuntimeSystemPatch::ExternalCommandDispatched {
                        command: command.name.clone(),
                    });
                self.systems.transactions.push(tx);
                return output;
            }
        }

        // Builtin runtime mutations.
        let pre_active_tabs = self.systems.workspace_layout.active_tabs.clone();
        let pre_focus = self.systems.focus_graph.focused.clone();
        let pre_selection_primary = self.systems.selection_model.primary.clone();

        let mut mutator = UiTreeMutator::new(&mut self.tree);
        let applied = ui_apply_builtin_command(&mut mutator, &mut self.systems, &command, &mut tx);
        let (patches, touched_nodes) = mutator.finish();
        tx.touched_nodes = touched_nodes;
        tx.patch_count = patches.len();

        if applied {
            output.executed.push(command);
            output.tree_patches.extend(patches);

            // Emit explicit runtime-system patches for inspectability.
            for (group_id, layout_id) in &self.systems.workspace_layout.active_tabs {
                if pre_active_tabs.get(group_id) != Some(layout_id) {
                    output
                        .system_patches
                        .push(UiRuntimeSystemPatch::WorkspaceActiveTabChanged {
                            group_id: group_id.clone(),
                            active_layout_id: layout_id.clone(),
                        });
                }
            }
            for (scope, focused) in &self.systems.focus_graph.focused {
                if pre_focus.get(scope) != Some(focused) {
                    output
                        .system_patches
                        .push(UiRuntimeSystemPatch::FocusChanged {
                            scope: scope.clone(),
                            focused: Some(*focused),
                        });
                }
            }
            for scope in pre_focus.keys() {
                if !self.systems.focus_graph.focused.contains_key(scope) {
                    output
                        .system_patches
                        .push(UiRuntimeSystemPatch::FocusChanged {
                            scope: scope.clone(),
                            focused: None,
                        });
                }
            }
            for (scope, primary) in &self.systems.selection_model.primary {
                if pre_selection_primary.get(scope) != Some(primary) {
                    output
                        .system_patches
                        .push(UiRuntimeSystemPatch::SelectionChanged {
                            scope: scope.clone(),
                            primary: Some(*primary),
                        });
                }
            }
            for scope in pre_selection_primary.keys() {
                if !self.systems.selection_model.primary.contains_key(scope) {
                    output
                        .system_patches
                        .push(UiRuntimeSystemPatch::SelectionChanged {
                            scope: scope.clone(),
                            primary: None,
                        });
                }
            }
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

#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
pub struct UiRuntimeReloadOutput {
    pub report: UiHotReloadTransferReport,
    #[serde(default)]
    pub system_patches: Vec<UiRuntimeSystemPatch>,
    pub invalidation: Option<UiInvalidationResult>,
    pub scheduler: Option<UiSchedulerCoalesceReport>,
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
    ExternalCommandDispatched {
        command: String,
    },
    HotReloadApplied {
        report: UiHotReloadTransferReport,
    },
    HotReloadInvalidated {
        invalidated_nodes: Vec<UiNodeId>,
    },
}

#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
pub struct UiCommandExecutionOutput {
    #[serde(default)]
    pub executed: Vec<UiCommand>,
    #[serde(default)]
    pub tree_patches: Vec<UiPatch>,
    #[serde(default)]
    pub system_patches: Vec<UiRuntimeSystemPatch>,
    #[serde(default)]
    pub rejections: Vec<UiCommandRejection>,
}

impl UiCommandExecutionOutput {
    fn merge(&mut self, other: UiCommandExecutionOutput) {
        self.executed.extend(other.executed);
        self.tree_patches.extend(other.tree_patches);
        self.system_patches.extend(other.system_patches);
        self.rejections.extend(other.rejections);
    }
}

fn runtime_state_delta_patches(
    previous_active_tabs: &BTreeMap<String, String>,
    previous_focus: &BTreeMap<String, UiNodeId>,
    previous_selection_primary: &BTreeMap<String, UiNodeId>,
    next_active_tabs: &BTreeMap<String, String>,
    next_focus: &BTreeMap<String, UiNodeId>,
    next_selection_primary: &BTreeMap<String, UiNodeId>,
) -> Vec<UiRuntimeSystemPatch> {
    let mut patches = Vec::new();

    for (group_id, layout_id) in next_active_tabs {
        if previous_active_tabs.get(group_id) != Some(layout_id) {
            patches.push(UiRuntimeSystemPatch::WorkspaceActiveTabChanged {
                group_id: group_id.clone(),
                active_layout_id: layout_id.clone(),
            });
        }
    }

    for (scope, focused) in next_focus {
        if previous_focus.get(scope) != Some(focused) {
            patches.push(UiRuntimeSystemPatch::FocusChanged {
                scope: scope.clone(),
                focused: Some(*focused),
            });
        }
    }
    for scope in previous_focus.keys() {
        if !next_focus.contains_key(scope) {
            patches.push(UiRuntimeSystemPatch::FocusChanged {
                scope: scope.clone(),
                focused: None,
            });
        }
    }

    for (scope, primary) in next_selection_primary {
        if previous_selection_primary.get(scope) != Some(primary) {
            patches.push(UiRuntimeSystemPatch::SelectionChanged {
                scope: scope.clone(),
                primary: Some(*primary),
            });
        }
    }
    for scope in previous_selection_primary.keys() {
        if !next_selection_primary.contains_key(scope) {
            patches.push(UiRuntimeSystemPatch::SelectionChanged {
                scope: scope.clone(),
                primary: None,
            });
        }
    }

    patches
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
                indexes
                    .signal_to_computed
                    .entry(*signal)
                    .or_default()
                    .push(idx);
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

/// Spatial snapshot for "layout correctness" queries.
///
/// This is intentionally tool/LLM-friendly: it makes ownership, containment,
/// overlay order, anchors, and focus traversal inspectable without host-specific
/// assumptions.
#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
pub struct UiSpatialSnapshot {
    pub viewport: UiRect,
    /// Runtime-owned active tab selection per tab group.
    #[serde(default)]
    pub active_tabs: BTreeMap<String, String>,
    #[serde(default)]
    pub nodes: Vec<UiSpatialNodeSnapshot>,
    #[serde(default)]
    pub overlays: Vec<UiOverlayResolved>,
    #[serde(default)]
    pub containment_violations: Vec<UiContainmentViolation>,
    #[serde(default)]
    pub focus_traversal: Vec<UiFocusTraversalSnapshot>,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiSpatialNodeSnapshot {
    pub node: UiNodeId,
    pub parent: Option<UiNodeId>,
    pub rect: UiRect,
    pub layout_kind: UiLayoutKind,
    /// The first ancestor that claims panel/widget ownership (when present).
    #[serde(default)]
    pub owner_panel: Option<UiNodeId>,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiOverlayResolved {
    pub id: String,
    pub node: UiNodeId,
    #[serde(default)]
    pub order: i32,
    #[serde(default)]
    pub rect: Option<UiRect>,
    #[serde(default)]
    pub anchor_target: Option<UiNodeId>,
    #[serde(default)]
    pub anchor_target_rect: Option<UiRect>,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiContainmentViolation {
    pub parent: UiNodeId,
    pub child: UiNodeId,
    pub parent_rect: UiRect,
    pub child_rect: UiRect,
    pub reason: String,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiFocusTraversalSnapshot {
    pub scope: String,
    #[serde(default)]
    pub order: Vec<UiNodeId>,
}

pub fn ui_compute_spatial_snapshot(
    tree: &UiTree,
    systems: &crate::UiRuntimeSystems,
    viewport_size: [f32; 2],
) -> UiSpatialSnapshot {
    let resolved = ui_solve_workspace_layout(tree, systems, viewport_size);
    ui_spatial_snapshot_from_layout(tree, systems, &resolved)
}

pub fn ui_spatial_snapshot_from_layout(
    tree: &UiTree,
    systems: &crate::UiRuntimeSystems,
    resolved: &UiResolvedLayout,
) -> UiSpatialSnapshot {
    let parent_index = ui_build_parent_index(tree);
    let mut rect_index = BTreeMap::<UiNodeId, UiRect>::new();
    let mut kind_index = BTreeMap::<UiNodeId, UiLayoutKind>::new();
    for entry in &resolved.nodes {
        rect_index.insert(entry.node, entry.rect);
        kind_index.insert(entry.node, entry.layout_kind);
    }

    let mut snapshot = UiSpatialSnapshot {
        viewport: resolved.viewport,
        active_tabs: systems.workspace_layout.active_tabs.clone(),
        ..UiSpatialSnapshot::default()
    };

    // Node snapshots (ownership + geometry).
    for entry in &resolved.nodes {
        let parent = parent_index.get(&entry.node).copied();
        snapshot.nodes.push(UiSpatialNodeSnapshot {
            node: entry.node,
            parent,
            rect: entry.rect,
            layout_kind: entry.layout_kind,
            owner_panel: ui_find_owner_panel(tree, &parent_index, entry.node),
        });
    }

    // Overlay order + anchor relationships.
    for overlay in &systems.overlay_stack.entries {
        let rect = rect_index.get(&overlay.node).copied();
        let (anchor_target, anchor_target_rect) = overlay
            .anchor
            .as_ref()
            .and_then(|anchor| {
                rect_index
                    .get(&anchor.target)
                    .copied()
                    .map(|r| (anchor.target, r))
            })
            .map(|(id, rect)| (Some(id), Some(rect)))
            .unwrap_or((None, None));
        snapshot.overlays.push(UiOverlayResolved {
            id: overlay.id.clone(),
            node: overlay.node,
            order: overlay.order,
            rect,
            anchor_target,
            anchor_target_rect,
        });
    }
    snapshot
        .overlays
        .sort_by(|a, b| (a.order, a.id.clone()).cmp(&(b.order, b.id.clone())));

    // Containment violations: when a parent claims overflow clipping, child rects should stay inside.
    for (child, parent) in &parent_index {
        let Some(child_rect) = rect_index.get(child).copied() else {
            continue;
        };
        let Some(parent_rect) = rect_index.get(parent).copied() else {
            continue;
        };
        let Some(parent_node) = tree.node(*parent) else {
            continue;
        };

        let overflow_x = parent_node.layout.overflow_x;
        let overflow_y = parent_node.layout.overflow_y;
        if overflow_x == UiOverflowBehavior::Visible && overflow_y == UiOverflowBehavior::Visible {
            continue;
        }

        if overflow_x != UiOverflowBehavior::Visible
            && (child_rect.x < parent_rect.x
                || child_rect.x + child_rect.width > parent_rect.x + parent_rect.width)
        {
            snapshot
                .containment_violations
                .push(UiContainmentViolation {
                    parent: *parent,
                    child: *child,
                    parent_rect,
                    child_rect,
                    reason: "overflow_x".to_string(),
                });
        }
        if overflow_y != UiOverflowBehavior::Visible
            && (child_rect.y < parent_rect.y
                || child_rect.y + child_rect.height > parent_rect.y + parent_rect.height)
        {
            snapshot
                .containment_violations
                .push(UiContainmentViolation {
                    parent: *parent,
                    child: *child,
                    parent_rect,
                    child_rect,
                    reason: "overflow_y".to_string(),
                });
        }
    }

    // Focus traversal: prefer explicit traversal edges when provided; otherwise derive a stable order
    // from the tree (scope-membership order).
    let mut scopes = BTreeSet::<String>::new();
    scopes.extend(systems.focus_graph.scopes.iter().cloned());
    for node in tree.nodes.values() {
        if let Some(scope) = node.focus_scope.as_deref() {
            scopes.insert(scope.to_string());
        }
    }
    for scope in scopes {
        let order = ui_derive_focus_traversal_order(tree, systems, &scope);
        snapshot
            .focus_traversal
            .push(UiFocusTraversalSnapshot { scope, order });
    }

    snapshot
}

fn ui_build_parent_index(tree: &UiTree) -> BTreeMap<UiNodeId, UiNodeId> {
    let mut parents = BTreeMap::new();
    for node in tree.nodes.values() {
        for child in &node.children {
            parents.insert(*child, node.id);
        }
    }
    parents
}

fn ui_find_owner_panel(
    tree: &UiTree,
    parent_index: &BTreeMap<UiNodeId, UiNodeId>,
    node: UiNodeId,
) -> Option<UiNodeId> {
    let mut cursor = Some(node);
    while let Some(id) = cursor {
        let Some(entry) = tree.node(id) else { break };
        if matches!(entry.kind, crate::UiWidgetKind::Panel) {
            return Some(id);
        }
        cursor = parent_index.get(&id).copied();
    }
    None
}

fn ui_derive_focus_traversal_order(
    tree: &UiTree,
    systems: &crate::UiRuntimeSystems,
    scope: &str,
) -> Vec<UiNodeId> {
    if !systems.focus_graph.traversal_edges.is_empty() {
        // If explicit edges exist, we still want a stable linear order for tools.
        // Use deterministic DFS over edges from an arbitrary start (sorted by id).
        let mut nodes: BTreeSet<UiNodeId> = BTreeSet::new();
        for edge in &systems.focus_graph.traversal_edges {
            if edge.scope == scope {
                nodes.insert(edge.from);
                nodes.insert(edge.to);
            }
        }
        return nodes.into_iter().collect();
    }

    // Fallback: stable preorder traversal of nodes that declare membership in the scope.
    let mut order = Vec::new();
    if let Some(root) = tree.root {
        ui_collect_focusable_preorder(tree, root, scope, &mut order);
    }
    order
}

fn ui_collect_focusable_preorder(
    tree: &UiTree,
    node: UiNodeId,
    scope: &str,
    out: &mut Vec<UiNodeId>,
) {
    let Some(entry) = tree.node(node) else { return };
    if entry.focus_scope.as_deref() == Some(scope) {
        out.push(node);
    }
    for child in &entry.children {
        ui_collect_focusable_preorder(tree, *child, scope, out);
    }
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum UiResizeDirection {
    #[default]
    Horizontal,
    Vertical,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Serialize, Deserialize)]
#[serde(default)]
pub struct UiResizeConstraints {
    pub min_px: f32,
    pub max_px: f32,
}

/// Pure resize math: size is always derived from drag-start, never accumulated.
///
/// This mirrors the verified contract from the older K_OS shell: it prevents
/// "flying panels" caused by incorrectly accumulating deltas.
pub fn ui_resize_size_from_drag_start(
    initial_px: f32,
    delta_px_from_start: f32,
    direction_sign: f32,
    constraints: UiResizeConstraints,
) -> f32 {
    let unconstrained = initial_px + delta_px_from_start * direction_sign;
    unconstrained
        .max(constraints.min_px)
        .min(constraints.max_px.max(constraints.min_px))
}

fn ui_eval_derived_expr(expr: &UiDerivedExpr, signals: &BTreeMap<UiSignalId, UiValue>) -> UiValue {
    match expr {
        UiDerivedExpr::Literal(value) => value.clone(),
        UiDerivedExpr::Signal(id) => signals.get(id).cloned().unwrap_or(UiValue::Null),
        UiDerivedExpr::Not(inner) => {
            UiValue::Bool(!ui_value_truthy(&ui_eval_derived_expr(inner, signals)))
        }
        UiDerivedExpr::And(a, b) => UiValue::Bool(
            ui_value_truthy(&ui_eval_derived_expr(a, signals))
                && ui_value_truthy(&ui_eval_derived_expr(b, signals)),
        ),
        UiDerivedExpr::Or(a, b) => UiValue::Bool(
            ui_value_truthy(&ui_eval_derived_expr(a, signals))
                || ui_value_truthy(&ui_eval_derived_expr(b, signals)),
        ),
        UiDerivedExpr::Eq(a, b) => {
            UiValue::Bool(ui_eval_derived_expr(a, signals) == ui_eval_derived_expr(b, signals))
        }
        UiDerivedExpr::Add(a, b) => ui_value_binary_numeric_op(a, b, signals, |x, y| x + y),
        UiDerivedExpr::Sub(a, b) => ui_value_binary_numeric_op(a, b, signals, |x, y| x - y),
        UiDerivedExpr::Mul(a, b) => ui_value_binary_numeric_op(a, b, signals, |x, y| x * y),
        UiDerivedExpr::Div(a, b) => {
            let rhs = ui_eval_derived_expr(b, signals);
            match rhs {
                UiValue::Int(0) => UiValue::Null,
                UiValue::Float(v) if v.abs() <= f64::EPSILON => UiValue::Null,
                _ => ui_value_binary_numeric_op(a, b, signals, |x, y| x / y),
            }
        }
        UiDerivedExpr::ToString(inner) => {
            UiValue::String(ui_value_to_string(&ui_eval_derived_expr(inner, signals)))
        }
    }
}

fn ui_value_truthy(value: &UiValue) -> bool {
    match value {
        UiValue::Null => false,
        UiValue::Bool(v) => *v,
        UiValue::Int(v) => *v != 0,
        UiValue::Float(v) => v.abs() > f64::EPSILON,
        UiValue::String(v) => match v.trim().to_ascii_lowercase().as_str() {
            "" | "0" | "false" | "no" | "off" | "null" => false,
            _ => true,
        },
        UiValue::Callback { .. } => true,
    }
}

fn ui_value_to_string(value: &UiValue) -> String {
    match value {
        UiValue::Null => "null".to_string(),
        UiValue::Bool(v) => v.to_string(),
        UiValue::Int(v) => v.to_string(),
        UiValue::Float(v) => v.to_string(),
        UiValue::String(v) => v.clone(),
        UiValue::Callback { event, .. } => format!("<callback {}>", event),
    }
}

fn ui_value_binary_numeric_op(
    a: &UiDerivedExpr,
    b: &UiDerivedExpr,
    signals: &BTreeMap<UiSignalId, UiValue>,
    op: fn(f64, f64) -> f64,
) -> UiValue {
    let left = ui_eval_derived_expr(a, signals);
    let right = ui_eval_derived_expr(b, signals);

    let (l, l_is_int) = match left {
        UiValue::Int(v) => (v as f64, true),
        UiValue::Float(v) => (v, false),
        UiValue::String(v) => match v.parse::<f64>() {
            Ok(parsed) => (parsed, false),
            Err(_) => return UiValue::Null,
        },
        _ => return UiValue::Null,
    };
    let (r, r_is_int) = match right {
        UiValue::Int(v) => (v as f64, true),
        UiValue::Float(v) => (v, false),
        UiValue::String(v) => match v.parse::<f64>() {
            Ok(parsed) => (parsed, false),
            Err(_) => return UiValue::Null,
        },
        _ => return UiValue::Null,
    };

    let result = op(l, r);
    if l_is_int && r_is_int && result.is_finite() && result.fract().abs() <= f64::EPSILON {
        UiValue::Int(result as i64)
    } else {
        UiValue::Float(result)
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

            systems
                .workspace_layout
                .active_tabs
                .insert(group_id, layout_id);
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
                systems
                    .selection_model
                    .primary
                    .insert(scope.clone(), node_id);
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
            if let Some(resource) = systems
                .resources
                .iter_mut()
                .find(|res| res.id == resource_id)
            {
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

impl UiWorkspaceLayout {
    pub fn active_tab(&self, group_id: &str) -> Option<&str> {
        self.active_tabs.get(group_id).map(|s| s.as_str())
    }
}

// Note: Any native-only "projection" formats should stay in adapter crates. This
// module only defines backend-neutral runtime and inspection contracts.

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{ui_runtime_systems_from_tree, UiLayoutKind, UiNode, UiTree, UiWidgetKind};

    #[test]
    fn resize_math_is_relative_to_drag_start_not_accumulated() {
        let initial = 384.0;
        let constraints = UiResizeConstraints {
            min_px: 0.0,
            max_px: 10_000.0,
        };

        let moves = [50.0, 100.0, 150.0]; // cumulative deltas from drag-start
        let mut size = initial;
        for delta in moves {
            size = ui_resize_size_from_drag_start(initial, delta, 1.0, constraints);
        }
        assert_eq!(size, 534.0);
    }

    #[test]
    fn resize_math_clamps_min_and_max() {
        let initial = 384.0;
        let constraints = UiResizeConstraints {
            min_px: 192.0,
            max_px: 1536.0,
        };

        assert_eq!(
            ui_resize_size_from_drag_start(initial, -300.0, 1.0, constraints),
            192.0
        );
        assert_eq!(
            ui_resize_size_from_drag_start(initial, 2000.0, 1.0, constraints),
            1536.0
        );
        assert_eq!(
            ui_resize_size_from_drag_start(initial, 100.0, 1.0, constraints),
            484.0
        );
    }

    #[test]
    fn focus_traversal_fallback_is_stable_preorder_for_scope_members() {
        let root = UiNodeId(1);
        let a = UiNodeId(2);
        let b = UiNodeId(3);

        let mut root_node = UiNode::new(root, UiWidgetKind::Panel);
        root_node.layout.kind = UiLayoutKind::FlexColumn;
        root_node.children = vec![a, b];

        let mut a_node = UiNode::new(a, UiWidgetKind::Element("button".to_string()));
        a_node.focus_scope = Some("main".to_string());
        let mut b_node = UiNode::new(b, UiWidgetKind::Element("button".to_string()));
        b_node.focus_scope = Some("main".to_string());

        let mut tree = UiTree::default();
        tree.root = Some(root);
        tree.nodes.insert(root, root_node);
        tree.nodes.insert(a, a_node);
        tree.nodes.insert(b, b_node);

        let systems = ui_runtime_systems_from_tree(&tree);
        let snapshot = ui_compute_spatial_snapshot(&tree, &systems, [300.0, 200.0]);
        let main = snapshot
            .focus_traversal
            .iter()
            .find(|entry| entry.scope == "main")
            .expect("main scope should be present");
        assert_eq!(main.order, vec![a, b]);
    }
}
