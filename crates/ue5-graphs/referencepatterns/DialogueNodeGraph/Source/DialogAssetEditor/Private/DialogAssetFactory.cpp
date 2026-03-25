/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#include "DialogAssetFactory.h"
#include "DialogAsset.h"

UDialogAssetFactory::UDialogAssetFactory(const FObjectInitializer& objectInitializer) : Super(objectInitializer) {
	SupportedClass = UDialogAsset::StaticClass();
}

UObject* UDialogAssetFactory::FactoryCreateNew(UClass* uclass, UObject* inParent, FName name, EObjectFlags flags, UObject* context, FFeedbackContext* warn) {
    UDialogAsset* asset = NewObject<UDialogAsset>(inParent, name, flags);
	return asset;
}

bool UDialogAssetFactory::CanCreateNew() const {
    return true;
}
