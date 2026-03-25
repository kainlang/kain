#include "QuestState.h"

#include "DialogueMasterTask.h"
#include "QuestAsset.h"
#include "QuestNodeInfo.h"

FQuestState::FQuestState(UQuestAsset* quest)
{
	this->CurrentStep = quest->_CurrentStep;

	for(UQuestRuntimeNode * node : quest->Graph->Nodes)
	{
		if(node->NodeType == EQuestNodeType::QuestTaskListNode)
		{
			UQuestTaskListNodeInfo * nodeInfo = Cast<UQuestTaskListNodeInfo>(node->NodeInfo);
			TMap<UDialogueMasterTask*, int>& taskStateMap = this->TaskStates.Add(nodeInfo->GetID()).TaskStatesMap;
			for(UDialogueMasterTask * task : nodeInfo->Tasks)
			{
				taskStateMap.Add(task, task->CurrentNumberOfCompletionDone);
			}
		}
	}
}

FQuestState::FQuestState()
{
}
