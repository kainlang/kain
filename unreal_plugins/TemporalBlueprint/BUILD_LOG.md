# Build Log - Temporal 
 
**Build Date**: Sat 02 28 2026 07:15 
**Status**: FAILED 
**Error Type**: KAIN COMPILATION 
 
## KAIN Compilation Errors 
 
```
 KAIN Compiler v0.1.0 (build 1772209264)
🚀 Building UE5 Plugin: Temporal
📍 Plugin directory: 

📚 Loaded stdlib from: m:\Code\Kain\stdlib\ue5
📁 Source files: 21 (stdlib: 12, user: 9)
   📚 Stdlib files:
      1. actor.kn
      2. common.kn
      3. components.kn
      4. gameplay.kn
      5. materials.kn
      6. math.kn
      7. particles.kn
      8. patterns.kn
      9. shaders.kn
      10. skeletal_mesh.kn
      11. utilities.kn
      12. world.kn
   📝 User files:
      1. Kain/types.kn
      2. Kain/components.kn
      3. Kain/actors.kn
      4. Kain/subsystems.kn
      5. Kain/algorithms.kn
      6. Kain/editor.kn
      7. Kain/editor_ui.kn
      8. Kain/editor_toolbar.kn
      9. Kain/details.kn

🔍 Validating source files...
   ✓ actor.kn validated
   ✓ common.kn validated
   ✓ components.kn validated
   ✓ gameplay.kn validated
   ✓ materials.kn validated
   ✓ math.kn validated
   ✓ particles.kn validated
   ✓ patterns.kn validated
   ✓ shaders.kn validated
   ✓ skeletal_mesh.kn validated
   ✓ utilities.kn validated
   ✓ world.kn validated
   ✓ types.kn validated
   ✓ components.kn validated
   ✓ actors.kn validated
   ✓ subsystems.kn validated
   ✓ algorithms.kn validated
   ✓ editor.kn validated
   ✓ editor_ui.kn validated
   ✓ editor_toolbar.kn validated
   ✓ details.kn validated

   ℹ️  Stdlib merge: 409 total → 1 kept (407 pruned by tree-shake, 1 shadowed by user code)
🔍 Type checking merged program...
   ✓ Type checking passed

🔄 Monomorphizing generic functions...
   ✓ Monomorphization complete

🔬 Running Unreal Semantic Validator (Oracle)...
   ✓ Oracle validation passed

📦 Multi-module layout: 2 module(s)
ℹ️  No shaders detected - skipping shader compilation

DEBUG: After shader compilation, target_actors.len() = 0
📐 Generating Blueprints for 5 actors...
   ✓ Binary blueprint: BP_TemporalManagerActor (5388 bytes)
   ✓ Binary blueprint: BP_TemporalActorProxy (3899 bytes)
   ❌ Blueprint generation error for BP_TemporalZoneActor: Asset write error: Failed to write .uasset: engine_version >= UE4_ADDED_PACKAGE_OWNER but new is None
   ❌ Blueprint generation error for BP_TemporalAnchorActor: Asset write error: Failed to write .uasset: engine_version >= UE4_ADDED_PACKAGE_OWNER but new is None
   ✓ Binary blueprint: BP_TemporalPortalActor (3702 bytes)

DEBUG: target_actors.len() = 0

🎯 Generating modular plugin files (per-file output)...
   📦 Generating master header with forward declarations...
      ✓ TemporalEditorTypes.h (complete type definitions for editor code - OPTION 3!)
      ✓ Temporal.h (master header with forward decls)
   📄 Slicing item: TemporalEra → ETemporalEra.h/cpp
      ✓ ETemporalEra.h
      ✓ ETemporalEra.cpp
   📄 Slicing item: TemporalTransitionType → ETemporalTransitionType.h/cpp
      ✓ ETemporalTransitionType.h
      ✓ ETemporalTransitionType.cpp
   📄 Slicing item: CausalityRule → ECausalityRule.h/cpp
      ✓ ECausalityRule.h
      ✓ ECausalityRule.cpp
   📄 Slicing item: TemporalActorBehavior → ETemporalActorBehavior.h/cpp
      ✓ ETemporalActorBehavior.h
      ✓ ETemporalActorBehavior.cpp
   📄 Slicing item: TemporalTransitionState → ETemporalTransitionState.h/cpp
      ✓ ETemporalTransitionState.h
      ✓ ETemporalTransitionState.cpp
   📄 Slicing item: TemporalEventType → ETemporalEventType.h/cpp
      ✓ ETemporalEventType.h
      ✓ ETemporalEventType.cpp
   📄 Slicing item: TemporalAnchorType → ETemporalAnchorType.h/cpp
      ✓ ETemporalAnchorType.h
      ✓ ETemporalAnchorType.cpp
   📄 Slicing item: TemporalLayerBlend → ETemporalLayerBlend.h/cpp
      ✓ ETemporalLayerBlend.h
      ✓ ETemporalLayerBlend.cpp
   📄 Slicing item: TemporalSnapshotMode → ETemporalSnapshotMode.h/cpp
      ✓ ETemporalSnapshotMode.h
      ✓ ETemporalSnapshotMode.cpp
   📄 Slicing item: TemporalDebugMode → ETemporalDebugMode.h/cpp
      ✓ ETemporalDebugMode.h
      ✓ ETemporalDebugMode.cpp
   📄 Slicing item: TemporalEraConfig → FTemporalEraConfig.h/cpp
      ✓ FTemporalEraConfig.h
      ✓ FTemporalEraConfig.cpp
   📄 Slicing item: TemporalActorState → FTemporalActorState.h/cpp
      ✓ FTemporalActorState.h
      ✓ FTemporalActorState.cpp
   📄 Slicing item: TemporalTransitionParams → FTemporalTransitionParams.h/cpp
      ✓ FTemporalTransitionParams.h
      ✓ FTemporalTransitionParams.cpp
   📄 Slicing item: TemporalCausalityLink → FTemporalCausalityLink.h/cpp
      ✓ FTemporalCausalityLink.h
      ✓ FTemporalCausalityLink.cpp
   📄 Slicing item: TemporalAnchor → FTemporalAnchor.h/cpp
      ✓ FTemporalAnchor.h
      ✓ FTemporalAnchor.cpp
   📄 Slicing item: TemporalZone → FTemporalZone.h/cpp
      ✓ FTemporalZone.h
      ✓ FTemporalZone.cpp
   📄 Slicing item: TemporalSnapshot → FTemporalSnapshot.h/cpp
      ✓ FTemporalSnapshot.h
      ✓ FTemporalSnapshot.cpp
   📄 Slicing item: TemporalEvent → FTemporalEvent.h/cpp
      ✓ FTemporalEvent.h
      ✓ FTemporalEvent.cpp
   📄 Slicing item: TemporalTimelineNode → FTemporalTimelineNode.h/cpp
      ✓ FTemporalTimelineNode.h
      ✓ FTemporalTimelineNode.cpp
   📄 Slicing item: TemporalBlendWeight → FTemporalBlendWeight.h/cpp
      ✓ FTemporalBlendWeight.h
      ✓ FTemporalBlendWeight.cpp
   📄 Slicing item: TemporalMeshVariant → FTemporalMeshVariant.h/cpp
      ✓ FTemporalMeshVariant.h
      ✓ FTemporalMeshVariant.cpp
   📄 Slicing item: TemporalDebugInfo → FTemporalDebugInfo.h/cpp
      ✓ FTemporalDebugInfo.h
      ✓ FTemporalDebugInfo.cpp
   📄 Slicing item: TemporalEraPresetData → FTemporalEraPresetData.h/cpp
      ✓ FTemporalEraPresetData.h
      ✓ FTemporalEraPresetData.cpp
   📄 Slicing item: TemporalTransitionPresetData → FTemporalTransitionPresetData.h/cpp
      ✓ FTemporalTransitionPresetData.h
      ✓ FTemporalTransitionPresetData.cpp
   📄 Slicing item: TemporalActorPresetData → FTemporalActorPresetData.h/cpp
      ✓ FTemporalActorPresetData.h
      ✓ FTemporalActorPresetData.cpp
   📄 Slicing item: TemporalZonePresetData → FTemporalZonePresetData.h/cpp
      ✓ FTemporalZonePresetData.h
      ✓ FTemporalZonePresetData.cpp
   📄 Slicing item: TemporalActorComponent → FTemporalActorComponent.h/cpp
      ✓ FTemporalActorComponent.h
      ✓ FTemporalActorComponent.cpp
   📄 Slicing item: TemporalZoneComponent → FTemporalZoneComponent.h/cpp
      ✓ FTemporalZoneComponent.h
      ✓ FTemporalZoneComponent.cpp
   📄 Slicing item: TemporalAnchorComponent → FTemporalAnchorComponent.h/cpp
      ✓ FTemporalAnchorComponent.h
      ✓ FTemporalAnchorComponent.cpp
   📄 Slicing item: TemporalCameraComponent → FTemporalCameraComponent.h/cpp
      ✓ FTemporalCameraComponent.h
      ✓ FTemporalCameraComponent.cpp
   📄 Slicing item: TemporalManagerActor → ATemporalManagerActor.h/cpp
      ✓ ATemporalManagerActor.h
      ✓ ATemporalManagerActor.cpp
   📄 Slicing item: TemporalActorProxy → ATemporalActorProxy.h/cpp
      ✓ ATemporalActorProxy.h
      ✓ ATemporalActorProxy.cpp
   📄 Slicing item: TemporalZoneActor → ATemporalZoneActor.h/cpp
      ✓ ATemporalZoneActor.h
      ✓ ATemporalZoneActor.cpp
   📄 Slicing item: TemporalAnchorActor → ATemporalAnchorActor.h/cpp
      ✓ ATemporalAnchorActor.h
      ✓ ATemporalAnchorActor.cpp
   📄 Slicing item: TemporalPortalActor → ATemporalPortalActor.h/cpp
      ✓ ATemporalPortalActor.h
      ✓ ATemporalPortalActor.cpp
   📄 Slicing item: TemporalSubsystem → FTemporalSubsystem.h/cpp
      ✓ FTemporalSubsystem.h
      ✓ FTemporalSubsystem.cpp
   📄 Slicing item: TemporalEditorSubsystem → FTemporalEditorSubsystem.h/cpp
      ✓ FTemporalEditorSubsystem.h
      ✓ FTemporalEditorSubsystem.cpp
   📦 Generating stdlib functions header...
      ✓ KainStdlib.h (stdlib utility functions)
   📦 Generating blueprint function library...
      ✓ TemporalBlueprintLibrary.h
      ✓ TemporalBlueprintLibrary.cpp
   🎨 Generating editor tools (Slate UI, Details, Viewport, Toolbar...)...
      ✓ TemporalBlueprintEditor.h (editor module master header)
   🧹 Removed stale TemporalBlueprintEditor.h
   📄 Editor item: SSTemporalEditorPanel [Slate] → SSTemporalEditorPanel.h/cpp
      ✓ SSTemporalEditorPanel.h
      ✓ SSTemporalEditorPanel.cpp
IO error: The system cannot find the file specified. (os error 2)
```
