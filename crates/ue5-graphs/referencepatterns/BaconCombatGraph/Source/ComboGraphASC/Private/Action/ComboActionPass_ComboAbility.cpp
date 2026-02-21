// © 2025 mrbaconvn. All Rights Reserved.


#include "Action/ComboActionPass_ComboAbility.h"

#include "AbilitySystemGlobals.h"
#include "ContextAbilitySystemComponent.h"
#include "Ability/ComboAbilityDataPayload.h"
#include "ComboManager/ComboManagerComponent.h"
#include "AbilityContext.h" 
#include "ComboGraphHelper.h"
#include "GameFramework/Pawn.h"
#include "GraphInstance/ComboGraphInstance.h"

TArray<FSoftObjectPath> UComboActionPass_ComboAbility::GetPreviewAssets() const
{
	if(AbilityPayload.IsValid())
	{
		return AbilityPayload.Get<FCombatAbilityDataPayload>().GetPreviewAssets();
	}

	return Super::GetPreviewAssets();
}

FString UComboActionPass_ComboAbility::GetPassInformation_Implementation() const
{
	if(AbilityPayload.IsValid())
	{ 
		FCombatAbilityDataPayload GenericPayload = AbilityPayload.Get<FCombatAbilityDataPayload>();
		return GenericPayload.GetPreviewText();
	}
	return Super::GetPassInformation_Implementation();
}

bool UComboActionPass_ComboAbility::ExecutePass_Implementation(UComboGraphInstance* GraphInstance) const
{ 
	if (!IsValid(GraphInstance) || !GraphInstance->IsValidInstance())
	{
		return false;
	}

	APawn* Pawn = Cast<APawn>(GraphInstance->GetManagerComponent()->GetOwner());
	UContextAbilitySystemComponent* ASC = Cast<UContextAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn));
	if (!IsValid(ASC))
	{
		return false;
	}
 
	FCombatAbilityDataPayload ModifyPayload = AbilityPayload.Get<FCombatAbilityDataPayload>();
	ModifyPayload.Direction = UComboGraphHelper::GetPawnInputDirection(Pawn);
	
	TInstancedStruct<FAbilityContext> InstancedStruct;
	InstancedStruct.InitializeAs<FCombatAbilityDataPayload>(ModifyPayload); 
	
	return ASC->TryActivateContextAbility(InstancedStruct);
}
