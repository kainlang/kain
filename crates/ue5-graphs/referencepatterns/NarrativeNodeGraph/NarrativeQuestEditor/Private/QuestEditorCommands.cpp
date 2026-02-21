// Copyright Narrative Tools 2022. 

#include "QuestEditorCommands.h"
#include "QuestEditorStyle.h"

#define LOCTEXT_NAMESPACE "QuestEditorCommands"

FQuestEditorCommands::FQuestEditorCommands() 
	: TCommands<FQuestEditorCommands>("QuestEditor.Common", LOCTEXT("Common", "Common"), NAME_None, FQuestEditorStyle::GetStyleSetName())
{

}

void FQuestEditorCommands::RegisterCommands()
{
	UI_COMMAND(ShowQuestDetails, "Details", "Show the details for this Quest.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ViewTutorial, "Tutorial", "View a series of video tutorials on using Narrative.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(QuickAddNode, "Quick Add Node", "Quickly add a node from this node.", EUserInterfaceActionType::Button, FInputChord(EKeys::Q));
}

#undef LOCTEXT_NAMESPACE 