// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMContentAssetTypeActions.h"
#include "Assets/SMContentAsset.h"

#define LOCTEXT_NAMESPACE "SMContentAssetTypeActions"

FSMContentAssetTypeActions::FSMContentAssetTypeActions(uint32 InAssetCategory)
	: FAssetTypeActions_ClassTypeBase()
{
	MyAssetCategory = InAssetCategory;
}

FText FSMContentAssetTypeActions::GetName() const
{
	return LOCTEXT("FSMContentAssetTypeActions", "ContentSample");
}

FColor FSMContentAssetTypeActions::GetTypeColor() const
{
	return FColor(225, 225, 225);
}

UClass* FSMContentAssetTypeActions::GetSupportedClass() const
{
	return USMContentAsset::StaticClass();
}

uint32 FSMContentAssetTypeActions::GetCategories()
{
	return MyAssetCategory;
}

TWeakPtr<IClassTypeActions> FSMContentAssetTypeActions::GetClassTypeActions(const FAssetData& AssetData) const
{
	return nullptr;
}


#undef LOCTEXT_NAMESPACE
