// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Graph/Nodes/SMGraphK2Node_StateMachineNode.h"

#include "Graph/Schema/SMGraphK2Schema.h"
#include "Graph/Schema/SMGraphSchema.h"
#include "Graph/SMConduitGraph.h"
#include "Graph/SMGraph.h"
#include "Graph/SMGraphK2.h"
#include "Graph/SMStateGraph.h"
#include "Graph/SMTransitionGraph.h"

#include "Blueprints/SMBlueprint.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/Kismet2NameValidators.h"

#define LOCTEXT_NAMESPACE "SMGraphK2StateMachineNode"

class FSMNameValidator : public FStringSetNameValidator
{
public:
	FSMNameValidator(const USMGraphK2Node_StateMachineNode* InStateMachineNode)
		: FStringSetNameValidator(FString())
	{
		TArray<USMGraphK2Node_StateMachineNode*> Nodes;

		USMGraphK2* StateMachine = CastChecked<USMGraphK2>(InStateMachineNode->GetOuter());
		StateMachine->GetNodesOfClassEx<USMGraphK2Node_StateMachineNode, USMGraphK2Node_StateMachineNode>(Nodes);

		for (auto Node : Nodes)
		{
			if (Node != InStateMachineNode)
			{
				Names.Add(Node->GetStateMachineName());
			}
		}
	}
};


USMGraphK2Node_StateMachineNode::USMGraphK2Node_StateMachineNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCanRenameNode = true;
	BoundGraph = nullptr;
}

void USMGraphK2Node_StateMachineNode::AllocateDefaultPins()
{
	CreatePin(EGPD_Output, USMGraphK2Schema::PC_StateMachine, TEXT(""));
}

void USMGraphK2Node_StateMachineNode::OnRenameNode(const FString& NewName)
{
	FBlueprintEditorUtils::RenameGraph(GetStateMachineGraph(), NewName);
}

void USMGraphK2Node_StateMachineNode::PostPlacedNewNode()
{
	// Create a new state machine graph
	check(BoundGraph == nullptr);
	BoundGraph = Cast<USMGraph>(FBlueprintEditorUtils::CreateNewGraph(
		this,
		NAME_None,
		USMGraph::StaticClass(),
		USMGraphSchema::StaticClass()));
	check(BoundGraph);

	// Find an interesting name
	const TSharedPtr<INameValidatorInterface> NameValidator = FNameValidatorFactory::MakeValidator(this);
	FBlueprintEditorUtils::RenameGraphWithSuggestion(BoundGraph, NameValidator, TEXT("State Machine"));

	// Initialize the state machine graph
	const UEdGraphSchema* Schema = BoundGraph->GetSchema();
	Schema->CreateDefaultNodesForGraph(*BoundGraph);

	// Add the new graph as a child of our parent graph
	UEdGraph* ParentGraph = GetGraph();

	if (ParentGraph->SubGraphs.Find(BoundGraph) == INDEX_NONE)
	{
		ParentGraph->Modify();
		ParentGraph->SubGraphs.Add(BoundGraph);
	}
}

void USMGraphK2Node_StateMachineNode::PostPasteNode()
{
	for (const TObjectPtr<UEdGraphNode>& GraphNode : BoundGraph->Nodes)
	{
		GraphNode->CreateNewGuid();
		GraphNode->PostPasteNode();
	}
	
	// Find an interesting name, but try to keep the same if possible
	const TSharedPtr<INameValidatorInterface> NameValidator = FNameValidatorFactory::MakeValidator(this);
	FBlueprintEditorUtils::RenameGraphWithSuggestion(BoundGraph, NameValidator, GetStateMachineName());
	
	UEdGraph* ParentGraph = GetGraph();

	if (ParentGraph->SubGraphs.Find(BoundGraph) == INDEX_NONE)
	{
		ParentGraph->Modify();
		ParentGraph->SubGraphs.Add(BoundGraph);
	}

	Super::PostPasteNode();
}

TSharedPtr<INameValidatorInterface> USMGraphK2Node_StateMachineNode::MakeNameValidator() const
{
	return MakeShareable(new FSMNameValidator(this));
}

void USMGraphK2Node_StateMachineNode::DestroyNode()
{
	UEdGraph* GraphToRemove = BoundGraph;

	BoundGraph = nullptr;
	Super::DestroyNode();

	if (GraphToRemove)
	{
		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(this);
		FBlueprintEditorUtils::RemoveGraph(Blueprint, GraphToRemove, EGraphRemoveFlags::Recompile);
	}
}

FText USMGraphK2Node_StateMachineNode::GetMenuCategory() const
{
	return FText::FromString(STATE_MACHINE_HELPER_CATEGORY);
}

UObject* USMGraphK2Node_StateMachineNode::GetJumpTargetForDoubleClick() const
{
	return BoundGraph;
}

bool USMGraphK2Node_StateMachineNode::IsNodePure() const
{
	return true;
}

bool USMGraphK2Node_StateMachineNode::IsActionFilteredOut(class FBlueprintActionFilter const& Filter)
{
	for (UBlueprint* Blueprint : Filter.Context.Blueprints)
	{
		if (!Cast<USMBlueprint>(Blueprint))
		{
			return true;
		}
	}

	for (UEdGraph* Graph : Filter.Context.Graphs)
	{
		USMGraphK2* TopLevelGraph = Cast<USMGraphK2>(Graph);
		// Only allow the top level graph to create state machines.
		if (!TopLevelGraph || Graph->IsA<USMStateGraph>() || Graph->IsA<USMTransitionGraph>())
		{
			return true;
		}
	}

	return false;
}

bool USMGraphK2Node_StateMachineNode::IsCompatibleWithGraph(UEdGraph const* Graph) const
{
	return Graph->IsA<USMGraphK2>() && !Graph->IsA<USMStateGraph>() && !Graph->IsA<USMTransitionGraph>() && !Graph->IsA<USMConduitGraph>();
}

FText USMGraphK2Node_StateMachineNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if ((TitleType == ENodeTitleType::MenuTitle || TitleType == ENodeTitleType::ListView) && (BoundGraph == nullptr))
	{
		return LOCTEXT("AddNewStateMachine", "Add New State Machine...");
	}
	else if (BoundGraph == nullptr)
	{
		if (TitleType == ENodeTitleType::FullTitle)
		{
			return LOCTEXT("NullStateMachineFullTitle", "Error: No Graph\nState Machine");
		}
		else
		{
			return LOCTEXT("ErrorNoGraph", "Error: No Graph");
		}
	}
	else if (TitleType == ENodeTitleType::FullTitle)
	{
		if (CachedFullTitle.IsOutOfDate(this))
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("Title"), FText::FromName(BoundGraph->GetFName()));
			// FText::Format() is slow, so we cache this to save on performance
			CachedFullTitle.SetCachedText(FText::Format(LOCTEXT("StateMachineFullTitle", "{Title}\nState Machine"), Args), this);
		}
		return CachedFullTitle;
	}

	return FText::FromName(BoundGraph->GetFName());
}

void USMGraphK2Node_StateMachineNode::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	// actions get registered under specific object-keys; the idea is that 
	// actions might have to be updated (or deleted) if their object-key is  
	// mutated (or removed)... here we use the node's class (so if the node 
	// type disappears, then the action should go with it)
	UClass* ActionKey = GetClass();
	// to keep from needlessly instantiating a UBlueprintNodeSpawner, first   
	// check to make sure that the registrar is looking for actions of this type
	// (could be regenerating actions for a specific asset, and therefore the 
	// registrar would only accept actions corresponding to that asset)
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

FString USMGraphK2Node_StateMachineNode::GetStateMachineName() const
{
	return BoundGraph ? CastChecked<USMGraph>(BoundGraph)->GetName() : TEXT("(null)");
}

USMGraphK2* USMGraphK2Node_StateMachineNode::GetTopLevelStateMachineGraph() const
{
	return Cast<USMGraphK2>(GetGraph());
}

#undef LOCTEXT_NAMESPACE
