// Copyright Narrative Tools 2022. 

#include "QuestGraphNode_State.h"
#include "QuestEditorTypes.h"
#include "QuestEditorSettings.h"
#include "QuestSM.h"
#include "Quest.h"

void UQuestGraphNode_State::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UQuestEditorTypes::PinCategory_SingleNode, TEXT(""));
	CreatePin(EGPD_Output, UQuestEditorTypes::PinCategory_SingleNode, TEXT(""));
}

void UQuestGraphNode_State::DestroyNode()
{
	Super::DestroyNode();

	check(QuestNode->GetOwningQuest());

	//When the node is destroyed make sure it gets removed from the quest
	QuestNode->GetOwningQuest()->RemoveState(State);
}

FLinearColor UQuestGraphNode_State::GetGraphNodeColor() const
{
	return GetDefault<UQuestEditorSettings>()->StateNodeColor;
}

