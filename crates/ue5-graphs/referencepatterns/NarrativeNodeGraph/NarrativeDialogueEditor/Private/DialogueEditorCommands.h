// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

class FDialogueEditorCommands : public TCommands<FDialogueEditorCommands>
{
public:

	FDialogueEditorCommands();

	//Show the quest details
	TSharedPtr<FUICommandInfo> ShowDialogueDetails;

	//Open a chrome tab with the tutorials
	TSharedPtr<FUICommandInfo> ViewTutorial;

	//Quickly add a compatible node. If we're at a state, add an action. If we're at an action, add a state
	TSharedPtr<FUICommandInfo> QuickAddNode;

	virtual void RegisterCommands() override;

};
