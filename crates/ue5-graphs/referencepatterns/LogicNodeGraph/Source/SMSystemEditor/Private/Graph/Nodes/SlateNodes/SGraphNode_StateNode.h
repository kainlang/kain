// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SGraphNode_BaseNode.h"

#include "Widgets/Images/SImage.h"

class USMGraphNode_ConduitNode;
class USMGraphNode_StateNodeBase;
class SSMGraphNode_PropertyContent;

class SGraphNode_StateNode : public SGraphNode_BaseNode
{
public:
	SLATE_BEGIN_ARGS(SGraphNode_StateNode) : _ContentPadding(FMargin(4.f, 0.f, 4.f, 0.f)) {}
	SLATE_ARGUMENT(FMargin, ContentPadding)
	SLATE_ARGUMENT(TOptional<float>, PinPadding)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, USMGraphNode_StateNodeBase* InNode);

	// SGraphNode
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual void MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter, bool bMarkDirty = true) override;
	virtual void UpdateGraphNode() override;
	virtual void CreatePinWidgets() override;
	virtual void AddPin(const TSharedRef<SGraphPin>& PinToAdd) override;
	virtual TSharedPtr<SToolTip> GetComplexTooltip() override;
	virtual TArray<FOverlayWidgetInfo> GetOverlayWidgets(bool bSelected, const FVector2D& WidgetSize) const override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void RequestRenameOnSpawn() override;
	virtual bool IsNameReadOnly() const override;
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	// ~SGraphNode

	// SNodePanel::SNode
	virtual void GetNodeInfoPopups(FNodeInfoContext* Context, TArray<FGraphInformationPopupInfo>& Popups) const override;
	// ~SNodePanel::SNode

	// SGraphNode_BaseNode
	virtual void OnRefreshRequested(USMGraphNode_Base* InNode, bool bFullRefresh) override;
	// ~SGraphNode_BaseNode

protected:
	virtual TSharedPtr<SWidget> CreateContentBox();
	virtual FText GetNodeTitle() const;
	virtual FSlateColor GetBorderBackgroundColor() const;
	virtual const FSlateBrush* GetNameIcon() const;
	virtual TSharedPtr<SVerticalBox> BuildComplexTooltip();
	virtual UEdGraph* GetGraphToUseForTooltip() const;

	virtual void CalculateAnyStateImpact();
	virtual void CalculateLinkStateImpact();

protected:
	TArray<TSharedPtr<SWidget>> AnyStateImpactWidgets;
	TSharedPtr<SWidget> LinkStateImpactWidget;
	TSharedPtr<SWidget> FastPathWidget;
	TSharedPtr<SSMGraphNode_PropertyContent> PropertyContent;
	TSharedPtr<SImage> NodeIcon;
	FMargin ContentPadding;
	float PinPadding = 4.f;
	const int32 OverlayWidgetPadding = 20;
};

class SGraphNode_ConduitNode : public SGraphNode_StateNode
{
public:
	SLATE_BEGIN_ARGS(SGraphNode_ConduitNode) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, USMGraphNode_ConduitNode* InNode);
	// SNodePanel::SNode
	virtual void GetNodeInfoPopups(FNodeInfoContext* Context, TArray<FGraphInformationPopupInfo>& Popups) const override;
	// ~SNodePanel::SNode
protected:
	virtual const FSlateBrush* GetNameIcon() const override;
};