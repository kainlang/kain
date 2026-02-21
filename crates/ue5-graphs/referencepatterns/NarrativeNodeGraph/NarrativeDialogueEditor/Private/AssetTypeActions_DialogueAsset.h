// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "DialogueAsset.h"
#include "AssetTypeActions_Base.h"

class FAssetTypeActions_DialogueAsset : public FAssetTypeActions_Base
{

public:

	FAssetTypeActions_DialogueAsset(uint32 InAssetCategory);

	uint32 Category;

	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_DialogueAsset", "Dialogue Asset"); }
	virtual FColor GetTypeColor() const override { return FColor(255, 55, 120); }
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override;

	//Required to convert legacy dialogueassets into dialogueblueprints 
	virtual bool HasActions(const TArray<UObject*>& InObjects) const override;
	virtual void GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder) override;

private:

	void ConvertLegacyDialogueAssetsToBlueprint(TArray<TWeakObjectPtr<UDialogueAsset>> Objects);
};
