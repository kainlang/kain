// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMAssetExportDialog.h"

#include "ISMAssetToolsModule.h"
#include "AssetExporter/SMAssetExportManager.h"

#include "Blueprints/SMBlueprint.h"

#include "DesktopPlatformModule.h"
#include "EditorDirectories.h"
#include "IDesktopPlatform.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "SMAssetExportDialog"

bool LD::AssetExportDialog::OpenAssetExportDialog(USMBlueprint* InBlueprint)
{
	check(InBlueprint);

	const ISMAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<ISMAssetToolsModule>(
		LOGICDRIVER_ASSET_TOOLS_MODULE_NAME);

	const TArray<FString> SupportedExtensions = AssetToolsModule.GetAssetExporter()->GetSupportedExportTypes();

	const FString FileDescription = TEXT("Logic Driver Export");
	const FString JoinedExtensions = FString::Join(SupportedExtensions, TEXT(";*."));
	const FString FileTypes = FString::Printf(TEXT("%s (*.%s)|*.%s"), *FileDescription, *JoinedExtensions, *JoinedExtensions);

	TArray<FString> ExportFilenames;

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	const void* ParentWindowWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
	const bool bFileSelected = DesktopPlatform->SaveFileDialog(
		ParentWindowWindowHandle,
		LOCTEXT("ExportDialogTitle", "Export").ToString(),
		FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_EXPORT),
		InBlueprint->GetName(),
		FileTypes,
		EFileDialogFlags::None,
		ExportFilenames);

	if (bFileSelected && ExportFilenames.Num() > 0)
	{
		FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_EXPORT, FPaths::GetPath(ExportFilenames[0]));

		USMAssetExporter::FExportArgs ExportArgs;
		ExportArgs.Blueprint = InBlueprint;
		ExportArgs.ExportFullFilePath = ExportFilenames[0];
		AssetToolsModule.GetAssetExporter()->ExportAsset(MoveTemp(ExportArgs));
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
