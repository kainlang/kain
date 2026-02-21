// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMGraphK2Node_TransitionPostEvaluateNode.h"

#include "Graph/Schema/SMGraphK2Schema.h"
#include "Graph/SMTransitionGraph.h"

#include "Utilities/SMBlueprintEditorUtils.h"

#include "Blueprints/SMBlueprint.h"
#include "SMNode_Base.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraph/EdGraph.h"

#define LOCTEXT_NAMESPACE "SMTransitionPostEvaluateNode"

USMGraphK2Node_TransitionPostEvaluateNode::USMGraphK2Node_TransitionPostEvaluateNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USMGraphK2Node_TransitionPostEvaluateNode::AllocateDefaultPins()
{
	CreatePin(EGPD_Output, USMGraphK2Schema::PC_Exec, UEdGraphSchema_K2::PN_Then);
}

void USMGraphK2Node_TransitionPostEvaluateNode::PostPlacedNewNode()
{
	RuntimeNodeGuid = GetRuntimeContainerChecked()->GetRunTimeNodeChecked()->GetNodeGuid();
}

FText USMGraphK2Node_TransitionPostEvaluateNode::GetMenuCategory() const
{
	return FText::FromString(STATE_MACHINE_HELPER_CATEGORY);
}

FText USMGraphK2Node_TransitionPostEvaluateNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if ((TitleType == ENodeTitleType::MenuTitle || TitleType == ENodeTitleType::ListView))
	{
		return LOCTEXT("AddTransitionPostEvaluateEvent", "Add Event On Transition Post-Evaluate");
	}

	return FText::FromString(TEXT("On Transition Post-Evaluate"));
}

FText USMGraphK2Node_TransitionPostEvaluateNode::GetTooltipText() const
{
	return LOCTEXT("TransitionPostEvaluateNodeTooltip", "Called immediately after the transition result is evaluated.");
}

void USMGraphK2Node_TransitionPostEvaluateNode::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();

	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

bool USMGraphK2Node_TransitionPostEvaluateNode::IsActionFilteredOut(FBlueprintActionFilter const& Filter)
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
		if (FSMBlueprintEditorUtils::IsNodeAlreadyPlaced<USMGraphK2Node_TransitionPostEvaluateNode>(Graph))
		{
			return true;
		}
	}

	return false;
}

bool USMGraphK2Node_TransitionPostEvaluateNode::IsCompatibleWithGraph(UEdGraph const* Graph) const
{
	return Graph->IsA<USMTransitionGraph>() && !FSMBlueprintEditorUtils::IsNodeAlreadyPlaced<USMGraphK2Node_TransitionPostEvaluateNode>(Graph);
}

#undef LOCTEXT_NAMESPACE
