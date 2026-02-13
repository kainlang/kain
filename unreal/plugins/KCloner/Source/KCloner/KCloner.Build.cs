// Copyright 2026 K-Studio. All Rights Reserved.

using UnrealBuildTool;

public class KCloner : ModuleRules
{
	public KCloner(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"InputCore",
			"RenderCore",
			"RHI",
			"Niagara",
			"NiagaraCore",
			"NiagaraShader",
			"VectorVM",
			"MovieScene",       // Runtime! Games need this for cutscenes
			"MovieSceneTracks"  // Runtime! Custom sequencer tracks must link in packaged builds
		});

		if (Target.bBuildEditor)
		{
			PublicDependencyModuleNames.AddRange(new string[] {
				"UnrealEd",
				"EditorFramework",
				"LevelEditor",
				"EditorStyle",
				"ToolMenus"
				// MovieScene removed - it's in runtime deps now
			});
		}

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Projects",
			"AudioSynesthesia",
			"AudioMixer",
			"SignalProcessing"
		});

		PublicIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, "../ThirdParty/nanoflann"));
		PublicIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, "../ThirdParty/ExprTk"));

		// ExprTk requires exceptions and RTTI
		bEnableExceptions = true;
		bUseRTTI = true;
	}
}
