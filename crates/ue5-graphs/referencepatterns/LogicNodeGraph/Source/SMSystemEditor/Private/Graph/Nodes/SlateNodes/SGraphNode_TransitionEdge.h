// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SGraphNode_BaseNode.h"

#include "Styling/SlateColor.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "SNodePanel.h"

class SToolTip;
class USMGraphNode_TransitionEdge;

class SSMTransitionShorthandView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSMTransitionShorthandView) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, USMGraphNode_TransitionEdge* InTransitionEdge);

private:
	USMGraphNode_TransitionEdge* TransitionEdge = nullptr;
};

/**
 * Represents transition connections and reroute nodes.
 */
class SGraphNode_TransitionEdge : public SGraphNode_BaseNode
{
public:
	SLATE_BEGIN_ARGS(SGraphNode_TransitionEdge){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, USMGraphNode_Base* InNode);
	// SGraphNode interface
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual void MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter, bool bMarkDirty = true) override;
	virtual bool RequiresSecondPassLayout() const override;
	virtual void PerformSecondPassLayout(const TMap< UObject*, TSharedRef<SNode> >& NodeToWidgetLookup) const override;
	virtual void UpdateGraphNode() override;
	virtual void CreatePinWidgets() override;
	virtual void AddPin(const TSharedRef<SGraphPin>& PinToAdd) override;
	virtual TSharedPtr<SToolTip> GetComplexTooltip() override;
	virtual const FSlateBrush* GetShadowBrush(bool bSelected) const override;
	// ~SGraphNode interface

	// SWidget interface
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	// ~SWidget interface

	// Calculate position for multiple nodes to be placed between a start and end point, by providing this nodes index and max expected nodes 
	void PositionBetweenTwoNodesWithOffset(const FGeometry& StartGeom, const FGeometry& EndGeom, int32 NodeIndex, int32 MaxNodes) const;

	/** Find the appropriate transition this represents. */
	USMGraphNode_TransitionEdge* GetTransition() const;

	/** Calculate total size used to represent reroute nodes. */
	static FVector2D GetTotalRerouteSpacerSize();

	static FVector2D GetRerouteNodeSpacerSize() { return FVector2D(16.0f, 16.f); }
	static float GetRerouteNodePadding() { return 10.f; }
	static float GetNameBorderPadding() { return 2.f; };

protected:
	FSlateColor GetEdgeColor(int32 InIndex) const;
	const FSlateBrush* GetIcon(int32 InIndex) const;
	EVisibility GetIconVisibility() const;
	FText GetPriorityAsText() const;
	EVisibility GetPriorityVisibility() const;
	EVisibility GetTransitionNameVisibility() const;
	EVisibility GetTransitionNameWarningVisibility() const;

	FReply OnTransitionNameMouseDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	/** True if the transition name has a possibility of changing at run-time. */
	bool CanTransitionNameChange() const;

	/** Our transition's desired size, accounting for extra room for priority display. */
	int32 GetDesiredYOffset() const;

	/** The name margin which should be used when the transition is displaying a name. */
	FMargin GetNameMargin() const;

private:
	mutable EVisibility CachedNameVisibility = EVisibility::Collapsed;
	mutable FVector2D CachedNameSize;
};
