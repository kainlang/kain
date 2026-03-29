# Materialize — Architecture Diagram

## System Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                         USER INTERFACE LAYER                         │
├─────────────────────────────────────────────────────────────────────┤
│                                                                       │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │              SMaterializeEditor (Main Window)                  │  │
│  ├───────────────────────────────────────────────────────────────┤  │
│  │  Toolbar: [Preset ▼] [Load Selected] [Env: Day ▼]            │  │
│  │  Workflow: [Layer View] [Graph View]                          │  │
│  │  View Mode: [Material] [N] [R] [M] [AO] [H] [Base]           │  │
│  ├───────────────────────────────────────────────────────────────┤  │
│  │                                                                 │  │
│  │  ┌─────────────────────────┬──────────────────────────────┐   │  │
│  │  │  Workflow Content       │  Right Panel (Tabs)          │   │  │
│  │  │  (SWidgetSwitcher)      │                              │   │  │
│  │  │                         │  ┌────────────────────────┐  │   │  │
│  │  │  Layer View:            │  │ [Parameters] [Layers]  │  │   │  │
│  │  │  ┌──────────────────┐   │  ├────────────────────────┤  │   │  │
│  │  │  │  3D Viewport     │   │  │                        │  │   │  │
│  │  │  │  (Preview)       │   │  │  IDetailsView          │  │   │  │
│  │  │  └──────────────────┘   │  │  (ToolModel)           │  │   │  │
│  │  │  [N][R][M][AO][H][E]    │  │                        │  │   │  │
│  │  │  ┌──────────────────┐   │  │  OR                    │  │   │  │
│  │  │  │  Asset Picker    │   │  │                        │  │   │  │
│  │  │  └──────────────────┘   │  │  Layer List            │  │   │  │
│  │  │                         │  │  + Layer Details       │  │   │  │
│  │  │  Graph View:            │  │                        │  │   │  │
│  │  │  ┌──┬────────┬──────┐   │  └────────────────────────┘  │   │  │
│  │  │  │P │ Graph  │Exec  │   │                              │   │  │
│  │  │  │a │ Canvas │Btn   │   │                              │   │  │
│  │  │  │l │        │      │   │                              │   │  │
│  │  │  └──┴────────┴──────┘   │                              │   │  │
│  │  └─────────────────────────┴──────────────────────────────┘   │  │
│  └─────────────────────────────────────────────────────────────────┘  │
│                                                                       │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │           SMaterializeBatchWindow (Separate Window)           │  │
│  ├───────────────────────────────────────────────────────────────┤  │
│  │  [Add from Browser] [Add from Folder] [Remove] [Clear]       │  │
│  │  ┌─────────────────────────┬─────────────────────────────┐   │  │
│  │  │  Queue List             │  Settings Panel             │   │  │
│  │  │  ┌─────────────────────┐│  ┌─────────────────────────┐│   │  │
│  │  │  │ ✓ Texture1.png      ││  │ Export Format: UAsset   ││   │  │
│  │  │  │ ⏳ Texture2.png     ││  │ Naming: {Name}_{Chan}   ││   │  │
│  │  │  │ ⏸ Texture3.png      ││  │ Output: /Game/PBR       ││   │  │
│  │  │  └─────────────────────┘│  └─────────────────────────┘│   │  │
│  │  └─────────────────────────┴─────────────────────────────┘   │  │
│  │  [████████████░░░░░░░░░░] 60% (30/50)                        │  │
│  │  [Start] [Pause] [Cancel]                                    │  │
│  └───────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
```

## Data Flow

### Layer View Workflow

```
User Loads Texture
    ↓
ToolModel.SourceTexture = Texture
    ↓
OnModelChanged() → MaybeLiveUpdate()
    ↓
[0.15s Debounce Timer]
    ↓
OnGeneratePreview()
    ↓
Validate Texture (RF_NeedLoad, GPU resource)
    ↓
UMaterializeComputeEngine::GeneratePBRMapsGPU()
    ↓
RDG Pass: 15 Compute Shaders
    ↓
FMaterializeResult (7 transient textures)
    ↓
SetPreviewMaterialFromResult()
    ↓
Load Master Material (Plugin → Game → Transient)
    ↓
Create MID, Set Texture Parameters
    ↓
ViewportClient->SetPreviewMaterial(MID)
    ↓
UpdateChannelThumbnails() (6 thumbnails)
    ↓
User Sees Preview in 3D Viewport
```


### Graph View Workflow

```
User Places Nodes in Graph
    ↓
Connect Nodes with Wires
    ↓
Adjust Node Parameters
    ↓
OnGraphNodePropertyChanged()
    ↓
ExecuteGraph()
    ↓
ValidateGraphTopology() (Cycle Detection)
    ↓
TopologicalSort() (DFS-based)
    ↓
For Each Node in Execution Order:
    ├─ GetInputTexture() from connected pins
    ├─ ExecuteNodeRecursive()
    └─ CacheTexture(NodeGuid, Result)
    ↓
FindChannelOutputNodes() (7 channels)
    ↓
FMaterializeGraphExecutionResult
    ↓
SetPreviewMaterialFromGraphResult()
    ↓
ViewportClient->SetPreviewMaterial(MID)
    ↓
ExecuteWithPreviews() (256x256 node thumbnails)
    ↓
User Sees Preview + Node Thumbnails
```

### Batch Processing Workflow

```
User Opens Batch Window
    ↓
Add Textures from Folder (Recursive Scan)
    ↓
Configure Settings (Export Format, Naming, Output Path)
    ↓
Click "Start"
    ↓
BatchProcessor->StartProcessing()
    ↓
For Each Item in Queue:
    ├─ OnItemStarted.Broadcast(ItemId)
    ├─ ProcessSingleItem()
    │   ├─ Load Texture
    │   ├─ GeneratePBRMapsGPU()
    │   ├─ GetOutputPath() (Apply naming convention)
    │   └─ SaveAssets() (UAsset/PNG/TGA/EXR)
    ├─ OnItemCompleted.Broadcast(ItemId, Success)
    └─ UpdateProgress()
    ↓
FinishBatch()
    ↓
OnBatchCompleted.Broadcast(FinalProgress)
    ↓
Show Notification: "50 textures processed in 2m 30s"
```

## Component Interaction

```
┌──────────────────┐
│  SMaterializeEditor │
│  (Main Window)    │
└────────┬─────────┘
         │
         ├─────────────────────────────────────────┐
         │                                         │
         ▼                                         ▼
┌────────────────────┐                  ┌──────────────────┐
│  Layer View        │                  │  Graph View      │
│  ┌──────────────┐  │                  │  ┌────────────┐  │
│  │  Viewport    │  │                  │  │  Palette   │  │
│  └──────────────┘  │                  │  └────────────┘  │
│  ┌──────────────┐  │                  │  ┌────────────┐  │
│  │  Channel     │  │                  │  │  Graph     │  │
│  │  Strip       │  │                  │  │  Editor    │  │
│  └──────────────┘  │                  │  └────────────┘  │
│  ┌──────────────┐  │                  │  ┌────────────┐  │
│  │  Asset       │  │                  │  │  Execute   │  │
│  │  Picker      │  │                  │  │  Button    │  │
│  └──────────────┘  │                  │  └────────────┘  │
└────────┬───────────┘                  └────────┬─────────┘
         │                                       │
         ▼                                       ▼
┌────────────────────┐                  ┌──────────────────┐
│  KLayerEvaluator   │                  │  GraphExecutor   │
│  (Compositor)      │                  │  (Node Runner)   │
└────────┬───────────┘                  └────────┬─────────┘
         │                                       │
         └───────────────┬───────────────────────┘
                         │
                         ▼
                ┌────────────────────┐
                │  ComputeEngine     │
                │  (15 GPU Shaders)  │
                └────────┬───────────┘
                         │
                         ▼
                ┌────────────────────┐
                │  MaterializeResult │
                │  (7 Textures)      │
                └────────┬───────────┘
                         │
                         ▼
                ┌────────────────────┐
                │  MaterialLoader    │
                │  (Master Material) │
                └────────┬───────────┘
                         │
                         ▼
                ┌────────────────────┐
                │  MID Creation      │
                │  (Set Parameters)  │
                └────────┬───────────┘
                         │
                         ▼
                ┌────────────────────┐
                │  Viewport Preview  │
                └────────────────────┘
```

## Module Dependencies

```
MaterializeEditor (Editor Module)
    ├─ Depends on: Materialize (Runtime Module)
    ├─ Depends on: UnrealEd
    ├─ Depends on: Slate
    ├─ Depends on: SlateCore
    ├─ Depends on: PropertyEditor
    ├─ Depends on: ContentBrowser
    ├─ Depends on: AssetTools
    └─ Depends on: GraphEditor

Materialize (Runtime Module)
    ├─ Depends on: Core
    ├─ Depends on: CoreUObject
    ├─ Depends on: Engine
    ├─ Depends on: RenderCore
    ├─ Depends on: RHI
    └─ Depends on: Renderer
```
