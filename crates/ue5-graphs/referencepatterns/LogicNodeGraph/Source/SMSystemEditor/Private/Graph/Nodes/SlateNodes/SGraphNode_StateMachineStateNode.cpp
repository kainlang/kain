// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SGraphNode_StateMachineStateNode.h"

#include "Blueprints/SMBlueprint.h"
#include "Blueprints/SMBlueprintGeneratedClass.h"
#include "Configuration/SMEditorSettings.h"
#include "Configuration/SMEditorStyle.h"
#include "Graph/Nodes/SMGraphNode_StateMachineParentNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineStateNode.h"
#include "Graph/Nodes/SMGraphNode_StateNodeBase.h"
#include "Graph/SMGraph.h"
#include "Graph/SMGraphK2.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "Components/VerticalBox.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "SCommentBubble.h"
#include "SGraphPin.h"
#include "SMUnrealTypeDefs.h"
#include "Widgets/Images/SImage.h"

#define LOCTEXT_NAMESPACE "SGraphStateMachineStateNode"

//////////////////////////////////////////////////////////////////////////
void SGraphNode_StateMachineStateNode::Construct(const FArguments& InArgs, USMGraphNode_StateNodeBase* InNode)
{
	const USMEditorSettings* EditorSettings = FSMBlueprintEditorUtils::GetEditorSettings();
	SGraphNode_StateNode::FArguments Args;
	Args.ContentPadding(EditorSettings->StateMachineContentPadding);
	
	SGraphNode_StateNode::Construct(Args, InNode);

	{
		const FSlateBrush* ImageBrush = FSMEditorStyle::Get()->GetBrush(TEXT("SMGraph.IntermediateGraph"));
		IntermediateWidget =
			SNew(SBorder)
			.BorderImage(FSMUnrealAppStyle::Get().GetBrush("NoBorder"))
			.Cursor(EMouseCursor::Hand)
			.OnMouseDoubleClick(this, &SGraphNode_StateMachineStateNode::OnIntermediateIconDoubleClick)
			[
				SNew(SImage)
				.Image(ImageBrush)
				.ToolTipText(NSLOCTEXT("StateMachineStateNode", "StateMachineStateNodeIntermediateTooltip", "Intermediate (Local) graph is enabled to manage the state machine reference. From within the local graph you can call GetStateMachineReference."))
				.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.72f))
				.Visibility(EVisibility::Visible)
			];
	}

	{
		const FSlateBrush* ImageBrush = FSMEditorStyle::Get()->GetBrush(TEXT("SMGraph.Clock"));
		WaitForEndStateWidget =
			SNew(SImage)
			.Image(ImageBrush)
			.ToolTipText(NSLOCTEXT("StateMachineStateNode", "StateMachineWaitForEndStateTooltip", "Wait for an end state before processing transitions or being considered an end state."))
			.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.72f))
			.Visibility(EVisibility::Visible);
	}
}

TArray<FOverlayWidgetInfo> SGraphNode_StateMachineStateNode::GetOverlayWidgets(bool bSelected,
	const FVector2D& WidgetSize) const
{
	TArray<FOverlayWidgetInfo> Widgets = SGraphNode_StateNode::GetOverlayWidgets(bSelected, WidgetSize);

	const USMEditorSettings* EditorSettings = FSMBlueprintEditorUtils::GetEditorSettings();
	if (!EditorSettings->bDisableVisualCues)
	{
		if (USMGraphNode_StateMachineStateNode* StateMachineNode = Cast<USMGraphNode_StateMachineStateNode>(GraphNode))
		{
			if (StateMachineNode->ShouldWaitForEndState())
			{
				const FSlateBrush* ImageBrush = FSMEditorStyle::Get()->GetBrush(TEXT("SMGraph.Clock"));

				FOverlayWidgetInfo Info;
				Info.OverlayOffset = FVector2D(WidgetSize.X - (ImageBrush->ImageSize.X * 0.5f) - (Widgets.Num() * OverlayWidgetPadding), -(ImageBrush->ImageSize.Y * 0.5f));
				Info.Widget = WaitForEndStateWidget;

				Widgets.Add(Info);
			}

			if (StateMachineNode->IsUsingIntermediateGraph())
			{
				const FSlateBrush* ImageBrush = FSMEditorStyle::Get()->GetBrush(TEXT("SMGraph.IntermediateGraph"));

				FOverlayWidgetInfo Info;
				Info.OverlayOffset = FVector2D(WidgetSize.X - (ImageBrush->ImageSize.X * 0.5f) - (Widgets.Num() * OverlayWidgetPadding), -(ImageBrush->ImageSize.Y * 0.5f));
				Info.Widget = IntermediateWidget;

				Widgets.Add(Info);
			}
		}
	}

	return Widgets;
}

const FSlateBrush* SGraphNode_StateMachineStateNode::GetNameIcon() const
{
	USMGraphNode_StateMachineStateNode* StateMachineNode = CastChecked<USMGraphNode_StateMachineStateNode>(GraphNode);

	if (const FSlateBrush* Brush = StateMachineNode->GetNodeIcon())
	{
		return Brush;
	}
	
	if (StateMachineNode->IsStateMachineReference())
    {
    	return FSMEditorStyle::Get()->GetBrush(TEXT("SMGraph.StateMachineReference_16x"));
    }

	return FSMUnrealAppStyle::Get().GetBrush(TEXT("GraphEditor.StateMachine_16x"));
}

TSharedPtr<SVerticalBox> SGraphNode_StateMachineStateNode::BuildComplexTooltip()
{
	TSharedPtr<SVerticalBox> Widget = SGraphNode_StateNode::BuildComplexTooltip();
	USMGraphNode_StateMachineStateNode* StateMachineNode = CastChecked<USMGraphNode_StateMachineStateNode>(GraphNode);

	if (USMGraphNode_StateMachineParentNode* Parent = Cast<USMGraphNode_StateMachineParentNode>(GraphNode))
	{
		if (USMBlueprintGeneratedClass* BPGC = Cast<USMBlueprintGeneratedClass>(Parent->ParentClass.Get()))
		{
			if (UBlueprint* Blueprint = UBlueprint::GetBlueprintFromClass(BPGC))
			{
				Widget->AddSlot()
					.AutoHeight()
					.Padding(FMargin(2.f, 4.f, 2.f, 2.f))
					[
						SNew(STextBlock)
						.TextStyle(FSMEditorStyle::Get(), "SMGraph.Tooltip.Info")
						.Text(FText::Format(LOCTEXT("StateMachineParentTooltip", "Parent {0}"), FText::FromString(Blueprint->GetPathName())))
					];
				return Widget;
			}
		}
		// there has to be an error
		Widget->AddSlot()
			.AutoHeight()
			.Padding(FMargin(2.f, 4.f, 2.f, 2.f))
			[
				SNew(STextBlock)
				.TextStyle(FSMEditorStyle::Get(), "SMGraph.Tooltip.Error")
				.Text(LOCTEXT("StateMachineParentTooltipError", "Error: Parent expected but missing. Was it forcefully deleted? Select a new parent"))
			];

		return Widget;
	}
	
	if (StateMachineNode->IsStateMachineReference())
	{
		if (USMBlueprint* Blueprint = StateMachineNode->GetStateMachineReference())
		{
			Widget->AddSlot()
				.AutoHeight()
				.Padding(FMargin(2.f, 4.f, 2.f, 2.f))
				[
					SNew(STextBlock)
					.TextStyle(FSMEditorStyle::Get(), "SMGraph.Tooltip.Info")
					.Text(FText::Format(LOCTEXT("StateMachineReferenceTooltip", "Reference to {0}"), FText::FromString(Blueprint->GetPathName())))
				];
		}
		else
		{
			// there has to be an error
			Widget->AddSlot()
				.AutoHeight()
				.Padding(FMargin(2.f, 4.f, 2.f, 2.f))
				[
					SNew(STextBlock)
					.TextStyle(FSMEditorStyle::Get(), "SMGraph.Tooltip.Error")
					.Text(LOCTEXT("StateMachineReferenceTooltipError", "Error: Reference expected but missing. Was it forcefully deleted? Change the reference"))
				];
		}
	}

	if (StateMachineNode->IsUsingIntermediateGraph())
	{
		Widget->AddSlot()
			.AutoHeight()
			.Padding(FMargin(2.f, 4.f, 2.f, 2.f))
			[
				SNew(STextBlock)
				.TextStyle(FSMEditorStyle::Get(), "SMGraph.Tooltip.Info")
				.Text(LOCTEXT("StateMachineIntermediateTooltip", "Using intermediate graph"))
			];
	}

	return Widget;
}

UEdGraph* SGraphNode_StateMachineStateNode::GetGraphToUseForTooltip() const
{
	USMGraphNode_StateMachineStateNode* StateMachineNode = CastChecked<USMGraphNode_StateMachineStateNode>(GraphNode);

	if (USMGraphNode_StateMachineParentNode* Parent = Cast<USMGraphNode_StateMachineParentNode>(GraphNode))
	{
		if (USMBlueprintGeneratedClass* BPGC = Cast<USMBlueprintGeneratedClass>(Parent->ParentClass.Get()))
		{
			if (UBlueprint* Blueprint = UBlueprint::GetBlueprintFromClass(BPGC))
			{
				if (USMGraph* StateMachineGraph = FSMBlueprintEditorUtils::GetRootStateMachineGraph(Blueprint))
				{
					return StateMachineGraph;
				}
			}
		}
	}
	
	// This blueprint graph.
	if (!StateMachineNode->IsStateMachineReference())
	{
		return SGraphNode_StateNode::GetGraphToUseForTooltip();
	}

	// Other blueprint graph.
	USMGraph* StateMachineGraph = FSMBlueprintEditorUtils::GetRootStateMachineGraph(StateMachineNode->GetStateMachineReference(), true);
	if (StateMachineGraph != nullptr)
	{
		return StateMachineGraph;
	}

	// No graph could be found, use the top most level k2 graph instead which is likely empty.
	return FSMBlueprintEditorUtils::GetTopLevelStateMachineGraph(StateMachineNode->GetStateMachineReference());
}

FReply SGraphNode_StateMachineStateNode::OnIntermediateIconDoubleClick(const FGeometry& InMyGeometry,
	const FPointerEvent& InMouseEvent)
{
	const USMGraphNode_StateMachineStateNode* StateMachineNode = CastChecked<USMGraphNode_StateMachineStateNode>(GraphNode);
	if (StateMachineNode->IsUsingIntermediateGraph())
	{
		if (const UEdGraph* Graph = StateMachineNode->GetBoundGraph())
		{
			FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(Graph);
		}
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
