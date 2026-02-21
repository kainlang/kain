// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

class FQuestEditorCommands : public TCommands<FQuestEditorCommands>
{
public:

	FQuestEditorCommands();

	//Show the quest details
	TSharedPtr<FUICommandInfo> ShowQuestDetails;

	//Open a chrome tab with the tutorials
	TSharedPtr<FUICommandInfo> ViewTutorial;

	//Quickly add a compatible node. If we're at a state, add an action. If we're at an action, add a state
	TSharedPtr<FUICommandInfo> QuickAddNode;

	virtual void RegisterCommands() override;

};
