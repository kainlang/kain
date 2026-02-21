// Copyright Narrative Tools 2022. 

#include "QuestGraphNode_Success.h"
#include "QuestEditorSettings.h"
#include "QuestEditorTypes.h"

void UQuestGraphNode_Success::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UQuestEditorTypes::PinCategory_SingleNode, TEXT(""));
}

FLinearColor UQuestGraphNode_Success::GetGraphNodeColor() const
{
	return GetDefault<UQuestEditorSettings>()->SuccessNodeColor;
}
