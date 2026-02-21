// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "ISMSearch.h"

#include "Construction/ISMEditorConstructionManager.h"
#include "Graph/Nodes/PropertyNodes/SMGraphK2Node_PropertyNode_Base.h"
#include "Graph/Nodes/SMGraphNode_Base.h"

#include "SMNodeInstance.h"

#include "Engine/AssetManager.h"
#include "FindInBlueprintManager.h"
#include "Kismet2/BlueprintEditorUtils.h"

void ISMSearch::FSearchResultFiB::Finalize()
{
	check(Blueprint.IsValid());
	check(Graph.IsValid());
	check(GraphNode.IsValid());
	check(GraphPin.IsValid());

	Blueprint->FinalizeSearchData();
	Graph->FinalizeSearchData();
	GraphNode->FinalizeSearchData();
	GraphPin->FinalizeSearchData();
}

FString ISMSearch::FSearchResult::GetBlueprintName() const
{
	if (Blueprint.IsValid())
	{
		return Blueprint->GetName();
	}

	const FSoftObjectPath SoftObjectPath(BlueprintPath);
	return SoftObjectPath.GetAssetName();
}

FString ISMSearch::FSearchResult::GetNodeName() const
{
	if (const USMGraphNode_Base* SMGraphNode = Cast<USMGraphNode_Base>(GraphNode.Get()))
	{
		return SMGraphNode->GetNodeName();
	}
	return GraphNode.IsValid() ? GraphNode->GetName() : FiBResult->NodeName;
}

FString ISMSearch::FSearchResult::GetPropertyName() const
{
	return FiBResult.IsValid() ? FiBResult->PropertyName : FString();
}

int32 ISMSearch::FSearchResult::GetPropertyIndex() const
{
	return FiBResult.IsValid() ? FiBResult->ArrayIndex : INDEX_NONE;
}

int32 ISMSearch::FSearchResult::GetBeginMatchedIndex() const
{
	if (MatchedTextRanges.Num() > 0)
	{
		return MatchedTextRanges[0].BeginIndex;
	}
	return INDEX_NONE;
}

int32 ISMSearch::FSearchResult::GetEndMatchedIndex() const
{
	if (MatchedTextRanges.Num() > 0)
	{
		return MatchedTextRanges.Last().EndIndex;
	}

	return INDEX_NONE;
}

int32 ISMSearch::FSearchResult::FindMatchedTextRangeIntersectingRange(const FTextRange& InRange) const
{
	for (int32 Idx = 0; Idx < MatchedTextRanges.Num(); ++Idx)
	{
		const FTextRange& MatchedRange = MatchedTextRanges[Idx];
		if (!MatchedRange.Intersect(InRange).IsEmpty())
		{
			return Idx;
		}
	}

	return INDEX_NONE;
}

bool ISMSearch::FSearchResult::HasError() const
{
	return ReplaceResult.IsValid() && !ReplaceResult->ErrorMessage.IsEmpty();
}

void ISMSearch::FSearchResult::CheckResult() const
{
	check(Blueprint.IsValid());
	check(NodeInstance.IsValid());
	check(Property != nullptr);
}

void ISMSearch::FSearchResult::TryResolveObjects()
{
	const FSoftObjectPath SoftObjectPath(BlueprintPath);

	if (!Blueprint.IsValid())
	{
		Blueprint = Cast<UBlueprint>(SoftObjectPath.ResolveObject());
	}

	if (Blueprint.IsValid() && !GraphNode.IsValid() && FiBResult->NodeGuid.IsValid())
	{
		GraphNode = FBlueprintEditorUtils::GetNodeByGUID(Blueprint.Get(), FiBResult->NodeGuid);
	}

	if (Blueprint.IsValid() && GraphNode.IsValid() && Property == nullptr && FiBResult->PropertyGuid.IsValid())
	{
		if (const USMGraphNode_Base* SMGraphNode = Cast<USMGraphNode_Base>(GraphNode.Get()))
		{
			if (USMGraphK2Node_PropertyNode_Base* GraphPropertyNode = SMGraphNode->GetGraphPropertyNode(
				FiBResult->PropertyGuid))
			{
				NodeInstance = GraphPropertyNode->GetOwningTemplate();
				const FSMGraphProperty_Base* GraphProperty = GraphPropertyNode->GetPropertyNodeChecked();

				PropertyIndex = GraphProperty->ArrayIndex;

                UClass* ClassToUse = NodeInstance->GetClass();
				Property = GraphProperty->MemberReference.ResolveMember<FProperty>(ClassToUse);
			}
		}
	}
}

void ISMSearch::FSearchResult::LoadObjects()
{
	check(IsInGameThread());

	TryResolveObjects();

	if (!Blueprint.IsValid() && !BlueprintPath.IsEmpty())
	{
		FSMDisableConstructionScriptsOnScope DisableConstructionScriptsOnScope;
		if (bAllowConstructionScriptsOnLoad)
		{
			DisableConstructionScriptsOnScope.Cancel();
		}

		GIsEditorLoadingPackage = true;
		Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
		GIsEditorLoadingPackage = false;

		TryResolveObjects();
	}
}

TSharedPtr<FStreamableHandle> ISMSearch::FSearchResult::AsyncLoadObjects(const FSimpleDelegate& InOnLoadedDelegate)
{
	OnLoadDelegate = InOnLoadedDelegate;

	TryResolveObjects();

	if (!Blueprint.IsValid())
	{
		if (!bAllowConstructionScriptsOnLoad)
		{
			ISMEditorConstructionManager::Get().SetAllowConstructionScriptsOnLoadForBlueprint(BlueprintPath, false);
		}

		const FSoftObjectPath SoftObjectPath(BlueprintPath);
		return UAssetManager::GetStreamableManager().RequestAsyncLoad(SoftObjectPath, FStreamableDelegate::CreateLambda([this]()
		{
			if (!bAllowConstructionScriptsOnLoad)
			{
				ISMEditorConstructionManager::Get().SetAllowConstructionScriptsOnLoadForBlueprint(BlueprintPath, true);
			}
			TryResolveObjects();
			OnLoadDelegate.ExecuteIfBound();
		}));
	}

	OnLoadDelegate.ExecuteIfBound();

	return nullptr;
}
