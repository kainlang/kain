// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMAssetToolbar.h"

#include "ISMAssetToolsModule.h"
#include "AssetExporter/SMAssetExportDialog.h"
#include "AssetImporter/SMAssetImportDialog.h"
#include "Commands/SMAssetToolsCommands.h"

#include "ISMSystemEditorModule.h"

#include "Blueprints/SMBlueprint.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Toolkits/AssetEditorToolkit.h"

#define LOCTEXT_NAMESPACE "SMAssetToolbar"

FDelegateHandle FSMAssetToolbar::ExtenderHandle;

void FSMAssetToolbar::Initialize()
{
	const ISMSystemEditorModule& SMBlueprintEditorModule = FModuleManager::LoadModuleChecked<ISMSystemEditorModule>(LOGICDRIVER_EDITOR_MODULE_NAME);

	const auto Delegate = SMBlueprintEditorModule.GetMenuExtensibilityManager()->GetExtenderDelegates().Add_GetRef(
		FAssetEditorExtender::CreateStatic(&HandleMenuExtensibilityGetExtender));
	ExtenderHandle = Delegate.GetHandle();
}

void FSMAssetToolbar::Shutdown()
{
	const ISMSystemEditorModule& SMBlueprintEditorModule = FModuleManager::GetModuleChecked<ISMSystemEditorModule>(LOGICDRIVER_EDITOR_MODULE_NAME);
	SMBlueprintEditorModule.GetMenuExtensibilityManager()->GetExtenderDelegates().RemoveAll([&](const FAssetEditorExtender& Extender)
	{
		return ExtenderHandle == Extender.GetHandle();
	});
}

void FSMAssetToolbar::ConstructExportMenu(FMenuBuilder& InMenuBuilder, const TArray<UObject*> ContextSensitiveObjects)
{
	const FSMAssetToolsCommands& Commands = FSMAssetToolsCommands::Get();

	InMenuBuilder.BeginSection("LogicDriverImportAndExport", LOCTEXT("ImportAndExport", "Import and Export (Experimental)"));
	{
		InMenuBuilder.AddMenuEntry(Commands.ImportAsset);
		InMenuBuilder.AddMenuEntry(Commands.ExportAsset);
	}
	InMenuBuilder.EndSection();
}

TSharedRef<FExtender> FSMAssetToolbar::HandleMenuExtensibilityGetExtender(const TSharedRef<FUICommandList> CommandList,
	const TArray<UObject*> ContextSensitiveObjects)
{
	const TSharedRef<FUICommandList> MenuItemCommandList = MakeShareable(new FUICommandList);

	MenuItemCommandList->MapAction(
		FSMAssetToolsCommands::Get().ExportAsset,
		FExecuteAction::CreateStatic(&OnAssetExport, ContextSensitiveObjects)
	);

	MenuItemCommandList->MapAction(
		FSMAssetToolsCommands::Get().ImportAsset,
		FExecuteAction::CreateStatic(&OnAssetImport, ContextSensitiveObjects)
	);

	const TSharedPtr<FExtender> MenuExtender = MakeShared<FExtender>();
	MenuExtender->AddMenuExtension("FileBlueprint", EExtensionHook::After, MenuItemCommandList,
	                               FMenuExtensionDelegate::CreateStatic(&ConstructExportMenu, ContextSensitiveObjects));

	return MenuExtender.ToSharedRef();
}

void FSMAssetToolbar::OnAssetExport(const TArray<UObject*> ContextSensitiveObjects)
{
	if (!ensure(ContextSensitiveObjects.Num() == 1))
	{
		return;
	}

	if (USMBlueprint* Blueprint = Cast<USMBlueprint>(ContextSensitiveObjects[0]))
	{
		LD::AssetExportDialog::OpenAssetExportDialog(Blueprint);
	}
}

void FSMAssetToolbar::OnAssetImport(const TArray<UObject*> ContextSensitiveObjects)
{
	if (!ensure(ContextSensitiveObjects.Num() == 1))
	{
		return;
	}

	if (USMBlueprint* Blueprint = Cast<USMBlueprint>(ContextSensitiveObjects[0]))
	{
		LD::AssetImportDialog::OpenAssetImportDialog(Blueprint);
	}
}

#undef LOCTEXT_NAMESPACE
