// © 2025 mrbaconvn. All Rights Reserved.


#include "AbilityAsync_ActivateContextAbilityAndWait.h"

#include "ContextAbilitySystemComponent.h"

UAbilityAsync_ActivateContextAbilityAndWait* UAbilityAsync_ActivateContextAbilityAndWait::
ActivateContextAbilityAndWait(AActor* TargetActor, const TInstancedStruct<FAbilityContext>& Context)
{
	UAbilityAsync_ActivateContextAbilityAndWait* MyObj = NewObject<UAbilityAsync_ActivateContextAbilityAndWait>();
	MyObj->SetAbilityActor(TargetActor);
	MyObj->AbilityContext = Context;

	return MyObj;
}

void UAbilityAsync_ActivateContextAbilityAndWait::OnAbilityEnd(const FAbilityEndedData& AbilityEndData)
{
	if(ShouldBroadcastDelegates())
	{
		OnAbilityEndDelegate.Broadcast( AbilityEndData.bWasCancelled );
	}
	EndAction();
} 

void UAbilityAsync_ActivateContextAbilityAndWait::Activate()
{
	Super::Activate();

	UContextAbilitySystemComponent* ASC = Cast<UContextAbilitySystemComponent>(GetAbilitySystemComponent());
	if (IsValid(ASC) && ASC->TryActivateContextAbility(AbilityContext))
	{
		// Listen for the ability to end
		ASC->OnAbilityEnded.AddUObject(this, &UAbilityAsync_ActivateContextAbilityAndWait::OnAbilityEnd);
	}
	else
	{
		OnAbilityEndDelegate.Broadcast(false);
		EndAction();
	}
}

void UAbilityAsync_ActivateContextAbilityAndWait::EndAction()
{
	UContextAbilitySystemComponent* ASC = Cast<UContextAbilitySystemComponent>(GetAbilitySystemComponent());
	if (IsValid(ASC))
	{
		ASC->OnAbilityEnded.RemoveAll(this);
	}
	
	Super::EndAction();
}
