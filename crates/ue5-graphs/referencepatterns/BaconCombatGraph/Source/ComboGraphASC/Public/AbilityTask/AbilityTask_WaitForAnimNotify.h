// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h" 
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_WaitForAnimNotify.generated.h"

class UAnimNotifyData;
class UAnimSequenceBase;

UCLASS(Abstract)
class COMBOGRAPHASC_API UAbilityTask_WaitForAnimNotify : public UAbilityTask
{
	GENERATED_BODY()

protected:
	virtual void ProcessNotify(const UAnimSequenceBase* SourceAnim, const UAnimNotifyData* NotifyData, EAnimNotifyState NotifyState);
	virtual TSubclassOf<UAnimNotifyData> GetNotifyClass() const;
	
protected:
	virtual void Activate() override; 
	virtual void OnDestroy(bool AbilityEnded) override;
	virtual void TickTask(float DeltaTime) override;

	UFUNCTION()
	void OnNotifyReceived(const UAnimSequenceBase* SourceAnim, const UAnimNotifyData* NotifyData, EAnimNotifyState NotifyState); 
	
protected:
	TArray<TWeakObjectPtr<const UAnimNotifyData>> TickNotifyDataList;
	TWeakObjectPtr<UAnimSequenceBase> FilterAnimSource;
};
