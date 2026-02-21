// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameplayTask/GameplayTask_WaitForAnimNotify.h"
#include "GameplayTask_CheckHit.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnActorHit, AActor*, Actor, const FHitResult&, HitResult,
                                               const UAnimNotifyData*, NotifyData);

UCLASS()
class COMBOGRAPHASC_API UGameplayTask_CheckHit : public UGameplayTask_WaitForAnimNotify
{
	GENERATED_BODY()

public:
	UGameplayTask_CheckHit();
	
	UFUNCTION(BlueprintCallable, Category = "GameplayTasks", meta = (DisplayName="Check Hit", DefaultToSelf = "TaskOwner", BlueprintInternalUseOnly = "TRUE"))
	static UGameplayTask_CheckHit* CreateCheckHitTask(TScriptInterface<IGameplayTaskOwnerInterface> TaskOwner, float DamageInterval = 0.0f, ECollisionChannel HitChannel = ECC_Pawn);
	
protected:
	virtual TSubclassOf<UAnimNotifyData> GetNotifyClass() const override;
	virtual bool ShouldHitActor(AActor* HitActor) const;
	virtual void ProcessNotify(const UAnimSequenceBase* SourceAnim, const UAnimNotifyData* NotifyData, EAnimNotifyState NotifyState) override;

public:
	UPROPERTY(BlueprintAssignable)
	FOnActorHit OnActorHit;
	
protected: 
	TMap<TWeakObjectPtr<AActor>, float> ActorLastHitTimes;
	TArray<ECollisionChannel> HitChannelList;
	float DamageInterval;
};
