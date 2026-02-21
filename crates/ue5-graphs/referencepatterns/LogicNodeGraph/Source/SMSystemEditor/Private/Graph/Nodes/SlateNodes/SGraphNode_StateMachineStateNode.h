// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SGraphNode_StateNode.h"

class SGraphNode_StateMachineStateNode : public SGraphNode_StateNode
{
public:
	SLATE_BEGIN_ARGS(SGraphNode_StateMachineStateNode) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, USMGraphNode_StateNodeBase* InNode);

	// SGraphNode
	virtual TArray<FOverlayWidgetInfo> GetOverlayWidgets(bool bSelected, const FVector2D& WidgetSize) const override;
	// ~SGraphNode

protected:
	// SGraphNode_StateNode
	virtual const FSlateBrush* GetNameIcon() const override;
	virtual TSharedPtr<SVerticalBox> BuildComplexTooltip() override;
	virtual UEdGraph* GetGraphToUseForTooltip() const override;
	// ~SGraphNode_StateNode

	FReply OnIntermediateIconDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent);

protected:
	TSharedPtr<SWidget> IntermediateWidget;
	TSharedPtr<SWidget> WaitForEndStateWidget;
};