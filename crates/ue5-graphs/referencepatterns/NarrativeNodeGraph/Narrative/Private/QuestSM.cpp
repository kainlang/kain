// Copyright Narrative Tools 2022. 

#include "QuestSM.h"
#include "Quest.h"
#include "NarrativeEvent.h"
#include "NarrativeDataTask.h"
#include "NarrativeComponent.h"
#include "NarrativeQuestSettings.h"
#include "QuestTask.h"

#define LOCTEXT_NAMESPACE "StateMachine"

UQuestState::UQuestState()
{
	//Description = LOCTEXT("QuestStateDescription", "Write an update to appear in the players quest journal here.");
}

void UQuestState::Activate()
{
	//Once the state activates, activate all the branches it has 
	for (auto& Branch : Branches)
	{
		if (Branch)
		{
			Branch->Activate();
		}
	}

	UQuestNode::Activate();
}

void UQuestState::Deactivate()
{
	//Once the state deactivates, deactivate all the branches it has 
	for (auto& Branch : Branches)
	{
		if (Branch)
		{
			Branch->Deactivate();
		}
	}

	UQuestNode::Deactivate();
}

UQuestBranch::UQuestBranch()
{

}

void UQuestBranch::Activate()
{
	for (UNarrativeTask* QuestTask : QuestTasks)
	{
		if (QuestTask)
		{
			QuestTask->BeginTask();
		}
	}
	
	UQuestNode::Activate();
}

void UQuestBranch::Deactivate()
{
	for (UNarrativeTask* QuestTask : QuestTasks)
	{
		if (QuestTask)
		{
			QuestTask->EndTask();
		}
	}

	UQuestNode::Deactivate();
}

void UQuestBranch::OnQuestTaskComplete(class UNarrativeTask* UpdatedTask)
{
	if (OwningQuest)
	{
		if (AreTasksComplete())
		{
			OwningQuest->TakeBranch(this);
		}

		OwningQuest->OnQuestTaskCompleted(UpdatedTask, this);
	}
}

FText UQuestBranch::GetNodeTitle() const
{
	FString Title = "";
	int32 Idx = 0;

	Title += "Tasks: ";
	Title += LINE_TERMINATOR;

	for (UNarrativeTask* QuestTask : QuestTasks)
	{
		if (!QuestTask)
		{
			continue;
		}

		if (Idx > 0)
		{
			Title += LINE_TERMINATOR;
		}

		//For custom tasks, just display "Task"
		const FString TaskName = QuestTask->GetTaskNodeDescription().ToString();

		if (QuestTask->RequiredQuantity <= 1)
		{
			Title += FString::Printf(TEXT("%s: %s"), *TaskName);
		}
		else
		{
			Title += FString::Printf(TEXT("%s (0/%d)"), *TaskName, QuestTask->RequiredQuantity);
		}

		++Idx;
	}
	
	return FText::FromString(Title);
}

#if WITH_EDITOR

void UQuestBranch::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UQuestBranch, QuestTasks))
	{
		const int32 Idx = PropertyChangedEvent.GetArrayIndex(PropertyChangedEvent.GetPropertyName().ToString());

		if (QuestTasks.IsValidIndex(Idx))
		{
			
		}
	}
}

void UQuestBranch::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}

#endif

bool UQuestBranch::AreTasksComplete() const
{
	bool bCompletedAllTasks = true;

	for (auto& MyTask : QuestTasks)
	{
		if (MyTask && !MyTask->IsComplete())
		{
			bCompletedAllTasks = false;
			break;
		}
	}

	return bCompletedAllTasks;
}

FText UQuestState::GetNodeTitle() const
{
	if (ID == NAME_None)
	{
		return FText::GetEmpty();
	}

	FString ReadableString;
	ReadableString = FName::NameToDisplayString(ID.ToString(), false);
	return FText::FromString(ReadableString);
}


void UQuestNode::Activate()
{
	if (OwningQuest)
	{
		struct SOnEnteredStruct
		{
			UQuestNode* Node;
			bool bActivated;
		};

		SOnEnteredStruct Parms;
		Parms.Node = this;
		Parms.bActivated = true;

		if (UFunction* Func = OwningQuest->FindFunction(OnEnteredFuncName))
		{
			OwningQuest->ProcessEvent(Func, &Parms);
		}

		ProcessEvents(OwningQuest->GetOwningPawn(), OwningQuest->GetOwningController(), OwningQuest->GetOwningComp(), EEventRuntime::Start);
	}
}

void UQuestNode::Deactivate()
{
	if (OwningQuest)
	{
		struct SOnEnteredStruct
		{
			UQuestNode* Node;
			bool bActivated;
		};

		SOnEnteredStruct Parms;
		Parms.Node = this;
		Parms.bActivated = false;

		if (UFunction* Func = OwningQuest->FindFunction(OnEnteredFuncName))
		{
			OwningQuest->ProcessEvent(Func, &Parms);
		}

		ProcessEvents(OwningQuest->GetOwningPawn(), OwningQuest->GetOwningController(), OwningQuest->GetOwningComp(), EEventRuntime::End);
	}
}

void UQuestNode::EnsureUniqueID()
{
	if (UQuest* Quest = GetOwningQuest())
	{
		TArray<UQuestNode*> QuestNodes = Quest->GetNodes();
		TArray<FName> NodeIDs;

		for (auto& Node : QuestNodes)
		{
			if (Node != this)
			{
				NodeIDs.Add(Node->ID);
			}
		}

		int32 Suffix = 1;
		FName NewID = ID;

		if (!NodeIDs.Contains(NewID))
		{
			return;
		}

		// Check if the new ID already exists in the array
		while (NodeIDs.Contains(NewID))
		{
			// If it does, add a numeric suffix and try again
			NewID = FName(*FString::Printf(TEXT("%s%d"), *ID.ToString(), Suffix));
			Suffix++;
		}

		ID = NewID;
	}
}

UQuest* UQuestNode::GetOwningQuest() const
{
	//In the editor, outer will be the quest. At runtime, quest object is manually set
	return OwningQuest ? OwningQuest : Cast<UQuest>(GetOuter());
}

class UNarrativeComponent* UQuestNode::GetOwningNarrativeComp() const
{
	if (UQuest* Quest = GetOwningQuest())
	{
		return Quest->GetOwningComp();
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
