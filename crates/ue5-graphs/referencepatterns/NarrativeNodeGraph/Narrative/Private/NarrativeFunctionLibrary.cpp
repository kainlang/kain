// Copyright Narrative Tools 2022. 


#include "NarrativeFunctionLibrary.h"
#include "NarrativeComponent.h"
#include "NarrativeTaskManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "Engine/GameInstance.h"

class UNarrativeComponent* UNarrativeFunctionLibrary::GetNarrativeComponent(const UObject* WorldContextObject)
{
	return GetNarrativeComponentFromTarget(UGameplayStatics::GetPlayerController(WorldContextObject, 0));
}

class UNarrativeComponent* UNarrativeFunctionLibrary::GetNarrativeComponentFromTarget(AActor* Target)
{
	if (!Target)
	{
		return nullptr;
	}

	if (UNarrativeComponent* NarrativeComp = Target->FindComponentByClass<UNarrativeComponent>())
	{
		return NarrativeComp;
	}

	//Narrative comp may be on the controllers pawn or pawns controller
	if (APlayerController* OwningController = Cast<APlayerController>(Target))
	{
		if (OwningController->GetPawn())
		{
			return OwningController->GetPawn()->FindComponentByClass<UNarrativeComponent>();
		}
	}

	if (APawn* OwningPawn = Cast<APawn>(Target))
	{
		if (OwningPawn->GetController())
		{
			return OwningPawn->GetController()->FindComponentByClass<UNarrativeComponent>();
		}
	}

	return nullptr;
}

bool UNarrativeFunctionLibrary::CompleteNarrativeDataTask(class UNarrativeComponent* Target, const UNarrativeDataTask* Task, const FString& Argument, const int32 Quantity)
{
	if (Target)
	{
		return Target->CompleteNarrativeDataTask(Task, Argument, Quantity);
	}
	return false;
}

static FString DefaultString("LooseTask");

bool UNarrativeFunctionLibrary::CompleteLooseNarrativeDataTask(class UNarrativeComponent* Target, const FString& Argument, const int32 Quantity /*= 1*/)
{
	if (Target)
	{
		return Target->CompleteNarrativeDataTask(DefaultString, Argument, Quantity);
	}
	return false;
}

class UNarrativeDataTask* UNarrativeFunctionLibrary::GetTaskByName(const UObject* WorldContextObject, const FString& EventName)
{
	if (UGameInstance* GI = UGameplayStatics::GetGameInstance(WorldContextObject))
	{
		if (UNarrativeTaskManager* EventManager = GI->GetSubsystem<UNarrativeTaskManager>())
		{
			return EventManager->GetTask(EventName);
		}
	}

	return nullptr;
}

FString UNarrativeFunctionLibrary::MakeDisplayString(const FString& String)
{
	return FName::NameToDisplayString(String, false);
}
