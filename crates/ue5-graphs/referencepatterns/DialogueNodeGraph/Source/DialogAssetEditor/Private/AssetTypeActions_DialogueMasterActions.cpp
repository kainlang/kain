/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#include "AssetTypeActions_DialogueMasterActions.h"

#include "DialogNodeInfo.h"
#include "Factories/BlueprintFactory.h"
#include "CameraShot.h"

// -----------------------------------------------------------
// Dialogue Condition
// -----------------------------------------------------------
FAssetTypeActions_DialogueMasterCondition::FAssetTypeActions_DialogueMasterCondition(uint32 category)
{
	_assetCategory = category;
}

FText FAssetTypeActions_DialogueMasterCondition::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_DialogueCondition", "Dialogue Condition");
}

UClass* FAssetTypeActions_DialogueMasterCondition::GetSupportedClass() const
{
	return UAdvancedPrerequisiteBase::StaticClass();
}

uint32 FAssetTypeActions_DialogueMasterCondition::GetCategories()
{
	return _assetCategory;
}

UFactory* FAssetTypeActions_DialogueMasterCondition::GetFactoryForBlueprintType(UBlueprint* InBlueprint) const
{
	UBlueprintFactory* BlueprintFactory = NewObject<UBlueprintFactory>();
	BlueprintFactory->ParentClass = UAdvancedPrerequisiteBase::StaticClass();
	return BlueprintFactory;
}

// -----------------------------------------------------------
// Dialogue Action
// -----------------------------------------------------------
FAssetTypeActions_DialogueMasterAction::FAssetTypeActions_DialogueMasterAction(uint32 category)
{
	_assetCategory = category;
}

FText FAssetTypeActions_DialogueMasterAction::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_DialogueAction", "Dialogue Action");
}

UClass* FAssetTypeActions_DialogueMasterAction::GetSupportedClass() const
{
	return UDialogueMasterAction::StaticClass();
}

uint32 FAssetTypeActions_DialogueMasterAction::GetCategories()
{
	return _assetCategory;
}

UFactory* FAssetTypeActions_DialogueMasterAction::GetFactoryForBlueprintType(UBlueprint* InBlueprint) const
{
	UBlueprintFactory* BlueprintFactory = NewObject<UBlueprintFactory>();
	BlueprintFactory->ParentClass = UDialogueMasterAction::StaticClass();
	return BlueprintFactory;
}

// -----------------------------------------------------------
// Custom camera shot
// -----------------------------------------------------------
FAssetTypeActions_DialogueMasterCustomCameraShot::FAssetTypeActions_DialogueMasterCustomCameraShot(uint32 category)
{
	_assetCategory = category;
}

FText FAssetTypeActions_DialogueMasterCustomCameraShot::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_DialogueCameraShot", "Custom Camera shot");
}

UClass* FAssetTypeActions_DialogueMasterCustomCameraShot::GetSupportedClass() const
{
	return ADialogCameraActor::StaticClass();
}

uint32 FAssetTypeActions_DialogueMasterCustomCameraShot::GetCategories()
{
	return _assetCategory;
}

UFactory* FAssetTypeActions_DialogueMasterCustomCameraShot::GetFactoryForBlueprintType(UBlueprint* InBlueprint) const
{
	UBlueprintFactory* BlueprintFactory = NewObject<UBlueprintFactory>();
	BlueprintFactory->ParentClass = ADialogCameraActor::StaticClass();
	return BlueprintFactory;
}