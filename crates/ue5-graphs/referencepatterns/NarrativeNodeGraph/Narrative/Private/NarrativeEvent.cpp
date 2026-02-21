// Copyright Narrative Tools 2022. 


#include "NarrativeEvent.h"


UNarrativeEvent::UNarrativeEvent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bRefireOnLoad = true; 
}

void UNarrativeEvent::ExecuteEvent_Implementation(APawn* Pawn, APlayerController* Controller, class UNarrativeComponent* NarrativeComponent)
{
	
}

FString UNarrativeEvent::GetGraphDisplayText_Implementation()
{
	return GetName();
}

FText UNarrativeEvent::GetHintText_Implementation()
{
	return FText::GetEmpty();
}
