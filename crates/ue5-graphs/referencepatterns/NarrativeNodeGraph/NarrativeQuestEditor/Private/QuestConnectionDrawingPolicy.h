// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "ConnectionDrawingPolicy.h"

class FQuestGraphConnectionDrawingPolicy : public FConnectionDrawingPolicy
{

public:
	FQuestGraphConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj)
		: FConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements)
	{
		//Remove arrows from graph
		ArrowImage = nullptr;
		ArrowRadius = FVector2D(0.0f, 0.0f);
		TransitionColor = FLinearColor(0.92f, 0.72f, 0.2f, 1.f);
		TransitionTime = 1000.f;
	}

	void DetermineWiringStyle(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, /*inout*/ FConnectionParams& Params);

	FLinearColor TransitionColor;
	float TransitionTime; //in ms
};