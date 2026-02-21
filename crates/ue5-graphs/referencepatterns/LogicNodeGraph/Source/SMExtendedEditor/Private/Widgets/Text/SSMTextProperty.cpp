// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SSMTextProperty.h"

#include "Graph/Nodes/PropertyNodes/SMGraphK2Node_TextPropertyNode.h"
#include "Graph/SMTextPropertyGraph.h"
#include "SSMEditableTextBlock.h"
#include "SSMEditableTextBox.h"

#include "Blueprints/SMBlueprintEditor.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "EditorStyleSet.h"
#include "NodeFactory.h"
#include "SGraphPanel.h"
#include "SMUnrealTypeDefs.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "SSMTextProperty"

SSMTextProperty::SSMTextProperty()
{
}

void SSMTextProperty::Construct(const FArguments& InArgs)
{
	GraphNode = InArgs._GraphNode;
	WidgetInfo = InArgs._WidgetInfo ? *InArgs._WidgetInfo : FSMTextNodeWidgetInfo();

	FText DefaultText = WidgetInfo.DefaultText;
	TWeakPtr<SSMTextProperty> WeakPtrThis = SharedThis(this);
	
	TArray<TSharedRef<ITextDecorator>> Decorators;

	if (USMGraphK2Node_TextPropertyNode* Node = Cast<USMGraphK2Node_TextPropertyNode>(GraphNode))
	{
		Node->CreateDecorators(Decorators);

		if (DefaultText.IsEmpty())
		{
			DefaultText = Node->GetPropertyNode()->GetDisplayName();
		}

		if (Node->GetPropertyNode()->bIsInArray)
		{
			DefaultText = FText::FromString(FString::Printf(TEXT("%s %s"), *DefaultText.ToString(),
					*FString::FromInt(Node->GetPropertyNode()->ArrayIndex)));
		}

		CastChecked<USMTextPropertyGraph>(Node->GetPropertyGraph())->SwitchTextEditAction.BindSP(this, &SSMTextProperty::ToggleTextEdit);

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

	// Text body
	ChildSlot
	[
		SNew(SOverlay)
		+SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(FSMUnrealAppStyle::Get().GetBrush("Graph.StateNode.ColorSpill"))
			.BorderBackgroundColor(WidgetInfo.BackgroundColor)
			.Padding(1.f)
			[
				SAssignNew(HorizontalBox, SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Padding(NotifyPadding)
				[
					MakeNotifyIconWidget()
				]
				+SHorizontalBox::Slot()
				[
					SNew(SBox)
					.MinDesiredWidth(WidgetInfo.MinWidth)
					.MaxDesiredWidth(WidgetInfo.MaxWidth)
					.MinDesiredHeight(WidgetInfo.MinHeight)
					.MaxDesiredHeight(WidgetInfo.MaxHeight)
					.Padding(1.f)
					[
						SAssignNew(InlineEditableTextBody, SSMEditableTextBlock)
						.GraphNode(GraphNode.Get())
						.RichText(this, &SSMTextProperty::GetRichTextBody)
						.PlainText(this, &SSMTextProperty::GetPlainTextBody)
						.DefaultText(DefaultText)
						.DefaultTextStyle(WidgetInfo.DefaultTextStyle)
						.RichTextStyleDataTable(InArgs._RichTextInfo ? InArgs._RichTextInfo->RichTextStyleSet : nullptr)
						.RichTextStyleDecoratorInstances(MoveTemp(Decorators))
						.WrapTextAt(this, &SSMTextProperty::GetWrapText)
						.Style(&WidgetInfo.EditableTextStyle)
						.IsReadOnly(this, &SSMTextProperty::IsReadOnly)
						.OnTextCommitted(this, &SSMTextProperty::OnBodyTextCommitted)
						.MultiLine(true)
					]
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Padding(2.f) // Padding needed to help with zoom resize issues.
				[
					SAssignNew(InputPinContainer, SBox)
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.f, 0.f, 2.f, 0.f)
				[
					SNew(SImage)
					.Image(FSMUnrealAppStyle::Get().GetBrush(TEXT("Icons.Info")))
					.ToolTipText(LOCTEXT("GraphEditModeTooltip", "Text graph is in Graph Edit Mode: Only the format text node in the property graph can be edited.\nSelect 'Revert to Node Edit' to edit directly from this node again."))
					.Visibility_Lambda([this, WeakPtrThis]()
					{
						return (WeakPtrThis.IsValid() && GraphNode.IsValid() && IsInGraphEditMode()) ? EVisibility::Visible : EVisibility::Collapsed;
					})
				]
			]
		]
		+SOverlay::Slot()
		.Padding(HighlightPadding)
		[
			MakeHighlightBorder()
		]
	];
}

void SSMTextProperty::Finalize()
{
	if (USMGraphK2Node_TextPropertyNode* Node = Cast<USMGraphK2Node_TextPropertyNode>(GraphNode))
	{
		const USMTextPropertyGraph* Graph = CastChecked<USMTextPropertyGraph>(Node->GetPropertyGraph());
		if (UEdGraphPin* FormatTextPin = Graph->GetFormatTextNodePinChecked())
		{
			const TSharedPtr<SGraphNode> ParentNode = FindParentGraphNode();
			// Create a pin representing the FormatTextNode text pin.
			const TSharedPtr<SGraphPin> InputPin = (ParentNode.IsValid() && FormatTextPin) ? FNodeFactory::CreatePinWidget(FormatTextPin) : nullptr;

			if (InputPin.IsValid())
			{
				InputPin->SetOwner(ParentNode.ToSharedRef());
				InputPin->SetShowLabel(false);

				if (const FSMGraphProperty_Base* Prop = Node->GetPropertyNode())
				{
					InputPin->SetIsEditable(!Prop->IsVariableReadOnly());
				}
				
				const TWeakPtr<SHorizontalBox> Row = InputPin->GetFullPinHorizontalRowWidget();

				if (Row.Pin().IsValid())
				{
					bool bSuccess = false;
					if (FChildren* Children = Row.Pin()->GetChildren())
					{
						if (Children->Num() > 1)
						{
							// Hide the input pin.
							Children->GetChildAt(0)->SetVisibility(EVisibility::Collapsed);

							// Hide the text box. We only want to leave the localization button.
							/*
							 * If the following looks like a hack to you, then you would be correct!
							 * We just want the localization button that's defined in STextPropertyEditableTextBox.
							 * However there is no easy way to get that button or the even the PrimaryWidget which is
							 * the actual reference to what we want to hide. Why? It's private because of course it is,
							 * and recreating STextPropertyEditableTextBox much like every other slate text related item
							 * in the extended module was definitely considered, but luckily this nifty hack gets around that...
							 * at least until an engine update causes it to explode.
							 */
							if (FChildren* GrandChildren = Children->GetChildAt(1)->GetChildren())
							{
								if (GrandChildren->Num() > 1)
								{
									if (FChildren* GreatGrandChildren = GrandChildren->GetChildAt(1)->GetChildren())
									{
										if (GreatGrandChildren->Num() > 0)
										{
											FChildren* GreatGreatGrandChildren = GreatGrandChildren->GetChildAt(0)->GetChildren();

											if (GreatGreatGrandChildren->Num() > 0)
											{
												FChildren* MoreGreatGrandChildren = GreatGreatGrandChildren->GetChildAt(0)->GetChildren();

												if (MoreGreatGrandChildren->Num() > 0)
												{
													const TSharedRef<SWidget> ValueWidget = MoreGreatGrandChildren->GetChildAt(0);
													ValueWidget->SetVisibility(EVisibility::Collapsed);
													bSuccess = true;
												}
											}
										}
									}
								}
							}
						}
					}
					
					ensureMsgf(bSuccess, TEXT("Can't find value widget to hide on text node. Check to see if an engine update changed the slate structure."));
				}

				// To the center right of the main text body.
				check(InputPinContainer.IsValid());
				InputPinContainer->SetContent(InputPin.ToSharedRef());
			}
		}
	}
}

FReply SSMTextProperty::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	if (USMGraphK2Node_TextPropertyNode* Node = Cast<USMGraphK2Node_TextPropertyNode>(GraphNode))
	{
		const TSharedPtr<SGraphNode> ParentNode = FindParentGraphNode();
		// If the owning node belongs to an OwnerPanel we need to check if the panel has its own readonly properties.
		const TSharedPtr<SGraphPanel> OwningPanel = ParentNode.IsValid() ? ParentNode->GetOwnerPanel() : nullptr;

		const USMTextPropertyGraph* TextGraph = CastChecked<USMTextPropertyGraph>(Node->GetPropertyGraph());
		// Jump to the graph is property can't be edited directly.
		if (TextGraph->IsGraphBeingUsedToEdit() || TextGraph->IsVariableReadOnly() ||
			(OwningPanel.IsValid() && !OwningPanel->IsGraphEditable()))
		{
			Node->JumpToPropertyGraph();
			return FReply::Handled();
		}

		// Begin editing on this text property.
		ToggleTextEdit(true);
	}

	return SCompoundWidget::OnMouseButtonDoubleClick(InMyGeometry, InMouseEvent);
}

void SSMTextProperty::ToggleTextEdit(bool bValue)
{
	if (FSMBlueprintEditor* Editor = FSMBlueprintEditorUtils::GetStateMachineEditor(GraphNode.Get()))
	{
		// Check that we're not in debug mode.
		if (!Editor->InEditingMode())
		{
			return;
		}
	}
	
	if (!bValue && InlineEditableTextBody->IsInEditMode())
	{
		InlineEditableTextBody->ExitEditingMode();
	}
	else
	{
		InlineEditableTextBody->EnterEditingMode();
	}
}

bool SSMTextProperty::IsReadOnly() const
{
	if (const FSMBlueprintEditor* Editor = FSMBlueprintEditorUtils::GetStateMachineEditor(GraphNode.Get()))
	{
		// Check that we're not in debug mode.
		if (!Editor->InEditingMode())
		{
			return true;
		}
	}
	
	USMGraphK2Node_TextPropertyNode* PropertyNode = CastChecked<USMGraphK2Node_TextPropertyNode>(GraphNode);
	return PropertyNode->GetPropertyGraph()->IsGraphBeingUsedToEdit() || PropertyNode->GetPropertyNodeChecked()->IsVariableReadOnly();
}

bool SSMTextProperty::IsInGraphEditMode() const
{
	const USMGraphK2Node_TextPropertyNode* PropertyNode = CastChecked<USMGraphK2Node_TextPropertyNode>(GraphNode);
	return PropertyNode->GetPropertyGraph()->IsGraphBeingUsedToEdit();
}

FText SSMTextProperty::GetRichTextBody() const
{
	return CastChecked<USMTextPropertyGraph>(CastChecked<USMGraphK2Node_TextPropertyNode>(GraphNode)->GetPropertyGraph())->GetRichTextBody();
}

FText SSMTextProperty::GetPlainTextBody() const
{
	return CastChecked<USMTextPropertyGraph>(CastChecked<USMGraphK2Node_TextPropertyNode>(GraphNode)->GetPropertyGraph())->GetPlainTextBody();
}

void SSMTextProperty::OnBodyTextCommitted(const FText& InText, ETextCommit::Type CommitInfo)
{
	// Use the plain text version from the text block. The InText may include <RunInfo> otherwise.
	const FText PlainText = InlineEditableTextBody->GetEditableRichTextBlock()->GetPlainText();

	const USMGraphK2Node_TextPropertyNode* Node = CastChecked<USMGraphK2Node_TextPropertyNode>(GraphNode);
	CastChecked<USMTextPropertyGraph>(Node->GetPropertyGraph())->CommitNewText(PlainText);
}

float SSMTextProperty::GetWrapText() const
{
	// Set to most of max width. Extra padding needed to prevent cutoff.
	if (WidgetInfo.WrapTextAt == 0)
	{
		return WidgetInfo.MaxWidth * .9f;
	}
	
	return WidgetInfo.WrapTextAt > 0 ? WidgetInfo.WrapTextAt : 0;
}

#undef LOCTEXT_NAMESPACE