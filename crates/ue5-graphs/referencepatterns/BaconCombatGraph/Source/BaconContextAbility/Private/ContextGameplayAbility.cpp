// © 2025 mrbaconvn. All Rights Reserved.


#include "ContextGameplayAbility.h"
#include "ContextAbilitySystemComponent.h"

void UContextGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                    const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	bPayloadRetrieved = false;

	UContextAbilitySystemComponent* ASC = Cast<UContextAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());
	if (!IsValid(ASC))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
 
	TInstancedStruct<FAbilityContext> Payload;
	if (!ASC->PopAbilityContext(GetClass(), Payload))
	{
		ASC->OnAbilityPayloadListUpdate.AddUObject(this, &UContextGameplayAbility::OnAbilityPayloadListUpdate);
		return;
	}

	bPayloadRetrieved = true;
	StartAbility(Payload);
} 

void UContextGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	 
	if(bPayloadRetrieved)
	{
		return;
	}
	
	UContextAbilitySystemComponent* ASCManager = Cast<UContextAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());
	if (!IsValid(ASCManager))
	{
		return;
	}

	TInstancedStruct<FAbilityContext> Payload;
	ASCManager->PopAbilityContext(GetClass(), Payload, true);
	ASCManager->OnAbilityPayloadListUpdate.RemoveAll(this);
}

void UContextGameplayAbility::StartAbility_Implementation(const TInstancedStruct<FAbilityContext>& Context)
{
}

void UContextGameplayAbility::OnAbilityPayloadListUpdate(UContextAbilitySystemComponent* ASCManager)
{
	if(!IsValid(ASCManager))
	{
		K2_CancelAbility();
		return;
	}
	
	TInstancedStruct<FAbilityContext> Payload;
	if (!ASCManager->PopAbilityContext(GetClass(), Payload))
	{
		return;
	}

	ASCManager->OnAbilityPayloadListUpdate.RemoveAll(this);
	
	bPayloadRetrieved = true;
	StartAbility(Payload);
} 