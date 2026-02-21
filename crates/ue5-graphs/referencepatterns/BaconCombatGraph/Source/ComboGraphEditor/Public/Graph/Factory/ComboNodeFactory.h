// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraphUtilities.h"
#include "SGraphNode.h"

class COMBOGRAPHEDITOR_API SComboGraphNode : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SComboGraphNode) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphNode* InNode);

	virtual void GetNodeInfoPopups(FNodeInfoContext* Context, TArray<FGraphInformationPopupInfo>& Popups) const override;

	virtual const FSlateBrush* GetNodeBodyBrush() const;

	virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) override;

protected:
	virtual TSharedRef<SWidget> CreateNodeContentArea() override;
};

struct COMBOGRAPHEDITOR_API FComboNodeFactory : public FGraphPanelNodeFactory
{
	virtual TSharedPtr<class SGraphNode> CreateNode(class UEdGraphNode* InNode) const override;
};
