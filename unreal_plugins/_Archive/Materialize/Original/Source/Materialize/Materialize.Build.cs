using UnrealBuildTool;

public class Materialize : ModuleRules
{
	public Materialize(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"InputCore",
			"UnrealEd",
			"EditorFramework",
			"RenderCore",
			"RHI",
			"Renderer"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Projects",
			"EditorStyle",
			"ToolMenus",
			"AssetTools",
			"ContentBrowser",
			"AssetRegistry",
			"PropertyEditor",
			"LevelEditor",
			"AdvancedPreviewScene",
			"MaterialEditor",
			"DeveloperSettings",
			"ImageWrapper",
			"ImageCore",
			"DesktopPlatform",
			"AppFramework",
			"GraphEditor",
			"ApplicationCore"
		});
	}
}
