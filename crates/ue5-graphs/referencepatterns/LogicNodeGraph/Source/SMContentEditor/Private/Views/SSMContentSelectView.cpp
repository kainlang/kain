// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SSMContentSelectView.h"

#include "ISMContentEditorModule.h"
#include "SSMProjectConfigurationView.h"
#include "Utilities/SMContentEditorUtils.h"

#include "AssetViewUtils.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "SKismetInspector.h"
#include "SMUnrealTypeDefs.h"
#include "SPrimaryButton.h"
#include "SWarningOrErrorBox.h"
#include "Dialogs/DlgPickPath.h"
#include "Styling/StyleColors.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/SRichTextBlock.h"

#define LOCTEXT_NAMESPACE "SSMContentSelectView"

namespace ContentBrowserDefs
{
	constexpr float MajorItemWidth = 304;
	constexpr float MajorItemHeight = 104;

	FString GetDefaultInstallPath()
	{
		const FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
		const FContentBrowserItemPath CurrentPath = ContentBrowserModule.Get().GetCurrentPath();
		if (CurrentPath.HasInternalPath())
		{
			return CurrentPath.GetInternalPathString();
		}

		return TEXT("/Game");
	}
}

/**
 * Tile representing content view models.
 */
class SSMContentTile : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SSMContentTile) {}
	SLATE_ATTRIBUTE(bool, IsSelected)
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, TSharedPtr<FSMContentItemViewModel> Item)
	{
		check(Item.IsValid());
		check(Item->GetContentAsset().IsValid());

		IsSelected = InArgs._IsSelected;

		const bool bIsUpToDate = !Item->IsInstalledToPluginContent() || LD::ContentEditorPackageUtils::IsInstalledContentUpToDate(Item->GetContentAsset().Get());
		
		ChildSlot
		[
			SNew(SOverlay)
			.ToolTipText(Item->Description)
			.Visibility(EVisibility::Visible) // For tooltips
			// Image
			+ SOverlay::Slot()
			[
				SNew(SImage)
				.Visibility_Lambda([Item]()
				{
					return (Item->GetContentAsset().IsValid() &&
						Item->GetContentAsset()->TitleImage != nullptr) ? EVisibility::Visible : EVisibility::Collapsed;
				})
				.Image(Item->GetTitleBrush())
			]
			// Heading
			+ SOverlay::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Bottom)
			.Padding(FMargin(18.0f, 8.0f))
			[
				SNew(STextBlock)
				.Font(FSMUnrealAppStyle::Get().GetFontStyle("HeadingExtraSmall"))
				.ColorAndOpacity(FLinearColor(1, 1, 1, .9f))
				.TransformPolicy(ETextTransformPolicy::ToUpper)
				.ShadowOffset(FVector2D(1, 1))
				.ShadowColorAndOpacity(FLinearColor(0, 0, 0, .75))
				.Text(Item->Name)
				.WrapTextAt(250.0f)
			]
			// Content type
			+ SOverlay::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			.Padding(FMargin(18.0f, 8.0f))
			[
				SNew(STextBlock)
				.Font(FSMUnrealAppStyle::Get().GetFontStyle("HeadingExtraSmall"))
				.ColorAndOpacity(Item->GetTypeColor())
				.TransformPolicy(ETextTransformPolicy::ToUpper)
				.ShadowOffset(FVector2D(1, 1))
				.ShadowColorAndOpacity(FLinearColor(0, 0, 0, .75))
				.Text(Item->ContentType)
				.WrapTextAt(250.0f)
			]
			// Update Available
			+ SOverlay::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Top)
			.Padding(FMargin(18.0f, 8.0f))
			[
				SNew(SImage)
				.Image(FSMUnrealAppStyle::Get().GetBrush(TEXT("Icons.Info")))
				.ColorAndOpacity(FLinearColor(0.f, 0.6f, 0.75f))
				.ToolTipText(LOCTEXT("UpdateTooltip", "The latest version is not installed."))
				.Visibility(bIsUpToDate ? EVisibility::Collapsed : EVisibility::Visible)
			]
			// Border
			+ SOverlay::Slot()
			[
				SNew(SImage)
				.Visibility(EVisibility::HitTestInvisible)
				.Image(this, &SSMContentTile::GetSelectionOutlineBrush)
			]
		];
	}

private:
	const FSlateBrush* GetSelectionOutlineBrush() const
	{
		const bool bIsSelected = IsSelected.Get();
		const bool bIsTileHovered = IsHovered();

		if (bIsSelected && bIsTileHovered)
		{
			static const FName SelectedHover("ProjectBrowser.ProjectTile.SelectedHoverBorder");
			return FSMUnrealAppStyle::Get().GetBrush(SelectedHover);
		}
		else if (bIsSelected)
		{
			static const FName Selected("ProjectBrowser.ProjectTile.SelectedBorder");
			return FSMUnrealAppStyle::Get().GetBrush(Selected);
		}
		else if (bIsTileHovered)
		{
			static const FName Hovered("ProjectBrowser.ProjectTile.HoverBorder");
			return FSMUnrealAppStyle::Get().GetBrush(Hovered);
		}

		return FStyleDefaults::GetNoBrush();
	}
private:
	TAttribute<bool> IsSelected;
};

/**
 * Simple project directory path and picker widget.
 */
class SSMFilePath : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SSMFilePath)
	{}

	SLATE_ARGUMENT(FText, FolderPath)
	SLATE_ARGUMENT(FText, DestinationName)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		SelectedFolderPath = InArgs._FolderPath;
		DestinationFolderName = InArgs._DestinationName;

		ChildSlot
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Top)
			.Padding(0.0f, 4.0f, 8.0f, 8.0f)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("InstallLocation", "Install Location"))
				.ToolTipText(LOCTEXT("InstallLocationTooltip", "The directory within this project to install the content."))
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Top)
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(595.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SEditableTextBox)
						.IsReadOnly(true)
						.Text(this, &SSMFilePath::GetSelectedFolderPath)
					]
				]
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Top)
			.Padding(2.0f, 2.0f, 0.0f, 0.0f)
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FSMUnrealAppStyle::Get(), "SimpleButton")
				.OnClicked(this, &SSMFilePath::OnOpenDirectoryButtonClicked)
				.ToolTipText(LOCTEXT("BrowseForFolder", "Browse for a folder."))
				.ContentPadding(0)
				[
					SNew(SImage)
					.Image(FSMUnrealAppStyle::Get().GetBrush("Icons.FolderClosed"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				.AutoWidth()
				.Padding(32.0f, 4.0f, 8.0f, 8.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("FolderName", "Folder Name"))
					.ToolTipText(LOCTEXT("FolderNameTooltip", "The folder which will be created at the install location."))
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[
					SNew(SBox)
					.WidthOverride(275.0f)
					[
						SNew(SEditableTextBox)
						.IsReadOnly(false)
						.Text(this, &SSMFilePath::GetDestinationFolderName)
						.OnTextChanged(this, &SSMFilePath::OnDestinationNameChanged)
					]
				]
			]
		];
	}

	FReply OnOpenDirectoryButtonClicked()
	{
		const TSharedRef<SDlgPickPath> PickContentPathDlg =
		SNew(SDlgPickPath)
		.Title(LOCTEXT("ChooseContentPath", "Choose Location to Install Content"))
		.DefaultPath(SelectedFolderPath);

		if (PickContentPathDlg->ShowModal() == EAppReturnType::Ok)
		{
			SelectedFolderPath = PickContentPathDlg->GetPath();
		}

		return FReply::Handled();
	}

	void OnDestinationNameChanged(const FText& InText)
	{
		DestinationFolderName = InText;
	}

	void SetDestinationFolderName(const FText& InText)
	{
		DestinationFolderName = InText;
	}

	FText GetSelectedFolderPath() const
	{
		return SelectedFolderPath;
	}

	FText GetDestinationFolderName() const
	{
		return DestinationFolderName;
	}

private:
	FText SelectedFolderPath;
	FText DestinationFolderName;
};

SSMContentSelectView::~SSMContentSelectView()
{
	ProjectConfigurationBindingView.Reset();
}

void SSMContentSelectView::Construct(const FArguments& InArgs, const TArray<TSharedPtr<FSMContentItemViewModel>>& InContentItems)
{
	ContentItems = InContentItems;

	FilterContent();

	const ISMContentEditorModule& ContentEditorModule = FModuleManager::Get().GetModuleChecked<ISMContentEditorModule>(LOGICDRIVER_CONTENT_MODULE_NAME);
	const bool bIsContentCreationEnabled = ContentEditorModule.IsContentCreationEnabled();

	SAssignNew(ContentTileView, STileView<TSharedPtr<FSMContentItemViewModel>>)
		.ListItemsSource(&FilteredContentItems)
		.SelectionMode(ESelectionMode::Single)
		.ClearSelectionOnClick(false)
		.OnGenerateTile(this, &SSMContentSelectView::MakeContentViewWidget)
		.ItemHeight(ContentBrowserDefs::MajorItemHeight)
		.ItemWidth(ContentBrowserDefs::MajorItemWidth)
		.OnSelectionChanged(this, &SSMContentSelectView::OnContentViewSelectionChanged);

	const TSharedRef<SBorder> RootBox =
		SNew(SBorder)
		.BorderImage(FSMUnrealAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SBox)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding(FSMUnrealAppStyle::Get().GetMargin("StandardDialog.SlotPadding"))
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					//////////////////////////////
					// Search (Hidden for now) | Add New (Requires CVar)
					//////////////////////////////
					SNew(SHorizontalBox)
					/*
					+ SHorizontalBox::Slot()
					.Padding(2.0f, 2.0f)
					.FillWidth(0.8f)
					[
						SNew(SSearchBox)
						.SelectAllTextWhenFocused(true)
						.OnTextChanged(this, &SSMContentSelectView::OnFilterTextChanged)
						.HintText(LOCTEXT("SearchBoxHint", "Search For Content..."))
					]*/
					+ SHorizontalBox::Slot()
					.Padding(2.0f, 2.0f)
					.FillWidth(0.2f)
					.AutoWidth()
					[
						SNew(SButton)
						.Visibility_Lambda([bIsContentCreationEnabled]()
						{
							return bIsContentCreationEnabled ? EVisibility::Visible : EVisibility::Collapsed;
						})
						.Text(LOCTEXT("AddNewText", "Add New"))
						.ToolTipText(LOCTEXT("AddNewToolTip", "Create a new content sample for future projects."))
						.TextStyle(FSMUnrealAppStyle::Get(), "NormalText.Important")
						.ButtonStyle(FSMUnrealAppStyle::Get(), "FlatButton.Primary")
						.OnClicked_Lambda([this]()
						{
							bCreateNewSelected = true;
							RequestDestroyWindow();
							return FReply::Handled();
						})
					]
				]
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				[
					//////////////////////////////
					// ContentItems | Info | Settings
					//////////////////////////////
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SBorder)
						.BorderImage(FSMUnrealAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.AutoWidth()
								[
									SNew(STextBlock)
									.Text(LOCTEXT("ContentHeader", "Content"))
									.TextStyle(FSMUnrealAppStyle::Get(), "LargeText")
									.Margin(FMargin(0.f, 2.f))
								]
								+ SHorizontalBox::Slot()
								.HAlign(HAlign_Right)
								[
									SNew(SCheckBox)
									.IsChecked_Lambda([this]()
									{
										return bShowPluginContent ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
									})
									.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
									{
										bShowPluginContent = NewState == ECheckBoxState::Checked;
										FilterContent();
									})
									[
										SNew(STextBlock)
										.Text(LOCTEXT("ShowPluginContentLabel", "Show Plugin Content"))
										.ToolTipText(LOCTEXT("ShowPluginContentTooltip", "Show content which is normally installed to the plugin content folder as a dependency."))
									]
								]
							]
							+ SVerticalBox::Slot()
							[
								SNew(SBorder)
								.BorderImage(FSMUnrealAppStyle::Get().GetBrush("ProjectBrowser.MajorCategoryViewBorder"))
								[
									SNew(SOverlay)
#if !LOGICDRIVER_IS_MARKETPLACE_BUILD
									+ SOverlay::Slot()
									[
										SNew(STextBlock)
										.Visibility(ContentItems.Num() == 0 ? EVisibility::Visible : EVisibility::Collapsed)
										.Text(LOCTEXT("NoPluginContentText", "Follow the README on the repository to download content."))
										.TextStyle(FSMUnrealAppStyle::Get(), "NormalText.Important")
										.AutoWrapText(true)
										.Margin(2.f)
									]
#endif
									+ SOverlay::Slot()
									[
										SNew(SScrollBorder, ContentTileView.ToSharedRef())
										[
											ContentTileView.ToSharedRef()
										]
									]
								]
							]
						]
					]
					+ SHorizontalBox::Slot()
					.FillWidth(0.5f)
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					[
						SNew(SBox)
						.HAlign(HAlign_Fill)
						.VAlign(VAlign_Fill)
						[
							SNew(SBorder)
							.BorderImage(FSMUnrealAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
							[
								SNew(SScrollBox)
								+ SScrollBox::Slot()
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot()
									.VAlign(VAlign_Top)
									.HAlign(HAlign_Fill)
									.Padding(5.f, 0.f, 2.f, 0.f)
									[
										SNew(SVerticalBox)
										+ SVerticalBox::Slot()
										.AutoHeight()
										[
											SNew(STextBlock)
											.Text(this, &SSMContentSelectView::GetSelectedContentName)
											.TextStyle(FSMUnrealAppStyle::Get(), "LargeText")
											.Margin(FMargin(0.f, 2.f))
										]
										+ SVerticalBox::Slot()
										.VAlign(VAlign_Top)
										[
											SNew(SScaleBox)
											// Visibility here to correctly remove image size padding
											.Visibility_Raw(this, &SSMContentSelectView::GetSelectedContentImageVisibility)
											.Stretch(EStretch::ScaleToFit)
											.Content()
											[
												SNew(SImage)
												.Image_Raw(this, &SSMContentSelectView::GetSelectedContentImage)
											]
										]
										+ SVerticalBox::Slot()
										.AutoHeight()
										.Padding(0.f, 6.f)
										.VAlign(VAlign_Top)
										.HAlign(HAlign_Fill)
										[
											SNew(SRichTextBlock)
											.Text(this, &SSMContentSelectView::GetSelectedContentDetailedDescription)
											.TextStyle(FSMUnrealAppStyle::Get(), "TextBlock.ShadowedText")
											.AutoWrapText(true)
											.DecoratorStyleSet(&FSMUnrealAppStyle::Get())
											+ SRichTextBlock::HyperlinkDecorator(TEXT("browser"), this, &SSMContentSelectView::OnBrowserLinkClicked)
										]
									]
								]
							]
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SSeparator)
						.Orientation(EOrientation::Orient_Vertical)
						.Thickness(2.0f)
					]
					+ SHorizontalBox::Slot()
					.FillWidth(0.3f)
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					.Padding(FMargin(8.0f, 0.f, 25.f, 0.f))
					[
						SNew(SBorder)
						.BorderImage(FSMUnrealAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
						[
							SNew(SVerticalBox)
							.Visibility_Lambda([this]()
							{
								return GetSelectedContentViewModel().IsValid() ? EVisibility::Visible : EVisibility::Collapsed;
							})
							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(STextBlock)
								.Text(LOCTEXT("Configuration", "Configuration"))
								.TextStyle(FSMUnrealAppStyle::Get(), "LargeText")
								.Margin(FMargin(0.f, 2.f))
							]
							+ SVerticalBox::Slot()
							[
								SAssignNew(ProjectConfigurationBindingView, SSMProjectConfigurationView)
								.ViewModel(this, &SSMContentSelectView::GetSelectedProjectConfigurationViewModel)
							]
						]
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SSeparator)
					.Orientation(EOrientation::Orient_Horizontal)
					.Thickness(2.0f)
				]
				// Install path
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Bottom)
				[
					SNew(SBorder)
					.BorderImage(FSMUnrealAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
					.Padding(FMargin(12.5f, 12.f, 12.5f, 2.f))
					.Visibility_Lambda([this]()
					{
						return SelectedContent.IsValid() && !SelectedContent->IsInstalledToPluginContent() ? EVisibility::Visible : EVisibility::Collapsed;
					})
					[
						SAssignNew(FilePath, SSMFilePath)
						.FolderPath(FText::FromString(ContentBrowserDefs::GetDefaultInstallPath()))
					]
				]
				// Install Plugin Content path warning
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Bottom)
				[
					SNew(SBorder)
					.BorderImage(FSMUnrealAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
					.Padding(FMargin(12.5f, 2.f, 12.5f, 12.5f))
					.Visibility_Lambda([this]()
					{
						return SelectedContent.IsValid() && SelectedContent->GetContentAsset().IsValid()
						&& SelectedContent->GetContentAsset()->ContentAssetDependencies.Num() > 0 ? EVisibility::Visible : EVisibility::Hidden;
					})
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("SharedContentInstall", "Common dependencies will be installed to the Plugin's Content folder."))
							.Font(FSMUnrealAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
						]
						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Right)
						[
							SNew(SWarningOrErrorBox)
							.Visibility_Lambda([this]()
							{
								return DestinationPathErrorMessage.IsEmpty() ? EVisibility::Hidden : EVisibility::HitTestInvisible;
							})
							.Message_Lambda([this]()
							{
								return DestinationPathErrorMessage;
							})
							.MessageStyle(EMessageStyle::Error)
							.IconSize(FVector2D(16,16))
							.Padding(FMargin(8.0f, 2.0f, 4.0f, 2.0f))
						]
					]
				]
				// Install / cancel buttons
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(25.0f, 12.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					[
						SNew(SCheckBox)
						.IsChecked(this, &SSMContentSelectView::GetOpenMapOnInstallCheckState)
						.IsEnabled(this, &SSMContentSelectView::DoesContentHaveSampleMap)
						.OnCheckStateChanged(this, &SSMContentSelectView::OnOpenMapCheckStateChanged)
						.Content()
						[
							SNew(STextBlock)
							.Margin(2.f)
							.Text(LOCTEXT("OpenMapOnInstallText", "Open Map on Install"))
							.ToolTipText(LOCTEXT("OpenMapOnInstallTooltip", "Open the content's example map after installation has completed."))
						]
					]
					+ SHorizontalBox::Slot()
					.Padding(8.0f, 0.0f, 8.0f, 0.0f)
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(SPrimaryButton)
						.Text(LOCTEXT("InstallContentText", "Install"))
						.IsEnabled(this, &SSMContentSelectView::IsInstallButtonEnabled)
						.OnClicked_Lambda([this](){ OnInstallButtonClicked(); return FReply::Handled(); })
					]
					+ SHorizontalBox::Slot()
					.Padding(8.0f, 0.0f, 0.0f, 0.0f)
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.Text(LOCTEXT("CancelInstallText", "Cancel"))
						.OnClicked_Lambda([this](){ OnCancelButtonClicked(); return FReply::Handled(); })
					]
				]
			]
		];

	SWindow::Construct(SWindow::FArguments()
		.Title(LOCTEXT("ContentPickerTitle", "Select a Logic Driver Content Sample to Install"))
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(1190, 733)) // Same as new project dialog
		.MinWidth(450.f)
		.MinHeight(450.f)
		.SupportsMaximize(true)
		.SupportsMinimize(false)
		[
			RootBox
		]);

	if (FilteredContentItems.Num() > 0)
	{
		OnContentViewSelectionChanged(FilteredContentItems[0], ESelectInfo::Direct);
	}
}

bool SSMContentSelectView::GetUserConfirmedSelection() const
{
	return bUserConfirmedSelection;
}

bool SSMContentSelectView::GetUserCreatedNew() const
{
	return bCreateNewSelected;
}

TSharedPtr<FSMContentItemViewModel> SSMContentSelectView::GetSelectedContentViewModel() const
{
	return SelectedContent;
}

USMContentProjectConfigurationViewModel* SSMContentSelectView::GetSelectedProjectConfigurationViewModel() const
{
	const TSharedPtr<FSMContentItemViewModel> ContentItem = GetSelectedContentViewModel();
	if (ContentItem.IsValid())
	{
		return ContentItem->GetProjectConfigurationViewModel();
	}

	return nullptr;
}

void SSMContentSelectView::ConfirmSelection()
{
	if (SelectedContent.IsValid() && IsDestinationPathValid())
	{
		SelectedContent->InstallPath = FilePath->GetSelectedFolderPath().ToString();
		SelectedContent->DestinationFolder = FilePath->GetDestinationFolderName().ToString();

		SelectedContent->OnSelectionConfirmed.ExecuteIfBound();
		bUserConfirmedSelection = true;
		RequestDestroyWindow();
	}
}

bool SSMContentSelectView::IsDestinationPathValid() const
{
	const bool bResult = AssetViewUtils::IsValidFolderPathForCreate(FilePath->GetSelectedFolderPath().ToString(),
		FilePath->GetDestinationFolderName().ToString(), DestinationPathErrorMessage);
	if (bResult && !DestinationPathErrorMessage.IsEmpty())
	{
		DestinationPathErrorMessage = FText::GetEmpty();
	}
	return bResult;
}

void SSMContentSelectView::FilterContent()
{
	FilteredContentItems.Reset();
	for (TSharedPtr<FSMContentItemViewModel>& Content : ContentItems)
	{
		if (!bShowPluginContent && Content->IsInstalledToPluginContent())
		{
			// Only display plugins that can be installed to the project. Plugin content should be dependencies only.
			continue;
		}

		FilteredContentItems.Add(Content);
	}

	FilteredContentItems.Sort([](const TSharedPtr<FSMContentItemViewModel>& LHS, const TSharedPtr<FSMContentItemViewModel>& RHS)
	{
		return (LHS->GetContentAsset()->SortPriority < RHS->GetContentAsset()->SortPriority) ||
			(LHS->GetContentAsset()->ContentType > RHS->GetContentAsset()->ContentType);
	});

	if (ContentTileView.IsValid())
	{
		ContentTileView->RequestListRefresh();
	}
}

EVisibility SSMContentSelectView::GetSelectedContentImageVisibility() const
{
	return SelectedContent.IsValid() &&
		SelectedContent->GetContentAsset()->DescriptionImage != nullptr ? EVisibility::Visible : EVisibility::Collapsed;
}

const FSlateBrush* SSMContentSelectView::GetSelectedContentImage() const
{
	return SelectedContent.IsValid() ? SelectedContent->GetDescriptionBrush() : nullptr;
}

FText SSMContentSelectView::GetSelectedContentName() const
{
	return SelectedContent.IsValid() ? SelectedContent->Name : FText::GetEmpty();
}

FText SSMContentSelectView::GetSelectedContentDetailedDescription() const
{
	return SelectedContent.IsValid() ? SelectedContent->DetailedDescription : FText::GetEmpty();
}

FText SSMContentSelectView::GetSelectedContentInstallationPath() const
{
	return SelectedContent.IsValid() ? FText::FromString(SelectedContent->InstallPath) : FText::GetEmpty();
}

TSharedRef<ITableRow> SSMContentSelectView::MakeContentViewWidget(TSharedPtr<FSMContentItemViewModel> ContentItem,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	TWeakPtr<FSMContentItemViewModel> CurrentItem = ContentItem;

	TSharedRef<STableRow<TSharedPtr<FSMContentItemViewModel>>> Row =
		SNew(STableRow<TSharedPtr<FSMContentItemViewModel>>, OwnerTable)
		.Style(FSMUnrealAppStyle::Get(), "ProjectBrowser.TableRow")
		.ShowSelection(false)
		.Padding(2);

	Row->SetContent(
		SNew(SSMContentTile, ContentItem)
		.IsSelected_Lambda([CurrentItem, this]() { return CurrentItem == SelectedContent; })
	);

	return Row;
}

void SSMContentSelectView::OnFilterTextChanged(const FText& SearchText)
{
	// TODO: Search
}

void SSMContentSelectView::OnContentViewSelectionChanged(TSharedPtr<FSMContentItemViewModel> ContentItem,
	ESelectInfo::Type SelectInfo)
{
	SelectedContent = ContentItem;

	if (SelectedContent)
	{
		check(SelectedContent->GetContentAsset().IsValid());

		FilePath->SetDestinationFolderName(FText::FromString(SelectedContent->GetContentAsset()->GetOwningDirectory()));

		if (SelectedContent->InstallPath.IsEmpty())
		{
			SelectedContent->InstallPath = ContentBrowserDefs::GetDefaultInstallPath();
		}

		if (ProjectConfigurationBindingView.IsValid())
		{
			ProjectConfigurationBindingView->SetDetailsObject(GetSelectedProjectConfigurationViewModel());
		}
	}
}

ECheckBoxState SSMContentSelectView::GetOpenMapOnInstallCheckState() const
{
	if (!SelectedContent.IsValid() || !DoesContentHaveSampleMap())
	{
		return ECheckBoxState::Unchecked;
	}
	return SelectedContent->bOpenMapOnInstall ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

bool SSMContentSelectView::DoesContentHaveSampleMap() const
{
	if (!SelectedContent.IsValid() || !SelectedContent->GetContentAsset().IsValid())
	{
		return false;
	}

	return SelectedContent->GetContentAsset()->PrimaryMapName.Len() > 0;
}

void SSMContentSelectView::OnOpenMapCheckStateChanged(ECheckBoxState NewState)
{
	if (!SelectedContent.IsValid() || !SelectedContent->GetContentAsset().IsValid())
	{
		return;
	}

	SelectedContent->bOpenMapOnInstall = NewState == ECheckBoxState::Checked;
}

bool SSMContentSelectView::IsInstallButtonEnabled() const
{
	return SelectedContent.IsValid() && !FilePath->GetSelectedFolderPath().IsEmpty()
	&& !FilePath->GetDestinationFolderName().IsEmpty() && IsDestinationPathValid();
}

void SSMContentSelectView::OnInstallButtonClicked()
{
	ConfirmSelection();
}

void SSMContentSelectView::OnCancelButtonClicked()
{
	bUserConfirmedSelection = false;

	RequestDestroyWindow();
}

void SSMContentSelectView::OnBrowserLinkClicked(const FSlateHyperlinkRun::FMetadata& Metadata)
{
	if (const FString* Url = Metadata.Find(TEXT("href")))
	{
		FPlatformProcess::LaunchURL(**Url, nullptr, nullptr);
	}
}

#undef LOCTEXT_NAMESPACE
