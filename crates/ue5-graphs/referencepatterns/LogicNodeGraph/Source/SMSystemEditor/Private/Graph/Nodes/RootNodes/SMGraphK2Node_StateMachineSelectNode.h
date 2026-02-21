// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Graph/Nodes/RootNodes/SMGraphK2Node_RootNode.h"

#include "SMGraphK2Node_StateMachineSelectNode.generated.h"

UCLASS(MinimalAPI)
class USMGraphK2Node_StateMachineSelectNode : public USMGraphK2Node_RootNode
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode
	virtual void AllocateDefaultPins() override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual bool IsNodePure() const override { return true; }
	virtual bool IsCompatibleWithGraph(UEdGraph const* Graph) const override;
	// ~UEdGraphNode

	// USMGraphK2Node_Base
	virtual bool CanCollapseNode() const override { return false; }
	virtual bool CanCollapseToFunctionOrMacro() const override { return false; }
	// ~USMGraphK2Node_Base
};
