// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ComboAnimDrivenAbility.h"
#include "CombatAbility.generated.h"
 
UCLASS(Blueprintable)
class COMBOGRAPHASC_API UCombatAbility : public UComboAnimDrivenAbility
{
	GENERATED_BODY()

public:
	UCombatAbility();
	
protected:
	virtual void CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility) override;
};
