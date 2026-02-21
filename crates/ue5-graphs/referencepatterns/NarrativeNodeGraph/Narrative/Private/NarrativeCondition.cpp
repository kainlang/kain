// Copyright Narrative Tools 2022. 


#include "NarrativeCondition.h"


bool UNarrativeCondition::CheckCondition_Implementation(APawn* Pawn, APlayerController* Controller, class UNarrativeComponent* NarrativeComponent)
{
	return true;
}

FString UNarrativeCondition::GetGraphDisplayText_Implementation()
{
	return GetName();
}
