// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "ComboGraphInstance.generated.h"

class UComboGraphAsset;
class UComboGraphNodeData;
class UComboManagerComponent;

UENUM(BlueprintType)
enum class EComboResetReason : uint8
{
	RETRY,
	RESET,
	END_COMBO,
	COUNT,
};
 
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FComboInstanceInputWindowDelegate, bool, bIsInInputWindow);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FComboInstanceResetDelegate, EComboResetReason, ResetReason);

UCLASS(Abstract, Within = ComboManagerComponent)
class COMBOGRAPHRUNTIME_API UComboGraphInstance : public UObject
{
	GENERATED_BODY()

	friend class UComboManagerComponent;
	friend class UCombatAbility;
	
public:
	UFUNCTION(BlueprintCallable, Category = "Combo Graph|Combo Instance")
	virtual bool ResetInstance(EComboResetReason ResetReason = EComboResetReason::RESET);
	
	UFUNCTION(BlueprintPure, Category = "Combo Graph|Combo Instance")
	virtual bool IsValidInstance() const;
	
	UFUNCTION(BlueprintPure, Category = "Combo Graph|Combo Instance")
	UComboManagerComponent* GetManagerComponent() const; 
	
	UFUNCTION(BlueprintPure, Category = "Combo Graph|Combo Instance")
	const UComboGraphAsset* GetComboGraphAsset() const;
 
	UFUNCTION(BlueprintPure, Category = "Combo Graph|Combo Instance")
	const UComboGraphNodeData* GetCurrentNode() const;

	// INPUT
	
	UFUNCTION(BlueprintCallable, Category = "Combo Graph|Combo Instance", meta = (GameplayTagFilter = "ComboGraph.Input"))
	bool TriggerInput(FGameplayTag InInputTag);
	
	UFUNCTION(BlueprintPure, Category = "Combo Graph|Combo Instance")
	bool ConsumeCurrentInput(FGameplayTag& OutInputTag);

	UFUNCTION(BlueprintPure, Category = "Combo Graph|Combo Instance")
	bool GetInputHistory(FGameplayTag& OutInputTag, int InputHistoryIndex = 0) const;

	void SetSkipWindowConstraint(bool bSkip);
	
	// END INPUT
	
protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Combo Graph|Combo Instance")
	bool IsProceedBlocked() const;

	UFUNCTION(BlueprintNativeEvent, Category = "Combo Graph|Combo Instance")
	bool IsInInputWindow() const;
	
	/**
	 * This MUST be called before using the instance. 
	 */
	virtual bool ConstructInstance(UComboGraphAsset* InComboGraphAsset);
	virtual void BeginDestroy() override;
	virtual void SetInstanceActive(bool bActive);

	/**
	 * Delegate for when the graph is saved, we need to reset the instance
	 * so the instance graph can be updated
	 */
	UFUNCTION()
	void OnGraphChanged(UComboGraphAsset* InComboGraphAsset);
	
	virtual bool TryProceedGraph();
	virtual bool SetCurrentNode(const UComboGraphNodeData* ToNode);

protected:
	void SetInputWindowRequestCount(int32 InCount);
	void SetBlockProceedRequestCount(int32 InCount);

	FORCEINLINE int32 GetInputWindowRequestCount() const { return InputWindowRequestCount; }
	FORCEINLINE int32 GetBlockProceedRequestCount() const { return BlockProceedRequestCount; }
	
protected:
	UPROPERTY(BlueprintAssignable)
	FComboInstanceResetDelegate OnInstanceReset;

	UPROPERTY(BlueprintAssignable)
	FComboInstanceInputWindowDelegate OnInputWindowUpdate;

protected:
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Combo Graph|Combo Instance")
	uint8 bInstanceActive : 1 = false;
	
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Combo Graph|Combo Instance")
	uint8 bNeedProceed : 1 = false; 
	
	// Input
	
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Combo Graph|Input")
	uint8 bHasPendingInputWindow : 1 = false;
	
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Combo Graph|Input")
	uint8 bSkipWindowConstraint : 1 = true; 

	UPROPERTY(Transient, BlueprintReadWrite, Category = "Combo Graph|Input")
	FGameplayTag CurrentInput;
	
	UPROPERTY(Transient, BlueprintReadWrite, Category = "Combo Graph|Input")
	TArray<FGameplayTag> InputHistory;
	
private:
	int32 BlockProceedRequestCount = 0;
	int32 InputWindowRequestCount = 0;
	
	UPROPERTY()
	UComboManagerComponent* ManagerComponent = nullptr;
	
	UPROPERTY()
	UComboGraphAsset* ComboGraphAsset = nullptr;

	TWeakObjectPtr<const UComboGraphNodeData> CurrentNode = nullptr;
};