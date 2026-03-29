---------------
UESCULPT
---------------
LoginId:237a378b44f378e073fcf9958cf0245a
EpicAccountId:77be631e113f46719d575b8bc9d5ba80

Fatal error: [File:D:\build\++UE5\Sync\Engine\Source\Runtime\CoreUObject\Private\UObject\UObjectArray.cpp] [Line: 576] Maximum number of UObjects (0) exceeded when trying to add 1 object(s), make sure you update MaxObjectsInGame/MaxObjectsInEditor/MaxObjectsInProgram in project settings.

UnrealEditor_CoreUObject!FChunkedFixedUObjectArray::AddRange() [D:\build\++UE5\Sync\Engine\Source\Runtime\CoreUObject\Public\UObject\UObjectArray.h:690]
UnrealEditor_CoreUObject!FUObjectArray::AllocateUObjectIndex() [D:\build\++UE5\Sync\Engine\Source\Runtime\CoreUObject\Private\UObject\UObjectArray.cpp:248]
UnrealEditor_CoreUObject!UObjectBase::AddObject() [D:\build\++UE5\Sync\Engine\Source\Runtime\CoreUObject\Private\UObject\UObjectBase.cpp:222]
UnrealEditor_CoreUObject!StaticAllocateObject() [D:\build\++UE5\Sync\Engine\Source\Runtime\CoreUObject\Private\UObject\UObjectGlobals.cpp:3543]
UnrealEditor_CoreUObject!StaticConstructObject_Internal() [D:\build\++UE5\Sync\Engine\Source\Runtime\CoreUObject\Private\UObject\UObjectGlobals.cpp:4449]
UnrealEditor_CoreUObject!CreatePackage() [D:\build\++UE5\Sync\Engine\Source\Runtime\CoreUObject\Private\UObject\UObjectGlobals.cpp:1076]
UnrealEditor_UESculpt!FUESculptMaterialFactory::Generate_SculptClay() [M:\UnrealEngine\Plugins\UE5Proj\MyProject\Plugins\UESculpt\Source\UESculpt\Private\Generated\MaterialFactories.cpp:56]
UnrealEditor_UESculpt!FUESculptMaterialFactory::GenerateMaterials() [M:\UnrealEngine\Plugins\UE5Proj\MyProject\Plugins\UESculpt\Source\UESculpt\Private\Generated\MaterialFactories.cpp:45]
UnrealEditor_Core!FModuleManager::LoadModuleWithFailureReason() [D:\build\++UE5\Sync\Engine\Source\Runtime\Core\Private\Modules\ModuleManager.cpp:674]
UnrealEditor_Projects!FModuleDescriptor::LoadModulesForPhase() [D:\build\++UE5\Sync\Engine\Source\Runtime\Projects\Private\ModuleDescriptor.cpp:696]
UnrealEditor_Projects!FPluginManager::TryLoadModulesForPlugin() [D:\build\++UE5\Sync\Engine\Source\Runtime\Projects\Private\PluginManager.cpp:2766]
UnrealEditor_Projects!FPluginManager::LoadModulesForEnabledPlugins() [D:\build\++UE5\Sync\Engine\Source\Runtime\Projects\Private\PluginManager.cpp:2903]
UnrealEditor!FEngineLoop::AppInit() [D:\build\++UE5\Sync\Engine\Source\Runtime\Launch\Private\LaunchEngineLoop.cpp:6733]
UnrealEditor!FEngineLoop::PreInitPreStartupScreen() [D:\build\++UE5\Sync\Engine\Source\Runtime\Launch\Private\LaunchEngineLoop.cpp:2884]
UnrealEditor!GuardedMain() [D:\build\++UE5\Sync\Engine\Source\Runtime\Launch\Private\Launch.cpp:136]
UnrealEditor!GuardedMainWrapper() [D:\build\++UE5\Sync\Engine\Source\Runtime\Launch\Private\Windows\LaunchWindows.cpp:118]
UnrealEditor!LaunchWindowsStartup() [D:\build\++UE5\Sync\Engine\Source\Runtime\Launch\Private\Windows\LaunchWindows.cpp:258]
UnrealEditor!WinMain() [D:\build\++UE5\Sync\Engine\Source\Runtime\Launch\Private\Windows\LaunchWindows.cpp:298]
UnrealEditor!__scrt_common_main_seh() [D:\a\_work\1\s\src\vctools\crt\vcstartup\src\startup\exe_common.inl:288]



----------------
In EDITOR
----------------
LogClass: Error: EnumProperty FInstancerSettings::merge_mode is not initialized properly. Module:AutoInstancer File:Public/FInstancerSettings.h
LogClass: Error: EnumProperty FPlanetDefinition::archetype is not initialized properly. Module:KainCosmos File:Public/FPlanetDefinition.h
LogClass: Error: EnumProperty FResourceVein::tier is not initialized properly. Module:KainCosmos File:Public/FResourceVein.h
LogClass: Error: EnumProperty FMaterialCacheEntry::compile_status is not initialized properly. Module:PSOEliminator File:Public/FMaterialCacheEntry.h
LogClass: Error: EnumProperty FPSOCacheSettings::compile_priority is not initialized properly. Module:PSOEliminator File:Public/FPSOCacheSettings.h
LogClass: Error: EnumProperty FOptimizationPreset::target_category is not initialized properly. Module:VRAMSniper File:Public/FOptimizationPreset.h
LogClass: Error: EnumProperty FOptimizationPreset::compression_format is not initialized properly. Module:VRAMSniper File:Public/FOptimizationPreset.h
LogClass: Error: EnumProperty FTextureAnalysisData::format is not initialized properly. Module:VRAMSniper File:Public/FTextureAnalysisData.h
LogClass: Error: EnumProperty FTextureAnalysisData::category is not initialized properly. Module:VRAMSniper File:Public/FTextureAnalysisData.h
LogClass: Display: 9 Uninitialized script struct members found including 0 object properties
LogAutomationTest: Error: LogClass: EnumProperty FInstancerSettings::merge_mode is not initialized properly. Module:AutoInstancer File:Public/FInstancerSettings.h [log]
LogAutomationTest: Error: LogClass: EnumProperty FPlanetDefinition::archetype is not initialized properly. Module:KainCosmos File:Public/FPlanetDefinition.h [log]
LogAutomationTest: Error: LogClass: EnumProperty FResourceVein::tier is not initialized properly. Module:KainCosmos File:Public/FResourceVein.h [log]
LogAutomationTest: Error: LogClass: EnumProperty FMaterialCacheEntry::compile_status is not initialized properly. Module:PSOEliminator File:Public/FMaterialCacheEntry.h [log]
LogAutomationTest: Error: LogClass: EnumProperty FPSOCacheSettings::compile_priority is not initialized properly. Module:PSOEliminator File:Public/FPSOCacheSettings.h [log]
LogAutomationTest: Error: LogClass: EnumProperty FOptimizationPreset::target_category is not initialized properly. Module:VRAMSniper File:Public/FOptimizationPreset.h [log]
LogAutomationTest: Error: LogClass: EnumProperty FOptimizationPreset::compression_format is not initialized properly. Module:VRAMSniper File:Public/FOptimizationPreset.h [log]
LogAutomationTest: Error: LogClass: EnumProperty FTextureAnalysisData::format is not initialized properly. Module:VRAMSniper File:Public/FTextureAnalysisData.h [log]
LogAutomationTest: Error: LogClass: EnumProperty FTextureAnalysisData::category is not initialized properly. Module:VRAMSniper File:Public/FTextureAnalysisData.h [log]



---------
VRAMSNIPER DURING RUNTIME
---------
LoginId:237a378b44f378e073fcf9958cf0245a
EpicAccountId:77be631e113f46719d575b8bc9d5ba80

Unhandled Exception: EXCEPTION_ACCESS_VIOLATION writing address 0x00000000000000d8

UnrealEditor_VRAMSniper!AVRAMAnalyzer::BeginPlay() [M:\UnrealEngine\Plugins\UE5Proj\MyProject\Plugins\VRAMSniper\Source\VRAMSniper\Private\AVRAMAnalyzer.cpp:24]
UnrealEditor_Engine!AActor::DispatchBeginPlay() [D:\build\++UE5\Sync\Engine\Source\Runtime\Engine\Private\Actor.cpp:4193]
UnrealEditor_Engine!AWorldSettings::NotifyBeginPlay() [D:\build\++UE5\Sync\Engine\Source\Runtime\Engine\Private\WorldSettings.cpp:305]
UnrealEditor_Engine!AGameStateBase::HandleBeginPlay() [D:\build\++UE5\Sync\Engine\Source\Runtime\Engine\Private\GameStateBase.cpp:228]
UnrealEditor_Engine!AGameModeBase::StartPlay() [D:\build\++UE5\Sync\Engine\Source\Runtime\Engine\Private\GameModeBase.cpp:206]
UnrealEditor_Engine!UWorld::BeginPlay() [D:\build\++UE5\Sync\Engine\Source\Runtime\Engine\Private\World.cpp:5330]
UnrealEditor_Engine!UGameInstance::StartPlayInEditorGameInstance() [D:\build\++UE5\Sync\Engine\Source\Runtime\Engine\Private\GameInstance.cpp:568]
UnrealEditor_UnrealEd!UEditorEngine::CreateInnerProcessPIEGameInstance() [D:\build\++UE5\Sync\Engine\Source\Editor\UnrealEd\Private\PlayLevel.cpp:3141]
UnrealEditor_UnrealEd!UEditorEngine::OnLoginPIEComplete_Deferred() [D:\build\++UE5\Sync\Engine\Source\Editor\UnrealEd\Private\PlayLevel.cpp:1590]
UnrealEditor_UnrealEd!UEditorEngine::CreateNewPlayInEditorInstance() [D:\build\++UE5\Sync\Engine\Source\Editor\UnrealEd\Private\PlayLevel.cpp:1853]
UnrealEditor_UnrealEd!UEditorEngine::StartPlayInEditorSession() [D:\build\++UE5\Sync\Engine\Source\Editor\UnrealEd\Private\PlayLevel.cpp:2872]
UnrealEditor_UnrealEd!UEditorEngine::StartQueuedPlaySessionRequestImpl() [D:\build\++UE5\Sync\Engine\Source\Editor\UnrealEd\Private\PlayLevel.cpp:1167]
UnrealEditor_UnrealEd!UEditorEngine::StartQueuedPlaySessionRequest() [D:\build\++UE5\Sync\Engine\Source\Editor\UnrealEd\Private\PlayLevel.cpp:1068]
UnrealEditor_UnrealEd!UEditorEngine::Tick() [D:\build\++UE5\Sync\Engine\Source\Editor\UnrealEd\Private\EditorEngine.cpp:1902]
UnrealEditor_UnrealEd!UUnrealEdEngine::Tick() [D:\build\++UE5\Sync\Engine\Source\Editor\UnrealEd\Private\UnrealEdEngine.cpp:550]
UnrealEditor!FEngineLoop::Tick() [D:\build\++UE5\Sync\Engine\Source\Runtime\Launch\Private\LaunchEngineLoop.cpp:5921]
UnrealEditor!GuardedMain() [D:\build\++UE5\Sync\Engine\Source\Runtime\Launch\Private\Launch.cpp:180]
UnrealEditor!GuardedMainWrapper() [D:\build\++UE5\Sync\Engine\Source\Runtime\Launch\Private\Windows\LaunchWindows.cpp:118]
UnrealEditor!LaunchWindowsStartup() [D:\build\++UE5\Sync\Engine\Source\Runtime\Launch\Private\Windows\LaunchWindows.cpp:258]
UnrealEditor!WinMain() [D:\build\++UE5\Sync\Engine\Source\Runtime\Launch\Private\Windows\LaunchWindows.cpp:298]
UnrealEditor!__scrt_common_main_seh() [D:\a\_work\1\s\src\vctools\crt\vcstartup\src\startup\exe_common.inl:288]
kernel32
ntdll



----------------
SLATE UI 
----------------
NO SLATE UI VISIBLE EXCEPT IN DETAILS PANEL HOWEVER IF YOU LOOK IN THE EDITOR WE HAVE A MULTITUDE OF SLATE UI ELEMENTS. 
FOR EXAMPLE VRAMSNIPER HAS A TOOLBAR BUTTON HOWEVER IT IS NOT VISIBLE IN THE EDITOR. DETAILS PANEL IS THE ONLY SLATE UI ELEMENT VISIBLE.