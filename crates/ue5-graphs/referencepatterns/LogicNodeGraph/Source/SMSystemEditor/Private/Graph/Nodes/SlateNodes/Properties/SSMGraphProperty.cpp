// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SSMGraphProperty.h"

#include "Configuration/SMEditorSettings.h"
#include "Configuration/SMEditorStyle.h"
#include "Graph/Nodes/PropertyNodes/SMGraphK2Node_GraphPropertyNode.h"
#include "Graph/Nodes/PropertyNodes/SMGraphK2Node_PropertyNode_Base.h"
#include "Graph/SMPropertyGraph.h"
#include "Helpers/SMDragDropHelpers.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "BPVariableDragDropAction.h"
#include "Editor/Kismet/Private/BPFunctionDragDropAction.h"
#include "EditorStyleSet.h"
#include "NodeFactory.h"
#include "SGraphPin.h"
#include "SKismetLinearExpression.h"
#include "SMUnrealTypeDefs.h"
#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "SSMGraphProperty"

SSMGraphProperty::SSMGraphProperty() : bIsValidDragDrop(false)
{
}

SSMGraphProperty::~SSMGraphProperty()
{
	if (USMGraphK2Node_GraphPropertyNode* Node = Cast<USMGraphK2Node_GraphPropertyNode>(GraphNode))
	{
		Node->ForceVisualRefreshEvent.RemoveAll(this);
	}
}

void SSMGraphProperty::Construct(const FArguments& InArgs)
{
	GraphNode = InArgs._GraphNode;
	WidgetInfo = InArgs._WidgetInfo ? *InArgs._WidgetInfo : FSMTextDisplayWidgetInfo();
	
	FText DefaultText = WidgetInfo.DefaultText;

	UEdGraphPin* ResultPin = nullptr;
	if (USMGraphK2Node_GraphPropertyNode* Node = Cast<USMGraphK2Node_GraphPropertyNode>(GraphNode))
	{
		Node->ForceVisualRefreshEvent.AddSP(this, &SSMGraphProperty::Refresh);
		
		if (DefaultText.IsEmpty())
		{
			DefaultText = Node->GetPropertyNode()->GetDisplayName();
		}

		if (Node->GetPropertyNode()->bIsInArray)
		{
			DefaultText = FText::FromString(FString::Printf(TEXT("%s %s"), *DefaultText.ToString(),
					*FString::FromInt(Node->GetPropertyNode()->ArrayIndex)));
		}

		ResultPin = Node->GetResultPin();

		if (FSMGraphProperty_Base* Prop = Node->GetPropertyNode())
		{
			if (UBlueprint* Blueprint = FSMBlueprintEditorUtils::FindBlueprintForNode(Node))
			{
				if (const FProperty* Property = Prop->MemberReference.ResolveMember<FProperty>(Blueprint))
				{
					const FText Description = Property->GetToolTipText();
					if (!Description.IsEmpty())
					{
						SetToolTipText(Description);
					}
				}
			}
		}
	}
	
	ChildSlot
	[
		SNew(SOverlay)
		+SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(FSMUnrealAppStyle::Get().GetBrush("Graph.StateNode.ColorSpill"))
			.BorderBackgroundColor(this, &SSMGraphProperty::GetBackgroundColor)
			.Padding(1.f)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Padding(NotifyPadding)
				[
					MakeNotifyIconWidget()
				]
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				[
					SNew(SBox)
					.MinDesiredWidth(WidgetInfo.MinWidth)
					.MaxDesiredWidth(WidgetInfo.MaxWidth)
					.MinDesiredHeight(WidgetInfo.MinHeight)
					.MaxDesiredHeight(WidgetInfo.MaxHeight)
					.Clipping(WidgetInfo.Clipping)
					.Padding(1.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						.VAlign(VAlign_Center)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.VAlign(VAlign_Center)
							[
								// Default text.
								SNew(STextBlock)
								.Text(DefaultText)
								.TextStyle(&WidgetInfo.DefaultTextStyle)
								.Margin(FMargin(1.f))
							]
							+ SHorizontalBox::Slot()
							.VAlign(VAlign_Center)
							.AutoWidth()
							[
								// Linear expression.
								SAssignNew(ExpressionWidget, SKismetLinearExpression, ResultPin)
								.Clipping(EWidgetClipping::ClipToBounds)
								.IsEditable(false)
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.HAlign(HAlign_Left)
							.VAlign(VAlign_Center)
							.Padding(2.f) // Padding needed to help with zoom resize issues.
							[
								SAssignNew(InputPinContainer, SBox)
							]
						]
					]
				]
			]
		]
		// Optional highlight border.
		+SOverlay::Slot()
		.Padding(HighlightPadding)
		[
			MakeHighlightBorder()
		]
	];

	HandleExpressionChange(ResultPin);
}

void SSMGraphProperty::Finalize()
{
	UEdGraphPin* ResultPin = FindResultPin();
	TSharedPtr<SGraphNode> ParentNode = FindParentGraphNode();
	TSharedPtr<SGraphPin> InputPin = (ParentNode.IsValid() && ResultPin) ? FNodeFactory::CreatePinWidget(ResultPin) : nullptr;
	// Don't display if invalid or another connection is present. We only want this to display / edit the default value.
	if (InputPin.IsValid() && !InputPin->IsConnected())
	{
		InputPin->SetOwner(ParentNode.ToSharedRef());
		InputPin->SetOnlyShowDefaultValue(true);
		InputPin->SetShowLabel(false);
		InputPin->SetPinColorModifier(FSMBlueprintEditorUtils::GetEditorSettings()->PropertyPinColorModifier); // Without this the color can wash out the text.
		if (USMGraphK2Node_GraphPropertyNode* Node = Cast<USMGraphK2Node_GraphPropertyNode>(GraphNode))
		{
			if (const FSMGraphProperty_Base* Prop = Node->GetPropertyNode())
			{
				InputPin->SetIsEditable(!Prop->IsVariableReadOnly());
			}
		}
		
		TWeakPtr<SHorizontalBox> Row = InputPin->GetFullPinHorizontalRowWidget();
		// We want to hide the k2 selection pin as this is only for defaults.
		if (Row.Pin().IsValid())
		{
			if (FChildren* Children = Row.Pin()->GetChildren())
			{
				// The first child should be the pin.
				if (Children->Num() > 1)
				{
					Children->GetChildAt(0)->SetVisibility(EVisibility::Collapsed);
				}
			}
		}

		check(InputPinContainer.IsValid());
		InputPinContainer->SetContent(InputPin.ToSharedRef());
	}

	InputPinPtr = InputPin;
}

FReply SSMGraphProperty::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	if (IsDragDropValid(DragDropEvent))
	{
		bIsValidDragDrop = true;
		SetCursor(EMouseCursor::GrabHand);

		// Tooltip message.
		FSMDragDropHelpers::SetDragDropMessage(DragDropEvent);
		
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SSMGraphProperty::OnDragLeave(const FDragDropEvent& DragDropEvent)
{
	SetCursor(EMouseCursor::CardinalCross);
	bIsValidDragDrop = false;
	SCompoundWidget::OnDragLeave(DragDropEvent);
}

FReply SSMGraphProperty::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	if (IsDragDropValid(DragDropEvent))
	{
		UEdGraphPin* ResultPin = nullptr;
		if (USMGraphK2Node_GraphPropertyNode* Node = Cast<USMGraphK2Node_GraphPropertyNode>(GraphNode))
		{
			USMPropertyGraph* Graph = CastChecked<USMPropertyGraph>(Node->GetPropertyGraph());

			TSharedPtr<FKismetVariableDragDropAction> VariableDragDrop = DragDropEvent.GetOperationAs<FKismetVariableDragDropAction>();
			if (VariableDragDrop.IsValid())
			{
				FProperty* Property = VariableDragDrop->GetVariableProperty();
				Graph->SetPropertyOnGraph(Property);
			}
			TSharedPtr<FKismetFunctionDragDropAction> FunctionDragDrop = DragDropEvent.GetOperationAs<FKismetFunctionDragDropAction>();
			if (FunctionDragDrop.IsValid())
			{
				UFunction const* Function = FSMDragDropAction_Function::GetFunction(FunctionDragDrop.Get());
				Graph->SetFunctionOnGraph(const_cast<UFunction*>(Function));
			}

			ResultPin = Node->GetResultPin();
		}

		SetCursor(EMouseCursor::CardinalCross);

		bIsValidDragDrop = false;
		ExpressionWidget->SetExpressionRoot(ResultPin);
		HandleExpressionChange(ResultPin);

		return FReply::Handled();
	}

	return SCompoundWidget::OnDrop(MyGeometry, DragDropEvent);
}

void SSMGraphProperty::Refresh()
{
	HandleExpressionChange(FindResultPin());
}

bool SSMGraphProperty::IsDragDropValid(const FDragDropEvent& DragDropEvent) const
{
	return FSMDragDropHelpers::IsDragDropValidForPropertyNode(Cast<USMGraphK2Node_PropertyNode_Base>(GraphNode), DragDropEvent, true);
}

void SSMGraphProperty::HandleExpressionChange(UEdGraphPin* ResultPin)
{
	if (ResultPin && ResultPin->LinkedTo.Num())
	{
		// Display normal object evaluation.
		ExpressionWidget->SetVisibility(EVisibility::HitTestInvisible);
		ExpressionWidget->SetExpressionRoot(ResultPin);

		if (InputPinPtr.IsValid())
		{
			InputPinPtr.Pin()->SetVisibility(EVisibility::Collapsed);
		}
	}
	else
	{
		// Display default text only.
		ExpressionWidget->SetVisibility(EVisibility::Collapsed);
		if (InputPinPtr.IsValid())
		{
			InputPinPtr.Pin()->SetVisibility(EVisibility::HitTestInvisible);
		}
	}
}

FSlateColor SSMGraphProperty::GetBackgroundColor() const
{
	return bIsValidDragDrop ? WidgetInfo.OnDropBackgroundColor : WidgetInfo.BackgroundColor;
}

#undef LOCTEXT_NAMESPACE
