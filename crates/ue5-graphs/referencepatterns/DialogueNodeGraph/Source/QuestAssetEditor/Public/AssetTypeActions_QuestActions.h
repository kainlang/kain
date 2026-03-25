/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */
#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions/AssetTypeActions_Blueprint.h"

class FAssetTypeActions_DialogueMasterQuestTask : public FAssetTypeActions_Blueprint
{
public:
	FAssetTypeActions_DialogueMasterQuestTask(uint32 category);

public: // FAssetTypeActions_Base interface
	virtual FText GetName() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override;

	virtual UFactory* GetFactoryForBlueprintType(UBlueprint* InBlueprint) const override;

private:
	uint32 _assetCategory;
	
};

class FAssetTypeActions_DialogueMasterStatistic : public FAssetTypeActions_Base
{
public:
	FAssetTypeActions_DialogueMasterStatistic(uint32 Category);

	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override;
private:
	uint32 _assetCategory;
};



