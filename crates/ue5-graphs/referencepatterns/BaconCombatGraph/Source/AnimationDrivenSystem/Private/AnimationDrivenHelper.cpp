// © 2025 mrbaconvn. All Rights Reserved.


#include "AnimationDrivenHelper.h"

TArray<UAnimNotify*> UAnimationDrivenHelper::GetCurrentNotifies(ACharacter* Character, float OffsetSeconds,
	float Duration, TSubclassOf<UAnimNotify> NotifyClass)
{ 
	TArray<UAnimNotify*> Notifies = GetNotifiesInTimeFrame<UAnimNotify>(Character, OffsetSeconds, Duration);

	for (int i = Notifies.Num() - 1; i >= 0; --i)
	{
		if (!Notifies[i]->IsA(NotifyClass))
		{
			Notifies.RemoveAt(i);
		}
	}

	return Notifies;
}

TArray<UAnimNotifyState*> UAnimationDrivenHelper::GetCurrentNotifyStates(ACharacter* Character, float OffsetSeconds,
	float Duration, TSubclassOf<UAnimNotifyState> NotifyStateClass)
{
	TArray<UAnimNotifyState*> Notifies = GetNotifiesInTimeFrame<UAnimNotifyState>(Character, OffsetSeconds, Duration);

	for (int i = Notifies.Num() - 1; i >= 0; --i)
	{
		if (!Notifies[i]->IsA(NotifyStateClass))
		{
			Notifies.RemoveAt(i);
		}
	}

	return Notifies;
}
