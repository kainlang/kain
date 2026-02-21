// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "ISMSearch.h"

#include "IDetailsView.h"
#include "Misc/NotifyHook.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"

enum class EFiBCacheOpFlags;
enum class EFiBCacheOpType;
enum class ESMAssetLoadType;
enum class ESMPropertyTypeTemplate;
class USearchFilterAssetsViewModel;
class USearchFilterPropertiesViewModel;
class USMSearchSettings;
class SSMGraphPanel;
class SMultiLineEditableTextBox;

class SSMSearchView : public SCompoundWidget, public FNotifyHook
{
public:
	SLATE_BEGIN_ARGS(SSMSearchView) {}

	SLATE_END_ARGS()

	virtual ~SSMSearchView() override;

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

	// SCompoundWidget
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	// ~SCompoundWidget

	/**
	 * Checks if this view is loading an asset.
	 * @param InPath The asset path.
	 */
	bool IsAssetLoading(const FString& InPath) const;

protected:
	/** Start an async search. */
	void StartSearch();
	/** Cancel an async search. */
	void CancelSearch();
	/** Clear the current result user selection. */
	void ClearSelection();
	/** Clear all loaded results. */
	void ClearResults();
	/** Sort any loaded results. */
	void SortResults();

	/** Refresh the search if applicable. */
	void RefreshSearch();

	/** Run a synchronous replacement on all searches. */
	void ReplaceAll();

	/** Run a synchronous replacement on the selected searches. */
	void ReplaceSelected();

	/** Replace only the given results. */
	void Replace(const TArray<TSharedPtr<ISMSearch::FSearchResult>>& InSearchResults);

	/** Remove OnChange listeners from all active blueprints. */
	void StopListeningForBlueprintChanges();

	/**
	 * Refresh the results list.
	 * @param bFullRebuild Force refresh the list, otherwise it only refreshes on a delta change to element count.
	 */
	void RefreshList(bool bFullRebuild = false);

	/** Perform indexing of all state machine blueprints. */
	void IndexAllBlueprints();

	/** Cancel our own compile indexing. */
	void CancelIndexAllBlueprints();

	/** Attempt to cancel a UE cache. */
	void CancelCaching();

	/**
	 * Tries to load an asset if it is not already loading.
	 * @return true if the asset is now being loaded, false if it is already being loaded.
	 */
	bool TryLoadAsset(const TSharedPtr<ISMSearch::FSearchResult>& InItem, const FSimpleDelegate& OnLoadDelegate = FSimpleDelegate());

	/**
	 * Run TryResolve on all items.
	 */
	void TryResolveAllObjects();

	/** Switch the current load type. */
	void SwitchAssetLoadType(ESMAssetLoadType InLoadType);

	/** Highlight or remove the highlight of a search result. */
	void HighlightProperty(TWeakPtr<ISMSearch::FSearchResult> InSearchResult, bool bValue);

private:
	void OnSearchTextCommitted(const FText& InFilterText, ETextCommit::Type InCommitType);
	void OnSearchTextChanged(const FText& InFilterText);
	void OnReplaceTextCommitted(const FText& InFilterText, ETextCommit::Type InCommitType);
	void OnReplaceTextChanged(const FText& InFilterText);

	/** Make the progress bar for the search status. */
	TSharedRef<SWidget> MakeSearchProgressBarWidget();

	/** Make the progress bar when caching assets. */
	TSharedRef<SWidget> MakeCacheProgressBarWidget();

	/** Callback to return the cache bar's display text, informing the user of the situation */
	FText GetCacheProgressBarStatusText() const;

	/** If UE is caching any blueprints. */
	bool IsCacheInProgress() const;

	/** If we are compiling blueprints. */
	bool IsFullIndexInProgress() const;

	/** Progress searching blueprints. */
	TOptional<float> GetSearchPercentComplete() const;

	/** Progress percent caching blueprints. */
	TOptional<float> GetCachePercentComplete() const;

	/** Progress when we are compiling blueprints for indexing. */
	TOptional<float> GetFullIndexPercentComplete() const;

	/** Make the "Options" menu. */
	TSharedRef<SWidget> MakeAddFilterMenu();
	void MakeIndexingAndLoadingSubMenu(FMenuBuilder& MenuBuilder);
	void MakeIndexSubMenu(FMenuBuilder& MenuBuilder);
	void MakeAssetLoadSubMenu(FMenuBuilder& MenuBuilder);
	void MakeAssetSubMenu(FMenuBuilder& MenuBuilder);
	void MakePropertySubMenu(FMenuBuilder& MenuBuilder);
	void MakePropertyTypesSubMenu(FMenuBuilder& MenuBuilder);
	void BuildPropertyTypeFilterWidget();

	/** When a property template is changed. */
	void HandleTemplateChanged(ESMPropertyTypeTemplate NewTemplate);

	/** Saves the current template settings. */
	void SavePropertyTemplateSettings();

	/** Load the template settings. */
	void LoadPropertyTemplateSettings();

	/** When an individual property type is changed. */
	void HandlePinTypeChanged(const FEdGraphPinType& InPinType, int32 InIndex);

	void BuildHeader();
	EColumnSortMode::Type GetColumnSortMode(const FName ColumnId) const;
	EColumnSortPriority::Type GetColumnSortPriority(const FName ColumnId) const;
	void OnColumnSortModeChanged(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type InSortMode);

	bool CanSearch() const;
	bool IsSearching() const;
	TOptional<SSearchBox::FSearchResultData> GetSearchResultData() const;
	void OnSearchUpdated(const ISMSearch::FSearchSummary& InSearchSummary);
	void OnSearchCompleted(const ISMSearch::FSearchSummary& InSearchSummary);
	void OnSearchCanceled(const ISMSearch::FSearchSummary& InSearchSummary);

	TSharedRef<ITableRow> OnGenerateWidgetForItem(TSharedPtr<ISMSearch::FSearchResult> InItem, const TSharedRef<STableViewBase>& OwnerTable);
	void OnResultSelected(TSharedPtr<ISMSearch::FSearchResult> InSelectedItem, ESelectInfo::Type SelectInfo);
	void OnDoubleClickItem(TSharedPtr<ISMSearch::FSearchResult> InItem);

	void OnBlueprintChanged(UBlueprint* Blueprint);

	/** If the user has opted in to replace mode. */
	bool IsReplaceEnabled() const { return bReplaceEnabled; };

	/** If replacement is allowed on the given selection. */
	bool CanReplaceSelected() const;

	/** If a replacement is allowed given current search status. */
	bool CanReplaceAll() const;

	/** Checks if there is an error present. */
	bool HasError() const;

	/** Create details panel object to use in menus. */
	FDetailsViewArgs CreateDetailsArgs();

private:
	TSharedPtr<FUICommandList> CommandList;

	/** The search settings for this editor project. */
	USMSearchSettings* SearchSettings = nullptr;
	USearchFilterPropertiesViewModel* FilterPropertiesViewModel = nullptr;
	USearchFilterAssetsViewModel* FilterAssetsViewModel = nullptr;

	/** String entered in to the search bar, updated on change. */
	FString SearchString;
	/** String entered into the replace bar, updated on change. */
	FString ReplaceString;
	/** The summary of an operation. */
	FString OperationSummaryString;

	ISMSearch::FSearchArgs SearchArguments;
	ISMSearch::FReplaceArgs ReplaceArguments;

	TSharedPtr<SBorder> GraphPreviewBorder;
	TSharedPtr<SSMGraphPanel> GraphPreview;
	TSharedPtr<SSearchBox> SearchBox;
	TSharedPtr<SMultiLineEditableTextBox> ReplacementTextBox;

	/** Individual pin type selection. */
	TSharedPtr<SVerticalBox> PinTypeSelectionBox;

	/** The results header. */
	TSharedPtr<SHeaderRow> ResultsHeaderRow;

	/** The list view containing results. */
	TSharedPtr<SListView<TSharedPtr<ISMSearch::FSearchResult>>> ResultsListView;

	/** The selected search result. */
	TWeakPtr<ISMSearch::FSearchResult> SelectedSearchResult;

	/** The search result data. */
	ISMSearch::FSearchSummary ResultSummary;

	/** The replace result data. */
	ISMSearch::FReplaceSummary ReplaceSummary;

	/** Delegate handle to the current search. */
	FDelegateHandle AsyncSearchHandle;

	/** Assets currently loading asynchronously. */
	TMap<FString, TSharedPtr<FStreamableHandle>> AssetsLoading;

	/** Blueprints that are being monitored for changes. */
	TSet<TWeakObjectPtr<UBlueprint>> ActiveBlueprints;

	// Sorting.
	EColumnSortMode::Type PrimarySortMode = EColumnSortMode::None;
	EColumnSortMode::Type SecondarySortMode = EColumnSortMode::None;
	FName PrimarySortedColumn;
	FName SecondarySortedColumn;

	/** The current size of the widget, set by OnArrangeChildren. */
	mutable FVector2D WidgetSize;

	/** Last cached asset name (used during continuous cache operations). */
	mutable FSoftObjectPath LastCachedAssetPath;

	/** Record the time when a suspected stuck cache is detected. */
	float TimeSinceStuckCacheCheck = 0.f;

	/** If text replacement is allowed. */
	bool bReplaceEnabled = false;
	bool bHadError = false;
	bool bFilterMenuToggled = false;
};
