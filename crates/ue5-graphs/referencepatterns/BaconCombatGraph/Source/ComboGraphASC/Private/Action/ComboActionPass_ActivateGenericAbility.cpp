// Fill out your copyright notice in the Description page of Project Settings.


#include "Action/ComboActionPass_ActivateGenericAbility.h" 

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbility.h"
#include "ComboManager/ComboManagerComponent.h"
#include "GraphInstance/ComboGraphInstance.h"

TArray<FSoftObjectPath> UComboActionPass_ActivateGenericAbility::GetPreviewAssets() const
{
	if (AbilityClass.IsNull())
	{
		return Super::GetPreviewAssets();
	}
 
	return { AbilityClass.GetUniqueID() };
}

FString UComboActionPass_ActivateGenericAbility::GetPassInformation_Implementation() const
{
	if (AbilityClass.IsNull())
	{
		return "Activate ability: NONE";
	}
	
	return FString::Printf(TEXT("Activate ability: %s \nAuto grant: %s"), *AbilityClass.GetAssetName(), bAutoGrant ? TEXT("true") : TEXT("false"));
}

bool UComboActionPass_ActivateGenericAbility::ExecutePass_Implementation(UComboGraphInstance* GraphInstance) const
{
	// If bAutoGrant is true, we will grant the ability to the ASC
	// Activate the ability
	if (!IsValid(GraphInstance) || AbilityClass.IsNull())
	{
		return false;
	}

	TSubclassOf<UGameplayAbility> AbilityToActivate = AbilityClass.LoadSynchronous();
	if (!IsValid(AbilityToActivate))
	{
		return false;
	}
	
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GraphInstance->GetManagerComponent()->GetOwner());
	if (!IsValid(ASC))
	{
		return false;
	}
	
	if (bAutoGrant && ASC->FindAbilitySpecFromClass(AbilityToActivate) == nullptr)
	{ 
		if (ASC->IsOwnerActorAuthoritative())
		{
			ASC->K2_GiveAbility(AbilityToActivate, 1, INDEX_NONE);
		}
		else
		{
			UE_LOG (LogTemp, Error, TEXT("Ability %s is not granted to ASC %s because it doesn't have authority"), *AbilityToActivate->GetName(), *ASC->GetName());
		}
	}

	bool bActivateSuccess = ASC->TryActivateAbilityByClass(AbilityToActivate);

	return bActivateSuccess;
} 