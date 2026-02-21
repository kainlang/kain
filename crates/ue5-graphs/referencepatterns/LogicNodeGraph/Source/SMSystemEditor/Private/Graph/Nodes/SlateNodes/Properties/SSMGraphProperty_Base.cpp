// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Graph/Nodes/SlateNodes/Properties/SSMGraphProperty_Base.h"

#include "Configuration/SMEditorStyle.h"
#include "Graph/Nodes/PropertyNodes/SMGraphK2Node_PropertyNode_Base.h"

#include "SGraphPin.h"
#include "SMUnrealTypeDefs.h"
#include "Widgets/Images/SImage.h"

#define LOCTEXT_NAMESPACE "SSMGraphProperty_Base"

const FMargin SSMGraphProperty_Base::NotifyPadding = FMargin(3.f, 0.f, 0.f, 0.f);

SSMGraphProperty_Base::SSMGraphProperty_Base(): GraphNode(nullptr)
{
}

void SSMGraphProperty_Base::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	SCompoundWidget::OnMouseEnter(MyGeometry, MouseEvent);
	if (USMGraphK2Node_PropertyNode_Base* Node = Cast<USMGraphK2Node_PropertyNode_Base>(GraphNode))
	{
		Node->bMouseOverNodeProperty = true;
	}
}

void SSMGraphProperty_Base::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	SCompoundWidget::OnMouseLeave(MouseEvent);

	if (!MouseEvent.IsMouseButtonDown(EKeys::RightMouseButton))
	{
		if (USMGraphK2Node_PropertyNode_Base* Node = Cast<USMGraphK2Node_PropertyNode_Base>(GraphNode))
		{
			Node->bMouseOverNodeProperty = false;
		}
	}
}

FReply SSMGraphProperty_Base::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	if (USMGraphK2Node_PropertyNode_Base* Node = Cast<USMGraphK2Node_PropertyNode_Base>(GraphNode))
	{
		Node->JumpToPropertyGraph();
		return FReply::Handled();
	}

	return SCompoundWidget::OnMouseButtonDoubleClick(InMyGeometry, InMouseEvent);
}

TSharedPtr<SGraphNode> SSMGraphProperty_Base::FindParentGraphNode() const
{
	for (TSharedPtr<SWidget> Parent = GetParentWidget(); Parent.IsValid(); Parent = Parent->GetParentWidget())
	{
		FString Type = Parent->GetType().ToString();
		if (Type.Contains("SGraphNode"))
		{
			return StaticCastSharedPtr<SGraphNode>(Parent);
		}
	}

	return nullptr;
}

UEdGraphPin* SSMGraphProperty_Base::FindResultPin() const
{
	if (USMGraphK2Node_PropertyNode_Base* Node = Cast<USMGraphK2Node_PropertyNode_Base>(GraphNode))
	{
		return Node->GetResultPin();
	}

	return nullptr;
}

TSharedRef<SBorder> SSMGraphProperty_Base::MakeHighlightBorder()
{
	return SNew(SBorder)
	.BorderImage(FSMEditorStyle::Get()->GetBrush("BoxHighlight"))
	.BorderBackgroundColor(this, &SSMGraphProperty_Base::GetHighlightColor)
	.Visibility(this, &SSMGraphProperty_Base::GetHighlightVisibility);
}

TSharedRef<SWidget> SSMGraphProperty_Base::MakeNotifyIconWidget()
{
	return SNew(SImage)
	.Image(this, &SSMGraphProperty_Base::GetNotifyIconBrush)
	.ToolTipText(this, &SSMGraphProperty_Base::GetNotifyIconTooltip)
	.Visibility(this, &SSMGraphProperty_Base::GetNotifyVisibility);
}

EVisibility SSMGraphProperty_Base::GetHighlightVisibility() const
{
	bool bVisible = false;
	if (const USMGraphK2Node_PropertyNode_Base* Node = Cast<USMGraphK2Node_PropertyNode_Base>(GraphNode))
	{
		const USMGraphK2Node_PropertyNode_Base::FHighlightArgs& Args = Node->GetHighlightArgs();
		bVisible = Args.bEnable;
	}

	return bVisible ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
}

FSlateColor SSMGraphProperty_Base::GetHighlightColor() const
{
	if (const USMGraphK2Node_PropertyNode_Base* Node = Cast<USMGraphK2Node_PropertyNode_Base>(GraphNode))
	{
		const USMGraphK2Node_PropertyNode_Base::FHighlightArgs& Args = Node->GetHighlightArgs();
		if (Args.bEnable)
		{
			return Node->GetHighlightArgs().Color;
		}
	}

	return FLinearColor::Transparent;
}

EVisibility SSMGraphProperty_Base::GetNotifyVisibility() const
{
	bool bVisible = false;

	if (const USMGraphK2Node_PropertyNode_Base* Node = Cast<USMGraphK2Node_PropertyNode_Base>(GraphNode))
	{
		const USMGraphK2Node_PropertyNode_Base::FNotifyArgs& Args = Node->GetNotifyArgs();
		bVisible = Args.bEnable;
	}

	return bVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

const FSlateBrush* SSMGraphProperty_Base::GetNotifyIconBrush() const
{
	if (const USMGraphK2Node_PropertyNode_Base* Node = Cast<USMGraphK2Node_PropertyNode_Base>(GraphNode))
	{
		const USMGraphK2Node_PropertyNode_Base::FNotifyArgs& Args = Node->GetNotifyArgs();
		switch (Args.LogType)
		{
		case ESMLogType::Note:
			{
				return FSMUnrealAppStyle::Get().GetBrush(TEXT("Icons.Info"));
			}
		case ESMLogType::Warning:
			{
				return FSMUnrealAppStyle::Get().GetBrush(TEXT("Icons.Warning"));
			}
		case ESMLogType::Error:
			{
				return FSMUnrealAppStyle::Get().GetBrush(TEXT("Icons.Error"));
			}
		}
	}

	return FStyleDefaults::GetNoBrush();
}

FText SSMGraphProperty_Base::GetNotifyIconTooltip() const
{
	if (const USMGraphK2Node_PropertyNode_Base* Node = Cast<USMGraphK2Node_PropertyNode_Base>(GraphNode))
	{
		const USMGraphK2Node_PropertyNode_Base::FNotifyArgs& Args = Node->GetNotifyArgs();
		return FText::FromString(Args.Message);
	}

	return FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE
