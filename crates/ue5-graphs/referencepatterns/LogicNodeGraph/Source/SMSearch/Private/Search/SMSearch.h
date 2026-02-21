// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "ISMSearch.h"

#include "Async/AsyncWork.h"

struct FActiveSearch;
struct FAssetData;
class FRegexPattern;

class FSMSearch final : public ISMSearch
{
	typedef TSharedPtr<FActiveSearch, ESPMode::ThreadSafe> FActiveSearchPtr;
	typedef TSharedRef<FActiveSearch, ESPMode::ThreadSafe> FActiveSearchRef;
	
public:
	class FSearchAsyncTask : public FNonAbandonableTask
	{
	public:
		FSMSearch* SearchInstance;
		TWeakPtr<FActiveSearch, ESPMode::ThreadSafe> ActiveSearch;

		FSearchAsyncTask(FSMSearch* InSearchInstance, const FActiveSearchPtr& InActiveSearch);

		TStatId GetStatId() const;
		void DoWork();
	};

	FSMSearch();
	
	// ISMSearch
	virtual ~FSMSearch() override;
	virtual FDelegateHandle SearchAsync(const FSearchArgs& InArgs, const FOnSearchCompleted& InOnSearchCompletedDelegate,
		const FOnSearchUpdated& InOnSearchUpdatedDelegate, const FOnSearchCanceled& InOnSearchCanceledDelegate) override;
	virtual void CancelAsyncSearch(const FDelegateHandle& InDelegateHandle) override;
	virtual FReplaceSummary ReplacePropertyValues(const FReplaceArgs& InReplaceArgs, const FSearchArgs& InSearchArgs) override;
	virtual bool EnableDeferredIndexing(bool bEnable) override;
	virtual void GetIndexingStatus(FIndexingStatus& OutIndexingStatus) override;
	// ~ISMSearch

	void RunSearch(FActiveSearchRef InActiveSearch);

	void BroadcastSearchUpdated(const FActiveSearchRef& InActiveSearch);
	void BroadcastSearchComplete(const FActiveSearchRef& InActiveSearch);
	void BroadcastSearchCanceled(const FActiveSearchRef& InActiveSearch);

private:
	/** Destroy the indexer UE manages. */
	bool ShutdownIndexer();

	/** Make sure everything is cleaned up. */
	void FinishSearch(const FActiveSearchRef& InActiveSearch);

	void SearchStateMachine(USMBlueprint* InBlueprint, const FActiveSearchRef& InActiveSearch, TArray<TSharedPtr<FSearchResult>>& OutResults) const;
	void SearchObject(UObject* InObject, FActiveSearchRef InActiveSearch, TArray<TSharedPtr<FSearchResult>>& OutResults) const;
	TSharedPtr<FSearchResult> SearchProperty(FProperty* InProperty, UObject* InObject, const FActiveSearchRef& InActiveSearch, int32 InPropertyIndex = 0) const;

	/** Search a string and record any matches under a single result. */
	TSharedPtr<FSearchResult> SearchString(const FString& InString, const FActiveSearchRef& InActiveSearch) const;

	/** Checks if the asset shouldn't be included. */
	bool IsAssetFilteredOut(const FAssetData& InAssetData, const FSearchArgs& InArgs) const;

	/** Create a regex pattern from the given args. */
	TSharedPtr<FRegexPattern> CreateRegexPattern(const FSearchArgs& InArgs) const;

	/** Locate the child result that contains the default value string. */
	bool FindDefaultValueResult(const TSharedPtr<FFindInBlueprintsResult>& InResult, TArray<TSharedPtr<FFindInBlueprintsResult>>& OutValueResults) const;

	/** Find the parent class node. */
	TSharedPtr<FFindInBlueprintsResult> FindParentResult(const TSharedPtr<FFindInBlueprintsResult>& InResult) const;

	/** Find the UEdGraphNode result. */
	TSharedPtr<FFindInBlueprintsResult> FindNodeResult(const TSharedPtr<FFindInBlueprintsResult>& InDefaultValueResult) const;

	/** Locate the child result containing property information. */
	TSharedPtr<FSearchResultFiB> CreateFiBResult(const TSharedPtr<FFindInBlueprintsResult>& InDefaultValueResult, const TSharedPtr<FFindInBlueprintsResult>& TopMostResult) const;

private:
	/** The default value prefix UE uses. */
	FString DefaultValuePrefix;

	/** Default value containing LD's node data. */
	FString NodeDataPrefix;

	/** Each registered delegate mapped to the active search. */
	TMap<FDelegateHandle, FActiveSearchPtr> ActiveSearches;

	/** If set construction scripts should be enabled after searching. */
	bool bReEnableConstructionScriptsOnLoad = false;

	/** The local deferred indexing status which may not be the same as the engine status. */
	TOptional<bool> bDeferredIndexingEnabled;
};
