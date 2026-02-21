// Copyright Narrative Tools 2022. 


#include "NarrativePartyComponent.h"
#include "Net/UnrealNetwork.h"
#include <GameFramework/PlayerController.h>
#include <GameFramework/PlayerState.h>

UNarrativePartyComponent::UNarrativePartyComponent()
{

}

void UNarrativePartyComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UNarrativePartyComponent, PartyMemberStates);
}

bool UNarrativePartyComponent::BeginDialogue(TSubclassOf<class UDialogue> DialogueClass, const FDialoguePlayParams PlayParams)
{
	if (HasAuthority())
	{
		//Server constructs the dialogue, then replicates it back to the client so it can begin it
		if (SetCurrentDialogue(DialogueClass, PlayParams))
		{
			OnDialogueBegan.Broadcast(CurrentDialogue);

			//Tell all our party members to begin the dialogue 
			for (auto& GroupMember : PartyMembers)
			{
				if (GroupMember)
				{
					GroupMember->BeginPartyDialogue(DialogueClass, CurrentDialogue->MakeIDsFromNPCNodes(CurrentDialogue->NPCReplyChain), CurrentDialogue->MakeIDsFromPlayerNodes(CurrentDialogue->AvailableResponses));
				}
			}

			CurrentDialogue->Play();

			return true;
		}
	}

	return false;
}

void UNarrativePartyComponent::SelectDialogueOption(class UDialogueNode_Player* Option, class APlayerState* Selector )
{
	if (HasAuthority() && Option && CurrentDialogue)
	{
		const bool bSelectAccepted = CurrentDialogue->SelectDialogueOption(Option);

		if (bSelectAccepted)
		{
			//Tell all our party members to select the option
			for (auto& GroupMember : PartyMembers)
			{
				if (GroupMember)
				{
					GroupMember->ClientSelectDialogueOption(Option->GetID(), Selector);
				}
			}
		}
	}
}

void UNarrativePartyComponent::ExitDialogue()
{
	if (HasAuthority())
	{
		for (auto& GroupMemberComp : GetPartyMembers())
		{
			if (GroupMemberComp)
			{
				GroupMemberComp->CurrentDialogue = nullptr;
				GroupMemberComp->ClientExitPartyDialogue();
			}
		}

		ClientExitDialogue();
	}
}

APawn* UNarrativePartyComponent::GetOwningPawn() const
{
	//Find the local players pawn
	if (APlayerController* PC = GetOwningController())
	{
		return PC->GetPawn();
	}
	return nullptr;
}

APlayerController* UNarrativePartyComponent::GetOwningController() const
{
	//Find the local players pawn
	for (auto& MemberState : PartyMemberStates)
	{
		if (MemberState)
		{
			if (APlayerController* PC = Cast<APlayerController>(MemberState->GetOwningController()))
			{
				if (PC->IsLocalController())
				{
					return PC;
				}
			}
		}
	}
	return nullptr;
}

bool UNarrativePartyComponent::AddPartyMember(class UNarrativeComponent* Member)
{
	if (HasAuthority() && Member)
	{
		//Parties can't have other parties in them 
		if (Member->IsA<UNarrativePartyComponent>())
		{
			return false;
		}

		UNarrativePartyComponent* const ExistingParty = Member->GetParty();

		if (ExistingParty)
		{	
			//Already in the party, don't do anything 
			if (ExistingParty == this)
			{
				return false;
			}

			//We're in another party, remove the player from that one first
			ExistingParty->RemovePartyMember(Member);
		}

		//Add the player to the party
		if (APlayerController* PC = Member->GetOwningController())
		{
			PartyMembers.Add(Member);
			PartyMemberStates.Add(PC->PlayerState);

			Member->PartyComponent = this;
			Member->OnRep_PartyComponent(ExistingParty);

			return true;
		}

	}

	return false;
}

bool UNarrativePartyComponent::RemovePartyMember(class UNarrativeComponent* Member)
{
	if (HasAuthority() && Member && PartyMembers.Contains(Member))
	{
		if (APlayerController* PC = Member->GetOwningController())
		{
			PartyMembers.Remove(Member);
			PartyMemberStates.Remove(PC->PlayerState);

			Member->PartyComponent = nullptr;
			Member->OnRep_PartyComponent(this);

			return true;
		}
	}

	return false;
}

bool UNarrativePartyComponent::IsPartyLeader(class APlayerState* Member)
{
	if (PartyMemberStates.IsValidIndex(0))
	{
		return PartyMemberStates[0] == Member;
	}

	return false;
}