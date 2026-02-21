// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ComboGraphInstance.h"
#include "ComboGraphInstance_Generic.generated.h"

UCLASS(Blueprintable)
class COMBOGRAPHRUNTIME_API UComboGraphInstance_Generic : public UComboGraphInstance
{
	GENERATED_BODY()
	
protected:
	UComboGraphInstance_Generic();

	virtual void SetInstanceActive(bool bActive) override;

	UFUNCTION(BlueprintCallable, Category = "Combo Graph|Combo Instance")
	void RequestInputWindow(bool bToggle);

	UFUNCTION(BlueprintCallable, Category = "Combo Graph|Combo Instance")
	void RequestBlockProceedGraph(bool bToggle); 
};
