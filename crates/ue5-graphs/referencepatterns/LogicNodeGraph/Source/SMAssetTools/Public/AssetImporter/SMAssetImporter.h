// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "ISMAssetManager.h"

#include "UObject/StrongObjectPtr.h"

#include "SMAssetImporter.generated.h"

class USMBlueprint;
class USMGraph;

UCLASS(Abstract)
class SMASSETTOOLS_API USMAssetImporter : public UObject
{
	GENERATED_BODY()

public:
	struct FImportArgs
	{
		/** [Required if ImportData null] Full file system path to import from. */
		FString ImportFullFilePath;

		/** [Required if ImportFullFilePath empty] The raw data to be imported. */
		void* ImportData = nullptr;

		/** [Optional] Type of import such as "json". If empty the extension from the file path will be used. */
		FString ImportType;

		/** [Required if Blueprint null] The relative content path to create and save an asset to. */
		FString SaveToContentPath;

		/** [Required if SaveToContentPath empty] The existing blueprint which will receive the import data. */
		TWeakObjectPtr<USMBlueprint> ImportToBlueprint;

		/** [Optional] If ImportToBlueprint is specified, should it be cleared of existing data prior to an import? */
		bool bClearExisting = true;

		/** [Optional] Verify the version field LD::JsonFields::FIELD_JSON_VERSION when importing. */
		bool bCheckVersion = true;

		/** [Optional] Compile the blueprint after import. */
		bool bCompileBlueprint = false;
	};

	enum class EImportStatus : uint8
	{
		Success,
		Failure
	};

	struct FImportResult
	{
		EImportStatus ResultStatus;
		TWeakObjectPtr<USMBlueprint> Blueprint;
		TStrongObjectPtr<USMAssetImporter> AssetImporter;
	};

	EImportStatus ReadImportFile(const FString& InFilePath, const FImportArgs& InImportArgs);
	EImportStatus ReadImportData(void* InData, const FImportArgs& InImportArgs);
	USMBlueprint* CreateBlueprint(const FImportArgs& InImportArgs);
	EImportStatus ImportCDO(UObject* InCDO);
	EImportStatus ImportRootGraph(USMGraph* InGraph);
	void FinishImport(USMBlueprint* InBlueprint, EImportStatus InStatus);

protected:
	/**
	 * Called prior to other methods so the source file can be validated and opened. This can prevent a blueprint
	 * being created or an existing blueprint graph destroyed if the input file isn't valid.
	 *
	 * This is not called if raw import data is used instead.
	 *
	 * @param InFilePath The file path of the file to be imported.
	 * @param InImportArgs The import args are not guaranteed to be filled out.
	 * @return The status of the import. Returning Failure will prevent processing from continuing.
	 */
	virtual EImportStatus OnReadImportFile(const FString& InFilePath, const FImportArgs& InImportArgs) { return EImportStatus::Success; }

	/**
	 * Called prior to other methods so the source data can be validated and read. This can prevent a blueprint
	 * being created or an existing blueprint graph destroyed if the input file isn't valid.
	 *
	 * This is not called if a file path is being used instead.
	 *
	 * @param InData Raw data to be read.
	 * @param InImportArgs The import args are not guaranteed to be filled out.
	 * @return The status of the import. Returning Failure will prevent processing from continuing.
	 */
	virtual EImportStatus OnReadImportData(void* InData, const FImportArgs& InImportArgs) { return EImportStatus::Success; }

	/**
	 * Create a blueprint for use with import. Only called if no blueprint was passed into the original import call.
	 * This method is not necessary to overload if the parent class doesn't need to change.
	 *
	 * @param InImportArgs The import args are not guaranteed to be filled out.
	 * @param InOutCreationArgs The creation args to use when creating a new blueprint.
	 */
	virtual void OnGetBlueprintCreationArgs(const FImportArgs& InImportArgs, ISMAssetManager::FCreateStateMachineBlueprintArgs& InOutCreationArgs) {}

	/**
	 * Called when the class defaults are being imported.
	 *
	 * @param InCDO The class default object.
	 * @return The status of the import. Returning Failure will prevent processing from continuing.
	 */
	virtual EImportStatus OnImportCDO(UObject* InCDO) { return EImportStatus::Success; }

	/**
	 * Called when the root graph is being imported. All sub graphs should be created at this stage.
	 *
	 * @param InGraph The root state machine graph.
	 * @return The status of the import. Returning Failure will prevent processing from continuing.
	 */
	virtual EImportStatus OnImportRootGraph(USMGraph* InGraph) { return EImportStatus::Success; }

	/**
	 * Called after all other import methods have finished.
	 *
	 * @param InBlueprint The blueprint being imported.
	 * @param InStatus The final status of the import.
	 */
	virtual void OnFinishImport(USMBlueprint* InBlueprint, EImportStatus InStatus) {}

};