//  Copyright Narrative Tools 2022.


#include "QuestNodeUserWidget.h"


void UQuestNodeUserWidget::InitializeFromNode(class UQuestNode* InNode, class UQuest* InQuest)
{
	if (InNode)
	{
		Node = InNode;
		Quest = InQuest;

		OnNodeInitialized(InNode, InQuest);
	}
}
