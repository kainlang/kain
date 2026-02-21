// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMGraphK2ConnectionDrawingPolicy.h"

FSMGraphK2ConnectionDrawingPolicy::FSMGraphK2ConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float ZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj)
	: FKismetConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, ZoomFactor, InClippingRect, InDrawElements, InGraphObj)
{
}