// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Assets/SMContentAsset.h"

#include "Utilities/SMContentEditorUtils.h"

#include "ISMSystemModule.h"

#include "InputMappingContext.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

FString USMContentAsset::GetOwningDirectory() const
{
	const FString Result = GetPathName();
	
	FString PathPart; FString FilenamePart; FString ExtensionPart;
	FPaths::Split(Result, PathPart, FilenamePart, ExtensionPart);
	
	FString Directory = FPaths::GetPathLeaf(PathPart);
	
	return Directory;
}

FString USMContentAsset::GetDirectoryPath(bool bRelative) const
{
	const FString Directory = GetOwningDirectory();
	
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT(LD_PLUGIN_NAME));
	check(Plugin.IsValid());
	
	const FString ContentDir = bRelative ? Plugin->GetMountedAssetPath() : Plugin->GetContentDir();
	const FString FullPath = bInstallToPluginContent ?
		ContentDir / Directory :
		ContentDir / TEXT("Implementations") / Author.ToString() / Directory;

	return FullPath;
}

FString USMContentAsset::GetStagedFilesPath(bool bRelative) const
{
	return GetDirectoryPath(bRelative) / GetStagedFilesDirectoryName();
}

FString USMContentAsset::GetPackFilePath(bool bRelative) const
{
	return FPaths::SetExtension(GetDirectoryPath(bRelative) / GetName(), TEXT("pak"));
}

TArray<FAssetData> USMContentAsset::GetStagedAssets() const
{
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(FName("AssetRegistry"));
	const IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FString FullPath = GetDirectoryPath(true);
	FullPath.RemoveFromEnd(TEXT("/"));
	TArray<FAssetData> ContentAssets;

	TArray<FAssetData> OutAssets;
	AssetRegistry.GetAssetsByPath(*FullPath, OutAssets, true, true);

	return OutAssets;
}

bool USMContentAsset::IsExtractedAndStaged() const
{
	if (!ensureMsgf(PackagedFiles.Num() > 0, TEXT("No files packaged; nothing to install!")))
	{
		return false;
	}

	TArray<FString> FilesPackagedCopy = PackagedFiles;

	const TArray<FAssetData> StagedAssets = GetStagedAssets();
	if (StagedAssets.Num() == 0)
	{
		return false;
	}

	for (const FAssetData& Asset : StagedAssets)
	{
		FilesPackagedCopy.RemoveSingle(Asset.AssetName.ToString());
	}

	return FilesPackagedCopy.Num() == 0;
}

void USMContentAsset::SetInputMappingContext(const UInputMappingContext* InInputMappingContext)
{
	InputActionsMapping.Empty();
	if (InInputMappingContext)
	{
		const TArray<FEnhancedActionKeyMapping>& Mappings = const_cast<TArray<FEnhancedActionKeyMapping>&>(InInputMappingContext->GetMappings());

		for (const FEnhancedActionKeyMapping& Mapping : Mappings)
		{
			if (Mapping.Action)
			{
				FString ActionName = Mapping.Action->GetName();
				ActionName.RemoveFromStart(TEXT("IA_"));
				FSMInputActionWrapper& SupportedMappings = InputActionsMapping.FindOrAdd(*ActionName);
				SupportedMappings.Keys.Add(Mapping.Key);
			}
		}
	}
}

void USMContentAsset::PackageAsset()
{
	Modify();
	LD::ContentEditorPackageUtils::PackageContent(this, PackagedFiles);
}

FString USMContentAsset::GetStagedFilesDirectoryName()
{
	return TEXT("Files");
}
