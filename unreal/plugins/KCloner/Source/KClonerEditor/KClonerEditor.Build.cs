// Copyright 2026 K-Studio. All Rights Reserved.

using UnrealBuildTool;

public class KClonerEditor : ModuleRules
{
	public KClonerEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"UnrealEd",
				"AssetTools",
				"EditorStyle",
				"Kismet",
				"InputCore",
				"KCloner", // Dependency on the runtime module
				"LevelEditor", // For context menu extensions
				"AdvancedPreviewScene",
				"EditorInteractiveToolsFramework",
				// Baking Dependencies
				"MeshDescription",
				"StaticMeshDescription",
				"SkeletalMeshDescription",
				"MeshUtilities",
				"MeshMergeUtilities",
				"MeshUtilitiesCommon",
				"AnimationDataController",
				"AnimationBlueprintLibrary", // Useful helpers
				"ToolMenus", // For GetActions context menus
				"GeometryCache", // For geometry cache export
				"Sequencer",
				"Sequencer",
				"MovieScene",
				"MovieSceneTracks",
				"PropertyEditor"
			}
		);
	}
}
