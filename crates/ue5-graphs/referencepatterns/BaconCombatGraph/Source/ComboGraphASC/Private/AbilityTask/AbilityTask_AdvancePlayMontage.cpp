// © 2025 mrbaconvn. All Rights Reserved.

#include "AbilityTask/AbilityTask_AdvancePlayMontage.h"

#include "AbilitySystemGlobals.h"
#include "GameFramework/Character.h"

void UAbilityTask_AdvancePlayMontage::OnDestroy(bool AbilityEnded)
{ 
	if (Ability)
	{ 
		const bool bPlayingThisMontage = Ability && Ability->GetCurrentMontage() == MontageToPlay;
		if (bPlayingThisMontage && bStopWhenAbilityEnds)
		{
			// Reset AnimRootMotionTranslationScale since on the client it will clear the delegate before OnMontageBlendingOut is called
			// TODO: Do we need to do anything more than this?
			
			ACharacter* Character = Cast<ACharacter>(GetAvatarActor());
			if (Character && Character->GetLocalRole() == ROLE_AutonomousProxy && Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalPredicted)
			{
				Character->SetAnimRootMotionTranslationScale(1.f);
			}
		}
	}

	Super::OnDestroy(AbilityEnded);
}

UAbilityTask_AdvancePlayMontage* UAbilityTask_AdvancePlayMontage::CreatePlayMontageAdvance(
	UGameplayAbility* OwningAbility, FName TaskInstanceName, UAnimMontage* MontageToPlay, float Rate,
	FName StartSection, bool bStopWhenAbilityEnds, float AnimRootMotionTranslationScale, float StartTimeSeconds,
	bool bAllowInterruptAfterBlendOut)
{
	UAbilitySystemGlobals::NonShipping_ApplyGlobalAbilityScaler_Rate(Rate);

	UAbilityTask_AdvancePlayMontage* MyObj = NewAbilityTask<UAbilityTask_AdvancePlayMontage>(OwningAbility, TaskInstanceName);
	MyObj->MontageToPlay = MontageToPlay;
	MyObj->Rate = Rate;
	MyObj->StartSection = StartSection;
	MyObj->AnimRootMotionTranslationScale = AnimRootMotionTranslationScale;
	MyObj->bStopWhenAbilityEnds = bStopWhenAbilityEnds;
	MyObj->bAllowInterruptAfterBlendOut = bAllowInterruptAfterBlendOut;
	MyObj->StartTimeSeconds = StartTimeSeconds;

	return MyObj;
}
