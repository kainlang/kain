// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMUtilityLauncherCommands.h"

#include "SMUnrealTypeDefs.h"

#define LOCTEXT_NAMESPACE "SMUtilityLauncherCommands"

FSMUtilityLauncherCommands::FSMUtilityLauncherCommands(): TCommands<FSMUtilityLauncherCommands>(TEXT("SMUtilityLauncher"), NSLOCTEXT("Contexts", "SMUtilityLauncher", "Logic Driver Tools"),
	NAME_None, FSMUnrealAppStyle::Get().GetStyleSetName())
{
}

void FSMUtilityLauncherCommands::RegisterCommands()
{
	UI_COMMAND(OpenDocs, "Documentation", "View the Logic Driver documentation site.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(OpenDiscord, "Discord", "Access the Logic Driver Discord community.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ViewSystemInfo, "Generate System Info...", "Generate and view system information to assist with bug reports.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(CreateStateMachineClass, "New State Machine Class...", "Create a new State Machine Blueprint Class.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(CreateNodeClass, "New Node Class...", "Create a new Node Blueprint Class.", EUserInterfaceActionType::Button, FInputChord());
}

const FSMUtilityLauncherCommands& FSMUtilityLauncherCommands::Get()
{
	return TCommands<FSMUtilityLauncherCommands>::Get();
}

#undef LOCTEXT_NAMESPACE
