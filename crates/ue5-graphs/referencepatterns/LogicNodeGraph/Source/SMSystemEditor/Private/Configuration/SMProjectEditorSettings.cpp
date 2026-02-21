// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMProjectEditorSettings.h"

#include "SMConduitInstance.h"
#include "SMInstance.h"
#include "SMStateInstance.h"
#include "SMStateMachineInstance.h"
#include "SMTransitionInstance.h"

USMProjectEditorSettings::USMProjectEditorSettings()
{
	InstalledVersion = "";
	bUpdateAssetsOnStartup = true;
	bDisplayAssetUpdateProgress = true;
	bDisplayUpdateNotification = true;
	
	bDisplayMemoryLimitsOnCompile = true;
	bAlwaysDisplayStructMemoryUsage = false;
	StructMemoryLimitWarningThreshold = 0.9f;
	
	bDisplayInputEventNotes = true;
	
	bRestrictInvalidCharacters = true;
	bWarnIfChildrenAreOutOfDate = true;
	bCalculateGuidsOnCompile = true;
	bLinkerLoadHandling = true;
	
	bDefaultNewTransitionsToTrue = false;
	bDefaultNewConduitsToTrue = false;
	bConfigureNewConduitsAsTransitions = true;

	DefaultStateMachineBlueprintParentClass = USMInstance::StaticClass();
	DefaultStateMachineBlueprintNamePrefix = TEXT("BP_");
	DefaultNodeBlueprintNamePrefix = TEXT("BP_");
	
	EditorNodeConstructionScriptSetting = ESMEditorConstructionScriptProjectSetting::SM_Standard;
	DefaultStateClass = USMStateInstance::StaticClass();
	DefaultStateMachineClass = USMStateMachineInstance::StaticClass();
	DefaultConduitClass = USMConduitInstance::StaticClass();
	DefaultTransitionClass = USMTransitionInstance::StaticClass();
	bEnableVariableCustomization = true;
	bEnableReferenceTemplatesByDefault = false;
	bRunConstructionScriptsOnLoad = true;

	OverrideActorSoftReferencePins = ESMPinOverride::LogicDriverOnly;
	OverrideObjectPins = ESMPinOverride::LogicDriverOnly;
	bDisplayThumbnailsByDefault = true;
	
	bEnablePreviewMode = true;
}

void USMProjectEditorSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (DefaultStateMachineBlueprintParentClass.IsNull())
	{
		DefaultStateMachineBlueprintParentClass = USMInstance::StaticClass();
	}
	
	if (DefaultStateClass.IsNull())
	{
		DefaultStateClass = USMStateInstance::StaticClass();
	}

	if (DefaultStateMachineClass.IsNull())
	{
		DefaultStateMachineClass = USMStateMachineInstance::StaticClass();
	}

	if (DefaultConduitClass.IsNull())
	{
		DefaultConduitClass = USMConduitInstance::StaticClass();
	}

	if (DefaultTransitionClass.IsNull())
	{
		DefaultTransitionClass = USMTransitionInstance::StaticClass();
	}
	
	SaveConfig();
}
