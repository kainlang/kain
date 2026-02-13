using UnrealBuildTool;

public class SketchfabImporterEditor : ModuleRules
{
	public SketchfabImporterEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"HTTP",
				"Json",
				"JsonUtilities",
				"SketchfabImporter"
			}
		);
			
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				"EditorStyle",
				"EditorWidgets",
				"UnrealEd",
				"InputCore",
				"ImageWrapper",
				"AssetTools",
				"ContentBrowser",
				"ToolMenus",
				"Projects",
				"MergeActors",
				"MeshUtilities",
				"InterchangePipelines",
				"InterchangeCore",
				"InterchangeEngine"
			}
		);
	}
}
