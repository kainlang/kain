// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GraphInstance/ComboGraphInstance.h"
#include "Components/ActorComponent.h" 
#include "ComboManagerComponent.generated.h"

class UComboGraphAsset;
class UComboGraphData;
class UComboGraphNodeData;
class UComboGraphInstance;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnComboNodeTriggered, const UComboGraphNodeData*); 
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnComboInstanceProceedEvent, UComboGraphInstance*, ComboGraphInstance, bool, bSuccess);  

USTRUCT(BlueprintType)
struct FComboGraphGrantParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combo Manager")
	TSoftObjectPtr<UComboGraphAsset> ComboGraphAsset = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combo Manager")
	TSoftClassPtr<UComboGraphInstance> GraphInstance = UComboGraphInstance::StaticClass();
};

UCLASS( ClassGroup=(ComboGraph), meta=(BlueprintSpawnableComponent), Blueprintable)
class COMBOGRAPHRUNTIME_API UComboManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:	 
	UComboManagerComponent();

	virtual void BeginPlay() override;
	
	// COMBO GRAPH
	
	UFUNCTION(BlueprintCallable, Category = "Combo Manager")
	bool GrantComboGraph(const FComboGraphGrantParams& GrantParams);

	UFUNCTION(BlueprintCallable, Category = "Combo Manager")
	bool RemoveComboGraph(TSoftObjectPtr<UComboGraphAsset> InComboGraphAsset);

	UFUNCTION(BlueprintCallable, Category = "Combo Manager")
	int GetGraphIndex(UComboGraphAsset* InComboGraphAsset) const; 
	
	UFUNCTION(BlueprintCallable, Category = "Combo Manager")
	void SetComboInstanceActive(const UComboGraphInstance* InComboGraphInstance);

	UFUNCTION(BlueprintPure, Category = "Combo Manager")
	UComboGraphInstance* GetComboGraphInstanceByAsset(const TSoftObjectPtr<UComboGraphAsset>& InComboGraphAsset) const;
	
	UFUNCTION(BlueprintPure, Category = "Combo Manager")
	UComboGraphInstance* GetCurrentInstance(int OffsetIndex = 0) const;

	// END COMBO GRAPH

	// INPUT

	UFUNCTION(BlueprintCallable, Category = "Combo Manager", meta = (GameplayTagFilter = "ComboGraph.Input"))
	bool TriggerInput(FGameplayTag InInputTag);
	
	// END INPUT
	
protected:
	bool GrantDefaultComboGraphs();
	
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	UFUNCTION()
	void OnCurrentInstanceReset(EComboResetReason Reason);

	UFUNCTION()
	void OnCurrentInstanceInputWindowUpdate(bool bIsInInputWindow);
	
public: 
	FOnComboNodeTriggered OnComboNodeExecuted;
	
	UPROPERTY(BlueprintAssignable)
	FComboInstanceResetDelegate OnInstanceReset;
	
	UPROPERTY(BlueprintAssignable)
	FOnComboInstanceProceedEvent OnInstancePreProceed;
	
	UPROPERTY(BlueprintAssignable)
	FOnComboInstanceProceedEvent OnInstanceProceed;

	UPROPERTY(BlueprintAssignable)
	FComboInstanceInputWindowDelegate OnInputWindowUpdate;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combo Manager")
	TArray<FComboGraphGrantParams> DefaultComboGraphs;
	
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Combo Manager")
	TArray<UComboGraphInstance*> ComboGraphInstances;

	UPROPERTY(Transient)
	int ActiveGraphIndex = INDEX_NONE;
};
