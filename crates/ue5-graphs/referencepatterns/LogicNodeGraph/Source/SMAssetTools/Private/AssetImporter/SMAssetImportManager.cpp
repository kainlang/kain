// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "AssetImporter/SMAssetImportManager.h"

#include "SMAssetToolsLog.h"

#include "Blueprints/SMBlueprint.h"
#include "Graph/SMGraph.h"

#include "Utilities/SMBlueprintEditorUtils.h"

#include "Kismet2/KismetEditorUtilities.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "SMAssetImportManager"

void FSMAssetImportManager::RegisterImporter(const FString& InImporterName, UClass* InImporterClass)
{
	if (!ensure(!InImporterName.IsEmpty()))
	{
		return;
	}
	check(InImporterClass);
	MappedImporters.Add(InImporterName.ToLower(), InImporterClass);
}

void FSMAssetImportManager::UnregisterImporter(const FString& InImporterName)
{
	MappedImporters.Remove(InImporterName.ToLower());
}

USMAssetImporter::FImportResult FSMAssetImportManager::ImportAsset(const USMAssetImporter::FImportArgs& InImportArgs)
{
	USMAssetImporter::FImportResult Result;
	Result.ResultStatus = USMAssetImporter::EImportStatus::Failure;
	if (!ensureMsgf(!(InImportArgs.ImportFullFilePath.IsEmpty() != true) != !(InImportArgs.ImportData != nullptr), TEXT("Either import file XOR import data should be set.")))
	{
		return MoveTemp(Result);
	}

	const FString ImportType = InImportArgs.ImportType.IsEmpty() ? FPaths::GetExtension(InImportArgs.ImportFullFilePath) :
	InImportArgs.ImportType;

	if (!ensureMsgf(!ImportType.IsEmpty(), TEXT("No import type provided or discoverable.")))
	{
		return MoveTemp(Result);
	}

	if (!ensureMsgf(!InImportArgs.SaveToContentPath.IsEmpty() || InImportArgs.ImportToBlueprint.IsValid(),
		TEXT("SaveToFilePath and ImportToBlueprint aren't set. At least one is required for importing.")))
	{
		return MoveTemp(Result);
	}

	USMAssetImporter* Importer = nullptr;
	if (const TSubclassOf<USMAssetImporter>* ImporterClassPtr = MappedImporters.Find(ImportType.ToLower()))
	{
		if (const UClass* ImporterClass = *ImporterClassPtr)
		{
			Importer = NewObject<USMAssetImporter>(GetTransientPackage(), ImporterClass);
		}
	}

	if (!ensureMsgf(Importer, TEXT("Could not find importer for %s."), *ImportType))
	{
		return MoveTemp(Result);
	}

	Importer->AddToRoot();

	FScopedTransaction Transaction(NSLOCTEXT("LogicDriverImport", "ImportAsset", "Import Asset"));

	if (!InImportArgs.ImportFullFilePath.IsEmpty())
	{
		if (Importer->ReadImportFile(InImportArgs.ImportFullFilePath, InImportArgs) == USMAssetImporter::EImportStatus::Failure)
		{
			LDASSETTOOLS_LOG_ERROR(TEXT("Could not validate file %s for import."), *InImportArgs.ImportFullFilePath);
			return MoveTemp(Result);
		}
	}
	else
	{
		if (Importer->ReadImportData(InImportArgs.ImportData, InImportArgs) == USMAssetImporter::EImportStatus::Failure)
		{
			LDASSETTOOLS_LOG_ERROR(TEXT("Could not validate import data %s for import."), *InImportArgs.ImportType);
			return MoveTemp(Result);
		}
	}

	USMBlueprint* BlueprintToUse = InImportArgs.ImportToBlueprint.Get();
	if (BlueprintToUse)
	{
		// Existing blueprint.
		if (InImportArgs.bClearExisting)
		{
			FSMBlueprintEditorUtils::RemoveAllNodesFromGraph(FSMBlueprintEditorUtils::GetRootStateMachineGraph(BlueprintToUse), BlueprintToUse);
		}
	}
	else
	{
		// Create new blueprint.
		BlueprintToUse = Importer->CreateBlueprint(InImportArgs);
		if (!ensureMsgf(BlueprintToUse, TEXT("Could not create a new blueprint for import at path %s."), *InImportArgs.SaveToContentPath))
		{
			return MoveTemp(Result);
		}
	}

	USMAssetImporter::FImportArgs CompiledImportArgs(InImportArgs);
	CompiledImportArgs.ImportToBlueprint = BlueprintToUse;
	CompiledImportArgs.ImportType = ImportType;

	const USMAssetImporter::EImportStatus Status = ImportAsset(MoveTemp(CompiledImportArgs), Importer);
	Result.ResultStatus = Status;
	Result.Blueprint = BlueprintToUse;
	Result.AssetImporter = TStrongObjectPtr<USMAssetImporter>(Importer);

	if (CompiledImportArgs.bCompileBlueprint)
	{
		FKismetEditorUtilities::CompileBlueprint(BlueprintToUse);
	}

	OnAssetImportedEvent.Broadcast(Result);

	Importer->RemoveFromRoot();

	return MoveTemp(Result);
}

TArray<FString> FSMAssetImportManager::GetSupportedImportTypes() const
{
	TArray<FString> ImportTypes;
	MappedImporters.GetKeys(ImportTypes);

	return MoveTemp(ImportTypes);
}

USMAssetImporter::EImportStatus FSMAssetImportManager::ImportAsset(const USMAssetImporter::FImportArgs& InImportArgs, USMAssetImporter* InImporter)
{
	check(InImporter);
	check(InImportArgs.ImportToBlueprint.IsValid());
	check(InImportArgs.ImportToBlueprint->GeneratedClass);
	check(InImportArgs.ImportToBlueprint->GeneratedClass->ClassDefaultObject);

	auto HandleImportFinish = [&](USMAssetImporter::EImportStatus InStatus) -> void
	{
		InImporter->FinishImport(InImportArgs.ImportToBlueprint.Get(), InStatus);
	};

	{
		const USMAssetImporter::EImportStatus Status = InImporter->ImportCDO(InImportArgs.ImportToBlueprint->GeneratedClass->ClassDefaultObject);
		if (Status == USMAssetImporter::EImportStatus::Failure)
		{
			HandleImportFinish(Status);
			return Status;
		}
	}

	{
		USMGraph* RootStateMachineGraph = FSMBlueprintEditorUtils::GetRootStateMachineGraph(InImportArgs.ImportToBlueprint.Get());
		check(RootStateMachineGraph);
		const USMAssetImporter::EImportStatus Status = InImporter->ImportRootGraph(RootStateMachineGraph);
		if (Status == USMAssetImporter::EImportStatus::Failure)
		{
			HandleImportFinish(Status);
			return Status;
		}
	}

	HandleImportFinish(USMAssetImporter::EImportStatus::Success);
	return USMAssetImporter::EImportStatus::Success;
}

#undef LOCTEXT_NAMESPACE