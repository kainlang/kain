// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SGraphPin.h"
#include "SMUnrealTypeDefs.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class SSMGraphPin_StateMachinePin : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SSMGraphPin_StateMachinePin) {}
	SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, UEdGraphPin* InPin)
	{
		SGraphPin::Construct(SGraphPin::FArguments(), InPin);
	}

protected:

	const FSlateBrush* GetPinBorder() const
	{
		return IsHovered()
			? FSMUnrealAppStyle::Get().GetBrush(TEXT("Graph.StateNode.Pin.BackgroundHovered"))
			: FSMUnrealAppStyle::Get().GetBrush(TEXT("Graph.StateNode.Pin.Background"));
	}
};
