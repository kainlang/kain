// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Blueprints/SMBlueprint.h"

#include "UObject/StrongObjectPtr.h"

#include "SMAssetExporter.generated.h"

class USMBlueprint;
class USMGraphNode_Base;

UCLASS(Abstract)
class SMASSETTOOLS_API USMAssetExporter : public UObject
{
	GENERATED_BODY()

public:
	enum class EExportStatus : uint8
	{
		Success,
		Failure,
		Skipped
	};

	struct FExportArgs
	{
		/** [Required] The blueprint to export. */
		TWeakObjectPtr<USMBlueprint> Blueprint;

		/** [Required if bMemoryOnly == false] The file path to write the file to. */
		FString ExportFullFilePath;

		/** [Optional] The export type to use. If not set the type is determined from the ExportFilePath. */
		FString ExportType;

		/** [Optional] Will not write to a file when true. The exporter should provide the object on FinishExport. */
		bool bMemoryOnly = false;
	};

	struct FExportResult
	{
		EExportStatus ExportStatus;
		TWeakObjectPtr<USMBlueprint> ExportedBlueprint;
		TStrongObjectPtr<USMAssetExporter> AssetExporter;
	};

	EExportStatus BeginExport(const FExportArgs& InExportArgs);
	EExportStatus ExportCDO(const UObject* InCDO);
	EExportStatus ExportNode(const UEdGraphNode* InGraphNode);
	void FinishExport(USMBlueprint* InBlueprint, EExportStatus InStatus);

protected:
	/**
	 * Called before all other export methods.
	 *
	 * @param InExportArgs The export args to use for exporting the asset.
	 * @return The status of the export. Returning Failure will prevent processing from continuing.
	 */
	virtual EExportStatus OnBeginExport(const FExportArgs& InExportArgs) { return EExportStatus::Success; }

	/**
	 * Called when the class defaults are being exported.
	 *
	 * @param InCDO The class default object.
	 * @return The status of the export. Returning Failure will prevent processing from continuing.
	 */
	virtual EExportStatus OnExportCDO(const UObject* InCDO) { return EExportStatus::Success; }

	/**
	 * Called for every node in the graph.
	 *
	 * @param InGraphNode The current graph node being exported.
	 * @return The status of the export. Returning Failure will prevent processing from continuing.
	 */
	virtual EExportStatus OnExportNode(const UEdGraphNode* InGraphNode) { return EExportStatus::Success; }

	/**
	 * Called after all other export methods. Finish writing any data to disk here.
	 *
	 * @param InBlueprint The blueprint that has finished exporting.
	 * @param InStatus The status of the export.
	 */
	virtual void OnFinishExport(USMBlueprint* InBlueprint, EExportStatus InStatus) {}
};