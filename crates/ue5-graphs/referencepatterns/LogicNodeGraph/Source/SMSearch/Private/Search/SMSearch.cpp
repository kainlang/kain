// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMSearch.h"

#include "SMSearchLog.h"

#include "Blueprints/SMBlueprint.h"
#include "Blueprints/SMBlueprintGeneratedClass.h"
#include "SMInstance.h"

#include "ISMAssetToolsModule.h"
#include "ISMGraphGeneration.h"

#include "Graph/Nodes/SMGraphNode_Base.h"
#include "Utilities/SMBlueprintEditorUtils.h"
#include "Utilities/SMIndexerUtils.h"
#include "Utilities/SMPropertyUtils.h"
#include "Utilities/SMTextUtils.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Async/AsyncWork.h"
#include "CoreGlobals.h"
#include "FindInBlueprintManager.h"
#include "FindInBlueprints.h"
#include "HAL/ThreadSafeBool.h"
#include "Internationalization/Regex.h"
#include "Misc/ConfigCacheIni.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "SMSearch"

struct FActiveSearch
{
	ISMSearch::FSearchArgs SearchArgs;
	ISMSearch::FSearchSummary SummaryResult;
	TSharedPtr<FRegexPattern> RegexPattern;

	ISMSearch::FOnSearchUpdated OnSearchUpdatedDelegate;
	ISMSearch::FOnSearchCompleted OnSearchCompletedDelegate;
	ISMSearch::FOnSearchCanceled OnSearchCanceledDelegate;
	TUniquePtr<FAsyncTask<FSMSearch::FSearchAsyncTask>> AsyncTask;

	TSharedPtr<FStreamSearch> StreamSearch;

	FThreadSafeBool bCancel = false;

	float LastPercentComplete = -1.f;
};

FSMSearch::FSearchAsyncTask::FSearchAsyncTask(FSMSearch* InSearchInstance,
	const FActiveSearchPtr& InActiveSearch)
{
	SearchInstance = InSearchInstance;
	ActiveSearch = InActiveSearch;
}

TStatId FSMSearch::FSearchAsyncTask::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(LogicDriverSearchAsyncTask, STATGROUP_ThreadPoolAsyncTasks);
}

void FSMSearch::FSearchAsyncTask::DoWork()
{
	if (SearchInstance && ActiveSearch.IsValid())
	{
		SearchInstance->RunSearch(ActiveSearch.Pin().ToSharedRef());

		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateLambda([this] ()
			{
				if (SearchInstance && ActiveSearch.IsValid())
				{
					if (ActiveSearch.Pin()->bCancel)
					{
						SearchInstance->BroadcastSearchCanceled(ActiveSearch.Pin().ToSharedRef());
					}
					else
					{
						SearchInstance->BroadcastSearchComplete(ActiveSearch.Pin().ToSharedRef());
					}
				}
			}),
			TStatId(),
			nullptr,
			ENamedThreads::GameThread);
	}
}

FSMSearch::FSMSearch()
{
	DefaultValuePrefix = FString::Printf(TEXT("%s: "), *FFindInBlueprintSearchTags::FiB_DefaultValue.ToString());
	NodeDataPrefix = FString::Printf(TEXT("%s: _"), *FSMSearchTags::FiB_NodeData.ToString());
}

FSMSearch::~FSMSearch()
{
}

FDelegateHandle FSMSearch::SearchAsync(const FSearchArgs& InArgs,
	const FOnSearchCompleted& InOnSearchCompletedDelegate,
	const FOnSearchUpdated& InOnSearchUpdatedDelegate,
	const FOnSearchCanceled& InOnSearchCanceledDelegate)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SearchPropertyValuesAsync"), STAT_SearchPropertyValues, STATGROUP_LogicDriverSearch);

	FActiveSearchPtr ActiveSearch = MakeShared<FActiveSearch, ESPMode::ThreadSafe>();
	ActiveSearch->OnSearchUpdatedDelegate = InOnSearchUpdatedDelegate;
	ActiveSearch->OnSearchCompletedDelegate = InOnSearchCompletedDelegate;
	ActiveSearch->OnSearchCanceledDelegate = InOnSearchCanceledDelegate;
	ActiveSearch->SearchArgs = InArgs;
	ActiveSearch->RegexPattern = CreateRegexPattern(InArgs);
	ActiveSearch->AsyncTask = MakeUnique<FAsyncTask<FSearchAsyncTask>>(this, ActiveSearch);

	const FDelegateHandle Handle = ActiveSearch->OnSearchCompletedDelegate.GetHandle();
	ActiveSearches.Add(Handle, ActiveSearch);

	ActiveSearch->AsyncTask->StartBackgroundTask();

	return Handle;
}

void FSMSearch::CancelAsyncSearch(const FDelegateHandle& InDelegateHandle)
{
	if (const FActiveSearchPtr* ActiveSearchPtr = ActiveSearches.Find(InDelegateHandle))
	{
		const FActiveSearchPtr ActiveSearch = *ActiveSearchPtr;
		if (ActiveSearch.IsValid())
		{
			ActiveSearch->bCancel = true;
			if (ActiveSearch->AsyncTask.IsValid())
			{
				if (!ActiveSearch->AsyncTask->Cancel())
				{
					ActiveSearch->AsyncTask->EnsureCompletion();
					return;
				}
				ActiveSearch->AsyncTask.Reset();
			}

			ActiveSearch->OnSearchCompletedDelegate.Unbind();
		}

		ActiveSearches.Remove(InDelegateHandle);
	}
}

ISMSearch::FReplaceSummary FSMSearch::ReplacePropertyValues(const FReplaceArgs& InReplaceArgs, const FSearchArgs& InSearchArgs)
{
	FScopedTransaction Transaction(LOCTEXT("ReplacePropertyValues", "Replace Property Values"));

	FReplaceSummary Summary;

	TSet<UBlueprint*> BlueprintsUpdated;

	for (const TSharedPtr<FSearchResult>& Result : InReplaceArgs.SearchResults)
	{
		check(Result.IsValid());

		// Force objects to load if they haven't. This may be slow!
		Result->LoadObjects();
		Result->CheckResult();

		const TSharedPtr<FReplaceResult> ReplacementResult = MakeShared<FReplaceResult>();
		Result->ReplaceResult = ReplacementResult;

		// Verify value hasn't changed since the previous search.
		{
			const FActiveSearchRef ActiveSearch = MakeShared<FActiveSearch, ESPMode::ThreadSafe>();
			ActiveSearch->SearchArgs = InSearchArgs;
			ActiveSearch->RegexPattern = CreateRegexPattern(InSearchArgs);

			TSharedPtr<FSearchResult> CurrentResult = SearchProperty(Result->Property,
				Result->NodeInstance.Get(), ActiveSearch, Result->PropertyIndex);

			if (!CurrentResult.IsValid() || CurrentResult->PropertyValue != Result->PropertyValue)
			{
				ReplacementResult->ErrorMessage = LOCTEXT("ErrorMessageValueModified", "Value not replaced. The property value has been modified since the last search.");
				LDSEARCH_LOG_ERROR(TEXT("Could not replace property %s's value. It has been modified since the last search. \
Expected value: '%s', search value: '%s', replacement value: '%s'."),
				*Result->Property->GetName(), *Result->PropertyValue, *InSearchArgs.SearchString, *InReplaceArgs.ReplaceString);
				continue;
			}
		}

		// Update the property value to the replacement string.
		{
			FString ReplacementString = Result->PropertyValue;

			int32 Slack = 0;
			for (const FTextRange& LineRange : Result->MatchedTextRanges)
			{
				// In case the replacement word has more or less characters.
				const int32 AdjustedIndex = LineRange.BeginIndex + Slack;

				ReplacementString.RemoveAt(AdjustedIndex, LineRange.Len());
				ReplacementString.InsertAt(AdjustedIndex, InReplaceArgs.ReplaceString);

				Slack += InReplaceArgs.ReplaceString.Len() - LineRange.Len();
			}

			// Handle literal to non-literal text to maintain localization.
			if ((!Result->Namespace.IsEmpty() || !Result->Key.IsEmpty()))
			{
				FText ReplacementText = FText::ChangeKey(Result->Namespace, Result->Key, FText::FromString(ReplacementString));
				ReplacementString = LD::TextUtils::TextToStringBuffer(ReplacementText);
			}

			const ISMAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<ISMAssetToolsModule>(
LOGICDRIVER_ASSET_TOOLS_MODULE_NAME);

			if (USMGraphNode_Base* SMGraphNode = Cast<USMGraphNode_Base>(Result->GraphNode.Get()))
			{
				// State machine node.
				ISMGraphGeneration::FSetNodePropertyArgs PropertyArgs;
				PropertyArgs.PropertyName = Result->Property->GetFName();
				PropertyArgs.PropertyDefaultValue = ReplacementString;
				PropertyArgs.PropertyIndex = Result->PropertyIndex;
				PropertyArgs.NodeInstance = Result->NodeInstance.Get();
				const bool bSuccess = AssetToolsModule.GetGraphGenerationInterface()->SetNodePropertyValue(SMGraphNode, PropertyArgs);
				if (!bSuccess)
				{
					ReplacementResult->ErrorMessage = LOCTEXT("ErrorMessageValueNotSet", "Value not replaced. There was an error setting the value of the node property.");
					LDSEARCH_LOG_ERROR(TEXT("Could not replace property %s's value. There was an error setting the value of the node property."),
					*Result->Property->GetName());
					continue;
				}
			}
			else
			{
				// Generic property.
				LD::PropertyUtils::SetPropertyValue(Result->Property, ReplacementString, Result->NodeInstance.Get(), Result->PropertyIndex);
			}

			// Use literal text.
			FText Text;
			if (const TCHAR* Success = FTextStringHelper::ReadFromBuffer(*ReplacementString, Text))
			{
				ReplacementString = Text.ToString();
			}

			ReplacementResult->NewValue = MoveTemp(ReplacementString);

			BlueprintsUpdated.Add(Result->Blueprint.Get());
		}
	}

	// Update index data.
	for (UBlueprint* Blueprint: BlueprintsUpdated)
	{
		FFindInBlueprintSearchManager& FiBManager = FFindInBlueprintSearchManager::Get();
		FSearchData UpdatedSearchData = FiBManager.QuerySingleBlueprint(Blueprint, true);

		FiBManager.ApplySearchDataToDatabase(MoveTemp(UpdatedSearchData));
	}

	Summary.SearchResults = InReplaceArgs.SearchResults;
	return MoveTemp(Summary);
}

bool FSMSearch::EnableDeferredIndexing(bool bEnable)
{
	check(IsInGameThread());

	const FFindInBlueprintSearchManager* Instance = FFindInBlueprintSearchManager::Instance;

	bool bWasDisabled = false;
	GConfig->GetBool(TEXT("BlueprintSearchSettings"), TEXT("bDisableDeferredIndexing"), bWasDisabled, GEditorIni);

	// Don't bother changing if it's set locally or already matching the engine setting.
	if (Instance != nullptr && ((bDeferredIndexingEnabled.IsSet() && bEnable == *bDeferredIndexingEnabled) ||
		(!bDeferredIndexingEnabled.IsSet() && !bWasDisabled == bEnable)))
	{
		return bEnable;
	}

	GConfig->SetBool(TEXT("BlueprintSearchSettings"), TEXT("bDisableDeferredIndexing"), !bEnable, GEditorIni);

	// If we successfully restarted the indexer with the new value.
	const bool bValueSet = ShutdownIndexer();
	if (bValueSet)
	{
		bDeferredIndexingEnabled = bEnable;
	}

	// Re-instantiate the manager. This is the only way for the manager to recognize updated settings.
	FFindInBlueprintSearchManager::Get();

	// Restore the original user value. The new value will already be loaded into the manager.
	GConfig->SetBool(TEXT("BlueprintSearchSettings"), TEXT("bDisableDeferredIndexing"), bWasDisabled, GEditorIni);

	return bDeferredIndexingEnabled.Get(false);
}

void FSMSearch::GetIndexingStatus(FIndexingStatus& OutIndexingStatus)
{
	GConfig->GetBool(TEXT("BlueprintSearchSettings"), TEXT("bDisableDeferredIndexing"), OutIndexingStatus.bDeferredIndexingEnabledInEngineConfig, GEditorIni);
	OutIndexingStatus.bDeferredIndexingEnabledInEngineConfig = !OutIndexingStatus.bDeferredIndexingEnabledInEngineConfig;
	OutIndexingStatus.bDeferredIndexingEnabledInLogicDriver = bDeferredIndexingEnabled;
}

void FSMSearch::RunSearch(FActiveSearchRef InActiveSearch)
{
	InActiveSearch->SummaryResult.StartTime = FDateTime::UtcNow();

	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	// Convert classes to package paths.
	TArray<FName> ClassAssetPathNames;
	TArray<FTopLevelAssetPath> ClassNames;
	for (const TSoftClassPtr<USMInstance>& SoftClassPtr : InActiveSearch->SearchArgs.StateMachineClasses)
	{
		const FSoftObjectPath& SoftObjectPath = SoftClassPtr.ToSoftObjectPath();
		ClassAssetPathNames.AddUnique(*SoftObjectPath.GetLongPackageName());

		if (InActiveSearch->SearchArgs.bIncludeSubClasses)
		{
			ClassNames.Add({ SoftObjectPath.GetLongPackageFName(), *SoftObjectPath.GetAssetName() });
		}
	}

	const bool bLookingForSubClasses = ClassNames.Num() > 0;

	TArray<FString> ParentClassNames
	{ TEXT("ParentClass=") + USMBlueprintGeneratedClass::StaticClass()->GetName(), TEXT("ParentClass=") + USMInstance::StaticClass()->GetName() };

	// Find subclasses, replace parent classes with these.
	if (bLookingForSubClasses)
	{
		TSet<FTopLevelAssetPath> DerivedClassNames;
		AssetRegistryModule.Get().GetDerivedClassNames(ClassNames, {}, DerivedClassNames);
		ParentClassNames.Reset(DerivedClassNames.Num());
		for (const FTopLevelAssetPath& ClassName : DerivedClassNames)
		{
			ParentClassNames.Add(TEXT("ParentClass=") + ClassName.ToString());
		}
	}
	FString ParentClassFilter = ParentClassNames.Num() == 0 ? TEXT("") : FString::Printf(TEXT("(%s)"), *FString::Join(ParentClassNames, TEXT("||")));

	// Directories that all matches must fall in.
	TArray<FString> DirectoryPathStrings;
	DirectoryPathStrings.Reserve(InActiveSearch->SearchArgs.PackagePaths.Num());
	for (const FName& Path : InActiveSearch->SearchArgs.PackagePaths)
	{
		DirectoryPathStrings.Add(TEXT("Path=") + Path.ToString());
	}

	// Classes that are treated as paths.
	TArray<FString> ClassAssetPathStrings;
	ClassAssetPathStrings.Reserve(ClassAssetPathNames.Num());
	for (const FName& Path : ClassAssetPathNames)
	{
		ClassAssetPathStrings.Add(TEXT("Path=") + Path.ToString());
	}

	const FString DirectoryFilter = DirectoryPathStrings.Num() == 0 ? TEXT("") : FString::Printf(TEXT("(%s)"), *FString::Join(DirectoryPathStrings, TEXT("||")));
	const FString ClassPathFilter = ClassAssetPathNames.Num() == 0 ? TEXT("") : FString::Printf(TEXT("(%s)"), *FString::Join(ClassAssetPathStrings, TEXT("||")));

	// (Directories && (ClassPaths || SubClasses))
	//              && RootClasses)
	FString PathFilter;
	if (DirectoryFilter.Len() > 0)
	{
		PathFilter = FString::Printf(TEXT("(%s)"), *DirectoryFilter);
	}
	if (ClassPathFilter.Len() > 0)
	{
		if (DirectoryFilter.Len() > 0)
		{
			PathFilter += TEXT(" && ");
		}

		if (bLookingForSubClasses)
		{
			// (ClassPaths || SubClasses)
			PathFilter += FString::Printf(TEXT("(%s || %s)"), *ClassPathFilter, *ParentClassFilter);
		}
		else
		{
			// (RootClasses)
			PathFilter += FString::Printf(TEXT("(%s)"), *ClassPathFilter);
		}
	}
	else
	{
		if (PathFilter.Len() == 0)
		{
			// Root Classes only
			PathFilter = FString::Printf(TEXT("(%s)"), *ParentClassFilter);
		}
		else
		{
			// Directories && Root Classes
			PathFilter += FString::Printf(TEXT("&& %s"), *ParentClassFilter);
		}
	}

	// Pin filter
	TArray<FString> PinCategories;
	for (const FEdGraphPinType& PinType : InActiveSearch->SearchArgs.PinTypes)
	{
		PinCategories.Add(TEXT("PinCategory=") + PinType.PinCategory.ToString());
	}
	const FString PinTypeString = PinCategories.Num() == 0 ? TEXT("") : FString::Printf(TEXT("(%s) &&"), *FString::Join(PinCategories, TEXT("||")));

	// Final search string should either be the user query, or everything ("") if using regex since UE won't process this.
	const FString FinalSearchString = InActiveSearch->SearchArgs.bRegex ? TEXT("\"\"") : InActiveSearch->SearchArgs.SearchString;

	// Blueprint((Path=) && (ParentClass=SMBlueprintGeneratedClass || ParentClass=SMInstance) && Pins((PinCategory=Text) && NodeData=_ && DefaultValue=default))
	// NodeData before DefaultValue, helps with huge queries returning incorrectly formatted results.
	const FString FormattedSearchString = FString::Printf(
	TEXT("Blueprint((%s) && Pins(%s %s=_ && %s=%s))"),
	*PathFilter, *PinTypeString, *FSMSearchTags::FiB_NodeData.ToString(), *FFindInBlueprintSearchTags::FiB_DefaultValue.ToString(), *FinalSearchString);

	LDSEARCH_LOG_INFO(TEXT("Starting FiB search with query: \"%s\""), *FormattedSearchString);

	auto CheckForExactNameInString = [](const FString& InString, const FString& InName) -> bool
	{
		const int32 FoundIndex = InString.Find(InName);
		if (FoundIndex == INDEX_NONE)
		{
			return false;
		}

		// Found, but make sure it's exact.
		const int32 TestIndex = FoundIndex + InName.Len();
		if (TestIndex < InString.Len())
		{
			const TCHAR Character = InString[TestIndex];
			if (FChar::IsAlpha(Character) || FChar::IsAlnum(Character))
			{
				return false;
			}
		}

		return true;
	};

	auto CheckForResults = [&]()
	{
		TArray<TSharedPtr<FFindInBlueprintsResult>> FiBResults;
		InActiveSearch->StreamSearch->GetFilteredItems(FiBResults);

		for (const TSharedPtr<FFindInBlueprintsResult>& FiBResult : FiBResults)
		{
			TArray<TSharedPtr<FFindInBlueprintsResult>> FiBDefaultValues;
			FindDefaultValueResult(FiBResult, FiBDefaultValues);
			for (const TSharedPtr<FFindInBlueprintsResult>& FiBDefaultValue : FiBDefaultValues)
			{
				FString DefaultValueString = FiBDefaultValue->GetDisplayString().ToString().RightChop(DefaultValuePrefix.Len());
				TSharedPtr<FSearchResult> SearchResult = SearchString(DefaultValueString, InActiveSearch);

				if (SearchResult.IsValid())
				{
					// Extract info from Find in Blueprints.
					const TSharedPtr<FSearchResultFiB> FiBPropertyResult = CreateFiBResult(FiBDefaultValue, FiBResult);
					{
						SearchResult->FiBResult = FiBPropertyResult;
						SearchResult->BlueprintPath = FiBResult->GetDisplayString().ToString();
						SearchResult->PropertyValue = MoveTemp(DefaultValueString);
						SearchResult->bAllowConstructionScriptsOnLoad = InActiveSearch->SearchArgs.bAllowConstructionScriptsOnLoad;
					}

					FSoftObjectPath ResultPath(SearchResult->BlueprintPath);
					FString ResultPathString = ResultPath.GetAssetPathString();
					ResultPathString.RemoveFromEnd(TEXT("_C"));

					// Filter class names not found -- The FiB search will have not performed an exact match.
					bool bMatchingClass = InActiveSearch->SearchArgs.StateMachineClasses.Num() == 0;
					for (const TSoftClassPtr<USMInstance>& SubClass : InActiveSearch->SearchArgs.StateMachineClasses)
					{
						// Test the asset class.
						FString SubClassPathString = SubClass.ToSoftObjectPath().GetAssetPathString();
						SubClassPathString.RemoveFromEnd(TEXT("_C"));
						if (SubClassPathString == ResultPathString)
						{
							bMatchingClass = true;
							break;
						}

						// Test parent class if sub classes are allowed.
						if (InActiveSearch->SearchArgs.bIncludeSubClasses)
						{
							// Parent could be invalid if there are no children since the search query wouldn't have included the parent filter.
							if (FiBPropertyResult->Parent.IsValid())
							{
								FString SubClassName = SubClass.GetAssetName();
								const FString ParentDisplayString = FiBPropertyResult->Parent->GetDisplayString().ToString();
								if (CheckForExactNameInString(ParentDisplayString, SubClassName))
								{
									bMatchingClass = true;
									break;
								}
							}
						}
					}

					if (!bMatchingClass)
					{
						continue;
					}

					FString PropertyName = SearchResult->GetPropertyName();
					PropertyName.RemoveSpacesInline();

					// Filter property names not requested.
					if (InActiveSearch->SearchArgs.PropertyNames.Num() > 0 && !InActiveSearch->SearchArgs.PropertyNames.Contains(*PropertyName))
					{
						continue;
					}

					// Some assets may be loaded by now so try to resolve object references.
					SearchResult->TryResolveObjects();
					InActiveSearch->SummaryResult.SearchResults.Add(SearchResult);
				}
			}
		}
	};

	FStreamSearchOptions InSearchOptions;
	InSearchOptions.ImaginaryDataFilter = ESearchQueryFilter::PinsFilter;
	InSearchOptions.MinimiumVersionRequirement = EFiBVersion::FIB_VER_LATEST;
	InActiveSearch->StreamSearch = MakeShared<FStreamSearch>(FormattedSearchString, MoveTemp(InSearchOptions));

	while (!InActiveSearch->StreamSearch->IsComplete())
	{
		if (InActiveSearch->bCancel)
		{
			break;
		}

		if (!FFindInBlueprintSearchManager::Get().IsTickable() || IsInGameThread())
		{
			// In the event the manager is caching pending BPs it may need to tick but needs the global find window
			// open. Manually tick to ensure the process completes.
			FFindInBlueprintSearchManager::Get().Tick(0.f);
		}

		CheckForResults();

		InActiveSearch->SummaryResult.Progress = InActiveSearch->StreamSearch->GetPercentComplete();
		if (InActiveSearch->SummaryResult.Progress != InActiveSearch->LastPercentComplete)
		{
			InActiveSearch->LastPercentComplete = InActiveSearch->SummaryResult.Progress;

			if (!IsInGameThread() && InActiveSearch->OnSearchUpdatedDelegate.IsBound())
			{
				const TWeakPtr<FActiveSearch, ESPMode::ThreadSafe> ActiveSearchWeakPtr(InActiveSearch);

				FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
				FSimpleDelegateGraphTask::FDelegate::CreateLambda([ActiveSearchWeakPtr, this] ()
				{
					if (ActiveSearchWeakPtr.IsValid())
					{
						BroadcastSearchUpdated(ActiveSearchWeakPtr.Pin().ToSharedRef());
					}
				}),
				TStatId(),
				nullptr,
				ENamedThreads::GameThread);

				// Give the stream thread time to do work.
				FPlatformProcess::Sleep(0.01f);
			}
		}
	}

	// Check again since some results may have not been found. Newly added assets like to show up right when the search completes.
	CheckForResults();

	InActiveSearch->SummaryResult.FinishTime = FDateTime::UtcNow();
	InActiveSearch->SummaryResult.bComplete = true;

	if (InActiveSearch->StreamSearch.IsValid())
	{
		InActiveSearch->StreamSearch->EnsureCompletion();
		InActiveSearch->StreamSearch.Reset();
	}
}

void FSMSearch::BroadcastSearchUpdated(const FActiveSearchRef& InActiveSearch)
{
	check(IsInGameThread());
	InActiveSearch->OnSearchUpdatedDelegate.ExecuteIfBound(InActiveSearch->SummaryResult);
}

void FSMSearch::BroadcastSearchComplete(const FActiveSearchRef& InActiveSearch)
{
	check(IsInGameThread());
	InActiveSearch->OnSearchCompletedDelegate.ExecuteIfBound(InActiveSearch->SummaryResult);
	FinishSearch(InActiveSearch);
}

void FSMSearch::BroadcastSearchCanceled(const FActiveSearchRef& InActiveSearch)
{
	check(IsInGameThread());
	InActiveSearch->OnSearchCanceledDelegate.ExecuteIfBound(InActiveSearch->SummaryResult);
	FinishSearch(InActiveSearch);
}

bool FSMSearch::ShutdownIndexer()
{
	check(IsInGameThread());

	if (FFindInBlueprintSearchManager* Instance = FFindInBlueprintSearchManager::Instance)
	{
		Instance->CancelCacheAll(nullptr);

		if (ensure(!Instance->IsCacheInProgress()))
		{
			delete Instance;
			FFindInBlueprintSearchManager::Instance = nullptr;

			// Shutdown successful.
			return true;
		}

		// Couldn't shutdown.
		return false;
	}

	// Already shutdown.
	return true;
}

void FSMSearch::FinishSearch(const FActiveSearchRef& InActiveSearch)
{
	check(IsInGameThread());

	if (InActiveSearch->AsyncTask.IsValid())
	{
		InActiveSearch->AsyncTask->EnsureCompletion();
		InActiveSearch->AsyncTask.Reset();
	}

	ActiveSearches.Remove(InActiveSearch->OnSearchCompletedDelegate.GetHandle());

	InActiveSearch->OnSearchUpdatedDelegate.Unbind();
	InActiveSearch->OnSearchCompletedDelegate.Unbind();
	InActiveSearch->OnSearchCanceledDelegate.Unbind();

	InActiveSearch->StreamSearch.Reset();
}

void FSMSearch::SearchStateMachine(
	USMBlueprint* InBlueprint,
	const FActiveSearchRef& InActiveSearch,
	TArray<TSharedPtr<FSearchResult>>& OutResults) const
{
	check(InBlueprint);

	TArray<USMGraphNode_Base*> GraphNodes;
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested(InBlueprint, GraphNodes);

	for (USMGraphNode_Base* GraphNode : GraphNodes)
	{
		check(GraphNode);

		TArray<USMNodeInstance*> Templates;
		GraphNode->GetAllNodeTemplates(Templates);

		for (USMNodeInstance* Template : Templates)
		{
			if (Template != nullptr)
			{
				TArray<TSharedPtr<FSearchResult>> ObjectResults;
				SearchObject(Template, InActiveSearch, ObjectResults);

				for (const TSharedPtr<FSearchResult>& Result : ObjectResults)
				{
					Result->Blueprint = InBlueprint;
					Result->GraphNode = GraphNode;
				}

				OutResults.Append(MoveTemp(ObjectResults));
			}
		}
	}
}

void FSMSearch::SearchObject(
	UObject* InObject,
	FActiveSearchRef InActiveSearch,
	TArray<TSharedPtr<FSearchResult>>& OutResults) const
{
	check(InObject);
	TArray<FProperty*> PropertiesToSearch;
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	for (TFieldIterator<FProperty> PropertyItr(InObject->GetClass()); PropertyItr; ++PropertyItr)
	{
		FProperty* Property = *PropertyItr;
		if (InActiveSearch->SearchArgs.PropertyNames.Num() > 0 && !InActiveSearch->SearchArgs.PropertyNames.Contains(Property->GetFName()))
		{
			// Filter property names not requested.
			continue;
		}

		const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property);

		// Filter out properties not matching the correct type.
		if (InActiveSearch->SearchArgs.PinTypes.Num() > 0)
		{
			FEdGraphPinType PinType;
			K2Schema->ConvertPropertyToPinType(ArrayProperty ? ArrayProperty->Inner : Property, PinType);
			if (!InActiveSearch->SearchArgs.PinTypes.Contains(PinType))
			{
				continue;
			}
		}

		if (ArrayProperty)
		{
			FScriptArrayHelper Helper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<uint8>(InObject));

			for (int32 Idx = 0; Idx < Helper.Num(); ++Idx)
			{
				TSharedPtr<FSearchResult> Result = SearchProperty(Property, InObject, InActiveSearch, Idx);
				if (Result.IsValid())
				{
					OutResults.Add(MoveTemp(Result));
				}
			}
		}
		else
		{
			TSharedPtr<FSearchResult> Result = SearchProperty(Property, InObject, InActiveSearch);
			if (Result.IsValid())
			{
				OutResults.Add(MoveTemp(Result));
			}
		}
	}
}

TSharedPtr<ISMSearch::FSearchResult> FSMSearch::SearchProperty(
	FProperty* InProperty,
	UObject* InObject,
	const FActiveSearchRef& InActiveSearch,
	int32 InPropertyIndex) const
{
	FString StringValue = LD::PropertyUtils::GetPropertyValue(InProperty, InObject, InPropertyIndex);

	FString Namespace;
	FString Key;

	// Use literal text if possible.
	FText Text;
	if (const TCHAR* Success = FTextStringHelper::ReadFromBuffer(*StringValue, Text))
	{
		Namespace = FTextInspector::GetNamespace(Text).Get(FString());
		Key = FTextInspector::GetKey(Text).Get(FString());
		StringValue = Text.ToString();
	}

	if (!StringValue.IsEmpty())
	{
		TSharedPtr<FSearchResult> StringResult = SearchString(StringValue, InActiveSearch);
		if (StringResult.IsValid())
		{
			StringResult->Property = InProperty;
			StringResult->PropertyIndex = InPropertyIndex;
			StringResult->NodeInstance = Cast<USMNodeInstance>(InObject);
			StringResult->Namespace = Namespace;
			StringResult->Key = Key;
		}

		return MoveTemp(StringResult);
	}

	return nullptr;
}

TSharedPtr<ISMSearch::FSearchResult> FSMSearch::SearchString(
	const FString& InString,
	const FActiveSearchRef& InActiveSearch) const
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SearchString"), STAT_SearchString, STATGROUP_LogicDriverSearch);

	TSharedPtr<FSearchResult> Result;

	int32 LastEndIndex = INDEX_NONE;
	const ESearchCase::Type CaseSensitivity = InActiveSearch->SearchArgs.bCaseSensitive ? ESearchCase::CaseSensitive : ESearchCase::IgnoreCase;

	if (InActiveSearch->RegexPattern.IsValid())
	{
		// Perform a regex search. This isn't the default because it's slower.
		check(InActiveSearch->RegexPattern.IsValid());
		FRegexMatcher RegexMatcher(*InActiveSearch->RegexPattern, InString);

		while (RegexMatcher.FindNext())
		{
			if (!Result.IsValid())
			{
				Result = MakeShared<FSearchResult>();
			}

			Result->MatchedTextRanges.Add(FTextRange(RegexMatcher.GetMatchBeginning(), RegexMatcher.GetMatchEnding()));
		}
	}
	else
	{
		// Perform a standard search for all occurrences of the phrase in the string.
		for (int32 Index = InString.Find(InActiveSearch->SearchArgs.SearchString, CaseSensitivity); Index != INDEX_NONE;
			Index = InString.Find(InActiveSearch->SearchArgs.SearchString, CaseSensitivity, ESearchDir::FromStart, LastEndIndex))
		{
			LastEndIndex = Index + InActiveSearch->SearchArgs.SearchString.Len();

			// Match only full words. Don't use regex so we don't have to worry about escaping the string when regex is disabled.
			if (InActiveSearch->SearchArgs.bFullWord)
			{
				const int32 PrevIndex = Index - 1;
				if (PrevIndex >= 0 && (FChar::IsAlpha(InString[PrevIndex]) || FChar::IsAlnum(InString[PrevIndex])))
				{
					continue;
				}

				if (LastEndIndex < InString.Len() && (FChar::IsAlpha(InString[LastEndIndex]) || FChar::IsAlnum(InString[LastEndIndex])))
				{
					continue;
				}
			}

			if (!Result.IsValid())
			{
				Result = MakeShared<FSearchResult>();
			}

			if (Result->MatchedTextRanges.Num() &&
				Result->MatchedTextRanges.Last().BeginIndex == Index &&
				Result->MatchedTextRanges.Last().EndIndex == LastEndIndex)
			{
				// When searching for a single item at the end index, UE automatically clamps it to len() - 1 which
				// can cause an infinite loop if we don't check for it here.
				break;
			}
			Result->MatchedTextRanges.Add(FTextRange(Index, LastEndIndex));
		}
	}

	if (Result.IsValid())
	{
		Result->PropertyValue = InString;
	}

	return MoveTemp(Result);
}

bool FSMSearch::IsAssetFilteredOut(const FAssetData& InAssetData, const FSearchArgs& InArgs) const
{
	if (InArgs.StateMachineClasses.Num() > 0 && !InArgs.bIncludeSubClasses)
	{
		return !InArgs.StateMachineClasses.ContainsByPredicate([&](const TSoftClassPtr<UObject>& FilterClass)
		{
			FString FilterClassPath = FilterClass.ToSoftObjectPath().GetAssetPathString();
			FilterClassPath.RemoveFromEnd(TEXT("_C"));
			return FilterClassPath == InAssetData.ToSoftObjectPath().GetAssetPathString();
		});
	}

	return false;
}

TSharedPtr<FRegexPattern> FSMSearch::CreateRegexPattern(const FSearchArgs& InArgs) const
{
	if (!InArgs.bRegex)
	{
		return nullptr;
	}

	FString RegexPatternString = InArgs.SearchString;
	if (InArgs.bFullWord)
	{
		RegexPatternString = FString::Printf(TEXT("\\b(%s)\\b"), *RegexPatternString);
	}
	if (!InArgs.bCaseSensitive)
	{
		RegexPatternString = FString::Printf(TEXT("(?i)%s"), *RegexPatternString);
	}

	return MakeShareable(new FRegexPattern(RegexPatternString));
}

bool FSMSearch::FindDefaultValueResult(
	const TSharedPtr<FFindInBlueprintsResult>& InResult,
	TArray<TSharedPtr<FFindInBlueprintsResult>>& OutValueResults) const
{
	if (InResult.IsValid())
	{
		if (InResult->GetDisplayString().ToString().StartsWith(DefaultValuePrefix) && InResult->Children.Num() == 0 &&
			InResult->GetCategory().IsEmpty())
		{
			OutValueResults.Add(InResult);
			return true;
		}

		for (const TSharedPtr<FFindInBlueprintsResult>& NextChild : InResult->Children)
		{
			FindDefaultValueResult(NextChild, OutValueResults);
		}
	}

	return OutValueResults.Num() > 0;
}

TSharedPtr<FFindInBlueprintsResult> FSMSearch::FindParentResult(
	const TSharedPtr<FFindInBlueprintsResult>& InResult) const
{
	static FString ParentValuePrefix = FString::Printf(TEXT("%s: "), *FFindInBlueprintSearchTags::FiB_ParentClass.ToString());
	if (InResult.IsValid())
	{
		const FString& CategoryString = InResult->GetCategory().ToString();
		if (CategoryString.IsEmpty() && InResult->GetDisplayString().ToString().StartsWith(ParentValuePrefix))
		{
			return InResult;
		}

		for (const TSharedPtr<FFindInBlueprintsResult>& NextChild : InResult->Children)
		{
			TSharedPtr<FFindInBlueprintsResult> FoundResult = FindParentResult(NextChild);
			if (FoundResult.IsValid())
			{
				return FoundResult;
			}
		}
	}

	return nullptr;
}

TSharedPtr<FFindInBlueprintsResult> FSMSearch::FindNodeResult(
	const TSharedPtr<FFindInBlueprintsResult>& InDefaultValueResult) const
{
	if (InDefaultValueResult.IsValid())
	{
		const FString& CategoryString = InDefaultValueResult->GetCategory().ToString();
		if (CategoryString == TEXT("Node"))
		{
			return InDefaultValueResult;
		}

		return FindNodeResult(InDefaultValueResult->Parent.Pin());
	}

	return nullptr;
}

TSharedPtr<ISMSearch::FSearchResultFiB> FSMSearch::CreateFiBResult(
	const TSharedPtr<FFindInBlueprintsResult>& InDefaultValueResult,
	const TSharedPtr<FFindInBlueprintsResult>& TopMostResult) const
{
	TSharedPtr<FSearchResultFiB> ReturnValue = MakeShared<FSearchResultFiB>();
	ReturnValue->Blueprint = InDefaultValueResult;
	ReturnValue->Parent = FindParentResult(TopMostResult);

	const TSharedPtr<FFindInBlueprintsResult> GraphNodeResult = FindNodeResult(InDefaultValueResult);
	if (GraphNodeResult.IsValid())
	{
		ReturnValue->GraphNode = GraphNodeResult;
		ReturnValue->Graph = GraphNodeResult->Parent.Pin();
		check(GraphNodeResult->Children.Num() > 0)
		ensure(GraphNodeResult->Children.Num() == 1);
		ReturnValue->GraphPin = GraphNodeResult->Children[0];

		for (int32 PinChildrenIdx = 0; PinChildrenIdx < ReturnValue->GraphPin->Children.Num(); ++PinChildrenIdx)
		{
			const TSharedPtr<FFindInBlueprintsResult>& Child = ReturnValue->GraphPin->Children[PinChildrenIdx];
			FString DisplayString = Child->GetDisplayString().ToString();
			if (DisplayString.StartsWith(NodeDataPrefix))
			{
				auto ExtractFromString = [&] (const FString& InString, const FString& InPrefix) -> FString
				{
					FString ResultString;
					int32 StartIdx = InString.Find(InPrefix);
					if (StartIdx != INDEX_NONE)
					{
						StartIdx += InPrefix.Len();
						for (int32 Idx = StartIdx; Idx < InString.Len(); ++Idx)
						{
							const TCHAR Character = InString[Idx];
							if (Character == '}' || !ensureMsgf(Character != '{', TEXT("Unexpected character detected, this shouldn't be valid in index data.")))
							{
								break;
							}

							ResultString += Character;
						}
					}

					return MoveTemp(ResultString);
				};

				DisplayString = DisplayString.RightChop(NodeDataPrefix.Len());
				// Now it will be _Name:{%s}_NodeGuid:{%s}_PropGuid:{%s}

				const FString NamePrefix = TEXT("Name:{");
				const FString NodeGuidPrefix = TEXT("_NodeGuid:{");
				const FString PropertyNamePrefix = TEXT("_PropName:{");
				const FString PropertyGuidPrefix = TEXT("_PropGuid:{");
				const FString ArrayPrefix = TEXT("_Arr:{");

				const FString NameString = ExtractFromString(DisplayString, NamePrefix);
				const FString NodeGuidString = ExtractFromString(DisplayString, NodeGuidPrefix);
				const FString PropertyNameString = ExtractFromString(DisplayString, PropertyNamePrefix);
				const FString PropertyGuidString = ExtractFromString(DisplayString, PropertyGuidPrefix);
				const FString ArrayIndexString = ExtractFromString(DisplayString, ArrayPrefix);

				ReturnValue->NodeName = NameString;
				ReturnValue->PropertyName = PropertyNameString;
				FGuid::Parse(NodeGuidString, ReturnValue->NodeGuid);
				FGuid::Parse(PropertyGuidString, ReturnValue->PropertyGuid);
				ReturnValue->ArrayIndex = FCString::Atoi(*ArrayIndexString);

				break;
			}
		}
	}

	return MoveTemp(ReturnValue);
}

#undef LOCTEXT_NAMESPACE
