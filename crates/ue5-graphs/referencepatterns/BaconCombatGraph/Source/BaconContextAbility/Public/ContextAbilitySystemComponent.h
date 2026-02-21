// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AbilityContext.h"

#include "Runtime/Launch/Resources/Version.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
#include "StructUtils/InstancedStruct.h"
#else 
#include "InstancedStruct.h"
#endif

#include "ContextAbilitySystemComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnAbilityPayloadListUpdate, UContextAbilitySystemComponent*);
 
struct FGameplayTagContainer; 

UCLASS(ClassGroup=AbilitySystem, meta=(BlueprintSpawnableComponent))
class BACONCONTEXTABILITY_API UContextAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

	friend class UContextGameplayAbility;
	
public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	UFUNCTION(BlueprintCallable, Category = "Context Ability System")
	bool TryActivateContextAbility(const TInstancedStruct<FAbilityContext>& Payload);

protected:
	bool PopAbilityContext(TSubclassOf<UContextGameplayAbility> InAbility, TInstancedStruct<FAbilityContext>& OutContext, bool bInsistingRemoval = false); 
	
	UFUNCTION(Server, Reliable)
	void Server_UpdateContext(TInstancedStruct<FAbilityContext> InContext);

	UFUNCTION(Client, Reliable)
	void Client_UpdateContext(TInstancedStruct<FAbilityContext> InContext);

	UFUNCTION()
	void OnAbilityFailed(const UGameplayAbility* Ability, const FGameplayTagContainer& Tags);
	
public: 
	FOnAbilityPayloadListUpdate OnAbilityPayloadListUpdate;
	
private:
	TArray<TSubclassOf<UContextGameplayAbility>> PendingRemoveContextAbilityList;
	TArray<TInstancedStruct<FAbilityContext>> AbilityContextList;
};
