# TitanGraph - Technical Documentation

## Overview

TitanGraph is a professional-grade Procedural Quest & Narrative Node Editor for Unreal Engine 5. It provides a visual node-graph editor for designing complex quest systems and dialogue trees, then compiles them to optimized C++ state machines for runtime execution.

**Price Point:** $399-599  
**Target Audience:** RPG developers, narrative designers, quest designers  
**Performance:** < 0.1ms per quest update, supports 1000+ active quests

---

## Core Features

### 1. Visual Node Graph Editor
- **12 Node Types:**
  - Quest Start - Entry point for quest graph
  - Quest Complete - Quest completion node
  - Objective - Single objective with progress tracking
  - Dialogue - Dialogue with NPC (speaker, text, choices)
  - Condition - Branching logic (inventory, stats, flags)
  - Action - Execute action (give item, set flag, spawn actor)
  - Branch - If/else branching
  - Delay - Time-based delay before next node
  - Parallel - Execute multiple branches simultaneously
  - Sequence - Execute nodes in sequence
  - Random - Random branch selection
  - SubQuest - Nested sub-quest

### 2. State Machine Compilation
- Compiles quest graphs to flat C++ arrays
- Zero Blueprint overhead
- Inline condition evaluation
- Direct C++ objective tracking
- Optimized switch statement execution

### 3. Quest System
- Quest lifecycle management (NotStarted → Active → Completed → TurnedIn)
- Objective tracking with progress (8 objective types)
- Quest flags for world state
- Time-limited quests
- Repeatable quests with cooldowns
- Optional objectives
- Hidden objectives (revealed when active)

### 4. Dialogue System
- Visual dialogue tree editor
- Branching dialogue with conditions
- Multiple choice types (Continue, Branch, AcceptQuest, DeclineQuest, CompleteQuest)
- Speaker identification
- Audio integration
- Animation triggers
- Camera angle control

### 5. Condition System
- Inventory checks (HasItem)
- Stat comparisons (GreaterThan, LessThan, etc.)
- Flag checks (HasFlag)
- Level requirements
- Custom Blueprint conditions

### 6. Reward System
- Experience rewards
- Gold/currency rewards
- Item rewards
- Reputation rewards
- Feature/area unlocks
- Custom Blueprint rewards

### 7. Multiplayer Support
- Full replication support
- Server-authoritative quest state
- Client-side prediction for UI
- Bandwidth optimization (delta compression, batch updates)
- Proper RPC flow (Server_, Client_, Multicast_)

### 8. Save/Load System
- Binary or JSON save format
- Quest state persistence
- Objective progress persistence
- Quest flag persistence
- Dialogue history persistence
- Save migration support

### 9. Localization Support
- Export to JSON for translation
- Import from JSON
- Dialogue text externalization
- Objective text externalization

### 10. Editor UI
- Graph canvas with zoom/pan
- Node inspector panel
- Quest list widget
- Objective list widget
- Dialogue tree widget
- Reward preview widget
- Minimap widget
- Statistics panel
- Comprehensive toolbar
- Details panel with per-node properties

---

## Architecture

### UEdGraph Integration

TitanGraph generates the following C++ classes for UE5 graph integration:

```cpp
// Base node class
class UQuestGraphNode : public UEdGraphNode { ... };

// Specialized node classes
class UQuestGraphNode_Start : public UQuestGraphNode { ... };
class UQuestGraphNode_Complete : public UQuestGraphNode { ... };
class UQuestGraphNode_Objective : public UQuestGraphNode { ... };
class UQuestGraphNode_Dialogue : public UQuestGraphNode { ... };
class UQuestGraphNode_Condition : public UQuestGraphNode { ... };
class UQuestGraphNode_Action : public UQuestGraphNode { ... };
class UQuestGraphNode_Branch : public UQuestGraphNode { ... };
class UQuestGraphNode_Delay : public UQuestGraphNode { ... };

// Graph schema for connection rules
class UQuestGraphSchema : public UEdGraphSchema { ... };
```

### Pin Types

- **Execution Pins (White):** Quest flow control
- **Data Pins (Colored):** Quest data (objectives, rewards, conditions)

### State Machine Compilation

**Input:** Quest graph with nodes and connections  
**Output:** Optimized C++ switch statement

**Example:**

```
Quest Graph:
  [Start] → [Objective 1] → [Objective 2] → [Complete]

Compiled C++ State Machine:
  switch (current_node_id) {
      case 0: // Start
          current_node_id = 1;
          break;
      case 1: // Objective 1
          if (objective_1_complete) {
              current_node_id = 2;
          }
          break;
      case 2: // Objective 2
          if (objective_2_complete) {
              current_node_id = 3;
          }
          break;
      case 3: // Complete
          CompleteQuest();
          break;
  }
```

### Optimization Techniques

1. **Inline Condition Evaluation** - No Blueprint overhead
2. **Direct Memory Access** - Objective tracking in C++ arrays
3. **Compile-Time Constant Folding** - Static branches optimized away
4. **Jump Table Optimization** - Large graphs use jump tables
5. **Dead Code Elimination** - Unreachable nodes removed
6. **GPU Acceleration** - Optional compute shader for 1000+ quests

---

## Performance Targets

| Metric | Target | Notes |
|--------|--------|-------|
| Quest Update Time | < 0.1ms | Per quest, 1000+ active quests |
| Graph Compilation | < 1ms | 100+ nodes |
| Memory per Quest | < 10KB | Active quest in memory |
| Blueprint Overhead | 0ms | Zero Blueprint calls |
| GC Pressure | 0 | No allocations during updates |

---

## Data Structures

### ActiveQuest
```kain
struct ActiveQuest:
    quest_id: Int
    quest_state: QuestState
    start_time: Float
    objectives: Array<ObjectiveProgress>
    current_node_id: Int
    visited_nodes: Array<Int>
```

### ObjectiveProgress
```kain
struct ObjectiveProgress:
    objective_id: Int
    current_count: Int
    target_count: Int
    is_complete: Bool
    is_optional: Bool
```

### DialogueChoice
```kain
struct DialogueChoice:
    choice_id: Int
    choice_text: String
    choice_type: DialogueChoiceType
    next_node_id: Int
    required_item_id: Int
    required_flag: String
```

### QuestGraphNode
```kain
struct QuestGraphNode:
    node_id: Int
    node_type: QuestNodeType
    node_data: String
    output_pins: Array<Int>
    input_pins: Array<Int>
```

---

## Networking

### Replication Strategy

- **Quest State:** Replicated to all clients
- **Objective Progress:** Replicated to quest owner only
- **Dialogue State:** Local to client (no replication)
- **Quest Flags:** Replicated to all clients (world state)

### RPC Flow

1. Client interacts with quest giver
2. Server validates interaction
3. Server starts quest, replicates to client
4. Client updates objective progress locally
5. Client sends progress to server
6. Server validates and replicates to all clients
7. Server checks completion, grants rewards
8. Server replicates completion to all clients

### Bandwidth Optimization

- Delta compression for objective progress
- Batch updates for multiple objectives
- Lazy replication for non-critical data
- Client-side prediction for UI responsiveness

---

## JSON Export Format

```json
{
  "quest_id": 1,
  "quest_name": "The Lost Artifact",
  "quest_description": "Find the ancient artifact in the ruins.",
  "objectives": [
    {
      "id": 1,
      "text": "Search the ruins",
      "type": "ReachLocation"
    },
    {
      "id": 2,
      "text": "Defeat the guardian",
      "type": "KillTarget"
    }
  ],
  "dialogue": [
    {
      "id": 1,
      "speaker": "Elder",
      "text": "Please, you must help us!",
      "choices": [
        {"text": "I'll help", "next": 2},
        {"text": "Not interested", "next": -1}
      ]
    }
  ],
  "rewards": [
    {"type": "Experience", "value": 500},
    {"type": "Gold", "value": 100},
    {"type": "Item", "item_id": 42, "quantity": 1}
  ]
}
```

---

## Blueprint API

### Quest Manager

```cpp
// Query quest state
UFUNCTION(BlueprintCallable)
bool IsQuestActive(int32 QuestId);

UFUNCTION(BlueprintCallable)
bool IsQuestCompleted(int32 QuestId);

UFUNCTION(BlueprintCallable)
int32 GetObjectiveProgress(int32 QuestId, int32 ObjectiveId);

UFUNCTION(BlueprintCallable)
int32 GetActiveQuestCount();

UFUNCTION(BlueprintCallable)
int32 GetCompletedQuestCount();

UFUNCTION(BlueprintPure)
bool IsInDialogue();

UFUNCTION(BlueprintCallable)
bool HasQuestFlag(const FString& FlagName);

// Quest lifecycle
UFUNCTION(BlueprintCallable)
void StartQuest(int32 QuestId);

UFUNCTION(BlueprintCallable)
void CompleteQuest(int32 QuestId);

UFUNCTION(BlueprintCallable)
void TurnInQuest(int32 QuestId);

UFUNCTION(BlueprintCallable)
void AbandonQuest(int32 QuestId);

UFUNCTION(BlueprintCallable)
void FailQuest(int32 QuestId);
```

### Quest Giver

```cpp
UFUNCTION(BlueprintCallable)
void AddAvailableQuest(int32 QuestId);

UFUNCTION(BlueprintCallable)
void RemoveAvailableQuest(int32 QuestId);

UFUNCTION(BlueprintPure)
bool HasAvailableQuests();
```

### Utility Functions

```cpp
UFUNCTION(BlueprintCallable)
int32 CalculateQuestRewardXP(int32 BaseXP, int32 PlayerLevel, int32 QuestLevel);

UFUNCTION(BlueprintCallable)
FLinearColor GetQuestDifficultyColor(int32 QuestLevel, int32 PlayerLevel);

UFUNCTION(BlueprintCallable)
FString FormatObjectiveText(EObjectiveType ObjectiveType, const FString& TargetName, int32 Current, int32 Target);

UFUNCTION(BlueprintCallable)
FString GetQuestStateDisplayName(EQuestState QuestState);

UFUNCTION(BlueprintCallable)
FString GetRewardTypeIcon(ERewardType RewardType);

UFUNCTION(BlueprintPure)
bool IsObjectiveComplete(int32 Current, int32 Target);

UFUNCTION(BlueprintCallable)
bool ValidateQuestGraph(const TArray<FQuestGraphNode>& GraphNodes);

UFUNCTION(BlueprintCallable)
FString ExportQuestToJSON(int32 QuestId);

UFUNCTION(BlueprintCallable)
bool ImportQuestFromJSON(const FString& JsonData);
```

---

## Debugging Tools

```cpp
UFUNCTION(BlueprintCallable)
void DebugPrintQuestState(int32 QuestId);

UFUNCTION(BlueprintCallable)
void DebugCompleteObjective(int32 QuestId, int32 ObjectiveId);

UFUNCTION(BlueprintCallable)
void DebugFailQuest(int32 QuestId);

UFUNCTION(BlueprintCallable)
void DebugResetQuest(int32 QuestId);

UFUNCTION(BlueprintCallable)
TArray<int32> DebugListActiveQuests();

UFUNCTION(BlueprintCallable)
bool DebugValidateQuestGraph(int32 QuestId);

UFUNCTION(BlueprintCallable)
void DebugExportQuestGraph(int32 QuestId, const FString& Path);

UFUNCTION(BlueprintCallable)
int32 DebugImportQuestGraph(const FString& Path);
```

---

## Usage Example

### 1. Create Quest Graph

1. Open TitanGraph editor: **Tools → TitanGraph → Create New Quest Graph**
2. Add nodes: **Ctrl+1** (Start), **Ctrl+2** (Objective), **Ctrl+3** (Dialogue)
3. Connect nodes by dragging execution pins
4. Configure node properties in Details panel
5. Compile graph: **Ctrl+B**
6. Test graph: **Ctrl+T**

### 2. Assign Quest to NPC

```cpp
// In Blueprint or C++
AQuestGiver* NPC = SpawnActor<AQuestGiver>();
NPC->AddAvailableQuest(1); // Quest ID 1
```

### 3. Player Interaction

```cpp
// Player interacts with NPC
void APlayerCharacter::InteractWithNPC(AQuestGiver* NPC)
{
    NPC->Server_Interact(this);
}
```

### 4. Track Objective Progress

```cpp
// Update objective progress
AQuestManager* QuestManager = GetQuestManager();
QuestManager->Server_UpdateObjectiveProgress(1, 1, 5); // Quest 1, Objective 1, Progress 5
```

### 5. Complete Quest

```cpp
// Turn in quest
QuestManager->Server_TurnInQuest(1);
```

---

## Build Instructions

1. Run `Build5.4.bat` to compile the plugin
2. Copy generated files to your UE5 project's `Plugins/TitanGraph/` folder
3. Regenerate Visual Studio project files
4. Compile in Visual Studio or Rider
5. Enable plugin in UE5 Editor (Edit → Plugins → TitanGraph)
6. Restart editor

---

## File Structure

```
TitanGraph/
├── titangraph.kn           # Main KAIN source file (1450+ lines)
├── Build5.4.bat            # Build script
├── TECHNICAL.md            # This file
├── Source/                 # Generated C++ files (after build)
│   ├── TitanGraph.h
│   ├── TitanGraph.cpp
│   ├── QuestManager.h
│   ├── QuestManager.cpp
│   ├── QuestGiver.h
│   ├── QuestGiver.cpp
│   ├── QuestGraphNode.h
│   ├── QuestGraphNode.cpp
│   ├── QuestGraphSchema.h
│   ├── QuestGraphSchema.cpp
│   └── ...
├── TitanGraph.uplugin      # Plugin descriptor (generated)
└── TitanGraph.Build.cs     # Build configuration (generated)
```

---

## Comparison to Alternatives

| Feature | TitanGraph | Blueprint | Dialogue Plugin | Quest Plugin |
|---------|-----------|-----------|-----------------|--------------|
| Visual Editor | ✅ | ✅ | ✅ | ❌ |
| C++ Performance | ✅ | ❌ | ❌ | ✅ |
| Dialogue Trees | ✅ | ❌ | ✅ | ❌ |
| Quest System | ✅ | ❌ | ❌ | ✅ |
| Multiplayer | ✅ | ⚠️ | ❌ | ⚠️ |
| Localization | ✅ | ❌ | ✅ | ❌ |
| State Machine | ✅ | ❌ | ❌ | ❌ |
| GPU Acceleration | ✅ | ❌ | ❌ | ❌ |
| Price | $399-599 | Free | $99-199 | $199-399 |

---

## Support & Documentation

- **Documentation:** Full API reference included
- **Examples:** Sample quest graphs included
- **Support:** Email support for technical issues
- **Updates:** Free updates for major UE5 versions

---

## License

Commercial license included with purchase. Royalty-free for shipped games.

---

## Credits

Built with KAIN - The LLM-first game development language.
