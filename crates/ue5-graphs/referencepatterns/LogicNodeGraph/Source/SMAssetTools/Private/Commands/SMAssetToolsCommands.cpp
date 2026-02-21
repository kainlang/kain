// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMAssetToolsCommands.h"

#define LOCTEXT_NAMESPACE "SMAssetToolsCommands"

void FSMAssetToolsCommands::RegisterCommands()
{
	UI_COMMAND(ExportAsset, "Export", "Export a state machine asset to a supported format", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ImportAsset, "Import", "Import a state machine asset from a supported format", EUserInterfaceActionType::Button, FInputChord());
}

const FSMAssetToolsCommands& FSMAssetToolsCommands::Get()
{
	return TCommands<FSMAssetToolsCommands>::Get();
}

#undef LOCTEXT_NAMESPACE
