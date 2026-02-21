// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimationDrivenComponent.h"
#include "GameplayTask_WaitForAnimNotify.h"
#include "GameplayTask_WaitForAnimDrivenData.generated.h"
 
UCLASS()
class ANIMATIONDRIVENSYSTEM_API UGameplayTask_WaitForAnimDrivenData : public UGameplayTask_WaitForAnimNotify
{
	GENERATED_BODY()
	
public:
	/**
	 * Wait for the specified AnimNotifyData to be received from the AnimationDrivenComponent
	 * any animation could trigger this task's delegate (use filter version if needed)
	 */
	UFUNCTION(BlueprintCallable, Category = "GameplayTasks", meta = (DisplayName="Wait for Animation Driven Notify", DefaultToSelf = "TaskOwner", BlueprintInternalUseOnly = "TRUE"))
	static UGameplayTask_WaitForAnimDrivenData* CreateTask(TScriptInterface<IGameplayTaskOwnerInterface> TaskOwner, TSubclassOf<UAnimNotifyData> DataClass, bool bShouldTick = false);

	/**
	 *  Wait for the specified AnimNotifyData to be received from the AnimationDrivenComponent
	 *  but it has to be fired from the same animation
	 */
	UFUNCTION(BlueprintCallable, Category = "GameplayTasks", meta = (DisplayName="Wait for Animation Driven Notify (Filter Animation)", DefaultToSelf = "TaskOwner", BlueprintInternalUseOnly = "TRUE"))
	static UGameplayTask_WaitForAnimDrivenData* CreateTask_FilterAnimation(TScriptInterface<IGameplayTaskOwnerInterface> TaskOwner, TSubclassOf<UAnimNotifyData> DataClass, UAnimSequenceBase* Animation, bool bShouldTick = false);

	UPROPERTY(BlueprintAssignable)
	FOnAnimDrivenNotifyReceived	OnAnimationNotifyReceived;

protected:
	virtual TSubclassOf<UAnimNotifyData> GetNotifyClass() const override;
	virtual void ProcessNotify(const UAnimSequenceBase* SourceAnim, const UAnimNotifyData* NotifyData, EAnimNotifyState NotifyState) override;

protected:
	UPROPERTY()
	TSubclassOf<UAnimNotifyData> DataClass;
};
