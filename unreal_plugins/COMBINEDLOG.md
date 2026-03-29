 
------------- 
MetaFitter - UE5 BUILD 
------------- 
Parsing command line: BuildPlugin -Plugin=m:\Code\Factory\MetaFitter\MetaFitter\MetaFitter.uplugin -Package=m:\Code\Factory\MetaFitter\_Builds\MetaFitter_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
UnrealBuildTool failed. See log for more details. (C:\Users\Admin\AppData\Roaming\Unreal Engine\AutomationTool\Logs\D+Unreal+UE_5.4\UBA-UnrealEditor-Win64-Development.txt) 
BUILD FAILED 
 
Total errors found: 3 
 
 
------------- 
MetaHumanVAT - UE5 BUILD 
------------- 
Parsing command line: BuildPlugin -Plugin=m:\Code\Factory\MetaHumanVAT\MetaHumanVAT.uplugin -Package=m:\Code\Factory\MetaHumanVAT\_Builds\MetaHumanVAT_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
  Running Internal UnrealHeaderTool M:\Code\Factory\MetaHumanVAT\_Builds\MetaHumanVAT_5.4\HostProject\HostProject.uproject M:\Code\Factory\MetaHumanVAT\_Builds\MetaHumanVAT_5.4\HostProject\Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest -WarningsAsErrors -installed 
M:\Code\Factory\MetaHumanVAT\_Builds\MetaHumanVAT_5.4\HostProject\Plugins\MetaHumanVAT\Source\MetaHumanVAT\Private\AVATPreviewActor.cpp(146): error C2664: 'void AddPass_BakeSkeletalToVAT(FRDGBuilder &,int32,int32,int32,int32,int32,int32,FRHIShaderResourceView *,FRHIShaderResourceView *,FRHIShaderResourceView *,FRHIShaderResourceView *,FRHIShaderResourceView *,FRDGTextureRef,FIntVector,bool,bool,bool,bool)': cannot convert argument 8 from 'FRDGTextureRef' to 'FRHIShaderResourceView *' 
UnrealBuildTool failed. See log for more details. (C:\Users\Admin\AppData\Roaming\Unreal Engine\AutomationTool\Logs\D+Unreal+UE_5.4\UBA-UnrealEditor-Win64-Development.txt) 
BUILD FAILED 
 
Total errors found: 5 
 
 
------------- 
VoxelForgePro - UE5 BUILD 
------------- 
Parsing command line: BuildPlugin -Plugin=m:\Code\Factory\VoxelForgePro\VoxelForgePro\VoxelForgePro.uplugin -Package=m:\Code\Factory\VoxelForgePro\_Builds\VoxelForgePro_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
  Running Internal UnrealHeaderTool M:\Code\Factory\VoxelForgePro\_Builds\VoxelForgePro_5.4\HostProject\HostProject.uproject M:\Code\Factory\VoxelForgePro\_Builds\VoxelForgePro_5.4\HostProject\Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest -WarningsAsErrors -installed 
M:\Code\Factory\VoxelForgePro\_Builds\VoxelForgePro_5.4\HostProject\Plugins\VoxelForgePro\Source\VoxelForgeProEditor\Public\SVoxelToolPalette.h(33): error C3646: '_selected_tool': unknown override specifier 
M:\Code\Factory\VoxelForgePro\_Builds\VoxelForgePro_5.4\HostProject\Plugins\VoxelForgePro\Source\VoxelForgeProEditor\Public\SVoxelToolPalette.h(33): error C4430: missing type specifier - int assumed. Note: C++ does not support default-int 
M:\Code\Factory\VoxelForgePro\_Builds\VoxelForgePro_5.4\HostProject\Plugins\VoxelForgePro\Source\VoxelForgeProEditor\Public\SVoxelToolPalette.h(33): error C2061: syntax error: identifier 'EditMode' 
M:\Code\Factory\VoxelForgePro\_Builds\VoxelForgePro_5.4\HostProject\Plugins\VoxelForgePro\Source\VoxelForgeProEditor\Public\SVoxelToolPalette.h(28): error C3861: 'EditMode': identifier not found 
M:\Code\Factory\VoxelForgePro\_Builds\VoxelForgePro_5.4\HostProject\Plugins\VoxelForgePro\Source\VoxelForgeProEditor\Public\SVoxelToolPalette.h(28): error C2614: 'SVoxelToolPalette::FArguments': illegal member initialization: '_selected_tool' is not a base or member 
M:\Code\Factory\VoxelForgePro\_Builds\VoxelForgePro_5.4\HostProject\Plugins\VoxelForgePro\Source\VoxelForgeProEditor\Public\SVoxelToolPalette.h(33): error C2065: '_selected_tool': undeclared identifier 
M:\Code\Factory\VoxelForgePro\_Builds\VoxelForgePro_5.4\HostProject\Plugins\VoxelForgePro\Source\VoxelForgeProEditor\Public\SVoxelToolPalette.h(33): error C2065: 'InArg': undeclared identifier 
M:\Code\Factory\VoxelForgePro\_Builds\VoxelForgePro_5.4\HostProject\Plugins\VoxelForgePro\Source\VoxelForgeProEditor\Public\SVoxelToolPalette.h(33): error C3646: '_selected_tool': unknown override specifier 
M:\Code\Factory\VoxelForgePro\_Builds\VoxelForgePro_5.4\HostProject\Plugins\VoxelForgePro\Source\VoxelForgeProEditor\Public\SVoxelToolPalette.h(33): error C4430: missing type specifier - int assumed. Note: C++ does not support default-int 
M:\Code\Factory\VoxelForgePro\_Builds\VoxelForgePro_5.4\HostProject\Plugins\VoxelForgePro\Source\VoxelForgeProEditor\Public\SVoxelToolPalette.h(33): error C2061: syntax error: identifier 'EditMode' 
M:\Code\Factory\VoxelForgePro\_Builds\VoxelForgePro_5.4\HostProject\Plugins\VoxelForgePro\Source\VoxelForgeProEditor\Public\SVoxelToolPalette.h(28): error C3861: 'EditMode': identifier not found 
M:\Code\Factory\VoxelForgePro\_Builds\VoxelForgePro_5.4\HostProject\Plugins\VoxelForgePro\Source\VoxelForgeProEditor\Public\SVoxelToolPalette.h(28): error C2614: 'SVoxelToolPalette::FArguments': illegal member initialization: '_selected_tool' is not a base or member 
M:\Code\Factory\VoxelForgePro\_Builds\VoxelForgePro_5.4\HostProject\Plugins\VoxelForgePro\Source\VoxelForgeProEditor\Public\SVoxelToolPalette.h(33): error C2065: '_selected_tool': undeclared identifier 
M:\Code\Factory\VoxelForgePro\_Builds\VoxelForgePro_5.4\HostProject\Plugins\VoxelForgePro\Source\VoxelForgeProEditor\Public\SVoxelToolPalette.h(33): error C2065: 'InArg': undeclared identifier 
UnrealBuildTool failed. See log for more details. (C:\Users\Admin\AppData\Roaming\Unreal Engine\AutomationTool\Logs\D+Unreal+UE_5.4\UBA-UnrealEditor-Win64-Development.txt) 
BUILD FAILED 
 
Total errors found: 18 
 
 
------------- 
UESculpt - UE5 BUILD 
------------- 
Parsing command line: BuildPlugin -Plugin=m:\Code\Factory\UESculpt\UESculpt.uplugin -Package=m:\Code\Factory\UESculpt\_Builds\UESculpt_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
  Running Internal UnrealHeaderTool M:\Code\Factory\UESculpt\_Builds\UESculpt_5.4\HostProject\HostProject.uproject M:\Code\Factory\UESculpt\_Builds\UESculpt_5.4\HostProject\Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest -WarningsAsErrors -installed 
M:\Code\Factory\UESculpt\_Builds\UESculpt_5.4\HostProject\Plugins\UESculpt\Source\UESculpt\Private\UESculptBlueprintLibrary.cpp(36): error C2676: binary '==': 'const ESculptSymmetryMode' does not define this operator or a conversion to a type acceptable to the predefined operator 
M:\Code\Factory\UESculpt\_Builds\UESculpt_5.4\HostProject\Plugins\UESculpt\Source\UESculpt\Private\UESculptBlueprintLibrary.cpp(37): error C2440: 'return': cannot convert from 'ESculptSymmetryMode' to 'bool' 
UnrealBuildTool failed. See log for more details. (C:\Users\Admin\AppData\Roaming\Unreal Engine\AutomationTool\Logs\D+Unreal+UE_5.4\UBA-UnrealEditor-Win64-Development.txt) 
BUILD FAILED 
 
Total errors found: 6 
 
 
------------- 
FluidFlow - UE5 BUILD 
------------- 
Parsing command line: BuildPlugin -Plugin=m:\Code\Factory\FluidFlow\FluidFlow.uplugin -Package=m:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
  Running Internal UnrealHeaderTool M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\HostProject.uproject M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest -WarningsAsErrors -installed 
UnrealBuildTool failed. See log for more details. (C:\Users\Admin\AppData\Roaming\Unreal Engine\AutomationTool\Logs\D+Unreal+UE_5.4\UBA-UnrealEditor-Win64-Development.txt) 
BUILD FAILED 
 
Total errors found: 4 
 
 
------------- 
MetaHumanVAT - UE5 BUILD 
------------- 
Parsing command line: BuildPlugin -Plugin=m:\Code\Factory\MetaHumanVAT\MetaHumanVAT.uplugin -Package=m:\Code\Factory\MetaHumanVAT\_Builds\MetaHumanVAT_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
  Running Internal UnrealHeaderTool M:\Code\Factory\MetaHumanVAT\_Builds\MetaHumanVAT_5.4\HostProject\HostProject.uproject M:\Code\Factory\MetaHumanVAT\_Builds\MetaHumanVAT_5.4\HostProject\Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest -WarningsAsErrors -installed 
M:\Code\Factory\MetaHumanVAT\_Builds\MetaHumanVAT_5.4\HostProject\Plugins\MetaHumanVAT\Source\MetaHumanVAT\Private\AVATPreviewActor.cpp(14): fatal error C1083: Cannot open include file: 'CompressVATTexture.h': No such file or directory 
UnrealBuildTool failed. See log for more details. (C:\Users\Admin\AppData\Roaming\Unreal Engine\AutomationTool\Logs\D+Unreal+UE_5.4\UBA-UnrealEditor-Win64-Development.txt) 
BUILD FAILED 
 
Total errors found: 5 
 
 
------------- 
CrowdFlowDirector - UE5 BUILD 
------------- 
Parsing command line: BuildPlugin -Plugin=m:\Code\Factory\CrowdFlowDirector\CrowdFlowDirector.uplugin -Package=m:\Code\Factory\CrowdFlowDirector\_Builds\CrowdFlowDirector_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
  Running Internal UnrealHeaderTool M:\Code\Factory\CrowdFlowDirector\_Builds\CrowdFlowDirector_5.4\HostProject\HostProject.uproject M:\Code\Factory\CrowdFlowDirector\_Builds\CrowdFlowDirector_5.4\HostProject\Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest -WarningsAsErrors -installed 
M:\Code\Factory\CrowdFlowDirector\_Builds\CrowdFlowDirector_5.4\HostProject\Plugins\CrowdFlowDirector\Source\CrowdFlowDirector\Private\ACrowdFlowDirectorActor.cpp(151): error C2664: 'void AddPass_GenerateCrowdTargets(FRDGBuilder &,int32,int32,FVector3f,FVector3f,FVector3f,float,float,FRDGTextureRef,FRDGTextureRef,FRDGTextureRef,FIntVector,bool,bool)': cannot convert argument 12 from 'FRDGTextureRef' to 'FIntVector' 
M:\Code\Factory\CrowdFlowDirector\_Builds\CrowdFlowDirector_5.4\HostProject\Plugins\CrowdFlowDirector\Source\CrowdFlowDirector\Private\ACrowdFlowDirectorActor.cpp(152): error C2664: 'void AddPass_SolveCrowdFlowField(FRDGBuilder &,int32,float,float,float,float,float,FRHIShaderResourceView *,FRHIShaderResourceView *,FRDGTextureRef,FIntVector,bool)': cannot convert argument 11 from 'FRDGTextureRef' to 'FIntVector' 
M:\Code\Factory\CrowdFlowDirector\_Builds\CrowdFlowDirector_5.4\HostProject\Plugins\CrowdFlowDirector\Source\CrowdFlowDirector\Private\ACrowdFlowDirectorActor.cpp(153): error C2660: 'AddPass_IntegrateCrowdAgents': function does not take 10 arguments 
M:\Code\Factory\CrowdFlowDirector\_Builds\CrowdFlowDirector_5.4\HostProject\Plugins\CrowdFlowDirector\Source\CrowdFlowDirector\Private\ACrowdFlowDirectorActor.cpp(154): error C2660: 'AddPass_BuildCrowdLodMask': function does not take 10 arguments 
UnrealBuildTool failed. See log for more details. (C:\Users\Admin\AppData\Roaming\Unreal Engine\AutomationTool\Logs\D+Unreal+UE_5.4\UBA-UnrealEditor-Win64-Development.txt) 
BUILD FAILED 
 
Total errors found: 8 
 
 
------------- 
TacticalRaidGAS - UE5 BUILD 
------------- 
Parsing command line: BuildPlugin -Plugin=m:\Code\Factory\TacticalRaidGAS\TacticalRaidGAS.uplugin -Package=m:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
  Running Internal UnrealHeaderTool M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\HostProject.uproject M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest -WarningsAsErrors -installed 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\Tasks\WaitForSquadConfirmTask.h(7): Error: Inappropriate '*' on variable of type 'FGameplayEventData', cannot have an exposed pointer to this type. 
UnrealBuildTool failed. See log for more details. (C:\Users\Admin\AppData\Roaming\Unreal Engine\AutomationTool\Logs\D+Unreal+UE_5.4\UBA-UnrealEditor-Win64-Development.txt) 
BUILD FAILED 
 
Total errors found: 5 
 
 
------------- 
TacticalRaidGAS - UE5 BUILD 
------------- 
Parsing command line: BuildPlugin -Plugin=m:\Code\Factory\TacticalRaidGAS\TacticalRaidGAS.uplugin -Package=m:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
  Running Internal UnrealHeaderTool M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\HostProject.uproject M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest -WarningsAsErrors -installed 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\Tasks\WaitForSquadConfirmTask.h(7): Error: Inappropriate '*' on variable of type 'FGameplayEventData', cannot have an exposed pointer to this type. 
UnrealBuildTool failed. See log for more details. (C:\Users\Admin\AppData\Roaming\Unreal Engine\AutomationTool\Logs\D+Unreal+UE_5.4\UBA-UnrealEditor-Win64-Development.txt) 
BUILD FAILED 
 
Total errors found: 5 
 
 
------------- 
CrowdFlowDirector - UE5 BUILD 
------------- 
Parsing command line: BuildPlugin -Plugin=m:\Code\Factory\CrowdFlowDirector\CrowdFlowDirector.uplugin -Package=m:\Code\Factory\CrowdFlowDirector\_Builds\CrowdFlowDirector_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
  Running Internal UnrealHeaderTool M:\Code\Factory\CrowdFlowDirector\_Builds\CrowdFlowDirector_5.4\HostProject\HostProject.uproject M:\Code\Factory\CrowdFlowDirector\_Builds\CrowdFlowDirector_5.4\HostProject\Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest -WarningsAsErrors -installed 
M:\Code\Factory\CrowdFlowDirector\_Builds\CrowdFlowDirector_5.4\HostProject\Plugins\CrowdFlowDirector\Source\CrowdFlowDirector\Private\ACrowdFlowDirectorActor.cpp(151): error C2664: 'void AddPass_GenerateCrowdTargets(FRDGBuilder &,int32,int32,FVector3f,FVector3f,FVector3f,float,float,FRDGTextureRef,FRDGTextureRef,FRDGTextureRef,FIntVector,bool,bool)': cannot convert argument 12 from 'FRDGTextureRef' to 'FIntVector' 
M:\Code\Factory\CrowdFlowDirector\_Builds\CrowdFlowDirector_5.4\HostProject\Plugins\CrowdFlowDirector\Source\CrowdFlowDirector\Private\ACrowdFlowDirectorActor.cpp(152): error C2664: 'void AddPass_SolveCrowdFlowField(FRDGBuilder &,int32,float,float,float,float,float,FRHIShaderResourceView *,FRHIShaderResourceView *,FRDGTextureRef,FIntVector,bool)': cannot convert argument 11 from 'FRDGTextureRef' to 'FIntVector' 
M:\Code\Factory\CrowdFlowDirector\_Builds\CrowdFlowDirector_5.4\HostProject\Plugins\CrowdFlowDirector\Source\CrowdFlowDirector\Private\ACrowdFlowDirectorActor.cpp(153): error C2660: 'AddPass_IntegrateCrowdAgents': function does not take 10 arguments 
M:\Code\Factory\CrowdFlowDirector\_Builds\CrowdFlowDirector_5.4\HostProject\Plugins\CrowdFlowDirector\Source\CrowdFlowDirector\Private\ACrowdFlowDirectorActor.cpp(154): error C2660: 'AddPass_BuildCrowdLodMask': function does not take 10 arguments 
UnrealBuildTool failed. See log for more details. (C:\Users\Admin\AppData\Roaming\Unreal Engine\AutomationTool\Logs\D+Unreal+UE_5.4\UBA-UnrealEditor-Win64-Development.txt) 
BUILD FAILED 
 
Total errors found: 8 
 
 
------------- 
TacticalRaidGAS - UE5 BUILD 
------------- 
Parsing command line: BuildPlugin -Plugin=M:\Code\Factory\TacticalRaidGAS\TacticalRaidGAS.uplugin -Package=M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
  Running Internal UnrealHeaderTool M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\HostProject.uproject M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest -WarningsAsErrors -installed 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\ERaidPhase.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\EThreatBand.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\ATacticalReconDrone.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\ATacticalRaidDirector.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\FTacticalRaidBalance.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\FSquadStateComponent.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\FTacticalVisionComponent.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Abilities\GA_AirburstGrenade.cpp(1): fatal error C1083: Cannot open include file: 'GA_AirburstGrenade.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Abilities\GA_BurstFire.cpp(1): fatal error C1083: Cannot open include file: 'GA_BurstFire.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Abilities\GA_CombatRoll.cpp(1): fatal error C1083: Cannot open include file: 'GA_CombatRoll.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Abilities\GA_JamComms.cpp(1): fatal error C1083: Cannot open include file: 'GA_JamComms.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Abilities\GA_MedicRevive.cpp(1): fatal error C1083: Cannot open include file: 'GA_MedicRevive.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Abilities\GA_PingTarget.cpp(1): fatal error C1083: Cannot open include file: 'GA_PingTarget.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Abilities\GA_TacticalSprint.cpp(1): fatal error C1083: Cannot open include file: 'GA_TacticalSprint.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Cues\GC_JammerField.cpp(1): fatal error C1083: Cannot open include file: 'GC_JammerField.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Cues\GC_MuzzleFlash.cpp(1): fatal error C1083: Cannot open include file: 'GC_MuzzleFlash.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Cues\GC_ReviveBeam.cpp(1): fatal error C1083: Cannot open include file: 'GC_ReviveBeam.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Cues\GC_SprintBurst.cpp(1): fatal error C1083: Cannot open include file: 'GC_SprintBurst.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Cues\GC_ThreatPingPulse.cpp(1): fatal error C1083: Cannot open include file: 'GC_ThreatPingPulse.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_Cooldown_AirburstGrenade.cpp(1): fatal error C1083: Cannot open include file: 'GE_Cooldown_AirburstGrenade.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_Cooldown_BurstFire.cpp(1): fatal error C1083: Cannot open include file: 'GE_Cooldown_BurstFire.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_Cooldown_JamComms.cpp(1): fatal error C1083: Cannot open include file: 'GE_Cooldown_JamComms.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_Cooldown_MedicRevive.cpp(1): fatal error C1083: Cannot open include file: 'GE_Cooldown_MedicRevive.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_Cooldown_PingTarget.cpp(1): fatal error C1083: Cannot open include file: 'GE_Cooldown_PingTarget.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_Cooldown_TacticalSprint.cpp(1): fatal error C1083: Cannot open include file: 'GE_Cooldown_TacticalSprint.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_JammedCommsDebuff.cpp(1): fatal error C1083: Cannot open include file: 'GE_JammedCommsDebuff.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_StaminaCost_BurstFire.cpp(1): fatal error C1083: Cannot open include file: 'GE_StaminaCost_BurstFire.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_StaminaCost_TacticalSprint.cpp(1): fatal error C1083: Cannot open include file: 'GE_StaminaCost_TacticalSprint.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_SuppressedDebuff.cpp(1): fatal error C1083: Cannot open include file: 'GE_SuppressedDebuff.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(12): error C2757: 'Movement': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(17): error C2757: 'Support': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(22): error C2757: 'Utility': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(27): error C2757: 'Weapon': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(38): error C2757: 'Movement': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(42): error C2757: 'Support': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(46): error C2757: 'Weapon': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(57): error C2757: 'Movement': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(61): error C2757: 'Utility': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(66): error C2757: 'Weapon': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(77): error C2757: 'LifeState': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(85): error C2757: 'Posture': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(91): error C2757: 'RoleTag': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(98): error C2757: 'TeamTag': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(109): error C2757: 'Combat': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(115): error C2757: 'Squad': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(126): error C2757: 'Intel': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(131): error C2757: 'Mobility': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(136): error C2757: 'Utility': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
 
[TRUNCATED: 36 more errors not shown] 
 
Total errors found: 86 
 
 
------------- 
TacticalRaidGAS - UE5 BUILD 
------------- 
Parsing command line: BuildPlugin -Plugin=m:\Code\Factory\TacticalRaidGAS\TacticalRaidGAS.uplugin -Package=m:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
  Running Internal UnrealHeaderTool M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\HostProject.uproject M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest -WarningsAsErrors -installed 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\EThreatBand.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\ERaidPhase.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\ATacticalReconDrone.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\ATacticalRaidDirector.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\FTacticalRaidBalance.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\FSquadStateComponent.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Abilities\GA_AirburstGrenade.cpp(1): fatal error C1083: Cannot open include file: 'GA_AirburstGrenade.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\FTacticalVisionComponent.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Abilities\GA_BurstFire.cpp(1): fatal error C1083: Cannot open include file: 'GA_BurstFire.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Abilities\GA_CombatRoll.cpp(1): fatal error C1083: Cannot open include file: 'GA_CombatRoll.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Abilities\GA_JamComms.cpp(1): fatal error C1083: Cannot open include file: 'GA_JamComms.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Abilities\GA_MedicRevive.cpp(1): fatal error C1083: Cannot open include file: 'GA_MedicRevive.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Abilities\GA_PingTarget.cpp(1): fatal error C1083: Cannot open include file: 'GA_PingTarget.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Abilities\GA_TacticalSprint.cpp(1): fatal error C1083: Cannot open include file: 'GA_TacticalSprint.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Cues\GC_JammerField.cpp(1): fatal error C1083: Cannot open include file: 'GC_JammerField.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Cues\GC_ReviveBeam.cpp(1): fatal error C1083: Cannot open include file: 'GC_ReviveBeam.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Cues\GC_MuzzleFlash.cpp(1): fatal error C1083: Cannot open include file: 'GC_MuzzleFlash.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Cues\GC_SprintBurst.cpp(1): fatal error C1083: Cannot open include file: 'GC_SprintBurst.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Cues\GC_ThreatPingPulse.cpp(1): fatal error C1083: Cannot open include file: 'GC_ThreatPingPulse.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_Cooldown_BurstFire.cpp(1): fatal error C1083: Cannot open include file: 'GE_Cooldown_BurstFire.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_Cooldown_AirburstGrenade.cpp(1): fatal error C1083: Cannot open include file: 'GE_Cooldown_AirburstGrenade.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_Cooldown_PingTarget.cpp(1): fatal error C1083: Cannot open include file: 'GE_Cooldown_PingTarget.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_Cooldown_JamComms.cpp(1): fatal error C1083: Cannot open include file: 'GE_Cooldown_JamComms.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_Cooldown_MedicRevive.cpp(1): fatal error C1083: Cannot open include file: 'GE_Cooldown_MedicRevive.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_Cooldown_TacticalSprint.cpp(1): fatal error C1083: Cannot open include file: 'GE_Cooldown_TacticalSprint.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_StaminaCost_BurstFire.cpp(1): fatal error C1083: Cannot open include file: 'GE_StaminaCost_BurstFire.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_JammedCommsDebuff.cpp(1): fatal error C1083: Cannot open include file: 'GE_JammedCommsDebuff.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_SuppressedDebuff.cpp(1): fatal error C1083: Cannot open include file: 'GE_SuppressedDebuff.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_StaminaCost_TacticalSprint.cpp(1): fatal error C1083: Cannot open include file: 'GE_StaminaCost_TacticalSprint.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(12): error C2757: 'Movement': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(17): error C2757: 'Support': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(22): error C2757: 'Utility': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(27): error C2757: 'Weapon': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(38): error C2757: 'Movement': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(42): error C2757: 'Support': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(46): error C2757: 'Weapon': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(57): error C2757: 'Movement': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(61): error C2757: 'Utility': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(66): error C2757: 'Weapon': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(77): error C2757: 'LifeState': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(85): error C2757: 'Posture': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(91): error C2757: 'RoleTag': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(98): error C2757: 'TeamTag': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(109): error C2757: 'Combat': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(115): error C2757: 'Squad': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(126): error C2757: 'Intel': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(131): error C2757: 'Mobility': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(136): error C2757: 'Utility': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
 
[TRUNCATED: 36 more errors not shown] 
 
Total errors found: 86 
 
 
------------- 
TacticalRaidGAS - UE5 BUILD 
------------- 
Parsing command line: BuildPlugin -Plugin=M:\Code\Factory\TacticalRaidGAS\TacticalRaidGAS.uplugin -Package=M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
  Running Internal UnrealHeaderTool M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\HostProject.uproject M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest -WarningsAsErrors -installed 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\ERaidPhase.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\ATacticalReconDrone.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\ATacticalRaidDirector.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\EThreatBand.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\FTacticalRaidBalance.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\FSquadStateComponent.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\FTacticalVisionComponent.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(12): error C2757: 'Movement': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(17): error C2757: 'Support': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(22): error C2757: 'Utility': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(27): error C2757: 'Weapon': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(38): error C2757: 'Movement': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(42): error C2757: 'Support': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(46): error C2757: 'Weapon': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(57): error C2757: 'Movement': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(61): error C2757: 'Utility': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(66): error C2757: 'Weapon': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(77): error C2757: 'LifeState': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(85): error C2757: 'Posture': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(91): error C2757: 'RoleTag': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(98): error C2757: 'TeamTag': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(109): error C2757: 'Combat': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(115): error C2757: 'Squad': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(126): error C2757: 'Intel': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(131): error C2757: 'Mobility': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(136): error C2757: 'Utility': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(146): error C2757: 'Combat': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(151): error C2757: 'Support': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(161): error C2757: 'Combat': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(165): error C2757: 'Movement': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(169): error C2757: 'Utility': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Abilities\GA_AirburstGrenade.cpp(25): error C2653: 'UGE_StaminaCost_BurstFire': is not a class or namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Abilities\GA_AirburstGrenade.cpp(28): error C2653: 'UGE_Cooldown_AirburstGrenade': is not a class or namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(12): error C2757: 'Movement': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(17): error C2757: 'Support': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(22): error C2757: 'Utility': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(27): error C2757: 'Weapon': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(38): error C2757: 'Movement': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(42): error C2757: 'Support': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(46): error C2757: 'Weapon': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(57): error C2757: 'Movement': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(61): error C2757: 'Utility': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(66): error C2757: 'Weapon': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(77): error C2757: 'LifeState': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(85): error C2757: 'Posture': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(91): error C2757: 'RoleTag': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(98): error C2757: 'TeamTag': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Public\GameplayTags.h(109): error C2757: 'Combat': a symbol with this name already exists and therefore this name cannot be used as a namespace name 
 
[TRUNCATED: 563 more errors not shown] 
 
Total errors found: 613 
 
 
------------- 
TacticalRaidGAS - UE5 BUILD 
------------- 
Parsing command line: BuildPlugin -Plugin=M:\Code\Factory\TacticalRaidGAS\TacticalRaidGAS.uplugin -Package=M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
  Running Internal UnrealHeaderTool M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\HostProject.uproject M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest -WarningsAsErrors -installed 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\ERaidPhase.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\ATacticalReconDrone.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\EThreatBand.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\ATacticalRaidDirector.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\FTacticalRaidBalance.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\FSquadStateComponent.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\FTacticalVisionComponent.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Abilities\GA_AirburstGrenade.cpp(25): error C2653: 'UGE_StaminaCost_BurstFire': is not a class or namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Abilities\GA_AirburstGrenade.cpp(28): error C2653: 'UGE_Cooldown_AirburstGrenade': is not a class or namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Abilities\GA_BurstFire.cpp(33): error C2653: 'UGE_StaminaCost_BurstFire': is not a class or namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Abilities\GA_BurstFire.cpp(36): error C2653: 'UGE_Cooldown_BurstFire': is not a class or namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Abilities\GA_JamComms.cpp(24): error C2653: 'UGE_Cooldown_JamComms': is not a class or namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Abilities\GA_MedicRevive.cpp(30): error C2653: 'UGE_StaminaCost_BurstFire': is not a class or namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Abilities\GA_MedicRevive.cpp(33): error C2653: 'UGE_Cooldown_MedicRevive': is not a class or namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Abilities\GA_PingTarget.cpp(25): error C2653: 'UGE_Cooldown_PingTarget': is not a class or namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Abilities\GA_TacticalSprint.cpp(24): error C2653: 'UGE_StaminaCost_TacticalSprint': is not a class or namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Abilities\GA_TacticalSprint.cpp(27): error C2653: 'UGE_Cooldown_TacticalSprint': is not a class or namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_JammedCommsDebuff.cpp(13): error C2653: 'UUnknownSet': is not a class or namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_JammedCommsDebuff.cpp(13): error C3861: 'GetAbilityHasteAttribute': identifier not found 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_JammedCommsDebuff.cpp(14): error C2039: 'Multiplicative': is not a member of 'EGameplayModOp' 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_JammedCommsDebuff.cpp(14): error C2065: 'Multiplicative': undeclared identifier 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_StaminaCost_BurstFire.cpp(13): error C2653: 'UUnknownSet': is not a class or namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_StaminaCost_BurstFire.cpp(13): error C3861: 'GetStaminaAttribute': identifier not found 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_StaminaCost_TacticalSprint.cpp(13): error C2653: 'UUnknownSet': is not a class or namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_StaminaCost_TacticalSprint.cpp(13): error C3861: 'GetStaminaAttribute': identifier not found 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_SuppressedDebuff.cpp(17): error C2653: 'UUnknownSet': is not a class or namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_SuppressedDebuff.cpp(17): error C3861: 'GetMoveSpeedAttribute': identifier not found 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_SuppressedDebuff.cpp(18): error C2039: 'Multiplicative': is not a member of 'EGameplayModOp' 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_SuppressedDebuff.cpp(18): error C2065: 'Multiplicative': undeclared identifier 
UnrealBuildTool failed. See log for more details. (C:\Users\Admin\AppData\Roaming\Unreal Engine\AutomationTool\Logs\D+Unreal+UE_5.4\UBA-UnrealEditor-Win64-Development.txt) 
BUILD FAILED 
 
Total errors found: 33 
 
 
------------- 
TacticalRaidGAS - UE5 BUILD 
------------- 
Parsing command line: BuildPlugin -Plugin=m:\Code\Factory\TacticalRaidGAS\TacticalRaidGAS.uplugin -Package=m:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
  Running Internal UnrealHeaderTool M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\HostProject.uproject M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest -WarningsAsErrors -installed 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\ERaidPhase.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\EThreatBand.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\ATacticalReconDrone.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\ATacticalRaidDirector.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\FTacticalRaidBalance.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\FSquadStateComponent.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\FTacticalVisionComponent.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Abilities\GA_AirburstGrenade.cpp(25): error C2653: 'UGE_StaminaCost_BurstFire': is not a class or namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Abilities\GA_AirburstGrenade.cpp(28): error C2653: 'UGE_Cooldown_AirburstGrenade': is not a class or namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Abilities\GA_BurstFire.cpp(33): error C2653: 'UGE_StaminaCost_BurstFire': is not a class or namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Abilities\GA_BurstFire.cpp(36): error C2653: 'UGE_Cooldown_BurstFire': is not a class or namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Abilities\GA_JamComms.cpp(24): error C2653: 'UGE_Cooldown_JamComms': is not a class or namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Abilities\GA_MedicRevive.cpp(30): error C2653: 'UGE_StaminaCost_BurstFire': is not a class or namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Abilities\GA_MedicRevive.cpp(33): error C2653: 'UGE_Cooldown_MedicRevive': is not a class or namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Abilities\GA_PingTarget.cpp(25): error C2653: 'UGE_Cooldown_PingTarget': is not a class or namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Abilities\GA_TacticalSprint.cpp(24): error C2653: 'UGE_StaminaCost_TacticalSprint': is not a class or namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Abilities\GA_TacticalSprint.cpp(27): error C2653: 'UGE_Cooldown_TacticalSprint': is not a class or namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_JammedCommsDebuff.cpp(13): error C2653: 'UUnknownSet': is not a class or namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_JammedCommsDebuff.cpp(13): error C3861: 'GetAbilityHasteAttribute': identifier not found 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_JammedCommsDebuff.cpp(14): error C2039: 'Multiplicative': is not a member of 'EGameplayModOp' 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_JammedCommsDebuff.cpp(14): error C2065: 'Multiplicative': undeclared identifier 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_StaminaCost_BurstFire.cpp(13): error C2653: 'UUnknownSet': is not a class or namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_StaminaCost_BurstFire.cpp(13): error C3861: 'GetStaminaAttribute': identifier not found 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_StaminaCost_TacticalSprint.cpp(13): error C2653: 'UUnknownSet': is not a class or namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_StaminaCost_TacticalSprint.cpp(13): error C3861: 'GetStaminaAttribute': identifier not found 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_SuppressedDebuff.cpp(17): error C2653: 'UUnknownSet': is not a class or namespace name 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_SuppressedDebuff.cpp(17): error C3861: 'GetMoveSpeedAttribute': identifier not found 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_SuppressedDebuff.cpp(18): error C2039: 'Multiplicative': is not a member of 'EGameplayModOp' 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\Effects\GE_SuppressedDebuff.cpp(18): error C2065: 'Multiplicative': undeclared identifier 
UnrealBuildTool failed. See log for more details. (C:\Users\Admin\AppData\Roaming\Unreal Engine\AutomationTool\Logs\D+Unreal+UE_5.4\UBA-UnrealEditor-Win64-Development.txt) 
BUILD FAILED 
 
Total errors found: 33 
 
 
------------- 
TacticalRaidGAS - UE5 BUILD 
------------- 
Parsing command line: BuildPlugin -Plugin=M:\Code\Factory\TacticalRaidGAS\TacticalRaidGAS.uplugin -Package=M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
  Running Internal UnrealHeaderTool M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\HostProject.uproject M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest -WarningsAsErrors -installed 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\ERaidPhase.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\ATacticalReconDrone.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\EThreatBand.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\ATacticalRaidDirector.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\FTacticalRaidBalance.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\FSquadStateComponent.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\FTacticalVisionComponent.cpp(5): fatal error C1083: Cannot open include file: 'TacticalRaidGASBlueprintLibrary.h': No such file or directory 
UnrealBuildTool failed. See log for more details. (C:\Users\Admin\AppData\Roaming\Unreal Engine\AutomationTool\Logs\D+Unreal+UE_5.4\UBA-UnrealEditor-Win64-Development.txt) 
BUILD FAILED 
 
Total errors found: 11 
 
 
------------- 
TacticalRaidGAS - UE5 BUILD 
------------- 
Parsing command line: BuildPlugin -Plugin=M:\Code\Factory\TacticalRaidGAS\TacticalRaidGAS.uplugin -Package=M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
  Running Internal UnrealHeaderTool M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\HostProject.uproject M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest -WarningsAsErrors -installed 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\ATacticalReconDrone.cpp(50): error C3861: 'send_gameplay_event': identifier not found 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\ATacticalReconDrone.cpp(66): error C3861: 'send_gameplay_event': identifier not found 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\ATacticalRaidDirector.cpp(78): error C3861: 'send_gameplay_event': identifier not found 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Source\Private\ATacticalRaidDirector.cpp(83): error C3861: 'send_gameplay_event': identifier not found 
UnrealBuildTool failed. See log for more details. (C:\Users\Admin\AppData\Roaming\Unreal Engine\AutomationTool\Logs\D+Unreal+UE_5.4\UBA-UnrealEditor-Win64-Development.txt) 
BUILD FAILED 
 
Total errors found: 8 
 
 
------------- 
TacticalRaidGAS - UE5 BUILD 
------------- 
Parsing command line: BuildPlugin -Plugin=M:\Code\Factory\TacticalRaidGAS\TacticalRaidGAS.uplugin -Package=M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
  Running Internal UnrealHeaderTool M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\HostProject.uproject M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest -WarningsAsErrors -installed 
Module.TacticalRaidGAS.cpp.obj : error LNK2001: unresolved external symbol "protected: virtual void __cdecl UGameplayTask::OnGameplayTaskDeactivated(class UGameplayTask &)" (?OnGameplayTaskDeactivated@UGameplayTask@@MEAAXAEAV1@@Z) 
WaitForSquadConfirmTask.cpp.obj : error LNK2001: unresolved external symbol "protected: virtual void __cdecl UGameplayTask::OnGameplayTaskDeactivated(class UGameplayTask &)" (?OnGameplayTaskDeactivated@UGameplayTask@@MEAAXAEAV1@@Z) 
Module.TacticalRaidGAS.cpp.obj : error LNK2001: unresolved external symbol "protected: virtual void __cdecl UGameplayTask::OnGameplayTaskInitialized(class UGameplayTask &)" (?OnGameplayTaskInitialized@UGameplayTask@@MEAAXAEAV1@@Z) 
WaitForSquadConfirmTask.cpp.obj : error LNK2001: unresolved external symbol "protected: virtual void __cdecl UGameplayTask::OnGameplayTaskInitialized(class UGameplayTask &)" (?OnGameplayTaskInitialized@UGameplayTask@@MEAAXAEAV1@@Z) 
Module.TacticalRaidGAS.cpp.obj : error LNK2001: unresolved external symbol "protected: virtual unsigned char __cdecl UGameplayTask::GetGameplayTaskDefaultPriority(void)const " (?GetGameplayTaskDefaultPriority@UGameplayTask@@MEBAEXZ) 
WaitForSquadConfirmTask.cpp.obj : error LNK2001: unresolved external symbol "protected: virtual unsigned char __cdecl UGameplayTask::GetGameplayTaskDefaultPriority(void)const " (?GetGameplayTaskDefaultPriority@UGameplayTask@@MEBAEXZ) 
Module.TacticalRaidGAS.cpp.obj : error LNK2001: unresolved external symbol "protected: virtual class AActor * __cdecl UGameplayTask::GetGameplayTaskAvatar(class UGameplayTask const *)const " (?GetGameplayTaskAvatar@UGameplayTask@@MEBAPEAVAActor@@PEBV1@@Z) 
WaitForSquadConfirmTask.cpp.obj : error LNK2001: unresolved external symbol "protected: virtual class AActor * __cdecl UGameplayTask::GetGameplayTaskAvatar(class UGameplayTask const *)const " (?GetGameplayTaskAvatar@UGameplayTask@@MEBAPEAVAActor@@PEBV1@@Z) 
Module.TacticalRaidGAS.cpp.obj : error LNK2001: unresolved external symbol "protected: virtual class AActor * __cdecl UGameplayTask::GetGameplayTaskOwner(class UGameplayTask const *)const " (?GetGameplayTaskOwner@UGameplayTask@@MEBAPEAVAActor@@PEBV1@@Z) 
WaitForSquadConfirmTask.cpp.obj : error LNK2001: unresolved external symbol "protected: virtual class AActor * __cdecl UGameplayTask::GetGameplayTaskOwner(class UGameplayTask const *)const " (?GetGameplayTaskOwner@UGameplayTask@@MEBAPEAVAActor@@PEBV1@@Z) 
Module.TacticalRaidGAS.cpp.obj : error LNK2001: unresolved external symbol "protected: virtual class UGameplayTasksComponent * __cdecl UGameplayTask::GetGameplayTasksComponent(class UGameplayTask const &)const " (?GetGameplayTasksComponent@UGameplayTask@@MEBAPEAVUGameplayTasksComponent@@AEBV1@@Z) 
WaitForSquadConfirmTask.cpp.obj : error LNK2001: unresolved external symbol "protected: virtual class UGameplayTasksComponent * __cdecl UGameplayTask::GetGameplayTasksComponent(class UGameplayTask const &)const " (?GetGameplayTasksComponent@UGameplayTask@@MEBAPEAVUGameplayTasksComponent@@AEBV1@@Z) 
Module.TacticalRaidGAS.cpp.obj : error LNK2001: unresolved external symbol "public: virtual class FString __cdecl UGameplayTask::GenerateDebugDescription(void)const " (?GenerateDebugDescription@UGameplayTask@@UEBA?AVFString@@XZ) 
WaitForSquadConfirmTask.cpp.obj : error LNK2001: unresolved external symbol "public: virtual class FString __cdecl UGameplayTask::GenerateDebugDescription(void)const " (?GenerateDebugDescription@UGameplayTask@@UEBA?AVFString@@XZ) 
Module.TacticalRaidGAS.cpp.obj : error LNK2001: unresolved external symbol "protected: virtual void __cdecl UGameplayTask::Resume(void)" (?Resume@UGameplayTask@@MEAAXXZ) 
WaitForSquadConfirmTask.cpp.obj : error LNK2001: unresolved external symbol "protected: virtual void __cdecl UGameplayTask::Resume(void)" (?Resume@UGameplayTask@@MEAAXXZ) 
Module.TacticalRaidGAS.cpp.obj : error LNK2001: unresolved external symbol "protected: virtual void __cdecl UGameplayTask::Pause(void)" (?Pause@UGameplayTask@@MEAAXXZ) 
WaitForSquadConfirmTask.cpp.obj : error LNK2001: unresolved external symbol "protected: virtual void __cdecl UGameplayTask::Pause(void)" (?Pause@UGameplayTask@@MEAAXXZ) 
Module.TacticalRaidGAS.cpp.obj : error LNK2001: unresolved external symbol "public: virtual class FString __cdecl UGameplayTask::GetDebugString(void)const " (?GetDebugString@UGameplayTask@@UEBA?AVFString@@XZ) 
WaitForSquadConfirmTask.cpp.obj : error LNK2001: unresolved external symbol "public: virtual class FString __cdecl UGameplayTask::GetDebugString(void)const " (?GetDebugString@UGameplayTask@@UEBA?AVFString@@XZ) 
Module.TacticalRaidGAS.cpp.obj : error LNK2001: unresolved external symbol "public: virtual void __cdecl UGameplayTask::ExternalCancel(void)" (?ExternalCancel@UGameplayTask@@UEAAXXZ) 
WaitForSquadConfirmTask.cpp.obj : error LNK2001: unresolved external symbol "public: virtual void __cdecl UGameplayTask::ExternalCancel(void)" (?ExternalCancel@UGameplayTask@@UEAAXXZ) 
Module.TacticalRaidGAS.cpp.obj : error LNK2001: unresolved external symbol "public: virtual void __cdecl UGameplayTask::ExternalConfirm(bool)" (?ExternalConfirm@UGameplayTask@@UEAAX_N@Z) 
WaitForSquadConfirmTask.cpp.obj : error LNK2001: unresolved external symbol "public: virtual void __cdecl UGameplayTask::ExternalConfirm(bool)" (?ExternalConfirm@UGameplayTask@@UEAAX_N@Z) 
Module.TacticalRaidGAS.cpp.obj : error LNK2001: unresolved external symbol "protected: virtual void __cdecl UGameplayTask::RegisterReplicationFragments(class UE::Net::FFragmentRegistrationContext &,enum UE::Net::EFragmentRegistrationFlags)" (?RegisterReplicationFragments@UGameplayTask@@MEAAXAEAVFFragmentRegistrationContext@Net@UE@@W4EFragmentRegistrationFlags@34@@Z) 
WaitForSquadConfirmTask.cpp.obj : error LNK2001: unresolved external symbol "protected: virtual void __cdecl UGameplayTask::RegisterReplicationFragments(class UE::Net::FFragmentRegistrationContext &,enum UE::Net::EFragmentRegistrationFlags)" (?RegisterReplicationFragments@UGameplayTask@@MEAAXAEAVFFragmentRegistrationContext@Net@UE@@W4EFragmentRegistrationFlags@34@@Z) 
Module.TacticalRaidGAS.cpp.obj : error LNK2001: unresolved external symbol "public: virtual class UWorld * __cdecl UGameplayTask::GetWorld(void)const " (?GetWorld@UGameplayTask@@UEBAPEAVUWorld@@XZ) 
WaitForSquadConfirmTask.cpp.obj : error LNK2001: unresolved external symbol "public: virtual class UWorld * __cdecl UGameplayTask::GetWorld(void)const " (?GetWorld@UGameplayTask@@UEBAPEAVUWorld@@XZ) 
WaitForSquadConfirmTask.cpp.obj : error LNK2019: unresolved external symbol "__declspec(dllimport) protected: virtual void __cdecl UGameplayTask::Activate(void)" (__imp_?Activate@UGameplayTask@@MEAAXXZ) referenced in function "public: virtual void __cdecl UWaitForSquadConfirmTask::Activate(void)" (?Activate@UWaitForSquadConfirmTask@@UEAAXXZ) 
WaitForSquadConfirmTask.cpp.obj : error LNK2019: unresolved external symbol "__declspec(dllimport) protected: void __cdecl UGameplayTask::InitTask(class IGameplayTaskOwnerInterface &,unsigned char)" (__imp_?InitTask@UGameplayTask@@IEAAXAEAVIGameplayTaskOwnerInterface@@E@Z) referenced in function "public: static class UWaitForSquadConfirmTask * __cdecl UWaitForSquadConfirmTask::CreateWaitForSquadConfirmTask(class UGameplayAbility *)" (?CreateWaitForSquadConfirmTask@UWaitForSquadConfirmTask@@SAPEAV1@PEAVUGameplayAbility@@@Z) 
M:\Code\Factory\TacticalRaidGAS\_Builds\TacticalRaidGAS_5.4\HostProject\Plugins\TacticalRaidGAS\Binaries\Win64\UnrealEditor-TacticalRaidGAS.dll : fatal error LNK1120: 16 unresolved externals 
UnrealBuildTool failed. See log for more details. (C:\Users\Admin\AppData\Roaming\Unreal Engine\AutomationTool\Logs\D+Unreal+UE_5.4\UBA-UnrealEditor-Win64-Development.txt) 
BUILD FAILED 
 
Total errors found: 35 
 
 
------------- 
Cosmos - UE5 BUILD 
------------- 
Parsing command line: BuildPlugin -Plugin=m:\Code\Factory\Cosmos\Cosmos\Cosmos.uplugin -Package=m:\Code\Factory\Cosmos\_Builds\Cosmos_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
  Running Internal UnrealHeaderTool M:\Code\Factory\Cosmos\_Builds\Cosmos_5.4\HostProject\HostProject.uproject M:\Code\Factory\Cosmos\_Builds\Cosmos_5.4\HostProject\Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest -WarningsAsErrors -installed 
M:\Code\Factory\Cosmos\_Builds\Cosmos_5.4\HostProject\Plugins\Cosmos\Source\Cosmos\Private\AKainSovereignPlanet.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cosmos\_Builds\Cosmos_5.4\HostProject\Plugins\Cosmos\Source\Cosmos\Private\CosmosBlueprintLibrary.cpp(5): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cosmos\_Builds\Cosmos_5.4\HostProject\Plugins\Cosmos\Source\Cosmos\Private\EFactionStance.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cosmos\_Builds\Cosmos_5.4\HostProject\Plugins\Cosmos\Source\Cosmos\Private\EEditorToolMode.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cosmos\_Builds\Cosmos_5.4\HostProject\Plugins\Cosmos\Source\Cosmos\Private\EPlanetArchetype.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cosmos\_Builds\Cosmos_5.4\HostProject\Plugins\Cosmos\Source\Cosmos\Private\EOrbitalDrift.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cosmos\_Builds\Cosmos_5.4\HostProject\Plugins\Cosmos\Source\Cosmos\Private\EResourceTier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cosmos\_Builds\Cosmos_5.4\HostProject\Plugins\Cosmos\Source\Cosmos\Private\FClimateComponent.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cosmos\_Builds\Cosmos_5.4\HostProject\Plugins\Cosmos\Source\Cosmos\Private\FClimateState.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cosmos\_Builds\Cosmos_5.4\HostProject\Plugins\Cosmos\Source\Cosmos\Private\FOrbitVectors.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cosmos\_Builds\Cosmos_5.4\HostProject\Plugins\Cosmos\Source\Cosmos\Private\FOrbitalPhysicsComponent.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cosmos\_Builds\Cosmos_5.4\HostProject\Plugins\Cosmos\Source\Cosmos\Private\FPlanetDefinition.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cosmos\_Builds\Cosmos_5.4\HostProject\Plugins\Cosmos\Source\Cosmos\Private\FResourceLatticeComponent.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cosmos\_Builds\Cosmos_5.4\HostProject\Plugins\Cosmos\Source\Cosmos\Private\FResourceVein.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cosmos\_Builds\Cosmos_5.4\HostProject\Plugins\Cosmos\Source\Cosmos\Private\FSovereignComponent.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
UnrealBuildTool failed. See log for more details. (C:\Users\Admin\AppData\Roaming\Unreal Engine\AutomationTool\Logs\D+Unreal+UE_5.4\UBA-UnrealEditor-Win64-Development.txt) 
BUILD FAILED 
 
Total errors found: 19 
 
 
------------- 
MetaHumanVAT - UE5 BUILD 
------------- 
Parsing command line: BuildPlugin -Plugin=M:\Code\Factory\MetaHumanVAT\MetaHumanVAT.uplugin -Package=M:\Code\Factory\MetaHumanVAT\_Builds\MetaHumanVAT_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
  Running Internal UnrealHeaderTool M:\Code\Factory\MetaHumanVAT\_Builds\MetaHumanVAT_5.4\HostProject\HostProject.uproject M:\Code\Factory\MetaHumanVAT\_Builds\MetaHumanVAT_5.4\HostProject\Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest -WarningsAsErrors -installed 
M:\Code\Factory\MetaHumanVAT\_Builds\MetaHumanVAT_5.4\HostProject\Plugins\MetaHumanVAT\Source\MetaHumanVAT\Private\EVATQualityPreset.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\MetaHumanVAT\_Builds\MetaHumanVAT_5.4\HostProject\Plugins\MetaHumanVAT\Source\MetaHumanVAT\Private\EVATCompressionMode.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\MetaHumanVAT\_Builds\MetaHumanVAT_5.4\HostProject\Plugins\MetaHumanVAT\Source\MetaHumanVAT\Private\AVATPreviewActor.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\MetaHumanVAT\_Builds\MetaHumanVAT_5.4\HostProject\Plugins\MetaHumanVAT\Source\MetaHumanVAT\Private\FVATBakeSettings.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\MetaHumanVAT\_Builds\MetaHumanVAT_5.4\HostProject\Plugins\MetaHumanVAT\Source\MetaHumanVAT\Private\AVATCrowdSpawner.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\MetaHumanVAT\_Builds\MetaHumanVAT_5.4\HostProject\Plugins\MetaHumanVAT\Source\MetaHumanVAT\Private\EVATPlaybackMode.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\MetaHumanVAT\_Builds\MetaHumanVAT_5.4\HostProject\Plugins\MetaHumanVAT\Source\MetaHumanVAT\Private\EVATBakeState.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\MetaHumanVAT\_Builds\MetaHumanVAT_5.4\HostProject\Plugins\MetaHumanVAT\Source\MetaHumanVAT\Private\FVATInstancedCrowdComponent.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\MetaHumanVAT\_Builds\MetaHumanVAT_5.4\HostProject\Plugins\MetaHumanVAT\Source\MetaHumanVAT\Private\FVATMaterialSettings.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\MetaHumanVAT\_Builds\MetaHumanVAT_5.4\HostProject\Plugins\MetaHumanVAT\Source\MetaHumanVAT\Private\FVATBakerComponent.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\MetaHumanVAT\_Builds\MetaHumanVAT_5.4\HostProject\Plugins\MetaHumanVAT\Source\MetaHumanVAT\Private\FVATPlaybackComponent.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\MetaHumanVAT\_Builds\MetaHumanVAT_5.4\HostProject\Plugins\MetaHumanVAT\Source\MetaHumanVAT\Private\FVATQualityProfile.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\MetaHumanVAT\_Builds\MetaHumanVAT_5.4\HostProject\Plugins\MetaHumanVAT\Source\MetaHumanVAT\Private\MetaHumanVATBlueprintLibrary.cpp(5): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
UnrealBuildTool failed. See log for more details. (C:\Users\Admin\AppData\Roaming\Unreal Engine\AutomationTool\Logs\D+Unreal+UE_5.4\UBA-UnrealEditor-Win64-Development.txt) 
BUILD FAILED 
 
Total errors found: 17 
 
 
------------- 
Temporal - KAIN COMPILATION 
------------- 
 
 KAIN Compiler v0.1.0 (build 1772188785)
🚀 Building UE5 Plugin: Temporal
📍 Plugin directory: 

📚 Loaded stdlib from: M:\Code\Kain\stdlib\ue5
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
 
 
------------- 
UESculpt - UE5 BUILD 
------------- 
Parsing command line: BuildPlugin -Plugin=M:\Code\Factory\UESculpt\UESculpt.uplugin -Package=M:\Code\Factory\UESculpt\_Builds\UESculpt_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
  Running Internal UnrealHeaderTool M:\Code\Factory\UESculpt\_Builds\UESculpt_5.4\HostProject\HostProject.uproject M:\Code\Factory\UESculpt\_Builds\UESculpt_5.4\HostProject\Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest -WarningsAsErrors -installed 
M:\Code\Factory\UESculpt\_Builds\UESculpt_5.4\HostProject\Plugins\UESculpt\Source\UESculpt\Private\ESculptLayerBlendMode.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\UESculpt\_Builds\UESculpt_5.4\HostProject\Plugins\UESculpt\Source\UESculpt\Private\FSculptBrushPreset.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\UESculpt\_Builds\UESculpt_5.4\HostProject\Plugins\UESculpt\Source\UESculpt\Private\ESculptSymmetryMode.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\UESculpt\_Builds\UESculpt_5.4\HostProject\Plugins\UESculpt\Source\UESculpt\Private\ESculptSubdivisionMode.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\UESculpt\_Builds\UESculpt_5.4\HostProject\Plugins\UESculpt\Source\UESculpt\Private\ESculptBrushType.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\UESculpt\_Builds\UESculpt_5.4\HostProject\Plugins\UESculpt\Source\UESculpt\Private\FSculptMaterialPreset.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\UESculpt\_Builds\UESculpt_5.4\HostProject\Plugins\UESculpt\Source\UESculpt\Private\FSculptHistoryComponent.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\UESculpt\_Builds\UESculpt_5.4\HostProject\Plugins\UESculpt\Source\UESculpt\Private\ASculptMesh.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\UESculpt\_Builds\UESculpt_5.4\HostProject\Plugins\UESculpt\Source\UESculpt\Private\FSculptMeshComponent.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\UESculpt\_Builds\UESculpt_5.4\HostProject\Plugins\UESculpt\Source\UESculpt\Private\UESculptBlueprintLibrary.cpp(5): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
UnrealBuildTool failed. See log for more details. (C:\Users\Admin\AppData\Roaming\Unreal Engine\AutomationTool\Logs\D+Unreal+UE_5.4\UBA-UnrealEditor-Win64-Development.txt) 
BUILD FAILED 
 
Total errors found: 14 
 
 
------------- 
PokeredFirmwareSmoke - KAIN COMPILATION 
------------- 
 
 KAIN Compiler v0.1.0 (build 1772209264)
🚀 Building UE5 Plugin: PokeredFirmwareSmoke
📍 Plugin directory: .

📚 Loaded stdlib from: m:\Code\Kain\stdlib\ue5
📁 Source files: 13 (stdlib: 12, user: 1)
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
      1. sm64_all.kn

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
   ✓ sm64_all.kn validated

   ℹ️  Stdlib merge: 409 total → 0 kept (409 pruned by tree-shake, 0 shadowed by user code)
🔍 Type checking merged program...
Runtime error: ❌ Type error in merged program: Type error at Span { start: 0, end: 0 }: actor.kn:1:1: Item type not yet supported in type checker
 
 
------------- 
ZenMograph - UE5 BUILD 
------------- 
Parsing command line: BuildPlugin -Plugin=m:\Code\Factory\Cinema4DMograph\ZenMograph\ZenMograph.uplugin -Package=m:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
  Running Internal UnrealHeaderTool M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\HostProject.uproject M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest -WarningsAsErrors -installed 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\AClonerActor.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\EAudioMode.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\EEasingType.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\AClonerEffectorSubsystem.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\EEffectorShape.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\ESkeletalMode.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\EClonerMode.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\EMeshSampleMode.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FAttractModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FBounceModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FBakeSettings.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FBakeResult.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FClonerData.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FClonerAnimationComponent.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FClonerEffectorComponent.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FClonerInstanceComponent.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FClonerNiagaraDataInterface.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FClonerPresetData.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FClonerPerformanceComponent.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FClonerTargetComponent.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FClonerVFXComponent.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FDelayModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FColorModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FDistributionLayer.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FEffectorData.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FFloatModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FExpressionModifierPreset.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FFigure8Modifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FLissajousModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FGravityModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FInstanceCache.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FElasticModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FModifierBase.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FModifierPresetData.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FPendulumModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FPresetVariable.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FNoiseModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FModifierState.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FOrbitModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FPulseModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FPushModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FRandomModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FTumbleModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FSwayModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FStepModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FShakeModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FTargetModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FVortexModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
 
[TRUNCATED: 4 more errors not shown] 
 
Total errors found: 54 
 
 
------------- 
PokeredFirmwareSmoke - KAIN COMPILATION 
------------- 
 
 KAIN Compiler v0.1.0 (build 1772209264)
🚀 Building UE5 Plugin: PokeredFirmwareSmoke
📍 Plugin directory: .

📚 Loaded stdlib from: M:\Code\Kain\stdlib\ue5
📁 Source files: 13 (stdlib: 12, user: 1)
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
      1. sm64_all.kn

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
   ✓ sm64_all.kn validated

   ℹ️  Stdlib merge: 409 total → 0 kept (409 pruned by tree-shake, 0 shadowed by user code)
🔍 Type checking merged program...
Runtime error: ❌ Type error in merged program: Type error at Span { start: 0, end: 0 }: actor.kn:1:1: Item type not yet supported in type checker
 
 
------------- 
ZenMograph - UE5 BUILD 
------------- 
Parsing command line: BuildPlugin -Plugin=m:\Code\Factory\Cinema4DMograph\ZenMograph\ZenMograph.uplugin -Package=m:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
  Running Internal UnrealHeaderTool M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\HostProject.uproject M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest -WarningsAsErrors -installed 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\AClonerEffectorSubsystem.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\EMeshSampleMode.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\EClonerMode.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\EEasingType.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\AClonerActor.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\EEffectorShape.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\EAudioMode.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FBakeResult.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\ESkeletalMode.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FAttractModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FBakeSettings.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FBounceModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FClonerAnimationComponent.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FClonerData.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FClonerInstanceComponent.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FClonerEffectorComponent.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FClonerPerformanceComponent.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FClonerTargetComponent.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FClonerPresetData.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FClonerNiagaraDataInterface.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FClonerVFXComponent.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FDistributionLayer.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FColorModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FExpressionModifierPreset.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FEffectorData.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FDelayModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FElasticModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FFigure8Modifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FLissajousModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FGravityModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FInstanceCache.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FFloatModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FModifierPresetData.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FModifierBase.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FModifierState.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FOrbitModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FPushModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FPulseModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FPendulumModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FPresetVariable.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FNoiseModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FRandomModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FTumbleModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FSwayModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FTargetModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FVortexModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FStepModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FShakeModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
 
[TRUNCATED: 4 more errors not shown] 
 
Total errors found: 54 
 
 
------------- 
FluidFlow - UE5 BUILD 
------------- 
Parsing command line: BuildPlugin -Plugin=m:\Code\Factory\FluidFlow\FluidFlow.uplugin -Package=m:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
  Running Internal UnrealHeaderTool M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\HostProject.uproject M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest -WarningsAsErrors -installed 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\AHyperFluidEmitter.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\AHyperFluidWorld.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\AHyperFluidProbe.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\AHyperFluidController.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\EAdaptiveStrategy.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\EAdvectionScheme.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\EBoundaryType.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\EBreakupModel.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\ECacheStrategy.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\ECoalescenceModel.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\EConvergenceCriteria.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\ECollisionModel.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\ECouplingField.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\ECouplingStrategy.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\EDomainDecomposition.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\EDragModel.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\EEmissionShape.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\EEvaporationModel.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\EFSIMethod.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\EFluidClass.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\EGPUBackend.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\EHybridSolver.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\ELESModel.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\ELoadBalancingStrategy.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\EParticleType.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\EPrecisionMode.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\EPressureProfile.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\EPressureSolver.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\EQualityTier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\ERANSModel.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\ERadiationModel.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\EReactionMechanism.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\EReynoldsStressModel.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\ESolverFamily.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\ESyntheticTurbulence.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\ETemperatureProfile.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\ETimeIntegrator.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\ETrackingScheme.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\ETransitionModel.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\ETurbulenceModel.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\EValidationCaseType.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\EVisualizationMode.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\EVelocityProfile.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\EWallFunction.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\FAdaptiveMeshComponent.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\FBoundaryConditionComponent.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\FBoundaryProfile.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\FluidFlow\_Builds\FluidFlow_5.4\HostProject\Plugins\FluidFlow\Source\FluidFlow\Private\FBoundaryShaderParams.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
 
[TRUNCATED: 42 more errors not shown] 
 
Total errors found: 92 
 
 
------------- 
CrowdFlowDirector - UE5 BUILD 
------------- 
Parsing command line: BuildPlugin -Plugin=m:\Code\Factory\CrowdFlowDirector\CrowdFlowDirector.uplugin -Package=m:\Code\Factory\CrowdFlowDirector\_Builds\CrowdFlowDirector_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
  Running Internal UnrealHeaderTool M:\Code\Factory\CrowdFlowDirector\_Builds\CrowdFlowDirector_5.4\HostProject\HostProject.uproject M:\Code\Factory\CrowdFlowDirector\_Builds\CrowdFlowDirector_5.4\HostProject\Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest -WarningsAsErrors -installed 
M:\Code\Factory\CrowdFlowDirector\_Builds\CrowdFlowDirector_5.4\HostProject\Plugins\CrowdFlowDirector\Source\CrowdFlowDirector\Private\ECrowdFlowFieldMode.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\CrowdFlowDirector\_Builds\CrowdFlowDirector_5.4\HostProject\Plugins\CrowdFlowDirector\Source\CrowdFlowDirector\Private\ECrowdEffectorShape.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\CrowdFlowDirector\_Builds\CrowdFlowDirector_5.4\HostProject\Plugins\CrowdFlowDirector\Source\CrowdFlowDirector\Private\ACrowdFlowDirectorActor.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\CrowdFlowDirector\_Builds\CrowdFlowDirector_5.4\HostProject\Plugins\CrowdFlowDirector\Source\CrowdFlowDirector\Private\CrowdFlowDirectorBlueprintLibrary.cpp(5): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\CrowdFlowDirector\_Builds\CrowdFlowDirector_5.4\HostProject\Plugins\CrowdFlowDirector\Source\CrowdFlowDirector\Private\ECrowdDebugView.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\CrowdFlowDirector\_Builds\CrowdFlowDirector_5.4\HostProject\Plugins\CrowdFlowDirector\Source\CrowdFlowDirector\Private\ECrowdPlaybackMode.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\CrowdFlowDirector\_Builds\CrowdFlowDirector_5.4\HostProject\Plugins\CrowdFlowDirector\Source\CrowdFlowDirector\Private\ECrowdLODPolicy.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\CrowdFlowDirector\_Builds\CrowdFlowDirector_5.4\HostProject\Plugins\CrowdFlowDirector\Source\CrowdFlowDirector\Private\FCrowdAnalyticsComponent.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\CrowdFlowDirector\_Builds\CrowdFlowDirector_5.4\HostProject\Plugins\CrowdFlowDirector\Source\CrowdFlowDirector\Private\ECrowdLayoutMode.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\CrowdFlowDirector\_Builds\CrowdFlowDirector_5.4\HostProject\Plugins\CrowdFlowDirector\Source\CrowdFlowDirector\Private\FCrowdDirectorComponent.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\CrowdFlowDirector\_Builds\CrowdFlowDirector_5.4\HostProject\Plugins\CrowdFlowDirector\Source\CrowdFlowDirector\Private\FCrowdEventCue.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\CrowdFlowDirector\_Builds\CrowdFlowDirector_5.4\HostProject\Plugins\CrowdFlowDirector\Source\CrowdFlowDirector\Private\FCrowdFlowProfile.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\CrowdFlowDirector\_Builds\CrowdFlowDirector_5.4\HostProject\Plugins\CrowdFlowDirector\Source\CrowdFlowDirector\Private\FCrowdEffectorStackComponent.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\CrowdFlowDirector\_Builds\CrowdFlowDirector_5.4\HostProject\Plugins\CrowdFlowDirector\Source\CrowdFlowDirector\Private\FCrowdFlowComponent.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\CrowdFlowDirector\_Builds\CrowdFlowDirector_5.4\HostProject\Plugins\CrowdFlowDirector\Source\CrowdFlowDirector\Private\FCrowdFormationPreset.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\CrowdFlowDirector\_Builds\CrowdFlowDirector_5.4\HostProject\Plugins\CrowdFlowDirector\Source\CrowdFlowDirector\Private\FCrowdVATPlaybackComponent.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\CrowdFlowDirector\_Builds\CrowdFlowDirector_5.4\HostProject\Plugins\CrowdFlowDirector\Source\CrowdFlowDirector\Private\FCrowdVATClip.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
UnrealBuildTool failed. See log for more details. (C:\Users\Admin\AppData\Roaming\Unreal Engine\AutomationTool\Logs\D+Unreal+UE_5.4\UBA-UnrealEditor-Win64-Development.txt) 
BUILD FAILED 
 
Total errors found: 21 
 
 
------------- 
Temporal - KAIN COMPILATION 
------------- 
 
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
 
 
------------- 
NarrativeGraph - UE5 BUILD 
------------- 
Parsing command line: BuildPlugin -Plugin=m:\Code\Factory\NarrativeGraph\NarrativeGraph\NarrativeGraph.uplugin -Package=m:\Code\Factory\NarrativeGraph\_Builds\NarrativeGraph_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
  Running Internal UnrealHeaderTool M:\Code\Factory\NarrativeGraph\_Builds\NarrativeGraph_5.4\HostProject\HostProject.uproject M:\Code\Factory\NarrativeGraph\_Builds\NarrativeGraph_5.4\HostProject\Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest -WarningsAsErrors -installed 
M:\Code\Factory\NarrativeGraph\_Builds\NarrativeGraph_5.4\HostProject\Plugins\NarrativeGraph\Source\NarrativeGraph\Private\ANarrativeActor.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\NarrativeGraph\_Builds\NarrativeGraph_5.4\HostProject\Plugins\NarrativeGraph\Source\NarrativeGraph\Private\EDialogueNodeType.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\NarrativeGraph\_Builds\NarrativeGraph_5.4\HostProject\Plugins\NarrativeGraph\Source\NarrativeGraph\Private\EQuestNodeType.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\NarrativeGraph\_Builds\NarrativeGraph_5.4\HostProject\Plugins\NarrativeGraph\Source\NarrativeGraph\Private\EQuestState.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\NarrativeGraph\_Builds\NarrativeGraph_5.4\HostProject\Plugins\NarrativeGraph\Source\NarrativeGraph\Private\FNarrativeManagerComponent.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\NarrativeGraph\_Builds\NarrativeGraph_5.4\HostProject\Plugins\NarrativeGraph\Source\NarrativeGraph\Private\FDialogueChoice.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\NarrativeGraph\_Builds\NarrativeGraph_5.4\HostProject\Plugins\NarrativeGraph\Source\NarrativeGraph\Private\FNarrativeSubsystem.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\NarrativeGraph\_Builds\NarrativeGraph_5.4\HostProject\Plugins\NarrativeGraph\Source\NarrativeGraph\Private\FQuestObjective.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\NarrativeGraph\_Builds\NarrativeGraph_5.4\HostProject\Plugins\NarrativeGraph\Source\NarrativeGraph\Private\FSpeakerInfo.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\NarrativeGraph\_Builds\NarrativeGraph_5.4\HostProject\Plugins\NarrativeGraph\Source\NarrativeGraph\Private\NarrativeGraphBlueprintLibrary.cpp(5): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
UnrealBuildTool failed. See log for more details. (C:\Users\Admin\AppData\Roaming\Unreal Engine\AutomationTool\Logs\D+Unreal+UE_5.4\UBA-UnrealEditor-Win64-Development.txt) 
BUILD FAILED 
 
Total errors found: 14 
 
 
------------- 
MetaHumanVAT - KAIN COMPILATION 
------------- 
 
 KAIN Compiler v0.1.0 (build 1772314021)
ℹ️  No KAIN.toml found, using auto-detection...

🔍 Using directory name as plugin name: Code
Runtime error: No .kn files found in current directory. Please create a .kn file or add a KAIN.toml configuration.
 
 
------------- 
NarrativeGraph - KAIN COMPILATION 
------------- 
 
 KAIN Compiler v0.1.0 (build 1772314021)
ℹ️  No KAIN.toml found, using auto-detection...

🔍 Using directory name as plugin name: Code
Runtime error: No .kn files found in current directory. Please create a .kn file or add a KAIN.toml configuration.
 
 
------------- 
MetaHumanVAT - KAIN COMPILATION 
------------- 
 
 KAIN Compiler v0.1.0 (build 1772314021)
ℹ️  No KAIN.toml found, using auto-detection...

🔍 Using directory name as plugin name: Code
Runtime error: No .kn files found in current directory. Please create a .kn file or add a KAIN.toml configuration.
 
 
------------- 
NarrativeGraph - UE5 BUILD 
------------- 
Parsing command line: BuildPlugin -Plugin=m:\Code\Factory\NarrativeGraph\NarrativeGraph\NarrativeGraph.uplugin -Package=m:\Code\Factory\NarrativeGraph\_Builds\NarrativeGraph_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
  Running Internal UnrealHeaderTool M:\Code\Factory\NarrativeGraph\_Builds\NarrativeGraph_5.4\HostProject\HostProject.uproject M:\Code\Factory\NarrativeGraph\_Builds\NarrativeGraph_5.4\HostProject\Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest -WarningsAsErrors -installed 
M:\Code\Factory\NarrativeGraph\_Builds\NarrativeGraph_5.4\HostProject\Plugins\NarrativeGraph\Source\NarrativeGraph\Private\ANarrativeActor.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\NarrativeGraph\_Builds\NarrativeGraph_5.4\HostProject\Plugins\NarrativeGraph\Source\NarrativeGraph\Private\EQuestNodeType.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\NarrativeGraph\_Builds\NarrativeGraph_5.4\HostProject\Plugins\NarrativeGraph\Source\NarrativeGraph\Private\EDialogueNodeType.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\NarrativeGraph\_Builds\NarrativeGraph_5.4\HostProject\Plugins\NarrativeGraph\Source\NarrativeGraph\Private\EQuestState.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\NarrativeGraph\_Builds\NarrativeGraph_5.4\HostProject\Plugins\NarrativeGraph\Source\NarrativeGraph\Private\FDialogueChoice.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\NarrativeGraph\_Builds\NarrativeGraph_5.4\HostProject\Plugins\NarrativeGraph\Source\NarrativeGraph\Private\FNarrativeManagerComponent.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\NarrativeGraph\_Builds\NarrativeGraph_5.4\HostProject\Plugins\NarrativeGraph\Source\NarrativeGraph\Private\FNarrativeSubsystem.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\NarrativeGraph\_Builds\NarrativeGraph_5.4\HostProject\Plugins\NarrativeGraph\Source\NarrativeGraph\Private\FSpeakerInfo.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\NarrativeGraph\_Builds\NarrativeGraph_5.4\HostProject\Plugins\NarrativeGraph\Source\NarrativeGraph\Private\FQuestObjective.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\NarrativeGraph\_Builds\NarrativeGraph_5.4\HostProject\Plugins\NarrativeGraph\Source\NarrativeGraph\Private\NarrativeGraphBlueprintLibrary.cpp(5): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
UnrealBuildTool failed. See log for more details. (C:\Users\Admin\AppData\Roaming\Unreal Engine\AutomationTool\Logs\D+Unreal+UE_5.4\UBA-UnrealEditor-Win64-Development.txt) 
BUILD FAILED 
 
Total errors found: 14 
 
 
------------- 
SM64SubsetSmoke - UE5 BUILD 
------------- 
Parsing command line: BuildPlugin -Plugin=m:\Code\Factory\SM64SubsetSmoke\SM64SubsetSmoke.uplugin -Package=m:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
  Running Internal UnrealHeaderTool M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\HostProject.uproject M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest -WarningsAsErrors -installed 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDoorAction.h(28): Error: Struct 'FDoorAction' shares engine name 'DoorAction' with struct 'FDoorAction' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDoorAction.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(22): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(29): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(36): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(43): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(50): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(57): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(64): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(71): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(78): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(85): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(92): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(99): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(106): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(113): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(120): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(127): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(134): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(141): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(148): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(155): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(162): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(169): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(176): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(183): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdControl.h(22): Error: Struct 'FGdControl' shares engine name 'GdControl' with struct 'FGdControl' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdControl.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\Ffu.h(28): Error: Struct 'Ffu' shares engine name 'fu' with struct 'Ffu' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\Ffu.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\Ffu.h(41): Error: Struct 'Ffu' shares engine name 'fu' with struct 'Ffu' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\Ffu.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(22): Error: Struct 'FDynList' shares engine name 'DynList' with struct 'FDynList' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(29): Error: Struct 'FDynList' shares engine name 'DynList' with struct 'FDynList' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(36): Error: Struct 'FDynList' shares engine name 'DynList' with struct 'FDynList' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(43): Error: Struct 'FDynList' shares engine name 'DynList' with struct 'FDynList' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(50): Error: Struct 'FDynList' shares engine name 'DynList' with struct 'FDynList' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(57): Error: Struct 'FDynList' shares engine name 'DynList' with struct 'FDynList' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(64): Error: Struct 'FDynList' shares engine name 'DynList' with struct 'FDynList' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(71): Error: Struct 'FDynList' shares engine name 'DynList' with struct 'FDynList' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\Fdu.h(28): Error: Struct 'Fdu' shares engine name 'du' with struct 'Fdu' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\Fdu.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGMemBlock.h(22): Error: Struct 'FGMemBlock' shares engine name 'GMemBlock' with struct 'FGMemBlock' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGMemBlock.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGMemBlock.h(29): Error: Struct 'FGMemBlock' shares engine name 'GMemBlock' with struct 'FGMemBlock' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGMemBlock.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGMemBlock.h(36): Error: Struct 'FGMemBlock' shares engine name 'GMemBlock' with struct 'FGMemBlock' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGMemBlock.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGMemBlock.h(43): Error: Struct 'FGMemBlock' shares engine name 'GMemBlock' with struct 'FGMemBlock' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGMemBlock.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdFaceData.h(22): Error: Struct 'FGdFaceData' shares engine name 'GdFaceData' with struct 'FGdFaceData' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdFaceData.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdFaceData.h(29): Error: Struct 'FGdFaceData' shares engine name 'GdFaceData' with struct 'FGdFaceData' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdFaceData.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdFaceData.h(36): Error: Struct 'FGdFaceData' shares engine name 'GdFaceData' with struct 'FGdFaceData' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdFaceData.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdFaceData.h(43): Error: Struct 'FGdFaceData' shares engine name 'GdFaceData' with struct 'FGdFaceData' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdFaceData.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdFaceData.h(50): Error: Struct 'FGdFaceData' shares engine name 'GdFaceData' with struct 'FGdFaceData' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdFaceData.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdVtxData.h(22): Error: Struct 'FGdVtxData' shares engine name 'GdVtxData' with struct 'FGdVtxData' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdVtxData.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdVtxData.h(29): Error: Struct 'FGdVtxData' shares engine name 'GdVtxData' with struct 'FGdVtxData' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdVtxData.h(15) 
 
[TRUNCATED: 1552 more errors not shown] 
 
Total errors found: 1602 
 
 
------------- 
SM64SubsetSmoke - UE5 BUILD 
------------- 
Parsing command line: BuildPlugin -Plugin=m:\Code\Factory\SM64SubsetSmoke\SM64SubsetSmoke.uplugin -Package=m:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
  Running Internal UnrealHeaderTool M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\HostProject.uproject M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest -WarningsAsErrors -installed 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(22): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(29): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(36): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(43): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(50): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(57): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(64): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(71): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(78): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(85): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(92): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(99): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(106): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(113): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(120): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(127): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(134): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(141): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(148): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(155): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(162): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(169): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(176): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(183): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDoorAction.h(28): Error: Struct 'FDoorAction' shares engine name 'DoorAction' with struct 'FDoorAction' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDoorAction.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\Fdu.h(28): Error: Struct 'Fdu' shares engine name 'du' with struct 'Fdu' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\Fdu.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\Ffu.h(28): Error: Struct 'Ffu' shares engine name 'fu' with struct 'Ffu' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\Ffu.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\Ffu.h(41): Error: Struct 'Ffu' shares engine name 'fu' with struct 'Ffu' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\Ffu.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(22): Error: Struct 'FDynList' shares engine name 'DynList' with struct 'FDynList' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(29): Error: Struct 'FDynList' shares engine name 'DynList' with struct 'FDynList' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(36): Error: Struct 'FDynList' shares engine name 'DynList' with struct 'FDynList' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(43): Error: Struct 'FDynList' shares engine name 'DynList' with struct 'FDynList' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(50): Error: Struct 'FDynList' shares engine name 'DynList' with struct 'FDynList' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(57): Error: Struct 'FDynList' shares engine name 'DynList' with struct 'FDynList' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(64): Error: Struct 'FDynList' shares engine name 'DynList' with struct 'FDynList' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(71): Error: Struct 'FDynList' shares engine name 'DynList' with struct 'FDynList' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdControl.h(22): Error: Struct 'FGdControl' shares engine name 'GdControl' with struct 'FGdControl' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdControl.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdFaceData.h(22): Error: Struct 'FGdFaceData' shares engine name 'GdFaceData' with struct 'FGdFaceData' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdFaceData.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdFaceData.h(29): Error: Struct 'FGdFaceData' shares engine name 'GdFaceData' with struct 'FGdFaceData' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdFaceData.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdFaceData.h(36): Error: Struct 'FGdFaceData' shares engine name 'GdFaceData' with struct 'FGdFaceData' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdFaceData.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdFaceData.h(43): Error: Struct 'FGdFaceData' shares engine name 'GdFaceData' with struct 'FGdFaceData' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdFaceData.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdFaceData.h(50): Error: Struct 'FGdFaceData' shares engine name 'GdFaceData' with struct 'FGdFaceData' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdFaceData.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdVtxData.h(22): Error: Struct 'FGdVtxData' shares engine name 'GdVtxData' with struct 'FGdVtxData' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdVtxData.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdVtxData.h(29): Error: Struct 'FGdVtxData' shares engine name 'GdVtxData' with struct 'FGdVtxData' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdVtxData.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdVtxData.h(36): Error: Struct 'FGdVtxData' shares engine name 'GdVtxData' with struct 'FGdVtxData' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdVtxData.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdVtxData.h(43): Error: Struct 'FGdVtxData' shares engine name 'GdVtxData' with struct 'FGdVtxData' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdVtxData.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdVtxData.h(50): Error: Struct 'FGdVtxData' shares engine name 'GdVtxData' with struct 'FGdVtxData' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdVtxData.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGMemBlock.h(22): Error: Struct 'FGMemBlock' shares engine name 'GMemBlock' with struct 'FGMemBlock' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGMemBlock.h(15) 
 
[TRUNCATED: 1552 more errors not shown] 
 
Total errors found: 1602 
 
 
------------- 
SM64SubsetSmoke - UE5 BUILD 
------------- 
Parsing command line: BuildPlugin -Plugin=m:\Code\Factory\SM64SubsetSmoke\SM64SubsetSmoke.uplugin -Package=m:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
  Running Internal UnrealHeaderTool M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\HostProject.uproject M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest -WarningsAsErrors -installed 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDoorAction.h(28): Error: Struct 'FDoorAction' shares engine name 'DoorAction' with struct 'FDoorAction' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDoorAction.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\Fdu.h(28): Error: Struct 'Fdu' shares engine name 'du' with struct 'Fdu' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\Fdu.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(22): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(29): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(36): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(43): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(50): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(57): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(64): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(71): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(78): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(85): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(92): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(99): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(106): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(113): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(120): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(127): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(134): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(141): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(148): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(155): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(162): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(169): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(176): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(183): Error: Struct 'FAnimDataInfo' shares engine name 'AnimDataInfo' with struct 'FAnimDataInfo' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FAnimDataInfo.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(22): Error: Struct 'FDynList' shares engine name 'DynList' with struct 'FDynList' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(29): Error: Struct 'FDynList' shares engine name 'DynList' with struct 'FDynList' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(36): Error: Struct 'FDynList' shares engine name 'DynList' with struct 'FDynList' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(43): Error: Struct 'FDynList' shares engine name 'DynList' with struct 'FDynList' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(50): Error: Struct 'FDynList' shares engine name 'DynList' with struct 'FDynList' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(57): Error: Struct 'FDynList' shares engine name 'DynList' with struct 'FDynList' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(64): Error: Struct 'FDynList' shares engine name 'DynList' with struct 'FDynList' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(71): Error: Struct 'FDynList' shares engine name 'DynList' with struct 'FDynList' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FDynList.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdControl.h(22): Error: Struct 'FGdControl' shares engine name 'GdControl' with struct 'FGdControl' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdControl.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGMemBlock.h(22): Error: Struct 'FGMemBlock' shares engine name 'GMemBlock' with struct 'FGMemBlock' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGMemBlock.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGMemBlock.h(29): Error: Struct 'FGMemBlock' shares engine name 'GMemBlock' with struct 'FGMemBlock' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGMemBlock.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGMemBlock.h(36): Error: Struct 'FGMemBlock' shares engine name 'GMemBlock' with struct 'FGMemBlock' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGMemBlock.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGMemBlock.h(43): Error: Struct 'FGMemBlock' shares engine name 'GMemBlock' with struct 'FGMemBlock' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGMemBlock.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdVtxData.h(22): Error: Struct 'FGdVtxData' shares engine name 'GdVtxData' with struct 'FGdVtxData' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdVtxData.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdVtxData.h(29): Error: Struct 'FGdVtxData' shares engine name 'GdVtxData' with struct 'FGdVtxData' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdVtxData.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdVtxData.h(36): Error: Struct 'FGdVtxData' shares engine name 'GdVtxData' with struct 'FGdVtxData' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdVtxData.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdVtxData.h(43): Error: Struct 'FGdVtxData' shares engine name 'GdVtxData' with struct 'FGdVtxData' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdVtxData.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdVtxData.h(50): Error: Struct 'FGdVtxData' shares engine name 'GdVtxData' with struct 'FGdVtxData' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdVtxData.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FLateralPosition.h(28): Error: Struct 'FLateralPosition' shares engine name 'LateralPosition' with struct 'FLateralPosition' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FLateralPosition.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\Ffu.h(28): Error: Struct 'Ffu' shares engine name 'fu' with struct 'Ffu' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\Ffu.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\Ffu.h(41): Error: Struct 'Ffu' shares engine name 'fu' with struct 'Ffu' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\Ffu.h(15) 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdFaceData.h(22): Error: Struct 'FGdFaceData' shares engine name 'GdFaceData' with struct 'FGdFaceData' in M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\FGdFaceData.h(15) 
 
[TRUNCATED: 1552 more errors not shown] 
 
Total errors found: 1602 
 
 
------------- 
SM64SubsetSmoke - UE5 BUILD 
------------- 
Parsing command line: BuildPlugin -Plugin=m:\Code\Factory\SM64SubsetSmoke\SM64SubsetSmoke.uplugin -Package=m:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
  Running Internal UnrealHeaderTool M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\HostProject.uproject M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest -WarningsAsErrors -installed 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\Fdu.h(24): Error: Unable to find 'class', 'delegate', 'enum', or 'struct' with name 'AnonymousStruct' 
UnrealBuildTool failed. See log for more details. (C:\Users\Admin\AppData\Roaming\Unreal Engine\AutomationTool\Logs\D+Unreal+UE_5.4\UBA-UnrealEditor-Win64-Development.txt) 
BUILD FAILED 
 
Total errors found: 5 
 
 
------------- 
SM64SubsetSmoke - UE5 BUILD 
------------- 
Parsing command line: BuildPlugin -Plugin=m:\Code\Factory\SM64SubsetSmoke\SM64SubsetSmoke.uplugin -Package=m:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
  Running Internal UnrealHeaderTool M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\HostProject.uproject M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest -WarningsAsErrors -installed 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\EDebugPrintStateInfo.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FDoorAction.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FDynList.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FAnimDataInfo.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FGMemBlock.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FGdControl.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FGdFaceData.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FGdVtxData.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FGfxPool.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FGoombaProperties.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
m:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Intermediate\Build\Win64\UnrealEditor\Inc\SM64SubsetSmoke\UHT\FILE.generated.h(22): error C2371: 'FILE': redefinition; different basic types 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FILE.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FOSContRamReadFormat.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FOSMesgQueue.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FLateralPosition.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FOSMesg.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FOSPfs.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FOSPifRam.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FOSThread.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FObjectHitbox.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FOSTimer.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FSaveBuffer.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FRacingPenguinData.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FSLWalkingPenguinStep.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FStruct80331B30.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FSpawnParticlesInfo.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FUnusedChuckyaData.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FStruct80331C38.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FWaterDropletParams.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\Fdu_word_AnonStruct.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\Fdu.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FosExceptionVector.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\Ffu.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\SM64SubsetSmoke.h(10): error C2371: 'FILE': redefinition; different basic types 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(15): error C2860: 'void' cannot be used as a function parameter except for '(void)' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(17): error C2065: 'AI_STATUS_REG': undeclared identifier 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(18): error C2065: 'AI_STATUS_FIFO_FULL': undeclared identifier 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(30): error C2059: syntax error: ';' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(31): error C2059: syntax error: ';' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(32): error C2059: syntax error: ')' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(33): error C2059: syntax error: '=' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(34): error C2143: syntax error: missing ';' before '{' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(35): error C2059: syntax error: ')' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(36): error C2059: syntax error: ')' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(38): error C2181: illegal else without matching if 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(40): error C2059: syntax error: ')' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(42): error C3861: 'osRestoreInt': identifier not found 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(48): error C2059: syntax error: ';' 
 
[TRUNCATED: 96 more errors not shown] 
 
Total errors found: 146 
 
 
------------- 
SM64SubsetSmoke - UE5 BUILD 
------------- 
Parsing command line: BuildPlugin -Plugin=m:\Code\Factory\SM64SubsetSmoke\SM64SubsetSmoke.uplugin -Package=m:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
  Running Internal UnrealHeaderTool M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\HostProject.uproject M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest -WarningsAsErrors -installed 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FAnimDataInfo.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\EDebugPrintStateInfo.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FDynList.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FDoorAction.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FGMemBlock.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FGdControl.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FGdFaceData.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FGdVtxData.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FGoombaProperties.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FGfxPool.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
m:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Intermediate\Build\Win64\UnrealEditor\Inc\SM64SubsetSmoke\UHT\FILE.generated.h(22): error C2371: 'FILE': redefinition; different basic types 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FILE.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FLateralPosition.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FOSContRamReadFormat.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FOSMesg.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FOSMesgQueue.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FOSPfs.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FOSThread.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FOSPifRam.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FOSTimer.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FObjectHitbox.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FSLWalkingPenguinStep.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FRacingPenguinData.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FSaveBuffer.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FSpawnParticlesInfo.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FStruct80331B30.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FStruct80331C38.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FWaterDropletParams.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FUnusedChuckyaData.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\Fdu_word_AnonStruct.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\Fdu.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\Ffu.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FosExceptionVector.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\SM64SubsetSmoke.h(10): error C2371: 'FILE': redefinition; different basic types 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(15): error C2860: 'void' cannot be used as a function parameter except for '(void)' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(17): error C2065: 'AI_STATUS_REG': undeclared identifier 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(18): error C2065: 'AI_STATUS_FIFO_FULL': undeclared identifier 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(30): error C2059: syntax error: ';' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(31): error C2059: syntax error: ';' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(32): error C2059: syntax error: ')' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(33): error C2059: syntax error: '=' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(34): error C2143: syntax error: missing ';' before '{' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(35): error C2059: syntax error: ')' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(36): error C2059: syntax error: ')' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(38): error C2181: illegal else without matching if 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(40): error C2059: syntax error: ')' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(42): error C3861: 'osRestoreInt': identifier not found 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(48): error C2059: syntax error: ';' 
 
[TRUNCATED: 96 more errors not shown] 
 
Total errors found: 146 
 
 
------------- 
SM64SubsetSmoke - UE5 BUILD 
------------- 
Parsing command line: BuildPlugin -Plugin=m:\Code\Factory\SM64SubsetSmoke\SM64SubsetSmoke.uplugin -Package=m:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
  Running Internal UnrealHeaderTool M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\HostProject.uproject M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest -WarningsAsErrors -installed 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\EDebugPrintStateInfo.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FGMemBlock.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FAnimDataInfo.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FDoorAction.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FGdFaceData.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FDynList.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FGdVtxData.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FGdControl.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
m:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Intermediate\Build\Win64\UnrealEditor\Inc\SM64SubsetSmoke\UHT\FILE.generated.h(22): error C2371: 'FILE': redefinition; different basic types 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FILE.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FOSContRamReadFormat.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FGfxPool.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FGoombaProperties.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FLateralPosition.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FOSMesg.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FOSPfs.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FOSMesgQueue.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FOSPifRam.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FObjectHitbox.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FOSThread.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FOSTimer.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FRacingPenguinData.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FSpawnParticlesInfo.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FSaveBuffer.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FSLWalkingPenguinStep.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FStruct80331C38.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FUnusedChuckyaData.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\Fdu.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FStruct80331B30.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FWaterDropletParams.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\Fdu_word_AnonStruct.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\Ffu.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FosExceptionVector.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
m:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Intermediate\Build\Win64\UnrealEditor\Inc\SM64SubsetSmoke\UHT\FILE.generated.h(22): error C2371: 'FILE': redefinition; different basic types 
m:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Intermediate\Build\Win64\UnrealEditor\Inc\SM64SubsetSmoke\UHT\FILE.gen.cpp(19): error C2027: use of undefined type 'FILE' 
m:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Intermediate\Build\Win64\UnrealEditor\Inc\SM64SubsetSmoke\UHT\FILE.gen.cpp(29): error C2027: use of undefined type 'FILE' 
D:\Unreal\UE_5.4\Engine\Source\Runtime\Core\Public\Templates\IsPODType.h(13): error C2139: 'FILE': an undefined class is not allowed as an argument to compiler intrinsic type trait '__is_pod' 
m:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Intermediate\Build\Win64\UnrealEditor\Inc\SM64SubsetSmoke\UHT\FILE.gen.cpp(53): error C2027: use of undefined type 'FILE' 
m:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Intermediate\Build\Win64\UnrealEditor\Inc\SM64SubsetSmoke\UHT\FILE.gen.cpp(45): error C2737: 'StructParams': const object must be initialized 
m:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Intermediate\Build\Win64\UnrealEditor\Inc\SM64SubsetSmoke\UHT\FILE.gen.cpp(72): error C2027: use of undefined type 'FILE' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\SM64SubsetSmoke.h(10): error C2371: 'FILE': redefinition; different basic types 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(15): error C2860: 'void' cannot be used as a function parameter except for '(void)' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(17): error C2065: 'AI_STATUS_REG': undeclared identifier 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(18): error C2065: 'AI_STATUS_FIFO_FULL': undeclared identifier 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(30): error C2059: syntax error: ';' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(31): error C2059: syntax error: ';' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(32): error C2059: syntax error: ')' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(33): error C2059: syntax error: '=' 
 
[TRUNCATED: 96 more errors not shown] 
 
Total errors found: 146 
 
 
------------- 
SM64SubsetSmoke - UE5 BUILD 
------------- 
Parsing command line: BuildPlugin -Plugin=m:\Code\Factory\SM64SubsetSmoke\SM64SubsetSmoke.uplugin -Package=m:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
  Running Internal UnrealHeaderTool M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\HostProject.uproject M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest -WarningsAsErrors -installed 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\EDebugPrintStateInfo.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FDoorAction.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FAnimDataInfo.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FDynList.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FGdControl.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FGMemBlock.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FGdFaceData.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FGdVtxData.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FGfxPool.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FGoombaProperties.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FLateralPosition.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
m:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Intermediate\Build\Win64\UnrealEditor\Inc\SM64SubsetSmoke\UHT\FILE.generated.h(22): error C2371: 'FILE': redefinition; different basic types 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FILE.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FOSContRamReadFormat.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FOSMesg.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FOSMesgQueue.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FOSPfs.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FOSThread.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FOSPifRam.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FOSTimer.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FObjectHitbox.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FSLWalkingPenguinStep.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FRacingPenguinData.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FSaveBuffer.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FSpawnParticlesInfo.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FStruct80331B30.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FWaterDropletParams.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FUnusedChuckyaData.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FStruct80331C38.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\Fdu.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FosExceptionVector.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\Ffu.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\Fdu_word_AnonStruct.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\SM64SubsetSmoke.h(10): error C2371: 'FILE': redefinition; different basic types 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(15): error C2860: 'void' cannot be used as a function parameter except for '(void)' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(17): error C2065: 'AI_STATUS_REG': undeclared identifier 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(18): error C2065: 'AI_STATUS_FIFO_FULL': undeclared identifier 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(30): error C2059: syntax error: ';' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(31): error C2059: syntax error: ';' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(32): error C2059: syntax error: ')' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(33): error C2059: syntax error: '=' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(34): error C2143: syntax error: missing ';' before '{' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(35): error C2059: syntax error: ')' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(36): error C2059: syntax error: ')' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(38): error C2181: illegal else without matching if 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(40): error C2059: syntax error: ')' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(42): error C3861: 'osRestoreInt': identifier not found 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(48): error C2059: syntax error: ';' 
 
[TRUNCATED: 96 more errors not shown] 
 
Total errors found: 146 
 
 
------------- 
ConfigSmokeTest - KAIN COMPILATION 
------------- 
 
 KAIN Compiler v0.1.0 (build 1772372109)
ℹ️  No KAIN.toml found, using auto-detection...

🔍 Using directory name as plugin name: Code
Runtime error: No .kn files found in current directory. Please create a .kn file or add a KAIN.toml configuration.
 
 
------------- 
SM64SubsetSmoke - UE5 BUILD 
------------- 
Parsing command line: BuildPlugin -Plugin=M:\Code\Factory\SM64SubsetSmoke\SM64SubsetSmoke.uplugin -Package=M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
  Running Internal UnrealHeaderTool M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\HostProject.uproject M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest -WarningsAsErrors -installed 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FGdVtxData.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FGdFaceData.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FAnimDataInfo.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\EDebugPrintStateInfo.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FDynList.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FGMemBlock.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FDoorAction.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FGdControl.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FLateralPosition.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Intermediate\Build\Win64\UnrealEditor\Inc\SM64SubsetSmoke\UHT\FILE.generated.h(22): error C2371: 'FILE': redefinition; different basic types 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FILE.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FOSContRamReadFormat.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FGoombaProperties.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FOSMesgQueue.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FOSMesg.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FGfxPool.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FOSPfs.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FOSTimer.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FOSPifRam.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FSLWalkingPenguinStep.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FSaveBuffer.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FSpawnParticlesInfo.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FOSThread.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FRacingPenguinData.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FObjectHitbox.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FUnusedChuckyaData.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FWaterDropletParams.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\Fdu_word_AnonStruct.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\Fdu.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FStruct80331B30.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FStruct80331C38.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\Ffu.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FosExceptionVector.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\SM64SubsetSmoke.h(10): error C2371: 'FILE': redefinition; different basic types 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(15): error C2860: 'void' cannot be used as a function parameter except for '(void)' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(17): error C2065: 'AI_STATUS_REG': undeclared identifier 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(18): error C2065: 'AI_STATUS_FIFO_FULL': undeclared identifier 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(30): error C2059: syntax error: ';' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(31): error C2059: syntax error: ';' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(32): error C2059: syntax error: ')' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(33): error C2059: syntax error: '=' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(34): error C2143: syntax error: missing ';' before '{' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(35): error C2059: syntax error: ')' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(36): error C2059: syntax error: ')' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(38): error C2181: illegal else without matching if 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(40): error C2059: syntax error: ')' 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(42): error C3861: 'osRestoreInt': identifier not found 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Public\KainStdlib.h(48): error C2059: syntax error: ';' 
 
[TRUNCATED: 96 more errors not shown] 
 
Total errors found: 146 
 
 
------------- 
SM64SubsetSmoke - UE5 BUILD 
------------- 
Parsing command line: BuildPlugin -Plugin=M:\Code\Factory\SM64SubsetSmoke\SM64SubsetSmoke.uplugin -Package=M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
  Running Internal UnrealHeaderTool M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\HostProject.uproject M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest -WarningsAsErrors -installed 
M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Source\Private\FStruct80331C38.cpp : fatal error C1083: Cannot open compiler generated file: 'M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Intermediate\Build\Win64\x64\UnrealEditor\Development\SM64SubsetSmoke\FStruct80331C38.cpp.obj': No such file or directory 
cl : Command line error D8022 : cannot open 'M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Intermediate\Build\Win64\x64\UnrealEditor\Development\SM64SubsetSmoke\FUnusedChuckyaData.cpp.obj.rsp' 
cl : Command line error D8022 : cannot open 'M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Intermediate\Build\Win64\x64\UnrealEditor\Development\SM64SubsetSmoke\FWaterDropletParams.cpp.obj.rsp' 
cl : Command line error D8022 : cannot open 'M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Intermediate\Build\Win64\x64\UnrealEditor\Development\SM64SubsetSmoke\Fdu.cpp.obj.rsp' 
cl : Command line error D8022 : cannot open 'M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Intermediate\Build\Win64\x64\UnrealEditor\Development\SM64SubsetSmoke\Fdu_word_AnonStruct.cpp.obj.rsp' 
cl : Command line error D8022 : cannot open 'M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Intermediate\Build\Win64\x64\UnrealEditor\Development\SM64SubsetSmoke\Ffu.cpp.obj.rsp' 
cl : Command line error D8022 : cannot open 'M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Intermediate\Build\Win64\x64\UnrealEditor\Development\SM64SubsetSmoke\FosExceptionVector.cpp.obj.rsp' 
cl : Command line error D8022 : cannot open 'M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Intermediate\Build\Win64\x64\UnrealEditor\Development\SM64SubsetSmoke\Module.SM64SubsetSmoke.cpp.obj.rsp' 
cl : Command line error D8022 : cannot open 'M:\Code\Factory\SM64SubsetSmoke\_Builds\SM64SubsetSmoke_5.4\HostProject\Plugins\SM64SubsetSmoke\Intermediate\Build\Win64\x64\UnrealEditor\Development\SM64SubsetSmoke\SM64SubsetSmoke.cpp.obj.rsp' 
UnrealBuildTool failed. See log for more details. (C:\Users\Admin\AppData\Roaming\Unreal Engine\AutomationTool\Logs\D+Unreal+UE_5.4\UBA-UnrealEditor-Win64-Development.txt) 
BUILD FAILED 
 
Total errors found: 13 
 
 
------------- 
ZenMograph - UE5 BUILD 
------------- 
Parsing command line: BuildPlugin -Plugin=m:\Code\Factory\Cinema4DMograph\ZenMograph\ZenMograph.uplugin -Package=m:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
  Running Internal UnrealHeaderTool M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\HostProject.uproject M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest -WarningsAsErrors -installed 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\EClonerMode.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\EMeshSampleMode.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\AClonerEffectorSubsystem.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\EEffectorShape.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\ESkeletalMode.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\AClonerActor.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\EEasingType.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\EAudioMode.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FBounceModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FClonerData.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FClonerEffectorComponent.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FBakeSettings.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FClonerAnimationComponent.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FBakeResult.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FAttractModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FClonerInstanceComponent.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FClonerPresetData.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FClonerPerformanceComponent.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FClonerVFXComponent.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FClonerNiagaraDataInterface.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FClonerTargetComponent.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FColorModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FDelayModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FDistributionLayer.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FFigure8Modifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FExpressionModifierPreset.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FFloatModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FInstanceCache.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FElasticModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FGravityModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FEffectorData.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FLissajousModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FModifierBase.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FModifierPresetData.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FNoiseModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FPresetVariable.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FOrbitModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FPendulumModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FModifierState.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FPulseModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FRandomModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FPushModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FStepModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FShakeModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FTargetModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FSwayModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FTumbleModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\FVortexModifier.cpp(8): fatal error C1083: Cannot open include file: 'AbilitySystemBlueprintLibrary.h': No such file or directory 
 
[TRUNCATED: 4 more errors not shown] 
 
Total errors found: 54 
 
 
------------- 
ZenMograph - UE5 BUILD 
------------- 
Parsing command line: BuildPlugin -Plugin=m:\Code\Factory\Cinema4DMograph\ZenMograph\ZenMograph.uplugin -Package=m:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
  Running Internal UnrealHeaderTool M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\HostProject.uproject M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest -WarningsAsErrors -installed 
  Running Internal UnrealHeaderTool M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\HostProject.uproject M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Intermediate\Build\Win64\UnrealGame\Development\UnrealGame.uhtmanifest -WarningsAsErrors -installed 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\ZenMographBlueprintLibrary.cpp(1003): error C2665: 'GetNameSafe': no overloaded function could convert all the argument types 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\ZenMographBlueprintLibrary.cpp(1003): error C2062: type 'unknown-type' unexpected 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\ZenMographBlueprintLibrary.cpp(1003): error C2653: 'UE_FMT_STR_Checker': is not a class or namespace name 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\ZenMographBlueprintLibrary.cpp(1003): error C3861: 'Check': identifier not found 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\ZenMographBlueprintLibrary.cpp(1003): error C2131: expression did not evaluate to a constant 
M:\Code\Factory\Cinema4DMograph\_Builds\ZenMograph_5.4\HostProject\Plugins\ZenMograph\Source\ZenMograph\Private\ZenMographBlueprintLibrary.cpp(1003): fatal error C1903: unable to recover from previous error(s); stopping compilation 
UnrealBuildTool failed. See log for more details. (C:\Users\Admin\AppData\Roaming\Unreal Engine\AutomationTool\Logs\D+Unreal+UE_5.4\UBA-UnrealGame-Win64-Development.txt) 
BUILD FAILED 
 
Total errors found: 11 
 
