// Copyright Narrative Tools 2022. 

#include "DialogueConnectionDrawingPolicy.h"
#include "QuestSM.h"
#include "NarrativeDialogueSettings.h"
#include "DialogueGraphNode_NPC.h"
#include <EdGraph/EdGraphNode.h>
#include "DialogueEditorSettings.h"


FVector2D FDialogueGraphConnectionDrawingPolicy::ComputeSplineTangent(const FVector2D& Start, const FVector2D& End) const
{

	if (const UNarrativeDialogueSettings* DialogueSettings = GetDefault<UNarrativeDialogueSettings>())
	{
		const FVector2D DeltaPos = End - Start;
		const bool bGoingForward = DialogueSettings->bEnableVerticalWiring ? DeltaPos.Y >= 0.f : DeltaPos.X >= 0.0f;

		if (const UDialogueEditorSettings* DialogueEditorSettings = GetDefault<UDialogueEditorSettings>())
		{
			const float ClampedTensionX = FMath::Min<float>(FMath::Abs<float>(DeltaPos.X), bGoingForward ? DialogueEditorSettings->ForwardSplineHorizontalDeltaRange : DialogueEditorSettings->BackwardSplineHorizontalDeltaRange);
			const float ClampedTensionY = FMath::Min<float>(FMath::Abs<float>(DeltaPos.Y), bGoingForward ? DialogueEditorSettings->ForwardSplineVerticalDeltaRange : DialogueEditorSettings->BackwardSplineVerticalDeltaRange);

			FVector2D Result;

			if (bGoingForward)
			{
				Result = (ClampedTensionX * DialogueEditorSettings->ForwardSplineTangentFromHorizontalDelta) + (ClampedTensionY * DialogueEditorSettings->ForwardSplineTangentFromVerticalDelta);
			}
			else
			{
				Result = (ClampedTensionX * DialogueEditorSettings->BackwardSplineTangentFromHorizontalDelta) + (ClampedTensionY * DialogueEditorSettings->BackwardSplineTangentFromVerticalDelta);
			}

			if (DialogueSettings->bEnableVerticalWiring)
			{
				return Result.GetRotated(90.f);
			}
			else
			{
				return Result;
			}

		}


	}

	return FConnectionDrawingPolicy::ComputeSplineTangent(Start, End);

}

void FDialogueGraphConnectionDrawingPolicy::DetermineWiringStyle(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, /*inout*/ FConnectionParams& Params)
{
	Params.WireColor = FLinearColor::White;
	Params.WireThickness = 2.f;

	UEdGraphNode* OutNode = OutputPin ? OutputPin->GetOwningNode() : nullptr;
	UEdGraphNode* InNode = InputPin ? InputPin->GetOwningNode() : nullptr;

	if (const UNarrativeDialogueSettings* DialogueSettings = GetDefault<UNarrativeDialogueSettings>())
	{
		if (const UDialogueEditorSettings* DialogueEditorSettings = GetDefault<UDialogueEditorSettings>())
		{
			if (OutNode && InNode)
			{
				if (DialogueSettings->bEnableVerticalWiring ? OutNode->NodePosY > InNode->NodePosY : OutNode->NodePosX > InNode->NodePosX)
				{
					Params.WireColor = DialogueEditorSettings->BacklinkWireColor;
				}
			}
		}
	}
}

void FDialogueGraphConnectionDrawingPolicy::DrawConnection(int32 LayerId, const FVector2D& Start, const FVector2D& End, const FConnectionParams& Params)
{
	if (const UNarrativeDialogueSettings* DialogueSettings = GetDefault<UNarrativeDialogueSettings>())
	{
		const bool bLinkingBackwards = DialogueSettings->bEnableVerticalWiring ? Start.Y > End.Y : Start.X > End.X;

		//If we're linking backwards, actually draw two splines that meet halfway, to act like a reroute pin might.
		//Basically just makes backlinks beautiful instead of messy and hard to follow
		if (bLinkingBackwards)
		{
			FVector2D Halfway = (Start + End) / 2;

			if (DialogueSettings->bEnableVerticalWiring)
			{
				Halfway.X -= FMath::Min(150.f * ZoomFactor + (FMath::Abs(Start.Y - End.Y) / 2.f), 400.f);
			}
			else
			{
				Halfway.Y -= FMath::Min(150.f * ZoomFactor + (FMath::Abs(Start.X - End.X) / 2.f), 400.f);
			}

			//Draw backlinks with pins changed to make the reroute nodes style themselves a lot more nicely 
			FConnectionParams NewParam = Params;
			NewParam.EndDirection = EGPD_Output;
			NewParam.StartDirection = EGPD_Output;
			FConnectionDrawingPolicy::DrawConnection(LayerId, Halfway, Start, NewParam);
			NewParam.EndDirection = EGPD_Input;
			NewParam.StartDirection = EGPD_Input;
			FConnectionDrawingPolicy::DrawConnection(LayerId, End, Halfway, NewParam);
			
			//// Draw the arrow
			if (BacklinkImage != nullptr)
			{
				FVector2D ArrowPoint = Halfway - ArrowRadius * 2.f;

				const float Angle = DialogueSettings->bEnableVerticalWiring ? -HALF_PI : PI;

				FSlateDrawElement::MakeRotatedBox(
					DrawElementsList,
					ArrowLayerID,
					FPaintGeometry(ArrowPoint, (BacklinkImage->ImageSize * 2.f) * ZoomFactor, ZoomFactor),
					BacklinkImage,
					ESlateDrawEffect::None,
					Angle,
					TOptional<FVector2D>(),
					FSlateDrawElement::RelativeToElement,
					Params.WireColor
				);
			}
		}
		else
		{
			//Pivot for wires isnt correct so shift it, kinda hacky but works so meh
			if (DialogueSettings->bEnableVerticalWiring)
			{
				FConnectionDrawingPolicy::DrawConnection(LayerId, Start - FVector2D(12.f * ZoomFactor, 0.f), End + FVector2D(16.f * ZoomFactor, 0.f), Params);
			}
			else
			{
				FConnectionDrawingPolicy::DrawConnection(LayerId, Start, End, Params);
			}
		}
	}


}
