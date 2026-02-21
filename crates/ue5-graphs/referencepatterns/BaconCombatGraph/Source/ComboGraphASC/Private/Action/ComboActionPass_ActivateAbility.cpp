// © 2025 mrbaconvn. All Rights Reserved.


#include "Action/ComboActionPass_ActivateAbility.h"

#include "AbilityContext.h"
#include "AbilitySystemGlobals.h"
#include "ContextAbilitySystemComponent.h" 
#include "ComboManager/ComboManagerComponent.h"
#include "GraphInstance/ComboGraphInstance.h"

bool UComboActionPass_ActivateAbility::ExecutePass_Implementation(UComboGraphInstance* GraphInstance) const
{
	if (!IsValid(GraphInstance) || !GraphInstance->IsValidInstance())
	{
		return false;
	}

	UContextAbilitySystemComponent* ASC = Cast<UContextAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GraphInstance->GetManagerComponent()->GetOwner()));
	if (!IsValid(ASC))
	{
		return false;
	}
	
	return ASC->TryActivateContextAbility(AbilityPayload);
}

TArray<FSoftObjectPath> UComboActionPass_ActivateAbility::GetPreviewAssets() const
{
	if (AbilityPayload.IsValid())
	{
		return AbilityPayload.Get<FAbilityContext>().GetPreviewAssets();
	}
	return Super::GetPreviewAssets();
}

FString UComboActionPass_ActivateAbility::GetPassInformation_Implementation() const
{
	if(AbilityPayload.IsValid())
	{
		FAbilityContext GenericPayload = AbilityPayload.Get<FAbilityContext>();
		return GenericPayload.GetPreviewText();
	}
	
	return Super::GetPassInformation_Implementation();
}
