// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SGraphNode.h"

class USMGraphNode_Base;

class SGraphNode_BaseNode : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SGraphNode_BaseNode) {}
	SLATE_ARGUMENT(FMargin, ContentPadding)
	SLATE_END_ARGS()

	~SGraphNode_BaseNode();
	
	void Construct(const FArguments& InArgs, USMGraphNode_Base* InNode);

	// SGraphNode
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	// ~SGraphNode
	
	bool IsMouseOverNode() const { return bIsMouseOver; }
	virtual void OnRefreshRequested(USMGraphNode_Base* InNode, bool bFullRefresh);

protected:
	bool bIsMouseOver = false;

private:
	FDelegateHandle NodeRefreshHandle;
};
