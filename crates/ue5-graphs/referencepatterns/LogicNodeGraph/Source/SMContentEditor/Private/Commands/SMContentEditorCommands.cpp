// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMContentEditorCommands.h"

#include "SMUnrealTypeDefs.h"

#define LOCTEXT_NAMESPACE "SMContentEditorCommands"

FSMContentEditorCommands::FSMContentEditorCommands(): TCommands<FSMContentEditorCommands>(TEXT("SMContentEditor"), NSLOCTEXT("Contexts", "SMContentEditor", "Logic Driver Content"),
	NAME_None, FSMUnrealAppStyle::Get().GetStyleSetName())
{
}

void FSMContentEditorCommands::RegisterCommands()
{
	UI_COMMAND(InstallContent, "Content Samples", "View available Logic Driver content samples which can be installed to the project.", EUserInterfaceActionType::Button, FInputChord());
}

const FSMContentEditorCommands& FSMContentEditorCommands::Get()
{
	return TCommands<FSMContentEditorCommands>::Get();
}

#undef LOCTEXT_NAMESPACE
