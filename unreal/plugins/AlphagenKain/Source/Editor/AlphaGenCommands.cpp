// Copyright 2026 K-Studio. All Rights Reserved.

#include "AlphaGenCommands.h"

#define LOCTEXT_NAMESPACE "FAlphaGenCommands"

void FAlphaGenCommands::RegisterCommands()
{
	UI_COMMAND(
		OpenAlphaGenWidget,
		"Open AlphaGen",
		"Open the AlphaGen procedural texture generation widget",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::A)
	);
	
	UI_COMMAND(
		QuickGenerateRadial,
		"Quick Generate: Radial",
		"Quickly generate a radial falloff alpha texture",
		EUserInterfaceActionType::Button,
		FInputChord()
	);
	
	UI_COMMAND(
		QuickGeneratePerlin,
		"Quick Generate: Perlin",
		"Quickly generate a perlin noise alpha texture",
		EUserInterfaceActionType::Button,
		FInputChord()
	);
}

#undef LOCTEXT_NAMESPACE
