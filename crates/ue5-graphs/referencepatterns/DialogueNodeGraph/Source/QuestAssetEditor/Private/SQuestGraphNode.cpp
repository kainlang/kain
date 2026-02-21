/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */


#include "SQuestGraphNode.h"

#include "QuestAssetEditorSettings.h"
#include "QuestGraphNode.h"
#include "QuestGraphPin.h"
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


void SQuestGraphNode::Construct(const FArguments& InArgs, UEdGraphNode* InNode)
{
	GraphNode = InNode;
	UpdateGraphNode();
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SQuestGraphNode::UpdateGraphNode()
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
			.ColorAndOpacity( this, &SQuestGraphNode::GetNodeTitleIconColor )
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
				.BorderBackgroundColor( this, &SQuestGraphNode::GetNodeTitleColor )
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Top)
					.Padding(FMargin(0.f, 0.f, 4.f, 0.f))
					.AutoWidth()
					[
						SNew(SImage)
						.Image(IconBrush)
						.ColorAndOpacity(this, &SQuestGraphNode::GetNodeTitleIconColor)
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
			.BorderBackgroundColor( this, &SQuestGraphNode::GetNodeTitleIconColor )
			[
				SNew(SSpacer)
				.Size(FVector2D(20,20))
			]
		];

	SetDefaultTitleAreaWidget(DefaultTitleAreaWidget);

	SAssignNew(TitleLODBranchNode, SLevelOfDetailBranchNode)
	.UseLowDetailSlot(this, &SQuestGraphNode::UseLowDetailNodeTitles)
	.LowDetail()
	[
		SNew(SBorder)
		.BorderImage( FAppStyle::GetBrush("Graph.Node.ColorSpill") )
		.Padding( FMargin(75.0f, 22.0f) ) // Saving enough space for a 'typical' title so the transition isn't quite so abrupt
		.BorderBackgroundColor( this, &SQuestGraphNode::GetNodeTitleColor )
	]
	.HighDetail()
	[
		DefaultTitleAreaWidget
	];

	if (!SWidget::GetToolTip().IsValid())
	{
		TSharedRef<SToolTip> DefaultToolTip = IDocumentation::Get()->CreateToolTip( TAttribute< FText >( this, &SQuestGraphNode::GetNodeTooltip ), NULL, GraphNode->GetDocumentationLink(), GraphNode->GetDocumentationExcerptName() );
		SetToolTip(DefaultToolTip);
	}

	// Setup a meta tag for this node
	FGraphNodeMetaData TagMeta(TEXT("Graphnode"));
	PopulateMetaTag(&TagMeta);
	
	TSharedPtr<SVerticalBox> InnerVerticalBox;
	this->ContentScale.Bind( this, &SQuestGraphNode::GetContentScale );


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
					.ColorAndOpacity(this, &SQuestGraphNode::GetNodeBodyColor)
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
			.Text(this, &SQuestGraphNode::GetNodeComment)
			.OnTextCommitted(this, &SQuestGraphNode::OnCommentTextCommitted)
			.OnToggled(this, &SQuestGraphNode::OnCommentBubbleToggled)
			.ColorAndOpacity(CommentColor)
			.AllowPinning(true)
			.EnableTitleBarBubble(true)
			.EnableBubbleCtrls(true)
			.GraphLOD(this, &SQuestGraphNode::GetCurrentLOD)
			.IsGraphNodeHovered(this, &SQuestGraphNode::IsHovered);

		GetOrAddSlot(ENodeZone::TopCenter)
			.SlotOffset(TAttribute<FVector2D>(CommentBubble.Get(), &SCommentBubble::GetOffset))
			.SlotSize(TAttribute<FVector2D>(CommentBubble.Get(), &SCommentBubble::GetSize))
			.AllowScaling(TAttribute<bool>(CommentBubble.Get(), &SCommentBubble::IsScalingAllowed))
			.VAlign(VAlign_Top)
			[
				CommentBubble.ToSharedRef()
			];
	}

	//this->ContentScale.Bind(this, &SQuestGraphNode::GetContentScale);
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

	
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

TSharedRef<SWidget> SQuestGraphNode::CreateNodeContentArea()
{	
	UQuestGraphNode * questNode = Cast<UQuestGraphNode>(GraphNode);
	UQuestNodeInfo * nodeInfo = Cast<UQuestNodeInfo>(questNode->GetNodeInfo());
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
				/*
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SQuestActorNameCombo)
					.ParentNode(this)
					.dataModel(dialogNode)
				]*/
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

FText SQuestGraphNode::ConstructDetailsText(UQuestNodeInfo* NodeInfo)
{
	return NodeInfo->GetGraphNodeDescription();
}


void SQuestGraphNode::AddPin(const TSharedRef<SGraphPin>& PinToAdd)
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

