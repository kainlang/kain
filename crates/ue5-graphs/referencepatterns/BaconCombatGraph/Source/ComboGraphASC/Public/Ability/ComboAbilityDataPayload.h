// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilityContext.h"
#include "Engine/NetSerialization.h"
#include "ComboAbilityDataPayload.generated.h" 

class UAnimMontage; 

USTRUCT(BlueprintType)
struct COMBOGRAPHASC_API FAnimAbilityDataPayload : public FAbilityContext
{
	GENERATED_BODY()

public:
	virtual FString GetPreviewText() const override;
	virtual TArray<FSoftObjectPath> GetPreviewAssets() const override;
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TSoftObjectPtr<UAnimMontage> CombatMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	float PlayRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	FName SectionName = NAME_None;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	float TranslationScale = 1.0f;
};

USTRUCT(BlueprintType)
struct COMBOGRAPHASC_API FCombatAbilityDataPayload : public FAnimAbilityDataPayload
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, Transient, Category = "Combat")
	FVector_NetQuantize100 Direction;
};