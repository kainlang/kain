// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMAssetToolsModule.h"

#include "SMAssetToolsLog.h"
#include "AssetExporter/SMAssetExportManager.h"
#include "AssetExporter/Types/SMAssetExporterJson.h"
#include "AssetImporter/SMAssetImportManager.h"
#include "AssetImporter/Types/SMAssetImporterJson.h"
#include "AssetManager/SMAssetManager.h"
#include "Commands/SMAssetToolsCommands.h"
#include "GraphGeneration/SMGraphGeneration.h"
#include "UI/SMAssetToolbar.h"
#include "UI/SMNewAssetOptions.h"

#define LOCTEXT_NAMESPACE "SMAssetToolsModule"

DEFINE_LOG_CATEGORY(LogLogicDriverAssetTools)

void FSMAssetToolsModule::StartupModule()
{
	FSMAssetToolsCommands::Register();
	FSMAssetToolbar::Initialize();
	FSMNewAssetOptions::Initialize();
}

void FSMAssetToolsModule::ShutdownModule()
{
	FSMNewAssetOptions::Shutdown();
	FSMAssetToolbar::Shutdown();
	FSMAssetToolsCommands::Unregister();
}

TSharedPtr<ISMAssetManager> FSMAssetToolsModule::GetAssetManagerInterface() const
{
	if (!AssetManagerInterface.IsValid())
	{
		AssetManagerInterface = MakeShared<FSMAssetManager>();
	}

	return AssetManagerInterface;
}

TSharedPtr<ISMGraphGeneration> FSMAssetToolsModule::GetGraphGenerationInterface() const
{
	if (!GraphGenerationInterface.IsValid())
	{
		GraphGenerationInterface = MakeShared<FSMGraphGeneration>();
	}

	return GraphGenerationInterface;
}

TSharedPtr<FSMAssetExportManager> FSMAssetToolsModule::GetAssetExporter() const
{
	if (!AssetExporter.IsValid())
	{
		AssetExporter = MakeShared<FSMAssetExportManager>();
		AssetExporter->RegisterExporter(TEXT("json"), USMAssetExporterJson::StaticClass());
	}

	return AssetExporter;
}

TSharedPtr<FSMAssetImportManager> FSMAssetToolsModule::GetAssetImporter() const
{
	if (!AssetImporter.IsValid())
	{
		AssetImporter = MakeShared<FSMAssetImportManager>();
		AssetImporter->RegisterImporter(TEXT("json"), USMAssetImporterJson::StaticClass());
	}

	return AssetImporter;
}

IMPLEMENT_MODULE(FSMAssetToolsModule, SMAssetTools)

#undef LOCTEXT_NAMESPACE
