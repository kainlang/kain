// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"

class FSMContentEditorCommands : public TCommands<FSMContentEditorCommands>
{
public:
	/** Constructor */
	FSMContentEditorCommands();

	// TCommand
	virtual void RegisterCommands() override;
	FORCENOINLINE static const FSMContentEditorCommands& Get();
	// ~TCommand

	/** Add or install content to the project. */
	TSharedPtr<FUICommandInfo> InstallContent;
};
