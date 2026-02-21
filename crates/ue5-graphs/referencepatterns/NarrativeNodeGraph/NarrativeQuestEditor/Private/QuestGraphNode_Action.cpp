// Copyright Narrative Tools 2022. 

#include "QuestGraphNode_Action.h"
#include "QuestEditorTypes.h"
#include "QuestEditorSettings.h"
#include "QuestSM.h"
#include "Quest.h"

void UQuestGraphNode_Action::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UQuestEditorTypes::PinCategory_SingleNode, TEXT(""));
	CreatePin(EGPD_Output, UQuestEditorTypes::PinCategory_SingleNode, TEXT(""));
}

void UQuestGraphNode_Action::DestroyNode()
{
	Super::DestroyNode();

	//When the node is destroyed make sure it gets removed from the quest
	QuestNode->GetOwningQuest()->RemoveBranch(Branch);

}

FLinearColor UQuestGraphNode_Action::GetGraphNodeColor() const
{
	return GetDefault<UQuestEditorSettings>()->TaskNodeColor;
}
