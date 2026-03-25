/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#include "..\Public\QuestAssetAction.h"
#include "DialogAsset.h"
#include "QuestAsset.h"
#include "QuestAssetEditorApp.h"

QuestAssetAction::QuestAssetAction(uint32 category) {
    _assetCategory = category;
}

FText QuestAssetAction::GetName() const {
    return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_MyQuestAsset", "Quest Asset");
}

FColor QuestAssetAction::GetTypeColor() const {
    return FColor::Red;
}

UClass* QuestAssetAction::GetSupportedClass() const {
    return UQuestAsset::StaticClass();
}

void QuestAssetAction::OpenAssetEditor(const TArray<UObject*>& inObjects, TSharedPtr<class IToolkitHost> editWithinLevelEditor) {
    EToolkitMode::Type mode = editWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;
	for (UObject* object : inObjects) {
		UQuestAsset* questAsset = Cast<UQuestAsset>(object);
		if (questAsset != nullptr) {
			TSharedRef<QuestAssetEditorApp> editor(new QuestAssetEditorApp());
			editor->InitEditor(mode, editWithinLevelEditor, questAsset);
		}
	}
}

uint32 QuestAssetAction::GetCategories() {
    return _assetCategory;
}
