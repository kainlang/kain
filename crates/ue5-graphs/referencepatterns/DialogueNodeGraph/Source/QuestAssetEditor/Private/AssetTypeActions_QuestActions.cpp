/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#include "AssetTypeActions_QuestActions.h"

#include "Factories/BlueprintFactory.h"
#include "DialogueMasterTask.h"
#include "DialogueMasterStatistic.h"

// -----------------------------------------------------------
// Dialogue Master Quest Task
// -----------------------------------------------------------

FAssetTypeActions_DialogueMasterQuestTask::FAssetTypeActions_DialogueMasterQuestTask(uint32 category)
{
	_assetCategory = category;
}

FText FAssetTypeActions_DialogueMasterQuestTask::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_QuestTask", "Quest Task");
}

UClass* FAssetTypeActions_DialogueMasterQuestTask::GetSupportedClass() const
{
	return UDialogueMasterTask::StaticClass();
}

uint32 FAssetTypeActions_DialogueMasterQuestTask::GetCategories()
{
	return _assetCategory;
}

UFactory* FAssetTypeActions_DialogueMasterQuestTask::GetFactoryForBlueprintType(UBlueprint* InBlueprint) const
{
	UBlueprintFactory* BlueprintFactory = NewObject<UBlueprintFactory>();
	BlueprintFactory->ParentClass = UDialogueMasterTask::StaticClass();
	return BlueprintFactory;
}




// -----------------------------------------------------------
// Dialogue Master Statistic
// -----------------------------------------------------------

FAssetTypeActions_DialogueMasterStatistic::FAssetTypeActions_DialogueMasterStatistic(uint32 Category)
{
	this->_assetCategory = Category;
}

FText FAssetTypeActions_DialogueMasterStatistic::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_DialogueMasterStatistic", "Statistic");
}

FColor FAssetTypeActions_DialogueMasterStatistic::GetTypeColor() const
{
	return FColor(255, 55, 55);
} 

UClass* FAssetTypeActions_DialogueMasterStatistic::GetSupportedClass() const
{
	return UDialogueMasterStatistic::StaticClass();
}

uint32 FAssetTypeActions_DialogueMasterStatistic::GetCategories()
{
	return _assetCategory;
}

