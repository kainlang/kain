// © 2025 mrbaconvn. All Rights Reserved.

#include "Ability/ComboAnimDrivenAbility.h" 
#include "AbilitySystemComponent.h"   
#include "Abilities/Tasks/AbilityTask_WaitGameplayTag.h"
#include "Ability/ComboAbilityDataPayload.h"
#include "Animation/AnimMontage.h"
#include "AbilityTask/AbilityTask_AdvancePlayMontage.h" 

void UComboAnimDrivenAbility::StartAbility_Implementation(const TInstancedStruct<FAbilityContext>& Context)
{
	Super::StartAbility_Implementation(Context); 
 
	FAnimAbilityDataPayload AnimData;
	if(!ConvertContext<FAnimAbilityDataPayload>(Context, AnimData))
	{
		K2_CancelAbility();
		return;
	}
	
	Montage = AnimData.CombatMontage.LoadSynchronous();

	if(!IsValid(Montage))
	{
		K2_CancelAbility();
		return;
	}
 
	UAbilityTask_AdvancePlayMontage* PlayMontageTask =
		UAbilityTask_AdvancePlayMontage::CreatePlayMontageAdvance(this,
			FName("ComboAnimDrivenAbilityTask"),
			Montage,
			AnimData.PlayRate,
			AnimData.SectionName,
			true,
			AnimData.TranslationScale,
			0,
			false);

	PlayMontageTask->OnCompleted.AddDynamic(this, &UComboAnimDrivenAbility::OnMontageEnded);
	PlayMontageTask->OnInterrupted.AddDynamic(this, &UComboAnimDrivenAbility::OnMontageEnded);
	PlayMontageTask->OnCancelled.AddDynamic(this, &UComboAnimDrivenAbility::OnMontageEnded);
	PlayMontageTask->OnBlendOut.AddDynamic(this, &UComboAnimDrivenAbility::OnMontageEnded);
	
	PlayMontageTask->ReadyForActivation();

	// While playing the montage, we also check for activation blocked tags is still valid on the asc
	for (const FGameplayTag& Tag : ActivationBlockedTags)
	{
		UAbilityTask_WaitGameplayTagAdded* WaitTagTask = UAbilityTask_WaitGameplayTagAdded::WaitGameplayTagAdd(this, Tag, nullptr);
		WaitTagTask->Added.AddDynamic(this, &UComboAnimDrivenAbility::K2_CancelAbility);
		WaitTagTask->ReadyForActivation();
	}
}

UAnimMontage* UComboAnimDrivenAbility::GetCurrentMontage()
{
	return Montage;
}

void UComboAnimDrivenAbility::OnMontageEnded_Implementation()
{ 
	K2_EndAbility();
}
