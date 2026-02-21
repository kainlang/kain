// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMAssetTestInstance.h"
#include "SMTestHelpers.h"

#include "Blueprints/SMBlueprint.h"

#include "Blueprints/SMBlueprintGeneratedClass.h"
#include "Construction/ISMEditorConstructionManager.h"
#include "Graph/Nodes/SMGraphNode_StateNode.h"
#include "Utilities/SMPropertyUtils.h"

#include "ISMAssetManager.h"
#include "ISMAssetToolsModule.h"
#include "ISMGraphGeneration.h"
#include "ISMSearch.h"
#include "ISMSearchModule.h"

#include "Kismet2/KismetEditorUtilities.h"

#if WITH_DEV_AUTOMATION_TESTS

#if PLATFORM_DESKTOP

struct FLatentSearchHelper
{
	TArray<FAssetHandler> ReferencedAssets;
	FAutomationTestBase* Test = nullptr;
	int32 Iterations = 0;
	bool bCallbackCompleted = false;

	FLatentSearchHelper()
	{
	}

	~FLatentSearchHelper()
	{
		Cleanup();
	}

	void Cleanup()
	{
	}
};

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FAsyncSearchCommand, TSharedPtr<FLatentSearchHelper>, Payload);

bool FAsyncSearchCommand::Update()
{
	if (!Payload.IsValid())
	{
		return false;
	}

	if (Payload->bCallbackCompleted)
	{
		return true;
	}

	constexpr int32 MaxIterations = 1000;
	if (Payload->Iterations++ >= MaxIterations)
	{
		Payload->Test->TestTrue("Async search timed out", false);
		return true;
	}

	return false;
}

void ValidateSearchResult(FAutomationTestBase* Test, const ISMSearch::FSearchResult& Result,
	const FString& SearchText, UBlueprint* InBP, const FName& InPropertyName, bool bIsRegex = false)
{
	if (InBP != nullptr)
	{
		Test->TestEqual("BP found", Result.Blueprint.Get(), InBP);
	}

	Test->TestEqual("Property matched", Result.Property->GetFName(), InPropertyName);
	if (!bIsRegex)
	{
		Test->TestTrue("String matched", Result.PropertyValue.Contains(SearchText));
	}

	Test->TestTrue("Text ranges set", Result.MatchedTextRanges.Num() > 0);

	for (const FTextRange& Range : Result.MatchedTextRanges)
	{
		check(Test->TestTrue("Index in string", Result.PropertyValue.IsValidIndex(Range.BeginIndex)));
		check(Test->TestTrue("Index in string", Result.PropertyValue.IsValidIndex(Range.EndIndex)));
		if (!bIsRegex)
		{
			const int32 StartIndex = Result.PropertyValue.Find(SearchText, ESearchCase::IgnoreCase, ESearchDir::FromStart, Range.BeginIndex);
			Test->TestEqual("Index found", StartIndex, Range.BeginIndex);
			Test->TestEqual("Index length correct", StartIndex + SearchText.Len(), Range.EndIndex);
		}
	}
}

void ValidateReplaceResult(FAutomationTestBase* Test, const ISMSearch::FSearchResult& SearchResult)
{
	check(Test->TestTrue("Replace result is valid", SearchResult.ReplaceResult.IsValid()));
	Test->TestTrue("No errors", SearchResult.ReplaceResult->ErrorMessage.IsEmpty());
	Test->TestTrue("Construction scripts enabled on load", ISMEditorConstructionManager::Get().AreConstructionScriptsAllowedOnLoad());
}

void SetText(FAutomationTestBase* Test, USMGraphNode_Base* InGraphNode, const FName& InPropertyName, const FString& InString, int32 Index = 0)
{
	const ISMAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<ISMAssetToolsModule>(
	LOGICDRIVER_ASSET_TOOLS_MODULE_NAME);

	ISMGraphGeneration::FSetNodePropertyArgs PropertyArgs;
	PropertyArgs.PropertyName = InPropertyName;
	PropertyArgs.PropertyDefaultValue = InString;
	PropertyArgs.PropertyIndex = Index;
	AssetToolsModule.GetGraphGenerationInterface()->SetNodePropertyValue(InGraphNode, PropertyArgs);

	USMAssetTestPropertyStateInstance* NodeInstance = InGraphNode->GetNodeTemplateAs<USMAssetTestPropertyStateInstance>(true);
	FProperty* Property = NodeInstance->GetClass()->FindPropertyByName(InPropertyName);
	const FString PropertyValue = LD::PropertyUtils::GetPropertyValue(Property, NodeInstance, Index);

	Test->TestEqual("Property value set",
			  PropertyValue,
			  PropertyArgs.PropertyDefaultValue);

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(InGraphNode);

	FKismetEditorUtilities::CompileBlueprint(Blueprint);
}

FString GetText(const USMGraphNode_Base* InGraphNode, const FName& InPropertyName, int32 Index = 0)
{
	const ISMAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<ISMAssetToolsModule>(
	LOGICDRIVER_ASSET_TOOLS_MODULE_NAME);

	USMAssetTestPropertyStateInstance* NodeInstance = InGraphNode->GetNodeTemplateAs<USMAssetTestPropertyStateInstance>(true);
	FProperty* Property = NodeInstance->GetClass()->FindPropertyByName(InPropertyName);
	return LD::PropertyUtils::GetPropertyValue(Property, NodeInstance, Index);
}

USMBlueprint* CreateTextAsset(const FName& InAssetName, FAutomationTestBase* Test)
{
	const ISMAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<ISMAssetToolsModule>(
		LOGICDRIVER_ASSET_TOOLS_MODULE_NAME);
	USMBlueprint* NewBP = nullptr;
	{
		ISMAssetManager::FCreateStateMachineBlueprintArgs Args;
		Args.Name = InAssetName;
		Args.Path = FAssetHandler::DefaultGamePath();
		NewBP = AssetToolsModule.GetAssetManagerInterface()->CreateStateMachineBlueprint(Args);
		FKismetEditorUtilities::CompileBlueprint(NewBP);
	}

	{
		const ISMSearchModule& SearchModule = FModuleManager::LoadModuleChecked<ISMSearchModule>(\
			LOGICDRIVER_SEARCH_MODULE_NAME);

		SearchModule.GetSearchInterface()->EnableDeferredIndexing(false);

		ISMSearch::FIndexingStatus IndexStatus;
		SearchModule.GetSearchInterface()->GetIndexingStatus(IndexStatus);
		Test->TestFalse("Index status set", IndexStatus.bDeferredIndexingEnabledInLogicDriver.Get(true));
	}

	return NewBP;
}

USMGraphNode_StateNode* CreateInitialState(FAutomationTestBase* Test, USMBlueprint* InBlueprint)
{
	const ISMAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<ISMAssetToolsModule>(
		LOGICDRIVER_ASSET_TOOLS_MODULE_NAME);
	USMGraphNode_StateNode* InitialRootState = nullptr;
	{
		ISMGraphGeneration::FCreateStateNodeArgs CreateStateNodeArgs;
		CreateStateNodeArgs.StateName = "TextNode";
		CreateStateNodeArgs.StateInstanceClass = USMAssetTestPropertyStateInstance::StaticClass();
		CreateStateNodeArgs.bIsEntryState = true;
		InitialRootState = Cast<USMGraphNode_StateNode>(
			AssetToolsModule.GetGraphGenerationInterface()->CreateStateNode(InBlueprint, CreateStateNodeArgs));
		Test->TestNotNull("State node created", InitialRootState);
	}
	return InitialRootState;
}

#define SETUP_TEXT_SEARCH()\
	const ISMSearchModule& SearchModule = FModuleManager::LoadModuleChecked<ISMSearchModule>(\
		LOGICDRIVER_SEARCH_MODULE_NAME);\
	USMBlueprint* NewBP = CreateTextAsset(*FGuid::NewGuid().ToString(), Test);\
	FAssetHandler NewAsset = TestHelpers::CreateAssetFromBlueprint(NewBP);\
	USMGraphNode_StateNode* InitialRootState = CreateInitialState(Test, NewBP);\
	const FString UniqueText = FGuid::NewGuid().ToString();

bool TestEmpty(FAutomationTestBase* Test, const FName& InPropertyName)
{
	SETUP_TEXT_SEARCH();

	SetText(Test, InitialRootState, InPropertyName,
		FString::Printf(TEXT("Search for %s! It's a guid so only one asset should be found."), *UniqueText));

	{
		ISMSearch::FSearchArgs Args;
		Args.SearchString = "";

		const TSharedPtr<FLatentSearchHelper> Payload = MakeShared<FLatentSearchHelper>();
		Payload->Test = Test;
		Payload->ReferencedAssets.Add(NewAsset);

		ADD_LATENT_AUTOMATION_COMMAND(FAsyncSearchCommand(Payload));

		SearchModule.GetSearchInterface()->SearchAsync(Args, ISMSearch::FOnSearchCompleted::CreateLambda([=](const ISMSearch::FSearchSummary& Summary)
		{
			Test->TestEqual("No results for empty string", Summary.SearchResults.Num(), 0);

			Payload->bCallbackCompleted = true;
		}));
	}

	return true;
}

/**
 * Search for empty text.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssetToolsSearchEmptyTest, "LogicDriver.AssetTools.Search.Text.Empty",
                                 EAutomationTestFlags::ApplicationContextMask |
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
                                 EAutomationTestFlags::EngineFilter)

bool FAssetToolsSearchEmptyTest::RunTest(const FString& Parameters)
{
	const FName PropertyName = GET_MEMBER_NAME_CHECKED(USMAssetTestPropertyStateInstance, ExposedText);
	return TestEmpty(this, PropertyName);
}

/**
 * Search for empty text graph text.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssetToolsTextGraphSearchEmptyTest, "LogicDriver.AssetTools.Search.TextGraph.Empty",
								 EAutomationTestFlags::ApplicationContextMask |
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
								 EAutomationTestFlags::EngineFilter)

bool FAssetToolsTextGraphSearchEmptyTest::RunTest(const FString& Parameters)
{
	const FName PropertyName = GET_MEMBER_NAME_CHECKED(USMAssetTestPropertyStateInstance, TextGraph);
	return TestEmpty(this, PropertyName);
}

bool TestSingleResult(FAutomationTestBase* Test, const FName& InPropertyName)
{
	SETUP_TEXT_SEARCH();

	SetText(Test, InitialRootState, InPropertyName, FString::Printf(TEXT("Search for %s! It's a guid so only one asset should be found."), *UniqueText));

	{
		ISMSearch::FSearchArgs SearchArgs;
		SearchArgs.SearchString = UniqueText;
		SearchArgs.StateMachineClasses.Add(NewBP->GetGeneratedClass());

		const TSharedPtr<FLatentSearchHelper> Payload = MakeShared<FLatentSearchHelper>();
		Payload->Test = Test;
		Payload->ReferencedAssets.Add(NewAsset);

		ADD_LATENT_AUTOMATION_COMMAND(FAsyncSearchCommand(Payload));

		SearchModule.GetSearchInterface()->SearchAsync(SearchArgs, ISMSearch::FOnSearchCompleted::CreateLambda([=](const ISMSearch::FSearchSummary& Summary)
		{
			check(Test->TestEqual("1 result found", Summary.SearchResults.Num(), 1));
			ValidateSearchResult(Test, *Summary.SearchResults[0], SearchArgs.SearchString, NewBP, InPropertyName);

			// Replacement
			ISMSearch::FReplaceArgs ReplaceArgs;
			ReplaceArgs.ReplaceString = FGuid::NewGuid().ToString();
			ReplaceArgs.SearchResults = Summary.SearchResults;
			ISMSearch::FReplaceSummary ReplaceSummary =
				FModuleManager::LoadModuleChecked<ISMSearchModule>(LOGICDRIVER_SEARCH_MODULE_NAME).GetSearchInterface()->ReplacePropertyValues(ReplaceArgs, SearchArgs);

			check(ReplaceSummary.SearchResults.Num() == 1);
			ValidateReplaceResult(Test, *ReplaceSummary.SearchResults[0].Get());

			Payload->bCallbackCompleted = true;
		}));
	}

	return true;
}

/**
 * Search for text with a single expected result.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssetToolsSearchSingleResultTest, "LogicDriver.AssetTools.Search.Text.SingleResult",
                                 EAutomationTestFlags::ApplicationContextMask |
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
                                 EAutomationTestFlags::EngineFilter)

bool FAssetToolsSearchSingleResultTest::RunTest(const FString& Parameters)
{
	const FName PropertyName = GET_MEMBER_NAME_CHECKED(USMAssetTestPropertyStateInstance, ExposedText);
	return TestSingleResult(this, PropertyName);
}

/**
 * Search for text graph text with a single expected result.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssetToolsTextGraphSearchSingleResultTest, "LogicDriver.AssetTools.Search.TextGraph.SingleResult",
								 EAutomationTestFlags::ApplicationContextMask |
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
								 EAutomationTestFlags::EngineFilter)

bool FAssetToolsTextGraphSearchSingleResultTest::RunTest(const FString& Parameters)
{
	const FName PropertyName = GET_MEMBER_NAME_CHECKED(USMAssetTestPropertyStateInstance, TextGraph);
	return TestSingleResult(this, PropertyName);
}

bool TestMultipleResults(FAutomationTestBase* Test, const FName& InPropertyName, bool bLimitToOneAsset)
{
	SETUP_TEXT_SEARCH();

	USMBlueprint* AnotherBP = CreateTextAsset(*FGuid::NewGuid().ToString(), Test);
	USMGraphNode_StateNode* AnotherInitialState = CreateInitialState(Test, AnotherBP);

	// First BP
	SetText(Test, InitialRootState, InPropertyName,
		FString::Printf(TEXT("Search for %s! This time we have two occurrences! %s! So two results should be found."), *UniqueText, *UniqueText));
	// Second BP
	SetText(Test, AnotherInitialState, InPropertyName,
		FString::Printf(TEXT("Search for %s! This time we have two occurrences! %s! So two results should be found."), *UniqueText, *UniqueText));

	// Search word(2)
	{
		ISMSearch::FSearchArgs Args;
		Args.SearchString = UniqueText;

		// Search across all assets.
		if (!bLimitToOneAsset)
		{
			const TSharedPtr<FLatentSearchHelper> Payload = MakeShared<FLatentSearchHelper>();
			Payload->Test = Test;
			Payload->ReferencedAssets.Add(NewAsset);

			ADD_LATENT_AUTOMATION_COMMAND(FAsyncSearchCommand(Payload));

			SearchModule.GetSearchInterface()->SearchAsync(Args, ISMSearch::FOnSearchCompleted::CreateLambda([=](const ISMSearch::FSearchSummary& Summary)
			{
				check(Test->TestEqual("2 results found", Summary.SearchResults.Num(), 2));
				TMap<UBlueprint*, int32> BlueprintCount;
				for (const TSharedPtr<ISMSearch::FSearchResult>& Result : Summary.SearchResults)
				{
					Test->TestNotNull("BP set", Result->Blueprint.Get());
					BlueprintCount.FindOrAdd(Result->Blueprint.Get())++;
					ValidateSearchResult(Test, *Result, Args.SearchString, nullptr, InPropertyName);
				}

				Test->TestEqual("BPs found", BlueprintCount.Num(), 2);
				for (const TTuple<UBlueprint*, int32>& KeyValCount : BlueprintCount)
				{
					Test->TestEqual("Correct BPs set", KeyValCount.Value, 1);
				}

				Payload->bCallbackCompleted = true;
			}));
		}
		// Same search, but limit back to 1 asset.
		else if (bLimitToOneAsset)
		{
			const TSharedPtr<FLatentSearchHelper> Payload = MakeShared<FLatentSearchHelper>();
			Payload->Test = Test;
			Payload->ReferencedAssets.Add(NewAsset);

			ADD_LATENT_AUTOMATION_COMMAND(FAsyncSearchCommand(Payload));

			Args.PackagePaths.Add(*NewBP->GetPathName());

			SearchModule.GetSearchInterface()->SearchAsync(Args, ISMSearch::FOnSearchCompleted::CreateLambda([=](const ISMSearch::FSearchSummary& Summary)
			{
				check(Test->TestEqual("1 result found", Summary.SearchResults.Num(), 1));
				for (const TSharedPtr<ISMSearch::FSearchResult>& Result : Summary.SearchResults)
				{
					ValidateSearchResult(Test, *Result, Args.SearchString, NewBP, InPropertyName);
				}

				Payload->bCallbackCompleted = true;
			}));
		}
	}

	return true;
}

/**
 * Search for text with multiple expected results.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssetToolsSearchMultipleResultsMultiAssetsTest, "LogicDriver.AssetTools.Search.Text.MultipleResults.MultipleAssets",
                                 EAutomationTestFlags::ApplicationContextMask |
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
                                 EAutomationTestFlags::EngineFilter)

bool FAssetToolsSearchMultipleResultsMultiAssetsTest::RunTest(const FString& Parameters)
{
	const FName PropertyName = GET_MEMBER_NAME_CHECKED(USMAssetTestPropertyStateInstance, ExposedText);
	return TestMultipleResults(this, PropertyName, false);
}

/**
 * Search for text with multiple expected results.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssetToolsSearchMultipleResultsSingleAssetTest, "LogicDriver.AssetTools.Search.Text.MultipleResults.SingleAsset",
								 EAutomationTestFlags::ApplicationContextMask |
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
								 EAutomationTestFlags::EngineFilter)

bool FAssetToolsSearchMultipleResultsSingleAssetTest::RunTest(const FString& Parameters)
{
	const FName PropertyName = GET_MEMBER_NAME_CHECKED(USMAssetTestPropertyStateInstance, ExposedText);
	return TestMultipleResults(this, PropertyName, true);
}

/**
 * Search for text graph with multiple expected results across multiple assets.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssetToolsTextGraphSearchMultipleResultsMultiAssetsTest, "LogicDriver.AssetTools.Search.TextGraph.MultipleResults.MultipleAssets",
								 EAutomationTestFlags::ApplicationContextMask |
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
								 EAutomationTestFlags::EngineFilter)

bool FAssetToolsTextGraphSearchMultipleResultsMultiAssetsTest::RunTest(const FString& Parameters)
{
	const FName PropertyName = GET_MEMBER_NAME_CHECKED(USMAssetTestPropertyStateInstance, TextGraph);
	return TestMultipleResults(this, PropertyName, false);
}

/**
 * Search for text graph with multiple expected results over a single asset.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssetToolsTextGraphSearchMultipleResultsSingleAssetTest, "LogicDriver.AssetTools.Search.TextGraph.MultipleResults.SingleAsset",
								 EAutomationTestFlags::ApplicationContextMask |
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
								 EAutomationTestFlags::EngineFilter)

bool FAssetToolsTextGraphSearchMultipleResultsSingleAssetTest::RunTest(const FString& Parameters)
{
	const FName PropertyName = GET_MEMBER_NAME_CHECKED(USMAssetTestPropertyStateInstance, TextGraph);
	return TestMultipleResults(this, PropertyName, true);
}

bool TestMultipleResultsWithArrays(FAutomationTestBase* Test, const FName& InPropertyName)
{
	SETUP_TEXT_SEARCH();

	const int32 NumIndices = 3;
	// First BP
	for (int32 Idx = 0; Idx < NumIndices; ++Idx)
	{
		SetText(Test, InitialRootState, InPropertyName,
			FString::Printf(TEXT("Search for %s! Each line should return a result."), *UniqueText), Idx);
	}

	// Search word(2)
	{
		ISMSearch::FSearchArgs Args;
		Args.SearchString = UniqueText;

		// Search within 1 asset.
		{
			Args.PackagePaths.Add(*NewBP->GetPathName());

			const TSharedPtr<FLatentSearchHelper> Payload = MakeShared<FLatentSearchHelper>();
			Payload->Test = Test;
			Payload->ReferencedAssets.Add(NewAsset);

			ADD_LATENT_AUTOMATION_COMMAND(FAsyncSearchCommand(Payload));

			SearchModule.GetSearchInterface()->SearchAsync(Args, ISMSearch::FOnSearchCompleted::CreateLambda([=](const ISMSearch::FSearchSummary& Summary)
			{
				check(Test->TestEqual("Multiple results found", Summary.SearchResults.Num(), NumIndices));
				for (const TSharedPtr<ISMSearch::FSearchResult>& Result : Summary.SearchResults)
				{
					ValidateSearchResult(Test, *Result, Args.SearchString, NewBP, InPropertyName);
				}

				Payload->bCallbackCompleted = true;
			}));
		}
	}

	return true;
}

/**
 * Search for text arrays with multiple expected results.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssetToolsSearchMultipleResultsArrayTest, "LogicDriver.AssetTools.Search.Text.ArrayMultipleResults",
								 EAutomationTestFlags::ApplicationContextMask |
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
								 EAutomationTestFlags::EngineFilter)

bool FAssetToolsSearchMultipleResultsArrayTest::RunTest(const FString& Parameters)
{
	const FName PropertyName = GET_MEMBER_NAME_CHECKED(USMAssetTestPropertyStateInstance, ExposedTextArray);
	return TestMultipleResultsWithArrays(this, PropertyName);
}

/**
 * Search for text graph arrays with multiple expected results.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssetToolsTextGraphSearchMultipleResultsArrayTest, "LogicDriver.AssetTools.Search.TextGraph.ArrayMultipleResults",
								 EAutomationTestFlags::ApplicationContextMask |
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
								 EAutomationTestFlags::EngineFilter)

bool FAssetToolsTextGraphSearchMultipleResultsArrayTest::RunTest(const FString& Parameters)
{
	const FName PropertyName = GET_MEMBER_NAME_CHECKED(USMAssetTestPropertyStateInstance, TextGraphArray);
	return TestMultipleResultsWithArrays(this, PropertyName);
}

bool TestCaseSensitive(FAutomationTestBase* Test, const FName& InPropertyName, bool bTestNoMatch)
{
	SETUP_TEXT_SEARCH();

	SetText(Test, InitialRootState, InPropertyName, FString::Printf(TEXT("Search for A%s! It's a guid so only one asset should be found."), *UniqueText));

	{
		ISMSearch::FSearchArgs SearchArgs;

		// No match
		if (bTestNoMatch)
		{
			SearchArgs.SearchString = FString::Printf(TEXT("a%s"), *UniqueText);
			SearchArgs.PackagePaths.Add(*NewBP->GetPathName());
			SearchArgs.bCaseSensitive = true;

			const TSharedPtr<FLatentSearchHelper> Payload = MakeShared<FLatentSearchHelper>();
			Payload->Test = Test;
			Payload->ReferencedAssets.Add(NewAsset);

			ADD_LATENT_AUTOMATION_COMMAND(FAsyncSearchCommand(Payload));

			SearchModule.GetSearchInterface()->SearchAsync(SearchArgs, ISMSearch::FOnSearchCompleted::CreateLambda([=](const ISMSearch::FSearchSummary& Summary)
			{
				check(Test->TestEqual("0 results found", Summary.SearchResults.Num(), 0));

				Payload->bCallbackCompleted = true;
			}));
		}
		// Match
		else if (!bTestNoMatch)
		{
			SearchArgs.SearchString = FString::Printf(TEXT("A%s"), *UniqueText);

			const TSharedPtr<FLatentSearchHelper> Payload = MakeShared<FLatentSearchHelper>();
			Payload->Test = Test;
			Payload->ReferencedAssets.Add(NewAsset);

			ADD_LATENT_AUTOMATION_COMMAND(FAsyncSearchCommand(Payload));

			SearchModule.GetSearchInterface()->SearchAsync(SearchArgs, ISMSearch::FOnSearchCompleted::CreateLambda([=](const ISMSearch::FSearchSummary& Summary)
			{
				check(Test->TestEqual("1 result found", Summary.SearchResults.Num(), 1));
				ValidateSearchResult(Test, *Summary.SearchResults[0], SearchArgs.SearchString, NewBP, InPropertyName);

				// Replacement
				{
					ISMSearch::FReplaceArgs ReplaceArgs;
					ReplaceArgs.ReplaceString = FGuid::NewGuid().ToString();
					ReplaceArgs.SearchResults = Summary.SearchResults;
					ISMSearch::FReplaceSummary ReplaceSummary =
						FModuleManager::LoadModuleChecked<ISMSearchModule>(LOGICDRIVER_SEARCH_MODULE_NAME).GetSearchInterface()->ReplacePropertyValues(ReplaceArgs, SearchArgs);

					check(ReplaceSummary.SearchResults.Num() == 1);
					ValidateReplaceResult(Test, *ReplaceSummary.SearchResults[0].Get());

					Payload->bCallbackCompleted = true;
				}
			}));
		}
	}

	return true;
}

/**
 * Search with case sensitive with no matches found.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssetToolsTextGraphSearchTestCaseSensitiveNoMatchTest, "LogicDriver.AssetTools.Search.CaseSensitive.NoMatch",
								 EAutomationTestFlags::ApplicationContextMask |
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
								 EAutomationTestFlags::EngineFilter)

bool FAssetToolsTextGraphSearchTestCaseSensitiveNoMatchTest::RunTest(const FString& Parameters)
{
	const FName PropertyName = GET_MEMBER_NAME_CHECKED(USMAssetTestPropertyStateInstance, TextGraph);
	return TestCaseSensitive(this, PropertyName, true);
}

/**
 * Search with case sensitive with matches found.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssetToolsTextGraphSearchTestCaseSensitiveWithMatchTest, "LogicDriver.AssetTools.Search.CaseSensitive.Match",
								 EAutomationTestFlags::ApplicationContextMask |
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
								 EAutomationTestFlags::EngineFilter)

bool FAssetToolsTextGraphSearchTestCaseSensitiveWithMatchTest::RunTest(const FString& Parameters)
{
	const FName PropertyName = GET_MEMBER_NAME_CHECKED(USMAssetTestPropertyStateInstance, TextGraph);
	return TestCaseSensitive(this, PropertyName, false);
}

bool TestFullWord(FAutomationTestBase* Test, const FName& InPropertyName, bool bTestNoMatch)
{
	SETUP_TEXT_SEARCH();

	SetText(Test, InitialRootState, InPropertyName, FString::Printf(TEXT("Search for %s! It's a guid so only one asset should be found."), *UniqueText));

	{
		ISMSearch::FSearchArgs SearchArgs;

		// No match
		if (bTestNoMatch)
		{
			const FString UniqueSubset = UniqueText.LeftChop(UniqueText.Len() / 2);
			SearchArgs.SearchString = FString::Printf(TEXT("%s"), *UniqueSubset);
			SearchArgs.PackagePaths.Add(*NewBP->GetPathName());
			SearchArgs.bFullWord = true;

			const TSharedPtr<FLatentSearchHelper> Payload = MakeShared<FLatentSearchHelper>();
			Payload->Test = Test;
			Payload->ReferencedAssets.Add(NewAsset);

			ADD_LATENT_AUTOMATION_COMMAND(FAsyncSearchCommand(Payload));

			SearchModule.GetSearchInterface()->SearchAsync(SearchArgs, ISMSearch::FOnSearchCompleted::CreateLambda([=](const ISMSearch::FSearchSummary& Summary)
			{
				check(Test->TestEqual("0 results found", Summary.SearchResults.Num(), 0));

				Payload->bCallbackCompleted = true;
			}));
		}
		// Match
		else if (!bTestNoMatch)
		{
			SearchArgs.SearchString = FString::Printf(TEXT("%s"), *UniqueText);

			const TSharedPtr<FLatentSearchHelper> Payload = MakeShared<FLatentSearchHelper>();
			Payload->Test = Test;
			Payload->ReferencedAssets.Add(NewAsset);

			ADD_LATENT_AUTOMATION_COMMAND(FAsyncSearchCommand(Payload));

			SearchModule.GetSearchInterface()->SearchAsync(SearchArgs, ISMSearch::FOnSearchCompleted::CreateLambda([=](const ISMSearch::FSearchSummary& Summary)
			{
				check(Test->TestEqual("1 result found", Summary.SearchResults.Num(), 1));
				ValidateSearchResult(Test, *Summary.SearchResults[0], SearchArgs.SearchString, NewBP, InPropertyName);

				// Replacement
				{
					ISMSearch::FReplaceArgs ReplaceArgs;
					ReplaceArgs.ReplaceString = FGuid::NewGuid().ToString();
					ReplaceArgs.SearchResults = Summary.SearchResults;
					ISMSearch::FReplaceSummary ReplaceSummary =
						FModuleManager::LoadModuleChecked<ISMSearchModule>(LOGICDRIVER_SEARCH_MODULE_NAME).GetSearchInterface()->ReplacePropertyValues(ReplaceArgs, SearchArgs);

					check(ReplaceSummary.SearchResults.Num() == 1);
					ValidateReplaceResult(Test, *ReplaceSummary.SearchResults[0].Get());
				}

				Payload->bCallbackCompleted = true;
			}));
		}
	}

	return true;
}

/**
 * Search for full word with no matches found.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssetToolsTextGraphSearchTestFullWordNoMatchTest, "LogicDriver.AssetTools.Search.FullWord.NoMatch",
								 EAutomationTestFlags::ApplicationContextMask |
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
								 EAutomationTestFlags::EngineFilter)

bool FAssetToolsTextGraphSearchTestFullWordNoMatchTest::RunTest(const FString& Parameters)
{
	const FName PropertyName = GET_MEMBER_NAME_CHECKED(USMAssetTestPropertyStateInstance, TextGraph);
	return TestFullWord(this, PropertyName, true);
}

/**
 * Search for full word with matches found.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssetToolsTextGraphSearchTestFullWordWithMatchTest, "LogicDriver.AssetTools.Search.FullWord.Match",
								 EAutomationTestFlags::ApplicationContextMask |
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
								 EAutomationTestFlags::EngineFilter)

bool FAssetToolsTextGraphSearchTestFullWordWithMatchTest::RunTest(const FString& Parameters)
{
	const FName PropertyName = GET_MEMBER_NAME_CHECKED(USMAssetTestPropertyStateInstance, TextGraph);
	return TestFullWord(this, PropertyName, false);
}


bool TestRegex(FAutomationTestBase* Test, const FName& InPropertyName)
{
	SETUP_TEXT_SEARCH();

	SetText(Test, InitialRootState, InPropertyName, FString::Printf(TEXT("Search for the unique %s guid! It's a guid %s so only one asset %s should be found. %s, has a different match and shouldn't be replaced!"),
		*UniqueText, *UniqueText, *UniqueText, *UniqueText));

	{
		ISMSearch::FSearchArgs SearchArgs;

		// Match
		{
			SearchArgs.SearchString = FString::Printf(TEXT("\\w+ %s \\w+"), *UniqueText);
			SearchArgs.PackagePaths.Add(*NewBP->GetPathName());
			SearchArgs.bRegex = true;

			const TSharedPtr<FLatentSearchHelper> Payload = MakeShared<FLatentSearchHelper>();
			Payload->Test = Test;
			Payload->ReferencedAssets.Add(NewAsset);

			ADD_LATENT_AUTOMATION_COMMAND(FAsyncSearchCommand(Payload));

			SearchModule.GetSearchInterface()->SearchAsync(SearchArgs, ISMSearch::FOnSearchCompleted::CreateLambda([=](const ISMSearch::FSearchSummary& Summary)
			{
				check(Test->TestEqual("1 result found", Summary.SearchResults.Num(), 1));
				ValidateSearchResult(Test, *Summary.SearchResults[0], SearchArgs.SearchString, NewBP, InPropertyName, true);

				// Replacement
				{
					ISMSearch::FReplaceArgs ReplaceArgs;
					ReplaceArgs.ReplaceString = FGuid::NewGuid().ToString();
					ReplaceArgs.SearchResults = Summary.SearchResults;
					ISMSearch::FReplaceSummary ReplaceSummary =
						FModuleManager::LoadModuleChecked<ISMSearchModule>(LOGICDRIVER_SEARCH_MODULE_NAME).GetSearchInterface()->ReplacePropertyValues(ReplaceArgs, SearchArgs);

					check(ReplaceSummary.SearchResults.Num() == 1);
					ValidateReplaceResult(Test, *ReplaceSummary.SearchResults[0].Get());

					const FString PropertyValue = GetText(InitialRootState, InPropertyName);

					const FString ExpectedValue = FString::Printf(TEXT("Search for the %s! It's a %s only one %s be found. %s, has a different match and shouldn't be replaced!"),
					*ReplaceArgs.ReplaceString, *ReplaceArgs.ReplaceString, *ReplaceArgs.ReplaceString, *UniqueText);

					Test->TestEqual("Replacement value matches expected value", PropertyValue, ExpectedValue);
				}

				Payload->bCallbackCompleted = true;
			}));
		}
	}

	return true;
}

/**
 * Search regex.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssetToolsTextGraphSearchTestRegexTest, "LogicDriver.AssetTools.Search.Regex",
								 EAutomationTestFlags::ApplicationContextMask |
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
								 EAutomationTestFlags::EngineFilter)

bool FAssetToolsTextGraphSearchTestRegexTest::RunTest(const FString& Parameters)
{
	const FName PropertyName = GET_MEMBER_NAME_CHECKED(USMAssetTestPropertyStateInstance, TextGraph);
	return TestRegex(this, PropertyName);
}

#endif

#endif //WITH_DEV_AUTOMATION_TESTS
