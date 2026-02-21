// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions_Base.h"

class FAssetTypeActions_QuestAction : public FAssetTypeActions_Base
{
public:

	FAssetTypeActions_QuestAction(uint32 InAssetCategory);

	uint32 Category;

	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_NarrativeTask", "Data Task"); }
	virtual FColor GetTypeColor() const override { return FColor(255, 55, 120); /*return FColor(66, 134, 244); */ }
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override;

};
