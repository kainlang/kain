// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"
#include "AbilityContext.generated.h"

class UContextGameplayAbility; 

USTRUCT(BlueprintType, meta = (UsesHierarchy))
struct BACONCONTEXTABILITY_API FAbilityContext
{
	GENERATED_BODY()

public:
	virtual TArray<FSoftObjectPath> GetPreviewAssets() const;
	virtual FString GetPreviewText() const;
	virtual ~FAbilityContext() = default;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
	TSoftClassPtr<UContextGameplayAbility> AbilityClass;
};

