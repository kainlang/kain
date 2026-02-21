// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "KismetNodes/SGraphNodeK2Base.h"

class SGraphPin;
class USMGraphK2Node_RuntimeNode_Base;

/**
 * Slate representation of any entry K2 node, such as OnStateBegin or CanEnterTransition.
 */
class SGraphNode_ExecutionEntryNode : public SGraphNodeK2Base
{
public:
	SLATE_BEGIN_ARGS(SGraphNode_ExecutionEntryNode) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, USMGraphK2Node_RuntimeNode_Base* InNode);

	// SGraphNode
	virtual TArray<FOverlayWidgetInfo> GetOverlayWidgets(bool bSelected, const FVector2D& WidgetSize) const override;
	// ~SGraphNode

private:
	TSharedPtr<SWidget> FastPathWidget;
};
