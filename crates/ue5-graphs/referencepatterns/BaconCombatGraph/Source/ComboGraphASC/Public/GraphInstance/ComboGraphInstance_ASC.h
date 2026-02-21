// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h" 
#include "GraphInstance/ComboGraphInstance.h"
#include "ComboGraphInstance_ASC.generated.h"

class UAbilitySystemComponent;
class UASCComboManager;

UCLASS()
class COMBOGRAPHASC_API UComboGraphInstance_ASC : public UComboGraphInstance
{
	GENERATED_BODY()

	friend class UASCComboManager;
	
public:
	virtual void SetInstanceActive(bool bActive) override; 
	void SetAbilitySystemComponent(UAbilitySystemComponent* InASC);
	void OnBlockProceedGraph(FGameplayTag CallbackTag, int32 NewCount);
	void OnInputWindow(FGameplayTag CallbackTag, int32 NewCount); 

protected:
	TWeakObjectPtr<UAbilitySystemComponent> WeakASC;
};
