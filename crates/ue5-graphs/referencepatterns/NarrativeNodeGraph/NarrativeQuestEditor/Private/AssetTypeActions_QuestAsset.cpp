// Copyright Narrative Tools 2022. 

#include "AssetTypeActions_QuestAsset.h"
#include "QuestBlueprint.h"
#include "QuestGraphEditor.h"
#include "NarrativeQuestEditorModule.h"
#include "QuestAssetFactory.h"
#include <Engine/BlueprintCore.h>
#include <Animation/AnimBlueprint.h>

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FAssetTypeActions_QuestAsset::FAssetTypeActions_QuestAsset(uint32 InAssetCategory)
{
	Category = InAssetCategory;
}

UClass* FAssetTypeActions_QuestAsset::GetSupportedClass() const
{
	return UQuestBlueprint::StaticClass();
}

void FAssetTypeActions_QuestAsset::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor /*= TSharedPtr<IToolkitHost>()*/)
{
	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		if (UQuestBlueprint* QuestBP = Cast<UQuestBlueprint>(*ObjIt))
		{
			bool bFoundExisting = false;

			FQuestGraphEditor* ExistingInstance = nullptr;

			if (UAssetEditorSubsystem* AESubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
			{
				ExistingInstance = static_cast<FQuestGraphEditor*>(AESubsystem->FindEditorForAsset(QuestBP, false));
			}

			if (ExistingInstance != nullptr && ExistingInstance->GetQuestAsset() == nullptr)
			{
				ExistingInstance->InitQuestEditor(EToolkitMode::Standalone, EditWithinLevelEditor, QuestBP);
				bFoundExisting = true;
			}
			 
			if (!bFoundExisting)
			{
				FNarrativeQuestEditorModule& QuestEditorModule = FModuleManager::GetModuleChecked<FNarrativeQuestEditorModule>("NarrativeQuestEditor");
				TSharedRef<IQuestEditor> NewEditor = QuestEditorModule.CreateQuestEditor(EToolkitMode::Standalone, EditWithinLevelEditor, QuestBP);
			}
		}

	}
}

uint32 FAssetTypeActions_QuestAsset::GetCategories()
{
	return Category;
}

//class UFactory* FAssetTypeActions_QuestAsset::GetFactoryForBlueprintType(UBlueprint* InBlueprint) const
//{
//	if (UQuestAsset* QuestBP = Cast<UQuestAsset>(InBlueprint))
//	{
//		UQuestAssetFactory* QuestBPFactory = NewObject<UQuestAssetFactory>();
//
//		QuestBPFactory->ParentClass = TSubclassOf<UQuestAsset>(*InBlueprint->GeneratedClass);
//
//		return QuestBPFactory;
//	}
//
//	return nullptr;
//}

#undef LOCTEXT_NAMESPACE