// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Graph/Nodes/SlateNodes/Properties/SSMGraphProperty_Base.h"
#include "SMNodeWidgetInfo.h"

#include "Styling/SlateColor.h"

class UEdGraphNode;
class UEdGraphPin;
class SKismetLinearExpression;
class SBox;
class SGraphPin;

/**
 * Visual representation of an exposed graph property.
 */
class SSMGraphProperty : public SSMGraphProperty_Base
{
	SLATE_BEGIN_ARGS(SSMGraphProperty)
		: _GraphNode(nullptr)
		, _WidgetInfo(nullptr)
	{
	}

	/** Graph node containing the property. */
	SLATE_ARGUMENT(UEdGraphNode*, GraphNode)
	SLATE_ARGUMENT(const FSMTextDisplayWidgetInfo*, WidgetInfo)
	SLATE_END_ARGS()
	
	SSMGraphProperty();
	virtual ~SSMGraphProperty() override;
	
	void Construct(const FArguments& InArgs);
	virtual void Finalize() override;

	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

	virtual void Refresh() override;
	
protected:
	/** Validates that the drag drop event is allowed for this class. */
	bool IsDragDropValid(const FDragDropEvent& DragDropEvent) const;
	void HandleExpressionChange(UEdGraphPin* ResultPin);
	FSlateColor GetBackgroundColor() const;

protected:
	TSharedPtr<SKismetLinearExpression> ExpressionWidget;
	TSharedPtr<SBox> InputPinContainer;
	TWeakPtr<SGraphPin> InputPinPtr;
	
	FSMTextDisplayWidgetInfo WidgetInfo;
	bool bIsValidDragDrop;
};
