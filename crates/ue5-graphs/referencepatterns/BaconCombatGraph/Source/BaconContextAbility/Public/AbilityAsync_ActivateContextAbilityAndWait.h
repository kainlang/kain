// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilityContext.h"
#include "Runtime/Launch/Resources/Version.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
#include "StructUtils/InstancedStruct.h"
#else 
#include "InstancedStruct.h"
#endif
#include "Abilities/Async/AbilityAsync.h"
#include "AbilityAsync_ActivateContextAbilityAndWait.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAbilityEndResult, bool, bWasCancelled);

UCLASS()
class BACONCONTEXTABILITY_API UAbilityAsync_ActivateContextAbilityAndWait : public UAbilityAsync
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	TInstancedStruct<FAbilityContext> AbilityContext;
	
	UPROPERTY(BlueprintAssignable)
	FOnAbilityEndResult OnAbilityEndDelegate;
 
 	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (DefaultToSelf = "TargetActor", BlueprintInternalUseOnly = "TRUE"))
 	static UAbilityAsync_ActivateContextAbilityAndWait* ActivateContextAbilityAndWait(AActor* TargetActor, const TInstancedStruct<FAbilityContext>& Context);

	UFUNCTION()
	void OnAbilityEnd(const FAbilityEndedData& AbilityEndData);

	virtual void Activate() override;
	virtual void EndAction() override;
};
