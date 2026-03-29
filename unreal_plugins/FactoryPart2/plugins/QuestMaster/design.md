# QuestMaster — Design Document

## 1. Architecture Overview

QuestMaster is a comprehensive quest system plugin built on a layered architecture:

```
┌─────────────────────────────────────────────────────────────┐
│                     UI Layer (Slate Widgets)                 │
│  QuestLogWidget | QuestTrackerWidget | QuestNotificationWidget│
│  QuestDetailWidget | ObjectiveListWidget | RewardDisplayWidget│
│  QuestMapMarkerWidget | QuestDebugWidget                     │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                   Blueprint API Layer                        │
│  60+ Blueprint-callable functions for quest control          │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                     Actor Layer                              │
│  QuestManagerActor | QuestGiverActor | QuestObjectiveActor  │
│  QuestTriggerActor (with RPC support)                        │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                   Component Layer                            │
│  QuestTrackerComponent | QuestObjectiveComponent             │
│  QuestRewardComponent (with replication)                     │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                   Subsystem Layer                            │
│  QuestManagerSubsystem (World subsystem with tick)           │
│  - Quest instance management                                 │
│  - State tracking and persistence                            │
│  - Graph caching                                             │
│  - Variable and flag management                              │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                   Graph Runtime Layer                        │
│  QuestSystem (GraphInstance + NodeData execution)            │
│  - 15+ NodeData classes with execute_node                    │
│  - Condition evaluation engine                               │
│  - Objective tracking                                        │
│  - Timer management                                          │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                   Graph Editor Layer                         │
│  QuestGraph (15+ node types for visual authoring)            │
│  - StartQuest, Objective, Branch, Reward, CompleteQuest     │
│  - Condition, Parallel, Sequence, Timer, Event              │
│  - Dialogue, Teleport, Spawn, FailQuest, Comment           │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                   Data Layer                                 │
│  Enums (12+) | Structs (35+) | DataTables (4)               │
│  Helper functions (50+)                                      │
└─────────────────────────────────────────────────────────────┘
```

## 2. Module Structure

### 2.1 Runtime Module (QuestMaster)
- **Purpose**: Core quest functionality for gameplay
- **Contents**:
  - All actors, components, subsystems
  - Data structures and enums
  - Graph runtime execution
  - Blueprint function libraries
  - Networking and replication
- **Dependencies**: Core, CoreUObject, Engine, GameplayTags

### 2.2 Editor Module (QuestMasterEditor)
- **Purpose**: Quest authoring tools
- **Contents**:
  - Graph editor nodes and schema
  - Slate UI widgets
  - Asset editors and factories
  - Details customizations
  - Editor utilities
- **Dependencies**: Runtime module, Slate, SlateCore, UMG, UnrealEd

## 3. Component Design

### 3.1 Graph Editor (quest_graph_editor.kn)

**Node Types** (15+):
1. **StartQuestNode** - Entry point with quest metadata
2. **ObjectiveNode** - Define objectives with target counts
3. **BranchNode** - Multi-condition branching (AND/OR logic)
4. **RewardNode** - XP, gold, items, reputation rewards
5. **CompleteQuestNode** - Quest completion with success state
6. **FailQuestNode** - Quest failure with reason
7. **ConditionNode** - Single condition check with success/failure branches
8. **ParallelNode** - Up to 4 simultaneous objectives
9. **SequenceNode** - Ordered objective completion
10. **TimerNode** - Countdown timer with success/failure
11. **EventNode** - Trigger gameplay events
12. **DialogueNode** - Integration with DialogueForge
13. **TeleportNode** - Player teleportation
14. **SpawnNode** - Actor spawning
15. **CommentNode** - Designer notes

**Properties per node**:
- Common: node_id, node_name, description
- Specific: quest_id, objective_id, reward_data, condition_data, timer_duration, event_name, etc.
- Pins: input_exec, output_exec, success, failure, branch1-4, on_complete

### 3.2 Graph Runtime (quest_graph_runtime.kn)

**NodeData Classes** (15+):
- Each node type has a corresponding NodeData class
- `execute_node(instance: GraphInstance) -> Int` returns output pin index
- State tracking: current_node_id, visited_nodes, active_objectives
- Condition evaluation: evaluate_condition_runtime()
- Objective tracking: update_objective_count(), check_objective_completion()
- Timer management: update_timer(), check_timer_expiration()

**GraphInstance**:
- instance_id: Int
- quest_id: String
- state: QuestState (Inactive, Active, Completed, Failed)
- objectives: Array<ObjectiveInstance>
- variables: Array<QuestVariable>
- start_time: Float
- completion_time: Float
- tracked: Bool

### 3.3 Subsystem (quest_subsystem.kn)

**QuestManagerSubsystem** (@subsystem @tick):
- **Active quest management**: start_quest(), complete_quest(), fail_quest(), abandon_quest()
- **Objective management**: update_objective(), complete_objective(), reset_objective()
- **Quest tracking**: track_quest(), untrack_quest(), get_tracked_quest()
- **State queries**: get_active_quests(), get_completed_quests(), get_failed_quests(), get_available_quests()
- **Progress queries**: get_quest_progress(), get_objective_progress(), is_quest_complete()
- **Graph management**: load_quest_graph(), unload_quest_graph(), find_quest_graph()
- **Variable management**: set_global_variable(), get_global_variable()
- **Flag management**: set_persistent_flag(), check_persistent_flag()
- **Persistence**: save_quest_state(), load_quest_state(), auto_save()
- **Tick logic**: update_active_quests(), update_timers(), check_auto_save()

**State**:
- active_quests: Array<QuestInstance>
- completed_quests: Array<QuestInstance>
- failed_quests: Array<QuestInstance>
- loaded_graphs: Array<QuestGraph>
- global_variables: Array<QuestVariable>
- persistent_flags: Array<String>
- tracked_quest_id: String
- next_instance_id: Int
- max_active_quests: Int = 50
- auto_save_enabled: Bool = true
- auto_save_interval: Float = 30.0
- tick_rate: Float = 0.1

### 3.4 Actors (quest_actors.kn)

**QuestManagerActor**:
- Centralized quest coordination
- RPC support: Server_StartQuest(), Server_CompleteQuest(), Server_FailQuest(), Server_AbandonQuest()
- Multicast: Multicast_QuestStateChanged(), Multicast_ObjectiveUpdated()
- Replication: @replicated active_quest_count, tracked_quest_id

**QuestGiverActor**:
- Quest offering with interaction
- available_quests: Array<String>
- completed_quests: Array<String>
- repeatable: Bool
- interaction_text: String
- RPC: Server_AcceptQuest(), Server_TurnInQuest()

**QuestObjectiveActor**:
- Objective tracking with triggers
- quest_id: String
- objective_id: String
- target_count: Int
- current_count: Int
- auto_complete: Bool
- RPC: Server_UpdateObjective()

**QuestTriggerActor**:
- Event triggering on overlap/interaction
- trigger_type: String (Overlap, Interact, Timer)
- quest_id: String
- event_name: String
- one_time: Bool
- RPC: Server_TriggerEvent()

### 3.5 Components (quest_components.kn)

**QuestTrackerComponent** (@component):
- @replicated active_quests: Array<String>
- @replicated tracked_quest_id: String
- @replicated quest_progress: Array<Float>
- Functions: add_quest(), remove_quest(), update_progress()

**QuestObjectiveComponent** (@component):
- @replicated objective_id: String
- @replicated current_count: Int
- @replicated target_count: Int
- @replicated completed: Bool
- Functions: increment_count(), set_count(), reset()

**QuestRewardComponent** (@component):
- reward_xp: Int
- reward_gold: Int
- reward_items: Array<ItemReward>
- reward_reputation: Array<ReputationReward>
- Functions: apply_rewards(), give_xp(), give_gold(), give_items()

### 3.6 UI Widgets (quest_ui_widgets.kn)

**QuestLogWidget** (@slate):
- quest_list: Array<QuestInstance>
- filter_mode: QuestFilterMode (All, Active, Completed, Failed)
- sort_mode: QuestSortMode (Name, Priority, Progress)
- selected_quest_id: String
- Functions: set_quests(), filter_quests(), sort_quests(), select_quest()

**QuestTrackerWidget** (@slate):
- tracked_quest: QuestInstance
- show_objectives: Bool = true
- show_progress_bars: Bool = true
- max_objectives_shown: Int = 5
- Functions: set_tracked_quest(), update_objectives(), update_progress()

**QuestNotificationWidget** (@slate):
- notification_text: String
- notification_type: QuestNotificationType (Started, Completed, Failed, ObjectiveCompleted)
- display_duration: Float = 5.0
- fade_in_duration: Float = 0.5
- fade_out_duration: Float = 0.5
- Functions: show_quest_started(), show_quest_completed(), show_quest_failed(), show_objective_completed()

**QuestDetailWidget** (@slate):
- quest_data: QuestInstance
- show_description: Bool = true
- show_objectives: Bool = true
- show_rewards: Bool = true
- show_lore: Bool = true
- Functions: set_quest(), update_display()

**ObjectiveListWidget** (@slate):
- objectives: Array<ObjectiveInstance>
- show_progress_bars: Bool = true
- show_completion_state: Bool = true
- Functions: set_objectives(), update_objective(), mark_completed()

**RewardDisplayWidget** (@slate):
- rewards: QuestReward
- show_icons: Bool = true
- show_quantities: Bool = true
- icon_size: Float = 64.0
- Functions: set_rewards(), update_display()

**QuestMapMarkerWidget** (@slate):
- marker_location: Vec3
- marker_type: QuestMarkerType (Objective, QuestGiver, TurnIn)
- marker_icon: String
- marker_color: Vec3
- Functions: set_location(), set_type(), update_marker()

**QuestDebugWidget** (@slate):
- active_quest_count: Int
- tracked_quest_id: String
- debug_info: Array<String>
- show_variables: Bool = true
- show_flags: Bool = true
- Functions: update_debug_info(), toggle_visibility()

### 3.7 Data Structures (quest_data_structures.kn)

**Enums** (12+):
- QuestState: Inactive, Active, Completed, Failed, Abandoned
- QuestPriority: Low, Normal, High, Critical
- QuestCategory: Main, Side, Daily, Repeatable, Hidden
- ObjectiveType: Kill, Collect, Interact, Reach, Escort, Defend, Discover
- ConditionType: QuestComplete, QuestActive, ItemInInventory, StatValue, FlagSet, TimeOfDay, Location, Level, Reputation, Custom
- RewardType: XP, Gold, Item, Reputation, Unlock, Custom
- QuestNotificationType: Started, Completed, Failed, ObjectiveCompleted, ObjectiveFailed
- QuestFilterMode: All, Active, Completed, Failed, Available
- QuestSortMode: Name, Priority, Progress, Level, Category
- QuestMarkerType: Objective, QuestGiver, TurnIn, Area
- TimerMode: Countdown, Countup
- ParallelMode: WaitForAll, WaitForAny

**Structs** (35+):
- QuestData: quest_id, name, description, category, priority, prerequisites, rewards
- ObjectiveData: objective_id, description, type, target_count, optional, hidden
- QuestReward: xp, gold, items, reputation, custom_rewards
- QuestCondition: condition_type, target, comparison_value, invert
- QuestInstance: instance_id, quest_id, state, objectives, variables, start_time, completion_time
- ObjectiveInstance: objective_id, current_count, target_count, completed
- QuestVariable: variable_name, variable_type, current_value, is_persistent
- QuestGraph: graph_id, name, description, entry_node_id, nodes
- QuestNode: node_id, node_type, properties, pins
- ItemReward: item_id, quantity, quality
- ReputationReward: faction_id, amount
- QuestPrerequisite: quest_id, must_complete, must_fail
- QuestTimer: duration, elapsed, mode, show_ui
- QuestEvent: event_name, parameters, delay
- QuestMarker: location, type, icon, color
- (20+ more structs for specific node types, UI state, etc.)

**DataTables** (4):
- QuestDataTable: Define quests with all metadata
- ObjectiveDataTable: Define objectives with types and targets
- RewardDataTable: Define rewards with types and quantities
- QuestCategoryDataTable: Define categories with icons and colors

**Helper Functions** (50+):
- create_quest_instance(), create_objective_instance()
- evaluate_condition(), evaluate_all_conditions()
- is_quest_available(), is_objective_complete()
- calculate_quest_progress(), calculate_objective_progress()
- format_quest_text(), format_objective_text()
- get_quest_state_display_name(), get_objective_type_display_name()
- apply_quest_rewards(), give_xp(), give_gold(), give_items()
- (40+ more helper functions)

## 4. Data Flow

### 4.1 Quest Start Flow
```
Player interacts with QuestGiverActor
    ↓
QuestGiverActor.Server_AcceptQuest(quest_id)
    ↓
QuestManagerSubsystem.start_quest(quest_id, player)
    ↓
Create QuestInstance with unique instance_id
    ↓
Load QuestGraph from cache
    ↓
Execute StartQuestNode in graph runtime
    ↓
Update subsystem state (add to active_quests)
    ↓
Replicate to all clients via Multicast_QuestStateChanged
    ↓
Update UI (QuestNotificationWidget, QuestTrackerWidget)
```

### 4.2 Objective Update Flow
```
Player completes objective action (kill, collect, etc.)
    ↓
QuestObjectiveActor.Server_UpdateObjective(quest_id, objective_id, count)
    ↓
QuestManagerSubsystem.update_objective(quest_id, objective_id, count)
    ↓
Find QuestInstance in active_quests
    ↓
Update ObjectiveInstance.current_count
    ↓
Check if current_count >= target_count
    ↓
If complete: execute ObjectiveNode.execute_node()
    ↓
Advance to next node in graph
    ↓
Replicate to all clients via Multicast_ObjectiveUpdated
    ↓
Update UI (ObjectiveListWidget, QuestTrackerWidget)
```

### 4.3 Quest Completion Flow
```
All objectives completed
    ↓
Execute CompleteQuestNode in graph runtime
    ↓
Execute RewardNode (if present)
    ↓
Apply rewards via QuestRewardComponent
    ↓
QuestManagerSubsystem.complete_quest(quest_id)
    ↓
Move QuestInstance from active_quests to completed_quests
    ↓
Set QuestInstance.state = QuestState::Completed
    ↓
Set QuestInstance.completion_time = current_time
    ↓
Trigger OnQuestCompleted event
    ↓
Replicate to all clients via Multicast_QuestStateChanged
    ↓
Update UI (QuestNotificationWidget, QuestLogWidget)
    ↓
Save quest state to persistent storage
```

### 4.4 Networking Flow
```
Client Action (accept quest, update objective, etc.)
    ↓
Client sends RPC to server (Server_*)
    ↓
Server validates action
    ↓
Server updates authoritative state
    ↓
Server replicates state to all clients (Multicast_*)
    ↓
Clients update local state
    ↓
Clients update UI
```

## 5. Correctness Properties

### 5.1 Quest State Invariants

**Property 1**: A quest instance SHALL have exactly one state at any time.
- Formal: ∀ quest_instance, |{state | quest_instance.state = state}| = 1
- Test: Verify state transitions never leave quest in multiple states

**Property 2**: A quest SHALL NOT be in both active_quests and completed_quests simultaneously.
- Formal: ∀ quest, quest ∈ active_quests ⇒ quest ∉ completed_quests
- Test: Verify quest lists are mutually exclusive

**Property 3**: A completed quest SHALL have completion_time > start_time.
- Formal: ∀ quest, quest.state = Completed ⇒ quest.completion_time > quest.start_time
- Test: Verify completion time is always after start time

**Property 4**: An objective SHALL NOT have current_count > target_count.
- Formal: ∀ objective, objective.current_count ≤ objective.target_count
- Test: Verify objective count never exceeds target

**Property 5**: A quest with all objectives completed SHALL transition to Completed state.
- Formal: ∀ quest, (∀ obj ∈ quest.objectives, obj.completed = true) ⇒ quest.state = Completed
- Test: Verify quest completes when all objectives are done

### 5.2 Replication Invariants

**Property 6**: Quest state on server SHALL eventually replicate to all clients.
- Formal: ∀ client, eventually(client.quest_state = server.quest_state)
- Test: Verify state synchronization within network latency bounds

**Property 7**: Objective updates on server SHALL replicate to all clients in order.
- Formal: ∀ updates u1, u2, u1 < u2 on server ⇒ u1 < u2 on all clients
- Test: Verify update ordering is preserved

**Property 8**: A quest started on server SHALL appear in active_quests on all clients.
- Formal: ∀ quest, quest ∈ server.active_quests ⇒ eventually(∀ client, quest ∈ client.active_quests)
- Test: Verify quest replication to all clients

### 5.3 Graph Execution Invariants

**Property 9**: A graph execution SHALL always reach a terminal node (CompleteQuest or FailQuest).
- Formal: ∀ execution, eventually(current_node ∈ {CompleteQuestNode, FailQuestNode})
- Test: Verify no infinite loops in graph execution

**Property 10**: A condition node SHALL evaluate to exactly one output pin (success or failure).
- Formal: ∀ condition_node, execute_node() returns 0 XOR returns 1
- Test: Verify condition evaluation is deterministic

**Property 11**: A parallel node SHALL wait for all branches if wait_for_all = true.
- Formal: ∀ parallel_node, parallel_node.wait_for_all = true ⇒ on_complete triggers when all branches complete
- Test: Verify parallel completion logic

**Property 12**: A timer node SHALL trigger failure if elapsed >= duration.
- Formal: ∀ timer_node, timer_node.elapsed ≥ timer_node.duration ⇒ execute_node() returns failure_pin
- Test: Verify timer expiration logic

### 5.4 Persistence Invariants

**Property 13**: Quest state saved SHALL be identical to quest state loaded.
- Formal: ∀ state, save(state) then load() ⇒ loaded_state = state
- Test: Verify save/load round-trip preserves all data

**Property 14**: Persistent flags SHALL survive game restart.
- Formal: ∀ flag, set_persistent_flag(flag, true) then restart ⇒ check_persistent_flag(flag) = true
- Test: Verify flag persistence across sessions

**Property 15**: Auto-save SHALL occur every auto_save_interval seconds.
- Formal: ∀ t, t mod auto_save_interval = 0 ⇒ save_quest_state() is called
- Test: Verify auto-save timing

### 5.5 UI Invariants

**Property 16**: Tracked quest SHALL be displayed in QuestTrackerWidget.
- Formal: ∀ quest, quest.tracked = true ⇒ QuestTrackerWidget.tracked_quest = quest
- Test: Verify tracker widget updates when quest is tracked

**Property 17**: Quest notification SHALL display for exactly display_duration seconds.
- Formal: ∀ notification, notification.show() ⇒ notification.hide() after display_duration
- Test: Verify notification timing

**Property 18**: Quest log SHALL display all active quests when filter_mode = Active.
- Formal: ∀ quest, quest.state = Active ⇒ quest ∈ QuestLogWidget.quest_list when filter_mode = Active
- Test: Verify filtering logic

### 5.6 Reward Invariants

**Property 19**: Rewards SHALL be applied exactly once per quest completion.
- Formal: ∀ quest, quest.state = Completed ⇒ rewards applied exactly once
- Test: Verify rewards are not duplicated

**Property 20**: XP reward SHALL increase player XP by reward_xp amount.
- Formal: ∀ reward, apply_rewards(reward) ⇒ player.xp_after = player.xp_before + reward.xp
- Test: Verify XP application

## 6. Performance Considerations

### 6.1 Subsystem Tick Optimization
- Tick rate: 0.1s (10 Hz) - configurable
- Only update active quests, skip completed/failed
- Cache quest graphs to avoid repeated loading
- Batch UI updates to reduce overhead

### 6.2 Replication Optimization
- Use @replicated only on critical state
- Batch objective updates when possible
- Use multicast for state changes affecting all clients
- Compress quest data for network transmission

### 6.3 UI Optimization
- Update widgets only when state changes
- Use widget pooling for quest list items
- Lazy-load quest details on selection
- Throttle progress bar updates to 30 FPS

### 6.4 Memory Optimization
- Unload unused quest graphs after timeout
- Limit completed quest history to 100 entries
- Clear failed quest data after 24 hours (configurable)
- Use struct pooling for temporary allocations

## 7. Testing Strategy

### 7.1 Unit Tests
- Test each NodeData.execute_node() function
- Test condition evaluation logic
- Test objective tracking logic
- Test reward application logic
- Test helper functions

### 7.2 Integration Tests
- Test quest start-to-completion flow
- Test objective update flow
- Test quest failure flow
- Test quest abandonment flow
- Test save/load cycle

### 7.3 Networking Tests
- Test RPC functionality
- Test state replication
- Test multiplayer quest sharing
- Test network latency handling
- Test client-server synchronization

### 7.4 UI Tests
- Test widget creation and display
- Test widget updates on state change
- Test user input handling
- Test keyboard/gamepad navigation
- Test localization

### 7.5 Performance Tests
- Test with 50 active quests per player
- Test with 32 players in multiplayer
- Test subsystem tick performance
- Test UI update performance
- Test save/load performance

## 8. Future Enhancements

- Quest chains with automatic progression
- Dynamic quest generation
- Quest sharing in multiplayer
- Quest leaderboards
- Quest analytics and telemetry
- Quest localization tools
- Quest debugging tools
- Quest replay system
- Quest branching based on player choices
- Quest difficulty scaling
- Quest time limits with UI countdown
- Quest reputation requirements
- Quest level requirements
- Quest prerequisite chains
- Quest rewards preview

## 9. Implementation Notes

### 9.1 KAIN Features Used
- @graph_editor for visual authoring
- @graph_runtime for execution
- @subsystem @tick for state management
- @slate for UI widgets
- @component with @replicated for networking
- actor with Server_/Client_/Multicast_ RPCs
- @datatable for quest definitions
- @blueprint for Blueprint integration

### 9.2 Stdlib Functions Used
- Array operations: push(), pop(), len(), clear()
- String operations: split(), join(), format()
- Math operations: min(), max(), clamp()
- Time operations: get_world_time(), get_delta_seconds()
- Random operations: random_float(), random_int_range()

### 9.3 Code Organization
- quest_data_structures.kn: ~1,500 lines (enums, structs, helpers)
- quest_graph_editor.kn: ~600 lines (15 node types)
- quest_graph_runtime.kn: ~1,200 lines (15 NodeData classes)
- quest_actors.kn: ~1,000 lines (4 actors, 30+ blueprint functions)
- quest_components.kn: ~600 lines (3 components)
- quest_subsystem.kn: ~1,200 lines (subsystem with 50+ functions)
- quest_ui_widgets.kn: ~1,400 lines (8 widgets, 20+ blueprint functions)
- quest_blueprint_library.kn: ~500 lines (60+ blueprint functions)
- **Total**: ~8,000 lines (target: 8,000-11,000)

### 9.4 Module Configuration
```toml
[package]
name = "QuestMaster"
version = "1.0.0"

[ue5]
plugin_name = "QuestMaster"
engine_version = "5.4"
category = "Gameplay"
description = "Comprehensive quest system with graph editor, tracking, and multiplayer support"

[[ue5.modules]]
name = "QuestMaster"
type = "Runtime"
loading_phase = "Default"

[[ue5.modules]]
name = "QuestMasterEditor"
type = "Editor"
loading_phase = "Default"
depends_on = ["QuestMaster"]
```

## 10. Conclusion

QuestMaster provides a complete, production-ready quest system for UE5 with:
- Visual graph editor for quest authoring (15+ node types)
- Robust runtime execution with state management
- Full multiplayer networking support
- Comprehensive UI widgets for quest display
- Blueprint integration for designer-friendly workflows
- Data-driven quest definitions via DataTables
- Persistence and save/load support
- Performance optimizations for large-scale games

The architecture follows established patterns from DialogueForge and other Factory Part 2 plugins, ensuring consistency and maintainability. All correctness properties are testable and verifiable, ensuring a reliable quest system for production use.
