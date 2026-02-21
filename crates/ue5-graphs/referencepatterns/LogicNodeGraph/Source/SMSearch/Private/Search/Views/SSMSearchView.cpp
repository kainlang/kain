// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SSMSearchView.h"

#include "Configuration/SMSearchSettings.h"
#include "ISMAssetManager.h"
#include "ISMAssetToolsModule.h"
#include "ISMSearchModule.h"
#include "Search/ViewModels/SearchFilterViewModel.h"
#include "Search/Views/SSMSearchResultItemView.h"
#include "SMSearchLog.h"
#include "SSMGraphPanel.h"

#include "Configuration/SMEditorStyle.h"
#include "Graph/Nodes/PropertyNodes/SMGraphK2Node_PropertyNode_Base.h"
#include "Graph/Nodes/SMGraphNode_Base.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "DetailLayoutBuilder.h"
#include "Editor.h"
#include "EditorStyleSet.h"
#include "FindInBlueprintManager.h"
#include "FindInBlueprints.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDocumentation.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "SMUnrealTypeDefs.h"
#include "SPinTypeSelector.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Notifications/SProgressBar.h"

#define LOCTEXT_NAMESPACE "SMSearchView"

SSMSearchView::~SSMSearchView()
{
	SearchSettings->SaveConfig();
	FilterAssetsViewModel->SaveConfig();
	FilterPropertiesViewModel->SaveConfig();

	CancelSearch();
	StopListeningForBlueprintChanges();
}

void SSMSearchView::Construct(const FArguments& InArgs)
{
	SearchSettings = GetMutableDefault<USMSearchSettings>();
	FilterPropertiesViewModel = GetMutableDefault<USearchFilterPropertiesViewModel>();
	FilterAssetsViewModel = GetMutableDefault<USearchFilterAssetsViewModel>();

	bCanSupportFocus = true;

	const ISMSearchModule& SearchModule = FModuleManager::GetModuleChecked<ISMSearchModule>(LOGICDRIVER_SEARCH_MODULE_NAME);

	ISMSearch::FIndexingStatus IndexingStatus;
	SearchModule.GetSearchInterface()->GetIndexingStatus(IndexingStatus);

	// Don't disable deferred indexing if it already is disabled or has been triggered once already.
	if ((!IndexingStatus.bDeferredIndexingEnabledInLogicDriver.IsSet() &&
		IndexingStatus.bDeferredIndexingEnabledInEngineConfig != SearchSettings->bEnableDeferredIndexing))
	{
		if (!IsCacheInProgress())
		{
			SearchModule.GetSearchInterface()->EnableDeferredIndexing(SearchSettings->bEnableDeferredIndexing);
		}
		else
		{
			// Maybe a search was running from Find in Blueprints.
			SearchSettings->bEnableDeferredIndexing = IndexingStatus.bDeferredIndexingEnabledInEngineConfig;
		}
	}

	const TSharedRef<SScrollBar> HorizontalScrollBar = SNew(SScrollBar)
	.Orientation(Orient_Horizontal)
	.Thickness(FVector2D(12.0f, 12.0f));

	const TSharedRef<SScrollBar> VerticalScrollBar = SNew(SScrollBar)
	.Orientation(Orient_Vertical)
	.Thickness(FVector2D(12.0f, 12.0f));

	auto GetWidgetWidthAsOptionalSize = [this]() -> FOptionalSize
	{
		return FOptionalSize((WidgetSize.X / 2.f) - 7.f /* Account for divider and small offset for scrollbar */);
	};

	BuildHeader();

	/* TODO: Default sorting ?
	PrimarySortedColumn = SSMSearchResultItemView::ColumnName_Asset;
	PrimarySortMode = EColumnSortMode::Ascending;
	SecondarySortedColumn = SSMSearchResultItemView::ColumnName_Node;
	SecondarySortMode = EColumnSortMode::Ascending;
	*/

	const TSharedPtr<SWidget> FilterToggleButtonContent = SNew(SHorizontalBox)
	+SHorizontalBox::Slot()
	.AutoWidth()
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		SNew(STextBlock)
		.TextStyle(FSMUnrealAppStyle::Get(), "GenericFilters.TextStyle")
		.Font(FSMUnrealAppStyle::Get().GetFontStyle("FontAwesome.9"))
		.Text(FText::FromString(FString(TEXT("\xf0b0"))) /*fa-filter*/)
	]
	+SHorizontalBox::Slot()
	.AutoWidth()
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	.Padding(2, 0, 0, 0)
	[
		SNew(STextBlock)
		.TextStyle(FSMUnrealAppStyle::Get(), "GenericFilters.TextStyle")
		.Text(LOCTEXT("Options", "Options"))
	]
	+SHorizontalBox::Slot()
	.AutoWidth()
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	.Padding(1.f, 0.f)
	[
		SNew(SImage)
		.Image_Lambda([&]()
		{
			return bFilterMenuToggled ? FSMEditorStyle::Get()->GetBrush("Symbols.LeftArrow") :
			FSMEditorStyle::Get()->GetBrush("Symbols.RightArrow");
		})
	];

	ChildSlot
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		.MaxWidth(500.f)
		.Padding(0.f, 1.f, 0.f, 0.f)
		[
			SNew(SVerticalBox)
			.Visibility_Lambda([&]()
			{
				return bFilterMenuToggled ? EVisibility::Visible : EVisibility::Collapsed;
			})
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(3)
			[
				// Toggle button open.
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.ButtonStyle(FSMUnrealAppStyle::Get(), "ToggleButton")
					.ForegroundColor(FLinearColor::White)
					.ToolTipText(LOCTEXT("HideSearchOptionsToolTip", "Hide the search options."))
					.ContentPadding(FMargin(1, 1))
					.Visibility_Lambda([&]()
					{
						return !bFilterMenuToggled ? EVisibility::Collapsed : EVisibility::Visible;
					})
					.OnClicked_Lambda([&]()
					{
						bFilterMenuToggled = !bFilterMenuToggled;
						return FReply::Handled();
					})
					.Content()
					[
						FilterToggleButtonContent.ToSharedRef()
					]
				]
			]
			+SVerticalBox::Slot()
			.Padding(0.f, 2.f, 0.f, 0.f)
			[
				SNew(SScrollBox)
				.Orientation(Orient_Vertical)
				+SScrollBox::Slot()
				[
					MakeAddFilterMenu()
				]
			]
		]
		+SHorizontalBox::Slot()
		[
			SNew(SBorder)
			.Padding(3)
			.BorderImage(FSMUnrealAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.VAlign(VAlign_Top)
				.HAlign(HAlign_Fill)
				.AutoHeight()
				[
					// Main grid
					SNew(SGridPanel)
					.FillColumn(1, 0.7f)
					// Left buttons
					+SGridPanel::Slot(0, 0)
					.Padding(FMargin(2.f, 0.f, 0.f, 2.f))
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							// Toggle button open.
							SNew(SButton)
							.HAlign(HAlign_Left)
							.VAlign(VAlign_Center)
							.ButtonStyle(FSMUnrealAppStyle::Get(), "ToggleButton")
							.ForegroundColor(FLinearColor::White)
							.ToolTipText(LOCTEXT("ShowSearchOptionsToolTip", "Show the search options."))
							.ContentPadding(FMargin(1, 1))
							.Visibility_Lambda([&]()
							{
								return bFilterMenuToggled ? EVisibility::Collapsed : EVisibility::Visible;
							})
							.OnClicked_Lambda([&]()
							{
								bFilterMenuToggled = !bFilterMenuToggled;
								return FReply::Handled();
							})
							.Content()
							[
								FilterToggleButtonContent.ToSharedRef()
							]
						]
						+SHorizontalBox::Slot()
						.FillWidth(1.f)
						.VAlign(VAlign_Fill)
						.HAlign(HAlign_Fill)
						.Padding(1.f, 0.f)
						[
							// Case sensitive search
							SNew(SCheckBox)
							.Style(FSMUnrealAppStyle::Get(), "ToggleButtonCheckbox")
							.Type(ESlateCheckBoxType::ToggleButton)
							.ToolTipText(LOCTEXT("CaseSensitive_Tooltip", "Match the case of the word(s)."))
							.Padding(2.f)
							.OnCheckStateChanged_Lambda([&](ECheckBoxState NewState)
							{
								SearchArguments.bCaseSensitive = NewState == ECheckBoxState::Checked;
								RefreshSearch();
							})
							.Content()
							[
								SNew(STextBlock)
								.Margin(2.f)
								.Text(LOCTEXT("CaseSensitiveButton", "Cc"))
								.TextStyle(FSMUnrealAppStyle::Get(), "NormalText.Important")
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
							]
						]
						+SHorizontalBox::Slot()
						.FillWidth(1.f)
						.VAlign(VAlign_Fill)
						.HAlign(HAlign_Fill)
						.Padding(1.f, 0.f)
						[
							// Word search
							SNew(SCheckBox)
							.Style(FSMUnrealAppStyle::Get(), "ToggleButtonCheckbox")
							.Type(ESlateCheckBoxType::ToggleButton)
							.ToolTipText(LOCTEXT("Word_Tooltip", "Search full words only."))
							.Padding(2.f)
							.OnCheckStateChanged_Lambda([&](ECheckBoxState NewState)
							{
								SearchArguments.bFullWord = NewState == ECheckBoxState::Checked;
								RefreshSearch();
							})
							.Content()
							[
								SNew(STextBlock)
								.Margin(2.f)
								.Text(LOCTEXT("WordButton", "W"))
								.TextStyle(FSMUnrealAppStyle::Get(), "NormalText.Important")
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
							]
						]
						+SHorizontalBox::Slot()
						.FillWidth(1.f)
						.VAlign(VAlign_Fill)
						.HAlign(HAlign_Fill)
						.Padding(1.f, 0.f)
						[
							// Regex
							SNew(SCheckBox)
							.Style(FSMUnrealAppStyle::Get(), "ToggleButtonCheckbox")
							.Type(ESlateCheckBoxType::ToggleButton)
							.ToolTipText(LOCTEXT("Regex_Tooltip", "Use regular expressions."))
							.Padding(2.f)
							.OnCheckStateChanged_Lambda([&](ECheckBoxState NewState)
							{
								SearchArguments.bRegex = NewState == ECheckBoxState::Checked;
								RefreshSearch();
							})
							.Content()
							[
								SNew(STextBlock)
								.Margin(2.f)
								.Text(LOCTEXT("RegexButton", ".*"))
								.TextStyle(FSMUnrealAppStyle::Get(), "NormalText.Important")
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
							]
						]
					]
					// Replace checkbox
					+SGridPanel::Slot(0, 1)
					.Padding(FMargin(2.f, 0.f, 0.f, 2.f))
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.HAlign(HAlign_Right)
						[
							SNew(SCheckBox)
							.HAlign(HAlign_Right)
							.IsChecked_Lambda([this]()
							{
								return IsReplaceEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
							})
							.OnCheckStateChanged_Lambda([this](ECheckBoxState InCheckBoxState)
							{
								bReplaceEnabled = InCheckBoxState == ECheckBoxState::Checked;
							})
							.ToolTipText(LOCTEXT("ReplaceCheckBox_Tooltip", "Enable value replacement. (Ctrl + H)"))
						]
					]
					// Search box
					+SGridPanel::Slot(1, 0)
					.Padding(FMargin(2.f, 0.f, 0.f, 2.f))
					[
						SAssignNew(SearchBox, SSearchBox)
						.HintText(LOCTEXT("SearchTextHint", "Search Properties"))
						.OnTextCommitted(this, &SSMSearchView::OnSearchTextCommitted)
						.OnTextChanged(this, &SSMSearchView::OnSearchTextChanged)
						.IsSearching(this, &SSMSearchView::IsSearching)
						.SearchResultData(this, &SSMSearchView::GetSearchResultData)
					]
					// Replace box
					+SGridPanel::Slot(1, 1)
					.Padding(FMargin(2.f, 0.f, 0.f, 2.f))
					[
						SAssignNew(ReplacementTextBox, SMultiLineEditableTextBox)
						.HintText(LOCTEXT("ReplaceTextHint", "Replace"))
						.IsEnabled(this, &SSMSearchView::IsReplaceEnabled)
						.ModiferKeyForNewLine(EModifierKey::Shift)
						.OnTextCommitted(this, &SSMSearchView::OnReplaceTextCommitted)
						.OnTextChanged(this, &SSMSearchView::OnReplaceTextChanged)
					]
					// Summary box
					+SGridPanel::Slot(1, 2)
					.Padding(FMargin(2.f, 0.f, 0.f, 2.f))
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.AutoHeight()
						[
							MakeCacheProgressBarWidget()
						]
						+SVerticalBox::Slot()
						.AutoHeight()
						[
							MakeSearchProgressBarWidget()
						]
					]
					// Top right buttons
					+SGridPanel::Slot(2, 0)
					.Padding(FMargin(2.f, 0.f, 0.f, 2.f))
					[
						SNew(SBox)
						// Box/width necessary so buttons are sized evenly and to fit all text
						.WidthOverride(200.f)
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.Padding(FMargin(2.0f, 0.f, 0.f, 0.f))
							.FillWidth(0.5f)
							[
								SNew(SButton)
								.VAlign(VAlign_Center)
								.HAlign(HAlign_Center)
								.IsEnabled(this, &SSMSearchView::CanSearch)
								.OnClicked(FOnClicked::CreateLambda([this]()
								{
									StartSearch();
									return FReply::Handled();
								}))
								.Text(LOCTEXT("SearchButton", "Search"))
								.ToolTipText(LOCTEXT("SearchButton_Tooltip", "Search for all occurrences of the search string within property values."))
							]
							+SHorizontalBox::Slot()
							.Padding(FMargin(2.0f, 0.f, 0.f, 0.f))
							.FillWidth(0.5f)
							[
								SNew(SButton)
								.VAlign(VAlign_Center)
								.HAlign(HAlign_Center)
								.IsEnabled(this, &SSMSearchView::IsSearching)
								.OnClicked(FOnClicked::CreateLambda([this]()
								{
									CancelSearch();
									return FReply::Handled();
								}))
								.Text(LOCTEXT("CancelButton", "Cancel"))
								.ToolTipText(LOCTEXT("CancelButton_Tooltip", "Cancel the active search."))
							]
						]
					]
					// Middle right buttons
					+SGridPanel::Slot(2, 1)
					.Padding(FMargin(2.f, 0.f, 0.f, 2.f))
					[
						SNew(SBox)
						// Box/width necessary so buttons are sized evenly and to fit all text
						.WidthOverride(200.f)
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.Padding(FMargin(2.0f, 0.f, 0.f, 0.f))
							.FillWidth(0.5f)
							[
								SNew(SButton)
								.VAlign(VAlign_Center)
								.HAlign(HAlign_Center)
								.IsEnabled(this, &SSMSearchView::CanReplaceSelected)
								.OnClicked(FOnClicked::CreateLambda([this]()
								{
									ReplaceSelected();
									return FReply::Handled();
								}))
								.Text(LOCTEXT("ReplaceButton", "Replace"))
								.ToolTipText(LOCTEXT("ReplaceButton_Tooltip", "Replace all matching text in the selected results with the replace string."))
							]
							+SHorizontalBox::Slot()
							.Padding(FMargin(2.0f, 0.f, 0.f, 0.f))
							.FillWidth(0.5f)
							[
								SNew(SButton)
								.VAlign(VAlign_Center)
								.HAlign(HAlign_Center)
								.IsEnabled(this, &SSMSearchView::CanReplaceAll)
								.OnClicked(FOnClicked::CreateLambda([this]()
								{
									ReplaceAll();
									return FReply::Handled();
								}))
								.Text(LOCTEXT("ReplaceAllButton", "Replace All"))
								.ToolTipText(LOCTEXT("ReplaceAllButton_Tooltip", "Replace all matching text in all of the results with the replace string."))
							]
						]
					]
				]
				+SVerticalBox::Slot()
				.Padding(0.f, 2.f)
				.FillHeight(1.f)
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				[
					SNew(SSplitter)
					.Orientation(Orient_Horizontal)
					+SSplitter::Slot()
					[
						SNew(SGridPanel)
						.FillColumn(0, 1.0f)
						.FillRow(0, 1.0f)
						+SGridPanel::Slot(0,0)
						.HAlign(HAlign_Fill)
						[
							SNew(SScrollBox)
							.Orientation(Orient_Horizontal)
							.ExternalScrollbar(HorizontalScrollBar)
							+SScrollBox::Slot()
							[
								// This box needs to have a min desired width that adjusts to the window size.
								// Scroll boxes won't allow children to Fill because it would negate the point of a scrollbox.
								SNew(SBox)
								.MinDesiredWidth_Lambda(GetWidgetWidthAsOptionalSize)
								.HAlign(HAlign_Fill)
								[
									SNew(SBorder)
									.BorderImage(FSMUnrealAppStyle::Get().GetBrush("Menu.Background"))
									.HAlign(HAlign_Fill)
									.VAlign(VAlign_Fill)
									[
										SAssignNew(ResultsListView, SListView<TSharedPtr<ISMSearch::FSearchResult>>)
										.AllowOverscroll(EAllowOverscroll::No)
										.SelectionMode(ESelectionMode::Single)
										.ListItemsSource(&ResultSummary.SearchResults)
										.OnGenerateRow(this, &SSMSearchView::OnGenerateWidgetForItem)
										.OnSelectionChanged(this, &SSMSearchView::OnResultSelected)
										.OnMouseButtonDoubleClick(this, &SSMSearchView::OnDoubleClickItem)
										.HeaderRow(
											ResultsHeaderRow
										)
										.Visibility_Lambda([this]()
										{
											return ResultSummary.SearchResults.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed;
										})
										.ItemHeight(20)
										.ExternalScrollbar(VerticalScrollBar)
									]
								]
							]
						]
						+SGridPanel::Slot(1, 0)
						.HAlign(HAlign_Right)
						[
							VerticalScrollBar
						]
						+SGridPanel::Slot(0, 1)
						.VAlign(VAlign_Bottom)
						[
							HorizontalScrollBar
						]
					]
					+SSplitter::Slot()
					[
						SAssignNew(GraphPreviewBorder, SBorder)
						.BorderImage(FSMUnrealAppStyle::Get().GetBrush("Menu.Background"))
						.HAlign(HAlign_Fill)
						.VAlign(VAlign_Fill)
					]
				]
			]
		]
	];

	if (FilterPropertiesViewModel->PropertyTypeTemplate == ESMPropertyTypeTemplate::None)
	{
		LoadPropertyTemplateSettings();
	}
	else
	{
		HandleTemplateChanged(FilterPropertiesViewModel->PropertyTypeTemplate);
	}
}

void SSMSearchView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	FFindInBlueprintSearchManager& FiBManager = FFindInBlueprintSearchManager::Get();

	if (!IsSearching() && !FiBManager.IsTickable() && !FiBManager.IsAssetDiscoveryInProgress())
	{
		// In the event the manager is caching pending BPs it may need to tick but needs the global find window
		// open. Manually tick to ensure the process completes.
		FiBManager.Tick(InDeltaTime);
	}

	if (FiBManager.IsCacheInProgress() && !FiBManager.IsUnindexedCacheInProgress())
	{
		// The indexer can become stuck sometimes when an asset is loaded while a search is in progress.
		// This behavior can be recreated using the normal Find In Blueprints window. Tracing the code paths shows
		// no way of resolving on its own short of canceling the indexing.

		// This only occurs when both multi-threaded and deferred indexing are enabled. We turn off deferred indexing
		// to solve this problem as this and keep reasonable search speeds.

		const FSoftObjectPath CurrentCachedBlueprintPath = FiBManager.GetCurrentCacheBlueprintPath();
		const int32 NewNumUncachedAssets = FiBManager.GetNumberUncachedAssets();
		const int32 CurrentCacheIndex = FiBManager.GetCurrentCacheIndex();

		if (SearchSettings->bEnableDeferredIndexing && NewNumUncachedAssets == CurrentCacheIndex && LastCachedAssetPath == CurrentCachedBlueprintPath)
		{
			if (TimeSinceStuckCacheCheck >= 2.f)
			{
				bool bCancelIndex = true;
				if (UBlueprint* Blueprint = Cast<UBlueprint>(CurrentCachedBlueprintPath.ResolveObject()))
				{
					bool bRebuildSearchData = false;
					const FSearchData SearchData = FiBManager.QuerySingleBlueprint(Blueprint, bRebuildSearchData);
					const bool bNeedsIndex = SearchData.IsValid() && !SearchData.Value.IsEmpty() && !SearchData.IsIndexingCompleted();
					if (bNeedsIndex)
					{
						bCancelIndex = false;
						bRebuildSearchData = true;
						FSearchData UpdatedSearchData = FiBManager.QuerySingleBlueprint(Blueprint, bRebuildSearchData);
						UpdatedSearchData.StateFlags |= ESearchDataStateFlags::IsIndexed;

						FiBManager.ApplySearchDataToDatabase(MoveTemp(UpdatedSearchData));

						LDSEARCH_LOG_WARNING(
							TEXT("Asset %s isn't indexing and is retrying. This can happen when indexing in response to a blueprint compile while using deferred indexing. Index and uncached index count at %i."),
							*CurrentCachedBlueprintPath.ToString(), CurrentCacheIndex);
					}
				}

				if (bCancelIndex)
				{
					LDSEARCH_LOG_WARNING(
						TEXT("UE indexer may be stuck and will be canceled. This can happen when indexing in response to a blueprint compile while using deferred indexing. Asset %s, index and uncached index count at %i."),
						*CurrentCachedBlueprintPath.ToString(), CurrentCacheIndex);
					CancelCaching();
				}

				TimeSinceStuckCacheCheck = 0.f;
			}
			else
			{
				TimeSinceStuckCacheCheck += InDeltaTime;
			}
		}
		else
		{
			TimeSinceStuckCacheCheck = 0.f;
		}
	}
}

void SSMSearchView::OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	SCompoundWidget::OnArrangeChildren(AllottedGeometry, ArrangedChildren);
	WidgetSize = AllottedGeometry.GetLocalSize();
}

FReply SSMSearchView::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetCharacter() == 'H' && InKeyEvent.GetModifierKeys().IsControlDown())
	{
		bReplaceEnabled = !bReplaceEnabled;
		return FReply::Handled();
	}

	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

bool SSMSearchView::IsAssetLoading(const FString& InPath) const
{
	return AssetsLoading.Contains(InPath);
}

void SSMSearchView::StartSearch()
{
	if (!CanSearch())
	{
		return;
	}

	CancelSearch();
	ClearResults();

	SearchArguments.SearchString = SearchString;
	if (SearchArguments.SearchString.IsEmpty())
	{
		return;
	}

	SearchArguments.bAllowConstructionScriptsOnLoad = SearchSettings->bAllowConstructionScriptsOnLoad;

	SearchArguments.PackagePaths.Reset(FilterAssetsViewModel->Directories.Num());
	for (const FDirectoryPath& DirectoryPath : FilterAssetsViewModel->Directories)
	{
		if (!DirectoryPath.Path.IsEmpty())
		{
			FString FolderPath = DirectoryPath.Path;
			FString FullPath = FPaths::ConvertRelativePathToFull(FolderPath);
			const FString FullGameContentDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());

			if (FullPath.StartsWith(FullGameContentDir))
			{
				FPaths::MakePathRelativeTo(FullPath, *FullGameContentDir);
				FullPath.InsertAt(0, TEXT("/Game/"));
			}

			SearchArguments.PackagePaths.Add(*FullPath);
		}
	}

	SearchArguments.StateMachineClasses = FilterAssetsViewModel->StateMachines.Array();
	SearchArguments.bIncludeSubClasses = FilterAssetsViewModel->bSubClasses;

	SearchArguments.PropertyNames.Empty(FilterPropertiesViewModel->Names.Num());
	for (const FName& PropName : FilterPropertiesViewModel->Names)
	{
		FString PropNameString = PropName.ToString();
		PropNameString.RemoveSpacesInline();
		SearchArguments.PropertyNames.Add(*PropNameString);
	}

	const ISMSearchModule& SearchToolsModule = FModuleManager::GetModuleChecked<ISMSearchModule>(LOGICDRIVER_SEARCH_MODULE_NAME);

	AsyncSearchHandle = SearchToolsModule.GetSearchInterface()->SearchAsync(SearchArguments,
		ISMSearch::FOnSearchCompleted::CreateSP(this, &SSMSearchView::OnSearchCompleted),
		ISMSearch::FOnSearchUpdated::CreateSP(this, &SSMSearchView::OnSearchUpdated),
		ISMSearch::FOnSearchCanceled::CreateSP(this, &SSMSearchView::OnSearchCanceled));

	// Check if the search ran synchronously.
	if (ResultSummary.bComplete)
	{
		AsyncSearchHandle.Reset();
	}
}

void SSMSearchView::CancelSearch()
{
	if (AsyncSearchHandle.IsValid())
	{
		const ISMSearchModule& SearchToolsModule = FModuleManager::GetModuleChecked<ISMSearchModule>(LOGICDRIVER_SEARCH_MODULE_NAME);
		SearchToolsModule.GetSearchInterface()->CancelAsyncSearch(AsyncSearchHandle);
		AsyncSearchHandle.Reset();
	}

	ClearSelection();
}

void SSMSearchView::ClearSelection()
{
	HighlightProperty(SelectedSearchResult, false);

	StopListeningForBlueprintChanges();
	GraphPreview.Reset();
	GraphPreviewBorder->SetContent(SNullWidget::NullWidget);
	SelectedSearchResult.Reset();
}

void SSMSearchView::ClearResults()
{
	ClearSelection();
	ResultSummary = ISMSearch::FSearchSummary();
	ReplaceSummary = ISMSearch::FReplaceSummary();
	OperationSummaryString.Empty();
	RefreshList();
}

void SSMSearchView::SortResults()
{
	if(!PrimarySortedColumn.IsNone())
	{
		auto Compare = [this](const TSharedPtr<ISMSearch::FSearchResult>& Lhs, const TSharedPtr<ISMSearch::FSearchResult>& Rhs, const FName& ColName, EColumnSortMode::Type SortMode)
		{
			if (ColName == SSMSearchResultItemView::ColumnName_Error)
			{
				const FString ErrorLhs = Lhs->ReplaceResult.IsValid() ? Lhs->ReplaceResult->ErrorMessage.ToString() : FString();
				const FString ErrorRhs = Rhs->ReplaceResult.IsValid() ? Rhs->ReplaceResult->ErrorMessage.ToString() : FString();
				return SortMode == EColumnSortMode::Ascending ? ErrorLhs < ErrorRhs : ErrorLhs > ErrorRhs;
			}
			if (ColName == SSMSearchResultItemView::ColumnName_Asset)
			{
				return SortMode == EColumnSortMode::Ascending ? Lhs->GetBlueprintName() < Rhs->GetBlueprintName() : Lhs->GetBlueprintName() > Rhs->GetBlueprintName();
			}
			if (ColName == SSMSearchResultItemView::ColumnName_Node)
			{
				const FString NameLhs = Lhs->GetNodeName();
				const FString NameRhs = Rhs->GetNodeName();

				return SortMode == EColumnSortMode::Ascending ? NameLhs < NameRhs : NameLhs > NameRhs;
			}
			if (ColName == SSMSearchResultItemView::ColumnName_Property)
			{
				const FString NameLhs = Lhs->Property ? Lhs->Property->GetName() : FString();
				const FString NameRhs = Rhs->Property ? Rhs->Property->GetName() : FString();
				return SortMode == EColumnSortMode::Ascending ? NameLhs < NameRhs : NameLhs > NameRhs;
			}

			return SortMode == EColumnSortMode::Ascending ? Lhs->PropertyValue < Rhs->PropertyValue : Lhs->PropertyValue > Rhs->PropertyValue;
		};

		ResultSummary.SearchResults.Sort([&](const TSharedPtr<ISMSearch::FSearchResult>& Lhs, const TSharedPtr<ISMSearch::FSearchResult>& Rhs)
		{
			if (Compare(Lhs, Rhs, PrimarySortedColumn, PrimarySortMode))
			{
				return true; // Lhs must be before Rhs based on the primary sort order.
			}
			if (Compare(Rhs, Lhs, PrimarySortedColumn, PrimarySortMode)) // Invert operands order (goal is to check if operands are equal or not)
			{
				return false; // Rhs must be before Lhs based on the primary sort.
			}
			// Lhs == Rhs on the primary column, need to order according the secondary column if one is set.
			return SecondarySortedColumn.IsNone() ? false : Compare(Lhs, Rhs, SecondarySortedColumn, SecondarySortMode);
		});
	}

	if (ResultsListView.IsValid())
	{
		ResultsListView->RequestListRefresh();
	}
}

void SSMSearchView::RefreshSearch()
{
	if (SearchString == SearchArguments.SearchString && !SearchString.IsEmpty())
	{
		StartSearch();
	}
}

void SSMSearchView::ReplaceAll()
{
	if (CanReplaceAll())
	{
		Replace(ResultSummary.SearchResults);
	}
}

void SSMSearchView::ReplaceSelected()
{
	if (SelectedSearchResult.IsValid())
	{
		Replace({SelectedSearchResult.Pin()});
	}
}

void SSMSearchView::Replace(const TArray<TSharedPtr<ISMSearch::FSearchResult>>& InSearchResults)
{
	if (InSearchResults.Num() == 0)
	{
		return;
	}

	ReplaceArguments.ReplaceString = ReplaceString;
	const ISMSearchModule& SearchToolsModule = FModuleManager::GetModuleChecked<ISMSearchModule>(LOGICDRIVER_SEARCH_MODULE_NAME);

	ReplaceArguments.SearchResults = InSearchResults;
	ReplaceSummary = SearchToolsModule.GetSearchInterface()->ReplacePropertyValues(ReplaceArguments, SearchArguments);
	RefreshList(true);

	uint32 ItemsReplaced = 0;
	for (const TSharedPtr<ISMSearch::FSearchResult>& Result : ReplaceSummary.SearchResults)
	{
		check(Result.IsValid());
		if (Result->ReplaceResult.IsValid() && Result->ReplaceResult->ErrorMessage.IsEmpty())
		{
			ItemsReplaced++;
		}
	}

	OperationSummaryString = FString::Printf(TEXT("Replaced %d values in %d results, with %d errors."),
		ItemsReplaced, ReplaceSummary.SearchResults.Num(), ReplaceSummary.SearchResults.Num() - ItemsReplaced);
}

void SSMSearchView::StopListeningForBlueprintChanges()
{
	for (const TWeakObjectPtr<UBlueprint>& Blueprint : ActiveBlueprints)
	{
		if (Blueprint.IsValid())
		{
			Blueprint->OnChanged().RemoveAll(this);
		}
	}

	ActiveBlueprints.Empty();
}

void SSMSearchView::RefreshList(bool bFullRebuild)
{
	BuildHeader();
	SortResults();
	if (bFullRebuild && ResultsListView.IsValid())
	{
		ResultsListView->RebuildList();
	}
}

void SSMSearchView::IndexAllBlueprints()
{
	const ISMAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<ISMAssetToolsModule>(
	LOGICDRIVER_ASSET_TOOLS_MODULE_NAME);

	ISMAssetManager::FCompileBlueprintArgs CompileBlueprintArgs;
	CompileBlueprintArgs.AssetFilter.PackagePaths.Reserve(FilterAssetsViewModel->Directories.Num());

	for (const FDirectoryPath& DirectoryPath : FilterAssetsViewModel->Directories)
	{
		if (!DirectoryPath.Path.IsEmpty())
		{
			FString FolderPath = DirectoryPath.Path;
			FString FullPath = FPaths::ConvertRelativePathToFull(FolderPath);
			const FString FullGameContentDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());

			if (FullPath.StartsWith(FullGameContentDir))
			{
				FPaths::MakePathRelativeTo(FullPath, *FullGameContentDir);
				FullPath.InsertAt(0, TEXT("/Game/"));
			}

			CompileBlueprintArgs.AssetFilter.PackagePaths.Add(*FullPath);
			SearchArguments.PackagePaths.Add(*FullPath);
		}
	}
	CompileBlueprintArgs.AssetFilter.bRecursivePaths = true;

	CompileBlueprintArgs.bSave = true;
	CompileBlueprintArgs.bShowWarningMessage = true;
	CompileBlueprintArgs.CustomWarningTitle = LOCTEXT("CompileAllTitle", "Index State Machine Blueprints");
	CompileBlueprintArgs.CustomWarningMessage = LOCTEXT("CompileAllConfirmationMessage",
			"This process can take a long time and the editor may become unresponsive; there are {BlueprintCount} blueprints to load and compile.\n\nWould you like to checkout, load, and save all blueprints to make this indexing permanent? Otherwise, all state machine blueprints will need to be re-indexed the next time you start the editor!");
	TWeakPtr<SSMSearchView> WeakPtrThis = SharedThis(this);
	AssetToolsModule.GetAssetManagerInterface()->CompileBlueprints(CompileBlueprintArgs);
}

void SSMSearchView::CancelIndexAllBlueprints()
{
	const ISMAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<ISMAssetToolsModule>(
	LOGICDRIVER_ASSET_TOOLS_MODULE_NAME);

	AssetToolsModule.GetAssetManagerInterface()->CancelCompileBlueprints();
}

void SSMSearchView::CancelCaching()
{
	FFindInBlueprintSearchManager::Get().CancelCacheAll(nullptr);
}

bool SSMSearchView::TryLoadAsset(const TSharedPtr<ISMSearch::FSearchResult>& InItem, const FSimpleDelegate& OnLoadDelegate)
{
	if (InItem.IsValid())
	{
		if (SearchSettings->bAsyncLoad)
		{
			if (!AssetsLoading.Contains(InItem->BlueprintPath))
			{
				TWeakPtr<SSMSearchView> WeakPtrThis = SharedThis(this);
				const TSharedPtr<FStreamableHandle> Handle = InItem->AsyncLoadObjects(FSimpleDelegate::CreateLambda([WeakPtrThis, InItem, OnLoadDelegate]()
				{
					if (WeakPtrThis.IsValid())
					{
						WeakPtrThis.Pin()->AssetsLoading.Remove(InItem->BlueprintPath);
						WeakPtrThis.Pin()->TryResolveAllObjects();
						OnLoadDelegate.ExecuteIfBound();
					}
				}));
				if (Handle.IsValid())
				{
					AssetsLoading.Add(InItem->BlueprintPath, Handle);
					return true;
				}
			}
		}
		else
		{
			InItem->LoadObjects();
			TryResolveAllObjects();
			OnLoadDelegate.ExecuteIfBound();
		}
	}

	return false;
}

void SSMSearchView::TryResolveAllObjects()
{
	for (const TSharedPtr<ISMSearch::FSearchResult>& Result : ResultSummary.SearchResults)
	{
		// Other results may share the same package which is now loaded and should be resolved so their views
		// display updated information.
		Result->TryResolveObjects();
	}
}

void SSMSearchView::SwitchAssetLoadType(ESMAssetLoadType InLoadType)
{
	SearchSettings->AssetLoadType = InLoadType;
}

void SSMSearchView::HighlightProperty(TWeakPtr<ISMSearch::FSearchResult> InSearchResult, bool bValue)
{
	if (InSearchResult.IsValid())
	{
		const ISMSearch::FSearchResult* SearchResult = InSearchResult.Pin().Get();
		if (SearchResult->Property && SearchResult->NodeInstance.IsValid())
		{
			if (const USMGraphNode_Base* SMGraphNode = Cast<USMGraphNode_Base>(SearchResult->GraphNode.Get()))
			{
				if (USMGraphK2Node_PropertyNode_Base* GraphPropertyNode = SMGraphNode->GetGraphPropertyNode(
					SearchResult->Property->GetFName(), SearchResult->NodeInstance.Get(), SearchResult->GetPropertyIndex()))
				{
					USMGraphK2Node_PropertyNode_Base::FHighlightArgs HighlightArgs;
					HighlightArgs.bEnable = bValue;
					HighlightArgs.Color = SearchSettings->PropertyHighlightColor;

					GraphPropertyNode->SetHighlightedArgs(MoveTemp(HighlightArgs));
				}
			}
		}
	}
}

void SSMSearchView::OnSearchTextCommitted(const FText& InFilterText, ETextCommit::Type InCommitType)
{
	SearchString = InFilterText.ToString();
	if (InCommitType == ETextCommit::OnEnter)
	{
		StartSearch();
	}
}

void SSMSearchView::OnSearchTextChanged(const FText& InFilterText)
{
	SearchString = InFilterText.ToString();
}

void SSMSearchView::OnReplaceTextCommitted(const FText& InFilterText, ETextCommit::Type InCommitType)
{
	ReplaceArguments.ReplaceString = InFilterText.ToString();
	if (IsReplaceEnabled() && InCommitType == ETextCommit::OnEnter)
	{
		if (CanReplaceSelected())
		{
			ReplaceSelected();
		}
		else
		{
			ReplaceAll();
		}
	}
}

void SSMSearchView::OnReplaceTextChanged(const FText& InFilterText)
{
	ReplaceString = InFilterText.ToString();
}

TSharedRef<SWidget> SSMSearchView::MakeSearchProgressBarWidget()
{
	return SNew(SOverlay)
	+SOverlay::Slot()
	[
		SNew(STextBlock)
		.Visibility_Lambda([this] ()
		{
			return IsCacheInProgress() || IsSearching() || IsFullIndexInProgress() ? EVisibility::Collapsed : EVisibility::Visible;
		})
		.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateLambda([this]()
		{
			return FText::FromString(OperationSummaryString);
		})))
	]
	+SOverlay::Slot()
	[
		SNew(SHorizontalBox)
		.Visibility_Lambda([this]()
		{
			return IsSearching() || IsFullIndexInProgress() ? EVisibility::Visible : EVisibility::Collapsed;
		})
		+SHorizontalBox::Slot()
		.VAlign(EVerticalAlignment::VAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text_Lambda([&]()
			{
				return IsFullIndexInProgress() ? LOCTEXT("SearchLabelIndex", "Indexing") :  LOCTEXT("SearchLabel", "Searching");
			})
			.ColorAndOpacity(FCoreStyle::Get().GetColor("ErrorReporting.ForegroundColor"))
		]
		// Search progress bar
		+SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(4.0f, 2.0f, 4.0f, 2.0f)
		[
			SNew(SProgressBar)
			.Percent(this, &SSMSearchView::GetSearchPercentComplete)
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Top)
		[
			SNew(SButton)
			.Text(LOCTEXT("CancelIndexButton", "Cancel Index"))
			.ToolTipText(LOCTEXT("CancelIndexTooltip", "Attempt to cancel an in progress indexing."))
			.OnClicked_Lambda([this]()
			{
				CancelIndexAllBlueprints();
				return FReply::Handled();
			})
			.Visibility_Lambda([this]()
			{
				return IsFullIndexInProgress() ? EVisibility::Visible : EVisibility::Collapsed;
			})
		]
	];
}

TSharedRef<SWidget> SSMSearchView::MakeCacheProgressBarWidget()
{
	return SNew(SBorder)
	.Visibility_Lambda([this]()
	{
		const bool bIsPIESimulating = (GEditor->bIsSimulatingInEditor || GEditor->PlayWorld);
		return (!bIsPIESimulating && IsCacheInProgress()) ? EVisibility::Visible : EVisibility::Collapsed;
	})
	.BorderBackgroundColor_Lambda([]()
	{
		FSlateColor ReturnColor;
		if (FFindInBlueprintSearchManager::Get().IsUnindexedCacheInProgress())
		{
			// It turns yellow when an unindexed cache is in progress
			ReturnColor = FSlateColor(FLinearColor(0.4f, 0.4f, 0.0f));
		}
		else
		{
			// Use the background image color for a non-unindexed cache
			ReturnColor = FSlateColor(FLinearColor::White);
		}
		return ReturnColor;
	})
	.BorderImage_Lambda([this]()
	{
		const FSlateBrush* ReturnBrush = FCoreStyle::Get().GetBrush("ErrorReporting.Box");
		if (IsCacheInProgress() && !FFindInBlueprintSearchManager::Get().IsUnindexedCacheInProgress())
		{
			// Allow the content area to show through for a non-unindexed operation.
			ReturnBrush = FSMUnrealAppStyle::Get().GetBrush("NoBorder");
		}
		return ReturnBrush;
	})
	.Padding(FMargin(3,1))
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(EVerticalAlignment::VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(this, &SSMSearchView::GetCacheProgressBarStatusText)
					.ColorAndOpacity(FCoreStyle::Get().GetColor("ErrorReporting.ForegroundColor"))
				]

				// Cache progress bar
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(4.0f, 2.0f, 4.0f, 2.0f)
				[
					SNew(SProgressBar)
					.Percent(this, &SSMSearchView::GetCachePercentComplete)
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.0f, 0.0f, 0.0f, 2.0f)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						if (IsCacheInProgress())
						{
							LastCachedAssetPath = FFindInBlueprintSearchManager::Get().GetCurrentCacheBlueprintPath();
						}

						return FText::FromString(LastCachedAssetPath.ToString());
					})
					.ColorAndOpacity(FCoreStyle::Get().GetColor("ErrorReporting.ForegroundColor"))
				]
			]
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Top)
		[
			SNew(SButton)
			.Text(LOCTEXT("CancelIndexButton", "Cancel Index"))
			.ToolTipText(LOCTEXT("CancelIndexTooltip", "Attempt to cancel an in progress indexing."))
			.OnClicked_Lambda([this]()
			{
				CancelCaching();
				return FReply::Handled();
			})
		]
	];
}

FText SSMSearchView::GetCacheProgressBarStatusText() const
{
	const FFindInBlueprintSearchManager& FindInBlueprintManager = FFindInBlueprintSearchManager::Get();

	FFormatNamedArguments Args;
	FText ReturnDisplayText;
	if (IsCacheInProgress())
	{
		Args.Add(TEXT("CurrentIndex"), FindInBlueprintManager.GetCurrentCacheIndex());
		Args.Add(TEXT("Count"), FindInBlueprintManager.GetNumberUncachedAssets());

		ReturnDisplayText = FText::Format(LOCTEXT("CachingBlueprints", "Indexing Blueprints... {CurrentIndex}/{Count}"), Args);
	}
	else
	{
		const int32 UnindexedCount = FindInBlueprintManager.GetNumberUnindexedAssets();
		Args.Add(TEXT("UnindexedCount"), UnindexedCount);

		ReturnDisplayText = FText::Format(LOCTEXT("UncachedAssets", "Search incomplete. {Count} ({UnindexedCount} non-indexed/out-of-date) Blueprints need to be loaded and indexed!"), Args);

		const int32 FailedToCacheCount = FindInBlueprintManager.GetFailedToCacheCount();
		if (FailedToCacheCount > 0)
		{
			FFormatNamedArguments ArgsWithCacheFails;
			ArgsWithCacheFails.Add(TEXT("BaseMessage"), ReturnDisplayText);
			ArgsWithCacheFails.Add(TEXT("CacheFails"), FailedToCacheCount);
			ReturnDisplayText = FText::Format(LOCTEXT("UncachedAssetsWithCacheFails", "{BaseMessage} {CacheFails} Blueprints failed to cache."), ArgsWithCacheFails);
		}
	}

	return ReturnDisplayText;
}

bool SSMSearchView::IsCacheInProgress() const
{
	return !IsFullIndexInProgress() && FFindInBlueprintSearchManager::Get().IsCacheInProgress();
}

bool SSMSearchView::IsFullIndexInProgress() const
{
	const ISMAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<ISMAssetToolsModule>(
	LOGICDRIVER_ASSET_TOOLS_MODULE_NAME);

	return AssetToolsModule.GetAssetManagerInterface()->IsCompilingBlueprints();
}

TOptional<float> SSMSearchView::GetSearchPercentComplete() const
{
	return IsFullIndexInProgress() ? GetFullIndexPercentComplete() : ResultSummary.Progress;
}

TOptional<float> SSMSearchView::GetCachePercentComplete() const
{
	return FFindInBlueprintSearchManager::Get().GetCacheProgress();
}

TOptional<float> SSMSearchView::GetFullIndexPercentComplete() const
{
	const ISMAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<ISMAssetToolsModule>(
	LOGICDRIVER_ASSET_TOOLS_MODULE_NAME);

	const float Percent = AssetToolsModule.GetAssetManagerInterface()->GetCompileBlueprintsPercent();
	return Percent;
}

TSharedRef<SWidget> SSMSearchView::MakeAddFilterMenu()
{
	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, nullptr);

	MenuBuilder.BeginSection("AssetManagement", LOCTEXT("AssetManagementHeading", "Asset Management"));
	{
		MenuBuilder.AddSubMenu(LOCTEXT("AssetIndexingAndLoading_Label", "Indexing and Loading"),
			LOCTEXT("AssetIndexingAndLoading_Tooltip", "Manage asset indexing and loading options."),
			FNewMenuDelegate::CreateSP(this, &SSMSearchView::MakeIndexingAndLoadingSubMenu),
			false,
			FSlateIcon(),
			false);
	}
	MenuBuilder.EndSection();

	MakeAssetSubMenu(MenuBuilder);
	MakePropertySubMenu(MenuBuilder);

	return MenuBuilder.MakeWidget();
}

void SSMSearchView::MakeIndexingAndLoadingSubMenu(FMenuBuilder& MenuBuilder)
{
	MakeIndexSubMenu(MenuBuilder);
	MakeAssetLoadSubMenu(MenuBuilder);
}

void SSMSearchView::MakeIndexSubMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("AssetIndexing", LOCTEXT("AssetIndexingHeading", "Asset Indexing"));
	{
		const FText FullIndexText = LOCTEXT("RunFullIndexing_Label", "Run Full Index");
		const FText PartialIndexText = LOCTEXT("RunPartialIndexing_Label", "Run Index on Directories");
		MenuBuilder.AddMenuEntry(
		FilterAssetsViewModel->Directories.Num() > 0 ? PartialIndexText : FullIndexText,
		LOCTEXT("RunIndexing_Tooltip", "Index all state machine blueprints in the selected directories. If no directories are selected then every state machine blueprint will be loaded. This can be a very slow task and will resave assets."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]()
		{
			IndexAllBlueprints();
			FSlateApplication::Get().DismissAllMenus();
		}),
		FCanExecuteAction::CreateLambda([this]
		{
			return !IsCacheInProgress();
		}),
		FIsActionChecked()),
		NAME_None,
		EUserInterfaceActionType::Button
		);

		MenuBuilder.AddMenuEntry(
		LOCTEXT("DeferredIndexing_Label", "Deferred Indexing"),
		LOCTEXT("DeferredIndexing_Tooltip", "Enable or disable deferred indexing. Unreal Engine defaults this to on, but Logic Driver defaults it to off because it is buggy and\
\ncan stall indexing when a blueprint is compiled. If this value fails to change then it means the indexer couldn't be restarted."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]()
		{
			SearchSettings->bEnableDeferredIndexing = FModuleManager::GetModuleChecked<ISMSearchModule>(LOGICDRIVER_SEARCH_MODULE_NAME)
			.GetSearchInterface()->EnableDeferredIndexing(!SearchSettings->bEnableDeferredIndexing);
		}),
		FCanExecuteAction::CreateLambda([this]
		{
			return !IsSearching() && !FFindInBlueprintSearchManager::Get().IsCacheInProgress();
		}),
		FIsActionChecked::CreateLambda([this]()
		{
			return SearchSettings->bEnableDeferredIndexing;
		})),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
		);
	}
	MenuBuilder.EndSection();
}

void SSMSearchView::MakeAssetLoadSubMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("LogicDriverSearchAssetLoad", LOCTEXT("LogicDriverSearchAssetLoadHeading", "Asset Loading"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("LoadOnDemand_Label", "On Demand"),
			LOCTEXT("LoadOnDemand_Tooltip", "Load assets when they are needed, such as on selection."),
			FSlateIcon(),
			FUIAction(
			FExecuteAction::CreateSP(this, &SSMSearchView::SwitchAssetLoadType, ESMAssetLoadType::OnDemand),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([this]()
			{
				return SearchSettings->AssetLoadType == ESMAssetLoadType::OnDemand;
			})),
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("LoadOnView_Label", "On View"),
			LOCTEXT("LoadOnView_Tooltip", "Load assets when they become viewable in the list."),
			FSlateIcon(),
			FUIAction(
			FExecuteAction::CreateSP(this, &SSMSearchView::SwitchAssetLoadType, ESMAssetLoadType::OnViewable),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([this]()
			{
				return SearchSettings->AssetLoadType == ESMAssetLoadType::OnViewable;
			})),
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("AsyncLoading_Label", "Async Loading"),
			LOCTEXT("AsyncLoading_Tooltip", "Enable or disable async loading. If you experience crashes while loading assets try turning this off."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([this]()
			{
				SearchSettings->bAsyncLoad = !SearchSettings->bAsyncLoad;
			}),
			FCanExecuteAction::CreateLambda([this]
			{
				return true;
			}),
			FIsActionChecked::CreateLambda([this]()
			{
				return SearchSettings->bAsyncLoad;
			})),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("EnableConstructionScripts_Label", "Allow Construction Scripts"),
			LOCTEXT("EnableConstructionScripts_Tooltip", "Allow construction scripts to run when an asset is loaded from search. Disabling improves performance."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([this]()
			{
				SearchSettings->bAllowConstructionScriptsOnLoad = !SearchSettings->bAllowConstructionScriptsOnLoad;
			}),
			FCanExecuteAction::CreateLambda([] { return true; }),
			FIsActionChecked::CreateLambda([this]()
			{
				return SearchSettings->bAllowConstructionScriptsOnLoad;
			})),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);
	}
	MenuBuilder.EndSection();
}

void SSMSearchView::MakeAssetSubMenu(FMenuBuilder& MenuBuilder)
{
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	const FDetailsViewArgs DetailsViewArgs = CreateDetailsArgs();

	const TSharedRef<IDetailsView> PropertyView = EditModule.CreateDetailView(DetailsViewArgs);
	PropertyView->SetObject(FilterAssetsViewModel);

	MenuBuilder.BeginSection("LogicDriverSearchAssetFilters", LOCTEXT("LogicDriverSearchAssetFiltersHeading", "Asset Filters"));
	{
		MenuBuilder.AddWidget(PropertyView, LOCTEXT("AssetFilter", ""), true);
	}
	MenuBuilder.EndSection();
}

void SSMSearchView::MakePropertySubMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("LogicDriverSearchPropertyFilters", LOCTEXT("LogicDriverSearchPropertyFiltersHeading", "Property Filters"));

	MenuBuilder.AddSubMenu(
		LOCTEXT("PropertyTypes_Label", "Types"),
		LOCTEXT("PropertyTypes_Tooltip", "Select properties types to filter."),
		FNewMenuDelegate::CreateSP(this, &SSMSearchView::MakePropertyTypesSubMenu),
		false,
		FSlateIcon(),
		false);

	// View model version.
	{
		FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

		const FDetailsViewArgs DetailsViewArgs = CreateDetailsArgs();
		const TSharedRef<IDetailsView> PropertyView = EditModule.CreateDetailView(DetailsViewArgs);
		PropertyView->SetObject(FilterPropertiesViewModel);

		MenuBuilder.AddWidget(PropertyView, LOCTEXT("PropertyFilter", ""), true);
	}

	MenuBuilder.EndSection();
}

void SSMSearchView::MakePropertyTypesSubMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("FilterTypeAll_Label", "All"),
		LOCTEXT("FilterTypeAll_Tooltip", "Default property types to all types."),
		FSlateIcon(),
		FUIAction(
		FExecuteAction::CreateSP(this, &SSMSearchView::HandleTemplateChanged, ESMPropertyTypeTemplate::None),
		FCanExecuteAction::CreateLambda([] { return true; }),
		FIsActionChecked::CreateLambda([this]
		{
			return FilterPropertiesViewModel->PropertyTypeTemplate == ESMPropertyTypeTemplate::None;
		})),
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);
	MenuBuilder.AddMenuEntry(
		LOCTEXT("FilterTypeText_Label", "Text"),
		LOCTEXT("FilterTypeText_Tooltip", "Default property types to text based."),
		FSlateIcon(),
		FUIAction(
		FExecuteAction::CreateSP(this, &SSMSearchView::HandleTemplateChanged, ESMPropertyTypeTemplate::Text),
		FCanExecuteAction::CreateLambda([] { return true; }),
		FIsActionChecked::CreateLambda([this]
		{
			return FilterPropertiesViewModel->PropertyTypeTemplate == ESMPropertyTypeTemplate::Text;
		})),
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);
	MenuBuilder.AddMenuEntry(
		LOCTEXT("FilterTypeEnum_Label", "Enum"),
		LOCTEXT("FilterTypeEnum_Tooltip", "Default property types to enums."),
		FSlateIcon(),
		FUIAction(
		FExecuteAction::CreateSP(this, &SSMSearchView::HandleTemplateChanged, ESMPropertyTypeTemplate::Enum),
		FCanExecuteAction::CreateLambda([] { return true; }),
		FIsActionChecked::CreateLambda([this]
		{
			return FilterPropertiesViewModel->PropertyTypeTemplate == ESMPropertyTypeTemplate::Enum;
		})),
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);

	if (!PinTypeSelectionBox.IsValid())
	{
		SAssignNew(PinTypeSelectionBox, SVerticalBox)
		+SVerticalBox::Slot()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 4.0f, 0.0f)
		.AutoHeight();
	}

	BuildPropertyTypeFilterWidget();
	MenuBuilder.AddWidget(PinTypeSelectionBox.ToSharedRef(), LOCTEXT("PropertyTypeText", "Types"));
}

void SSMSearchView::BuildPropertyTypeFilterWidget()
{
	if (!PinTypeSelectionBox.IsValid())
	{
		return;
	}
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	PinTypeSelectionBox->ClearChildren();

	for (int32 Idx = 0; Idx < SearchArguments.PinTypes.Num(); ++Idx)
	{
		const FEdGraphPinType& Pin = SearchArguments.PinTypes[Idx];
		PinTypeSelectionBox->AddSlot()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SPinTypeSelector, FGetPinTypeTree::CreateUObject(K2Schema, &UEdGraphSchema_K2::GetVariableTypeTree))
				.TargetPinType(Pin)
				.OnPinTypeChanged(this, &SSMSearchView::HandlePinTypeChanged, Idx)
				.Schema(K2Schema)
				.TypeTreeFilter(ETypeTreeFilter::None)
				.bAllowArrays(false)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.OnClicked(FOnClicked::CreateLambda([this, Idx]()
				{
					SearchArguments.PinTypes.RemoveAt(Idx);
					FilterPropertiesViewModel->PropertyTypeTemplate = ESMPropertyTypeTemplate::None;
					BuildPropertyTypeFilterWidget();
					return FReply::Handled();
				}))
				.Text(LOCTEXT("RemovePropertyFilterButton_Text", "Remove"))
				.ToolTipText(LOCTEXT("RemovePropertyFilterButton_Tooltip", "Remove the property type filter."))
			]
		];
	}

	// New selection
	PinTypeSelectionBox->AddSlot()
	[
		SNew(SPinTypeSelector, FGetPinTypeTree::CreateUObject(K2Schema, &UEdGraphSchema_K2::GetVariableTypeTree))
		.OnPinTypeChanged(this, &SSMSearchView::HandlePinTypeChanged, -1)
		.Schema(K2Schema)
		.TypeTreeFilter(ETypeTreeFilter::None)
		.bAllowArrays(false)
		.Font(IDetailLayoutBuilder::GetDetailFont())
	];
}

void SSMSearchView::HandleTemplateChanged(ESMPropertyTypeTemplate NewTemplate)
{
	FilterPropertiesViewModel->PropertyTypeTemplate = NewTemplate;
	SearchArguments.PinTypes.Reset();

	switch (FilterPropertiesViewModel->PropertyTypeTemplate)
	{
	case ESMPropertyTypeTemplate::Text:
		{
			/* // Text graph not currently distinguishable from text
			FEdGraphPinType TextGraphPropertyType;
			TextGraphPropertyType.PinCategory = UEdGraphSchema_K2::PC_Struct;
			TextGraphPropertyType.PinSubCategoryObject = FSMTextGraphProperty::StaticStruct();
			SearchArguments.PinTypes.Add(MoveTemp(TextGraphPropertyType));
			*/

			FEdGraphPinType TextPropertyType;
			TextPropertyType.PinCategory = UEdGraphSchema_K2::PC_Text;
			SearchArguments.PinTypes.Add(MoveTemp(TextPropertyType));

			FEdGraphPinType StringPropertyType;
			StringPropertyType.PinCategory = UEdGraphSchema_K2::PC_String;
			SearchArguments.PinTypes.Add(MoveTemp(StringPropertyType));

			break;
		}
	case ESMPropertyTypeTemplate::Enum:
		{
			{
				FEdGraphPinType PropertyType;
				PropertyType.PinCategory = UEdGraphSchema_K2::PC_Enum;
				SearchArguments.PinTypes.Add(MoveTemp(PropertyType));
			}
			{
				FEdGraphPinType PropertyType;
				PropertyType.PinCategory = UEdGraphSchema_K2::PC_Byte;
				SearchArguments.PinTypes.Add(MoveTemp(PropertyType));
			}
			break;
		}
		default:
		{
			break;
		}
	}

	SavePropertyTemplateSettings();

	BuildPropertyTypeFilterWidget();
}

void SSMSearchView::SavePropertyTemplateSettings()
{
	FilterPropertiesViewModel->PinTypes = SearchArguments.PinTypes;
}

void SSMSearchView::LoadPropertyTemplateSettings()
{
	SearchArguments.PinTypes = FilterPropertiesViewModel->PinTypes;
}

void SSMSearchView::HandlePinTypeChanged(const FEdGraphPinType& InPinType, int32 InIndex)
{
	if (InIndex >= 0)
	{
		check(InIndex < SearchArguments.PinTypes.Num());
		SearchArguments.PinTypes[InIndex] = InPinType;
	}
	else
	{
		SearchArguments.PinTypes.AddUnique(InPinType);
	}

	FilterPropertiesViewModel->PropertyTypeTemplate = ESMPropertyTypeTemplate::None;

	SavePropertyTemplateSettings();

	BuildPropertyTypeFilterWidget();
}

void SSMSearchView::BuildHeader()
{
	if (!ResultsHeaderRow.IsValid())
	{
		SAssignNew(ResultsHeaderRow, SHeaderRow)
		+SHeaderRow::Column(SSMSearchResultItemView::ColumnName_Asset)
		.DefaultLabel(LOCTEXT("ResultListAssetHeader", "Asset"))
		.ManualWidth(120.f)
		.SortPriority(this, &SSMSearchView::GetColumnSortPriority, SSMSearchResultItemView::ColumnName_Asset)
		.SortMode(this, &SSMSearchView::GetColumnSortMode, SSMSearchResultItemView::ColumnName_Asset)
		.OnSort(this, &SSMSearchView::OnColumnSortModeChanged)
		+SHeaderRow::Column(SSMSearchResultItemView::ColumnName_Node)
		.DefaultLabel(LOCTEXT("ResultListNodeHeader", "Node"))
		.ManualWidth(125.f)
		.SortPriority(this, &SSMSearchView::GetColumnSortPriority, SSMSearchResultItemView::ColumnName_Node)
		.SortMode(this, &SSMSearchView::GetColumnSortMode, SSMSearchResultItemView::ColumnName_Node)
		.OnSort(this, &SSMSearchView::OnColumnSortModeChanged)
		+SHeaderRow::Column(SSMSearchResultItemView::ColumnName_Property)
		.DefaultLabel(LOCTEXT("ResultListPropertyHeader", "Property"))
		.ManualWidth(130.f)
		.SortPriority(this, &SSMSearchView::GetColumnSortPriority, SSMSearchResultItemView::ColumnName_Property)
		.SortMode(this, &SSMSearchView::GetColumnSortMode, SSMSearchResultItemView::ColumnName_Property)
		.OnSort(this, &SSMSearchView::OnColumnSortModeChanged)
		+SHeaderRow::Column(SSMSearchResultItemView::ColumnName_Value)
		.DefaultLabel(LOCTEXT("ResultListValueHeader", "Value"))
		.HAlignHeader(HAlign_Left)
		.HAlignCell(HAlign_Fill)
		.SortPriority(this, &SSMSearchView::GetColumnSortPriority, SSMSearchResultItemView::ColumnName_Value)
		.SortMode(this, &SSMSearchView::GetColumnSortMode, SSMSearchResultItemView::ColumnName_Value)
		.OnSort(this, &SSMSearchView::OnColumnSortModeChanged);
	}

	if (HasError())
	{
		if (!bHadError)
		{
			SHeaderRow::FColumn::FArguments Args;
			Args.ColumnId(SSMSearchResultItemView::ColumnName_Error);
			Args.DefaultLabel(LOCTEXT("ResultListErrorHeader", "Error"));
			Args.ManualWidth(32.f);
			Args.SortPriority(this, &SSMSearchView::GetColumnSortPriority, SSMSearchResultItemView::ColumnName_Error);
			Args.SortMode(this, &SSMSearchView::GetColumnSortMode, SSMSearchResultItemView::ColumnName_Error);
			Args.OnSort(this, &SSMSearchView::OnColumnSortModeChanged);
			Args.HAlignCell(HAlign_Center);
			ResultsHeaderRow->InsertColumn(MoveTemp(Args), 0);
			bHadError = true;
		}
	}
	else if (bHadError)
	{
		ResultsHeaderRow->RemoveColumn(SSMSearchResultItemView::ColumnName_Error);
		bHadError = false;
	}
}

EColumnSortMode::Type SSMSearchView::GetColumnSortMode(const FName ColumnId) const
{
	if (ColumnId == PrimarySortedColumn)
	{
		return PrimarySortMode;
	}
	if (ColumnId == SecondarySortedColumn)
	{
		return SecondarySortMode;
	}

	return EColumnSortMode::None;
}

EColumnSortPriority::Type SSMSearchView::GetColumnSortPriority(const FName ColumnId) const
{
	if (ColumnId == PrimarySortedColumn)
	{
		return EColumnSortPriority::Primary;
	}
	if (ColumnId == SecondarySortedColumn)
	{
		return EColumnSortPriority::Secondary;
	}

	return EColumnSortPriority::Max;
}

void SSMSearchView::OnColumnSortModeChanged(const EColumnSortPriority::Type SortPriority, const FName& ColumnId,
	const EColumnSortMode::Type InSortMode)
{
	if (SortPriority == EColumnSortPriority::Primary)
	{
		PrimarySortedColumn = ColumnId;
		PrimarySortMode = InSortMode;

		if (ColumnId == SecondarySortedColumn) // Cannot be primary and secondary at the same time.
		{
			SecondarySortedColumn = FName();
			SecondarySortMode = EColumnSortMode::None;
		}
	}
	else if (SortPriority == EColumnSortPriority::Secondary)
	{
		SecondarySortedColumn = ColumnId;
		SecondarySortMode = InSortMode;
	}

	SortResults();
}

bool SSMSearchView::CanSearch() const
{
	return !IsSearching() && !SearchString.IsEmpty() && !IsFullIndexInProgress();
}

bool SSMSearchView::IsSearching() const
{
	return AsyncSearchHandle.IsValid();
}

TOptional<SSearchBox::FSearchResultData> SSMSearchView::GetSearchResultData() const
{
	SSearchBox::FSearchResultData SearchResultData;
	SearchResultData.NumSearchResults = ResultSummary.SearchResults.Num();
	return MoveTemp(SearchResultData);
}

void SSMSearchView::OnSearchUpdated(const ISMSearch::FSearchSummary& InSearchSummary)
{
	ResultSummary = InSearchSummary;
	RefreshList();
}

void SSMSearchView::OnSearchCompleted(const ISMSearch::FSearchSummary& InSearchSummary)
{
	ResultSummary = InSearchSummary;
	RefreshList();

	AsyncSearchHandle.Reset();

	const FTimespan TimeSpent = InSearchSummary.FinishTime - InSearchSummary.StartTime;
	const FString TimeString = FString::SanitizeFloat(TimeSpent.GetTotalSeconds());

	OperationSummaryString = FString::Printf(TEXT("Found %d matches in %s seconds."), InSearchSummary.SearchResults.Num(), *TimeString);
}

void SSMSearchView::OnSearchCanceled(const ISMSearch::FSearchSummary& InSearchSummary)
{
	if (!IsSearching())
	{
		OnSearchCompleted(InSearchSummary);
	}
}

TSharedRef<ITableRow> SSMSearchView::OnGenerateWidgetForItem(TSharedPtr<ISMSearch::FSearchResult> InItem,
                                                                     const TSharedRef<STableViewBase>& OwnerTable)
{
	if (FilterAssetsViewModel && InItem.IsValid() &&
		SearchSettings->AssetLoadType == ESMAssetLoadType::OnViewable)
	{
		TryLoadAsset(InItem);
	}

	return SNew(SSMSearchResultItemView, SharedThis(this), InItem, OwnerTable, SearchArguments.SearchString);
}

void SSMSearchView::OnResultSelected(TSharedPtr<ISMSearch::FSearchResult> InSelectedItem,
	ESelectInfo::Type SelectInfo)
{
	HighlightProperty(SelectedSearchResult, false);

	SelectedSearchResult = InSelectedItem;

	if (InSelectedItem.IsValid())
	{
		auto OpenGraph = [this](TSharedPtr<ISMSearch::FSearchResult> SelectedItem)
		{
			// Make sure selection hasn't changed.
			if (SelectedItem.IsValid() && SelectedSearchResult.IsValid() && SelectedSearchResult.Pin() == SelectedItem)
			{
				check(SelectedItem->Blueprint.IsValid() && SelectedItem->GraphNode.IsValid());

				StopListeningForBlueprintChanges();

				SelectedItem->Blueprint->OnChanged().AddSP(this, &SSMSearchView::OnBlueprintChanged);
				ActiveBlueprints.Add(SelectedItem->Blueprint);

				UEdGraph* Graph = SelectedItem->GraphNode->GetGraph();
				SAssignNew(GraphPreview, SSMGraphPanel)
					.GraphObj(Graph)
					.IsEditable(true)
					.ShowGraphStateOverlay(false)
					.InitialZoomToFit(false);

				GraphPreviewBorder->SetContent(GraphPreview.ToSharedRef());

				GraphPreview->ScopeToSingleNode(SelectedItem->GraphNode.Get());

				HighlightProperty(SelectedSearchResult, true);
			}
		};

		if (InSelectedItem->GraphNode.IsValid())
		{
			OpenGraph(InSelectedItem);
		}
		else
		{
			TryLoadAsset(InSelectedItem, FSimpleDelegate::CreateLambda([=]()
			{
				OpenGraph(InSelectedItem);
			}));
		}
	}
	else
	{
		ClearSelection();
	}
}

void SSMSearchView::OnDoubleClickItem(TSharedPtr<ISMSearch::FSearchResult> InItem)
{
	if (InItem.IsValid())
	{
		if (InItem->GraphNode.IsValid())
		{
			FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(InItem->GraphNode.Get());
		}
		else if (InItem->Blueprint.IsValid())
		{
			FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(InItem->Blueprint.Get());
		}
	}
}

void SSMSearchView::OnBlueprintChanged(UBlueprint* Blueprint)
{
	if (ActiveBlueprints.Contains(Blueprint))
	{
		GraphPreview->PurgeVisualRepresentation();
		GraphPreview->Update();
	}
}

bool SSMSearchView::CanReplaceSelected() const
{
	return IsReplaceEnabled() && !IsSearching() && SelectedSearchResult.IsValid() && !SearchArguments.SearchString.IsEmpty() && !IsFullIndexInProgress();
}

bool SSMSearchView::CanReplaceAll() const
{
	return IsReplaceEnabled() && !IsSearching() && ResultSummary.SearchResults.Num() > 0 && !SearchArguments.SearchString.IsEmpty();
}

bool SSMSearchView::HasError() const
{
	for (const TSharedPtr<ISMSearch::FSearchResult>& SearchResult : ResultSummary.SearchResults)
	{
		if (SearchResult.IsValid() && SearchResult->HasError())
		{
			return true;
		}
	}

	return false;
}

FDetailsViewArgs SSMSearchView::CreateDetailsArgs()
{
	FNotifyHook* NotifyHook = this;

	FDetailsViewArgs DetailsViewArgs;

	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.bLockable = false;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.NotifyHook = NotifyHook;
	DetailsViewArgs.bSearchInitialKeyFocus = false;
	DetailsViewArgs.ViewIdentifier = NAME_None;
	DetailsViewArgs.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Automatic;
	DetailsViewArgs.bCustomFilterAreaLocation = true;
	DetailsViewArgs.bCustomNameAreaLocation = true;
	DetailsViewArgs.ColumnWidth = 0.7f;
	DetailsViewArgs.bShowPropertyMatrixButton = false;
	DetailsViewArgs.bShowOptions = false;

	return MoveTemp(DetailsViewArgs);
}

#undef LOCTEXT_NAMESPACE
