// © 2025 mrbaconvn. All Rights Reserved.

#include "Character/ComboBasicCharacter.h"

#include "AbilitySystemComponent.h" 
#include "ContextAbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Runtime/Launch/Resources/Version.h"

AComboBasicCharacter::AComboBasicCharacter()
{
	AbilitySystemComponent = CreateDefaultSubobject<UContextAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);

	// https://teambaconn.github.io/unreal-post/animation-driven/
#ifdef WITH_SERVER_CODE
	#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3
		GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesAndRefreshBonesWhenPlayingMontages;
	#else
		GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;
	#endif
#else
	// Default client setting
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
#endif
}

void AComboBasicCharacter::BeginPlay()
{
	Super::BeginPlay(); 
}

void AComboBasicCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
 
	// Set the ASC on the Server. Clients do this in OnRep_PlayerState()
	InitAbilitySystemComponent();
}

void AComboBasicCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// Set the ASC for clients. Server does this in PossessedBy.
	InitAbilitySystemComponent();
}

UAbilitySystemComponent* AComboBasicCharacter::InitAbilitySystemComponent()
{
	if(bASCInitialized || !IsValid(AbilitySystemComponent))
	{
		return nullptr;
	}
	
	bASCInitialized = true;
	AbilitySystemComponent->InitAbilityActorInfo(this, this); 

	OnASCInitialized.Broadcast(AbilitySystemComponent);
	return AbilitySystemComponent;
}

bool AComboBasicCharacter::IsASCInitialized() const
{
	return bASCInitialized;
} 

UAbilitySystemComponent* AComboBasicCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}
