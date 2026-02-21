// © 2025 mrbaconvn. All Rights Reserved.


#include "AbilityTask/AbilityTask_WaitForAnimDrivenData.h"
#include "AbilitySystemComponent.h"
#include "AnimationDrivenComponent.h"
#include "Animation/AnimMontage.h"

UAbilityTask_WaitForAnimDrivenData* UAbilityTask_WaitForAnimDrivenData::WaitForAnimDrivenData(
	UGameplayAbility* OwningAbility, TSubclassOf<UAnimNotifyData> DataClass, bool bShouldTick)
{
	UAbilityTask_WaitForAnimDrivenData* Task = NewAbilityTask<UAbilityTask_WaitForAnimDrivenData>(OwningAbility);
	Task->DataClass = DataClass;
	Task->bTickingTask = bShouldTick;
	Task->FilterAnimSource = OwningAbility->GetCurrentMontage();
	return Task;
}

UAbilityTask_WaitForAnimDrivenData* UAbilityTask_WaitForAnimDrivenData::WaitForAnimDrivenData_FilterAnimation(
	UGameplayAbility* OwningAbility, TSubclassOf<UAnimNotifyData> DataClass, UAnimSequenceBase* Animation,
	bool bShouldTick)
{
	UAbilityTask_WaitForAnimDrivenData* Task = NewAbilityTask<UAbilityTask_WaitForAnimDrivenData>(OwningAbility);
	Task->DataClass = DataClass;
	Task->bTickingTask = bShouldTick;
	Task->FilterAnimSource = Animation;
	return Task;
}

TSubclassOf<UAnimNotifyData> UAbilityTask_WaitForAnimDrivenData::GetNotifyClass() const
{
	return DataClass;
}

void UAbilityTask_WaitForAnimDrivenData::ProcessNotify(const UAnimSequenceBase* SourceAnim, const UAnimNotifyData* NotifyData, EAnimNotifyState NotifyState)
{ 
	OnAnimationNotifyReceived.Broadcast(SourceAnim, NotifyData, NotifyState);
}
 