// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Graph/SMStateGraph.h"

#include "SMIntermediateGraph.generated.h"

class USMGraphK2Node_IntermediateEntryNode;

UCLASS(MinimalAPI)
class USMIntermediateGraph : public USMStateGraph
{
	GENERATED_UCLASS_BODY()

	// USMGraphK2
	virtual FSMNode_Base* GetRuntimeNode() const override;
	// ~USMGraphK2
	
public:
	UPROPERTY()
	TObjectPtr<USMGraphK2Node_IntermediateEntryNode> IntermediateEntryNode;
};
