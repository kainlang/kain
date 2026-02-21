// Copyright Narrative Tools 2022. 

#include "AssetTypeActions_NarrativeQuestTask.h"
#include "NarrativeDataTask.h"
#include "NarrativeCondition.h"
#include "NarrativeEvent.h"
#include "QuestTask.h"
#include "NarrativeQuestTaskBlueprint.h"
#include <Factories/BlueprintFactory.h>

FAssetTypeActions_NarrativeQuestTask::FAssetTypeActions_NarrativeQuestTask(uint32 InAssetCategory) : Category(InAssetCategory)
{

}

UClass* FAssetTypeActions_NarrativeQuestTask::GetSupportedClass() const
{
	return UNarrativeTaskBlueprint::StaticClass();
}

uint32 FAssetTypeActions_NarrativeQuestTask::GetCategories()
{
	return Category;
}

FAssetTypeActions_NarrativeCondition::FAssetTypeActions_NarrativeCondition(uint32 InAssetCategory) : Category(InAssetCategory)
{

}

UClass* FAssetTypeActions_NarrativeCondition::GetSupportedClass() const
{
	return UNarrativeCondition::StaticClass();
}

uint32 FAssetTypeActions_NarrativeCondition::GetCategories()
{
	return Category;
}

UFactory* FAssetTypeActions_NarrativeCondition::GetFactoryForBlueprintType(UBlueprint* InBlueprint) const
{
	UBlueprintFactory* BlueprintFactory = NewObject<UBlueprintFactory>();
	BlueprintFactory->ParentClass = UNarrativeCondition::StaticClass();
	return BlueprintFactory;
}

FAssetTypeActions_NarrativeEvent::FAssetTypeActions_NarrativeEvent(uint32 InAssetCategory) : Category(InAssetCategory)
{

}

UClass* FAssetTypeActions_NarrativeEvent::GetSupportedClass() const
{
	return UNarrativeEvent::StaticClass();
}

uint32 FAssetTypeActions_NarrativeEvent::GetCategories()
{
	return Category;
}

UFactory* FAssetTypeActions_NarrativeEvent::GetFactoryForBlueprintType(UBlueprint* InBlueprint) const
{
	UBlueprintFactory* BlueprintFactory = NewObject<UBlueprintFactory>();
	BlueprintFactory->ParentClass = UNarrativeEvent::StaticClass();
	return BlueprintFactory;
}
