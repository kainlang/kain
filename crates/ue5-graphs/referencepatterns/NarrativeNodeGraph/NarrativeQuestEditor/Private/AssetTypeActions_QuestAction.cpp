// Copyright Narrative Tools 2022. 

#include "AssetTypeActions_QuestAction.h"
#include "NarrativeDataTask.h"

FAssetTypeActions_QuestAction::FAssetTypeActions_QuestAction(uint32 InAssetCategory) : Category(InAssetCategory)
{

}

UClass* FAssetTypeActions_QuestAction::GetSupportedClass() const
{
	return UNarrativeDataTask::StaticClass();
}

uint32 FAssetTypeActions_QuestAction::GetCategories()
{
	return Category;
}
