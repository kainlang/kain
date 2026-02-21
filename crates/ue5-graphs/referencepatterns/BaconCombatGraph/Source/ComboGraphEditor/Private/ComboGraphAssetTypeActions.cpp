// © 2025 mrbaconvn. All Rights Reserved.


#include "ComboGraphAssetTypeActions.h"
#include "ComboGraphAsset.h"
#include "ComboGraphEditorApp.h"

#define LOCTEXT_NAMESPACE "ComboGraphEditor"
#define ACTION_NAME LOCTEXT("AssetTypeActions.ComboGraph", "Combo Graph")

UComboGraphAssetTypeActions::UComboGraphAssetTypeActions(EAssetTypeCategories::Type category) {
	_assetCategory = category;
}

FText UComboGraphAssetTypeActions::GetName() const {
	return ACTION_NAME;
}

FColor UComboGraphAssetTypeActions::GetTypeColor() const {
	return FColor::Red;
}

UClass* UComboGraphAssetTypeActions::GetSupportedClass() const {
	return UComboGraphAsset::StaticClass();
}

void UComboGraphAssetTypeActions::OpenAssetEditor(const TArray<UObject*>& inObjects, TSharedPtr<class IToolkitHost> editWithinLevelEditor) {
	EToolkitMode::Type mode = editWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;
	for (UObject* object : inObjects) {
		UComboGraphAsset* ComboGraph = Cast<UComboGraphAsset>(object);
		if (ComboGraph != nullptr) {
			TSharedRef<FComboGraphEditorApp> editor(new FComboGraphEditorApp());
			editor->InitEditor(mode, editWithinLevelEditor, ComboGraph);
		}
	}
}

uint32 UComboGraphAssetTypeActions::GetCategories() {
	return _assetCategory;
}

#undef LOCTEXT_NAMESPACE