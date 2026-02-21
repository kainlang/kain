// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SGraphPin_ObjectPin.h"

#include "Configuration/SMProjectEditorSettings.h"
#include "Graph/Nodes/PropertyNodes/SMGraphK2Node_PropertyNode_Base.h"
#include "Utilities/SMBlueprintEditorUtils.h"
#include "Utilities/SMPropertyUtils.h"
#include "Utilities/SMSoundUtils.h"

#include "AssetThumbnail.h"
#include "SMUnrealTypeDefs.h"
#include "Sound/SoundBase.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SToolTip.h"

#define LOCTEXT_NAMESPACE "SMObjectPin"

TSharedPtr<SGraphPin> FSMObjectPinFactory::CreatePin(UEdGraphPin* InPin) const
{
	if (!InPin)
	{
		return nullptr;
	}

	const UEdGraphNode* OwningNode = InPin->GetOwningNodeUnchecked();
	if (OwningNode == nullptr)
	{
		return nullptr;
	}

	const ESMPinOverride OverridePins = FSMBlueprintEditorUtils::GetProjectEditorSettings()->OverrideObjectPins;

	if ((!OwningNode->IsA<USMGraphK2Node_PropertyNode_Base>() &&
		OverridePins != ESMPinOverride::AllBlueprints)
		|| OverridePins == ESMPinOverride::None)
	{
		// User has opted not to override generic object pins.
		return nullptr;
	}

	bool bValidCategory = false;
	const UClass* MetaClass = nullptr;
	if (InPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Object)
	{
		MetaClass = Cast<UClass>(InPin->PinType.PinSubCategoryObject.Get());
		bValidCategory = !(MetaClass && MetaClass->IsChildOf<UScriptStruct>());
	}
	else if (InPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Interface)
	{
		bValidCategory = true;
	}
	else if (InPin->PinType.PinCategory == UEdGraphSchema_K2::PC_SoftObject)
	{
		// Make sure we aren't overriding soft actor pointers.
		bValidCategory = !(InPin->PinType.PinSubCategoryObject.IsValid() &&
			(MetaClass = Cast<UClass>(InPin->PinType.PinSubCategoryObject.Get())) != nullptr &&
			MetaClass->IsChildOf<AActor>());
	}

	if (bValidCategory)
	{
		if (MetaClass && MetaClass->IsChildOf(USoundBase::StaticClass()))
		{
			return SNew(SGraphPin_SoundObjectPin, InPin, MetaClass);
		}
		
		return SNew(SGraphPin_ObjectPin, InPin)
		.MetaClass(MetaClass);
	}

	return nullptr;
}

void FSMObjectPinFactory::RegisterFactory()
{
	FEdGraphUtilities::RegisterVisualPinFactory(MakeShared<FSMObjectPinFactory>());
}

void SGraphPin_ObjectPin::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	MetaClass = InArgs._MetaClass;
	
	if (InGraphPinObj)
	{
		OwningPropertyNode = Cast<USMGraphK2Node_PropertyNode_Base>(InGraphPinObj->GetOwningNodeUnchecked());
	}
	
	SGraphPinObject::Construct(SGraphPinObject::FArguments(), InGraphPinObj);
}

TSharedRef<SWidget> SGraphPin_ObjectPin::GetDefaultValueWidget()
{
	if (GraphPinObj == nullptr)
	{
		return SNullWidget::NullWidget;
	}

	const UEdGraphSchema* Schema = GraphPinObj->GetSchema();

	if (Schema == nullptr)
	{
		return SNullWidget::NullWidget;
	}

	if (ShouldDisplayAsSelfPin())
	{
		return SNew(SEditableTextBox)
			.Style(FSMUnrealAppStyle::Get(), "Graph.EditableTextBox")
			.Text(this, &SGraphPin_ObjectPin::GetValue)
			.SelectAllTextWhenFocused(false)
			.Visibility(this, &SGraphPin_ObjectPin::GetDefaultValueVisibility)
			.IsReadOnly(true)
			.ForegroundColor(FSlateColor::UseForeground());
	}
	// Don't show literal buttons for component type objects
	if (Schema->ShouldShowAssetPickerForPin(GraphPinObj))
	{
		bCanDisplayThumbnail = LD::PropertyUtils::ShouldGraphPropertyDisplayThumbnail(OwningPropertyNode.Get());

		// Create widgets
		{
			SAssignNew(AssetPickerAnchor, SComboButton)
				.ButtonStyle(FSMUnrealAppStyle::Get(), "PropertyEditor.AssetComboStyle")
				.ForegroundColor(this, &SGraphPin_ObjectPin::OnGetComboForeground)
				.ContentPadding(FMargin(2, 2, 2, 1))
				.ButtonColorAndOpacity(this, &SGraphPin_ObjectPin::OnGetWidgetBackground)
				.MenuPlacement(MenuPlacement_BelowAnchor)
				.IsEnabled(this, &SGraphPin::IsEditingEnabled)
				.ButtonContent()
				[
					SNew(STextBlock)
					.ColorAndOpacity(this, &SGraphPin_ObjectPin::OnGetComboForeground)
					.TextStyle(FSMUnrealAppStyle::Get(), "PropertyEditor.AssetClass")
					.Font(FSMUnrealAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
					.Text(this, &SGraphPin_ObjectPin::OnGetComboTextValue)
					.ToolTipText(this, &SGraphPin_ObjectPin::GetObjectToolTip)
				]
				.OnGetMenuContent(this, &SGraphPin_ObjectPin::GenerateAssetPicker);

			CreateButtonsWidget();
		}
		
		// Create content based on whether the thumbnail is displayed or not.

		// With thumbnails we reverse the typical display seen in the details panel for consistency with how the
		// BP pin version is used in the engine and how property pins are handled on our graph nodes.

		// Without thumbnails the display is almost identical to the engine version except we increase the max width
		// of the text box.
		
		const TSharedPtr<SWidget> Box1Content = bCanDisplayThumbnail ? CreateVerticalSelectionWidget() : AssetPickerAnchor;
		const TSharedPtr<SWidget> Box2Content = bCanDisplayThumbnail ? CreateThumbnailWidget() : Buttons;
		
		return
			SNew(SHorizontalBox)
			.Visibility(this, &SGraphPin::GetDefaultValueVisibility)
			+ SHorizontalBox::Slot()
			  .AutoWidth()
			  .Padding(2, 0)
			  .MaxWidth(300.0f) // 400 is PinString max length, but we don't want to cut off our label which we handle separately
			[
				// -- With Thumbnail --
				// Combo box
				// Buttons
				// -- Without Thumbnail --
				// Combo box
				Box1Content.ToSharedRef()
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				// -- With Thumbnail --
				// Thumbnail
				// -- Without Thumbnail --
				// Buttons
				Box2Content.ToSharedRef()
			];
	}

	return SNullWidget::NullWidget;
}

void SGraphPin_ObjectPin::OnAssetSelectedFromPicker(const FAssetData& AssetData)
{
	SGraphPinObject::OnAssetSelectedFromPicker(AssetData);
	UpdateThumbnail();
}

FReply SGraphPin_ObjectPin::OnClickUse()
{
	FReply Reply = SGraphPinObject::OnClickUse();
	UpdateThumbnail();
	return Reply;
}

TSharedPtr<SWidget> SGraphPin_ObjectPin::CreateThumbnailWidget()
{
	constexpr int32 ThumbnailSizeSmall = 48; // Can't go smaller because the class icon overlay doesn't resize.
	
	const FAssetData& AssetData = GetAssetData(false);

	// Create main thumbnail
	Thumbnail = MakeShareable(new FAssetThumbnail(AssetData, ThumbnailSizeSmall, ThumbnailSizeSmall,
													  UThumbnailManager::Get().GetSharedThumbnailPool()));

	FAssetThumbnailConfig ThumbnailConfig;
	{
		ThumbnailConfig.bAllowFadeIn = false;
		ThumbnailConfig.bAllowHintText = false;
		ThumbnailConfig.bAllowRealTimeOnHovered = false; // we display our own tooltip on hover
		ThumbnailConfig.bForceGenericThumbnail = false;
	}
	
	const TSharedRef<SOverlay> ItemContentsOverlay = SNew(SOverlay);
	ItemContentsOverlay->AddSlot()
	[
		Thumbnail->MakeThumbnailWidget(MoveTemp(ThumbnailConfig))
	];

	// Create an overlay so children can add their own widgets on top of the thumbnail.
	TSharedPtr<SOverlay> ThumbnailOverlay =
		SNew(SOverlay)
		+SOverlay::Slot()
		[
			SNew(SBox)
			.Padding(0)
			.WidthOverride(ThumbnailSizeSmall)
			.HeightOverride(ThumbnailSizeSmall)
			.ToolTip(CreateThumbnailTooltipWidget())
			[
				ItemContentsOverlay
			]
		];

	const TSharedPtr<SWidget> ThumbnailOverlayContent = CreateThumbnailOverlayWidget();
	if (ThumbnailOverlayContent.IsValid())
	{
		ThumbnailOverlay->AddSlot()
		[
			ThumbnailOverlayContent.ToSharedRef()
		];
	}

	return ThumbnailOverlay;
}

TSharedPtr<SToolTip> SGraphPin_ObjectPin::CreateThumbnailTooltipWidget()
{
	constexpr int32 ThumbnailSizeLarge = 128;
	const FAssetData& AssetData = GetAssetData(false);
	// Create tooltip thumbnail
	ThumbnailTooltip = MakeShareable(new FAssetThumbnail(AssetData, ThumbnailSizeLarge, ThumbnailSizeLarge,
									  UThumbnailManager::Get().GetSharedThumbnailPool()));

	FAssetThumbnailConfig ThumbnailConfig;
	{
		ThumbnailConfig.bAllowFadeIn = false;
		ThumbnailConfig.bAllowHintText = false;
		ThumbnailConfig.bAllowRealTimeOnHovered = false;
		ThumbnailConfig.bForceGenericThumbnail = false;
	}
	
	const TSharedPtr<SToolTip> ToolTip = SNew(SToolTip).Content()
	[
		ThumbnailTooltip->MakeThumbnailWidget(MoveTemp(ThumbnailConfig))
	];

	return ToolTip;
}

TSharedPtr<SHorizontalBox> SGraphPin_ObjectPin::CreateButtonsWidget()
{
	return SAssignNew(Buttons, SHorizontalBox)
		// Use button
		+ SHorizontalBox::Slot()
		  .AutoWidth()
		  .Padding(1, 0)
		  .VAlign(VAlign_Center)
		[
			SAssignNew(UseButton, SButton)
			.ButtonStyle(FSMUnrealAppStyle::Get(), "NoBorder")
			.ButtonColorAndOpacity(this, &SGraphPin_ObjectPin::OnGetWidgetBackground)
			.OnClicked(GetOnUseButtonDelegate())
			.ContentPadding(1.f)
			.ToolTipText(NSLOCTEXT("GraphEditor", "ObjectGraphPin_Use_Tooltip", "Use asset browser selection"))
			.IsEnabled(this, &SGraphPin::IsEditingEnabled)
			[
				SNew(SImage)
				.ColorAndOpacity(this, &SGraphPin_ObjectPin::OnGetWidgetForeground)
				.Image(FSMUnrealAppStyle::GetBrush(TEXT("Icons.CircleArrowLeft")))
			]
		]
		// Browse button
		+ SHorizontalBox::Slot()
		  .AutoWidth()
		  .Padding(1, 0)
		  .VAlign(VAlign_Center)
		[
			SAssignNew(BrowseButton, SButton)
			.ButtonStyle(FSMUnrealAppStyle::Get(), "NoBorder")
			.ButtonColorAndOpacity(this, &SGraphPin_ObjectPin::OnGetWidgetBackground)
			.OnClicked(GetOnBrowseButtonDelegate())
			.ContentPadding(0)
			.ToolTipText(NSLOCTEXT("GraphEditor", "ObjectGraphPin_Browse_Tooltip", "Browse"))
			[
				SNew(SImage)
				.ColorAndOpacity(this, &SGraphPin_ObjectPin::OnGetWidgetForeground)
				.Image(FSMUnrealAppStyle::GetBrush(TEXT("Icons.Search")))
			]
		];
}

TSharedPtr<SWidget> SGraphPin_ObjectPin::CreateVerticalSelectionWidget()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			AssetPickerAnchor.ToSharedRef()
		]
		+ SVerticalBox::Slot()
		.HAlign(HAlign_Right)
		[
			Buttons.ToSharedRef()
		];
}

void SGraphPin_ObjectPin::UpdateThumbnail()
{
	const FAssetData& AssetData = GetAssetData(false);
	if (Thumbnail)
	{
		Thumbnail->SetAsset(AssetData);
	}
	if (ThumbnailTooltip)
	{
		ThumbnailTooltip->SetAsset(AssetData);
	}
}

void SGraphPin_SoundObjectPin::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj, const UClass* InClass)
{
	SGraphPin_ObjectPin::FArguments Args;
	Args.MetaClass(InClass);
	SGraphPin_ObjectPin::Construct(MoveTemp(Args), InGraphPinObj);
}

TSharedPtr<SWidget> SGraphPin_SoundObjectPin::CreateThumbnailOverlayWidget()
{
	TSharedPtr<SBox> Box;
	SAssignNew(Box, SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(FMargin(2));

	auto OnGetVisibilityLambda = [this, Box]() -> EVisibility
	{
		return (Box.IsValid() && Box->IsHovered() && IsPlaySoundEnabled()) ? EVisibility::Visible : EVisibility::Hidden;
	};

	const TSharedPtr<SWidget> Widget =
		SNew(SBox)
		.Visibility_Lambda(OnGetVisibilityLambda)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SBorder)
			.BorderImage(FSMUnrealAppStyle::GetBrush("ContentBrowser.ThumbnailShadow"))
			.BorderBackgroundColor(FLinearColor::Black)
			.ForegroundColor(FSlateColor::UseForeground())
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				CreatePlaySoundButton(true).ToSharedRef()
			]
		];

	Box->SetContent(Widget.ToSharedRef());
	Box->SetVisibility(EVisibility::Visible);

	return Box;
}

TSharedPtr<SToolTip> SGraphPin_SoundObjectPin::CreateThumbnailTooltipWidget()
{
	return nullptr;
}

TSharedPtr<SHorizontalBox> SGraphPin_SoundObjectPin::CreateButtonsWidget()
{
	TSharedPtr<SHorizontalBox> BaseButtons = SGraphPin_ObjectPin::CreateButtonsWidget();

	if (!CanDisplayThumbnail())
	{
		// Add our play sound button before the default buttons but only if the thumbnail is turned off since it has
		// a play button.
		BaseButtons->InsertSlot(0)
		.AutoWidth()
		.Padding(1, 0)
		.VAlign(VAlign_Center)
		[
			CreatePlaySoundButton().ToSharedRef()
		];
	}

	return BaseButtons;
}

TSharedPtr<SWidget> SGraphPin_SoundObjectPin::CreatePlaySoundButton(bool bUseLargeButton)
{
	return SNew(SButton)
		.ButtonStyle(FSMUnrealAppStyle::Get(), "NoBorder")
		.ButtonColorAndOpacity(this, &SGraphPin_SoundObjectPin::OnGetWidgetBackground)
		.OnClicked(this, &SGraphPin_SoundObjectPin::OnPlaySoundClicked)
		.IsEnabled(this, &SGraphPin_SoundObjectPin::IsPlaySoundEnabled)
		.ContentPadding(0)
		.ToolTipText(this, &SGraphPin_SoundObjectPin::GetPlaySoundTooltip)
		[
			SNew(SImage)
			.ColorAndOpacity(this, &SGraphPin_SoundObjectPin::OnGetWidgetForeground)
			.Image(this, &SGraphPin_SoundObjectPin::GetPlaySoundDisplayBrush, bUseLargeButton)
		];
}

const FSlateBrush* SGraphPin_SoundObjectPin::GetPlaySoundDisplayBrush(bool bUseLargeButton) const
{
	const FAssetData& AssetData = GetAssetData(false);
	if (LD::SoundUtils::IsSoundPlaying(AssetData))
	{
		return FSMUnrealAppStyle::GetBrush(bUseLargeButton ?
		"MediaAsset.AssetActions.Stop.Large" :
		"MediaAsset.AssetActions.Stop.Small");
	}

	return FSMUnrealAppStyle::GetBrush(bUseLargeButton ?
		"MediaAsset.AssetActions.Play.Large" :
		"MediaAsset.AssetActions.Play.Small");
}

FText SGraphPin_SoundObjectPin::GetPlaySoundTooltip() const
{
	const FAssetData& AssetData = GetAssetData(false);
	if (LD::SoundUtils::IsSoundPlaying(AssetData))
	{
		return LOCTEXT("Blueprint_StopSoundToolTip", "Stop selected Sound");
	}

	return LOCTEXT("Blueprint_PlaySoundToolTip", "Play selected Sound");
}

FReply SGraphPin_SoundObjectPin::OnPlaySoundClicked()
{
	const FAssetData& AssetData = GetAssetData(false);
	if (LD::SoundUtils::IsSoundPlaying(AssetData))
	{
		LD::SoundUtils::StopSound();
	}
	else
	{
		// Load and play sound
		LD::SoundUtils::PlaySound(Cast<USoundBase>(AssetData.GetAsset()));
	}
	return FReply::Handled();
}

bool SGraphPin_SoundObjectPin::IsPlaySoundEnabled() const
{
	const FAssetData& AssetData = GetAssetData(false);
	return AssetData.IsValid();
}

#undef LOCTEXT_NAMESPACE
