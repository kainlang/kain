// © 2025 mrbaconvn. All Rights Reserved.


#include "Graph/Node/ComboConditionNode.h"
#include "Action/ComboConditionPass.h"
#include "GraphData/NodeData/ComboConditionNodeData.h"

#define LOCTEXT_NAMESPACE "ComboGraphSchema"
#define NODE_TITLE LOCTEXT("Node.ComboConditionNode.Title", "Condition Node")
#define NODE_CREATION_TEXT LOCTEXT("Node.ComboConditionNode.CreationText", "Create condition branch")

FText UComboConditionNode::GetNodeCreationText() const
{
	return NODE_CREATION_TEXT;
}

FText UComboConditionNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (!IsValid(NodeInfo) || NodeInfo->ConditionPasses.Num() == 0)
	{
		return NODE_TITLE;
	}

	return UEnum::GetDisplayValueAsText(NodeInfo->ConditionType);
}

FLinearColor UComboConditionNode::GetNodeTitleColor() const
{
	return FLinearColor::Yellow;
}

void UComboConditionNode::GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	Super::GetNodeContextMenuActions(Menu, Context);
}

void UComboConditionNode::CreateDefaultPins()
{
	CreateCustomPin(EEdGraphPinDirection::EGPD_Input, INPUT_PIN_NAME);
	CreateCustomPin(EEdGraphPinDirection::EGPD_Output, TEXT("True"));
	CreateCustomPin(EEdGraphPinDirection::EGPD_Output, TEXT("False"));
}

TSharedRef<SWidget> UComboConditionNode::CreateBottomNodeContent() const
{
	if (!IsValid(NodeInfo) || NodeInfo->ConditionPasses.Num() == 0)
	{
		return SNullWidget::NullWidget;
	}

	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox); 
	VerticalBox->AddSlot()
		.AutoHeight()
		.Padding(FMargin(5, 5, 0, 0))
		[
			SNew(STextBlock)
				.Text(UEnum::GetDisplayValueAsText(NodeInfo->ConditionType))
				.ColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.1f))
				.AutoWrapText(true)
		];

	for (const TObjectPtr<UComboConditionPass>& Pass : NodeInfo->ConditionPasses)
	{
		if (!IsValid(Pass))
		{
			continue;
		}
		FString Name = Pass->GetPassInformation();
		VerticalBox->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0, 5, 0, 0))
			[
				SNew(SBorder)
					.Padding(FMargin(10, 5))
					.BorderBackgroundColor(FLinearColor(0.05f, 0.05f, 0.05f))
					.BorderImage(FCoreStyle::Get().GetBrush("ToolTip.BrightBackground"))
					[ 
						SNew(STextBlock)
							.Text(FText::FromString(Name))
							.ColorAndOpacity(FLinearColor::White)
							.AutoWrapText(true)
					]
			];
	}

	return SNew(SBox)
		.Padding(FMargin(10, 5))
		.WidthOverride(250)
		[
			VerticalBox
		];
}

void UComboConditionNode::InitNodeData(UObject* Outer)
{
	NodeInfo = NewObject<UComboConditionNodeData>(Outer);
}

void UComboConditionNode::SetNodeData(UComboGraphNodeData* InNodeData)
{
	NodeInfo = Cast<UComboConditionNodeData>(InNodeData);
}

UComboGraphNodeData* UComboConditionNode::GetNodeData() const
{
	return NodeInfo;
}

#undef NODE_TITLE
#undef NODE_CREATION_TEXT

#undef LOCTEXT_NAMESPACE