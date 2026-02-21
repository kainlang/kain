// Copyright Narrative Tools 2022. 

#include "DialogueEditorCommands.h"
#include "DialogueEditorStyle.h"

#define LOCTEXT_NAMESPACE "DialogueEditorCommands"

FDialogueEditorCommands::FDialogueEditorCommands() 
	: TCommands<FDialogueEditorCommands>("DialogueEditor.Common", LOCTEXT("Common", "Common"), NAME_None, FDialogueEditorStyle::GetStyleSetName())
{

}

void FDialogueEditorCommands::RegisterCommands()
{
	UI_COMMAND(ShowDialogueDetails, "Details", "Show the details for this Dialogue.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ViewTutorial, "Tutorial", "View a series of video tutorials on using Narrative.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(QuickAddNode, "Quick Add Node", "Quickly add a node from this node.", EUserInterfaceActionType::Button, FInputChord(EKeys::Q));
}

#undef LOCTEXT_NAMESPACE 