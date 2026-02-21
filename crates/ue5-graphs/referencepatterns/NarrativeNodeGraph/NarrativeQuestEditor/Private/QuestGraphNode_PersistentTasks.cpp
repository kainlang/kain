// Copyright Narrative Tools 2022. 

#include "QuestGraphNode_PersistentTasks.h"
#include "QuestEditorTypes.h"
#include "QuestEditorSettings.h"
#include "QuestSM.h"

void UQuestGraphNode_PersistentTasks::AllocateDefaultPins()
{
	CreatePin(EGPD_Output, UQuestEditorTypes::PinCategory_SingleNode, TEXT(""));
}

bool UQuestGraphNode_PersistentTasks::CanUserDeleteNode() const
{
	return false;
}

FLinearColor UQuestGraphNode_PersistentTasks::GetGraphNodeColor() const
{
	return GetDefault<UQuestEditorSettings>()->PersistentTasksNodeColor;
}
