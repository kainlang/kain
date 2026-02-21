// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Graph/Nodes/SMGraphK2Node_Base.h"

#include "SMGraphK2Node_RootNode.generated.h"

UCLASS(MinimalAPI)
class USMGraphK2Node_RootNode : public USMGraphK2Node_Base
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode
	virtual bool CanUserDeleteNode() const override { return false; }
	virtual bool CanDuplicateNode() const override { return false; }
	SMSYSTEMEDITOR_API virtual void PostPasteNode() override;
	SMSYSTEMEDITOR_API virtual void DestroyNode() override;
	SMSYSTEMEDITOR_API virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	SMSYSTEMEDITOR_API virtual FLinearColor GetNodeTitleColor() const override;
	SMSYSTEMEDITOR_API virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	// ~UEdGraphNode

	// USMGraphK2Node_Base
	virtual bool CanCollapseNode() const override { return true; }
	virtual bool CanCollapseToFunctionOrMacro() const override { return false; }
	// ~USMGraphK2Node_Base

protected:
	// If this node is in the process of being destroyed.
	bool bIsBeingDestroyed;

	// If this node can be placed more than once on the same graph.
	bool bAllowMoreThanOneNode;
};
