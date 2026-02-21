// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h" 
#include "GraphData/ComboGraphData.h"
#include "ComboInputNodeData.generated.h"

struct FGameplayTag;
class UComboGraphNodeData;

UCLASS()
class COMBOGRAPHRUNTIME_API UComboInputNodeData : public UComboGraphNodeData
{
	GENERATED_BODY()
	
public:
	virtual const UComboGraphNodeData* ExecuteNode(UComboGraphInstance* GraphInstance) const override; 

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Node Data", meta = (Categories = "ComboGraph.Input"))
	TArray<FGameplayTag> InputTags;

	/**
	 * Keep this 0 if you want to consume the current input. Higher number for traversing the input history stack
	 * and will not consume the input since it's the history
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Node Data")
	uint8 UseInputHistory = 0;
};
