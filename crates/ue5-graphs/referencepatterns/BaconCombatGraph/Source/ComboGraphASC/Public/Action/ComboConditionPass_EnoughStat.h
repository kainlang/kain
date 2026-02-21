// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h" 
#include "Action/ComboConditionPass.h"
#include "ComboConditionPass_EnoughStat.generated.h" 

UCLASS(DisplayName = "Condition Stat Enough", meta = (ToolTip = "Check if the character has enough stat"))
class COMBOGRAPHASC_API UComboConditionPass_EnoughStat : public UComboConditionPass
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FGameplayAttribute Attribute;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	float ThresholdValue;

public:
	virtual FString GetPassInformation_Implementation() const override;
	virtual bool ExecutePass_Implementation(UComboGraphInstance* GraphInstance) const override;
};