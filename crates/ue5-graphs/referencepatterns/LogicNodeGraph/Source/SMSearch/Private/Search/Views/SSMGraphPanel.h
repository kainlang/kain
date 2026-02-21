// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SGraphPanel.h"

/** Custom implementation of SGraphPanel to access and override protected methods. */
class SSMGraphPanel : public SGraphPanel
{
public:
	// ~SGraphPanel
	virtual void AddGraphNode(const TSharedRef<SNode>& NodeToAdd) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	// ~SGraphPanel

	/** Focuses on just one node, removing all other nodes. */
	void ScopeToSingleNode(TWeakObjectPtr<UEdGraphNode> InGraphNode);

	/** The single node to display if one is selected. */
	TWeakObjectPtr<UEdGraphNode> GetScopedNode() const { return ScopedGraphNode; }

private:
	/** The single node to display if one is selected. */
	TWeakObjectPtr<UEdGraphNode> ScopedGraphNode;

	// Track the last bounds of the node to help with auto zooming.
	FVector2D LastMinCorner;
	FVector2D LastMaxCorner;

	/** User has mouse button pressed and is dragging the view. */
	bool bUserMovingView = false;
};
