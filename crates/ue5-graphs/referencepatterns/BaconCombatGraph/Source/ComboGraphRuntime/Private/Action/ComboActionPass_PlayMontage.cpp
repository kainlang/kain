// © 2025 mrbaconvn. All Rights Reserved.


#include "Action/ComboActionPass_PlayMontage.h"
#include "ComboManager/ComboManagerComponent.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"

TArray<FSoftObjectPath> UComboActionPass_PlayMontage::GetPreviewAssets() const
{
	if(IsValid(MontageToPlay))
	{
		return { FSoftObjectPath(MontageToPlay) };
	}
	return {};
}

FString UComboActionPass_PlayMontage::GetPassInformation_Implementation() const
{
	return FString::Format(TEXT("Play \"{0}\""), { IsValid(MontageToPlay) ? MontageToPlay->GetName() : "None" });
}

bool UComboActionPass_PlayMontage::ExecutePass_Implementation(UComboGraphInstance* GraphInstance) const
{
	if(!IsValid(GraphInstance) || !GraphInstance->IsValidInstance())
	{
		return false;
	}
	
	ACharacter* Character = Cast<ACharacter>(GraphInstance->GetManagerComponent()->GetOwner());
	if (!IsValid(Character))
	{
		return false;
	}

	if (MontageToPlay)
	{
		Character->PlayAnimMontage(MontageToPlay, PlayRate, SectionName);
		return true;
	}
	return false;
} 