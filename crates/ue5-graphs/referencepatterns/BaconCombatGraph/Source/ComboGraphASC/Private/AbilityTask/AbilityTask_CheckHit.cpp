// © 2025 mrbaconvn. All Rights Reserved.

#include "AbilityTask/AbilityTask_CheckHit.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h" 
#include "DamageShape/DamageNotifyData.h"
#include "Engine/World.h"

UAbilityTask_CheckHit::UAbilityTask_CheckHit()
{
	bTickingTask = true;
	DamageInterval = 0.f;
}

UAbilityTask_CheckHit* UAbilityTask_CheckHit::CheckHit(UGameplayAbility* OwningAbility, float DamageInterval,
	ECollisionChannel HitChannel)
{ 
	UAbilityTask_CheckHit* Task = NewAbilityTask<UAbilityTask_CheckHit>(OwningAbility); 
	Task->FilterAnimSource = OwningAbility->GetCurrentMontage();
	Task->DamageInterval = DamageInterval; 
	Task->HitChannelList.Add(HitChannel);
	
	return Task;
} 

TSubclassOf<UAnimNotifyData> UAbilityTask_CheckHit::GetNotifyClass() const
{
	return UDamageNotifyData::StaticClass();
}

bool UAbilityTask_CheckHit::ShouldHitActor(AActor* HitActor) const
{
	return true;
}

void UAbilityTask_CheckHit::ProcessNotify(const UAnimSequenceBase* SourceAnim, const UAnimNotifyData* NotifyData, EAnimNotifyState NotifyState)
{
	Super::ProcessNotify(SourceAnim, NotifyData, NotifyState);

	const UDamageNotifyData* DamageNotifyData = Cast<UDamageNotifyData>(NotifyData);
	if(!IsValid(DamageNotifyData))
	{
		return;
	}
 
	float CurrentTime = GetWorld()->GetTimeSeconds();
	
	TArray<FHitResult> HitResults = DamageNotifyData->TraceHit(GetAvatarActor(), HitChannelList);
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
