// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h" 
#include "GraphData/ComboGraphData.h"
#include "ComboConditionNodeData.generated.h"

class UComboConditionPass;
class UComboConditionNode;
class UComboManagerComponent;

UENUM(Blueprintable)
enum class EComboConditionType : uint8
{
	ESCCT_MATCH_ALL UMETA(DisplayName = "Match All"),
	ESCCT_MATCH_ANY UMETA(DisplayName = "Match Any"),
	ESCCT_MATCH_NONE UMETA(DisplayName = "Match None"),
	ESCCT_MAX UMETA(Hidden)
};

UCLASS()
class COMBOGRAPHRUNTIME_API UComboConditionNodeData : public UComboGraphNodeData
{
	GENERATED_BODY()

friend class UComboConditionNode;
	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Node Data")
	TArray<TObjectPtr<UComboConditionPass>> ConditionPasses;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Node Data")
	EComboConditionType ConditionType;

protected:
	EComboConditionType CheckCondition(UComboGraphInstance* GraphInstance) const;
	bool IsNodeMeetCondition(UComboGraphInstance* GraphInstance) const;

public:
	virtual const UComboGraphNodeData* ExecuteNode(UComboGraphInstance* GraphInstance) const override; 
};
