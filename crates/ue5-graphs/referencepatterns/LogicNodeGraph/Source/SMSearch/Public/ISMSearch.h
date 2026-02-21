// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "EdGraph/EdGraphPin.h"
#include "UObject/SoftObjectPtr.h"

class FFindInBlueprintsResult;
class USMBlueprint;
class USMInstance;
class USMNodeInstance;
class UEdGraphNode;

struct FStreamableHandle;

class ISMSearch
{
public:
	virtual ~ISMSearch() {}

	/** Arguments for searching for field values within assets. */
	struct FSearchArgs
	{
		/** [Required] Word or phrase to search for. */
		FString SearchString;

		/** [Optional] Limit the search to these paths. */
		TArray<FName> PackagePaths;

		/** [Optional] Classes to filter. Same effect as adding an asset path to PackagePaths. */
		TArray<TSoftClassPtr<USMInstance>> StateMachineClasses;

		/** [Optional] Limit the search to these property names. */
		TSet<FName> PropertyNames;

		/** [Optional] Limit the search to these property types. */
		TArray<FEdGraphPinType> PinTypes;

		/** [Optional] Include subclasses of any StateMachineClasses. This requires all SMBlueprints loaded. */
		bool bIncludeSubClasses = false;

		/** [Optional] If the search should be case sensitive. */
		bool bCaseSensitive = false;

		/** [Optional] If only the full word(s) should be searched. */
		bool bFullWord = false;

		/** [Optional] Use regular expressions in the search. */
		bool bRegex = false;

		/**
		 * [Optional] Allow construction scripts to run when an asset is loaded from search. This is
		 * disabled for performance.
		 */
		bool bAllowConstructionScriptsOnLoad = false;
	};

	/** Results of a value replacement. */
	class FReplaceResult
	{
	public:

		/** The new value which replaced the old value. */
		FString NewValue;

		/** An error message if an error occurred. */
		FText ErrorMessage;
	};

	class FSearchResultFiB
	{
	public:

		TSharedPtr<FFindInBlueprintsResult> Blueprint;
		TSharedPtr<FFindInBlueprintsResult> Parent;
		TSharedPtr<FFindInBlueprintsResult> Graph;
		TSharedPtr<FFindInBlueprintsResult> GraphNode;
		TSharedPtr<FFindInBlueprintsResult> GraphPin;

		// These are extracted from FiB node data.
		FString NodeName;
		FString PropertyName;
		FGuid NodeGuid;
		FGuid PropertyGuid;
		int32 ArrayIndex = INDEX_NONE;

		void Finalize();

		FORCEINLINE bool operator==(const FSearchResultFiB& Other) const
		{
			return Blueprint.Get() == Other.Blueprint.Get()
				&& Parent.Get() == Other.Parent.Get()
				&& Graph.Get() == Other.Graph.Get()
				&& GraphNode.Get() == Other.GraphNode.Get()
				&& GraphPin.Get() == Other.GraphPin.Get()
				&& NodeName == Other.NodeName
				&& PropertyName == Other.PropertyName
				&& NodeGuid == Other.NodeGuid
				&& PropertyGuid == Other.PropertyGuid
				&& ArrayIndex == Other.ArrayIndex;
		}

		FORCEINLINE bool operator!=(const FSearchResultFiB& Other) const
		{
			return !(*this == Other);
		}
	};

	/** Results of a search. Replacement results are also stored here. */
	class FSearchResult
	{
	public:

		/** The path to the blueprint, used to load the asset if needed. */
		FString BlueprintPath;

		/** The result returned from Find In Blueprints. */
		TSharedPtr<FSearchResultFiB> FiBResult;

		/** The blueprint containing the text. */
		TWeakObjectPtr<UBlueprint> Blueprint;

		/** The graph node containing the property if applicable. */
		TWeakObjectPtr<UEdGraphNode> GraphNode;

		/** The node instance owning the property. */
		TWeakObjectPtr<USMNodeInstance> NodeInstance;

		/** The property containing the text. */
		FProperty* Property = nullptr;

		/** The index of the property if an array. */
		int32 PropertyIndex = 0;

		/** All occurrences of the search text found in the property value. */
		TArray<FTextRange> MatchedTextRanges;

		/** The text containing the search string. This is either the exported value or the literal value. */
		FString PropertyValue;

		/** The saved namespace if reading a text value. */
		FString Namespace;

		/** The saved key if reading a text value. */
		FString Key;

		/** The replace result, if one exists. */
		TSharedPtr<FReplaceResult> ReplaceResult;

		/** If construction scripts should run on load. Generally managed by search args. */
		bool bAllowConstructionScriptsOnLoad = false;

		/** Return the blueprint name */
		FString GetBlueprintName() const;

		/** Return the node name. */
		FString GetNodeName() const;

		/** Return the property name. */
		FString GetPropertyName() const;

		/** Return the array index this property falls in. INDEX_NONE if not an array. */
		int32 GetPropertyIndex() const;

		/** Return the first matched character index or INDEX_NONE. */
		int32 GetBeginMatchedIndex() const;

		/** Return the last matched character index or INDEX_NONE. */
		int32 GetEndMatchedIndex() const;

		/**
		 * Checks if a given range is contained within the matched text ranges.
		 * @return The array index of the matching MatchedTextRanges.
		 */
		int32 FindMatchedTextRangeIntersectingRange(const FTextRange& InRange) const;

		/** If an error occurred during search or replace. */
		bool HasError() const;

		/**
		 * Call check() on pointers which should be valid.
		 */
		void CheckResult() const;

		/**
		 * Tries to set object fields if they are loaded. Does not load objects.
		 */
		void TryResolveObjects();

		/**
		 * Attempt to load any objects from paths and guids.
		 */
		void LoadObjects();

		/**
		 * Attempt to async load any objects from paths and guids.
		 * @return The streamable handle to the async load. Will be null if no async load occurred.
		 */
		TSharedPtr<FStreamableHandle> AsyncLoadObjects(const FSimpleDelegate& InOnLoadedDelegate = FSimpleDelegate());

	private:
		FSimpleDelegate OnLoadDelegate;

	public:
		FORCEINLINE bool operator==(const FSearchResult& Other) const
		{
			if (BlueprintPath == Other.BlueprintPath
				&& Blueprint.Get() == Other.Blueprint.Get()
				&& GraphNode.Get() == Other.GraphNode.Get()
				&& NodeInstance.Get() == Other.NodeInstance.Get()
				&& FiBResult == Other.FiBResult
				&& Property == Other.Property
				&& PropertyIndex == Other.PropertyIndex
				&& PropertyValue == Other.PropertyValue
				&& MatchedTextRanges.Num() == Other.MatchedTextRanges.Num())
			{
				for (int32 TextRangeIdx = 0; TextRangeIdx < MatchedTextRanges.Num(); ++TextRangeIdx)
				{
					if (MatchedTextRanges[TextRangeIdx] != Other.MatchedTextRanges[TextRangeIdx])
					{
						return false;
					}
				}

				return true;
			}

			return false;
		}

		FORCEINLINE bool operator!=(const FSearchResult& Other) const
		{
			return !(*this == Other);
		}
	};

	/** Summary of the search. */
	struct FSearchSummary
	{
		/** All results found containing the search string. */
		TArray<TSharedPtr<FSearchResult>> SearchResults;

		/** UTC time the search started. */
		FDateTime StartTime;

		/** UTC time the search finished. */
		FDateTime FinishTime;

		/** Progress of the search. */
		float Progress = 0.f;

		/** When the search has been completed. */
		bool bComplete = false;
	};

	/** Arguments for replacing field values.  */
	struct FReplaceArgs
	{
		/** [Required] The word or phrase replacing the search text. */
		FString ReplaceString;

		/** [Required] Valid search results containing the properties to be updated. */
		TArray<TSharedPtr<FSearchResult>> SearchResults;
	};

	/** Summary of a find and replace. */
	struct FReplaceSummary
	{
		/** All results from the replacement operation. Each search result will contain a replacement result if successful. */
		TArray<TSharedPtr<FSearchResult>> SearchResults;
	};

	DECLARE_DELEGATE_OneParam(FOnSearchUpdated, const FSearchSummary& /* Summary */);
	DECLARE_DELEGATE_OneParam(FOnSearchCompleted, const FSearchSummary& /* Summary */);
	DECLARE_DELEGATE_OneParam(FOnSearchCanceled, const FSearchSummary& /* Summary */);

	/**
	 * Search for strings within exposed properties on a separate thread.
	 *
	 * @param InArgs Arguments to configure the search.
	 * @param InOnSearchCompletedDelegate The delegate to call when the search is complete.
	 * @param InOnSearchUpdatedDelegate The delegate to call when a search is updated.
	 * @param InOnSearchCanceledDelegate The delegate to call if a search is canceled.
	 *
	 * @return The registered delegate handle. This is only needed if this search needs to be canceled.
	 */
	virtual FDelegateHandle SearchAsync(const FSearchArgs& InArgs, const FOnSearchCompleted& InOnSearchCompletedDelegate,
		const FOnSearchUpdated& InOnSearchUpdatedDelegate = FOnSearchUpdated(), const FOnSearchCanceled& InOnSearchCanceledDelegate = FOnSearchCanceled()) = 0;

	/**
	 * Cancel an active async search.
	 *
	 * @param InDelegateHandle The handle for OnSearchCompletedDelegate.
	 */
	virtual void CancelAsyncSearch(const FDelegateHandle& InDelegateHandle) = 0;

	/**
	 * Replace strings within property values.
	 *
	 * @param InReplaceArgs The replacement arguments.
	 * @param InSearchArgs the original search arguments.
	 *
	 * @return The summary of the replacement which will contain any errors that occurred.
	 */
	virtual FReplaceSummary ReplacePropertyValues(const FReplaceArgs& InReplaceArgs, const FSearchArgs& InSearchArgs) = 0;

	/**
	 * Force recreate the UE indexer with or without deferred indexing support.
	 * When deferred indexing is enabled with multi-threading, the UE indexer can become stuck when a BP is recompiled.
	 *
	 * @param bEnable Enable or disable deferred indexing.
	 * @return the value deferred indexing is now set to. This may not equal bEnable if the indexer was unable to be restarted.
	 */
	virtual bool EnableDeferredIndexing(bool bEnable) = 0;

	struct FIndexingStatus
	{
		/** The engine ini file has this enabled. */
		bool bDeferredIndexingEnabledInEngineConfig = false;

		/**
		 * Logic Driver is locally overriding deferred indexing. Only set once EnableDeferredIndexing has been called
		 * and the value changed.
		 */
		TOptional<bool> bDeferredIndexingEnabledInLogicDriver = false;
	};

	/**
	 * Retrieve the current indexing values.
	 */
	virtual void GetIndexingStatus(FIndexingStatus& OutIndexingStatus) = 0;
};