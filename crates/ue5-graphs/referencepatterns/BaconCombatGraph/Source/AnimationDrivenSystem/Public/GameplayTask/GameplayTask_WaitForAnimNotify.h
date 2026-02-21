// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTask.h"
#include "GameplayTask_WaitForAnimNotify.generated.h"

enum class EAnimNotifyState : uint8;
class UAnimNotifyData;
class UAnimSequenceBase;
 
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAnimNotifyReceived, const UAnimNotifyData*, NotifyData, EAnimNotifyState, NotifyState);

UCLASS(Blueprintable)
class ANIMATIONDRIVENSYSTEM_API UGameplayTask_WaitForAnimNotify : public UGameplayTask
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
