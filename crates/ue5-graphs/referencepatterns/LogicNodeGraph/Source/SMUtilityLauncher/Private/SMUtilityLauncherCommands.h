// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"

class FSMUtilityLauncherCommands : public TCommands<FSMUtilityLauncherCommands>
{
public:
	/** Constructor */
	FSMUtilityLauncherCommands();

	// TCommand
	virtual void RegisterCommands() override;
	FORCENOINLINE static const FSMUtilityLauncherCommands& Get();
	// ~TCommand

	/** Open the documentation. */
	TSharedPtr<FUICommandInfo> OpenDocs;

	/** Open the Discord community. */
	TSharedPtr<FUICommandInfo> OpenDiscord;

	/** View system information for support. */
	TSharedPtr<FUICommandInfo> ViewSystemInfo;
	
	/** Create a new state machine class. */
	TSharedPtr<FUICommandInfo> CreateStateMachineClass;

	/** Create a new node class. */
	TSharedPtr<FUICommandInfo> CreateNodeClass;
};
