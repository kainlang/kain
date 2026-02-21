// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ComboActionPass.h"
#include "ComboConditionPass.generated.h"
 
UCLASS(Abstract)
class COMBOGRAPHRUNTIME_API UComboConditionPass : public UComboActionPass
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action Pass")
	bool bNegate = false;
}; 
