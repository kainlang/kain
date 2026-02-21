// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Action/ComboActionPass.h"
#include "ComboActionPass_ActivateGenericAbility.generated.h"

class UGameplayAbility;

UCLASS(DisplayName = "GAS activate ability", meta = (ToolTip = "Activate generic GAS ability"))
class COMBOGRAPHASC_API UComboActionPass_ActivateGenericAbility : public UComboActionPass
{
	GENERATED_BODY()

public:
	virtual TArray<FSoftObjectPath> GetPreviewAssets() const override;
	virtual FString GetPassInformation_Implementation() const override;
	virtual auto ExecutePass_Implementation(UComboGraphInstance* GraphInstance) const -> bool override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
	TSoftClassPtr<UGameplayAbility> AbilityClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
	bool bAutoGrant = true;  
};
