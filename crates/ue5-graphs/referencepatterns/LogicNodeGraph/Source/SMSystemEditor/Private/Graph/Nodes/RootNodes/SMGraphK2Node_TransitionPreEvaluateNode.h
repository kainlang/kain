// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Graph/Nodes/RootNodes/SMGraphK2Node_RuntimeNodeContainer.h"

#include "SMGraphK2Node_TransitionPreEvaluateNode.generated.h"

UCLASS(MinimalAPI)
class USMGraphK2Node_TransitionPreEvaluateNode final : public USMGraphK2Node_RuntimeNodeReference
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode
	virtual void AllocateDefaultPins() override;
	virtual void PostPlacedNewNode() override;
	virtual FText GetMenuCategory() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	/** Required to show up in BP right click context menu. */
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	/** Limit blueprints this shows up in. */
	virtual bool IsActionFilteredOut(class FBlueprintActionFilter const& Filter) override;
	virtual bool IsCompatibleWithGraph(UEdGraph const* Graph) const override;
	/** User can replace node. */
	virtual bool CanUserDeleteNode() const override { return true; }
	// ~UEdGraphNode

	// USMGraphK2Node_RuntimeNode_Base
	virtual bool IsConsideredForEntryConnection() const override { return true; }
	// ~USMGraphK2Node_RuntimeNode_Base
};
