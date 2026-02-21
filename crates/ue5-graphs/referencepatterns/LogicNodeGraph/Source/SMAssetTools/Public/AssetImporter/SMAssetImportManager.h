// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMAssetImporter.h"

#include "Templates/SubclassOf.h"

class SMASSETTOOLS_API FSMAssetImportManager
{
public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnAssetImportedSignature, const USMAssetImporter::FImportResult& /*InResult*/);

	virtual ~FSMAssetImportManager() {}

	/**
	 * Registers an importer to use with state machine assets.
	 *
	 * @param InImporterName The name to register the importer under, such as "json".
	 * @param InImporterClass The class to instantiate when the importer is used.
	 */
	void RegisterImporter(const FString& InImporterName, UClass* InImporterClass);

	/**
	 * Unregisters an importer for use with state machine assets.
	 *
	 * @param InImporterName The name of the previously registered importer.
	 */
	void UnregisterImporter(const FString& InImporterName);

	/**
	 * Import a state machine to a blueprint.
	 *
	 * @param InImportArgs Arguments to use when importing an asset.
	 *
	 * @return The blueprint created or used to receive the import data.
	 */
	USMAssetImporter::FImportResult ImportAsset(const USMAssetImporter::FImportArgs& InImportArgs);

	/** Return a list of all supported import types. */
	TArray<FString> GetSupportedImportTypes() const;

	/** Called when an asset has been imported. */
	FOnAssetImportedSignature& OnAssetImported() { return OnAssetImportedEvent; }

private:
	USMAssetImporter::EImportStatus ImportAsset(const USMAssetImporter::FImportArgs& InImportArgs, USMAssetImporter* InImporter);

private:
	TMap<FString, TSubclassOf<USMAssetImporter>> MappedImporters;
	FOnAssetImportedSignature OnAssetImportedEvent;
};
