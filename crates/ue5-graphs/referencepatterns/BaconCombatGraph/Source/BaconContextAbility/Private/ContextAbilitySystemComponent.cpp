// © 2025 mrbaconvn. All Rights Reserved.

#include "ContextAbilitySystemComponent.h"
#include "GameFramework/Pawn.h"
#include "AbilityContext.h" 
#include "ContextGameplayAbility.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Controller.h"

void UContextAbilitySystemComponent::BeginPlay()
{
	Super::BeginPlay();
	
	AbilityFailedCallbacks.AddUObject(this, &UContextAbilitySystemComponent::OnAbilityFailed);
}

void UContextAbilitySystemComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	AbilityActivatedCallbacks.RemoveAll(this);
}

bool UContextAbilitySystemComponent::TryActivateContextAbility(const TInstancedStruct<FAbilityContext>& Payload)
{
	if(!Payload.IsValid())
	{
		return false;
	}

	FAbilityContext GenericPayload = Payload.Get<FAbilityContext>();
	if(GenericPayload.AbilityClass.IsNull())
	{
		return false;
	}

	TSubclassOf<UContextGameplayAbility> AbilityClass = GenericPayload.AbilityClass.LoadSynchronous();
	EGameplayAbilityNetExecutionPolicy::Type Policy = AbilityClass->GetDefaultObject<UContextGameplayAbility>()->GetNetExecutionPolicy();
	
	AbilityContextList.Add(Payload);
	bool bActivate = TryActivateAbilityByClass(AbilityClass, true);
	if(!bActivate)
	{
		TInstancedStruct<FAbilityContext> OutContext;
		PopAbilityContext( AbilityClass, OutContext );
		return false;
	}
	OnAbilityPayloadListUpdate.Broadcast(this);

	if(Policy != EGameplayAbilityNetExecutionPolicy::LocalOnly && Policy != EGameplayAbilityNetExecutionPolicy::ServerOnly)
	{
		AActor* Avatar = GetAvatarActor();
		AController* Controller = nullptr;

		if (APawn* Pawn = Cast<APawn>(Avatar))
		{
			Controller = Pawn->GetController();
		}
		else if (APlayerState* PS = Cast<APlayerState>(Avatar))
		{
			Controller = PS->GetOwner<AController>();
		}

		if (IsValid(Controller))
		{ 
			if (!Controller->HasAuthority())
			{
				Server_UpdateContext(Payload);
			}
			else if (!Controller->IsLocalController())
			{
				Client_UpdateContext(Payload);
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("UContextAbilitySystemComponent::TryActivateContextAbility - Avatar actor %s does not have a valid controller!"), *GetAvatarActor()->GetName());
			return false;
		}
	}
	
	return true;
}

bool UContextAbilitySystemComponent::PopAbilityContext(TSubclassOf<UContextGameplayAbility> InAbility,
                                                       TInstancedStruct<FAbilityContext>& OutContext, bool bInsistingRemoval)
{
	if(!IsValid(InAbility))
	{
		return false;
	}

	bool bRemoved = false;
	for(int i = 0; i < AbilityContextList.Num(); i++)
	{ 
		FAbilityContext GenericPayload = AbilityContextList[i].Get<FAbilityContext>();
		if(GenericPayload.AbilityClass.LoadSynchronous() == InAbility)
		{
			OutContext = AbilityContextList[i];

			// We don't need to broadcast OnAbilityPayloadListUpdate here because we are removing the payload
			AbilityContextList.RemoveAt(i);
			bRemoved = true;
			break;
		}
	}

	if(!bRemoved && bInsistingRemoval)
	{
		// If the ability is not found, add it to the pending remove list so we can remove it later
		PendingRemoveContextAbilityList.Add(InAbility);
	}

	return bRemoved;
} 

void UContextAbilitySystemComponent::Server_UpdateContext_Implementation(TInstancedStruct<FAbilityContext> InContext)
{
	FAbilityContext GenericPayload = InContext.Get<FAbilityContext>();
	for(int i = 0; i < PendingRemoveContextAbilityList.Num(); i++)
	{
		if(GenericPayload.AbilityClass.LoadSynchronous() == PendingRemoveContextAbilityList[i])
		{
			PendingRemoveContextAbilityList.RemoveAt(i);
			return;
		}
	}
	
	AbilityContextList.Add(InContext);
	OnAbilityPayloadListUpdate.Broadcast(this);
}

void UContextAbilitySystemComponent::Client_UpdateContext_Implementation(TInstancedStruct<FAbilityContext> InContext)
{
	FAbilityContext GenericPayload = InContext.Get<FAbilityContext>();
	for(int i = 0; i < PendingRemoveContextAbilityList.Num(); i++)
	{
		if(GenericPayload.AbilityClass.LoadSynchronous() == PendingRemoveContextAbilityList[i])
		{
			PendingRemoveContextAbilityList.RemoveAt(i);
			return;
		}
	}
	
	AbilityContextList.Add(InContext);
	OnAbilityPayloadListUpdate.Broadcast(this);
}


void UContextAbilitySystemComponent::OnAbilityFailed(const UGameplayAbility* Ability, const FGameplayTagContainer& Tags)
{
	TSubclassOf<UContextGameplayAbility> AbilityClass = Ability->GetClass();
	if(!IsValid(AbilityClass))
	{
		return;
	}

	TInstancedStruct<FAbilityContext> OutPayload;
	PopAbilityContext(AbilityClass, OutPayload, true);
}