# QuestMaster — Comprehensive Quest System for UE5

**Version:** 1.0.0  
**Engine:** Unreal Engine 5.4+  
**Language:** KAIN  
**Category:** Gameplay Systems  

## Overview

QuestMaster is a production-ready quest system plugin that provides complete quest authoring, tracking, execution, and management capabilities for Unreal Engine 5. Built with KAIN, it features a visual graph editor for quest flow design, robust runtime execution, multiplayer networking, comprehensive UI widgets, and full Blueprint integration.

## Key Features

### 🎨 Visual Quest Authoring
- **15+ Node Types** for flexible quest design
- **Graph Editor** with intuitive visual workflow
- **Real-time Preview** of quest flow
- **Comment Nodes** for designer documentation

### ⚙️ Robust Runtime System
- **Graph Runtime** with 15+ NodeData execution classes
- **State Management** via World Subsystem with tick support
- **Condition Evaluation** engine with 15+ condition types
- **Timer Management** with countdown/countup modes
- **Parallel & Sequential** objective support

### 🌐 Multiplayer Ready
- **Full Replication** of quest state across clients
- **RPC Support** for Server/Client/Multicast operations
- **Network-optimized** state synchronization
- **Authority Validation** on server

### 🎮 Blueprint Integration
- **60+ Blueprint Functions** for quest control
- **Blueprint Events** for quest state changes
- **Designer-Friendly** API with clear naming
- **No C++ Required** for quest implementation

### 🖥️ Comprehensive UI
- **8 Slate Widgets** for quest display
- **Quest Log** with filtering and sorting
- **Quest Tracker** with real-time updates
- **Notifications** with fade in/out animations
- **Map Markers** for objective locations
- **Debug Widget** for development

### 💾 Persistence & Save System
- **Auto-Save** with configurable intervals
- **Manual Save/Load** support
- **Persistent Flags** across sessions
- **Global Variables** for quest state

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     UI Layer (8 Slate Widgets)               │
│  QuestLog | Tracker | Notifications | Details | Objectives   │
│  Rewards | MapMarkers | Debug                                │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                   Blueprint API (60+ Functions)              │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                   Actor Layer (4 Actors)                     │
│  QuestManager | QuestGiver | QuestObjective | QuestTrigger  │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                   Component Layer (3 Components)             │
│  QuestTracker | QuestObjective | QuestReward                │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                   Subsystem Layer (World Subsystem)          │
│  QuestManagerSubsystem - 50+ functions, tick support        │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                   Graph Runtime (15+ NodeData)               │
│  Quest execution, condition evaluation, objective tracking   │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                   Graph Editor (15+ Node Types)              │
│  Visual quest authoring with UEdGraph integration           │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                   Data Layer (12 Enums, 35+ Structs)         │
│  Type-safe quest definitions, 4 DataTables, 50+ helpers     │
└─────────────────────────────────────────────────────────────┘
```

## Graph Editor Node Types

### Core Nodes
1. **StartQuestNode** - Quest entry point with metadata
2. **ObjectiveNode** - Define objectives with target counts
3. **CompleteQuestNode** - Quest completion with rewards
4. **FailQuestNode** - Quest failure with reason

### Flow Control
5. **BranchNode** - Multi-condition branching (AND/OR logic)
6. **ConditionNode** - Single condition evaluation
7. **ParallelNode** - Up to 4 simultaneous branches
8. **SequenceNode** - Ordered objective completion

### Gameplay
9. **RewardNode** - XP, gold, items, reputation
10. **TimerNode** - Countdown/countup with UI
11. **EventNode** - Trigger gameplay events
12. **DialogueNode** - DialogueForge integration

### World Interaction
13. **TeleportNode** - Player teleportation with fade
14. **SpawnNode** - Actor spawning with parameters
15. **CommentNode** - Designer notes and documentation

## Data Structures

### Enums (12)
- `QuestState` - Inactive, Active, Completed, Failed, Abandoned
- `QuestPriority` - Low, Normal, High, Critical
- `QuestCategory` - Main, Side, Daily, Repeatable, Hidden, Tutorial, World, Faction
- `ObjectiveType` - Kill, Collect, Interact, Reach, Escort, Defend, Discover, Craft, Use, Talk, Custom
- `ConditionType` - 15+ condition types for quest logic
- `RewardType` - XP, Gold, Item, Reputation, Unlock, Custom, Skill, Achievement
- `QuestNotificationType` - Started, Completed, Failed, ObjectiveCompleted, etc.
- `QuestFilterMode` - All, Active, Completed, Failed, Available, Tracked
- `QuestSortMode` - Name, Priority, Progress, Level, Category, TimeStarted, TimeRemaining
- `QuestMarkerType` - Objective, QuestGiver, TurnIn, Area, Waypoint, Optional
- `TimerMode` - Countdown, Countup, None
- `ParallelMode` - WaitForAll, WaitForAny, WaitForCount

### Structs (35+)
- **QuestData** - Quest definition with metadata
- **ObjectiveData** - Objective definition with type and target
- **QuestReward** - XP, gold, items, reputation rewards
- **QuestCondition** - Condition evaluation data
- **QuestInstance** - Runtime quest state
- **ObjectiveInstance** - Runtime objective state
- **QuestVariable** - Global/persistent variables
- **QuestTimer** - Timer state and configuration
- **QuestMarker** - Map marker data
- **QuestProgress** - Progress calculation data
- **QuestNotification** - Notification display data
- **QuestSaveData** - Persistence data
- **QuestDebugInfo** - Debug display data
- ...and 20+ more specialized structs

### DataTables (4)
- **QuestDataTable** - Define quests with all metadata
- **ObjectiveDataTable** - Define objectives with types and targets
- **RewardDataTable** - Define rewards with types and quantities
- **QuestCategoryDataTable** - Define categories with icons and colors

## Actors

### QuestManagerActor
Centralized quest coordination with networking support.

**Replicated State:**
- `active_quest_count: Int`
- `tracked_quest_id: String`

**RPCs:**
- `Server_StartQuest(quest_id, player_id)`
- `Server_CompleteQuest(quest_id)`
- `Server_FailQuest(quest_id, reason)`
- `Server_AbandonQuest(quest_id)`
- `Server_UpdateObjective(quest_id, objective_id, count)`
- `Multicast_QuestStateChanged(quest_id, new_state)`
- `Multicast_ObjectiveUpdated(quest_id, objective_id, count)`

### QuestGiverActor
NPC or object that offers quests to players.

**Features:**
- Available quest management
- Interaction system
- Quest acceptance/turn-in
- Repeatable quest support

### QuestObjectiveActor
Objective tracking with overlap/interaction triggers.

**Replicated State:**
- `quest_id: String`
- `objective_id: String`
- `target_count: Int`
- `current_count: Int`

**Features:**
- Auto-completion on target reached
- Overlap/interaction triggers
- Progress tracking
- Marker display

### QuestTriggerActor
Event triggering on overlap, interaction, or timer.

**Features:**
- Multiple trigger types (Overlap, Interact, Timer)
- One-time or repeatable
- Quest requirement checking
- Event broadcasting

## Components

### QuestTrackerComponent
Tracks quest progress for an actor (typically player).

**Replicated Fields:**
- `active_quests: Array<String>`
- `tracked_quest_id: String`
- `quest_progress: Array<Float>`

**Functions:**
- `add_quest(quest_id)` - Add quest to tracker
- `remove_quest(quest_id)` - Remove quest from tracker
- `update_progress(quest_id, progress)` - Update progress percentage
- `track_quest(quest_id)` - Set as tracked quest
- `untrack_quest()` - Clear tracked quest

### QuestObjectiveComponent
Objective completion logic with replication.

**Replicated Fields:**
- `objective_id: String`
- `current_count: Int`
- `target_count: Int`
- `completed: Bool`

**Functions:**
- `increment_count(amount)` - Increment objective count
- `set_count(count)` - Set objective count directly
- `reset()` - Reset objective to initial state
- `get_progress()` - Get completion percentage

### QuestRewardComponent
Reward distribution and management.

**Fields:**
- `reward_xp: Int`
- `reward_gold: Int`
- `reward_items: Array<ItemReward>`
- `reward_reputation: Array<ReputationReward>`

**Functions:**
- `apply_rewards(player_id)` - Apply all rewards
- `give_xp_reward(player_id)` - Give XP only
- `give_gold_reward(player_id)` - Give gold only
- `give_item_rewards(player_id)` - Give items only

## UI Widgets

### QuestLogWidget
Full quest log with filtering and sorting.

**Features:**
- Display all active/completed/failed quests
- Filter by state, category, priority
- Sort by name, priority, progress
- Quest selection for details view

### QuestTrackerWidget
HUD tracker for currently tracked quest.

**Features:**
- Display tracked quest name
- Show objectives with progress bars
- Real-time updates
- Compact mode option

### QuestNotificationWidget
Toast notifications for quest events.

**Features:**
- Quest started/completed/failed notifications
- Objective completed notifications
- Fade in/out animations
- Configurable display duration

### QuestDetailWidget
Detailed quest information display.

**Features:**
- Quest description and lore
- Objective list with progress
- Reward display
- Category and priority indicators

### ObjectiveListWidget
Objective list with completion state.

**Features:**
- Display all objectives for a quest
- Progress bars for each objective
- Completion state indicators
- Optional/hidden objective filtering

### RewardDisplayWidget
Reward preview and display.

**Features:**
- XP and gold amounts
- Item rewards with icons
- Reputation rewards
- Horizontal/vertical layout modes

### QuestMapMarkerWidget
Map markers for quest objectives.

**Features:**
- Location-based markers
- Type-specific icons and colors
- Distance display
- Show/hide functionality

### QuestDebugWidget
Development debug information.

**Features:**
- Active/completed/failed quest counts
- Tracked quest display
- Global variables and flags
- Real-time refresh

## Subsystem API

### QuestManagerSubsystem
World subsystem with tick support for quest management.

**Quest Management:**
- `start_quest(quest_id, player_id) -> Int`
- `complete_quest(quest_id) -> Bool`
- `fail_quest(quest_id, reason) -> Bool`
- `abandon_quest(quest_id) -> Bool`

**Objective Management:**
- `update_objective(quest_id, objective_id, count) -> Bool`
- `complete_objective(quest_id, objective_id) -> Bool`
- `reset_objective(quest_id, objective_id) -> Bool`

**Quest Tracking:**
- `track_quest(quest_id) -> Bool`
- `untrack_quest(quest_id) -> Bool`
- `get_tracked_quest() -> String`

**State Queries:**
- `get_active_quests() -> Array<QuestInstance>`
- `get_completed_quests() -> Array<QuestInstance>`
- `get_failed_quests() -> Array<QuestInstance>`
- `get_available_quests(player_level) -> Array<String>`
- `is_quest_active(quest_id) -> Bool`
- `is_quest_complete(quest_id) -> Bool`

**Progress Queries:**
- `get_quest_progress(quest_id) -> Float`
- `get_objective_progress(quest_id, objective_id) -> Float`
- `get_active_quest_count() -> Int`

**Variable Management:**
- `set_global_variable(name, value) -> Bool`
- `get_global_variable(name) -> Float`
- `set_global_variable_int(name, value) -> Bool`
- `get_global_variable_int(name) -> Int`

**Flag Management:**
- `set_persistent_flag(flag_name, value) -> Bool`
- `check_persistent_flag(flag_name) -> Bool`
- `clear_persistent_flag(flag_name) -> Bool`

**Persistence:**
- `save_quest_state() -> Bool`
- `load_quest_state() -> Bool`
- `auto_save() -> Bool`

## Blueprint Integration

All subsystem functions are exposed to Blueprints with the `_from_subsystem` suffix:
- `start_quest_from_subsystem(quest_id, player_id)`
- `complete_quest_from_subsystem(quest_id)`
- `get_active_quests_from_subsystem()`
- `get_quest_progress_from_subsystem(quest_id)`
- ...and 50+ more functions

Helper functions are exposed with the `_bp` suffix:
- `create_quest_instance_bp(quest_id, player_id, instance_id)`
- `calculate_quest_progress_bp(quest_instance)`
- `evaluate_condition_bp(condition, context_value)`
- `apply_quest_rewards_bp(rewards, player_id)`
- ...and more

## Usage Examples

### Starting a Quest (Blueprint)

```
// Get the quest subsystem
QuestSubsystem = Get Quest Subsystem

// Start a quest
InstanceID = Start Quest From Subsystem(QuestSubsystem, "quest_main_001", PlayerID)

// Track the quest
Track Quest From Subsystem(QuestSubsystem, "quest_main_001")
```

### Updating an Objective (Blueprint)

```
// Update objective count
UpdateObjectiveFromSubsystem(QuestSubsystem, "quest_main_001", "objective_kill_enemies", 5)

// Check if objective is complete
Progress = Get Objective Progress From Subsystem(QuestSubsystem, "quest_main_001", "objective_kill_enemies")
```

### Creating a Quest Giver (Blueprint)

```
// Spawn QuestGiverActor
QuestGiver = Spawn Actor(QuestGiverActor, Location, Rotation)

// Add available quests
Add Available Quest(QuestGiver, "quest_side_001")
Add Available Quest(QuestGiver, "quest_side_002")

// On player interaction
Accept Quest From Player(QuestGiver, "quest_side_001", PlayerID)
```

## Performance Characteristics

- **Subsystem Tick Rate:** 0.1s (10 Hz) - configurable
- **Max Active Quests:** 50 per player - configurable
- **Auto-Save Interval:** 30 seconds - configurable
- **UI Update Rate:** Real-time on state change
- **Network Replication:** Optimized with @replicated fields

## Networking

### Replication Strategy
- **@replicated** fields on actors and components
- **Server authority** for quest state changes
- **Multicast RPCs** for state synchronization
- **Client prediction** for UI updates

### RPC Patterns
- **Server_*** - Client to server requests
- **Client_*** - Server to specific client
- **Multicast_*** - Server to all clients

## Integration with Other Systems

### DialogueForge Integration
- **DialogueNode** in quest graphs
- Pass quest variables to dialogue
- Wait for dialogue completion
- Skip if already completed

### Inventory System Integration
- **ItemReward** struct for item rewards
- Condition checks for items in inventory
- Item collection objectives

### Character System Integration
- **XP rewards** with level requirements
- **Stat-based conditions** for quest availability
- **Reputation rewards** for faction systems

## File Structure

```
QuestMaster/
├── src/
│   ├── quest_data_structures.kn    (1,500 LOC)
│   ├── quest_graph_editor.kn       (600 LOC)
│   ├── quest_graph_runtime.kn      (1,200 LOC)
│   ├── quest_subsystem.kn          (1,200 LOC)
│   ├── quest_actors.kn             (1,000 LOC)
│   ├── quest_components.kn         (600 LOC)
│   ├── quest_ui_widgets.kn         (1,400 LOC)
│   └── quest_blueprint_library.kn  (500 LOC)
├── KAIN.toml
├── README.md
├── IMPLEMENTATION_COMPLETE.md
├── BUILD_READY.md
├── requirements.md
├── design.md
├── tasks.md
└── feature_checklist.md
```

**Total Lines of Code:** ~8,000 LOC

## Building the Plugin

```bash
# Navigate to plugin directory
cd FactoryPart2/plugins/QuestMaster

# Build with KAIN compiler
kain build --ue5

# Output will be in Generated/ directory
```

## Testing

### Unit Testing
- Test each NodeData.execute_node() function
- Test condition evaluation logic
- Test objective tracking logic
- Test reward application logic

### Integration Testing
- Test quest start-to-completion flow
- Test objective update flow
- Test quest failure flow
- Test save/load cycle

### Networking Testing
- Test RPC functionality
- Test state replication
- Test multiplayer quest sharing
- Test client-server synchronization

## Known Limitations

- Maximum 50 active quests per player (configurable)
- Maximum 4 parallel branches in ParallelNode
- Maximum 4 objectives in SequenceNode
- Timer precision limited to subsystem tick rate (0.1s default)

## Future Enhancements

- Quest chains with automatic progression
- Dynamic quest generation
- Quest sharing in multiplayer
- Quest leaderboards
- Quest analytics and telemetry
- Quest localization tools
- Quest replay system
- Quest difficulty scaling

## Credits

**Plugin:** QuestMaster  
**Version:** 1.0.0  
**Language:** KAIN  
**Target:** Unreal Engine 5.4+  
**License:** Factory Part 2 Assembly Line  

## Support

For issues, questions, or feature requests, please refer to the Factory Part 2 documentation.

---

**QuestMaster** - Professional quest system for Unreal Engine 5, built with KAIN.
