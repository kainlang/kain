/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#include "QuestAssetFactory.h"
#include "QuestAsset.h"

UQuestAssetFactory::UQuestAssetFactory(const FObjectInitializer& objectInitializer) : Super(objectInitializer) {
	SupportedClass = UQuestAsset::StaticClass();
}

UObject* UQuestAssetFactory::FactoryCreateNew(UClass* uclass, UObject* inParent, FName name, EObjectFlags flags, UObject* context, FFeedbackContext* warn) {
    UQuestAsset* asset = NewObject<UQuestAsset>(inParent, name, flags);
	return asset;
}

bool UQuestAssetFactory::CanCreateNew() const {
    return true;
}
