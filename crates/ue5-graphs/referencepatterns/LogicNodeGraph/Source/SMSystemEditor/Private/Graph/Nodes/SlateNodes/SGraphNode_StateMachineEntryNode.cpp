// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SGraphNode_StateMachineEntryNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineEntryNode.h"

#include "SCommentBubble.h"
#include "SGraphPin.h"
#include "SMUnrealTypeDefs.h"
#include "Components/VerticalBox.h"

void SGraphNode_StateMachineEntryNode::Construct(const FArguments& InArgs, USMGraphNode_StateMachineEntryNode* InNode)
{
	this->GraphNode = InNode;

	this->SetCursor(EMouseCursor::CardinalCross);

	this->UpdateGraphNode();
}

void SGraphNode_StateMachineEntryNode::GetNodeInfoPopups(FNodeInfoContext* Context, TArray<FGraphInformationPopupInfo>& Popups) const
{

}

FSlateColor SGraphNode_StateMachineEntryNode::GetBorderBackgroundColor() const
{
	const FLinearColor InactiveStateColor(0.08f, 0.08f, 0.08f);
	return InactiveStateColor;
}

void SGraphNode_StateMachineEntryNode::UpdateGraphNode()
{
	InputPins.Empty();
	OutputPins.Empty();

	RightNodeBox.Reset();
	LeftNodeBox.Reset();

	this->ContentScale.Bind(this, &SGraphNode::GetContentScale);
	this->GetOrAddSlot(ENodeZone::Center)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBorder)
			.BorderImage(FSMUnrealAppStyle::Get().GetBrush("Graph.StateNode.Body"))
		.Padding(0)
		.BorderBackgroundColor(this, &SGraphNode_StateMachineEntryNode::GetBorderBackgroundColor)
		[
			SNew(SOverlay)

			// PIN AREA
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.Padding(10.0f)
		[
			SAssignNew(RightNodeBox, SVerticalBox)
		]
		]
		];

	CreatePinWidgets();
}

void SGraphNode_StateMachineEntryNode::AddPin(const TSharedRef<SGraphPin>& PinToAdd)
{
	PinToAdd->SetOwner(SharedThis(this));
	RightNodeBox->AddSlot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.FillHeight(1.0f)
		[
			PinToAdd
		];
	OutputPins.Add(PinToAdd);
}

FText SGraphNode_StateMachineEntryNode::GetPreviewCornerText() const
{
	return NSLOCTEXT("SGraphNodeStateEntry", "CornerTextDescription", "Entry point for state machine");
}
