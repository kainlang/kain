// Copyright 2026 K-Studio. All Rights Reserved.

#include "KClonerEditorCommands.h"

#define LOCTEXT_NAMESPACE "KClonerEditorCommands"

void FKClonerEditorCommands::RegisterCommands()
{
	UI_COMMAND(BakeAnim, "Bake to Anim", "Bake the current simulation to a Skeletal Mesh and Animation Sequence", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
