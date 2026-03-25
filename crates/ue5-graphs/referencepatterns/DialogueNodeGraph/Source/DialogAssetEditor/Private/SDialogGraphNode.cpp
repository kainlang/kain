/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */


#include "SDialogGraphNode.h"

#include "DialogAssetEditorSettings.h"
#include "DialogGraphNode.h"
#include "DialogGraphPin.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "GraphEditorSettings.h"
#include "IDocumentation.h"
#include "SCommentBubble.h"
#include "SGraphPin.h"
#include "SLevelOfDetailBranchNode.h"
#include "TutorialMetaData.h"
#include "Interfaces/IPluginManager.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "SActorNameCombo.h"


void SDialogGraphNode::Construct(const FArguments& InArgs, UEdGraphNode* InNode)
{
	GraphNode = InNode;
	UpdateGraphNode();
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SDialogGraphNode::UpdateGraphNode()
{
	// Nettoyer les enfants avant de les réajouter
	
	InputPins.Empty();
	OutputPins.Empty();
	RightNodeBox.Reset();
	LeftNodeBox.Reset();

	TSharedPtr<SVerticalBox> MainVerticalBox;
	SetupErrorReporting();
	TSharedPtr<SNodeTitle> NodeTitle = SNew(SNodeTitle, GraphNode);

	IconColor = FLinearColor::White;
	const FSlateBrush* IconBrush = nullptr;
	if (GraphNode != NULL && GraphNode->ShowPaletteIconOnNode())
	{
		IconBrush = GraphNode->GetIconAndTint(IconColor).GetOptionalIcon();
	}

	TSharedRef<SOverlay> DefaultTitleAreaWidget =
		SNew(SOverlay)
		+SOverlay::Slot()
		[
			SNew(SImage)
			.Image( FAppStyle::GetBrush("Graph.Node.TitleGloss") )
			.ColorAndOpacity( this, &SDialogGraphNode::GetNodeTitleIconColor )
		]
		+SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Fill)
			[
				SNew(SBorder)
				.BorderImage( FAppStyle::GetBrush("Graph.Node.ColorSpill") )
				.Padding(TitleBorderMargin)
				.BorderBackgroundColor( this, &SDialogGraphNode::GetNodeTitleColor )
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Top)
					.Padding(FMargin(0.f, 0.f, 4.f, 0.f))
					.AutoWidth()
					[
						SNew(SImage)
						.Image(IconBrush)
						.ColorAndOpacity(this, &SDialogGraphNode::GetNodeTitleIconColor)
					]
					+ SHorizontalBox::Slot()
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							CreateTitleWidget(NodeTitle)
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							NodeTitle.ToSharedRef()
						]
					]
				]
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.Padding(0, 0, 5, 0)
			.AutoWidth()
			[
				CreateTitleRightWidget()
			]
		]
		+SOverlay::Slot()
		.VAlign(VAlign_Top)
		[
			SNew(SBorder)
			.Visibility(EVisibility::HitTestInvisible)			
			.BorderImage( FAppStyle::GetBrush( "Graph.Node.TitleHighlight" ) )
			.BorderBackgroundColor( this, &SDialogGraphNode::GetNodeTitleIconColor )
			[
				SNew(SSpacer)
				.Size(FVector2D(20,20))
			]
		];

	SetDefaultTitleAreaWidget(DefaultTitleAreaWidget);

	SAssignNew(TitleLODBranchNode, SLevelOfDetailBranchNode)
	.UseLowDetailSlot(this, &SDialogGraphNode::UseLowDetailNodeTitles)
	.LowDetail()
	[
		SNew(SBorder)
		.BorderImage( FAppStyle::GetBrush("Graph.Node.ColorSpill") )
		.Padding( FMargin(75.0f, 22.0f) ) // Saving enough space for a 'typical' title so the transition isn't quite so abrupt
		.BorderBackgroundColor( this, &SDialogGraphNode::GetNodeTitleColor )
	]
	.HighDetail()
	[
		DefaultTitleAreaWidget
	];

	if (!SWidget::GetToolTip().IsValid())
	{
		TSharedRef<SToolTip> DefaultToolTip = IDocumentation::Get()->CreateToolTip( TAttribute< FText >( this, &SDialogGraphNode::GetNodeTooltip ), NULL, GraphNode->GetDocumentationLink(), GraphNode->GetDocumentationExcerptName() );
		SetToolTip(DefaultToolTip);
	}

	// Setup a meta tag for this node
	FGraphNodeMetaData TagMeta(TEXT("Graphnode"));
	PopulateMetaTag(&TagMeta);
	
	TSharedPtr<SVerticalBox> InnerVerticalBox;
	this->ContentScale.Bind( this, &SDialogGraphNode::GetContentScale );


	InnerVerticalBox = SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Top)
		.Padding(Settings->GetNonPinNodeBodyPadding())
		[
			TitleLODBranchNode.ToSharedRef()
		]

		+SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Top)
		[
			CreateNodeContentArea()
		];

	TSharedPtr<SWidget> EnabledStateWidget = GetEnabledStateWidget();
	if (EnabledStateWidget.IsValid())
	{
		InnerVerticalBox->AddSlot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Top)
			.Padding(FMargin(2, 0))
			[
				EnabledStateWidget.ToSharedRef()
			];
	}
	
	InnerVerticalBox->AddSlot()
		.AutoHeight()
		.Padding(Settings->GetNonPinNodeBodyPadding())
		[
			ErrorReporting->AsWidget()
		];

	InnerVerticalBox->AddSlot()
		.AutoHeight()
		.Padding(Settings->GetNonPinNodeBodyPadding())
		[
			VisualWarningReporting->AsWidget()
		];

	this->GetOrAddSlot( ENodeZone::Center )
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SAssignNew(MainVerticalBox, SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SOverlay)
				.AddMetaData<FGraphNodeMetaData>(TagMeta)
				+SOverlay::Slot()
				.Padding(Settings->GetNonPinNodeBodyPadding())
				[
					SNew(SImage)
					.Image(GetNodeBodyBrush())
					.ColorAndOpacity(this, &SDialogGraphNode::GetNodeBodyColor)
				]
				+SOverlay::Slot()
				[
					InnerVerticalBox.ToSharedRef()
				]
			]			
		];

	bool SupportsBubble = true;
	if (GraphNode != nullptr)
	{
		SupportsBubble = GraphNode->SupportsCommentBubble();
	}

	if (SupportsBubble)
	{
		// Create comment bubble
		TSharedPtr<SCommentBubble> CommentBubble;
		const FSlateColor CommentColor = GetDefault<UGraphEditorSettings>()->DefaultCommentNodeTitleColor;

		SAssignNew(CommentBubble, SCommentBubble)
			.GraphNode(GraphNode)
			.Text(this, &SDialogGraphNode::GetNodeComment)
			.OnTextCommitted(this, &SDialogGraphNode::OnCommentTextCommitted)
			.OnToggled(this, &SDialogGraphNode::OnCommentBubbleToggled)
			.ColorAndOpacity(CommentColor)
			.AllowPinning(true)
			.EnableTitleBarBubble(true)
			.EnableBubbleCtrls(true)
			.GraphLOD(this, &SDialogGraphNode::GetCurrentLOD)
			.IsGraphNodeHovered(this, &SDialogGraphNode::IsHovered);

		GetOrAddSlot(ENodeZone::TopCenter)
			.SlotOffset(TAttribute<FVector2D>(CommentBubble.Get(), &SCommentBubble::GetOffset))
			.SlotSize(TAttribute<FVector2D>(CommentBubble.Get(), &SCommentBubble::GetSize))
			.AllowScaling(TAttribute<bool>(CommentBubble.Get(), &SCommentBubble::IsScalingAllowed))
			.VAlign(VAlign_Top)
			[
				CommentBubble.ToSharedRef()
			];
	}

	//this->ContentScale.Bind(this, &SDialogGraphNode::GetContentScale);
	///////////////////////////////////////////////////////////////////////////
	/*
	this->ContentScale.Bind(this, &SDialogGraphNode::GetContentScale);
	this->GetOrAddSlot(ENodeZone::Center)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				// Boîte pour le texte multiligne
				SNew(SBox)
				.Padding(FMargin(4.0f, 2.0f))
				[
					SAssignNew(NodeTextBlock, STextBlock)
					.Text(nodeInfo->Replique.SpokenText)
					.AutoWrapText(true)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				// Boîte pour les pins
				SAssignNew(RightNodeBox, SVerticalBox)
			]
		];
	*/

	CreatePinWidgets();
	/*
	// Ajouter les pins au nœud
	for (auto& Pin : GraphNode->Pins)
	{
		TSharedRef<SGraphPin> NewPin = SNew(SDialogGraphPin, Pin);
		this->AddPin(NewPin);
	}
	*/
	
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

TSharedRef<SWidget> SDialogGraphNode::CreateNodeContentArea()
{	
	UDialogGraphNode * dialogNode = Cast<UDialogGraphNode>(GraphNode);
	UDialogNodeInfo * nodeInfo = Cast<UDialogNodeInfo>(dialogNode->GetNodeInfo());
	FText detailsText = ConstructDetailsText(nodeInfo);

	FString PluginContentDir = IPluginManager::Get().FindPlugin("DialogueMasterAssetEditor")->GetBaseDir() / TEXT("Content/");
	FString FontPath = PluginContentDir / TEXT("Fonts/Roboto-Regular.ttf");
	FString BoldFontPath = PluginContentDir / TEXT("Fonts/Roboto-Bold.ttf");
	
	// Create style for multiline rich text :
	FTextBlockStyle MyTextStyle = FTextBlockStyle()
	.SetFont(FSlateFontInfo(FontPath, 12))
	.SetColorAndOpacity(FSlateColor(FLinearColor::White));

	FTextBlockStyle BoldTextStyle = FTextBlockStyle()
	.SetFont(FSlateFontInfo(BoldFontPath, 14))
	.SetColorAndOpacity(FSlateColor(FLinearColor::White));
	
	FTextBlockStyle GreenTitleTextStyle = FTextBlockStyle()
	.SetFont(FSlateFontInfo(BoldFontPath, 16))
	.SetColorAndOpacity(FSlateColor(FLinearColor::Green));

	FTextBlockStyle BlueTitleTextStyle = FTextBlockStyle()
	.SetFont(FSlateFontInfo(BoldFontPath, 16))
	.SetColorAndOpacity(FSlateColor(FLinearColor::Blue));
	
	FTextBlockStyle RedTitleTextStyle = FTextBlockStyle()
		.SetFont(FSlateFontInfo(BoldFontPath, 16))
		.SetColorAndOpacity(FSlateColor(FLinearColor::Red));

	FTextBlockStyle GreenBoldTextStyle = FTextBlockStyle()
	.SetFont(FSlateFontInfo(BoldFontPath, 12))
	.SetColorAndOpacity(FSlateColor(FLinearColor::Green));

	
	FTextBlockStyle RedBoldTextStyle = FTextBlockStyle()
		.SetFont(FSlateFontInfo(BoldFontPath, 12))
		.SetColorAndOpacity(FSlateColor(FLinearColor::Red));

	FTextBlockStyle OrangeBoldTextStyle = FTextBlockStyle()
		.SetFont(FSlateFontInfo(BoldFontPath, 12))
		.SetColorAndOpacity(FSlateColor(FLinearColor::Yellow));

	FTextBlockStyle BlueBoldTextStyle = FTextBlockStyle()
		.SetFont(FSlateFontInfo(BoldFontPath, 12))
		.SetColorAndOpacity(FSlateColor(FLinearColor::Blue));
	
	FSlateStyleSet* MyStyleSet = new FSlateStyleSet("MyStyle");
	MyStyleSet->Set("RichText.GreenTitle", GreenTitleTextStyle);
	MyStyleSet->Set("RichText.RedTitle", RedTitleTextStyle);
	MyStyleSet->Set("RichText.BlueTitle", BlueTitleTextStyle);
	MyStyleSet->Set("RichText.Green", GreenBoldTextStyle);
	MyStyleSet->Set("RichText.Red", RedBoldTextStyle);
	MyStyleSet->Set("RichText.Bold", BoldTextStyle);
	MyStyleSet->Set("RichText.Orange", OrangeBoldTextStyle);
	MyStyleSet->Set("RichText.Blue", BlueBoldTextStyle);
	MyStyleSet->Set("h", RedBoldTextStyle);
	MyStyleSet->Set("b", BoldTextStyle);

	if(!nodeInfo->isBranchingNode)
	{
		// NODE CONTENT AREA
		return SNew(SBorder)
			.BorderImage( FAppStyle::GetBrush("NoBorder") )
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding( FMargin(0,3) )
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.AutoWidth()
				[
					// LEFT
					SAssignNew(LeftNodeBox, SVerticalBox)
				]
				// Insert the text here ...
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				.FillWidth(1.0f)
				[
					// CENTER

					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SActorNameCombo)
						.ParentNode(this)
						.dataModel(dialogNode)
					]
					+SVerticalBox::Slot()
					.FillHeight(1.0)
					[
						// Boîte pour le texte multiligne
						SNew(SBox)
						.Padding(FMargin(4.0f, 2.0f))
						[
							SAssignNew(NodeTextBlock, SRichTextBlock)
							.Text(detailsText)
							.AutoWrapText(true)
							.DecoratorStyleSet(MyStyleSet)
							.TextStyle(&MyTextStyle)
						]
					]
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				[
					// RIGHT
					SAssignNew(RightNodeBox, SVerticalBox)
				]
			];
	}

	// Pas de combo box quand c'est un branching node ...
	// NODE CONTENT AREA
	return SNew(SBorder)
		.BorderImage( FAppStyle::GetBrush("NoBorder") )
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.Padding( FMargin(0,3) )
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.AutoWidth()
			[
				// LEFT
				SAssignNew(LeftNodeBox, SVerticalBox)
			]
			// Insert the text here ...
			+SHorizontalBox::Slot()
			.HAlign(HAlign_Center)
			.FillWidth(1.0f)
			[
				// CENTER					
				// Boîte pour le texte multiligne
				SNew(SBox)
				.Padding(FMargin(4.0f, 2.0f))
				[
					SAssignNew(NodeTextBlock, SRichTextBlock)
					.Text(detailsText)
					.AutoWrapText(true)
					.DecoratorStyleSet(MyStyleSet)
					.TextStyle(&MyTextStyle)
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			[
				// RIGHT
				SAssignNew(RightNodeBox, SVerticalBox)
			]
		];
	
}

FString SDialogGraphNode::GetComparatorString(TEnumAsByte<EValueCheckOperator> EnumAsByte)
{
	FString result = "None";

	switch(EnumAsByte)
	{
	case Equals:
		result = "=";
		break;

	case NotEquals:
		result = "!=";
		break;

	case Greater:
		result = ">";
		break;

	case GreaterOrEquals:
		result = ">=";
		break;

	case Smaller:
		result = "<";
		break;

	case SmallerOrEquals:
		result = "<=";
		break;
	}
	
	return result;
}

FString SDialogGraphNode::ConstructConditionsText(UDialogNodeInfo* NodeInfo)
{
	FString result = "";
	if(NodeInfo->Replique.SwitchPrerequisites.Num() > 0
		||
		NodeInfo->Replique.CounterPrerequisites.Num() > 0
		||
		NodeInfo->Replique.AdvancedPrerequisites.Num() > 0)
	{
		result += "\n<RichText.GreenTitle>Conditions :</>\n";
		bool havePredecessor = false;
		if(NodeInfo->Replique.SwitchPrerequisites.Num() > 0)
		{
			havePredecessor = true;
			result += "<RichText.Green>Switches conditions :</>\n";

			for(FSwitchDialogPrerequisite condition : NodeInfo->Replique.SwitchPrerequisites)
			{
				result += " - " + condition.SwitchPrerequisiteName.ToString() + " = " + (condition.NeededValue ? "True" : "False") + "\n";
			}

			result += "-------------------------------\n";
		}

		if(NodeInfo->Replique.CounterPrerequisites.Num() > 0)
		{
			if(havePredecessor)
				result += "\n";

			havePredecessor = true;
			result += "<RichText.Green>Counters conditions :</>\n";

			for(FCounterDialogPrerequisite condition : NodeInfo->Replique.CounterPrerequisites)
			{
				result += " - " + condition.CounterPrerequisiteName.ToString() + " " + GetComparatorString(condition.ValueCheckOperator) + " " + FString::FromInt(condition.NeededValue) + "\n";
			}
			
			result += "-------------------------------\n";
		}

		if(NodeInfo->Replique.AdvancedPrerequisites.Num() > 0)
		{
			if(havePredecessor)
				result += "\n";

			havePredecessor = true;
			
			result += "<RichText.Green>Advanced conditions :</>\n";

			for(UAdvancedPrerequisiteBase * condition : NodeInfo->Replique.AdvancedPrerequisites)
			{
				if(IsValid(condition))
				{
					result += " - " + condition->GetDescription() + "\n";
				}
				else
				{
					result += " - Invalid selection !\n";
				}
			}
			
			result += "-------------------------------\n";
		}
	}

	return result;
}

FString SDialogGraphNode::GetCounterUpdateActionString(FCounterChangeValue action)
{
	FString result = "None";

	switch(action.ModifyValueOperator)
	{
	case Add:
		result = " += ";
		break;

	case Subtract:
		result = " -= ";
		break;

	case Set:
		result = " = ";
		break;
	}
	
	return result;
}

FString SDialogGraphNode::ConstructActionsText(UDialogNodeInfo* NodeInfo)
{
	FString result = "";

	if(NodeInfo->Replique.DialogActions.Num() > 0
		||
		NodeInfo->Replique.UpdateSwitchValue.Num() > 0
		||
		NodeInfo->Replique.UpdateCounterValue.Num() > 0)
	{
		result += "\n<RichText.RedTitle>Actions :</>\n";

		bool havePredecessor = false;
		
		if(NodeInfo->Replique.UpdateSwitchValue.Num() > 0)
		{
			havePredecessor = true;
			
			result += "<RichText.Red>Modify switches values :</>\n";

			for(FSwitchDialogPrerequisite action : NodeInfo->Replique.UpdateSwitchValue)
			{
				result += " - Set " + action.SwitchPrerequisiteName.ToString() + " to " + (action.NeededValue ? "True" : "False") + "\n";
			}
			
			result += "-------------------------------\n";
		}

		if(NodeInfo->Replique.UpdateCounterValue.Num() > 0)
		{
			if(havePredecessor)
				result += "\n";

			havePredecessor = true;
			result += "<RichText.Red>Modify counters values :</>\n";

			for(FCounterChangeValue action : NodeInfo->Replique.UpdateCounterValue)
			{
				result += " - " + action.CounterName.ToString() + GetCounterUpdateActionString(action) + FString::FromInt(action.NewValue) + "\n";
			}
			
			result += "-------------------------------\n";
		}

		if(NodeInfo->Replique.DialogActions.Num() > 0)
		{
			if(havePredecessor)
				result += "\n";

			havePredecessor = true;
			result += "<RichText.Red>Custom actions :</>\n";

			for(UDialogueMasterAction * action : NodeInfo->Replique.DialogActions)
			{
				if(IsValid(action))
				{
					result += " - " + action->GetDescription() + "\n";
				}
				else
				{
					result += " - Invalid selection !\n";
				}
			}
			
			result += "-------------------------------\n";
		}
	}
	
	return result;
}

FString SDialogGraphNode::ConstructFlagsInfoText(UDialogNodeInfo* NodeInfo)
{
	FString result = "";

	result += "\n<RichText.BlueTitle>Info :</>";

	if(NodeInfo->AutoSelect)
	{
		result += "\n<RichText.Blue>- Auto-selection enabled</>";
	}
	
	if(NodeInfo->PlayableOnlyOnce)
	{
		result += "\n<RichText.Blue>- Playable only once</>";
	}
	
	float duration = NodeInfo->Replique.DefaultDuration;
	FString durationStr = "\n<RichText.Blue>- Duration : " + FString::SanitizeFloat(duration) + " seconds</>";
	if(IsValid(NodeInfo->Replique.VoiceSound) && NodeInfo->Replique.DurationType == ELineDurationType::DEFAULT)
	{
		duration = NodeInfo->Replique.VoiceSound->Duration;

		durationStr = "\n<RichText.Blue>- Duration : " + FString::SanitizeFloat(duration) + " seconds (sound duration)</>";
	}
	else if(NodeInfo->Replique.DurationType == ELineDurationType::NEVER)
	{
		durationStr = "\n<RichText.Blue>- Duration : Infinite (wait until player skip)</>";
	}
	
	result += durationStr;
	


	if(!NodeInfo->SpatializedVoices)
	{
		result += "\n<RichText.Blue>- Voice sound not spatialized</>";
	}

	result += "\n";
	result += (NodeInfo->Skippable ? "<RichText.Blue>- Can be skipped</>" : "<RichText.Red>- Can NOT be skipped</>");
	
	if(NodeInfo->OverrideVoiceParameters)
	{
		result += "\n<RichText.Orange>- Voice parameters override enabled !</>";
	}
	
	result += "\n";
	
	return result;
}

FString SDialogGraphNode::ConstructWarningsText(UDialogNodeInfo* NodeInfo)
{
	FString result = "";
	bool bHaveWarnings = false;
	if (const UDialogAssetEditorSettings* DialogueSettings = GetDefault<UDialogAssetEditorSettings>())
	{
		if(DialogueSettings->bMissingVoiceSoundWarnings)
		{
			if(!IsValid(NodeInfo->Replique.VoiceSound))
			{
				result += "\n<RichText.Orange>Sound for the actor voice is missing !</>";
				bHaveWarnings = true;
			}
		}

		if(DialogueSettings->bMissingBodyAnimationWarnings)
		{
			if(!IsValid(NodeInfo->Replique.BodyAnimMontage))
			{
				result += "\n<RichText.Orange>Body animation montage is missing !</>";
				bHaveWarnings = true;
			}
		}

		if(DialogueSettings->bMissingFacialAnimationWarnings)
		{
			if(!IsValid(NodeInfo->Replique.FaceLipSyncAnimMontage))
			{
				result += "\n<RichText.Orange>Facial animation montage is missing !</>";
				bHaveWarnings = true;
			}
		}

		if(!NodeInfo->Skippable && NodeInfo->Replique.DurationType == ELineDurationType::NEVER)
		{
			result += "\n<RichText.Orange>Node marked to wait the player to skip, but Skippable is false! You must set Skippable to true to avoid the player beeing stuck!</>";
			bHaveWarnings = true;
		}
	}
	if(bHaveWarnings)
	{
		result = "\n<RichText.Orange>Warnings :</>" + result;
	}
	
	return result;
}

FText SDialogGraphNode::ConstructDetailsText(UDialogNodeInfo* NodeInfo)
{
	if(NodeInfo->isBranchingNode) return FText::FromString("");
	
	FText FormatInput = FText::FromString("{SpokenText}\n{ShortText}{FlagsInfo}{ConditionsText}{ActionsText}{Warnings}");
	FFormatNamedArguments Args;

	// Construct short spoken text string :
	FString shortSpokenText = "";
	if(!NodeInfo->Replique.ShortSpokenText.IsEmpty())
	{
		shortSpokenText = "Short text = " + NodeInfo->Replique.ShortSpokenText.ToString() + "\n";
	}
	Args.Add("ShortText", FText::FromString(shortSpokenText));

	// Add spoken text :
	Args.Add("SpokenText", NodeInfo->Replique.SpokenText);

	// Construct important flags info :
	Args.Add("FlagsInfo", FText::FromString(ConstructFlagsInfoText(NodeInfo)));

	// Construct conditions text string :
	Args.Add("ConditionsText", FText::FromString(ConstructConditionsText(NodeInfo)));

	// Construct actions text string :
	Args.Add("ActionsText", FText::FromString(ConstructActionsText(NodeInfo)));

	// Construct warnings text string :
	Args.Add("Warnings", FText::FromString(ConstructWarningsText(NodeInfo)));
	
	return FText::Format(FormatInput, Args);
}


void SDialogGraphNode::AddPin(const TSharedRef<SGraphPin>& PinToAdd)
{
	SGraphNode::AddPin(PinToAdd);
/*
	PinToAdd->SetOwner(SharedThis(this));
	
	// Ajouter le pin à l'interface
	RightNodeBox->AddSlot()
	.AutoHeight()
	[
		PinToAdd
	];

	// Stocker le pin dans la liste des pins
	InputPins.Add(PinToAdd);
	*/
}

