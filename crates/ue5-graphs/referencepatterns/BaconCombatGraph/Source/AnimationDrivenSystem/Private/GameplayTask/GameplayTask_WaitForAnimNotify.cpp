// © 2025 mrbaconvn. All Rights Reserved.

#include "GameplayTask/GameplayTask_WaitForAnimNotify.h"
#include "GameFramework/Actor.h"
#include "Animation/AnimSequenceBase.h"
#include "AnimationDrivenComponent.h"

void UGameplayTask_WaitForAnimNotify::Activate()
{
	Super::Activate();

	TickNotifyDataList.Empty();
	if(!IsValid(GetOwnerActor()))
	{
		EndTask();
		return;
	}
	
	UAnimationDrivenComponent* AnimDrivenComponent = Cast<UAnimationDrivenComponent>(GetAvatarActor()->GetComponentByClass(UAnimationDrivenComponent::StaticClass()));
	if(IsValid(AnimDrivenComponent))
	{
		AnimDrivenComponent->OnNotifyReceived.AddDynamic(this, &UGameplayTask_WaitForAnimNotify::OnNotifyReceived);
	}
}

void UGameplayTask_WaitForAnimNotify::OnDestroy(bool AbilityEnded)
{
	if(IsValid(GetOwnerActor()))
	{
		UAnimationDrivenComponent* AnimDrivenComponent = Cast<UAnimationDrivenComponent>(GetAvatarActor()->GetComponentByClass(UAnimationDrivenComponent::StaticClass()));
		if(IsValid(AnimDrivenComponent))
		{
			AnimDrivenComponent->OnNotifyReceived.RemoveDynamic(this, &UGameplayTask_WaitForAnimNotify::OnNotifyReceived);
		}
	}
	
	FilterAnimSource = nullptr;
	TickNotifyDataList.Empty();
	
	Super::OnDestroy(AbilityEnded);
}

void UGameplayTask_WaitForAnimNotify::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	if(!bTickingTask)
	{
		return;
	}

	for(const auto& NotifyData : TickNotifyDataList)
	{
		if(!NotifyData.IsValid())
		{
			continue;
		}

		ProcessNotify(FilterAnimSource.Get(), NotifyData.Get(), EAnimNotifyState::ENSD_TICK);
	}
}

void UGameplayTask_WaitForAnimNotify::OnNotifyReceived(const UAnimSequenceBase* SourceAnim,
	const UAnimNotifyData* NotifyData, EAnimNotifyState NotifyState)
{
	if(FilterAnimSource != nullptr && FilterAnimSource != SourceAnim)
	{
		return;
	}
	
	if(!IsValid(NotifyData) || !NotifyData->IsA(GetNotifyClass()))
	{
		return;
	} 

	if(NotifyState != EAnimNotifyState::ENSD_START)
	{
		// The notify only send start and end, if it's not start then we remove the notify data
		TickNotifyDataList.Remove(NotifyData);
	}
	else
	{
		TickNotifyDataList.Add(NotifyData);
	}
	
	ProcessNotify(SourceAnim, NotifyData, NotifyState);

	// We only tick if we have notify data to tick
	bTickingTask = TickNotifyDataList.Num() > 0;
}
 
void UGameplayTask_WaitForAnimNotify::ProcessNotify(const UAnimSequenceBase* SourceAnim,
	const UAnimNotifyData* NotifyData, EAnimNotifyState NotifyState)
{
}

TSubclassOf<UAnimNotifyData> UGameplayTask_WaitForAnimNotify::GetNotifyClass() const
{
	return nullptr;
} 