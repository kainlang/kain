// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions/AssetTypeActions_Blueprint.h"

class FAssetTypeActions_QuestAsset : public FAssetTypeActions_Blueprint
{
public:

	FAssetTypeActions_QuestAsset(uint32 InAssetCategory);

	uint32 Category;

	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_QuestBlueprint", "Quest Blueprint"); }
	virtual FColor GetTypeColor() const override { return FColor(255, 55, 120); }
	virtual UClass* GetSupportedClass() const override;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
	virtual uint32 GetCategories() override;

	//virtual class UFactory* GetFactoryForBlueprintType(UBlueprint* InBlueprint) const override;
};
