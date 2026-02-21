// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SGraphNode_ExecutionEntryNode.h"
#include "Configuration/SMEditorSettings.h"
#include "Configuration/SMEditorStyle.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_RuntimeNodeContainer.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "Widgets/Images/SImage.h"
#include "SCommentBubble.h"
#include "SGraphPin.h"

#define LOCTEXT_NAMESPACE "SGraphExecutionEntryNode"

void SGraphNode_ExecutionEntryNode::Construct(const FArguments& InArgs, USMGraphK2Node_RuntimeNode_Base* InNode)
{
	this->GraphNode = InNode;

	this->SetCursor(EMouseCursor::CardinalCross);

	const FSlateBrush* FastPathImageBrush = FSMEditorStyle::Get()->GetBrush(TEXT("SMGraph.FastPath"));
	
	FastPathWidget =
	SNew(SImage)
	.Image(FastPathImageBrush)
	.ToolTipText(NSLOCTEXT("GraphNodeNode", "GraphNodeFastPathTooltip", "Fast path enabled: This node will avoid using the blueprint graph."))
	.Visibility(EVisibility::Visible);
	
	this->UpdateGraphNode();
}

TArray<FOverlayWidgetInfo> SGraphNode_ExecutionEntryNode::GetOverlayWidgets(bool bSelected,
	const FVector2D& WidgetSize) const
{
	TArray<FOverlayWidgetInfo> Widgets = SGraphNodeK2Base::GetOverlayWidgets(bSelected, WidgetSize);

	const int32 OverlayWidgetPadding = 20;

	const USMEditorSettings* EditorSettings = FSMBlueprintEditorUtils::GetEditorSettings();
	if (EditorSettings->bDisplayFastPath)
	{
		if (const USMGraphK2Node_RuntimeNode_Base* SMGraphNode = Cast<USMGraphK2Node_RuntimeNode_Base>(GraphNode))
		{
			if (SMGraphNode->IsFastPathEnabled())
			{
				const FSlateBrush* ImageBrush = FSMEditorStyle::Get()->GetBrush(TEXT("SMGraph.FastPath"));

				FOverlayWidgetInfo Info;
				Info.OverlayOffset = FVector2D(WidgetSize.X - (ImageBrush->ImageSize.X * 0.5f) - (Widgets.Num() * OverlayWidgetPadding), -(ImageBrush->ImageSize.Y * 0.5f));
				Info.Widget = FastPathWidget;

				Widgets.Add(Info);
			}
		}
	}

	return Widgets;
}

#undef LOCTEXT_NAMESPACE