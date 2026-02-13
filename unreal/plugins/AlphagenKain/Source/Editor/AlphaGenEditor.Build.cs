// Copyright K-Studio. All Rights Reserved.
// AlphaGen Editor Module Build Configuration

using UnrealBuildTool;
using System.IO;

public class AlphaGenEditor : ModuleRules
{
	public AlphaGenEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
			}
		);
				
		PrivateIncludePaths.AddRange(
			new string[] {
				Path.Combine(ModuleDirectory),
				Path.Combine(ModuleDirectory, "Widgets"),
				Path.Combine(ModuleDirectory, "Generators"),
				Path.Combine(ModuleDirectory, "Compute"),
				Path.Combine(ModuleDirectory, "Core"),
			}
		);
			
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"RenderCore",
				"RHI",
				"Renderer",
				"AlphaGen", // Main runtime module
			}
		);
			
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				"EditorStyle",
				"UnrealEd",
				"ToolMenus",
				"Projects",
				"EditorFramework",
				"WorkspaceMenuStructure",
				"PropertyEditor",
				"DesktopPlatform",
				"ImageWrapper",
				"AssetRegistry",
				"ContentBrowser",
			}
		);
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
		);
	}
}
