// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ISMAssetToolsModule.h"

class FSMAssetToolsModule : public ISMAssetToolsModule
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	virtual TSharedPtr<ISMAssetManager> GetAssetManagerInterface() const override;
	virtual TSharedPtr<ISMGraphGeneration> GetGraphGenerationInterface() const override;
	virtual TSharedPtr<FSMAssetExportManager> GetAssetExporter() const override;
	virtual TSharedPtr<FSMAssetImportManager> GetAssetImporter() const override;

private:
	mutable TSharedPtr<ISMAssetManager> AssetManagerInterface;
	mutable TSharedPtr<ISMGraphGeneration> GraphGenerationInterface;
	mutable TSharedPtr<FSMAssetExportManager> AssetExporter;
	mutable TSharedPtr<FSMAssetImportManager> AssetImporter;
};
