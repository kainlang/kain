// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Graph/Nodes/SMGraphK2Node_StateMachineEntryNode.h"

#include "SMGraphK2Node_IntermediateRootStateMachineNodes.generated.h"

/**
 * This blueprint's root state machine start entry point
 */
UCLASS(MinimalAPI)
class USMGraphK2Node_IntermediateStateMachineStartNode : public USMGraphK2Node_RuntimeNodeReference
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode
	virtual bool CanUserDeleteNode() const override { return true; }
	virtual void AllocateDefaultPins() override;
	virtual ERedirectType DoPinsMatchForReconstruction(const UEdGraphPin* NewPin, int32 NewPinIndex, const UEdGraphPin* OldPin, int32 OldPinIndex) const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetMenuCategory() const override;
	virtual bool IsCompatibleWithGraph(UEdGraph const* Graph) const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual bool IsActionFilteredOut(class FBlueprintActionFilter const& Filter) override;
	virtual void PostPlacedNewNode() override;
	// ~UEdGraphNode

	// USMGraphK2Node_RuntimeNode_Base
	virtual bool IsCompatibleWithInstanceGraphNodeClass(TSubclassOf<USMGraphK2Node_FunctionNode_NodeInstance> InGraphNodeClass) const override;
	virtual bool IsConsideredForEntryConnection() const override { return true; }
	// ~USMGraphK2Node_RuntimeNode_Base
};

/**
 * When the blueprint's root state machine stops.
 */
UCLASS(MinimalAPI)
class USMGraphK2Node_IntermediateStateMachineStopNode : public USMGraphK2Node_RuntimeNodeReference
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode
	virtual bool CanUserDeleteNode() const override { return true; }
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetMenuCategory() const override;
	virtual bool IsCompatibleWithGraph(UEdGraph const* Graph) const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual bool IsActionFilteredOut(class FBlueprintActionFilter const& Filter) override;
	virtual void PostPlacedNewNode() override;
	// ~UEdGraphNode

	// USMGraphK2Node_RuntimeNode_Base
	virtual bool IsCompatibleWithInstanceGraphNodeClass(TSubclassOf<USMGraphK2Node_FunctionNode_NodeInstance> InGraphNodeClass) const override;
	virtual bool IsConsideredForEntryConnection() const override { return true; }
	// ~USMGraphK2Node_RuntimeNode_Base
};
