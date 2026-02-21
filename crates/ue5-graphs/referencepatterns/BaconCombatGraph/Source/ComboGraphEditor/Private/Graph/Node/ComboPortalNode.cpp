// © 2025 mrbaconvn. All Rights Reserved.

#include "Graph/Node/ComboPortalNode.h" 
#include "GraphData/NodeData/ComboPortalNodeData.h"
#include "GameplayTagContainer.h"

#define LOCTEXT_NAMESPACE "ComboGraphSchema"
#define NODE_TITLE LOCTEXT("Node.ComboPortalNode.Title", "Portal node") 
#define PORTAL_PIN_NAME LOCTEXT("Node.ComboPortalNode.InputPin", "\"{0}\" Portal")
#define NODE_CREATION_TEXT LOCTEXT("Node.ComboConditionNode.CreationText", "Create portal node")

FText UComboPortalNode::GetNodeCreationText() const
{
	return NODE_CREATION_TEXT;
}

FText UComboPortalNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NODE_TITLE;
}

FLinearColor UComboPortalNode::GetNodeTitleColor() const
{
	return FLinearColor::FromSRGBColor(FColor::Black);
}

void UComboPortalNode::GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	Super::GetNodeContextMenuActions(Menu, Context);
}

void UComboPortalNode::CreateDefaultPins()
{
	CreateCustomPin(EEdGraphPinDirection::EGPD_Output, DEFAULT_PIN_NAME);
}

void UComboPortalNode::InitNodeData(UObject* Outer)
{
	NodeData = NewObject<UComboPortalNodeData>(Outer);
}

void UComboPortalNode::SetNodeData(UComboGraphNodeData* InNodeData)
{
	NodeData = Cast<UComboPortalNodeData>(InNodeData);
}

UComboGraphNodeData* UComboPortalNode::GetNodeData() const
{
	return NodeData;
}

void UComboPortalNode::RebuildNode()
{
	UEdGraphPin* OutputPin = GetPinWithDirectionAt(0, EEdGraphPinDirection::EGPD_Output);
	if (OutputPin == nullptr)
	{
		return;
	}

	if (!IsValid(NodeData))
	{ 
		OutputPin->PinName = UNINITIALIZED_PIN_NAME;
		return;
	}

	FString FormattedString = FString::Format(*PORTAL_PIN_NAME.ToString(), {NodeData->PortalTag.ToString()});
	OutputPin->PinName = FName(*FormattedString);
}

#undef NODE_TITLE
#undef PORTAL_PIN_NAME
#undef NODE_CREATION_TEXT 

#undef LOCTEXT_NAMESPACE