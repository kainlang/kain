// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Framework/Commands/UICommandList.h"
#include "Framework/Commands/Commands.h"
#include "SMUnrealTypeDefs.h"

class ISMStateMachineBlueprintEditor;

class FSMExtendedEditorCommands : public TCommands<FSMExtendedEditorCommands>
{
public:
	/** Constructor */
	FSMExtendedEditorCommands()
		: TCommands<FSMExtendedEditorCommands>(TEXT("SMExtendedEditor"), NSLOCTEXT("Contexts", "SMExtendedEditor", "State Machine Editor"),
			NAME_None, FSMUnrealAppStyle::Get().GetStyleSetName())
	{
	}

	// TCommand
	virtual void RegisterCommands() override;
	FORCENOINLINE static const FSMExtendedEditorCommands& Get();
	// ~TCommand

	static void OnEditorCommandsCreated(ISMStateMachineBlueprintEditor* Editor, TSharedPtr<FUICommandList> CommandList);

	/** Use the node to edit. */
	TSharedPtr<FUICommandInfo> StartTextPropertyEdit;
	static void EditText(ISMStateMachineBlueprintEditor* Editor);
	static bool CanEditText(ISMStateMachineBlueprintEditor* Editor);
	
};
