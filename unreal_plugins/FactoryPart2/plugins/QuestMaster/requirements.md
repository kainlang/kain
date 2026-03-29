# QuestMaster — Requirements Specification

## 1. Overview

QuestMaster is a comprehensive quest system plugin for Unreal Engine 5 that provides quest authoring, tracking, execution, and management. It features a visual graph editor for quest flow design, runtime execution system, subsystems for state management, UI widgets for quest display, Blueprint integration, and full multiplayer networking support.

## 2. Functional Requirements (EARS Format)

### 2.1 Graph Editor Requirements

**REQ-GE-001**: WHEN a designer opens the quest graph editor, THEN the system SHALL provide at least 15 distinct node types for quest authoring.

**REQ-GE-002**: WHEN a designer creates a StartQuest node, THEN the system SHALL allow configuration of quest ID, name, description, category, priority, and auto-start behavior.

**REQ-GE-003**: WHEN a designer creates an Objective node, THEN the system SHALL allow configuration of objective ID, description, target count, optional flag, hidden flag, and completion conditions.

**REQ-GE-004**: WHEN a designer creates a Branch node, THEN the system SHALL support multi-condition branching with AND/OR logic and up to 4 conditions.

**REQ-GE-005**: WHEN a designer creates a Reward node, THEN the system SHALL allow configuration of XP, gold, items, reputation, and custom rewards.

**REQ-GE-006**: WHEN a designer creates a CompleteQuest node, THEN the system SHALL trigger quest completion with success/failure states and optional events.

**REQ-GE-007**: WHEN a designer creates a FailQuest node, THEN the system SHALL trigger quest failure with reason and optional retry logic.

**REQ-GE-008**: WHEN a designer creates a Condition node, THEN the system SHALL support at least 15 condition types including quest state, inventory, stats, flags, time, location, and custom conditions.

**REQ-GE-009**: WHEN a designer creates a Parallel node, THEN the system SHALL support up to 4 simultaneous objective branches with wait-for-all or wait-for-any logic.

**REQ-GE-010**: WHEN a designer creates a Sequence node, THEN the system SHALL enforce ordered objective completion with automatic progression.

**REQ-GE-011**: WHEN a designer creates a Timer node, THEN the system SHALL support countdown timers with success/failure branches and optional UI display.

**REQ-GE-012**: WHEN a designer creates an Event node, THEN the system SHALL trigger gameplay events with parameters and optional delay.

**REQ-GE-013**: WHEN a designer creates a Dialogue node, THEN the system SHALL integrate with DialogueForge for quest-driven conversations.

**REQ-GE-014**: WHEN a designer creates a Teleport node, THEN the system SHALL support player teleportation with fade effects.

**REQ-GE-015**: WHEN a designer creates a Spawn node, THEN the system SHALL support actor spawning with location, rotation, and attachment options.

### 2.2 Graph Runtime Requirements

**REQ-GR-001**: WHEN a quest graph is executed, THEN the system SHALL create a GraphInstance with unique instance ID and state tracking.

**REQ-GR-002**: WHEN a node is executed, THEN the system SHALL call the node's execute_node function and return the output pin index.

**REQ-GR-003**: WHEN an objective is updated, THEN the system SHALL track current count, target count, and completion state.

**REQ-GR-004**: WHEN a condition is evaluated, THEN the system SHALL return true/false based on current game state.

**REQ-GR-005**: WHEN a parallel node executes, THEN the system SHALL track completion state of all branches and trigger on_complete when logic is satisfied.

**REQ-GR-006**: WHEN a timer node executes, THEN the system SHALL count down from duration and trigger success/failure branches appropriately.

**REQ-GR-007**: WHEN a reward node executes, THEN the system SHALL apply all configured rewards to the player.

**REQ-GR-008**: WHEN a quest completes, THEN the system SHALL trigger OnQuestCompleted event with quest ID and success state.

**REQ-GR-009**: WHEN a quest fails, THEN the system SHALL trigger OnQuestFailed event with quest ID and failure reason.

**REQ-GR-010**: WHEN an objective completes, THEN the system SHALL trigger OnObjectiveCompleted event with quest ID and objective ID.

### 2.3 Subsystem Requirements

**REQ-SS-001**: WHEN the world initializes, THEN the system SHALL create a QuestManagerSubsystem with tick support.

**REQ-SS-002**: WHEN a quest is started, THEN the subsystem SHALL create a quest instance with unique ID and add it to active quests.

**REQ-SS-003**: WHEN a quest is completed, THEN the subsystem SHALL move it from active quests to completed quests and trigger completion events.

**REQ-SS-004**: WHEN a quest is failed, THEN the subsystem SHALL move it from active quests to failed quests and trigger failure events.

**REQ-SS-005**: WHEN an objective is updated, THEN the subsystem SHALL update the objective's current count and check for completion.

**REQ-SS-006**: WHEN the subsystem ticks, THEN it SHALL update all active quest timers and check for expiration.

**REQ-SS-007**: WHEN a quest is tracked, THEN the subsystem SHALL set it as the currently tracked quest and notify UI.

**REQ-SS-008**: WHEN a quest is untracked, THEN the subsystem SHALL clear the tracked quest and notify UI.

**REQ-SS-009**: WHEN a quest is abandoned, THEN the subsystem SHALL remove it from active quests and optionally trigger failure events.

**REQ-SS-010**: WHEN quest state is saved, THEN the subsystem SHALL serialize all active, completed, and failed quests to persistent storage.

**REQ-SS-011**: WHEN quest state is loaded, THEN the subsystem SHALL deserialize quest data and restore all quest instances.

**REQ-SS-012**: WHEN a quest graph is loaded, THEN the subsystem SHALL cache it for fast access.

**REQ-SS-013**: WHEN a quest graph is unloaded, THEN the subsystem SHALL remove it from cache.

**REQ-SS-014**: WHEN a global quest variable is set, THEN the subsystem SHALL update the variable and persist it if marked persistent.

**REQ-SS-015**: WHEN a global quest flag is set, THEN the subsystem SHALL update the flag and persist it if marked persistent.

**REQ-SS-016**: WHEN the subsystem is queried for active quests, THEN it SHALL return all quests with state Active.

**REQ-SS-017**: WHEN the subsystem is queried for completed quests, THEN it SHALL return all quests with state Completed.

**REQ-SS-018**: WHEN the subsystem is queried for failed quests, THEN it SHALL return all quests with state Failed.

**REQ-SS-019**: WHEN the subsystem is queried for available quests, THEN it SHALL return all quests that meet prerequisite conditions.

**REQ-SS-020**: WHEN the subsystem is queried for quest progress, THEN it SHALL return completion percentage based on objectives.

### 2.4 Actor Requirements

**REQ-AC-001**: WHEN a QuestManagerActor is spawned, THEN it SHALL provide centralized quest coordination with RPC support.

**REQ-AC-002**: WHEN a QuestGiverActor is spawned, THEN it SHALL provide quest offering with interaction support.

**REQ-AC-003**: WHEN a QuestObjectiveActor is spawned, THEN it SHALL provide objective tracking with overlap/interaction triggers.

**REQ-AC-004**: WHEN a QuestTriggerActor is spawned, THEN it SHALL trigger quest events on overlap or interaction.

**REQ-AC-005**: WHEN a player interacts with a QuestGiverActor, THEN it SHALL display available quests and allow acceptance.

**REQ-AC-006**: WHEN a player completes an objective at a QuestObjectiveActor, THEN it SHALL update the objective and notify the subsystem.

**REQ-AC-007**: WHEN a QuestTriggerActor is activated, THEN it SHALL trigger the configured quest event with parameters.

**REQ-AC-008**: WHEN a quest actor replicates, THEN it SHALL synchronize state across all clients.

### 2.5 Component Requirements

**REQ-CO-001**: WHEN a QuestTrackerComponent is added to an actor, THEN it SHALL track quest progress for that actor.

**REQ-CO-002**: WHEN a QuestObjectiveComponent is added to an actor, THEN it SHALL provide objective completion logic.

**REQ-CO-003**: WHEN a QuestRewardComponent is added to an actor, THEN it SHALL manage reward distribution.

**REQ-CO-004**: WHEN a component replicates, THEN it SHALL synchronize state across all clients.

### 2.6 UI Widget Requirements

**REQ-UI-001**: WHEN a QuestLogWidget is created, THEN it SHALL display all active, completed, and failed quests with filtering and sorting.

**REQ-UI-002**: WHEN a QuestTrackerWidget is created, THEN it SHALL display the currently tracked quest with objectives and progress bars.

**REQ-UI-003**: WHEN a QuestNotificationWidget is created, THEN it SHALL display quest start/complete/fail notifications with fade in/out.

**REQ-UI-004**: WHEN a QuestDetailWidget is created, THEN it SHALL display full quest information including description, objectives, rewards, and lore.

**REQ-UI-005**: WHEN an ObjectiveListWidget is created, THEN it SHALL display all objectives for a quest with completion state and progress.

**REQ-UI-006**: WHEN a RewardDisplayWidget is created, THEN it SHALL display all rewards for a quest with icons and quantities.

**REQ-UI-007**: WHEN a QuestMapMarkerWidget is created, THEN it SHALL display quest objective locations on the map.

**REQ-UI-008**: WHEN a QuestDebugWidget is created, THEN it SHALL display debug information for active quests.

**REQ-UI-009**: WHEN a quest is tracked, THEN the QuestTrackerWidget SHALL update to show the new quest.

**REQ-UI-010**: WHEN an objective is completed, THEN the ObjectiveListWidget SHALL update to show completion state.

**REQ-UI-011**: WHEN a quest is completed, THEN the QuestNotificationWidget SHALL display a completion notification.

**REQ-UI-012**: WHEN a quest is failed, THEN the QuestNotificationWidget SHALL display a failure notification.

### 2.7 Networking Requirements

**REQ-NET-001**: WHEN a quest is started on the server, THEN it SHALL replicate to all clients.

**REQ-NET-002**: WHEN an objective is updated on the server, THEN it SHALL replicate to all clients.

**REQ-NET-003**: WHEN a quest is completed on the server, THEN it SHALL replicate to all clients.

**REQ-NET-004**: WHEN a quest is failed on the server, THEN it SHALL replicate to all clients.

**REQ-NET-005**: WHEN a player accepts a quest, THEN the client SHALL send an RPC to the server.

**REQ-NET-006**: WHEN a player abandons a quest, THEN the client SHALL send an RPC to the server.

**REQ-NET-007**: WHEN a player tracks a quest, THEN the client SHALL send an RPC to the server.

**REQ-NET-008**: WHEN quest state changes, THEN the server SHALL multicast the change to all clients.

**REQ-NET-009**: WHEN a quest actor is spawned, THEN it SHALL replicate to all clients.

**REQ-NET-010**: WHEN a quest component state changes, THEN it SHALL replicate to all clients.

### 2.8 Blueprint Integration Requirements

**REQ-BP-001**: WHEN a Blueprint calls StartQuest, THEN the system SHALL start the quest and return the instance ID.

**REQ-BP-002**: WHEN a Blueprint calls CompleteQuest, THEN the system SHALL complete the quest and trigger events.

**REQ-BP-003**: WHEN a Blueprint calls FailQuest, THEN the system SHALL fail the quest and trigger events.

**REQ-BP-004**: WHEN a Blueprint calls UpdateObjective, THEN the system SHALL update the objective count and check for completion.

**REQ-BP-005**: WHEN a Blueprint calls GetActiveQuests, THEN the system SHALL return all active quests.

**REQ-BP-006**: WHEN a Blueprint calls GetQuestProgress, THEN the system SHALL return the completion percentage.

**REQ-BP-007**: WHEN a Blueprint calls TrackQuest, THEN the system SHALL set the quest as tracked.

**REQ-BP-008**: WHEN a Blueprint calls AbandonQuest, THEN the system SHALL remove the quest from active quests.

**REQ-BP-009**: WHEN a Blueprint calls GiveQuestReward, THEN the system SHALL apply all rewards to the player.

**REQ-BP-010**: WHEN a Blueprint calls CheckQuestCondition, THEN the system SHALL evaluate the condition and return true/false.

### 2.9 Data Structure Requirements

**REQ-DS-001**: WHEN a quest is defined, THEN it SHALL have a unique quest ID, name, description, category, priority, and prerequisites.

**REQ-DS-002**: WHEN an objective is defined, THEN it SHALL have a unique objective ID, description, target count, and completion conditions.

**REQ-DS-003**: WHEN a reward is defined, THEN it SHALL have XP, gold, items, reputation, and custom reward data.

**REQ-DS-004**: WHEN a condition is defined, THEN it SHALL have a condition type, target, comparison value, and invert flag.

**REQ-DS-005**: WHEN a quest instance is created, THEN it SHALL track instance ID, quest ID, state, objectives, start time, and completion time.

**REQ-DS-006**: WHEN an objective instance is created, THEN it SHALL track objective ID, current count, target count, and completion state.

**REQ-DS-007**: WHEN quest data is serialized, THEN it SHALL include all quest state, objectives, variables, and flags.

**REQ-DS-008**: WHEN quest data is deserialized, THEN it SHALL restore all quest state, objectives, variables, and flags.

### 2.10 DataTable Requirements

**REQ-DT-001**: WHEN a QuestData DataTable is created, THEN it SHALL define quest ID, name, description, category, rewards, and prerequisites.

**REQ-DT-002**: WHEN an ObjectiveData DataTable is created, THEN it SHALL define objective ID, description, target count, and type.

**REQ-DT-003**: WHEN a RewardData DataTable is created, THEN it SHALL define reward type, quantity, and item references.

**REQ-DT-004**: WHEN a QuestCategoryData DataTable is created, THEN it SHALL define category ID, name, icon, and color.

## 3. Non-Functional Requirements

### 3.1 Performance Requirements

**REQ-PERF-001**: The subsystem SHALL support at least 50 concurrent active quests per player.

**REQ-PERF-002**: The subsystem tick rate SHALL be configurable with a default of 0.1 seconds (10 Hz).

**REQ-PERF-003**: Quest condition evaluation SHALL complete within 1 millisecond.

**REQ-PERF-004**: Quest state serialization SHALL complete within 100 milliseconds.

**REQ-PERF-005**: UI widget updates SHALL complete within 16 milliseconds (60 FPS).

### 3.2 Scalability Requirements

**REQ-SCALE-001**: The system SHALL support at least 500 unique quest definitions.

**REQ-SCALE-002**: The system SHALL support at least 2000 unique objective definitions.

**REQ-SCALE-003**: The system SHALL support at least 100 quest instances per player.

**REQ-SCALE-004**: The system SHALL support at least 32 players in multiplayer.

### 3.3 Reliability Requirements

**REQ-REL-001**: Quest state SHALL be auto-saved every 30 seconds (configurable).

**REQ-REL-002**: Quest state SHALL be saved on quest completion.

**REQ-REL-003**: Quest state SHALL be saved on quest failure.

**REQ-REL-004**: Quest state SHALL be restored on game load without data loss.

### 3.4 Usability Requirements

**REQ-USE-001**: The graph editor SHALL provide visual feedback for node connections.

**REQ-USE-002**: The graph editor SHALL support copy/paste of nodes.

**REQ-USE-003**: The graph editor SHALL support undo/redo operations.

**REQ-USE-004**: The UI widgets SHALL support keyboard and gamepad navigation.

**REQ-USE-005**: The UI widgets SHALL support localization.

### 3.5 Maintainability Requirements

**REQ-MAINT-001**: The system SHALL use data-driven quest definitions via DataTables.

**REQ-MAINT-002**: The system SHALL support hot-reloading of quest graphs.

**REQ-MAINT-003**: The system SHALL provide debug logging with configurable verbosity.

**REQ-MAINT-004**: The system SHALL provide debug visualization for quest state.

## 4. Constraints

**CONST-001**: The system SHALL be implemented in KAIN language.

**CONST-002**: The system SHALL target Unreal Engine 5.4+.

**CONST-003**: The system SHALL use KAIN stdlib functions where applicable.

**CONST-004**: The system SHALL generate zero TODOs, zero shortcuts, zero simplifications.

**CONST-005**: The system SHALL follow established patterns from DialogueForge and other Factory Part 2 plugins.

## 5. Acceptance Criteria

**AC-001**: All graph editor node types SHALL be implemented and functional.

**AC-002**: All graph runtime NodeData classes SHALL execute correctly.

**AC-003**: The subsystem SHALL manage quest state correctly with tick support.

**AC-004**: All actors SHALL replicate correctly in multiplayer.

**AC-005**: All components SHALL replicate correctly in multiplayer.

**AC-006**: All UI widgets SHALL display correctly and update in real-time.

**AC-007**: All Blueprint functions SHALL be callable from Blueprints.

**AC-008**: All networking RPCs SHALL function correctly in multiplayer.

**AC-009**: Quest state SHALL persist correctly across save/load cycles.

**AC-010**: The system SHALL achieve 8000-11000 lines of KAIN code.

## 6. Dependencies

### 6.1 UE5 Modules
- Core, CoreUObject, Engine
- Slate, SlateCore (UI widgets)
- UMG (widget integration)
- GameplayTags (quest tagging)
- AIModule (NPC integration)
- NavigationSystem (map markers)

### 6.2 KAIN Features
- Actors with RPCs (Server_, Client_, Multicast_)
- Components with replication
- Subsystems with tick
- Graph editor and runtime
- Slate widgets
- Blueprint integration
- Enums and structs
- DataTables

### 6.3 Optional Integrations
- DialogueForge (quest-driven dialogue)
- Inventory system (item rewards)
- Character system (XP and stats)
- Reputation system (faction rewards)

## 7. Glossary

- **Quest**: A task or mission that the player can accept, track, and complete
- **Objective**: A specific goal within a quest that must be completed
- **Reward**: Items, XP, gold, or other benefits given upon quest completion
- **Condition**: A requirement that must be met for a quest or objective to be available or complete
- **Quest Giver**: An NPC or object that offers quests to the player
- **Quest Instance**: A runtime instance of a quest with state tracking
- **Tracked Quest**: The quest currently displayed in the HUD tracker
- **Prerequisite**: A quest or condition that must be completed before a quest becomes available
- **Branch**: A decision point in a quest graph that leads to different paths
- **Parallel**: Multiple objectives that can be completed in any order
- **Sequence**: Multiple objectives that must be completed in order

## 8. References

- DialogueForge plugin (graph editor, runtime, subsystem, UI patterns)
- VoxelWorldEngine plugin (networking and replication patterns)
- DungeonArchitect plugin (graph node patterns)
- KAIN stdlib documentation
- UE5 Quest System best practices
