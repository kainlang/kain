// Copyright Narrative Tools 2022. 

#include "QuestGraphNode_Root.h"
#include "QuestEditorTypes.h"
#include "QuestEditorSettings.h"

void UQuestGraphNode_Root::AllocateDefaultPins()
{
	CreatePin(EGPD_Output, UQuestEditorTypes::PinCategory_SingleNode, TEXT(""));
}

bool UQuestGraphNode_Root::CanUserDeleteNode() const
{
	return false;
}

FLinearColor UQuestGraphNode_Root::GetGraphNodeColor() const
{
	return GetDefault<UQuestEditorSettings>()->RootNodeColor;
}
