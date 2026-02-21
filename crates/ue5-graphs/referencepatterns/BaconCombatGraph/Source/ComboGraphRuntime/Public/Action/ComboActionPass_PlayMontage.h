// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ComboActionPass.h"
#include "ComboActionPass_PlayMontage.generated.h"

class UAnimMontage;

UCLASS(DisplayName = "Play montage", meta = (ToolTip = "Play anim montage on the character"))
class COMBOGRAPHRUNTIME_API UComboActionPass_PlayMontage : public UComboActionPass
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Montage")
	UAnimMontage* MontageToPlay;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Montage")
	float PlayRate = 1.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Montage")
	FName SectionName = NAME_None;

public:
	virtual TArray<FSoftObjectPath> GetPreviewAssets() const override;
	virtual FString GetPassInformation_Implementation() const override;
	virtual bool ExecutePass_Implementation(UComboGraphInstance* GraphInstance) const override;
};