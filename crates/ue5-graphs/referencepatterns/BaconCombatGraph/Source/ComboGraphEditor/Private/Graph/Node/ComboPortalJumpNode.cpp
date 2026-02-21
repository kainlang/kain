// © 2025 mrbaconvn. All Rights Reserved.

#include "Graph/Node/ComboPortalJumpNode.h"  
#include "GraphData/NodeData/ComboJumpNodeData.h" 

#define LOCTEXT_NAMESPACE "ComboGraphSchema"
#define NODE_TITLE LOCTEXT("Node.ComboPortalJumpNode.Title", "Jump node")
#define JUMP_PIN_NAME LOCTEXT("Node.ComboPortalJumpNode.JumpPin", "Jump to \"{0}\"")
#define NODE_CREATION_TEXT LOCTEXT("Node.ComboConditionNode.CreationText", "Create jump node")

FText UComboPortalJumpNode::GetNodeCreationText() const
{
	return NODE_CREATION_TEXT;
}

FText UComboPortalJumpNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NODE_TITLE;
}

FLinearColor UComboPortalJumpNode::GetNodeTitleColor() const
{
	return FLinearColor::FromSRGBColor(FColor::Black);
}

void UComboPortalJumpNode::CreateDefaultPins()
{
	CreateCustomPin(EEdGraphPinDirection::EGPD_Input, UNINITIALIZED_PIN_NAME);
}

void UComboPortalJumpNode::InitNodeData(UObject* Outer)
{
	NodeData = NewObject<UComboJumpNodeData>(Outer);
}

void UComboPortalJumpNode::SetNodeData(UComboGraphNodeData* InNodeData)
{
	NodeData = Cast<UComboJumpNodeData>(InNodeData);
}

UComboGraphNodeData* UComboPortalJumpNode::GetNodeData() const
{
	return NodeData;
} 

void UComboPortalJumpNode::RebuildNode()
{
	UEdGraphPin* OutputPin = GetPinWithDirectionAt(0, EEdGraphPinDirection::EGPD_Input);
	if (OutputPin == nullptr)
	{
		return;
	}

	if (!IsValid(NodeData))
	{
		OutputPin->PinName = UNINITIALIZED_PIN_NAME;
		return;
	}

	FString FormattedString = FString::Format(*JUMP_PIN_NAME.ToString(), { NodeData->PortalTag.ToString() }); 
	OutputPin->PinName = FName(*FormattedString);
}

#undef NODE_TITLE
#undef JUMP_PIN_NAME
#undef NODE_CREATION_TEXT

#undef LOCTEXT_NAMESPACE