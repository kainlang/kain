# TitanGraph - Procedural Quest & Narrative Node Editor

**Professional-grade quest and dialogue system for Unreal Engine 5**

![Price](https://img.shields.io/badge/Price-$399--599-blue)
![UE5](https://img.shields.io/badge/UE5-5.4+-green)
![Performance](https://img.shields.io/badge/Performance-<0.1ms-brightgreen)

---

## 🎯 What is TitanGraph?

TitanGraph is a visual node-graph editor for designing complex quest systems and dialogue trees in Unreal Engine 5. Unlike Blueprint-based solutions, TitanGraph compiles your quest graphs to **optimized C++ state machines** for maximum runtime performance.

### Key Benefits

- ✅ **Visual Quest Design** - Drag-and-drop node editor with 12 node types
- ✅ **Zero Blueprint Overhead** - Compiles to pure C++ for maximum performance
- ✅ **Dialogue Trees** - Branching dialogue with conditions and choices
- ✅ **Multiplayer Ready** - Full replication support out of the box
- ✅ **Localization Support** - Export/import JSON for translation
- ✅ **GPU Acceleration** - Optional compute shader for 1000+ active quests
- ✅ **Save/Load System** - Automatic quest persistence
- ✅ **Professional UI** - Comprehensive editor with graph canvas, inspector, and toolbar

---

## 🚀 Quick Start

### 1. Build the Plugin

```bash
cd Factory/TitanGraph
Build5.4.bat
```

### 2. Install to Your Project

Copy the generated files to your UE5 project:

```
YourProject/Plugins/TitanGraph/
```

### 3. Enable the Plugin

1. Open your UE5 project
2. Go to **Edit → Plugins**
3. Search for "TitanGraph"
4. Check the box to enable
5. Restart the editor

### 4. Create Your First Quest

1. **Tools → TitanGraph → Create New Quest Graph**
2. Add nodes: **Ctrl+1** (Start), **Ctrl+2** (Objective), **Ctrl+3** (Dialogue)
3. Connect nodes by dragging execution pins
4. Configure properties in the Details panel
5. Compile: **Ctrl+B**
6. Test: **Ctrl+T**

---

## 📋 Features

### Quest System

- **Quest Lifecycle:** NotStarted → Active → Completed → TurnedIn
- **8 Objective Types:** Kill, Collect, Talk, Reach, Use, Escort, Defend, Custom
- **Quest Flags:** Track world state across quests
- **Time Limits:** Optional time-limited quests
- **Repeatable Quests:** With configurable cooldowns
- **Optional Objectives:** Non-critical objectives
- **Hidden Objectives:** Revealed when active

### Dialogue System

- **Visual Dialogue Trees:** Node-based dialogue editor
- **Branching Dialogue:** Conditions and choices
- **5 Choice Types:** Continue, Branch, AcceptQuest, DeclineQuest, CompleteQuest
- **Speaker Identification:** Multiple NPCs in one dialogue
- **Audio Integration:** Link audio files to dialogue lines
- **Animation Triggers:** Trigger animations during dialogue
- **Camera Control:** Set camera angles per dialogue node

### Node Types (12 Total)

1. **Quest Start** - Entry point for quest graph
2. **Quest Complete** - Quest completion node
3. **Objective** - Single objective with progress tracking
4. **Dialogue** - Dialogue with NPC
5. **Condition** - Branching logic (inventory, stats, flags)
6. **Action** - Execute action (give item, set flag, spawn actor)
7. **Branch** - If/else branching
8. **Delay** - Time-based delay
9. **Parallel** - Execute multiple branches simultaneously
10. **Sequence** - Execute nodes in sequence
11. **Random** - Random branch selection
12. **SubQuest** - Nested sub-quest

### Condition System

- **Inventory Checks:** HasItem, item count
- **Stat Comparisons:** GreaterThan, LessThan, Equal, etc.
- **Flag Checks:** HasFlag, flag value
- **Level Requirements:** Player level checks
- **Custom Conditions:** Blueprint-extensible

### Reward System

- **Experience Rewards:** XP with level scaling
- **Gold/Currency:** Configurable currency rewards
- **Item Rewards:** Multiple items with quantities
- **Reputation:** Faction reputation changes
- **Unlocks:** Feature/area unlocks
- **Custom Rewards:** Blueprint-extensible

### Editor UI

- **Graph Canvas:** Zoom, pan, node selection
- **Node Inspector:** Per-node property editing
- **Quest List:** View all quests (active, completed, failed)
- **Objective List:** Track objective progress
- **Dialogue Tree:** Preview dialogue flow
- **Reward Preview:** See quest rewards
- **Minimap:** Navigate large graphs
- **Statistics Panel:** Quest completion stats
- **Comprehensive Toolbar:** Quick actions and shortcuts

---

## 🎮 Usage Example

### Create a Simple Quest

```cpp
// 1. Create quest graph in editor
// [Start] → [Objective: Kill 5 Goblins] → [Complete]

// 2. Assign quest to NPC
AQuestGiver* NPC = SpawnActor<AQuestGiver>();
NPC->AddAvailableQuest(1); // Quest ID 1

// 3. Player interacts with NPC
void APlayerCharacter::InteractWithNPC(AQuestGiver* NPC)
{
    NPC->Server_Interact(this);
}

// 4. Track objective progress
AQuestManager* QuestManager = GetQuestManager();
QuestManager->Server_UpdateObjectiveProgress(1, 1, 5); // Quest 1, Objective 1, 5 kills

// 5. Turn in quest
QuestManager->Server_TurnInQuest(1);
```

### Blueprint API

```cpp
// Query quest state
bool IsQuestActive(int32 QuestId);
bool IsQuestCompleted(int32 QuestId);
int32 GetObjectiveProgress(int32 QuestId, int32 ObjectiveId);

// Quest lifecycle
void StartQuest(int32 QuestId);
void CompleteQuest(int32 QuestId);
void TurnInQuest(int32 QuestId);
void AbandonQuest(int32 QuestId);

// Utility
FLinearColor GetQuestDifficultyColor(int32 QuestLevel, int32 PlayerLevel);
FString FormatObjectiveText(EObjectiveType Type, FString Target, int32 Current, int32 Max);
```

---

## ⚡ Performance

| Metric | Target | Notes |
|--------|--------|-------|
| Quest Update Time | **< 0.1ms** | Per quest, 1000+ active quests |
| Graph Compilation | **< 1ms** | 100+ nodes |
| Memory per Quest | **< 10KB** | Active quest in memory |
| Blueprint Overhead | **0ms** | Zero Blueprint calls |
| GC Pressure | **0** | No allocations during updates |

### Optimization Techniques

- **Inline Condition Evaluation** - No Blueprint overhead
- **Direct Memory Access** - C++ arrays for objective tracking
- **Compile-Time Constant Folding** - Static branches optimized away
- **Jump Table Optimization** - Large graphs use jump tables
- **Dead Code Elimination** - Unreachable nodes removed
- **GPU Acceleration** - Optional compute shader for massive quest counts

---

## 🌐 Multiplayer Support

TitanGraph is **multiplayer-ready** out of the box:

- ✅ Server-authoritative quest state
- ✅ Proper replication (quest state, objectives, flags)
- ✅ Client-side prediction for UI responsiveness
- ✅ Bandwidth optimization (delta compression, batch updates)
- ✅ Proper RPC flow (Server_, Client_, Multicast_)

---

## 💾 Save/Load System

Automatic quest persistence:

- ✅ Active quests (state, progress, time)
- ✅ Completed quests (completion time)
- ✅ Failed quests (failure reason)
- ✅ Quest flags (world state)
- ✅ Dialogue history (last node per NPC)
- ✅ Binary or JSON format
- ✅ Save migration support

---

## 🌍 Localization

Export quests to JSON for translation:

```json
{
  "quest_id": 1,
  "quest_name": "The Lost Artifact",
  "quest_description": "Find the ancient artifact in the ruins.",
  "objectives": [
    {"id": 1, "text": "Search the ruins"},
    {"id": 2, "text": "Defeat the guardian"}
  ],
  "dialogue": [
    {
      "speaker": "Elder",
      "text": "Please, you must help us!",
      "choices": [
        {"text": "I'll help", "next": 2},
        {"text": "Not interested", "next": -1}
      ]
    }
  ]
}
```

---

## 🛠️ Debugging Tools

Built-in debugging functions:

```cpp
DebugPrintQuestState(QuestId);
DebugCompleteObjective(QuestId, ObjectiveId);
DebugFailQuest(QuestId);
DebugResetQuest(QuestId);
DebugListActiveQuests();
DebugValidateQuestGraph(QuestId);
DebugExportQuestGraph(QuestId, Path);
DebugImportQuestGraph(Path);
```

---

## 📊 Comparison to Alternatives

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

## 📚 Documentation

- **TECHNICAL.md** - Full technical documentation
- **API Reference** - Complete Blueprint API
- **Examples** - Sample quest graphs included
- **Video Tutorials** - Coming soon

---

## 💰 Pricing

**$399-599** (Professional License)

- ✅ Commercial use
- ✅ Royalty-free for shipped games
- ✅ Free updates for major UE5 versions
- ✅ Email support
- ✅ Source code included

---

## 🎯 Target Audience

- **RPG Developers** - Complex quest systems
- **Narrative Designers** - Branching dialogue trees
- **Quest Designers** - Visual quest design
- **Indie Studios** - Professional tools at affordable price
- **AAA Studios** - Performance-critical quest systems

---

## 🏆 Why Choose TitanGraph?

### vs. Blueprint-Based Solutions
- **10-100x faster** - C++ state machine vs Blueprint execution
- **Zero GC pressure** - No Blueprint allocations
- **Scalable** - Supports 1000+ active quests

### vs. Custom C++ Solutions
- **Visual editor** - No code required for quest design
- **Faster iteration** - Compile in < 1ms
- **Less error-prone** - Visual validation

### vs. Other Quest Plugins
- **Dialogue trees** - Integrated dialogue system
- **GPU acceleration** - Optional compute shader
- **State machine compilation** - Optimized runtime

---

## 📦 What's Included

- ✅ TitanGraph plugin source code (1450+ lines of KAIN)
- ✅ Generated C++ code (20+ files, 8000+ lines)
- ✅ Visual node editor
- ✅ Comprehensive UI (graph canvas, inspector, toolbar)
- ✅ Sample quest graphs
- ✅ Full documentation
- ✅ Blueprint API
- ✅ Debugging tools
- ✅ Build scripts

---

## 🔧 System Requirements

- **Unreal Engine:** 5.4 or later
- **Platform:** Windows, Mac, Linux
- **Compiler:** MSVC 2022, Clang, GCC
- **RAM:** 8GB minimum, 16GB recommended
- **Disk Space:** 500MB for plugin

---

## 📞 Support

- **Email:** support@titangraph.com
- **Discord:** discord.gg/titangraph
- **Documentation:** docs.titangraph.com
- **GitHub:** github.com/titangraph/issues

---

## 📄 License

Commercial license included with purchase. Royalty-free for shipped games.

---

## 🙏 Credits

Built with **KAIN** - The LLM-first game development language.

---

## 🚀 Get Started Today!

```bash
cd Factory/TitanGraph
Build5.4.bat
```

**Transform your quest design workflow with TitanGraph!**
