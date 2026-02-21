// Copyright Narrative Tools 2022. 

#include "Quest.h"
#include "NarrativeDataTask.h"
#include "QuestSM.h"
#include "QuestBlueprintGeneratedClass.h"
#include "NarrativeEvent.h"
#include "NarrativeComponent.h"
#include "NarrativeFunctionLibrary.h"
#include "NarrativePartyComponent.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

UQuest::UQuest()
{
	QuestName = FText::FromString("My New Quest");
	QuestDescription = FText::FromString("Enter a description for your quest here.");
	bTracked = true; 
}

UWorld* UQuest::GetWorld() const
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}

	if (OwningComp)
	{
		return OwningComp->GetWorld();
	}
	
	return nullptr;
}

void UQuest::DuplicateAndInitializeFromQuest(UQuest* QuestTemplate)
{
	if (QuestTemplate)
	{
		//Duplicate the quest template, then steal all its states and branches
		UQuest* NewQuest = Cast<UQuest>(StaticDuplicateObject(QuestTemplate, this, NAME_None, RF_Transactional));
		NewQuest->SetFlags(RF_Transient | RF_DuplicateTransient);
		
		if (NewQuest)
		{
			QuestStartState = NewQuest->QuestStartState;
			States = NewQuest->States;
			Branches = NewQuest->Branches;
		}
	}
}

bool UQuest::Initialize(class UNarrativeComponent* InitializingComp, const FName& QuestStartID /*= NAME_None*/)
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		//We need a valid narrative component to make a quest for 
		if (!InitializingComp)
		{
			return false;
		}

		if (UQuestBlueprintGeneratedClass* BGClass = Cast<UQuestBlueprintGeneratedClass>(GetClass()))
		{
			BGClass->InitializeQuest(this);

			//If a quest doesn't have any states or branches, or doesn't have a valid start state something has gone wrong and we should abort
			if (States.Num() == 0 || Branches.Num() == 0 || !QuestStartState)
			{
				return false;
			}

			//At this point, we should have a valid quest assigned to us. Check if we have a valid start state
			if (QuestStartState)
			{
				//Add the inheritable states to the states list 
				States.Append(InheritableStates);

				OwningComp = InitializingComp;

				OwningPawn = OwningComp->GetOwningPawn();
				OwningController = OwningComp->GetOwningController();

				for (auto& Node : GetNodes())
				{
					if (Node)
					{
						Node->OwningQuest = this;
					}
				}

				return true;
			}
		}
	}


	return false;
}

void UQuest::Deinitialize()
{
	//Deactivate every node in the quest, we don't want quest tasks updating any more 
	for (auto& Branch : Branches)
	{
		if (Branch)
		{
			Branch->Deactivate();
		}
	}

	for (auto& State : States)
	{
		if (State)
		{
			State->Deactivate();
		}
	}

	for (auto& QuestActor : QuestActors)
	{
		QuestActor->Destroy();
	}

	QuestActors.Empty();
}

void UQuest::BeginQuest(const FName& QuestStartID /** = NAME_None*/)
{
	BPPreQuestStarted(this);

	QuestCompletion = EQuestCompletion::QC_Started;
	EnterState_Internal(QuestStartID.IsNone() ? QuestStartState : GetState(QuestStartID));

	BPOnQuestStarted(this);

	if (OwningComp)
	{
		OwningComp->OnQuestStarted.Broadcast(this);
	}
}

void UQuest::TakeBranch(UQuestBranch* Branch)
{
	//We're taking a branch, deactivate it, fire off its bound function and events, and then head to the destination state
	if (Branch)
	{
		Branch->Deactivate();
	}

	OnQuestBranchCompleted(Branch);

	//Client can call this function in order to process delegates and things but server needs to be setting the state, not client 
	if (OwningComp)
	{
		EnterState_Internal(Branch->DestinationState);
	}
}

void UQuest::SetTracked(const bool bNewTracked)
{
	if (bNewTracked != bTracked)
	{
		bTracked = bNewTracked;
		
		BPOnTrackedChanged(this, bTracked);
	}
}

void UQuest::EnterState(UQuestState* NewState)
{
	if (NewState && States.Contains(NewState))
	{
		EnterState_Internal(NewState);

		//If we're explicitly setting the state instead of moving to a new state by completing tasks, clients need to be told to go also 
		if (OwningComp)
		{
			OwningComp->SendNarrativeUpdate(FNarrativeUpdate::QuestNewState(GetClass(), NewState->GetID()));
		}
	}
}

void UQuest::EnterState_Internal(UQuestState* NewState)
{
	if (NewState && OwningComp)
	{
		//Before we set our new state, deactivate the old one
		if (CurrentState)
		{
			CurrentState->Deactivate();
		}

		CurrentState = NewState;
		ReachedStates.Add(CurrentState);

		//Update the quests completion
		if (NewState->StateNodeType == EStateNodeType::Success)
		{
			SucceedQuest(CurrentState->Description);
		}
		else if (NewState->StateNodeType == EStateNodeType::Failure)
		{
			FailQuest(CurrentState->Description);
		}

		//We dont call delegate updates when loading, as delegates are typically just for UI updates and things 
		if (!OwningComp->bIsLoading)
		{
			//Fire delegates because we're about to activate the current state. This can actually cause another state change, which will cause delegates to fire in the wrong order. 
			BPOnQuestNewState(this, NewState);

			if (OwningComp)
			{
				OwningComp->OnQuestNewState.Broadcast(this, NewState);
			}

			QuestNewState.Broadcast(this, CurrentState);
		}

		//Finally, activate our new state, therefore activating its branches allowing us to take one to progress through the quest. 
		CurrentState->Activate();

	}
}

class UQuestState* UQuest::GetState(FName ID) const
{
	for (auto& State : States)
	{
		if (State->GetID() == ID)
		{
			return State;
		}
	}
	return nullptr;
}

class UQuestBranch* UQuest::GetBranch(FName ID) const
{
	for (auto& Branch : Branches)
	{
		if (Branch->GetID() == ID)
		{
			return Branch;
		}
	}
	return nullptr;
}

FText UQuest::GetQuestName() const
{
	return QuestName;
}

FText UQuest::GetQuestDescription() const
{
	return QuestDescription;
}

void UQuest::SetQuestName(const FText& NewName)
{
	QuestName = NewName;
}

void UQuest::SetQuestDescription(const FText& NewDescription)
{
	QuestDescription = NewDescription;
}

void UQuest::FailQuest(FText QuestFailedMessage)
{
	QuestCompletion = EQuestCompletion::QC_Failed;

	if (OwningComp && !OwningComp->bIsLoading)
	{
		BPOnQuestFailed(this, QuestFailedMessage);

		QuestFailed.Broadcast(this, QuestFailedMessage);

		OwningComp->OnQuestFailed.Broadcast(this, QuestFailedMessage);
	}

	Deinitialize();
}

void UQuest::SucceedQuest(FText QuestSucceededMessage)
{
	QuestCompletion = EQuestCompletion::QC_Succeded;

	if (OwningComp && !OwningComp->bIsLoading)
	{
		BPOnQuestSucceeded(this, QuestSucceededMessage);

		QuestSucceeded.Broadcast(this, QuestSucceededMessage);

		OwningComp->OnQuestSucceeded.Broadcast(this, QuestSucceededMessage);
	}

	Deinitialize();
}

void UQuest::OnQuestTaskProgressChanged(const UNarrativeTask* Task, const class UQuestBranch* Step, int32 CurrentProgress, int32 RequiredProgress)
{
	BPOnQuestTaskProgressChanged(this, Task, Step, CurrentProgress, RequiredProgress);

	QuestTaskProgressChanged.Broadcast(this, Task, Step, CurrentProgress, RequiredProgress);

	if (OwningComp)
	{
		OwningComp->OnQuestTaskProgressChanged.Broadcast(this, Task, Step, CurrentProgress, RequiredProgress);
	}
}

void UQuest::OnQuestTaskCompleted(const UNarrativeTask* Task, const class UQuestBranch* Branch)
{
	BPOnQuestTaskCompleted(this, Task, Branch);

	QuestTaskCompleted.Broadcast(this, Task, Branch);

	if (OwningComp)
	{
		OwningComp->OnQuestTaskCompleted.Broadcast(this, Task, Branch);
	}
}

void UQuest::OnQuestBranchCompleted(const class UQuestBranch* Step)
{
	QuestBranchCompleted.Broadcast(this, Step);

	BPOnQuestBranchCompleted(this, Step);

	if (OwningComp)
	{
		OwningComp->OnQuestBranchCompleted.Broadcast(this, Step);
	}
}

AActor* UQuest::SpawnQuestActor_Implementation(TSubclassOf<class AActor> ActorClass, const FTransform& ActorTransform)
{
	AActor* Actor = nullptr;

	if (GetWorld())
	{
		FActorSpawnParameters SpawnParams;

		SpawnParams.bNoFail = true;
		SpawnParams.Owner = GetOwningController();
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		Actor = GetWorld()->SpawnActor<AActor>(ActorClass, ActorTransform);
	}

	QuestActors.Add(Actor);

	return Actor;
}

void UQuest::AddState(class UQuestState* State)
{
	States.Add(State);
}

void UQuest::AddBranch(class UQuestBranch* Branch)
{
	Branches.Add(Branch);
}

void UQuest::RemoveState(class UQuestState* State)
{
	States.Remove(State);
}

void UQuest::RemoveBranch(class UQuestBranch* Branch)
{
	Branches.Remove(Branch);
}

void UQuest::SetQuestStartState(class UQuestState* State)
{
	QuestStartState = State;
}

TArray<UQuestNode*> UQuest::GetNodes() const
{
	TArray<UQuestNode*> Ret;

	for (auto& State : States)
	{
		Ret.Add(State);
	}

	for (auto& Branch : Branches)
	{
		Ret.Add(Branch);
	}

	return Ret;
}

TArray<class APlayerController*> UQuest::GetGroupMembers() const
{
	TArray<class APlayerController*> GroupMembers;

	if (UNarrativePartyComponent* PartyComp = Cast<UNarrativePartyComponent>(OwningComp))
	{
		for (auto& GroupMemberComp : PartyComp->GetPartyMembers())
		{
			if (GroupMemberComp)
			{
				GroupMembers.Add(GroupMemberComp->GetOwningController());
			}
		}
	}
	else
	{
		GroupMembers.Add(OwningController);
	}

	return GroupMembers;
}
