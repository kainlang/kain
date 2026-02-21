// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/Launch/Resources/Version.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
#include "StructUtils/InstancedStruct.h"
#else 
#include "InstancedStruct.h"
#endif
#include "Abilities/GameplayAbility.h"
#include "AbilityContext.h"
#include "ContextGameplayAbility.generated.h"

class UContextAbilitySystemComponent; 

UCLASS(Blueprintable)
class BACONCONTEXTABILITY_API UContextGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	
	UFUNCTION(BlueprintNativeEvent)
	void StartAbility(const TInstancedStruct<FAbilityContext>& Context);

	template <typename ContextType>
	bool ConvertContext(const TInstancedStruct<FAbilityContext>& Context, ContextType& OutContext)
	{ 
        static_assert(TIsDerivedFrom<ContextType, FAbilityContext>::IsDerived, "T must be derived from FAbilityContext");
		
		if (!Context.IsValid() || !Context.GetScriptStruct()->IsChildOf(ContextType::StaticStruct()))
		{
			return false;
		}

		OutContext = Context.Get<ContextType>();
		return true;
	}
 
protected:
	UFUNCTION()
	void OnAbilityPayloadListUpdate(UContextAbilitySystemComponent* ASCManager);

protected:
	bool bPayloadRetrieved = false;
};
