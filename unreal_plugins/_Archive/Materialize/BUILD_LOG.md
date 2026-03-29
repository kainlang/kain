# Build Log - Materialize 
 
**Build Date**: Tue 02 24 2026 05:56 
**Status**: FAILED 
**Error Type**: UE5 BUILD 
 
## Errors 
 
```
Parsing command line: BuildPlugin -Plugin=m:\Code\Factory\Materialize\Materialize\Materialize.uplugin -Package=m:\Code\Factory\Materialize\_Builds\Materialize_5.4 -Rocket -TargetPlatforms=Win64 -UbtArgs=-NoWarningsAsErrors 
  Running Internal UnrealHeaderTool M:\Code\Factory\Materialize\_Builds\Materialize_5.4\HostProject\HostProject.uproject M:\Code\Factory\Materialize\_Builds\Materialize_5.4\HostProject\Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest -WarningsAsErrors -installed 
M:\Code\Factory\Materialize\_Builds\Materialize_5.4\HostProject\Plugins\Materialize\Source\Materialize\Public\EBlendMode.h(57): Error: Enum 'EBlendMode' shares engine name 'EBlendMode' with enum 'EBlendMode' in D:\Unreal\UE_5.4\Engine\Source\Runtime\Engine\Classes\Engine\EngineTypes.h(240) 
M:\Code\Factory\Materialize\_Builds\Materialize_5.4\HostProject\Plugins\Materialize\Source\Materialize\Public\FLayer.h(18): Error: Struct 'FLayer' shares engine name 'Layer' with class 'ULayer' in D:\Unreal\UE_5.4\Engine\Source\Runtime\Engine\Classes\Layers\Layer.h(30) 
M:\Code\Factory\Materialize\_Builds\Materialize_5.4\HostProject\Plugins\Materialize\Source\Materialize\Public\FMaterialStatistics.h(57): Error: Struct 'FMaterialStatistics' shares engine name 'MaterialStatistics' with struct 'FMaterialStatistics' in D:\Unreal\UE_5.4\Engine\Source\Editor\MaterialEditor\Public\MaterialEditingLibrary.h(17) 
D:\Unreal\UE_5.4\Engine\Plugins\Experimental\TextureGraph\Source\TextureGraph\Public\Expressions\Procedural\TG_Expression_Noise.h(13): Error: Enum 'ENoiseType' shares engine name 'ENoiseType' with enum 'ENoiseType' in M:\Code\Factory\Materialize\_Builds\Materialize_5.4\HostProject\Plugins\Materialize\Source\Materialize\Public\ENoiseType.h(15) 
UnrealBuildTool failed. See log for more details. (C:\Users\Admin\AppData\Roaming\Unreal Engine\AutomationTool\Logs\D+Unreal+UE_5.4\UBA-UnrealEditor-Win64-Development.txt) 
BUILD FAILED 
```
 
**Total Errors**: 8 
