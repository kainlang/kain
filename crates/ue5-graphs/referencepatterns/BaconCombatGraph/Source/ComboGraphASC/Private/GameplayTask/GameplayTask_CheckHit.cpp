// © 2025 mrbaconvn. All Rights Reserved.

#include "GameplayTask/GameplayTask_CheckHit.h"
#include "DamageShape/DamageNotifyData.h" 
#include "Animation/AnimSequenceBase.h"
#include "GameFramework/Actor.h" 
#include "Engine/World.h"

UGameplayTask_CheckHit::UGameplayTask_CheckHit()
{
	bTickingTask = true;
	DamageInterval = 0.f;
}

UGameplayTask_CheckHit* UGameplayTask_CheckHit::CreateCheckHitTask(
	TScriptInterface<IGameplayTaskOwnerInterface> InTaskOwner, float InDamageInterval, ECollisionChannel HitChannel)
{
	UGameplayTask_CheckHit* Task = NewTask<UGameplayTask_CheckHit>(InTaskOwner); 
	Task->DamageInterval = InDamageInterval;
	Task->HitChannelList.Add(HitChannel);

	return Task;
}

TSubclassOf<UAnimNotifyData> UGameplayTask_CheckHit::GetNotifyClass() const
{
	return UDamageNotifyData::StaticClass();
}

bool UGameplayTask_CheckHit::ShouldHitActor(AActor* HitActor) const
{
	return true;
}

void UGameplayTask_CheckHit::ProcessNotify(const UAnimSequenceBase* SourceAnim, const UAnimNotifyData* NotifyData,
	EAnimNotifyState NotifyState)
{
	Super::ProcessNotify(SourceAnim, NotifyData, NotifyState);
	
	const UDamageNotifyData* DamageNotifyData = Cast<UDamageNotifyData>(NotifyData);
	if(!IsValid(DamageNotifyData))
	{
		return;
	}
 
	float CurrentTime = GetWorld()->GetTimeSeconds();
	
	TArray<FHitResult> HitResults = DamageNotifyData->TraceHit(GetOwnerActor(), HitChannelList);
	for (const FHitResult& HitResult : HitResults)
	{
		AActor* HitActor = HitResult.GetActor();
		if (!IsValid(HitActor))
		{
			continue;
		}

		if (!ShouldHitActor(HitActor))
		{
			ActorLastHitTimes.Add(HitActor, CurrentTime);
			continue;
		}
 
		float* LastHitTime = ActorLastHitTimes.Find(HitActor); 
		if (!LastHitTime || (DamageInterval > 0 && (CurrentTime - *LastHitTime) >= DamageInterval))
		{ 
			OnActorHit.Broadcast(HitActor, HitResult, NotifyData); 
			ActorLastHitTimes.Add(HitActor, CurrentTime);
		}
	}

	if (NotifyState == EAnimNotifyState::ENSD_END)
	{
		ActorLastHitTimes.Empty();
	}
}
