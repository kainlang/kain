// © 2025 mrbaconvn. All Rights Reserved.

#include "GraphInstance/ComboGraphInstance_ASC.h"  

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "ComboGraphTags.h" 
#include "ComboManager/ComboManagerComponent.h" 

void UComboGraphInstance_ASC::SetInstanceActive(bool bActive)
{
	Super::SetInstanceActive(bActive);

	if(bActive)
	{
		SetAbilitySystemComponent(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetManagerComponent()->GetOwner()));
	}
	else
	{
		SetAbilitySystemComponent(nullptr);
	}
} 

void UComboGraphInstance_ASC::SetAbilitySystemComponent(UAbilitySystemComponent* InASC)
{
	if(WeakASC.IsValid())
	{
		UAbilitySystemComponent* ASC = WeakASC.Get();  
		ASC->RegisterGameplayTagEvent(ComboGraphTags::Block).RemoveAll(this); 
		ASC->RegisterGameplayTagEvent(ComboGraphTags::InputWindow).RemoveAll(this); 
		ASC->AbilityFailedCallbacks.RemoveAll(this);
	}
	
	WeakASC = InASC;
	if(WeakASC.IsValid())
	{
		UAbilitySystemComponent* ASC = WeakASC.Get();
		ASC->RegisterGameplayTagEvent(ComboGraphTags::Block).AddUObject(this, &UComboGraphInstance_ASC::OnBlockProceedGraph); 
		ASC->RegisterGameplayTagEvent(ComboGraphTags::InputWindow).AddUObject(this, &UComboGraphInstance_ASC::OnInputWindow);

		SetBlockProceedRequestCount(ASC->GetGameplayTagCount(ComboGraphTags::Block));
		SetInputWindowRequestCount(ASC->GetGameplayTagCount(ComboGraphTags::InputWindow));
	}
}

void UComboGraphInstance_ASC::OnBlockProceedGraph(const FGameplayTag CallbackTag, int32 NewCount)
{
	SetBlockProceedRequestCount(NewCount);
}

void UComboGraphInstance_ASC::OnInputWindow(const FGameplayTag CallbackTag, int32 NewCount)
{
	SetInputWindowRequestCount(NewCount);
}
 