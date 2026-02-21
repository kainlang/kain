// © 2025 mrbaconvn. All Rights Reserved.

#include "AbilityTask/AbilityTask_WaitForAnimNotify.h"

#include "AnimationDrivenComponent.h"
#include "Animation/AnimSequenceBase.h"

void UAbilityTask_WaitForAnimNotify::Activate()
{
	Super::Activate();
	
	TickNotifyDataList.Empty();
	if(!IsValid(GetAvatarActor()))
	{
		EndTask();
		return;
	} 
	 
	UAnimationDrivenComponent* AnimDrivenComponent = Cast<UAnimationDrivenComponent>(GetAvatarActor()->GetComponentByClass(UAnimationDrivenComponent::StaticClass()));
	if(IsValid(AnimDrivenComponent))
	{
		AnimDrivenComponent->OnNotifyReceived.AddDynamic(this, &UAbilityTask_WaitForAnimNotify::OnNotifyReceived);
	}
}

void UAbilityTask_WaitForAnimNotify::OnDestroy(bool AbilityEnded)
{
	if(IsValid(GetAvatarActor()))
	{
		UAnimationDrivenComponent* AnimDrivenComponent = Cast<UAnimationDrivenComponent>(GetAvatarActor()->GetComponentByClass(UAnimationDrivenComponent::StaticClass()));
		if(IsValid(AnimDrivenComponent))
		{
			AnimDrivenComponent->OnNotifyReceived.RemoveDynamic(this, &UAbilityTask_WaitForAnimNotify::OnNotifyReceived);
		}
	}

	FilterAnimSource = nullptr;
	TickNotifyDataList.Empty();
	
	Super::OnDestroy(AbilityEnded);
}

void UAbilityTask_WaitForAnimNotify::TickTask(float DeltaTime)
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

void UAbilityTask_WaitForAnimNotify::OnNotifyReceived(const UAnimSequenceBase* SourceAnim,
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
	
	// We will start ticking if the TickNotifyData is not empty
	bTickingTask = TickNotifyDataList.Num() > 0;
}

void UAbilityTask_WaitForAnimNotify::ProcessNotify(const UAnimSequenceBase* SourceAnim, const UAnimNotifyData* NotifyData, EAnimNotifyState NotifyState)
{
}

TSubclassOf<UAnimNotifyData> UAbilityTask_WaitForAnimNotify::GetNotifyClass() const
{
	return nullptr;
} 
