// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/BlueprintFunctionLibrary.h" 
#include "AnimationDrivenHelper.generated.h"
 
UCLASS()
class ANIMATIONDRIVENSYSTEM_API UAnimationDrivenHelper : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta=(DeterminesOutputType="NotifyClass"), Category="AnimationDriven|Helper")
	static TArray<UAnimNotify*> GetCurrentNotifies(ACharacter* Character, float OffsetSeconds, float Duration, TSubclassOf<UAnimNotify> NotifyClass);

	UFUNCTION( BlueprintCallable, meta=(DeterminesOutputType="NotifyStateClass"), Category="AnimationDriven|Helper")
	static TArray<UAnimNotifyState*> GetCurrentNotifyStates(ACharacter* Character, float OffsetSeconds, float Duration, TSubclassOf<UAnimNotifyState> NotifyStateClass);
	
	template <typename T>
	static TArray<T*> GetNotifiesInTimeFrame(ACharacter* Character, float OffsetSeconds, float Duration)
	{ 
		static_assert(TIsDerivedFrom<T, UAnimNotify>::Value || TIsDerivedFrom<T, UAnimNotifyState>::Value, 
			"T must be a subclass of UAnimNotify or UAnimNotifyState");

		TArray<T*> OutNotifies; 
		if (!IsValid(Character) || !IsValid(Character->GetMesh()))
		{
			return OutNotifies;
		}

		UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance();
		if (!IsValid(AnimInstance))
		{
			return OutNotifies;
		}

		for (const FAnimMontageInstance* MontageInstance : AnimInstance->MontageInstances)
		{
			if (!MontageInstance->IsActive())
			{
				continue;
			}

			float MontageDuration = MontageInstance->Montage->GetPlayLength() * MontageInstance->GetPlayRate();
			float StartPosition = MontageInstance->GetPosition() + OffsetSeconds / MontageDuration;
			float EndPosition = StartPosition + Duration / MontageDuration;

			for (const FAnimNotifyEvent& NotifyEvent : MontageInstance->Montage->Notifies)
			{ 
				if constexpr (TIsDerivedFrom<T, UAnimNotify>::Value)
				{ 
					if (IsValid(NotifyEvent.Notify) && NotifyEvent.Notify->IsA(T::StaticClass()))
					{
						if (NotifyEvent.GetTriggerTime() >= StartPosition && NotifyEvent.GetTriggerTime() <= EndPosition)
						{
							OutNotifies.Add(Cast<T>(NotifyEvent.Notify));
						} 
					}
				}
				else if constexpr (TIsDerivedFrom<T, UAnimNotifyState>::Value)
				{ 
					if (IsValid(NotifyEvent.NotifyStateClass) && NotifyEvent.NotifyStateClass->IsA(T::StaticClass()))
					{
						if (NotifyEvent.GetTriggerTime() <= StartPosition && NotifyEvent.GetEndTriggerTime() >= EndPosition)
						{
							OutNotifies.Add(Cast<T>(NotifyEvent.NotifyStateClass));
						} 
					}
				}
			}
		}

		return OutNotifies;
	} 
};
