// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h" 
#include "ContextGameplayAbility.h"
#include "ComboAnimDrivenAbility.generated.h"

UCLASS()
class COMBOGRAPHASC_API UComboAnimDrivenAbility : public UContextGameplayAbility
{
	GENERATED_BODY()

public:
	UAnimMontage* GetCurrentMontage();
	
protected:
	virtual void StartAbility_Implementation(const TInstancedStruct<FAbilityContext>& Context) override; 
	
	UFUNCTION(BlueprintNativeEvent)
	void OnMontageEnded();
	
protected:
	UPROPERTY(Transient, BlueprintReadWrite, Category = "Animation")
	TObjectPtr<UAnimMontage> Montage; 
};
