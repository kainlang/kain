// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GraphData/ComboGraphData.h" 
#include "ComboResetNodeData.generated.h"

class UComboGraphNodeData;

UCLASS()
class COMBOGRAPHRUNTIME_API UComboResetNodeData : public UComboGraphNodeData
{
	GENERATED_BODY()

public:
	virtual const UComboGraphNodeData* ExecuteNode(UComboGraphInstance* GraphInstance) const override;
};
