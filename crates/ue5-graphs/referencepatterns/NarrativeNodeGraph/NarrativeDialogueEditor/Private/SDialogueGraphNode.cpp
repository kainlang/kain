// Copyright Narrative Tools 2022. 

#include "SDialogueGraphNode.h"
#include "DialogueGraphNode.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Editor.h"
#include "DialogueGraph.h"
#include "SGraphPanel.h"
#include "ScopedTransaction.h"
#include "DialogueSM.h"
#include "Dialogue.h"
#include "DialogueEditorStyle.h"
#include "NarrativeCondition.h"
#include "NarrativeEvent.h"
#include "DialogueEditorStyle.h"
#include "DialogueEditorSettings.h"
#include <Blueprint/UserWidget.h>
#include "Components/VerticalBox.h"
#include <Slate/SObjectWidget.h>
#include "DialogueNodeUserWidget.h"
#include "DialogueBlueprint.h"

#define LOCTEXT_NAMESPACE "SDialogueGraphNode"

void SDialogueGraphNode::Construct(const FArguments& InArgs, UDialogueGraphNode* InNode)
{
	GraphNode = InNode;

	UpdateGraphNode();


}

void SDialogueGraphNode::CreatePinWidgets()
{
	UDialogueGraphNode* DialogueGraphNode = CastChecked<UDialogueGraphNode>(GraphNode);

	for (int32 PinIdx = 0; PinIdx < DialogueGraphNode->Pins.Num(); PinIdx++)
	{
		UEdGraphPin* MyPin = DialogueGraphNode->Pins[PinIdx];
		if (!MyPin->bHidden)
		{
			TSharedPtr<SGraphPin> NewPin = SNew(SGraphPin, MyPin);

			AddPin(NewPin.ToSharedRef());
		}
	}
}

void SDialogueGraphNode::UpdateGraphNode()
{

	InputPins.Empty();
	OutputPins.Empty();

	// Reset variables that are going to be exposed, in case we are refreshing an already setup node.
	RightNodeBox.Reset();
	LeftNodeBox.Reset();

	UDialogueGraphNode* DialogueGraphNode = Cast<UDialogueGraphNode>(GraphNode);
	UDialogueNode* DialogueNode = DialogueGraphNode ? DialogueGraphNode->DialogueNode : nullptr;

	//UDialogueNodeUserWidget* DialogueNodeWidgetInstance = nullptr;
	UWorld* WidgetWorld = GEditor->GetEditorWorldContext().World();
	class UDialogueNodeUserWidget* DialogueNodeWidgetInstance = nullptr;

	// In a function where you want to get the reference to the UUserWidget asset
	// Load the UUserWidget asset using SoftObjectPtr::LoadSynchronous
	if (const UDialogueEditorSettings* DialogueSettings = GetDefault<UDialogueEditorSettings>())
	{
		UClass* WidgetClass = DialogueSettings->DefaultDialogueWidgetClass.LoadSynchronous();

		// Spawn an instance of the UUserWidget class
		DialogueNodeWidgetInstance = CreateWidget<UDialogueNodeUserWidget>(WidgetWorld, WidgetClass);
	}

	if (DialogueNodeWidgetInstance && DialogueNode)
	{
		DialogueNodeWidgetInstance->SetFlags(RF_Transient);

		if (UDialogueBlueprint* DialogueBP = Cast<UDialogueBlueprint>(GraphNode->GetGraph()->GetOuter()))
		{
			if (UDialogue* OwningDialogue = Cast<UDialogue>(DialogueBP->GeneratedClass->GetDefaultObject()))
			{
				DialogueNodeWidgetInstance->InitializeFromNode(DialogueNode, OwningDialogue);
				DialogueNodeWidgetRef = DialogueNodeWidgetInstance->TakeWidget();

				if (DialogueNodeWidgetInstance->RightPinBox && DialogueNodeWidgetInstance->LeftPinBox)
				{
					//Tell the C++ code to autopopulate our UMG blueprint widgets pin boxes! 
					RightNodeBox = StaticCastSharedRef<SVerticalBox>(DialogueNodeWidgetInstance->RightPinBox->TakeWidget());
					LeftNodeBox = StaticCastSharedRef<SVerticalBox>(DialogueNodeWidgetInstance->LeftPinBox->TakeWidget());
				}
				else
				{
					//Didnt have pin boxes, clear the dialogue node ref to force node to use old style 
					DialogueNodeWidgetRef.Reset();
				}
			}
		}
	}

	//Fancy new dialogue UMG based nodes!
	if (DialogueNodeWidgetRef.IsValid())
	{

		this->GetOrAddSlot(ENodeZone::Center)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
			[
				DialogueNodeWidgetRef.ToSharedRef()
			]
			];
	}
	else // Old style yucky slate ones
	{
		auto SizeBox = SNew(SBox);
		SizeBox->SetMinDesiredWidth(250.f);
		SizeBox->SetMinDesiredHeight(60.f);

		//
		this->GetOrAddSlot(ENodeZone::Center)
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Fill)
	[
	SNew(SBorder)
	.BorderImage(FAppStyle::Get().GetBrush("BTEditor.Graph.BTNode.Body"))
	.BorderBackgroundColor(this, &SDialogueGraphNode::GetBorderColor)
		[
			SNew(SOverlay)
			+SOverlay::Slot()
			[
				SizeBox
			]
			+SOverlay::Slot()
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Right)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				[
					SNew(SImage)
					.Image(FDialogueEditorStyle::Get()->GetBrush("ClassIcon.DialogueBlueprint"))
					.ToolTipText(this, &SDialogueGraphNode::GetWarningIconTooltip)
					.ColorAndOpacity(this, &SDialogueGraphNode::GetWarningIconColor)
				]
			]
			+SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding(5.f)
			[
				
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.AutoWidth()
				[
					SNew(SBox)
					.MinDesiredHeight(10.f)
					[
						SAssignNew(LeftNodeBox, SVerticalBox)
					]
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				.FillWidth(1.f)
				[

					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Fill)
					.FillHeight(1.f)
					[
						SNew(STextBlock)
						.Text(this, &SDialogueGraphNode::GetConditionsText)
					.Visibility(this, &SDialogueGraphNode::GetCondsVis)
					.Justification(ETextJustify::Center)
					.AutoWrapText(true)
					.TextStyle(FAppStyle::Get(), TEXT("PhysicsAssetEditor.Tools.Font"))
					.ColorAndOpacity(this, &SDialogueGraphNode::GetCondsColor)
					]
					+ SVerticalBox::Slot()
					.VAlign(VAlign_Top)
					.HAlign(HAlign_Fill)
					.AutoHeight()
					[
					SNew(STextBlock)
						.Text(this, &SDialogueGraphNode::GetNodeTitleText)
						.ColorAndOpacity(this, &SDialogueGraphNode::GetNodeTitleColor)
						.Justification(ETextJustify::Center)
						.Clipping(EWidgetClipping::Inherit)
						.TextStyle(FAppStyle::Get(), TEXT("PhysicsAssetEditor.Tools.Font"))
					]
					+ SVerticalBox::Slot()
					.VAlign(VAlign_Bottom)
					.HAlign(HAlign_Fill)
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(this, &SDialogueGraphNode::GetNodeText)
						.Justification(ETextJustify::Center)
						.WrapTextAt(200.f)
					]
					+ SVerticalBox::Slot()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Fill)
						.FillHeight(1.f)
						[
							SNew(STextBlock)
							.Text(this, &SDialogueGraphNode::GetEventsText)
						.Visibility(this, &SDialogueGraphNode::GetEventsVis)
						.Justification(ETextJustify::Center)
						.AutoWrapText(true)
						.TextStyle(FAppStyle::Get(), TEXT("PhysicsAssetEditor.Tools.Font"))
						.ColorAndOpacity(this, &SDialogueGraphNode::GetEventsColor)
						]
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				.AutoWidth()
				[
					SNew(SBox)
					.MinDesiredHeight(10.f)
				[
					SAssignNew(RightNodeBox, SVerticalBox)
				]
				]
			]
			]
];
	}


	CreatePinWidgets();

	//Nodes created before coords update won't have their coords set, set them in here
	if (DialogueGraphNode && DialogueNode)
	{
		DialogueNode->NodePos = FVector2D(DialogueGraphNode->NodePosX, DialogueGraphNode->NodePosY);
	}
}

TSharedPtr<SToolTip> SDialogueGraphNode::GetComplexTooltip()
{
	return nullptr;
}

void SDialogueGraphNode::AddPin(const TSharedRef<SGraphPin>& PinToAdd)
{
	PinToAdd->SetOwner(SharedThis(this));

	if (!RightNodeBox.IsValid() || !LeftNodeBox.IsValid())
	{
		return;
	}

	const UEdGraphPin* PinObj = PinToAdd->GetPinObj();

	if (PinToAdd->GetDirection() == EEdGraphPinDirection::EGPD_Input)
	{
		LeftNodeBox->AddSlot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.FillHeight(1.0f)
			.Padding(5.f, 0.f)
			[
				PinToAdd
			];
		InputPins.Add(PinToAdd);
	}
	else
	{
		RightNodeBox->AddSlot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.FillHeight(1.0f)
			[
				PinToAdd
			];
		OutputPins.Add(PinToAdd);
	}

	

}

void SDialogueGraphNode::MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter, bool bMarkDirty /* = true */)
{
	SGraphNode::MoveTo(NewPosition, NodeFilter, bMarkDirty);

	//Super call may have moved node, update coords
	if (UDialogueGraphNode* DialogueGraphNode = Cast<UDialogueGraphNode>(GraphNode))
	{
		if (DialogueGraphNode->DialogueNode)
		{
			DialogueGraphNode->DialogueNode->NodePos = FVector2D(DialogueGraphNode->NodePosX, DialogueGraphNode->NodePosY);
		}
	}
}

FReply SDialogueGraphNode::OnMouseDown(const FGeometry& SenderGeometry, const FPointerEvent& MouseEvent)
{
	return FReply::Handled();
}

FText SDialogueGraphNode::GetNodeText() const
{
	if (UDialogueGraphNode* Node = CastChecked<UDialogueGraphNode>(GraphNode))
	{
		return Node->GetNodeText();
	}

	return FText::GetEmpty();
}

FText SDialogueGraphNode::GetEventsText() const
{
	FString EvtText = "";

	if (UDialogueGraphNode* DialogueGraphNode = Cast<UDialogueGraphNode>(GraphNode))
	{
		if (DialogueGraphNode->DialogueNode)
		{

			if (DialogueGraphNode->DialogueNode->Events.Num())
			{
				int32 Idx = 0;
				for (auto& Evt : DialogueGraphNode->DialogueNode->Events)
				{
					if (Evt)
					{
						//Only append newline if we have more than one Evtition
						if (Idx == 0)
						{
							EvtText += Evt->GetGraphDisplayText();
						}
						else
						{
							EvtText += LINE_TERMINATOR + Evt->GetGraphDisplayText();
						}

						Idx++;
					}
				}
			}
		}
	}

	return FText::FromString(EvtText);
}

FText SDialogueGraphNode::GetNodeTitleText() const
{
	if (UDialogueGraphNode* DialogueGraphNode = Cast<UDialogueGraphNode>(GraphNode))
	{
		return DialogueGraphNode->GetNodeTitleText();
	}

	return FText::GetEmpty();
}


EVisibility SDialogueGraphNode::GetEventsVis() const
{
	return GetEventsText().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
}


EVisibility SDialogueGraphNode::GetCondsVis() const
{
	return GetConditionsText().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
}

FText SDialogueGraphNode::GetWarningIconTooltip() const
{

	if (const UDialogueEditorSettings* DialogueSettings = GetDefault<UDialogueEditorSettings>())
	{
		if (DialogueSettings->bEnableWarnings)
		{
			if (UDialogueGraphNode* DialogueGraphNode = Cast<UDialogueGraphNode>(GraphNode))
			{
				if (DialogueGraphNode->DialogueNode)
				{
					if (DialogueGraphNode->DialogueNode->IsMissingCues() && DialogueSettings && DialogueSettings->bWarnMissingSoundCues)
					{
						return LOCTEXT("MissingCuesWarning", "This dialogue node is missing audio cues. ");
					}
				}
			}
		}
	}
	return FText::GetEmpty();
}


FSlateColor SDialogueGraphNode::GetWarningIconColor() const
{
	if (UDialogueGraphNode* DialogueGraphNode = Cast<UDialogueGraphNode>(GraphNode))
	{
		if (!GetWarningIconTooltip().IsEmptyOrWhitespace())
		{
			return FSlateColor(FColorList::Red);
		}
	}
	return FSlateColor(FColor::Transparent);
}

FSlateColor SDialogueGraphNode::GetNodeTitleColor() const
{
	return FSlateColor(FColor::White);
}

FText SDialogueGraphNode::GetConditionsText() const
{
	if (UDialogueGraphNode* DialogueGraphNode = Cast<UDialogueGraphNode>(GraphNode))
	{
		if (DialogueGraphNode->DialogueNode)
		{
			FString CondText = "";

			if (DialogueGraphNode->DialogueNode->Conditions.Num())
			{
				int32 Idx = 0;
				for (auto& Cond : DialogueGraphNode->DialogueNode->Conditions)
				{
					if (Cond)
					{
						//Only append newline if we have more than one condition
						if (Idx == 0)
						{
							CondText += Cond->GetGraphDisplayText();
						}
						else
						{
							CondText += LINE_TERMINATOR + Cond->GetGraphDisplayText();
						}
					}
					++Idx;
				}
			}

			return FText::FromString(CondText);
		}
	}
	return FText::GetEmpty();
}


FSlateColor SDialogueGraphNode::GetCondsColor() const
{
	return FColorList::Orange;
}


FSlateColor SDialogueGraphNode::GetEventsColor() const
{
	return FColorList::Grey;
}

FSlateColor SDialogueGraphNode::GetBorderColor() const
{
	if (UDialogueGraphNode* DialogueGraphNode = Cast<UDialogueGraphNode>(GraphNode))
	{
		return DialogueGraphNode->GetGraphNodeColor();
	}

	return FLinearColor::Gray;
}

void SDialogueGraphNodePin::Construct(const FArguments& InArgs, UEdGraphPin* InPin)
{
	this->SetCursor(EMouseCursor::Default);

	bShowLabel = true;

	GraphPinObj = InPin;
	check(GraphPinObj != NULL);

	const UEdGraphSchema* Schema = GraphPinObj->GetSchema();
	check(Schema);

	SBorder::Construct(SBorder::FArguments()
		.BorderImage(this, &SDialogueGraphNodePin::GetPinBorder)
		.BorderBackgroundColor(this, &SDialogueGraphNodePin::GetPinColor)
		.Cursor(EMouseCursor::Crosshairs)
		.Padding(FMargin(10.0f))
	);
}

FSlateColor SDialogueGraphNodePin::GetPinColor() const
{
	return FSlateColor(FLinearColor::White);
}

TSharedRef<SWidget> SDialogueGraphNodePin::GetDefaultValueWidget()
{
	//Todo find out why this is here
	return SNew(STextBlock);
}

const FSlateBrush* SDialogueGraphNodePin::GetPinBorder() const
{
	return FAppStyle::Get().GetBrush(TEXT("Graph.StateNode.Body"));
}

#undef LOCTEXT_NAMESPACE