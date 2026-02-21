// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMGraphK2Node_TransitionEnteredNode.h"

#include "Graph/Nodes/FunctionNodes/SMGraphK2Node_FunctionNodes_TransitionInstance.h"
#include "Graph/Schema/SMGraphK2Schema.h"
#include "Graph/SMConduitGraph.h"
#include "Graph/SMTransitionGraph.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "Blueprints/SMBlueprint.h"
#include "SMNode_Base.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraph/EdGraph.h"

#define LOCTEXT_NAMESPACE "SMTransitionEnteredNode"

USMGraphK2Node_TransitionEnteredNode::USMGraphK2Node_TransitionEnteredNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USMGraphK2Node_TransitionEnteredNode::AllocateDefaultPins()
{
	CreatePin(EGPD_Output, USMGraphK2Schema::PC_Exec, UEdGraphSchema_K2::PN_Then);
}

void USMGraphK2Node_TransitionEnteredNode::PostPlacedNewNode()
{
	RuntimeNodeGuid = GetRuntimeContainerChecked()->GetRunTimeNodeChecked()->GetNodeGuid();
}

FText USMGraphK2Node_TransitionEnteredNode::GetMenuCategory() const
{
	return FText::FromString(STATE_MACHINE_HELPER_CATEGORY);
}

FText USMGraphK2Node_TransitionEnteredNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	const bool IsConduit = Cast<USMConduitGraph>(FSMBlueprintEditorUtils::FindTopLevelOwningGraph(GetGraph())) != nullptr;

	if ((TitleType == ENodeTitleType::MenuTitle || TitleType == ENodeTitleType::ListView))
	{
		return IsConduit ? LOCTEXT("AddConduitEnteredEvent", "Add Event On Conduit Entered") : LOCTEXT("AddTransitionEnteredEvent", "Add Event On Transition Entered");
	}

	return FText::FromString(IsConduit ? TEXT("On Conduit Entered") : TEXT("On Transition Entered"));
}

FText USMGraphK2Node_TransitionEnteredNode::GetTooltipText() const
{
	return LOCTEXT("TransitionEnteredNodeTooltip", "Called after the transition result is evaluated and when the transition is successfully taken.");
}

void USMGraphK2Node_TransitionEnteredNode::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();

	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

bool USMGraphK2Node_TransitionEnteredNode::IsActionFilteredOut(FBlueprintActionFilter const& Filter)
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
		// Only works on transition and conduit graphs.
		if (!Graph->IsA<USMTransitionGraph>() && !Graph->IsA<USMConduitGraph>())
		{
			return true;
		}

		// Only allow one node.
		if (FSMBlueprintEditorUtils::IsNodeAlreadyPlaced<USMGraphK2Node_TransitionEnteredNode>(Graph))
		{
			return true;
		}
	}

	return false;
}

bool USMGraphK2Node_TransitionEnteredNode::IsCompatibleWithGraph(UEdGraph const* Graph) const
{
	return (Graph->IsA<USMTransitionGraph>() || Graph->IsA<USMConduitGraph>()) && !FSMBlueprintEditorUtils::IsNodeAlreadyPlaced<USMGraphK2Node_TransitionEnteredNode>(Graph);
}

bool USMGraphK2Node_TransitionEnteredNode::IsCompatibleWithInstanceGraphNodeClass(
	TSubclassOf<USMGraphK2Node_FunctionNode_NodeInstance> InGraphNodeClass) const
{
	return InGraphNodeClass == USMGraphK2Node_TransitionInstance_OnTransitionTaken::StaticClass() ||
		InGraphNodeClass == USMGraphK2Node_ConduitInstance_OnConduitEntered::StaticClass();
}

#undef LOCTEXT_NAMESPACE
