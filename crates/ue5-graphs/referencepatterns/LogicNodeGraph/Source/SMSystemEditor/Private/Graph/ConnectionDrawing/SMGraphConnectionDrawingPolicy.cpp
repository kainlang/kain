// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMGraphConnectionDrawingPolicy.h"

#include "Graph/Nodes/SlateNodes/SGraphNode_TransitionEdge.h"
#include "Graph/Nodes/SMGraphNode_StateMachineEntryNode.h"
#include "Graph/Nodes/SMGraphNode_StateNodeBase.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"

#include "Rendering/DrawElements.h"

FSMGraphConnectionDrawingPolicy::FSMGraphConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float ZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj)
	: FConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, ZoomFactor, InClippingRect, InDrawElements)
	, GraphObj(InGraphObj)
{
	
}

void FSMGraphConnectionDrawingPolicy::DetermineWiringStyle(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, /*inout*/ FConnectionParams& Params)
{
	Params.AssociatedPin1 = OutputPin;
	Params.AssociatedPin2 = InputPin;
	Params.WireThickness = 1.5f;

	if (InputPin)
	{
		if (USMGraphNode_TransitionEdge* TransitionNode = Cast<USMGraphNode_TransitionEdge>(InputPin->GetOwningNode()))
		{
			TransitionNode->bIsHoveredByUser = HoveredPins.Num() > 0;
			
			if (USMTransitionInstance* Instance = Cast<USMTransitionInstance>(TransitionNode->GetNodeTemplate()))
			{
				if (Instance->IsIconHidden())
				{
					// Only track hover information if the icon is hidden.
					// For now that's the only time we care about when the node was last hovered.
					
					if (HoveredPins.Num() > 0)
					{
						TransitionNode->TimeSinceHover = 0.f;
						TransitionNode->LastHoverTimeStamp = FDateTime::UtcNow();
					}
					else
					{
						const FDateTime CurrentTimeStamp = FDateTime::UtcNow();
						TransitionNode->TimeSinceHover = (CurrentTimeStamp - TransitionNode->LastHoverTimeStamp).GetTotalSeconds();
					}

					TransitionNode->bIsHoveredByUser = TransitionNode->TimeSinceHover < 0.5; // Half second buffer to be considered hovered.
				}
			}
			
			Params.WireColor = TransitionNode->GetTransitionColor(HoveredPins.Contains(InputPin));
			Params.bDrawBubbles = TransitionNode->WasEvaluating();
			// Transition connects same states -- special case drawing.
			if (TransitionNode->GetFromState() == TransitionNode->GetToState())
			{
				Params.bUserFlag2 = true;
			}
			
			if (TransitionNode->ShouldRunParallel())
			{
				Params.bUserFlag1 = true;
			}
		}
	}

	const bool bDeemphasizeUnhoveredPins = HoveredPins.Num() > 0;
	if (bDeemphasizeUnhoveredPins)
	{
		ApplyHoverDeemphasis(OutputPin, InputPin, /*inout*/ Params.WireThickness, /*inout*/ Params.WireColor);
	}
}

void FSMGraphConnectionDrawingPolicy::Draw(TMap<TSharedRef<SWidget>, FArrangedWidget>& InPinGeometries, FArrangedChildren& ArrangedNodes)
{
	// Build an acceleration structure to quickly find geometry for the nodes
	NodeWidgetMap.Empty();
	for (int32 NodeIndex = 0; NodeIndex < ArrangedNodes.Num(); ++NodeIndex)
	{
		FArrangedWidget& CurWidget = ArrangedNodes[NodeIndex];
		TSharedRef<SGraphNode> ChildNode = StaticCastSharedRef<SGraphNode>(CurWidget.Widget);
		NodeWidgetMap.Add(ChildNode->GetNodeObj(), NodeIndex);
	}

	FConnectionDrawingPolicy::Draw(InPinGeometries, ArrangedNodes);
}

FVector2D FSMGraphConnectionDrawingPolicy::ComputeSplineTangent(const FVector2D& Start, const FVector2D& End) const
{
	const FVector2D Delta = End - Start;
	const FVector2D NormDelta = Delta.GetSafeNormal();

	return NormDelta;
}

void FSMGraphConnectionDrawingPolicy::DetermineLinkGeometry(FArrangedChildren& ArrangedNodes, TSharedRef<SWidget>& OutputPinWidget,
	UEdGraphPin* OutputPin, UEdGraphPin* InputPin, FArrangedWidget*& StartWidgetGeometry, FArrangedWidget*& EndWidgetGeometry)
{
	if (USMGraphNode_StateMachineEntryNode* EntryNode = Cast<USMGraphNode_StateMachineEntryNode>(OutputPin->GetOwningNode()))
	{
		StartWidgetGeometry = PinGeometries->Find(OutputPinWidget);

		USMGraphNode_StateNodeBase* State = CastChecked<USMGraphNode_StateNodeBase>(InputPin->GetOwningNode());
		if (const int32* StateIndex = NodeWidgetMap.Find(State))
		{
			/*
			 * This used to be FindChecked, but there's an edge case that crashes...
			 * Create parent node, open node, create a new state, while the state is still being named press back on the mouse so you go to the parent
			 * Hover mouse over parent. It tries to draw the graph in a popup, but because the state hasn't been committed it doesn't exist yet.
			 * Presumably fails with references too.
			 */
			EndWidgetGeometry = &ArrangedNodes[*StateIndex];
		}
	}
	else if (USMGraphNode_TransitionEdge* EdgeNode = Cast<USMGraphNode_TransitionEdge>(InputPin->GetOwningNode()))
	{
		USMGraphNode_StateNodeBase* Start = EdgeNode->GetFromState(true);
		USMGraphNode_StateNodeBase* End = EdgeNode->GetToState(true);
		if (Start != nullptr && End != nullptr)
		{
			int32* StartNodeIndex = NodeWidgetMap.Find(Start);
			int32* EndNodeIndex = NodeWidgetMap.Find(End);
			if (StartNodeIndex && EndNodeIndex)
			{
				StartWidgetGeometry = &ArrangedNodes[*StartNodeIndex];
				EndWidgetGeometry = &ArrangedNodes[*EndNodeIndex];
			}
		}
	}
	else
	{
		StartWidgetGeometry = PinGeometries->Find(OutputPinWidget);

		if (TSharedPtr<SGraphPin>* pTargetWidget = PinToPinWidgetMap.Find(InputPin))
		{
			TSharedRef<SGraphPin> InputWidget = (*pTargetWidget).ToSharedRef();
			EndWidgetGeometry = PinGeometries->Find(InputWidget);
		}
	}

	// Cancel out if the widgets are both still being constructed. Prevents a flicker drawing the connections initially.
	if (StartWidgetGeometry && EndWidgetGeometry && StartWidgetGeometry->Widget->NeedsPrepass() && EndWidgetGeometry->Widget->NeedsPrepass())
	{
		// If the widgets are off screen we should try rendering it since the connection could still be visible.
		if (ClippingRect.ContainsPoint(StartWidgetGeometry->Geometry.GetAbsolutePosition()) ||
			ClippingRect.ContainsPoint(EndWidgetGeometry->Geometry.GetAbsolutePosition()))
		{
			// At least one widget is on screen and at least one widget is being constructed. Cancel the geometry out so it won't be rendered.
			StartWidgetGeometry = EndWidgetGeometry = nullptr;
		}
	}
}

void FSMGraphConnectionDrawingPolicy::DrawPreviewConnector(const FGeometry& PinGeometry, const FVector2D& StartPoint, const FVector2D& EndPoint, UEdGraphPin* Pin)
{
	FConnectionParams Params;
	DetermineWiringStyle(Pin, nullptr, /*inout*/ Params);

	const FVector2D SeedPoint = EndPoint;
	const FVector2D AdjustedStartPoint = FGeometryHelper::FindClosestPointOnGeom(PinGeometry, SeedPoint);

	DrawSplineWithArrow(AdjustedStartPoint, EndPoint, Params);
}

void FSMGraphConnectionDrawingPolicy::DrawSplineWithArrow(const FVector2D& StartAnchorPoint, const FVector2D& EndAnchorPoint, const FConnectionParams& Params)
{
	if (Params.bUserFlag2)
	{
		// Looping back to self.
		Internal_DrawCurvedLineWithArrow(StartAnchorPoint, Params);
	}
	else
	{
		// Connecting different points.
		Internal_DrawLineWithArrow(StartAnchorPoint, EndAnchorPoint, Params);
	}
}

void FSMGraphConnectionDrawingPolicy::DrawSplineWithArrow(const FGeometry& StartGeom, const FGeometry& EndGeom, const FConnectionParams& Params)
{
	// Get a reasonable seed point (halfway between the boxes)
	const FVector2D StartCenter = FGeometryHelper::CenterOf(StartGeom);

	// This is a curve connecting itself.
	if (Params.bUserFlag2)
	{
		// Look for a point diagonally down and right. We want the bottom right corner chosen.
		FVector2D StartAnchorPoint = FGeometryHelper::FindClosestPointOnGeom(StartGeom, StartCenter * 100);
		StartAnchorPoint.X -= 20.f * ZoomFactor; // Move to the left so its not exactly on the corner overlapping connected transitions.
		DrawSplineWithArrow(StartAnchorPoint, StartAnchorPoint, Params);

		return;
	}
	
	const FVector2D EndCenter = FGeometryHelper::CenterOf(EndGeom);
	const FVector2D SeedPoint = (StartCenter + EndCenter) * 0.5f;

	// Find the (approximate) closest points between the two boxes
	const FVector2D StartAnchorPoint = FGeometryHelper::FindClosestPointOnGeom(StartGeom, SeedPoint);
	const FVector2D EndAnchorPoint = FGeometryHelper::FindClosestPointOnGeom(EndGeom, SeedPoint);

	DrawSplineWithArrow(StartAnchorPoint, EndAnchorPoint, Params);
}

void FSMGraphConnectionDrawingPolicy::Internal_DrawLineWithArrow(const FVector2D& StartAnchorPoint, const FVector2D& EndAnchorPoint, const FConnectionParams& Params)
{
	const float LineSeparationAmount = 4.5f;

	const FVector2D DeltaPos = EndAnchorPoint - StartAnchorPoint;
	const FVector2D UnitDelta = DeltaPos.GetSafeNormal();
	const FVector2D Normal = FVector2D(DeltaPos.Y, -DeltaPos.X).GetSafeNormal();

	// Come up with the final start/end points
	const FVector2D DirectionBias = Normal * LineSeparationAmount;
	const FVector2D LengthBias = ArrowRadius.X * UnitDelta;
	const FVector2D StartPoint = StartAnchorPoint + DirectionBias + LengthBias;
	const FVector2D EndPoint = EndAnchorPoint + DirectionBias - LengthBias;

	const bool bIsParallel = Params.bUserFlag1;
	if (bIsParallel)
	{
		DrawConnection(WireLayerID, StartPoint - Normal * 2.5f, EndPoint - Normal * 2.5f, Params);
		DrawConnection(WireLayerID, StartPoint + Normal * 2.5f, EndPoint + Normal * 2.5f, Params);
	}
	else
	{
		DrawConnection(WireLayerID, StartPoint, EndPoint, Params);
	}
	Internal_DrawArrow(EndPoint, DeltaPos, Params);
}

void FSMGraphConnectionDrawingPolicy::Internal_DrawCurvedLineWithArrow(const FVector2D& StartAnchorPoint, const FConnectionParams& Params)
{
	const float MaxX = 45.f * ZoomFactor;
	const float MaxY = 45.f * ZoomFactor;

	const FVector P1(StartAnchorPoint.X, StartAnchorPoint.Y, 0.f);
	const FVector P2(StartAnchorPoint.X + MaxX, StartAnchorPoint.Y + MaxY, 0.f);
	const FVector P3(StartAnchorPoint.X - MaxX, StartAnchorPoint.Y + MaxY, 0.f);
	const FVector P4(StartAnchorPoint.X - 12.f * ZoomFactor, StartAnchorPoint.Y, 0.f);

	FVector ControlPoints[]{ P1, P2, P3, P4 };

	const int32 NumPoints = 12;

	TArray<FVector> CurvePoints;
	FVector::EvaluateBezier(ControlPoints, NumPoints, CurvePoints);

	for (int32 i = 1; i < NumPoints; ++i)
	{
		const FVector2D Start(CurvePoints[i - 1].X, CurvePoints[i - 1].Y);
		const FVector2D End(CurvePoints[i].X, CurvePoints[i].Y);

		DrawConnection(WireLayerID, Start, End, Params);
	}

	const FVector DeltaPos = CurvePoints[NumPoints - 1] - CurvePoints[NumPoints - 2];
	const FVector2D End(CurvePoints[NumPoints - 1].X, CurvePoints[NumPoints - 1].Y);

	Internal_DrawArrow(End, FVector2D(DeltaPos.X, DeltaPos.Y), Params);
}

void FSMGraphConnectionDrawingPolicy::Internal_DrawArrow(const FVector2D Pos, const FVector2D &DeltaPos, const FConnectionParams &Params)
{
	const FVector2D ArrowDrawPos = Pos - ArrowRadius;
	const float AngleInRadians = FMath::Atan2(DeltaPos.Y, DeltaPos.X);

	FSlateDrawElement::MakeRotatedBox(
		DrawElementsList,
		ArrowLayerID,
		FPaintGeometry(ArrowDrawPos, ArrowImage->ImageSize * ZoomFactor, ZoomFactor),
		ArrowImage,
		ESlateDrawEffect::None,
		AngleInRadians,
		TOptional<FVector2D>(),
		FSlateDrawElement::RelativeToElement,
		Params.WireColor
	);
}
