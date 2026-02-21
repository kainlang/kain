// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "SGraphPin.h"
#include "SMUnrealTypeDefs.h"

class SSMGraphPin_StatePin : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SSMGraphPin_StatePin) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InPin)
	{
		this->SetCursor(EMouseCursor::Default);
		bShowLabel = true;

		GraphPinObj = InPin;
		check(GraphPinObj);

		const UEdGraphSchema* Schema = GraphPinObj->GetSchema();
		check(Schema);

		SBorder::Construct(SBorder::FArguments()
			.BorderImage(this, &SSMGraphPin_StatePin::GetPinBorder)
			.BorderBackgroundColor(this, &SSMGraphPin_StatePin::GetPinColor)
			.OnMouseButtonDown(this, &SSMGraphPin_StatePin::OnPinMouseDown)
			.Cursor(this, &SSMGraphPin_StatePin::GetPinCursor)
		);
	}

	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

protected:
	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override
	{
		return SNew(STextBlock);
	}

	const FSlateBrush* GetPinBorder() const
	{
		return IsHovered()
			? FSMUnrealAppStyle::Get().GetBrush(TEXT("Graph.StateNode.Pin.BackgroundHovered"))
			: FSMUnrealAppStyle::Get().GetBrush(TEXT("Graph.StateNode.Pin.Background"));
	}
};
