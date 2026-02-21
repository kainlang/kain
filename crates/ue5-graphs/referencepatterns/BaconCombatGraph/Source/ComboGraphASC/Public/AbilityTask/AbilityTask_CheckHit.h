// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilityTask_WaitForAnimNotify.h"
#include "GameplayTask/GameplayTask_CheckHit.h"
#include "AbilityTask_CheckHit.generated.h"

UCLASS()
class COMBOGRAPHASC_API UAbilityTask_CheckHit : public UAbilityTask_WaitForAnimNotify
{
	GENERATED_BODY()

public:
	UAbilityTask_CheckHit();
	
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_CheckHit* CheckHit(UGameplayAbility* OwningAbility, float DamageInterval = 0.0f, ECollisionChannel HitChannel = ECC_Pawn);
 
	UPROPERTY(BlueprintAssignable)
	FOnActorHit OnActorHit;
	
protected: 
	virtual TSubclassOf<UAnimNotifyData> GetNotifyClass() const override;
	virtual bool ShouldHitActor(AActor* HitActor) const;
	virtual void ProcessNotify(const UAnimSequenceBase* SourceAnim, const UAnimNotifyData* NotifyData, EAnimNotifyState NotifyState) override;

protected: 
	TMap<TWeakObjectPtr<AActor>, float> ActorLastHitTimes;
	TArray<ECollisionChannel> HitChannelList;
	float DamageInterval;
};