// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h" 
#include "AbilityTask_WaitForAnimNotify.h"
#include "AnimationDrivenComponent.h" 
#include "AbilityTask_WaitForAnimDrivenData.generated.h" 

UCLASS()
class COMBOGRAPHASC_API UAbilityTask_WaitForAnimDrivenData : public UAbilityTask_WaitForAnimNotify
{
	GENERATED_BODY()

public:
	/**
	 * Wait for the specified AnimNotifyData to be received from the AnimationDrivenComponent
	 * but it has to be the same animation that the ability played
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_WaitForAnimDrivenData* WaitForAnimDrivenData(UGameplayAbility* OwningAbility, TSubclassOf<UAnimNotifyData> DataClass, bool bShouldTick = false);

	/**
	 * Wait for the specified AnimNotifyData to be received from the AnimationDrivenComponent
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_WaitForAnimDrivenData* WaitForAnimDrivenData_FilterAnimation(UGameplayAbility* OwningAbility, TSubclassOf<UAnimNotifyData> DataClass, UAnimSequenceBase* Animation, bool bShouldTick = false);
	
	UPROPERTY(BlueprintAssignable)
	FOnAnimDrivenNotifyReceived	OnAnimationNotifyReceived;
	
protected:
	virtual TSubclassOf<UAnimNotifyData> GetNotifyClass() const override;
	virtual void ProcessNotify(const UAnimSequenceBase* SourceAnim, const UAnimNotifyData* NotifyData, EAnimNotifyState NotifyState) override;
	
protected:
	UPROPERTY()
	TSubclassOf<UAnimNotifyData> DataClass; 
};
