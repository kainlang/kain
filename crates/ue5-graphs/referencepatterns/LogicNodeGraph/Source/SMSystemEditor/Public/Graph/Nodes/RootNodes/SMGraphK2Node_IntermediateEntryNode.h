// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Graph/Nodes/SMGraphK2Node_StateMachineEntryNode.h"

#include "SMGraphK2Node_IntermediateEntryNode.generated.h"

/**
 * State Start override for intermediate graphs.
 */
UCLASS(MinimalAPI)
class USMGraphK2Node_IntermediateEntryNode : public USMGraphK2Node_StateMachineEntryNode
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual bool IsCompatibleWithGraph(UEdGraph const* Graph) const override;
	// ~UEdGraphNode
};
