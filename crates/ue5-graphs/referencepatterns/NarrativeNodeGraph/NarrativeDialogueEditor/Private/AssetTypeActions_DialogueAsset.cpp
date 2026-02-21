// Copyright Narrative Tools 2022. 


#include "AssetTypeActions_DialogueAsset.h"
#include "DialogueGraphEditor.h"
#include "NarrativeDialogueEditorModule.h"
#include "DialogueAsset.h"
#include "DialogueAssetFactory.h"
#include "IContentBrowserSingleton.h"
#include <ContentBrowserModule.h>

#define LOCTEXT_NAMESPACE "AssetTypeActions_DialogueAsset"

FAssetTypeActions_DialogueAsset::FAssetTypeActions_DialogueAsset(uint32 InAssetCategory) : Category(InAssetCategory)
{

}

UClass* FAssetTypeActions_DialogueAsset::GetSupportedClass() const
{
	return UDialogueAsset::StaticClass();
}

uint32 FAssetTypeActions_DialogueAsset::GetCategories()
{
	return Category;
}

bool FAssetTypeActions_DialogueAsset::HasActions(const TArray<UObject*>& InObjects) const
{
	return true;
}

void FAssetTypeActions_DialogueAsset::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	auto LegacyDialogues = GetTypedWeakObjectPtrs<UDialogueAsset>(InObjects);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ConvertLegacyDialogueAssetToBlueprint", "Convert Legacy Dialogue Asset To Blueprint"),
		LOCTEXT("ConvertLegacyDialogueAssetToBlueprintDescription", "Converts a Legacy Dialogue Asset into a new Dialogue Blueprint."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(),
			"LevelEditor.ViewOptions"),
		FUIAction(
			FExecuteAction::CreateSP(this,
				&FAssetTypeActions_DialogueAsset::ConvertLegacyDialogueAssetsToBlueprint, LegacyDialogues),
			FCanExecuteAction()));
}

void FAssetTypeActions_DialogueAsset::ConvertLegacyDialogueAssetsToBlueprint(TArray<TWeakObjectPtr<UDialogueAsset>> Objects)
{

	TArray<UObject*> ObjectsToSync;
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Object = (*ObjIt).Get();
		if (Object)
		{
			UE_LOG(LogTemp, Warning, TEXT("Converting %s into DialogueBlueprint!"), *GetNameSafe(Object));

			// Determine an appropriate name
			FString Name;
			FString PackageName;
			CreateUniqueAssetName(Object->GetOutermost()->GetName(), "_Blueprint", PackageName, Name);

			// Create the factory used to generate the asset
			UDialogueAssetFactory* Factory = NewObject<UDialogueAssetFactory>();
			Factory->LegacyAsset = Object;

			FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
			UObject* NewAsset = AssetToolsModule.Get().CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), UDialogueBlueprint::StaticClass(), Factory);

			if (NewAsset)
			{
				ObjectsToSync.Add(NewAsset);
			}
		}
	}

	//FAssetTools isn't exposed from its module but this is its implementation so just use this 
	if (ObjectsToSync.Num() > 0)
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		ContentBrowserModule.Get().SyncBrowserToAssets(ObjectsToSync, /*bAllowLockedBrowsers=*/true);
	}
}

#undef LOCTEXT_NAMESPACE