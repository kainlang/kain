#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CombatGraphInstance.generated.h"

class UCombatGraphAsset;
class UCombatGraphNodeData;

UENUM(BlueprintType)
enum class ECombatGraphResetReason : uint8
{
	RETRY,
	RESET,
	END_GRAPH,
	COUNT,
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCombatGraphInstanceResetDelegate, ECombatGraphResetReason, ResetReason);

UCLASS(Abstract)
class GRAPHRUNTIMETEST_API UCombatGraphInstance : public UObject
{
	GENERATED_BODY()

public:
	// Instance lifecycle
	UFUNCTION(BlueprintCallable, Category = "CombatGraph|Instance")
	virtual bool ResetInstance(ECombatGraphResetReason ResetReason = ECombatGraphResetReason::RESET);

	UFUNCTION(BlueprintPure, Category = "CombatGraph|Instance")
	virtual bool IsValidInstance() const;

	UFUNCTION(BlueprintPure, Category = "CombatGraph|Instance")
	const UCombatGraphAsset* GetGraphAsset() const;

	// Node access
	UFUNCTION(BlueprintPure, Category = "CombatGraph|Instance")
	const UCombatGraphNodeData* GetCurrentNode() const;

protected:
	// Construction and destruction
	virtual bool ConstructInstance(UCombatGraphAsset* InGraphAsset);
	virtual void BeginDestroy() override;
	virtual void SetInstanceActive(bool bActive);

	// Graph traversal
	virtual bool TryProceedGraph();
	virtual bool SetCurrentNode(const UCombatGraphNodeData* ToNode);

	// Blueprint events
	UFUNCTION(BlueprintNativeEvent, Category = "CombatGraph|Instance")
	bool IsProceedBlocked() const;

protected:
	// Delegates
	UPROPERTY(BlueprintAssignable)
	FCombatGraphInstanceResetDelegate OnInstanceReset;

	// State flags
	UPROPERTY(Transient, BlueprintReadOnly, Category = "CombatGraph|Instance")
	uint8 bInstanceActive : 1;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "CombatGraph|Instance")
	uint8 bNeedProceed : 1;

private:
	// Graph asset reference
	UPROPERTY()
	UCombatGraphAsset* GraphAsset;

	// Current node tracking
	TWeakObjectPtr<const UCombatGraphNodeData> CurrentNode;
};