// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMGraphK2Node_TransitionPreEvaluateNode.h"

#include "Graph/Schema/SMGraphK2Schema.h"
#include "Graph/SMTransitionGraph.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "Blueprints/SMBlueprint.h"
#include "SMNode_Base.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraph/EdGraph.h"

#define LOCTEXT_NAMESPACE "SMTransitionPreEvaluateNode"

USMGraphK2Node_TransitionPreEvaluateNode::USMGraphK2Node_TransitionPreEvaluateNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USMGraphK2Node_TransitionPreEvaluateNode::AllocateDefaultPins()
{
	CreatePin(EGPD_Output, USMGraphK2Schema::PC_Exec, UEdGraphSchema_K2::PN_Then);
}

void USMGraphK2Node_TransitionPreEvaluateNode::PostPlacedNewNode()
{
	RuntimeNodeGuid = GetRuntimeContainerChecked()->GetRunTimeNodeChecked()->GetNodeGuid();
}

FText USMGraphK2Node_TransitionPreEvaluateNode::GetMenuCategory() const
{
	return FText::FromString(STATE_MACHINE_HELPER_CATEGORY);
}

FText USMGraphK2Node_TransitionPreEvaluateNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if ((TitleType == ENodeTitleType::MenuTitle || TitleType == ENodeTitleType::ListView))
	{
		return LOCTEXT("AddTransitionPreEvaluateEvent", "Add Event On Transition Pre-Evaluate");
	}

	return FText::FromString(TEXT("On Transition Pre-Evaluate"));
}

FText USMGraphK2Node_TransitionPreEvaluateNode::GetTooltipText() const
{
	return LOCTEXT("TransitionPreEvaluateNodeTooltip", "Called immediately before the transition result is evaluated.");
}

void USMGraphK2Node_TransitionPreEvaluateNode::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();

	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

bool USMGraphK2Node_TransitionPreEvaluateNode::IsActionFilteredOut(FBlueprintActionFilter const& Filter)
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
		// Only works on transition graphs.
		USMTransitionGraph* TransitionGraph = Cast<USMTransitionGraph>(Graph);
		if (!TransitionGraph)
		{
			return true;
		}

		// Only allow one node.
		if (FSMBlueprintEditorUtils::IsNodeAlreadyPlaced<USMGraphK2Node_TransitionPreEvaluateNode>(Graph))
		{
			return true;
		}
	}

	return false;
}

bool USMGraphK2Node_TransitionPreEvaluateNode::IsCompatibleWithGraph(UEdGraph const* Graph) const
{
	return Graph->IsA<USMTransitionGraph>() && !FSMBlueprintEditorUtils::IsNodeAlreadyPlaced<USMGraphK2Node_TransitionPreEvaluateNode>(Graph);
}

#undef LOCTEXT_NAMESPACE
