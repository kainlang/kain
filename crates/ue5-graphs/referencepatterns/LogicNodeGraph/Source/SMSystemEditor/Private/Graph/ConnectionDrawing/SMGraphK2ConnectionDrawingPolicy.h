// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "BlueprintConnectionDrawingPolicy.h"

class FSlateWindowElementList;
class UEdGraph;

class FSMGraphK2ConnectionDrawingPolicy : public FKismetConnectionDrawingPolicy
{
public:
	FSMGraphK2ConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float ZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj);

	// FKismetConnectionDrawingPolicy

	// If we want to make special debug displays for our logic graphs we can override methods here.

	// ~FKismetConnectionDrawingPolicy
};
