// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SGraphNode_BaseNode.h"
#include "Graph/Nodes/SMGraphNode_Base.h"

#define LOCTEXT_NAMESPACE "SGraphBaseNode"

SGraphNode_BaseNode::~SGraphNode_BaseNode()
{
	if (NodeRefreshHandle.IsValid())
	{
		if (USMGraphNode_Base* SMGraphNode = Cast<USMGraphNode_Base>(GraphNode))
		{
			SMGraphNode->OnGraphNodeRefreshRequestedEvent.Remove(NodeRefreshHandle);
		}
	}
}

void SGraphNode_BaseNode::Construct(const FArguments& InArgs, USMGraphNode_Base* InNode)
{
	check(InNode);
	GraphNode = InNode;
	
	NodeRefreshHandle = InNode->OnGraphNodeRefreshRequestedEvent.AddSP(this, &SGraphNode_BaseNode::OnRefreshRequested);
}

void SGraphNode_BaseNode::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	SGraphNode::OnMouseEnter(MyGeometry, MouseEvent);
	bIsMouseOver = true;
}

void SGraphNode_BaseNode::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	SGraphNode::OnMouseLeave(MouseEvent);
	bIsMouseOver = false;
}

void SGraphNode_BaseNode::OnRefreshRequested(USMGraphNode_Base* InNode, bool bFullRefresh)
{
	UpdateGraphNode();
}

#undef LOCTEXT_NAMESPACE
