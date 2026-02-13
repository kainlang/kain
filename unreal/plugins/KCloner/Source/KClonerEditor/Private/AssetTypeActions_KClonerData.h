// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"
#include "KClonerData.h"

class FAssetTypeActions_KClonerData : public FAssetTypeActions_Base
{
public:
	FAssetTypeActions_KClonerData(EAssetTypeCategories::Type InAssetCategory)
		: MyAssetCategory(InAssetCategory)
	{}

	// IAssetTypeActions interface
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
	virtual bool HasActions(const TArray<UObject*>& InObjects) const override { return true; }
	virtual void GetActions(const TArray<UObject*>& InObjects, struct FToolMenuSection& Section) override;
	// End of IAssetTypeActions interface

private:
	EAssetTypeCategories::Type MyAssetCategory;

	// Bake action handlers
	void ExecuteBakeToStaticMesh(TArray<TWeakObjectPtr<UKClonerData>> Objects);
	void ExecuteBakeToAlembic(TArray<TWeakObjectPtr<UKClonerData>> Objects);
	void ExecuteBakeToGeometryCache(TArray<TWeakObjectPtr<UKClonerData>> Objects);
	void ExecuteBakeToVAT(TArray<TWeakObjectPtr<UKClonerData>> Objects);
};
