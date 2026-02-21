// Copyright Narrative Tools 2022. 

#include "QuestGraphNode_Failure.h"
#include "QuestEditorSettings.h"
#include "QuestEditorTypes.h"

void UQuestGraphNode_Failure::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UQuestEditorTypes::PinCategory_SingleNode, TEXT(""));
}

FLinearColor UQuestGraphNode_Failure::GetGraphNodeColor() const
{
	return GetDefault<UQuestEditorSettings>()->FailedNodeColor;
}
