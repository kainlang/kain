// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMAssetImportDialog.h"

#include "ISMAssetToolsModule.h"
#include "AssetImporter/SMAssetImportManager.h"

#include "Blueprints/SMBlueprint.h"

#include "ContentBrowserModule.h"
#include "DesktopPlatformModule.h"
#include "EditorDirectories.h"
#include "IContentBrowserSingleton.h"
#include "IDesktopPlatform.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "SMAssetImportDialog"

bool LD::AssetImportDialog::OpenAssetImportDialog(USMBlueprint* InBlueprint)
{
	const ISMAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<ISMAssetToolsModule>(
	LOGICDRIVER_ASSET_TOOLS_MODULE_NAME);

	const TArray<FString> SupportedExtensions = AssetToolsModule.GetAssetImporter()->GetSupportedImportTypes();

	const FString FileDescription = TEXT("Logic Driver Import");
	const FString JoinedExtensions = FString::Join(SupportedExtensions, TEXT(";*."));
	const FString FileTypes = FString::Printf(TEXT("%s (*.%s)|*.%s"), *FileDescription, *JoinedExtensions, *JoinedExtensions);

	TArray<FString> ImportFilenames;

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	const void* ParentWindowWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
	const bool bFileSelected = DesktopPlatform->OpenFileDialog(
		ParentWindowWindowHandle,
		LOCTEXT("ImportDialogTitle", "Import").ToString(),
		FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_IMPORT),
		 InBlueprint ? InBlueprint->GetName() : FString(),
		FileTypes,
		EFileDialogFlags::None,
		ImportFilenames);

	if (bFileSelected && ImportFilenames.Num() > 0)
	{
		FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_IMPORT, FPaths::GetPath(ImportFilenames[0]));

		USMAssetImporter::FImportArgs Args;
		Args.ImportFullFilePath = ImportFilenames[0];
		Args.ImportToBlueprint = InBlueprint; // Blueprint always known when loading from import menu.
		if (InBlueprint == nullptr)
		{
			// Attempt to use the current directory.
			const FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			Args.SaveToContentPath = ContentBrowserModule.Get().GetCurrentPath().GetInternalPathString();
		}

		AssetToolsModule.GetAssetImporter()->ImportAsset(MoveTemp(Args));
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
