// © 2025 mrbaconvn. All Rights Reserved.


#include "Graph/Node/ComboRootNode.h"

#include "GraphData/NodeData/ComboRootNodeData.h"

#define LOCTEXT_NAMESPACE "ComboGraphSchema"
#define NODE_TITLE LOCTEXT("Node.ComboRootNode.Title", "Root")
#define NODE_BEGIN "Start combo"

FText UComboRootNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NODE_TITLE;
}

FLinearColor UComboRootNode::GetNodeTitleColor() const
{
	return FLinearColor::Green;
}

FLinearColor UComboRootNode::GetPinColor() const
{
	return FLinearColor(0.2f, 1.0f, 0.2f);
}

bool UComboRootNode::CanUserDeleteNode() const
{
	return false;
}

bool UComboRootNode::CanDuplicateNode() const
{
	return false;
}

void UComboRootNode::CreateDefaultPins()
{
	CreateCustomPin(EEdGraphPinDirection::EGPD_Output, NODE_BEGIN);
}

void UComboRootNode::InitNodeData(UObject* Outer)
{
	NodeData = NewObject<UComboRootNodeData>(Outer);
}

void UComboRootNode::SetNodeData(UComboGraphNodeData* InNodeData)
{
	NodeData = Cast<UComboRootNodeData>(InNodeData);
}

UComboGraphNodeData* UComboRootNode::GetNodeData() const
{
	return NodeData;
}

#undef NODE_TITLE
#undef NODE_BEGIN 

#undef LOCTEXT_NAMESPACE


