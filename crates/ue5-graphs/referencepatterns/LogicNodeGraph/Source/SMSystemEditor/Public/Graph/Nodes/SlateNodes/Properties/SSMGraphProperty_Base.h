// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

class SKismetLinearExpression;
class SBorder;
class SBox;
class SGraphNode;
class SGraphPin;
class UEdGraphNode;
class UEdGraphPin;
struct FSlateBrush;

/**
 * Base representation of an exposed graph property. Extend this to implement custom graph properties.
 */
class SMSYSTEMEDITOR_API SSMGraphProperty_Base : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SSMGraphProperty_Base)
		: _GraphNode(nullptr)
	{
	}

	/** Graph node containing the property. */
	SLATE_ARGUMENT(UEdGraphNode*, GraphNode)
	SLATE_END_ARGS()

	SSMGraphProperty_Base();

	virtual void Finalize() {}
	virtual void Refresh() {}
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;

	TWeakObjectPtr<UEdGraphNode> GetGraphNode() const { return GraphNode; }
	TSharedPtr<SGraphNode> FindParentGraphNode() const;
	UEdGraphPin* FindResultPin() const;

protected:
	TSharedRef<SBorder> MakeHighlightBorder();
	TSharedRef<SWidget> MakeNotifyIconWidget();

	EVisibility GetHighlightVisibility() const;
	FSlateColor GetHighlightColor() const;

	EVisibility GetNotifyVisibility() const;
	const FSlateBrush* GetNotifyIconBrush() const;
	FText GetNotifyIconTooltip() const;

protected:
	TWeakObjectPtr<UEdGraphNode> GraphNode;
	static constexpr float HighlightPadding = -6.f;
	static const FMargin NotifyPadding;
};
