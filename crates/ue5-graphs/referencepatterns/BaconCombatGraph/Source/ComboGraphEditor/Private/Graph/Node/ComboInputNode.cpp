// © 2025 mrbaconvn. All Rights Reserved.

#include "Graph/Node/ComboInputNode.h"  

#include "GameplayTagContainer.h"
#include "GraphData/NodeData/ComboInputNodeData.h"
#include "Framework/Commands/UIAction.h"

#define LOCTEXT_NAMESPACE "ComboGraphSchema"
#define NODE_TITLE LOCTEXT("Node.ComboInputNode.Title", "Input Branch")
#define NODE_CREATION_TEXT LOCTEXT("Node.ComboInputNode.CreationText", "Create input branch")

FText UComboInputNode::GetNodeCreationText() const
{
	return NODE_CREATION_TEXT;
}

FText UComboInputNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NODE_TITLE;
}

FLinearColor UComboInputNode::GetNodeTitleColor() const
{
	return FLinearColor::FromSRGBColor(FColor::Emerald);
}

void UComboInputNode::GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	Super::GetNodeContextMenuActions(Menu, Context);
}

void UComboInputNode::CreateDefaultPins()
{
	CreateCustomPin(EEdGraphPinDirection::EGPD_Input, INPUT_PIN_NAME);
	CreateCustomPin(EEdGraphPinDirection::EGPD_Output, DEFAULT_PIN_NAME);
}

TSharedRef<SWidget> UComboInputNode::CreateBottomNodeContent() const
{
	if (!IsValid(NodeData) || NodeData->UseInputHistory == false)
    {
        return SNullWidget::NullWidget;
    } 
	FString Text = FString::Printf(TEXT("Use previous input"));
    return SNew(SBox)
        .Padding(FMargin(15, 10)) 
        [ 
        	SNew(STextBlock)
				.Text(FText::FromString(Text))
				.ColorAndOpacity(GetNodeTitleColor())
				.AutoWrapText(true)
        ];
}

void UComboInputNode::InitNodeData(UObject* Outer)
{
	NodeData = NewObject<UComboInputNodeData>(Outer);
}

void UComboInputNode::SetNodeData(UComboGraphNodeData* InNodeData)
{
	NodeData = Cast<UComboInputNodeData>(InNodeData);
}

UComboGraphNodeData* UComboInputNode::GetNodeData() const
{
	return NodeData;
} 

void UComboInputNode::RebuildNode()
{
	if (!IsValid(NodeData))
	{
		return;
	}

	int NumOutputs = NodeData->InputTags.Num();
	const EEdGraphPinDirection DIRECTION = EEdGraphPinDirection::EGPD_Output;

	// Add one more for default pin
	ReservePins(DIRECTION, NumOutputs + 1);

	for (int i = 0; i < NumOutputs; i++)
	{
		UEdGraphPin* Pin = GetPinWithDirectionAt(i, DIRECTION);
		if (Pin == nullptr)
		{
			continue;
		}
		FName Name = UNINITIALIZED_PIN_NAME;

		if (NodeData->InputTags[i].IsValid())
		{
			FString TagName = NodeData->InputTags[i].ToString();
			// Replace the tag prefix with empty string
			TagName.RemoveFromStart(TEXT("ComboGraph.Input."));
			Name = FName(*TagName);
		}
		Pin->PinName = Name;
	}

	UEdGraphPin* DefaultPin = GetPinWithDirectionAt(NumOutputs, DIRECTION);
	DefaultPin->PinName = DEFAULT_PIN_NAME;
}

#undef NODE_TITLE
#undef NODE_CREATION_TEXT

#undef LOCTEXT_NAMESPACE