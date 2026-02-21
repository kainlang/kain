// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "EnhancedActionKeyMapping.h"

#include "SMContentAsset.generated.h"

class UInputMappingContext;
class UTexture2D;
struct FAssetData;

USTRUCT()
struct FSMInputActionWrapper
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = Default)
	TArray<FKey> Keys;
};

/** The type that can be assigned to content assets. */
UENUM()
enum class ESMContentType
{
	Example,
	Template,
	Library
};

/**
 * Import content to your project to use as examples or a base for your own implementation.
 */
UCLASS(NotBlueprintable, NotBlueprintType, HideDropdown)
class USMContentAsset : public UObject
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, Category = Content)
	FText Name;

	UPROPERTY(EditDefaultsOnly, Category = Content)
	FText Author;

	UPROPERTY(EditDefaultsOnly, meta = (MultiLine = "true"), Category = Content)
	FText Description;

	UPROPERTY(EditDefaultsOnly, meta = (MultiLine = "true"), Category = Content)
	FText DetailedDescription;

	/** The small title image. */
	UPROPERTY(EditDefaultsOnly, Category = Content)
	TObjectPtr<UTexture2D> TitleImage;

	/** The large image to display when selected. */
	UPROPERTY(EditDefaultsOnly, Category = Content)
	TObjectPtr<UTexture2D> DescriptionImage;

	/** The type of content this asset represents. */
	UPROPERTY(EditDefaultsOnly, Category = Content)
	ESMContentType ContentType;
	
	/** The order to display by default during Content selection. */
	UPROPERTY(EditDefaultsOnly, Category = Content)
	int32 SortPriority = 10;

	/** The name of the map which should be opened when the Content is installed. */
	UPROPERTY(EditDefaultsOnly, Category = Content)
	FString PrimaryMapName;

	/** Other content assets this one is dependent on. */
	UPROPERTY(EditDefaultsOnly, Category = Content)
	TArray<TSoftObjectPtr<USMContentAsset>> ContentAssetDependencies;

	/** The content should be installed to the plugin content folder only. */
	UPROPERTY(EditDefaultsOnly, Category = Content)
	bool bInstallToPluginContent = false;

public:
	// UObject
	virtual bool IsEditorOnly() const override final { return true; }
	// ~UObject
	
	/** The relative directory owning this file. */
	FString GetOwningDirectory() const;

	/** The full directory path to this asset. */
	FString GetDirectoryPath(bool bRelative = false) const;

	/** The full path to the staged files before installation. */
	FString GetStagedFilesPath(bool bRelative = false) const;

	/** Return the expected pak file path location */
	FString GetPackFilePath(bool bRelative = false) const;

	/**
	 * Retrieve the actual UAssets for this content; Only valid if staged.
	 * These are assets either extracted and ready to be migrated or are assets ready to be packaged into a pak file.
	 */
	TArray<FAssetData> GetStagedAssets() const;

	/**
	 * Checks if this asset is extracted to the plugin content folder and staged, potentially ready for migration.
	 */
	bool IsExtractedAndStaged() const;

	/** Set the input mapping context for the asset This will duplicate the input context. */
	void SetInputMappingContext(const UInputMappingContext* InInputMappingContext);

	/** Return the input action context embedded into the content asset. */
	const TMap<FName, FSMInputActionWrapper>& GetInputActionsMapping() const { return InputActionsMapping; }

	/** Return the files packaged with this asset. */
	const TArray<FString>& GetPackagedFiles() const { return PackagedFiles; }

	/** The single contained directory files should be staged to prior to install. Not a full path. */
	static FString GetStagedFilesDirectoryName();
	
	UFUNCTION(CallInEditor, Category = Package)
	void PackageAsset();

private:
	/** Input values detected during packaging. */
	UPROPERTY(VisibleDefaultsOnly, Category = Package)
	TMap<FName, FSMInputActionWrapper> InputActionsMapping;

	/** Read only view of all packaged files. */
	UPROPERTY(VisibleDefaultsOnly, Category = Package)
	TArray<FString> PackagedFiles;
};

