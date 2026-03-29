# DialogueForge

**Full dialogue system with branching conversations, conditions, graph editor, voice line playback, and GAS integration**

## Overview

DialogueForge is a comprehensive narrative systems plugin for Unreal Engine 5 that provides a complete dialogue authoring and runtime system. It features a visual graph editor with 28 node types, branching dialogue with conditions, multiple speakers with voice line playback, choice systems with consequences, and full Gameplay Ability System integration.

## Features

### Graph Editor (28 Node Types)
- **SpeakerNode** - Display dialogue from a speaker with emotion, camera angle, and animation
- **ChoiceNode** - Present player choices with up to 4 options
- **ConditionNode** - Branch based on quest state, inventory, stats, or custom conditions
- **BranchNode** - Multi-condition branching with AND/OR logic
- **EventNode** - Trigger gameplay events from dialogue
- **QuestNode** - Start, complete, or fail quests
- **RandomNode** - Weighted random branching
- **DelayNode** - Timed pauses in dialogue
- **AnimationNode** - Play character animations
- **CameraNode** - Control camera angles and positioning
- **SoundNode** - Play sound effects
- **VariableNode** - Set dialogue variables
- **JumpNode** - Jump to other nodes
- **EndNode** - End dialogue with custom events
- **StartNode** - Entry point for dialogue graphs
- **CommentNode** - Add notes to graphs
- **SkillCheckNode** - D&D-style skill checks with critical success/failure
- **SubDialogueNode** - Call other dialogue graphs
- **ParallelNode** - Execute multiple branches simultaneously
- **InventoryCheckNode** - Check for items with optional consumption
- **ReputationCheckNode** - Check faction reputation
- **TimeCheckNode** - Check time of day, day of week, weather
- **GiveItemNode** - Give items to player
- **GiveXPNode** - Award experience points
- **ModifyReputationNode** - Change faction reputation
- **TeleportNode** - Teleport player
- **SpawnActorNode** - Spawn actors during dialogue
- **DestroyActorNode** - Destroy actors during dialogue
- **SetFlagNode** - Set persistent flags
- **CheckFlagNode** - Check persistent flags

### Graph Runtime
- Full NodeData execution system
- GraphInstance state management
- Variable tracking and persistence
- Visited node history
- Choice history tracking
- Condition evaluation engine
- Random node selection with weights
- Skill check system with dice rolling

### Actors
- **DialogueManagerActor** - Central dialogue coordinator with RPC support
- **DialogueSpeakerActor** - Speaker with voice line playback, emotion system, gestures, lip sync
- **DialogueTriggerActor** - Trigger dialogues on overlap or interaction

### Subsystem
- **DialogueManagerSubsystem** - World subsystem with tick support
- Active dialogue tracking (up to 10 concurrent)
- Speaker registration system
- Graph loading and caching
- Global variable management
- Persistent flag system
- Quest trigger tracking
- Dialogue history logging
- Auto-save functionality
- Debug logging

### UI Widgets (8 Slate Widgets)
- **DialogueWidget** - Main dialogue display with text scrolling
- **ChoiceListWidget** - Choice selection with keyboard/gamepad navigation
- **SpeakerPortraitWidget** - Speaker portraits with emotion overlays
- **DialogueHistoryWidget** - Scrollable dialogue history
- **SubtitleWidget** - Subtitle display with auto-hide
- **SkillCheckWidget** - Animated skill check results
- **QuestNotificationWidget** - Quest start/complete/fail notifications
- **DialogueDebugWidget** - Debug overlay for development

### GAS Integration
- **DialogueAbilityComponent** - Replicated component for ability triggers
- Ability triggers on dialogue nodes
- Gameplay effect application
- Attribute modification
- Gameplay tag system
- Ability conditions and cooldowns
- Global ability manager
- Damage/healing from dialogue
- Stat modification

### Data Structures
- 14 enums (DialogueNodeType, ConditionType, ResponseType, SpeakerEmotion, CameraAngle, etc.)
- 30+ structs (DialogueNode, DialogueChoice, DialogueCondition, DialogueSpeaker, etc.)
- 4 DataTable structs (SpeakerData, DialogueGraphData, EmotionData, CameraAngleData)
- 40+ helper functions for condition evaluation, node creation, text formatting

### Blueprint Integration
- 60+ blueprint-callable functions
- Actor spawning and management
- Dialogue control (start, end, pause, resume, advance)
- Speaker management
- Variable and flag manipulation
- Voice line control
- UI widget creation
- GAS integration functions

## File Structure

```
DialogueForge/
├── src/
│   ├── dialogue_data_structures.kn    (14 enums, 30+ structs, 40+ functions)
│   ├── dialogue_graph_editor.kn       (28 node types for visual authoring)
│   ├── dialogue_graph_runtime.kn      (28 NodeData classes with execution)
│   ├── dialogue_actors.kn             (3 actors, 25+ blueprint functions)
│   ├── dialogue_subsystem.kn          (@subsystem with @tick, 25+ functions)
│   ├── dialogue_ui_widgets.kn         (8 Slate widgets, 8 blueprint functions)
│   └── dialogue_gas_integration.kn    (GAS component, manager, 20+ functions)
├── KAIN.toml                          (Runtime + Editor modules)
├── README.md                          (This file)
├── IMPLEMENTATION_COMPLETE.md         (Implementation details)
└── BUILD_READY.md                     (Build instructions)
```

## Usage Examples

### Starting a Dialogue
```cpp
// Blueprint
ADialogueManagerActor* Manager = GetDialogueManager();
int32 InstanceID = Manager->StartDialogue("greeting_001", PlayerActor, NPCActor);

// C++ via Subsystem
UDialogueManagerSubsystem* Subsystem = GetWorld()->GetSubsystem<UDialogueManagerSubsystem>();
int32 InstanceID = Subsystem->StartDialogue("greeting_001", PlayerActor, NPCActor);
```

### Registering a Speaker
```cpp
FDialogueSpeaker Speaker;
Speaker.SpeakerId = "merchant";
Speaker.DisplayName = "Merchant";
Speaker.PortraitTexturePath = "/Game/UI/Portraits/Merchant";
Speaker.DefaultEmotion = ESpeakerEmotion::Neutral;

Subsystem->RegisterSpeaker(Speaker);
```

### Playing Voice Lines
```cpp
ADialogueSpeakerActor* Speaker = GetSpeaker();
Speaker->Server_Speak("Welcome, traveler!", "/Game/Audio/Merchant_Greeting", ESpeakerEmotion::Happy, 3.0f);
```

### Making Choices
```cpp
// Player selects choice 2
Manager->Server_MakeChoice(InstanceID, 2, NextNodeID);
```

### Setting Variables
```cpp
Subsystem->SetGlobalVariable("quest_stage", 2.0f);
Subsystem->SetPersistentFlag("met_merchant", true);
```

### GAS Integration
```cpp
// Trigger ability from dialogue
UDialogueAbilityComponent* Component = Actor->FindComponentByClass<UDialogueAbilityComponent>();
FDialogueAbilityTrigger Trigger = CreateAbilityTrigger("GA_Persuasion", 5, true, false);
Component->RegisterAbilityTrigger(Trigger);

// Apply gameplay effect
FDialogueGameplayEffect Effect = CreateGameplayEffect("GE_Charisma_Boost", 5, 30.0f, 10.0f);
Component->ApplyGameplayEffect(Effect, Speaker, Listener);
```

## Technical Details

### Line Count
- **dialogue_data_structures.kn**: ~1,200 lines
- **dialogue_graph_editor.kn**: ~450 lines
- **dialogue_graph_runtime.kn**: ~800 lines
- **dialogue_actors.kn**: ~650 lines
- **dialogue_subsystem.kn**: ~550 lines
- **dialogue_ui_widgets.kn**: ~600 lines
- **dialogue_gas_integration.kn**: ~500 lines
- **Total**: ~4,750 lines

### Generated C++ (Estimated)
- Runtime module: ~15,000 lines
- Editor module: ~8,000 lines
- Total: ~23,000 lines

### Compression Ratio
1 line KAIN → ~4.8 lines C++ (base)
With stdlib: 1 line KAIN → ~15-20 lines C++ (including UE5 boilerplate, macros, networking)

## Module Structure

### Runtime Module (DialogueForge)
- All actors, components, subsystems
- Data structures and enums
- Graph runtime execution
- Blueprint function libraries
- Networking and replication

### Editor Module (DialogueForgeEditor)
- Graph editor nodes and schema
- Slate UI widgets
- Asset editors and factories
- Details customizations
- Editor utilities

## Dependencies

### UE5 Modules
- Core, CoreUObject, Engine
- Slate, SlateCore (UI widgets)
- UMG (widget integration)
- GameplayAbilities (GAS integration)
- AIModule (for NPC integration)
- AudioMixer (voice line playback)

### KAIN Features Used
- Actors with RPCs (Server_, Client_, Multicast_)
- Components with replication
- Subsystems with tick
- Graph editor and runtime
- Slate widgets
- Blueprint integration
- Enums and structs
- DataTables

## Performance Characteristics

- **Max concurrent dialogues**: 10 (configurable)
- **Subsystem tick rate**: 0.1s (10 Hz)
- **Voice line queue**: Unlimited
- **History entries**: 100 max
- **Graph caching**: All loaded graphs cached
- **Speaker registration**: Dynamic, no limit
- **Replication**: Optimized with @replicated on critical state only

## Future Enhancements

- Localization support
- Voice line streaming
- Dialogue analytics
- Branching visualization
- Auto-generated subtitles
- Lip sync integration
- Facial animation triggers
- Dialogue mini-games
- Relationship system
- Mood system
- Context-aware responses

## License

Part of the KAIN Factory Part 2 plugin collection.

## Author

Generated by KAIN Compiler v1.0
