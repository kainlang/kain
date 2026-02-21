// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "AssetExporter/SMAssetExporter.h"

USMAssetExporter::EExportStatus USMAssetExporter::BeginExport(const FExportArgs& InExportArgs)
{
	return OnBeginExport(InExportArgs);
}

USMAssetExporter::EExportStatus USMAssetExporter::ExportCDO(const UObject* InCDO)
{
	return OnExportCDO(InCDO);
}

USMAssetExporter::EExportStatus USMAssetExporter::ExportNode(const UEdGraphNode* InGraphNode)
{
	return OnExportNode(InGraphNode);
}

void USMAssetExporter::FinishExport(USMBlueprint* InBlueprint, EExportStatus InStatus)
{
	OnFinishExport(InBlueprint, InStatus);
}
