// © 2025 mrbaconvn. All Rights Reserved.


#include "Action/ComboConditionPass_EnoughStat.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Character.h"
#include "ComboManager/ComboManagerComponent.h"

bool UComboConditionPass_EnoughStat::ExecutePass_Implementation(UComboGraphInstance* GraphInstance) const
{
	if(!IsValid(GraphInstance) || !GraphInstance->IsValidInstance())
	{
		return false;
	}
	
	ACharacter* Character = Cast<ACharacter>(GraphInstance->GetManagerComponent()->GetOwner());
	if (!IsValid(Character) || !Attribute.IsValid())
	{
		return false;
	}

	if (auto ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Character))
	{ 
		bool bFound;
		auto Result = ASC->GetGameplayAttributeValue(Attribute, bFound);
		if (bFound)
		{
			return !bNegate ? Result >= ThresholdValue : Result < ThresholdValue;
		}
	}

	return false;
}

FString UComboConditionPass_EnoughStat::GetPassInformation_Implementation() const
{
	FString Value = FString::Printf(TEXT("%.1f"), ThresholdValue);
	if (FMath::IsNearlyEqual(ThresholdValue, static_cast<int>(ThresholdValue))) 
	{
		Value = FString::Printf(TEXT("%d"), static_cast<int>(ThresholdValue));
	} 

	return FString::Format(TEXT("{2} {1} {0}"), { Attribute.GetName(), Value, !bNegate ? "Enough" : "Not enough"});
}
