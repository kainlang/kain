// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SSMGraphPanel.h"

void SSMGraphPanel::AddGraphNode(const TSharedRef<SNode>& NodeToAdd)
{
	if (ScopedGraphNode.IsValid())
	{
		const TSharedRef<SGraphNode> GraphNode = StaticCastSharedRef<SGraphNode>(NodeToAdd);
		if (GraphNode->GetNodeObj() != ScopedGraphNode.Get())
		{
			return;
		}
	}

	SGraphPanel::AddGraphNode(NodeToAdd);
}

FReply SSMGraphPanel::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		bUserMovingView = true;
	}
	return SGraphPanel::OnMouseButtonDown(MyGeometry, MouseEvent);
}

FReply SSMGraphPanel::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		bUserMovingView = false;
	}
	return SGraphPanel::OnMouseButtonUp(MyGeometry, MouseEvent);
}

void SSMGraphPanel::ScopeToSingleNode(TWeakObjectPtr<UEdGraphNode> InGraphNode)
{
	ScopedGraphNode = InGraphNode;
	RemoveAllNodes();
	AddNode(InGraphNode.Get(), NotUserAdded);

	PurgeVisualRepresentation();
	Update();

	LastMinCorner = LastMaxCorner = FVector2D();

	RegisterActiveTimer(0.1f, FWidgetActiveTimerDelegate::CreateLambda([this](double InCurrentTime, float InDeltaTime)
	{
		// Center after node layout has completed.. hack until a better solution can be found. If the layout can be created in a single frame
		// that could fix IsNodeCulled.
		if (!bUserMovingView)
		{
			CenterObject(ScopedGraphNode.Get());

			// Only stop once it's clear the node is no longer resizing due to layout changes.
			{
				FVector2D MinCorner;
				FVector2D MaxCorner;
				GetBoundsForNode(ScopedGraphNode.Get(), MinCorner, MaxCorner);
				if (MaxCorner.ComponentwiseAllGreaterThan(MinCorner) && LastMinCorner == MinCorner && LastMaxCorner == MaxCorner)
				{
					return EActiveTimerReturnType::Stop;
				}

				LastMinCorner = MinCorner;
				LastMaxCorner = MaxCorner;
			}
		}
		return EActiveTimerReturnType::Continue;
	}));
}
