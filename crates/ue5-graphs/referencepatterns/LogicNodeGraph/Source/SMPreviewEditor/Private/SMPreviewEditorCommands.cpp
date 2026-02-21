// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMPreviewEditorCommands.h"

#define LOCTEXT_NAMESPACE "SMPreviewEditorCommands"

void FSMPreviewEditorCommands::RegisterCommands()
{
	UI_COMMAND(ResetCamera, "Reset Camera", "Resets the camera to focus on the scene", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ShowGrid, "Show Grid", "Toggles the grid", EUserInterfaceActionType::ToggleButton, FInputChord());
}

const FSMPreviewEditorCommands& FSMPreviewEditorCommands::Get()
{
	return TCommands<FSMPreviewEditorCommands>::Get();
}

#undef LOCTEXT_NAMESPACE
