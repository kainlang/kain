// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class USMInstalledContentAsset;
class USMContentAsset;
class USMContentProjectConfigurationViewModel;
class UInputMappingContext;

/**
 * Utils to assist with saving and loading content packages.
 */
namespace LD::ContentEditorPackageUtils
{
	/** Package a content asset for distribution in the content folder. */
	void PackageContent(USMContentAsset* InAsset, TArray<FString>& OutFilesProcessed);

	/** Extract the pak file to UAssets. */
	void ExtractPackage(const USMContentAsset* InAsset);

	/** Remove left over files; Only needed if a step failed. Not perfect and could fail. */
	void CleanupExtractedPackage(const USMContentAsset* InAsset);

	/** Perform the full installation process on a content asset, extracting the pak and migrating files. */
	bool InstallContent(const USMContentAsset* InAsset, const FString& InTargetPath,
		const USMContentProjectConfigurationViewModel* InProjectConfigViewModel = nullptr, bool bOpenMapOnInstall = false);

	/** Return the default installed content asset name. */
	const FString& GetInstalledContentAssetName();

	/** Find and update or create a new installed content asset. The PAK file should already have been extracted. */
	USMInstalledContentAsset* UpdateOrCreateInstalledContentAsset(const USMContentAsset* InContentAsset);

	/**
	 * Return the installed content asset if it exists.
	 * @param InDestinationPath The directory path to look for the asset. For plugin content it should be relative.
	 */
	USMInstalledContentAsset* FindInstalledContentAsset(const FString& InDestinationPath);

	/**
	 * Compares the hash of the installed content asset to the pak file.
	 * @return true if the hash matches.
	 */
	bool IsInstalledContentUpToDate(const USMContentAsset* InContentAsset);
}