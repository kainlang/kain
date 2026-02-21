// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMNewAssetOptions.h"

#include "AssetImporter/SMAssetImportDialog.h"

#include "Blueprints/SMBlueprintFactory.h"

#include "Widgets/SNullWidget.h"

#define LOCTEXT_NAMESPACE "SMNewAssetOptions"

static FDelegateHandle OnGetNewAssetDialogOptionsHandle;

void FSMNewAssetOptions::Initialize()
{
	OnGetNewAssetDialogOptionsHandle = USMBlueprintFactory::OnGetNewAssetDialogOptions().AddStatic(&FSMNewAssetOptions::OnGetNewAssetDialogOptions);
}

void FSMNewAssetOptions::Shutdown()
{
	USMBlueprintFactory::OnGetNewAssetDialogOptions().Remove(OnGetNewAssetDialogOptionsHandle);
}

void FSMNewAssetOptions::OnGetNewAssetDialogOptions(TArray<FSMNewAssetDialogOption>& OutOptions)
{
	FSMNewAssetDialogOption ImportOption(
	LOCTEXT("ImportStateMachineLabel", "Import State Machine (Experimental)"),
	LOCTEXT("ImportStateMachineDescription", "Create a new state machine asset and import data from an external file."),
	LOCTEXT("ImportSelectLabel", "Select a File to Import"), // Not used

	FSMNewAssetDialogOption::FOnCanContinue(),
	FSMNewAssetDialogOption::FOnCanContinue::CreateLambda([] { return false; }),
		FSMNewAssetDialogOption::FOnCanContinue::CreateLambda([&]()
		{
			return LD::AssetImportDialog::OpenAssetImportDialog();
		}),
	SNullWidget::NullWidget);

	OutOptions.Add(MoveTemp(ImportOption));
}

#undef LOCTEXT_NAMESPACE