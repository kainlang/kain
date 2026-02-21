// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h" 
#include "ComboBasicCharacter.generated.h"

class UContextAbilitySystemComponent;
class UAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAbilitySystemComponentInitialized, UAbilitySystemComponent*, ASC);

UCLASS(Blueprintable)
class AComboBasicCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public: 
	AComboBasicCharacter();
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
	UFUNCTION(BlueprintPure, Category = "Ability System")
	bool IsASCInitialized() const;

protected: 
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

private:
	UAbilitySystemComponent* InitAbilitySystemComponent();

public:
	UPROPERTY(BlueprintAssignable, Category = "Ability System")
	FOnAbilitySystemComponentInitialized OnASCInitialized;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Ability System")
	UContextAbilitySystemComponent* AbilitySystemComponent = nullptr;

private:
	bool bASCInitialized = false; 
};

