// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "AssetTypeActions/AssetTypeActions_ClassTypeBase.h"

class FSMContentAssetTypeActions : public FAssetTypeActions_ClassTypeBase
{
public:
	FSMContentAssetTypeActions(uint32 InAssetCategory);

	// FAssetTypeActions_Base
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override;
	virtual TWeakPtr<IClassTypeActions> GetClassTypeActions(const FAssetData& AssetData) const override;
	// ~FAssetTypeActions_Base

protected:
	uint32 MyAssetCategory;
};
