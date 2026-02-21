// Copyright Narrative Tools 2022. 

#include "NarrativeComponent.h"
#include "NarrativePartyComponent.h"
#include "NarrativeSaveGame.h"
#include "NarrativeFunctionLibrary.h"
#include "Quest.h"
#include "NarrativeDataTask.h"
#include "QuestSM.h"
#include "DialogueSM.h"
#include "Dialogue.h"
#include "NarrativeCondition.h"
#include "NarrativeEvent.h"
#include "NarrativeDialogueSettings.h"
#include "QuestTask.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Engine/ActorChannel.h"

DEFINE_LOG_CATEGORY(LogNarrative);

// Sets default values for this component's properties
UNarrativeComponent::UNarrativeComponent()
{
	SetIsReplicatedByDefault(true);
	SetComponentTickEnabled(true);
	PrimaryComponentTick.bCanEverTick = true;

	OnNarrativeDataTaskCompleted.AddDynamic(this, &UNarrativeComponent::NarrativeDataTaskCompleted);
	OnQuestStarted.AddDynamic(this, &UNarrativeComponent::QuestStarted);
	OnQuestFailed.AddDynamic(this, &UNarrativeComponent::QuestFailed);
	OnQuestSucceeded.AddDynamic(this, &UNarrativeComponent::QuestSucceeded);
	OnQuestForgotten.AddDynamic(this, &UNarrativeComponent::QuestForgotten);
	OnQuestBranchCompleted.AddDynamic(this, &UNarrativeComponent::QuestBranchCompleted);
	OnQuestNewState.AddDynamic(this, &UNarrativeComponent::QuestNewState);
	OnQuestTaskProgressChanged.AddDynamic(this, &UNarrativeComponent::QuestTaskProgressMade);
	OnBeginSave.AddDynamic(this, &UNarrativeComponent::BeginSave);
	OnBeginLoad.AddDynamic(this, &UNarrativeComponent::BeginLoad);
	OnSaveComplete.AddDynamic(this, &UNarrativeComponent::SaveComplete);
	OnLoadComplete.AddDynamic(this, &UNarrativeComponent::LoadComplete);

	OnDialogueBegan.AddDynamic(this, &UNarrativeComponent::DialogueBegan);
	OnDialogueFinished.AddDynamic(this, &UNarrativeComponent::DialogueFinished);

	bIsLoading = false;
}


void UNarrativeComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerPC = GetOwningController();

	if (HasAuthority() && GetNetMode() != NM_Standalone && !GetOwner()->bAlwaysRelevant)
	{
		UE_LOG(LogNarrative, Warning, TEXT("Narrative has an owning actor %s that is not marked always relevant. This may cause sync issues. "), *GetNameSafe(GetOwner()));
	}
}

void UNarrativeComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (CurrentDialogue)
	{
		CurrentDialogue->TickDialogue(DeltaTime);
	}
}

void UNarrativeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{

	Super::EndPlay(EndPlayReason);

	if (CurrentDialogue)
	{
		CurrentDialogue->Deinitialize();
	}

}

void UNarrativeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UNarrativeComponent, PendingUpdateList);
	DOREPLIFETIME(UNarrativeComponent, PartyComponent);

	//Uncomment if you don't other players narrative components to replicate their updates to you 
	//DOREPLIFETIME_CONDITION(UNarrativeComponent, PendingUpdateList, COND_OwnerOnly);
}

bool UNarrativeComponent::HasAuthority() const
{
	return GetOwnerRole() >= ROLE_Authority;
}

bool UNarrativeComponent::IsPartyComponent() const
{
	return false;
}

bool UNarrativeComponent::IsQuestStartedOrFinished(TSubclassOf<class UQuest> QuestClass) const
{
	if (!IsValid(QuestClass))
	{
		return false;
	}

	for (auto& QuestInProgress : QuestList)
	{
		if (QuestInProgress && QuestInProgress->GetClass()->IsChildOf(QuestClass))
		{
			return QuestInProgress->QuestCompletion != EQuestCompletion::QC_NotStarted;
		}
	}

	//If quest isnt in the quest list at all we can return false
	return false;
}

bool UNarrativeComponent::IsQuestInProgress(TSubclassOf<class UQuest> QuestClass) const
{  
	if (!IsValid(QuestClass))
	{
		return false;
	}

	for (auto& QuestInProgress : QuestList)
	{
		if (QuestInProgress && QuestInProgress->GetClass()->IsChildOf(QuestClass))
		{
			return QuestInProgress->QuestCompletion == EQuestCompletion::QC_Started;
		}
	}

	return false;
}

bool UNarrativeComponent::IsQuestSucceeded(TSubclassOf<class UQuest> QuestClass) const
{
	if (!IsValid(QuestClass))
	{
		return false;
	}

	for (auto& QuestInProgress : QuestList)
	{
		if (QuestInProgress && QuestInProgress->GetClass()->IsChildOf(QuestClass))
		{
			return QuestInProgress->QuestCompletion == EQuestCompletion::QC_Succeded;
		}
	}
	return false;
}

bool UNarrativeComponent::IsQuestFailed(TSubclassOf<class UQuest> QuestClass) const
{
	if (!IsValid(QuestClass))
	{
		return false;
	}

	for (auto& QuestInProgress : QuestList)
	{
		if (QuestInProgress && QuestInProgress->GetClass()->IsChildOf(QuestClass))
		{
			return QuestInProgress->QuestCompletion == EQuestCompletion::QC_Failed;
		}
	}
	return false;
}

bool UNarrativeComponent::IsQuestFinished(TSubclassOf<class UQuest> QuestClass) const
{
	if (!IsValid(QuestClass))
	{
		return false;
	}

	for (auto& QuestInProgress : QuestList)
	{
		if (QuestInProgress && QuestInProgress->GetClass()->IsChildOf(QuestClass))
		{
			return QuestInProgress->QuestCompletion == EQuestCompletion::QC_Failed || QuestInProgress->QuestCompletion == EQuestCompletion::QC_Succeded;
		}
	}
	return false;
}

class UQuest* UNarrativeComponent::BeginQuest(TSubclassOf<class UQuest> QuestClass, FName StartFromID /*= NAME_None*/)
{

	if (QuestClass == UQuest::StaticClass())
	{
		UE_LOG(LogNarrative, Warning, TEXT("BeginQuest was passed UQuest. Supplied quest must be a child of UQuest. "));
		return nullptr;
	}

	if (UQuest* NewQuest = MakeQuestInstance(QuestClass))
	{
		//If loading from save file don't send update since server will batch all quests and send them to client to begin 
		if (!bIsLoading && HasAuthority() && GetNetMode() != NM_Standalone)
		{
			SendNarrativeUpdate(FNarrativeUpdate::BeginQuest(QuestClass, StartFromID));
		}

		/*Call this after SendNarrativeUpdate as BeginQuest itself may trigger another SendNarrativeUpdateand we want BeginQuest to get called
		before any other narrative updates come through*/
		NewQuest->BeginQuest(StartFromID);

		return NewQuest;
	}

	return nullptr;
}

bool UNarrativeComponent::RestartQuest(TSubclassOf<class UQuest> QuestClass, FName StartFromID)
{
	if (!IsValid(QuestClass))
	{
		return false;
	}

	for (int32 i = 0; i < QuestList.Num(); i++)
	{
		if (QuestList.IsValidIndex(i) && QuestList[i]->GetClass()->IsChildOf(QuestClass))
		{
			OnQuestRestarted.Broadcast(QuestList[i]);
			QuestList.RemoveAt(i);
			BeginQuest(QuestClass, StartFromID);

			if (HasAuthority() && GetNetMode() != NM_Standalone)
			{
				SendNarrativeUpdate(FNarrativeUpdate::RestartQuest(QuestClass, StartFromID));
			}

			return true;
		}
	}

	return false;
}

bool UNarrativeComponent::ForgetQuest(TSubclassOf<class UQuest> QuestClass)
{
	if (!IsValid(QuestClass))
	{
		return false;
	}

	for (int32 i = 0; i < QuestList.Num(); i++)
	{
		if (QuestList.IsValidIndex(i) && QuestList[i]->GetClass()->IsChildOf(QuestClass))
		{
			QuestList[i]->Deinitialize();

			OnQuestForgotten.Broadcast(QuestList[i]);
			QuestList.RemoveAt(i);

			if (HasAuthority() && GetNetMode() != NM_Standalone)
			{
				SendNarrativeUpdate(FNarrativeUpdate::ForgetQuest(QuestClass));
			}
			return true;
		}
	}

	return false;
}

bool UNarrativeComponent::HasDialogueAvailable(TSubclassOf<class UDialogue> DialogueClass, const FDialoguePlayParams PlayParams)
{
	if (IsValid(DialogueClass))
	{
		return MakeDialogueInstance(DialogueClass, PlayParams) != nullptr;
	}

	return false;
}

bool UNarrativeComponent::SetCurrentDialogue(TSubclassOf<class UDialogue> Dialogue, const FDialoguePlayParams PlayParams)
{
	if (Dialogue)
	{
		/**If we already have a dialogue running make sure its cleaned up before we begin a new one */
		if (CurrentDialogue)
		{
			//Check that our CurrentDialogue's priority isn't lower than the new one
			if (UDialogue* NewDialogue = Dialogue->GetDefaultObject<UDialogue>())
			{
				const int32 NewPriority = PlayParams.Priority != -1 ? PlayParams.Priority : NewDialogue->Priority;

				if (NewPriority > CurrentDialogue->Priority)
				{
					return false; 
				}
			}

			OnDialogueFinished.Broadcast(CurrentDialogue, true);

			CurrentDialogue->Deinitialize();
			CurrentDialogue = nullptr;
		}

		CurrentDialogue = MakeDialogueInstance(Dialogue, PlayParams);

		return CurrentDialogue != nullptr;
	}

	return false;
}

bool UNarrativeComponent::BeginDialogue(TSubclassOf<class UDialogue> DialogueClass, const FDialoguePlayParams PlayParams)
{
	if (HasAuthority())
	{
		//Server constructs the dialogue, grabs the authoritative dialogue lines, and passes them to the client for it to begin dialogue 
		if (SetCurrentDialogue(DialogueClass, PlayParams))
		{
			OnDialogueBegan.Broadcast(CurrentDialogue);

			if (GetNetMode() != NM_Standalone)
			{
				ClientBeginDialogue(DialogueClass, CurrentDialogue->MakeIDsFromNPCNodes(CurrentDialogue->NPCReplyChain), CurrentDialogue->MakeIDsFromPlayerNodes(CurrentDialogue->AvailableResponses));
			}

			if (CurrentDialogue)
			{
				CurrentDialogue->Play();
			}

			return true;
		}
	}

	return false;
}

void UNarrativeComponent::ClientBeginDialogue_Implementation(TSubclassOf<class UDialogue> DialogueClass, const TArray<FName>& NPCReplyChainIDs, const TArray<FName>& AvailableResponseIDs)
{
	if (IsValid(DialogueClass))
	{
		//Server constructs the dialogue, then replicates it back to the client so it can begin it
		if (!HasAuthority())
		{
			if (SetCurrentDialogue(DialogueClass))
			{
				//Created dialogue won't have a valid chunk yet on the client - use the servers authed chunk it sent
				ClientRecieveDialogueChunk(NPCReplyChainIDs, AvailableResponseIDs);

				OnDialogueBegan.Broadcast(CurrentDialogue);
			}
		}
	}
}

void UNarrativeComponent::BeginPartyDialogue(TSubclassOf<class UDialogue> Dialogue, const TArray<FName>& NPCReplyChainIDs, const TArray<FName>& AvailableResponseIDs)
{
	if (HasAuthority() && PartyComponent)
	{
		//Tell the client its began a party dialogue, let it construct it 
		ClientBeginPartyDialogue(Dialogue, NPCReplyChainIDs, AvailableResponseIDs);

		//By pointing current dialogue at our parties dialogue, the solo dialogue stuff that has already been coded should handle the local dialogue fine! 
		CurrentDialogue = PartyComponent->CurrentDialogue;
	}

}

void UNarrativeComponent::ClientBeginPartyDialogue_Implementation(TSubclassOf<class UDialogue> Dialogue, const TArray<FName>& NPCReplyChainIDs, const TArray<FName>& AvailableResponseIDs)
{
	//Servers began a party dialogue, and now we need to begin ours. 
	if (PartyComponent)
	{
		PartyComponent->ClientBeginDialogue(Dialogue, NPCReplyChainIDs, AvailableResponseIDs);

		checkf(PartyComponent->CurrentDialogue,TEXT("We tried starting a dialogue on our party component locally, but it has failed for some reason. "));

		//By pointing current dialogue at our parties dialogue, the solo dialogue stuff that has already been coded should handle the local dialogue fine! 
		CurrentDialogue = PartyComponent->CurrentDialogue;
	}
}

void UNarrativeComponent::ClientExitDialogue_Implementation()
{
	if (CurrentDialogue)
	{
		OnDialogueFinished.Broadcast(CurrentDialogue, false);

		CurrentDialogue->Deinitialize();
		CurrentDialogue = nullptr;
	}

}

void UNarrativeComponent::ClientExitPartyDialogue_Implementation()
{
	if (PartyComponent)
	{
		PartyComponent->ClientExitDialogue();

		CurrentDialogue = nullptr;
	}
}

void UNarrativeComponent::ClientRecieveDialogueChunk_Implementation(const TArray<FName>& NPCReplyChainIDs, const TArray<FName>& AvailableResponseIDs)
{
	if (!HasAuthority())
	{
		if (CurrentDialogue)
		{
			CurrentDialogue->ClientReceiveDialogueChunk(NPCReplyChainIDs, AvailableResponseIDs);
		}
	}
}

void UNarrativeComponent::TryExitDialogue()
{
	if (CurrentDialogue && CurrentDialogue->bCanBeExited)
	{
		if(HasAuthority())
		{
			if (CurrentDialogue->OwningComp)
			{
				CurrentDialogue->OwningComp->ExitDialogue();
			}
		}
		else
		{
			ServerTryExitDialogue();
		}
	}
}

void UNarrativeComponent::ExitDialogue()
{
	if (HasAuthority())
	{
		ClientExitDialogue();
	}
}

void UNarrativeComponent::ServerTryExitDialogue_Implementation()
{
	TryExitDialogue();
}

bool UNarrativeComponent::IsInDialogue()
{
	return CurrentDialogue != nullptr;
}

void UNarrativeComponent::TrySelectDialogueOption(class UDialogueNode_Player* Option)
{
	//Ask the server to select the option 
	if (CurrentDialogue && CurrentDialogue->CanSelectDialogueOption(Option))
	{
		if (HasAuthority())
		{
			if (APlayerController* OwningPC = GetOwningController())
			{
				if (CurrentDialogue && CurrentDialogue->OwningComp)
				{
					CurrentDialogue->OwningComp->SelectDialogueOption(Option, OwningPC->PlayerState);
				}
			}
		}
		else
		{
			ServerSelectDialogueOption(Option->GetID());
		}
	}
}

void UNarrativeComponent::SelectDialogueOption(class UDialogueNode_Player* Option, class APlayerState* Selector)
{
	if (HasAuthority() && Option && CurrentDialogue)
	{
		const bool bSelectAccepted = CurrentDialogue->SelectDialogueOption(Option);

		if (bSelectAccepted && GetNetMode() != NM_Standalone)
		{
			ClientSelectDialogueOption(Option->GetID(), Selector);
		}
	}
}

void UNarrativeComponent::ClientSelectDialogueOption_Implementation(const FName& OptionID, class APlayerState* Selector)
{
	if (CurrentDialogue)
	{
		if (UDialogueNode_Player* Option = CurrentDialogue->GetPlayerReplyByID(OptionID))
		{
			//We need to aim the camera at the person that actually said the line 
			if (Selector)
			{
				CurrentDialogue->SetPartyCurrentSpeaker(Selector);
			}

			const bool bSelected = CurrentDialogue->SelectDialogueOption(Option);

			if (!bSelected)
			{
				UE_LOG(LogTemp, Warning, TEXT("SelectDialogueOption returned false for option %s"), *GetNameSafe(Option));
			}
			//check(bSelected);
		}
	}
}

void UNarrativeComponent::ServerSelectDialogueOption_Implementation(const FName& OptionID)
{
	//Resolve the option ID into the actual option object
	if (CurrentDialogue && !OptionID.IsNone())
	{
		if (UDialogueNode_Player* OptionNode = CurrentDialogue->GetPlayerReplyByID(OptionID))
		{
			TrySelectDialogueOption(OptionNode);
		}
		else
		{
			UE_LOG(LogNarrative, Warning, TEXT("UNarrativeComponent::ServerSelectDialogueOption_Implementation failed to resolve dialogue option %s."), *OptionID.ToString());
		}
	}
}

bool UNarrativeComponent::TrySkipCurrentDialogueLine()
{
	if (CurrentDialogue && CurrentDialogue->CanSkipCurrentLine())
	{
		if(HasAuthority())
		{
			if (CurrentDialogue->OwningComp)
			{
				return CurrentDialogue->OwningComp->SkipCurrentDialogueLine();
			}
		}
		else
		{
			ServerTrySkipCurrentDialogueLine();
		}
		return true;
	}
	return false;
}

bool UNarrativeComponent::SkipCurrentDialogueLine()
{
	if (HasAuthority() && CurrentDialogue)
	{
		/**To ensure clients are in sync, we skip the current line on the server, and then send the new chunk through to the clients. 
		This may be a little overkill as we re-send the entire chunk with each skip, however chunks are just a few lightweight FNames so no harm here.
		
		Resending the chunk ensures clients and server are seeing exactly the same thing, which is the top priority. */
		return CurrentDialogue->SkipCurrentLine();
	}

	return false;
}

void UNarrativeComponent::ServerTrySkipCurrentDialogueLine_Implementation()
{
	TrySkipCurrentDialogueLine();
}

bool UNarrativeComponent::CompleteNarrativeDataTask(const UNarrativeDataTask* Task, const FString& Argument, const int32 Quantity)
{
	/**
	Behind the scenes, narrative just uses lightweight strings for narrative Tasks. UNarrativeTasks just serve as nice containers for strings ,
	and prevents designers mistyping data tasks when making quests, since they don't have to type out the string every time, its stored in the asset.

	UNarrativeTasks also do other nice things for us like allowing us to store metadata about the Task, where a string wouldn't let us do that. 
	*/
	if (Task)
	{
		return CompleteNarrativeDataTask(Task->TaskName, Argument, Quantity);
	}

	return false;
}

bool UNarrativeComponent::CompleteNarrativeDataTask(const FString& TaskName, const FString& Argument, const int32 Quantity)
{
	if (HasAuthority())
	{
		if (TaskName.IsEmpty() || Argument.IsEmpty())
		{
			UE_LOG(LogNarrative, Warning, TEXT("Narrative tried to process an Task that was empty, or argument was empty."));
			return false;
		}

		//We need to lookup the asset to call the delegate
		if (UNarrativeDataTask* TaskAsset = UNarrativeFunctionLibrary::GetTaskByName(this, TaskName))
		{
			OnNarrativeDataTaskCompleted.Broadcast(TaskAsset, Argument);
		}
		else
		{
			UE_LOG(LogNarrative, Warning, TEXT("Narrative tried finding the asset for Task %s, but couldn't find it."), *TaskName);
		}

		FString TaskString = (TaskName + '_' + Argument).ToLower();
		TaskString.RemoveSpacesInline();

		//Convert the Task into an FString and run it through our active quests state machines
		return CompleteNarrativeTask_Internal(TaskString, false, Quantity);
	}
	else
	{
		//Client cant update quests 
		UE_LOG(LogNarrative, Log, TEXT("Client called UNarrativeComponent::CompleteNarrativeTask. This must be called by the server as quests are server authoritative."));
		return false;
	}

	//return false;
}

class UQuest* UNarrativeComponent::MakeQuestInstance(TSubclassOf<class UQuest> QuestClass)
{
	if (IsValid(QuestClass))
	{
		//If the quest is already in the players quest list issue a warning
		if (IsValid(GetQuestInstance(QuestClass)))
		{
			UE_LOG(LogNarrative, Warning, TEXT("Narrative was asked to begin a quest the player is already doing. Use RestartQuest() to replay a started quest. "));
			return nullptr;
		}

		if (UQuest* NewQuest = NewObject<UQuest>(GetOwner(), QuestClass))
		{
			const bool bInitializedSuccessfully = NewQuest->Initialize(this);

			if (bInitializedSuccessfully)
			{
				QuestList.Add(NewQuest);
				return NewQuest;
			}
		}
	}
	return nullptr;
}

bool UNarrativeComponent::CompleteNarrativeTask_Internal(const FString& RawTaskString, const bool bFromReplication, const int32 Quantity)
{
	if (GetOwnerRole() >= ROLE_Authority || bFromReplication)
	{
		if (int32* TimesCompleted = MasterTaskList.Find(RawTaskString))
		{
			(*TimesCompleted) += Quantity;
		}
		else
		{
			MasterTaskList.Add(RawTaskString, Quantity);
		}

		//In Narrative 3 CompleteNarrativeTask is no longer used for updating quests and is more of a legacy feature, so no more to do
		return true;
	}
	return false;
}

class UDialogue* UNarrativeComponent::MakeDialogueInstance(TSubclassOf<class UDialogue> DialogueClass, const FDialoguePlayParams PlayParams)
{
	if (IsValid(DialogueClass))
	{
		if (DialogueClass == UDialogue::StaticClass())
		{
			UE_LOG(LogNarrative, Warning, TEXT("UNarrativeComponent::MakeDialogue was passed UDialogue. Supplied Dialogue must be a child of UDialogue. "));
			return nullptr;
		}

		if (UDialogue* NewDialogue = NewObject<UDialogue>(GetOwner(), DialogueClass))
		{
			if (NewDialogue->Initialize(this, PlayParams))
			{
				return NewDialogue;
			}
		}
	}
	return nullptr;
}

bool UNarrativeComponent::HasCompletedTask(const UNarrativeDataTask* Task, const FString& Name, const int32 Quantity /*= 1 */)
{
	if (!Task)
	{
		return false;
	}

	return GetNumberOfTimesTaskWasCompleted(Task, Name) >= Quantity;
}

int32 UNarrativeComponent::GetNumberOfTimesTaskWasCompleted(const UNarrativeDataTask* Task, const FString& Name)
{
	if (!Task)
	{
		return 0;
	}

	if (int32* TimesCompleted = MasterTaskList.Find(Task->MakeTaskString(Name)))
	{
		return *TimesCompleted;
	}
	return 0;
}

void UNarrativeComponent::SendNarrativeUpdate(const FNarrativeUpdate& Update)
{
	if (HasAuthority() && GetNetMode() != NM_Standalone)
	{
		PendingUpdateList.Add(Update);
		ensure(PendingUpdateList.Num());
		PendingUpdateList.Last().CreationTime = GetWorld()->GetTimeSeconds();
	}

	//STALE UPDATE REMOVAL: Disabled as was causing sync issues 
	//Remove stale updates from the pendingupdatelist, otherwise it might grow to be huge
	//TODO could the update list history around be used to add more functionality? Its essentially a history of everything the player has done in order, could be valuable

}

void UNarrativeComponent::OnRep_PartyComponent(class UNarrativePartyComponent* OldPartyComponent)
{
	if (PartyComponent)
	{
		OnJoinedParty.Broadcast(PartyComponent, OldPartyComponent);
	}
	else
	{
		OnLeaveParty.Broadcast(OldPartyComponent);
	}
}

void UNarrativeComponent::OnRep_PendingUpdateList()
{
	//Process any updates the server has ran in the same order to ensure sync without having to replace a whole array of uquests 
	if (GetOwnerRole() < ROLE_Authority)
	{
		for (auto& Update : PendingUpdateList)
		{
			if (!Update.bAcked)
			{
				switch (Update.UpdateType)
				{
					case EUpdateType::UT_None:
					{
						//UE_LOG(LogTemp, Warning, TEXT("Client recieved a UT_None update from server with payload %s. Please submit a bug report explaining how this happened. "), *Update.Payload);
					}
					break;
					case EUpdateType::UT_CompleteTask:
					{
						if (Update.IntPayload.IsValidIndex(0))
						{
							CompleteNarrativeTask_Internal(Update.Payload, true, Update.IntPayload[0]);
						}
					}
					break;
					case EUpdateType::UT_TaskProgressMade:
					{
						if (Update.IntPayload.IsValidIndex(0) && Update.IntPayload.IsValidIndex(1))
						{
							const uint8 TaskIndex = Update.IntPayload[0];
							const uint8 NewProgress = Update.IntPayload[1];

							if (UQuest* Quest = GetQuestInstance(Update.QuestClass))
							{
								FName BranchID = FName(Update.Payload);

								if (UQuestBranch* Branch = Quest->GetBranch(BranchID))
								{
									if (Branch->QuestTasks.IsValidIndex(TaskIndex))
									{
										Branch->QuestTasks[TaskIndex]->SetProgressInternal(NewProgress, true);
									}
								}
							}
						}
					}
					break;
					case EUpdateType::UT_BeginQuest:
					{
						BeginQuest(Update.QuestClass, FName(Update.Payload));
					}
					break;
					case EUpdateType::UT_RestartQuest:
					{
						RestartQuest(Update.QuestClass, FName(Update.Payload));
					}
					break;
					case EUpdateType::UT_ForgetQuest:
					{
						ForgetQuest(Update.QuestClass);
					}
					break;
					case EUpdateType::UT_QuestNewState:
					{
						if (UQuest* Quest = GetQuestInstance(Update.QuestClass))
						{
							//Server should always have a valid state to tell us to go to
							check(!Update.Payload.IsEmpty());

							Quest->EnterState_Internal(Quest->GetState(FName(Update.Payload)));
						}
					}
					break;
				}

				Update.bAcked = true;
			}
		}

		//Clear the inputs out once we've processed them 
		//PendingUpdateList.Empty();
	}
}

bool UNarrativeComponent::IsQuestValid(const UQuest* Quest, FString& OutError)
{
	
	if (!Quest)
	{
		OutError = "Narrative was given a null quest asset.";
		return false;
	}

	//This has already repped back to client and so if client is replaying narrative will complain about quest 
	//being started twice
	for (auto& QuestInProgress : QuestList)
	{
		if (QuestInProgress == Quest)
		{
			if (QuestInProgress->QuestCompletion == EQuestCompletion::QC_Succeded)
			{
				OutError = "Narrative was given a quest that has already been completed. Use RestartQuest to replay quests";
			}
			else if (QuestInProgress->QuestCompletion == EQuestCompletion::QC_Failed)
			{
				OutError = "Narrative was given a quest that has already been failed. Use RestartQuest to replay quests";
			}
			return false;
		}
	}

	TSet<FName> StateNames;
	for (auto State : Quest->States)
	{
		if (State->GetID().IsNone())
		{
			OutError = FString::Printf(TEXT("Narrative was given a quest %s that has a state with no name. Please ensure all states are given names."), *Quest->GetQuestName().ToString());
			return false;
		}

		if (!StateNames.Contains(State->GetID()))
		{
			StateNames.Add(State->GetID());
		}
		else
		{
			OutError = FString::Printf(TEXT("Narrative was given a quest %s that has more then one state with the same name. Please ensure all states have a unique name."), *Quest->GetQuestName().ToString());
			return false;
		}
	}
	return true;
}

void UNarrativeComponent::NarrativeDataTaskCompleted(const UNarrativeDataTask* NarrativeTask, const FString& Name)
{
	//If we have a Party narrative component, we should forward any tasks we complete to that component as well as this one
	if (PartyComponent)
	{
		PartyComponent->CompleteNarrativeDataTask(NarrativeTask, Name);
	}
}

void UNarrativeComponent::QuestStarted(const UQuest* Quest)
{
	if (Quest)
	{
		UE_LOG(LogNarrative, Log, TEXT("Quest started: %s"), *GetNameSafe(Quest));
	}
}

void UNarrativeComponent::QuestForgotten(const UQuest* Quest)
{
	if (Quest)
	{
		UE_LOG(LogNarrative, Log, TEXT("Quest forgotten: %s"), *GetNameSafe(Quest));
	}
}

void UNarrativeComponent::QuestFailed(const UQuest* Quest, const FText& QuestFailedMessage)
{
	if (Quest)
	{
		UE_LOG(LogNarrative, Log, TEXT("Quest failed: %s. Failure state: %s"), *GetNameSafe(Quest), *QuestFailedMessage.ToString());
	}
}

void UNarrativeComponent::QuestSucceeded(const UQuest* Quest, const FText& QuestSucceededMessage)
{
	// No need to autosave on quest succeeded because QuestObjectiveCompleted already performs an autosave and is called on quest completion
	if (Quest)
	{
		UE_LOG(LogNarrative, Log, TEXT("Quest succeeded: %s. Succeeded state: %s"), *GetNameSafe(Quest), *QuestSucceededMessage.ToString());
	}
}

void UNarrativeComponent::QuestNewState(UQuest* Quest, const UQuestState* NewState)
{
	// No need to autosave on new objective because QuestObjectiveCompleted already performs an autosave and is called when we get a new objective
	if (Quest)
	{
		if (NewState)
		{
			UE_LOG(LogNarrative, Log, TEXT("Reached new state: %s in quest: %s"), *NewState->Description.ToString(), *GetNameSafe(Quest));
		}

		if (HasAuthority() && GetNetMode() != NM_Standalone)
		{
			//The server doesn't actually need to tell clients when they get to a new state, it can just send progress updates
			// and when progress == requiredprogress the client can advance to the state
			//SendNarrativeUpdate(FNarrativeUpdate::QuestNewState(Quest->GetClass(), NewState->GetID()));
		}
	}
}

void UNarrativeComponent::QuestTaskProgressMade(const UQuest* Quest, const UNarrativeTask* Task, const class UQuestBranch* Branch, int32 OldProgress, int32 NewProgress)
{
	if (Quest && Task)
	{
		UE_LOG(LogNarrative, Log, TEXT("Quest %s made progress - task %s is now %d/%d"), *GetNameSafe(Quest), *(Task->GetTaskDescription().ToString()), NewProgress, Task->RequiredQuantity);

		uint8 TaskIdx = 0;

		for (auto& QuestTask : Branch->QuestTasks)
		{
			if (Task == QuestTask)
			{
				break;
			}
			else
			{
				TaskIdx++;
			}
		}

		SendNarrativeUpdate(FNarrativeUpdate::TaskProgressMade(Quest->GetClass(), TaskIdx, NewProgress, Branch->GetID()));
	}
}

void UNarrativeComponent::QuestTaskCompleted(const UQuest* Quest, const UNarrativeTask* Task, const class UQuestBranch* Branch)
{

}

void UNarrativeComponent::QuestBranchCompleted(const UQuest* Quest, const class UQuestBranch* Branch)
{

}

void UNarrativeComponent::BeginSave(FString SaveName)
{
	UE_LOG(LogNarrative, Verbose, TEXT("Begun saving using save name: %s"), *SaveName);
}

void UNarrativeComponent::BeginLoad(FString SaveName)
{
	UE_LOG(LogNarrative, Verbose, TEXT("Begun loading using save name: %s"), *SaveName);
}

void UNarrativeComponent::SaveComplete(FString SaveName)
{
	UE_LOG(LogNarrative, Verbose, TEXT("Save complete for save name: %s"), *SaveName);
}

void UNarrativeComponent::LoadComplete(FString SaveName)
{
	UE_LOG(LogNarrative, Verbose, TEXT("Load complete for save name: %s"), *SaveName);
}

void UNarrativeComponent::DialogueRepliesAvailable(class UDialogue* Dialogue, const TArray<UDialogueNode_Player*>& PlayerReplies)
{

}

void UNarrativeComponent::DialogueLineStarted(class UDialogue* Dialogue, UDialogueNode* Node, const FDialogueLine& DialogueLine)
{

}

void UNarrativeComponent::DialogueLineFinished(class UDialogue* Dialogue, UDialogueNode* Node, const FDialogueLine& DialogueLine)
{

}

void UNarrativeComponent::DialogueBegan(UDialogue* Dialogue)
{
	
}

void UNarrativeComponent::DialogueFinished(UDialogue* Dialogue, const bool bStartingNewDialogue)
{

}

APawn* UNarrativeComponent::GetOwningPawn() const
{
	if (OwnerPC)
	{
		return OwnerPC->GetPawn();
	}

	APlayerController* OwningController = Cast<APlayerController>(GetOwner());
	APawn* OwningPawn = Cast<APawn>(GetOwner());

	if (OwningPawn)
	{
		return OwningPawn;
	}

	if (!OwningPawn && OwningController)
	{
		return OwningController->GetPawn();
	}

	return nullptr;
}

APlayerController* UNarrativeComponent::GetOwningController() const
{
	//We cache this on beginplay as to not re-find it every time 
	if (OwnerPC)
	{
		return OwnerPC;
	}

	APlayerController* OwningController = Cast<APlayerController>(GetOwner());
	APawn* OwningPawn = Cast<APawn>(GetOwner());

	if (OwningController)
	{
		return OwningController;
	}

	if (!OwningController && OwningPawn)
	{
		return Cast<APlayerController>(OwningPawn->GetController());
	}

	return nullptr;
}

class UDialogue* UNarrativeComponent::GetCurrentDialogue() const
{
	if (PartyComponent && PartyComponent->CurrentDialogue)
	{
		return PartyComponent->CurrentDialogue;
	}
	else if (CurrentDialogue)
	{
		return CurrentDialogue;
	}
	
	return nullptr;
}

TArray<UQuest*> UNarrativeComponent::GetFailedQuests() const
{
	TArray<UQuest*> FailedQuests;
	for (auto QIP : QuestList)
	{
		if (QIP->QuestCompletion == EQuestCompletion::QC_Failed)
		{
			FailedQuests.Add(QIP);
		}
	}
	return FailedQuests;
}


TArray<UQuest*> UNarrativeComponent::GetSucceededQuests() const
{
	TArray<UQuest*> SucceededQuests;
	for (auto QIP : QuestList)
	{
		if (QIP->QuestCompletion == EQuestCompletion::QC_Succeded)
		{
			SucceededQuests.Add(QIP);
		}
	}
	return SucceededQuests;
}

TArray<UQuest*> UNarrativeComponent::GetInProgressQuests() const
{
	TArray<UQuest*> InProgressQuests;
	for (auto QIP : QuestList)
	{
		if (QIP->QuestCompletion == EQuestCompletion::QC_Started)
		{
			InProgressQuests.Add(QIP);
		}
	}
	return InProgressQuests;
}

TArray<UQuest*> UNarrativeComponent::GetAllQuests() const
{
	return QuestList;
}

class UQuest* UNarrativeComponent::GetQuestInstance(TSubclassOf<class UQuest> QuestClass) const
{
	for (auto& QIP : QuestList)
	{
		if (QIP && QIP->GetClass()->IsChildOf(QuestClass))
		{
			return QIP;
		}
	}
	return nullptr;
}

bool UNarrativeComponent::Save(const FString& SaveName/** = "NarrativeSaveData"*/, const int32 Slot/** = 0*/)
{
	OnBeginSave.Broadcast(SaveName);
	
	if (UNarrativeSaveGame* NarrativeSaveGame = Cast<UNarrativeSaveGame>(UGameplayStatics::CreateSaveGameObject(UNarrativeSaveGame::StaticClass())))
	{
		NarrativeSaveGame->MasterTaskList = MasterTaskList;

		for (auto& Quest : QuestList)
		{
			if (Quest)
			{
				FNarrativeSavedQuest Save;
				Save.QuestClass = Quest->GetClass();
				Save.CurrentStateID = Quest->GetCurrentState()->GetID();

				//Save all the quests branches, and the current progress on each branches task 
				for (UQuestBranch* Branch : Quest->Branches)
				{
					TArray<int32> TasksProgress;

					for (auto& BranchTask : Branch->QuestTasks)
					{
						if (BranchTask)
						{
							TasksProgress.Add(BranchTask->CurrentProgress);
						}
					}

					Save.QuestBranches.Add(FSavedQuestBranch(Branch->GetID(), TasksProgress));
				}

				//Store all the reached states in the save file
				for (UQuestState* State : Quest->ReachedStates)
				{
					Save.ReachedStateNames.Add(State->GetID());
				}

				NarrativeSaveGame->SavedQuests.Add(Save);
			}
		}

		if (UGameplayStatics::SaveGameToSlot(NarrativeSaveGame, SaveName, Slot))
		{
			OnSaveComplete.Broadcast(SaveName);
			return true;
		}
	}
return false;
}

bool UNarrativeComponent::Load(const FString& SaveName/** = "NarrativeSaveData"*/, const int32 Slot/** = 0*/)
{
	if (!UGameplayStatics::DoesSaveGameExist(SaveName, 0))
	{
		return false;
	}

	if (UNarrativeSaveGame* NarrativeSaveGame = Cast<UNarrativeSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveName, Slot)))
	{
		OnBeginLoad.Broadcast(SaveName);

		Load_Internal(NarrativeSaveGame->SavedQuests, NarrativeSaveGame->MasterTaskList);

		//We've loaded on the server, we need to send all the load data to the client so it is synced 
		if (HasAuthority() && GetNetMode() != NM_Standalone)
		{
			//Send the unpacked save file to the client - need to split MasterTaskList into 2 arrays since TMaps arent networked
			TArray<FString> Tasks;
			TArray<int32> Quantities;
			MasterTaskList.GenerateKeyArray(Tasks);
			MasterTaskList.GenerateValueArray(Quantities);

			ClientReceiveSave(NarrativeSaveGame->SavedQuests, Tasks, Quantities);
		}

		OnLoadComplete.Broadcast(SaveName);
		return true;
	}
	return false;
}

bool UNarrativeComponent::DeleteSave(const FString& SaveName /*= "NarrativeSaveData"*/, const int32 Slot/** = 0*/)
{
	if (!UGameplayStatics::DoesSaveGameExist(SaveName, 0))
	{
		return false;
	}

	return UGameplayStatics::DeleteGameInSlot(SaveName, Slot);
}

void UNarrativeComponent::ClientReceiveSave_Implementation(const TArray<FNarrativeSavedQuest>& SavedQuests, const TArray<FString>& Tasks, const TArray<int32>& Quantities)
{
	ensure(Tasks.Num() == Quantities.Num());

	//For security reasons its probably best to not pass the save file names to clients
	FString DummyString = "ServerInvokedLoad";
	OnBeginLoad.Broadcast(DummyString);

	//Server sent the TMap as two seperate arrays, reconstruct
	TMap<FString, int32> RemadeTaskList;
	int32 Idx = 0;
	for (auto& Task : Tasks)
	{
		ensure(Quantities.IsValidIndex(Idx));

		RemadeTaskList.Add(Task, Quantities[Idx]);
		++Idx;
	}

	Load_Internal(SavedQuests, RemadeTaskList);

	//For a client save name should be irrelevant so dont bother sending via RPC 
	OnLoadComplete.Broadcast(DummyString);

}

bool UNarrativeComponent::Load_Internal(const TArray<FNarrativeSavedQuest>& SavedQuests, const TMap<FString, int32>& NewMasterList)
{
	bIsLoading = true;

	//Remove all our current quests - we're about the load the new ones from the save file 
	for (int32 i = QuestList.Num() - 1; i >= 0; --i)
	{
		if (QuestList.IsValidIndex(i))
		{
			ForgetQuest(QuestList[i]->GetClass());
		}
	}

	//QuestList.Empty();
	MasterTaskList.Empty();

	MasterTaskList = NewMasterList;

	for (auto& SaveQuest : SavedQuests)
	{
		//Restore the quest from the last state we were in 
		if (UQuest* BegunQuest = BeginQuest(SaveQuest.QuestClass, SaveQuest.CurrentStateID))
		{
			//Restore all quest branches - this is messy because our TMap design had to be changed into TArrays because TMaps cant replicate 
			if (UQuestState* CurrentState = BegunQuest->GetCurrentState())
			{
				for (auto& Branch : BegunQuest->Branches)//(auto& Branch : CurrentState->Branches)
				{
					//For each branch leading off the current state, set its currentprogress to the saved one
					for (auto& SavedBranch : SaveQuest.QuestBranches)
					{
						if (Branch->GetID() == SavedBranch.BranchID)
						{
							for (int32 i = 0; i < Branch->QuestTasks.Num(); ++i)
							{
								if (Branch->QuestTasks.IsValidIndex(i) && SavedBranch.TasksProgress.IsValidIndex(i))
								{
									Branch->QuestTasks[i]->BeginTaskInit();
									Branch->QuestTasks[i]->SetProgressInternal(SavedBranch.TasksProgress[i]);
								}
							}

							continue;
						}
					}
				}
			} 

			//Restore all reached states
			TArray<UQuestState*> AllReachedStates;

			//Remove the last state, it will have already been added by BeginQuest_Internal
			BegunQuest->ReachedStates.Empty();

			for (const FName StateName : SaveQuest.ReachedStateNames)
			{
				BegunQuest->ReachedStates.Add(BegunQuest->GetState(StateName));
			}
		}
	}

	bIsLoading = false;

	return true;
}

