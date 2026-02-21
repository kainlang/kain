// © 2025 mrbaconvn. All Rights Reserved.


#include "GameplayTask/GameplayTask_WaitForAnimDrivenData.h"
#include "Animation/AnimSequenceBase.h"

UGameplayTask_WaitForAnimDrivenData* UGameplayTask_WaitForAnimDrivenData::CreateTask(
	TScriptInterface<IGameplayTaskOwnerInterface> TaskOwner, TSubclassOf<UAnimNotifyData> DataClass, bool bShouldTick)
{
	UGameplayTask_WaitForAnimDrivenData* Task = NewTask<UGameplayTask_WaitForAnimDrivenData>(TaskOwner);
	Task->DataClass = DataClass;
	Task->bTickingTask = bShouldTick;
	Task->FilterAnimSource = nullptr; // No animation filter for this task
	return Task;
}

UGameplayTask_WaitForAnimDrivenData* UGameplayTask_WaitForAnimDrivenData::CreateTask_FilterAnimation(
	TScriptInterface<IGameplayTaskOwnerInterface> TaskOwner, TSubclassOf<UAnimNotifyData> DataClass,
	UAnimSequenceBase* Animation, bool bShouldTick)
{
	UGameplayTask_WaitForAnimDrivenData* Task = NewTask<UGameplayTask_WaitForAnimDrivenData>(TaskOwner);
	Task->DataClass = DataClass;
	Task->bTickingTask = bShouldTick;
	Task->FilterAnimSource = Animation;
	return Task;
}

TSubclassOf<UAnimNotifyData> UGameplayTask_WaitForAnimDrivenData::GetNotifyClass() const
{
	return DataClass;
}

void UGameplayTask_WaitForAnimDrivenData::ProcessNotify(const UAnimSequenceBase* SourceAnim, const UAnimNotifyData* NotifyData, EAnimNotifyState NotifyState)
{
	OnAnimationNotifyReceived.Broadcast(SourceAnim, NotifyData, NotifyState);
}