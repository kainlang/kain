// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMAssetExporter.h"

#include "Templates/SubclassOf.h"

class SMASSETTOOLS_API FSMAssetExportManager
{
public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnAssetExportedSignature, const USMAssetExporter::FExportResult& /* ExportResult */);

	virtual ~FSMAssetExportManager() {}

	/**
	 * Registers an exporter to use with state machine assets.
	 *
	 * @param InExporterName The name to register the exporter under, such as "json".
	 * @param InExporterClass The class to instantiate when the exporter is used.
	 */
	void RegisterExporter(const FString& InExporterName, UClass* InExporterClass);

	/**
	 * Unregisters an exporter for use with state machine assets.
	 *
	 * @param InExporterName The name of the previously registered exporter.
	 */
	void UnregisterExporter(const FString& InExporterName);

	/**
	 * Export a state machine blueprint. Export type is determined automatically.
	 *
	 * @param InExportArgs Arguments for configuring the export.
	 *
	 * @return The result of the export.
	 */
	USMAssetExporter::FExportResult ExportAsset(const USMAssetExporter::FExportArgs& InExportArgs);

	/** Return a list of all supported export types. */
	TArray<FString> GetSupportedExportTypes() const;

	/** Called when an asset has been exported. */
	FOnAssetExportedSignature& OnAssetExported() { return OnAssetExportedEvent; }

private:
	USMAssetExporter::EExportStatus ExportAsset(const USMAssetExporter::FExportArgs& InExportArgs, USMAssetExporter* InExporter);

private:
	TMap<FString, TSubclassOf<USMAssetExporter>> MappedExporters;
	FOnAssetExportedSignature OnAssetExportedEvent;
};
