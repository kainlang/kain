// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SGraphNode.h"

class SGraphPin;
class USMGraphNode_StateMachineEntryNode;

class SGraphNode_StateMachineEntryNode : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SGraphNode_StateMachineEntryNode) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, USMGraphNode_StateMachineEntryNode* InNode);

	// SNodePanel::SNode
	virtual void GetNodeInfoPopups(FNodeInfoContext* Context, TArray<FGraphInformationPopupInfo>& Popups) const override;
	// ~SNodePanel::SNode

	// SGraphNode
	virtual void UpdateGraphNode() override;
	virtual void AddPin(const TSharedRef<SGraphPin>& PinToAdd) override;
	// ~SGraphNode

protected:
	FSlateColor GetBorderBackgroundColor() const;
	FText GetPreviewCornerText() const;
	
};
