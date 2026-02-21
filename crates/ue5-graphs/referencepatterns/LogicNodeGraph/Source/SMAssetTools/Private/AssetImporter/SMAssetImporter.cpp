// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "AssetImporter/SMAssetImporter.h"

#include "ISMAssetManager.h"
#include "ISMAssetToolsModule.h"

#include "Blueprints/SMBlueprint.h"

#include "Kismet2/KismetEditorUtilities.h"

USMAssetImporter::EImportStatus USMAssetImporter::ReadImportFile(const FString& InFilePath,
                                                                 const FImportArgs& InImportArgs)
{
	return OnReadImportFile(InFilePath, InImportArgs);
}

USMAssetImporter::EImportStatus USMAssetImporter::ReadImportData(void* InData, const FImportArgs& InImportArgs)
{
	return OnReadImportData(InData, InImportArgs);
}

USMBlueprint* USMAssetImporter::CreateBlueprint(const FImportArgs& InImportArgs)
{
	const ISMAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<ISMAssetToolsModule>(
	LOGICDRIVER_ASSET_TOOLS_MODULE_NAME);

	const FName AssetName = *FPaths::GetBaseFilename(InImportArgs.ImportFullFilePath);

	ISMAssetManager::FCreateStateMachineBlueprintArgs Args;
	Args.Name = AssetName;
	Args.Path = InImportArgs.SaveToContentPath;

	OnGetBlueprintCreationArgs(InImportArgs, Args);

	// Create a new asset.
	USMBlueprint* NewBlueprint = AssetToolsModule.GetAssetManagerInterface()->CreateStateMachineBlueprint(Args);
	if (NewBlueprint)
	{
		FKismetEditorUtilities::CompileBlueprint(NewBlueprint);
	}

	return NewBlueprint;
}

USMAssetImporter::EImportStatus USMAssetImporter::ImportCDO(UObject* InCDO)
{
	return OnImportCDO(InCDO);
}

USMAssetImporter::EImportStatus USMAssetImporter::ImportRootGraph(USMGraph* InGraph)
{
	return OnImportRootGraph(InGraph);
}

void USMAssetImporter::FinishImport(USMBlueprint* InBlueprint, EImportStatus InStatus)
{
	OnFinishImport(InBlueprint, InStatus);
}
