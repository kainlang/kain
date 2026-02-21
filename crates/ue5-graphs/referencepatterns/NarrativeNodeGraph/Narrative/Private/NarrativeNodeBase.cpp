// Copyright Narrative Tools 2022. 


#include "NarrativeNodeBase.h"
#include "NarrativeCondition.h"
#include "NarrativeEvent.h"
#include "NarrativeComponent.h"
#include "NarrativePartyComponent.h"

UNarrativeNodeBase::UNarrativeNodeBase()
{
	//autofill the ID
	ID = GetFName();
}

#if WITH_EDITOR

void UNarrativeNodeBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.MemberProperty)
	{
		//If we changed the ID, make sure it doesn't conflict with any other IDs in the quest
		if (PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UNarrativeNodeBase, ID))
		{
			EnsureUniqueID();
		}
	}
}

#endif 

void UNarrativeNodeBase::ProcessEvents(APawn* Pawn, APlayerController* Controller, class UNarrativeComponent* NarrativeComponent, const EEventRuntime Runtime)
{
	if (!NarrativeComponent)
	{
		UE_LOG(LogNarrative, Warning, TEXT("Tried running events on node %s but Narrative Comp was null."), *GetNameSafe(this));
		return;
	}

	const bool bIsLoading = NarrativeComponent->bIsLoading;

	for (auto& Event : Events)
	{
		if (Event)
		{
			const bool bShouldFire = (!bIsLoading || Event->bRefireOnLoad ) && (Event->EventRuntime == Runtime || Event->EventRuntime == EEventRuntime::Both);

			if (bShouldFire)
			{
				TArray<UNarrativeComponent*> CompsToExecute;

				if (UNarrativePartyComponent* PartyComp = Cast<UNarrativePartyComponent>(NarrativeComponent))
				{
					if (Event->PartyEventPolicy == EPartyEventPolicy::AllPartyMembers)
					{
						CompsToExecute.Append(PartyComp->GetPartyMembers());
					}
					else if (Event->PartyEventPolicy == EPartyEventPolicy::PartyLeader)
					{
						CompsToExecute.Add(PartyComp->GetPartyLeader());
					}
					else if (Event->PartyEventPolicy == EPartyEventPolicy::Party)
					{
						CompsToExecute.Add(PartyComp);
					}
				}
				else
				{
					CompsToExecute.Add(NarrativeComponent);
				}

				for (auto& Comp : CompsToExecute)
				{
					Event->ExecuteEvent(Comp->GetOwningPawn(), Comp->GetOwningController(), Comp);
				}
			}
		}
	}
}

bool UNarrativeNodeBase::AreConditionsMet(APawn* Pawn, APlayerController* Controller, class UNarrativeComponent* NarrativeComponent)
{

	if (!NarrativeComponent)
	{
		UE_LOG(LogNarrative, Warning, TEXT("Tried running conditions on node %s but Narrative Comp was null."), *GetNameSafe(this));
		return false;
	}
	  
	//Ensure all conditions are met
	for (auto& Cond : Conditions)
	{	
		if (Cond)
		{
			//We're running a condition on a party! Figure out who we need to run the condition on
			if (UNarrativePartyComponent* PartyComp = Cast<UNarrativePartyComponent>(NarrativeComponent))
			{
				TArray<UNarrativeComponent*> ComponentsToCheck;

				UE_LOG(LogNarrative, Warning, TEXT("Running on party..."));
				//We need to check everyone in the party
				if (Cond->PartyConditionPolicy == EPartyConditionPolicy::AllPlayersPass || Cond->PartyConditionPolicy == EPartyConditionPolicy::AnyPlayerPasses)
				{
					ComponentsToCheck.Append(PartyComp->GetPartyMembers());
				}//We need to check the party leader
				else if (Cond->PartyConditionPolicy == EPartyConditionPolicy::PartyLeaderPasses)
				{
					ComponentsToCheck.Add(PartyComp->GetPartyLeader());
				}
				else if (Cond->PartyConditionPolicy == EPartyConditionPolicy::PartyPasses)
				{
					ComponentsToCheck.Add(PartyComp);
				}

				bool bAnyonePassed = false;

				//If any of our comps to check fail, return false 
				for (auto& ComponentToCheck : ComponentsToCheck)
				{	
					const bool bConditionPassed = Cond && Cond->CheckCondition(ComponentToCheck->GetOwningPawn(), ComponentToCheck->GetOwningController(), ComponentToCheck) != Cond->bNot;
					FString CondString = bConditionPassed ? "passed" : "failed";

					if (bConditionPassed)
					{
						//We'll check the next condition since someone passed
						if (Cond->PartyConditionPolicy == EPartyConditionPolicy::AnyPlayerPasses)
						{
							bAnyonePassed = true;
							break;
						}
					}
					else
					{
						if (Cond->PartyConditionPolicy != EPartyConditionPolicy::AnyPlayerPasses)
						{
							return false;
						}
					}

					UE_LOG(LogNarrative, Warning, TEXT("Checking %s condition, and they: %s"), *GetNameSafe(ComponentToCheck), *CondString);
				}

				//If we didn't break, no players passed 
				if (!bAnyonePassed && Cond->PartyConditionPolicy == EPartyConditionPolicy::AnyPlayerPasses)
				{
					return false;
				}

			}
			else
			{
				if (Cond && Cond->CheckCondition(Pawn, Controller, NarrativeComponent) == Cond->bNot)
				{
					return false;
				}
			}

		}
	}

	return true;
}
