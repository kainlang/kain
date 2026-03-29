# Build Log - CrowdFlowDirector 
 
**Build Date**: Sat 02 28 2026 07:11 
**Status**: FAILED 
**Error Type**: UE5 BUILD 
 
## Errors 
 
```
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
```
 
**Total Errors**: 21 
