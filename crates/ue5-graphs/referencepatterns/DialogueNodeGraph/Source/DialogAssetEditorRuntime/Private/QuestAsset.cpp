/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#include "QuestAsset.h"
#include "DialogueMasterComponent.h"
#include "QuestNodeInfo.h"
#include "DialogueMasterTask.h"
#include "QuestState.h"
#include "UObject/ObjectSaveContext.h"
#include "GameFramework/Pawn.h"


void UQuestAsset::PreSave(FObjectPreSaveContext saveContext) { 
	Super::PreSave(saveContext);
	if (_onPreSaveListener) {
		_onPreSaveListener();
	}
}

void UQuestAsset::OnTaskProgressUpdated(UDialogueMasterTask* DialogueMasterTask, int PreviousValue, int NewValue)
{
	//QuestTaskProgressUpdated.Broadcast(this, GetCurrentStepInfo(), DialogueMasterTask, PreviousValue, NewValue);
	_OwningComponent->OnQuestTaskProgressUpdated.Broadcast(this, GetCurrentStepInfo(), DialogueMasterTask, PreviousValue, NewValue);
}

void UQuestAsset::CheckStepTransition()
{
	// Check if we go to next step (all mandatory tasks completed on a task list) :
	TArray<UQuestTaskListNodeInfo*> taskLists = GetCurrentStepTasksList();
	for(UQuestTaskListNodeInfo* taskList : taskLists)
	{
		bool allCompleted = true;
		for(UDialogueMasterTask * task : taskList->Tasks)
		{
			if(!task->IsCompleted())
			{
				allCompleted = false;
			}
		}

		// If all tasks are completed, we go to the next step :
		if(allCompleted)
		{
			FName NodeId = taskList->GetID();
			UQuestRuntimeNode * nextStepNode = GetStepAfterTaskList(NodeId);
			if(IsValid(nextStepNode))
			{
				OnExitStep(GetStepInfo(_CurrentStep));
				_CurrentStep = nextStepNode->NodeInfo->GetID();
				OnEnteringStep(Cast<UQuestStepNodeInfo>(nextStepNode->NodeInfo));
				return;
			}
		}
	}
}

void UQuestAsset::OnQuestEnd(EStepType EndType)
{
	_OwningComponent->OnQuestEnd.Broadcast(this, EndType);
}

UQuestRuntimeNode* UQuestAsset::GetStepAfterTaskList(FName taskListID)
{
	UQuestRuntimeNode * taskListNode = GetTaskListNode(taskListID);
	if(IsValid(taskListNode))
	{
		UQuestRuntimePin * outputPin = taskListNode->OutputPins[0];
		if(IsValid(outputPin)
				&&
				outputPin->Connection != nullptr
				&&
				outputPin->Connection->Parent != nullptr
				&&
				outputPin->Connection->Parent->NodeType == EQuestNodeType::QuestStepNode
				&&
				outputPin->Connection->Parent->NodeInfo != nullptr)
		{
			return outputPin->Connection->Parent;
		}
	}

	return nullptr;
}

void UQuestAsset::OnTaskCompleted(UDialogueMasterTask* DialogueMasterTask)
{
	// Notify that a task is completed :
	//QuestTaskCompleted.Broadcast(this, GetCurrentStepInfo(), DialogueMasterTask);
	_OwningComponent->OnQuestTaskCompleted.Broadcast(this, GetCurrentStepInfo(), DialogueMasterTask);

	CheckStepTransition();
}

bool UQuestAsset::StepExists(FName Name) const
{
	for(UQuestRuntimeNode * node : Graph->Nodes)
	{
		if(IsValid(node)
			&&
			node->NodeType == EQuestNodeType::QuestStepNode
			&&
			node->NodeInfo->GetID() == Name)
		{
			return true;
		}
	}

	return false;
}

UQuestRuntimeNode* UQuestAsset::GetCurrentStepNode() const
{
	return GetStepNode(_CurrentStep);
}

UQuestRuntimeNode* UQuestAsset::GetNode(FName NodeID, EQuestNodeType NodeType) const
{
	if(NodeID != NAME_None)
	{
		for(UQuestRuntimeNode * node : Graph->Nodes)
		{
			if(node->NodeType == NodeType && node->NodeInfo->GetID() == NodeID)
			{
				return node;
			}
		}
	}

	return nullptr;
}

UQuestRuntimeNode* UQuestAsset::GetStepNode(FName StepName) const
{
	return GetNode(StepName, EQuestNodeType::QuestStepNode);
}

UQuestRuntimeNode* UQuestAsset::GetTaskListNode(FName TaskListName) const
{
	return GetNode(TaskListName, EQuestNodeType::QuestTaskListNode);
}

bool UQuestAsset::Initialize(UDialogueMasterComponent * OwningComponent, APlayerController * OwningPlayerController, FName StartStep)
{
    this->_OwningComponent = OwningComponent;
	this->_OwningPlayerController = OwningPlayerController;
    bool initToStartStep = StartStep == NAME_None;

    if(initToStartStep)
    {
    	for(UQuestRuntimeNode * node : Graph->Nodes)
    	{
    		if(node->NodeType == EQuestNodeType::QuestStartNode)
    		{
    			UQuestRuntimePin * outputPin = node->OutputPins[0];
    			if(outputPin->Connection != nullptr)
    			{
    				_CurrentStep = outputPin->Connection->Parent->NodeInfo->GetID();
    				if(_CurrentStep != NAME_None)
    				{
						OnEnteringStep(GetCurrentStepInfo());
    					return true;
    				}
    			}
    		}
    	}
    }
    else
    {
	    if(StepExists(StartStep))
	    {
	    	_CurrentStep = StartStep;
	    	OnEnteringStep(GetCurrentStepInfo(), true);
		    return true;
	    }
    }

	return false;
}

void UQuestAsset::RestoreFromState(FQuestState* state, UDialogueMasterComponent * OwningComponent, APlayerController * OwningPlayerController)
{
	// Restore tasks state :
	for(UQuestRuntimeNode * node : Graph->Nodes)
	{
		if(node->NodeType == EQuestNodeType::QuestTaskListNode)
		{
			UQuestTaskListNodeInfo * nodeInfo = Cast<UQuestTaskListNodeInfo>(node->NodeInfo);
			if(state->TaskStates.Contains(nodeInfo->GetID()))
			{
				TMap<UDialogueMasterTask*, int> taskStateMap = state->TaskStates[nodeInfo->GetID()].TaskStatesMap;
				for(UDialogueMasterTask * task : nodeInfo->Tasks)
				{
					if(taskStateMap.Contains(task))
					{
						int value = taskStateMap[task];
						task->CurrentNumberOfCompletionDone = value;
					}
				}
			}
		}
	}
	
	Initialize(OwningComponent, OwningPlayerController, state->CurrentStep);
}

UQuestStepNodeInfo* UQuestAsset::GetCurrentStepInfo() const
{
	return GetStepInfo(_CurrentStep);
}

TArray<UQuestTaskListNodeInfo*> UQuestAsset::GetCurrentStepTasksList() const
{
	return GetStepTasksList(_CurrentStep);
}

UQuestStepNodeInfo* UQuestAsset::GetStepInfo(FName StepName) const
{
	if(UQuestRuntimeNode * node = GetStepNode(StepName))
		return Cast<UQuestStepNodeInfo>(node->NodeInfo);
	
	return nullptr;
}

TArray<UQuestTaskListNodeInfo*> UQuestAsset::GetStepTasksList(FName StepName) const
{
	TArray<UQuestTaskListNodeInfo*> result;

	if(UQuestRuntimeNode * node = GetCurrentStepNode())
	{
		for(UQuestRuntimePin * outputPin : node->OutputPins)
		{
			if(IsValid(outputPin)
				&&
				outputPin->Connection != nullptr
				&&
				outputPin->Connection->Parent != nullptr
				&&
				outputPin->Connection->Parent->NodeType == EQuestNodeType::QuestTaskListNode
				&&
				outputPin->Connection->Parent->NodeInfo != nullptr)
			{
				result.Add(Cast<UQuestTaskListNodeInfo>(outputPin->Connection->Parent->NodeInfo));
			}
		}
	}

	return result;
}

FName UQuestAsset::GetCurrentStepID() const
{
	return _CurrentStep;
}

bool UQuestAsset::IsQuestSucceeded() const
{
	UQuestStepNodeInfo* info = GetCurrentStepInfo();
	if(IsValid(info))
		return info->StepType == QUEST_SUCCEEDED;

	return false;
}

bool UQuestAsset::IsQuestFailed() const
{
	UQuestStepNodeInfo* info = GetCurrentStepInfo();
	if(IsValid(info))
		return info->StepType == QUEST_FAILED;

	return false;
}

bool UQuestAsset::IsQuestFinished() const
{
	UQuestStepNodeInfo* info = GetCurrentStepInfo();
	if(IsValid(info))
		return info->StepType != NORMAL;

	return false;
}

EStepType UQuestAsset::GetCurrentStepType() const
{
	UQuestStepNodeInfo* info = GetCurrentStepInfo();
	if(IsValid(info))
		return info->StepType;

	return NORMAL;
}

void UQuestAsset::OnEnteringStep(UQuestStepNodeInfo* step, bool bRestoreState)
{
	if(step != nullptr)
	{
		TArray<UQuestTaskListNodeInfo*> taskLists = GetStepTasksList(step->ID);
		// Reset the runtime value :
		for(UQuestTaskListNodeInfo * taskList : taskLists)
		{
			for(UDialogueMasterTask * task : taskList->Tasks)
			{
				if(!bRestoreState)
					task->PreInitializeTask();
			}
		}
		
		// Notify entering step : (it will update the Quest UI)
		//QuestNewStep.Broadcast(this, step);
		_OwningComponent->OnQuestEnteredNewStep.Broadcast(this, step);

		for(UDialogueMasterAction * action : step->Actions)
		{
			action->Initialize(nullptr,
				Cast<AActor>(UGameplayStatics::GetPlayerController(_OwningComponent, 0)->GetPawn()),
				_OwningComponent->GetWorld(),
				nullptr);

			if(!bRestoreState)
			{
				if(action->TriggerMoment == Beginning
					||
					action->TriggerMoment == Both)
				{
					action->Trigger(_OwningComponent, Beginning);
				}
			}
		}
		
		// Enable the tasks :
		for(UQuestTaskListNodeInfo * taskList : taskLists)
		{
			for(UDialogueMasterTask * task : taskList->Tasks)
			{
				if(!bRestoreState)
					task->BeginTask(this, _OwningComponent, _OwningPlayerController);
				else
					task->RestoreTask(this, _OwningComponent, _OwningPlayerController);
			}
		}
		
		CheckStepTransition();

		if(step->StepType == QUEST_SUCCEEDED || step->StepType == QUEST_FAILED)
		{
			if(!bRestoreState)
				OnQuestEnd(step->StepType);
		}
	}
}

void UQuestAsset::OnExitStep(UQuestStepNodeInfo* exitedStep)
{
	if(exitedStep != nullptr)
	{
		TArray<UQuestTaskListNodeInfo*> taskLists = GetStepTasksList(exitedStep->ID);
		for(UQuestTaskListNodeInfo * taskList : taskLists)
		{
			for(UDialogueMasterTask * task : taskList->Tasks)
			{
				task->EndTask();
			}
		}

		for(UDialogueMasterAction * action : exitedStep->Actions)
		{
			action->Initialize(nullptr,
				UGameplayStatics::GetPlayerController(_OwningComponent, 0)->GetPawn(),
				_OwningComponent->GetWorld(),
				nullptr);
			
			if(action->TriggerMoment == End
				||
				action->TriggerMoment == Both)
			{
				action->Trigger(_OwningComponent, End);
			}
		}
		
		_OwningComponent->OnQuestExitStep.Broadcast(this, exitedStep);
	}
}
