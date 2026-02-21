// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "AssetExporter/SMAssetExportManager.h"

#include "Blueprints/SMBlueprintGeneratedClass.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "Misc/Paths.h"

void FSMAssetExportManager::RegisterExporter(const FString& InExporterName, UClass* InExporterClass)
{
	if (!ensure(!InExporterName.IsEmpty()))
	{
		return;
	}
	check(InExporterClass);
	MappedExporters.Add(InExporterName.ToLower(), InExporterClass);
}

void FSMAssetExportManager::UnregisterExporter(const FString& InExporterName)
{
	MappedExporters.Remove(InExporterName.ToLower());
}

USMAssetExporter::FExportResult FSMAssetExportManager::ExportAsset(const USMAssetExporter::FExportArgs& InExportArgs)
{
	check(InExportArgs.Blueprint.IsValid());

	USMAssetExporter::FExportResult Result;
	Result.ExportStatus = USMAssetExporter::EExportStatus::Failure;

	const FString Extension = InExportArgs.ExportType.IsEmpty() ? FPaths::GetExtension(InExportArgs.ExportFullFilePath) : InExportArgs.ExportType;

	if (ensureMsgf(!Extension.IsEmpty(), TEXT("Export format cannot be determined.")))
	{
		if (const TSubclassOf<USMAssetExporter>* ExporterClassPtr = MappedExporters.Find(Extension))
		{
			if (const UClass* ExporterClass = *ExporterClassPtr)
			{
				USMAssetExporter* Exporter = NewObject<USMAssetExporter>(GetTransientPackage(), ExporterClass);
				Result.ExportStatus = ExportAsset(InExportArgs, Exporter);
				Result.AssetExporter = TStrongObjectPtr<USMAssetExporter>(Exporter);
			}
		}
	}

	Result.ExportedBlueprint = InExportArgs.Blueprint;

	OnAssetExportedEvent.Broadcast(Result);

	return MoveTemp(Result);
}

TArray<FString> FSMAssetExportManager::GetSupportedExportTypes() const
{
	TArray<FString> ExportTypes;
	MappedExporters.GetKeys(ExportTypes);

	return MoveTemp(ExportTypes);
}

USMAssetExporter::EExportStatus FSMAssetExportManager::ExportAsset(const USMAssetExporter::FExportArgs& InExportArgs, USMAssetExporter* InExporter)
{
	check(InExportArgs.Blueprint.IsValid());
	check(InExporter);

	auto HandleExportFinish = [&](USMAssetExporter::EExportStatus InExportStatus) -> void
	{
		InExporter->FinishExport(InExportArgs.Blueprint.Get(), InExportStatus);
	};

	{
		const USMAssetExporter::EExportStatus Status = InExporter->BeginExport(InExportArgs);
		if (Status == USMAssetExporter::EExportStatus::Failure)
		{
			HandleExportFinish(Status);
			return Status;
		}
	}

	{
		const USMAssetExporter::EExportStatus Status = InExporter->ExportCDO(InExportArgs.Blueprint->GetGeneratedClass()->ClassDefaultObject);
		if (Status == USMAssetExporter::EExportStatus::Failure)
		{
			HandleExportFinish(Status);
			return Status;
		}
	}

	TArray<UEdGraphNode*> GraphNodes;
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested(InExportArgs.Blueprint.Get(), GraphNodes);

	for (const UEdGraphNode* GraphNode : GraphNodes)
	{
		switch (const USMAssetExporter::EExportStatus ExportStatus = InExporter->ExportNode(GraphNode))
		{
		case USMAssetExporter::EExportStatus::Failure:
			{
				HandleExportFinish(ExportStatus);
				return ExportStatus;
			}
		default:
			{
				break;
			}
		}
	}

	HandleExportFinish(USMAssetExporter::EExportStatus::Success);
	return USMAssetExporter::EExportStatus::Success;
}
