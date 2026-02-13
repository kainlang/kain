using UnrealBuildTool;

public class UltimateVFX : ModuleRules
{
    public UltimateVFX(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        
        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "RenderCore",
                "RHI",
                "Renderer"
            }
        );
        
        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Projects"
            }
        );
    }
}
