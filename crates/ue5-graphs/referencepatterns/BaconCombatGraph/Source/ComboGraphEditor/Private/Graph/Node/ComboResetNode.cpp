// © 2025 mrbaconvn. All Rights Reserved.


#include "Graph/Node/ComboResetNode.h"
#include "GraphData/NodeData/ComboResetNodeData.h"

#define LOCTEXT_NAMESPACE "ComboGraphSchema"
#define NODE_TITLE LOCTEXT("Node.ComboResetNode.Title", "Reset")
#define NODE_RESET "Back to root"
#define NODE_CREATION_TEXT LOCTEXT("Node.ComboResetNode.CreationText", "Create reset node")

FText UComboResetNode::GetNodeCreationText() const
{
	return NODE_CREATION_TEXT;
}

FText UComboResetNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NODE_TITLE;
}

FLinearColor UComboResetNode::GetNodeTitleColor() const
{
	return FLinearColor::FromSRGBColor(FColor::Purple);
}

FLinearColor UComboResetNode::GetPinColor() const
{
	return FLinearColor(FColor::Purple);
}

void UComboResetNode::CreateDefaultPins()
{
	CreateCustomPin(EEdGraphPinDirection::EGPD_Input, NODE_RESET);
}

void UComboResetNode::InitNodeData(UObject* Outer)
{
	NodeData = NewObject<UComboResetNodeData>(Outer);
}

void UComboResetNode::SetNodeData(UComboGraphNodeData* InNodeData)
{
	NodeData = Cast<UComboResetNodeData>(InNodeData);
}

UComboGraphNodeData* UComboResetNode::GetNodeData() const
{
	return NodeData;
}

#undef NODE_TITLE
#undef NODE_RESET
#undef NODE_CREATION_TEXT

#undef LOCTEXT_NAMESPACE


