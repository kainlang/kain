// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilityTask_AdvancePlayMontage.generated.h"

/**
 * Ability task to play a montage and wait for it to finish, with additional bug fix
 * When montage is finished, it will reset the root motion translation scale
 */

UCLASS()
class COMBOGRAPHASC_API UAbilityTask_AdvancePlayMontage : public UAbilityTask_PlayMontageAndWait
{
	GENERATED_BODY()

protected:
	virtual void OnDestroy(bool AbilityEnded) override;

public:
	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta = (DisplayName="Play Montage And Wait Advance",
		HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_AdvancePlayMontage* CreatePlayMontageAdvance(UGameplayAbility* OwningAbility,
		FName TaskInstanceName, UAnimMontage* Montage, float PlayRate = 1.f, FName Section = NAME_None, bool bStopWhenEnds = true, float Scale = 1.f, float StartSeconds = 0.f, bool bInteruptWhenBlendOut = false);
};
