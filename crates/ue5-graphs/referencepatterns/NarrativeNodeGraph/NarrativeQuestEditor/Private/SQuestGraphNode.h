// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Styling/SlateColor.h"
#include "Input/DragAndDrop.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "QuestGraphNode.h"
#include "SGraphNode.h"
#include "SGraphPin.h"

class NARRATIVEQUESTEDITOR_API SQuestGraphNode : public SGraphNode
{
public:

	SLATE_BEGIN_ARGS(SQuestGraphNode) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UQuestGraphNode* InNode);

	//~ Begin SGraphNode Interface
	virtual void CreatePinWidgets() override;
	virtual void UpdateGraphNode() override;
	virtual TSharedPtr<SToolTip> GetComplexTooltip() override;
	virtual void AddPin(const TSharedRef<SGraphPin>& PinToAdd) override;
	//~ End SGraphNode Interface

protected:

	FText GetNodeText() const;
	FText GetEventsText() const;
	FText GetNodeTitleText() const;
	FText GetNodeQuantityText() const;

	EVisibility GetEventsVis() const;
	EVisibility GetNodeTextVisibility() const;

	FSlateColor GetNodeTitleColor() const;
	FSlateColor GetEventsColor() const;

	FSlateColor GetBorderColor() const;

	TSharedPtr<SWidget> QuestNodeWidgetRef;

};

class SQuestGraphNodePin : public SGraphPin 
{

public:

	SLATE_BEGIN_ARGS(SQuestGraphNodePin) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InPin);

protected:

	//~ Begin SGraphPin Interface
	virtual FSlateColor GetPinColor() const override;
	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override;
	//~ End SGraphPin Interface

	const FSlateBrush* GetPinBorder() const;


};
