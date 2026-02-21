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

class FAssetTypeActions_DialogueMasterCondition : public FAssetTypeActions_Blueprint
{
public:
	FAssetTypeActions_DialogueMasterCondition(uint32 category);

public: // FAssetTypeActions_Base interface
	virtual FText GetName() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override;

	virtual UFactory* GetFactoryForBlueprintType(UBlueprint* InBlueprint) const override;

private:
	uint32 _assetCategory;
	
};


class FAssetTypeActions_DialogueMasterAction : public FAssetTypeActions_Blueprint
{
public:
	FAssetTypeActions_DialogueMasterAction(uint32 category);

public: // FAssetTypeActions_Base interface
	virtual FText GetName() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override;

	virtual UFactory* GetFactoryForBlueprintType(UBlueprint* InBlueprint) const override;

private:
	uint32 _assetCategory;
	
};


class FAssetTypeActions_DialogueMasterCustomCameraShot : public FAssetTypeActions_Blueprint
{
public:
	FAssetTypeActions_DialogueMasterCustomCameraShot(uint32 category);

public: // FAssetTypeActions_Base interface
	virtual FText GetName() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override;

	virtual UFactory* GetFactoryForBlueprintType(UBlueprint* InBlueprint) const override;

private:
	uint32 _assetCategory;
	
};