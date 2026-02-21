// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMGraphK2Node_StateReadNodes.h"

#include "Graph/Nodes/SMGraphNode_StateMachineStateNode.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/Schema/SMGraphK2Schema.h"
#include "Graph/SMConduitGraph.h"
#include "Graph/SMStateGraph.h"
#include "Graph/SMTransitionGraph.h"

#include "Blueprints/SMBlueprint.h"
#include "SMNode_Base.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "EdGraph/EdGraph.h"
#include "Kismet2/CompilerResultsLog.h"

#define LOCTEXT_NAMESPACE "SMStateMachineReadNode"

USMGraphK2Node_StateReadNode::USMGraphK2Node_StateReadNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText USMGraphK2Node_StateReadNode::GetMenuCategory() const
{
	return FText::FromString(STATE_MACHINE_HELPER_CATEGORY);
}

FString USMGraphK2Node_StateReadNode::GetMostRecentStateName() const
{
	USMGraphNode_StateNodeBase* StateNode = GetMostRecentState();

	if (StateNode == nullptr)
	{
		return FString();
	}

	return StateNode->GetStateName();
}

FString USMGraphK2Node_StateReadNode::GetTransitionName() const
{
	if (USMTransitionGraph* TransitionGraph = Cast<USMTransitionGraph>(GetGraph()))
	{
		if (USMGraphNode_TransitionEdge* Transition = TransitionGraph->GetOwningTransitionNode())
		{
			return Transition->GetTransitionName();
		}
	}

	return FString();
}

USMGraphNode_StateNodeBase* USMGraphK2Node_StateReadNode::GetMostRecentState() const
{
	if (USMTransitionGraph* TransitionGraph = Cast<USMTransitionGraph>(GetGraph()))
	{
		if (USMGraphNode_TransitionEdge* Transition = TransitionGraph->GetOwningTransitionNode())
		{
			return Transition->GetFromState();
		}
	}
	else if (USMStateGraph* StateGraph = Cast<USMStateGraph>(GetGraph()))
	{
		return StateGraph->GetOwningStateNode();
	}

	return nullptr;
}

bool USMGraphK2Node_StateReadNode::IsActionFilteredOut(FBlueprintActionFilter const& Filter)
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
		if (!Graph->IsA<USMTransitionGraph>() && !Graph->IsA<USMStateGraph>() && !Graph->IsA<USMConduitGraph>())
		{
			return true;
		}
	}

	return false;
}

void USMGraphK2Node_StateReadNode::PostPlacedNewNode()
{
	if (USMGraphK2Node_RuntimeNodeContainer* Container = GetRuntimeContainer())
	{
		RuntimeNodeGuid = Container->GetRunTimeNodeChecked()->GetNodeGuid();
	}
}

void USMGraphK2Node_StateReadNode::PostPasteNode()
{
	// Skip parent handling all together. Duplicating this type of node is fine.
	UK2Node::PostPasteNode();
	if (USMGraphK2Node_RuntimeNodeContainer* Container = GetRuntimeContainer())
	{
		RuntimeNodeGuid = Container->GetRunTimeNodeChecked()->GetNodeGuid();
	}
}

bool USMGraphK2Node_StateReadNode::IsCompatibleWithGraph(UEdGraph const* Graph) const
{
	return Graph->IsA<USMTransitionGraph>() || Graph->IsA<USMStateGraph>() || Graph->IsA<USMConduitGraph>();
}

USMGraphK2Node_StateReadNode_HasStateUpdated::USMGraphK2Node_StateReadNode_HasStateUpdated(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USMGraphK2Node_StateReadNode_HasStateUpdated::AllocateDefaultPins()
{
	CreatePin(EGPD_Output, USMGraphK2Schema::PC_Boolean, TEXT("bHasUpdated"));
}

FText USMGraphK2Node_StateReadNode_HasStateUpdated::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (TitleType != ENodeTitleType::MenuTitle)
	{
		const FString StateName = GetMostRecentStateName();
		if (!StateName.IsEmpty())
		{
			return FText::FromString(FString::Printf(TEXT("Has State '%s' Updated"), *StateName));
		}
	}
	
	return FText::FromString(TEXT("Has State Updated"));
}

FText USMGraphK2Node_StateReadNode_HasStateUpdated::GetTooltipText() const
{
	return LOCTEXT("StateUpdatedTooltip", "Called when the state has updated at least once.");
}

void USMGraphK2Node_StateReadNode_HasStateUpdated::GetMenuActions(
	FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	GetMenuActions_Internal(ActionRegistrar);
}

USMGraphK2Node_StateReadNode_TimeInState::USMGraphK2Node_StateReadNode_TimeInState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USMGraphK2Node_StateReadNode_TimeInState::AllocateDefaultPins()
{
	CreatePin(EGPD_Output, USMGraphK2Schema::PC_Real, USMGraphK2Schema::PC_Float, TEXT("TimeInState"));
}

FText USMGraphK2Node_StateReadNode_TimeInState::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (TitleType != ENodeTitleType::MenuTitle)
	{
		const FString StateName = GetMostRecentStateName();
		if (!StateName.IsEmpty())
		{
			return FText::FromString(FString::Printf(TEXT("Get Time in State '%s'"), *StateName));
		}
	}
	
	return FText::FromString(TEXT("Get Time in State"));
}

FText USMGraphK2Node_StateReadNode_TimeInState::GetTooltipText() const
{
	return LOCTEXT("StateTimeTooltip", "Current time in seconds state has been active.");
}

void USMGraphK2Node_StateReadNode_TimeInState::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	GetMenuActions_Internal(ActionRegistrar);
}

USMGraphK2Node_StateReadNode_CanEvaluate::USMGraphK2Node_StateReadNode_CanEvaluate(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USMGraphK2Node_StateReadNode_CanEvaluate::AllocateDefaultPins()
{
	CreatePin(EGPD_Output, USMGraphK2Schema::PC_Boolean, TEXT("bCanEvaluate"));
}

bool USMGraphK2Node_StateReadNode_CanEvaluate::IsCompatibleWithGraph(UEdGraph const* Graph) const
{
	return Graph->IsA<USMTransitionGraph>() || Graph->IsA<USMConduitGraph>();
}

FText USMGraphK2Node_StateReadNode_CanEvaluate::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("GetCanEvaluate", "Get Can Evaluate Conditionally");
}

FText USMGraphK2Node_StateReadNode_CanEvaluate::GetTooltipText() const
{
	return LOCTEXT("CanEvaluateTooltipRead", "If the transition or conduit is allowed to evaluate conditionally.");
}

void USMGraphK2Node_StateReadNode_CanEvaluate::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	GetMenuActions_Internal(ActionRegistrar);
}


USMGraphK2Node_StateReadNode_CanEvaluateFromEvent::USMGraphK2Node_StateReadNode_CanEvaluateFromEvent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USMGraphK2Node_StateReadNode_CanEvaluateFromEvent::AllocateDefaultPins()
{
	CreatePin(EGPD_Output, USMGraphK2Schema::PC_Boolean, TEXT("bCanEvaluateFromEvent"));
}

bool USMGraphK2Node_StateReadNode_CanEvaluateFromEvent::IsCompatibleWithGraph(UEdGraph const* Graph) const
{
	return Graph->IsA<USMTransitionGraph>();
}

FText USMGraphK2Node_StateReadNode_CanEvaluateFromEvent::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("GetCanTransitionEvaluateFromEvent", "Get Can Transition Evaluate From Event");
}

FText USMGraphK2Node_StateReadNode_CanEvaluateFromEvent::GetTooltipText() const
{
	return LOCTEXT("CanEvaluateFromEventTooltipRead", "If the transition is allowed to evaluate from auto-bound events.");
}

void USMGraphK2Node_StateReadNode_CanEvaluateFromEvent::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	GetMenuActions_Internal(ActionRegistrar);
}


USMGraphK2Node_StateMachineReadNode::USMGraphK2Node_StateMachineReadNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool USMGraphK2Node_StateMachineReadNode::IsCompatibleWithGraph(UEdGraph const* Graph) const
{
	if (USMTransitionGraph const* TransitionGraph = Cast<USMTransitionGraph>(Graph))
	{
		return Cast<USMGraphNode_StateMachineStateNode>(TransitionGraph->GetOwningTransitionNodeChecked()->GetFromState()) != nullptr;
	}

	return false;
}

bool USMGraphK2Node_StateMachineReadNode::IsActionFilteredOut(FBlueprintActionFilter const& Filter)
{
	for (UBlueprint* Blueprint : Filter.Context.Blueprints)
	{
		if (!Cast<USMBlueprint>(Blueprint))
		{
			return true;
		}
	}

	USMTransitionGraph* TransitionGraph = nullptr;
	for (UEdGraph* Graph : Filter.Context.Graphs)
	{
		// Only works on transition graphs.
		TransitionGraph = Cast<USMTransitionGraph>(Graph);
		if (!TransitionGraph)
		{
			return true;
		}
	}

	if (!TransitionGraph)
	{
		return false;
	}

	// Only work for state machine nodes.
	return !Cast<USMGraphNode_StateMachineStateNode>(TransitionGraph->GetOwningTransitionNodeChecked()->GetFromState());
}

void USMGraphK2Node_StateMachineReadNode::ValidateNodeDuringCompilation(FCompilerResultsLog& MessageLog) const
{
	UEdGraph* Graph = GetGraph();
	if (Graph == nullptr)
	{
		return;
	}
	
	if (USMTransitionGraph const* TransitionGraph = Cast<USMTransitionGraph>(Graph))
	{
		if (Cast<USMGraphNode_StateMachineStateNode>(TransitionGraph->GetOwningTransitionNodeChecked()->GetFromState()) == nullptr)
		{
			MessageLog.Error(TEXT("State Machine Read Node @@ is in a transition not exiting from a state machine."), this);
			return;
		}
	}
}

USMGraphK2Node_StateMachineReadNode_InEndState::USMGraphK2Node_StateMachineReadNode_InEndState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USMGraphK2Node_StateMachineReadNode_InEndState::AllocateDefaultPins()
{
	CreatePin(EGPD_Output, USMGraphK2Schema::PC_Boolean, GET_MEMBER_NAME_CHECKED(FSMNode_Base, bIsInEndState));
}

FText USMGraphK2Node_StateMachineReadNode_InEndState::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (TitleType != ENodeTitleType::MenuTitle)
	{
		const FString StateName = GetMostRecentStateName();
		if (!StateName.IsEmpty())
		{
			return FText::FromString(FString::Printf(TEXT("Is State Machine '%s' in End State"), *StateName));
		}
	}
	
	return FText::FromString(TEXT("Is State Machine in End State"));
}

FText USMGraphK2Node_StateMachineReadNode_InEndState::GetTooltipText() const
{
	return LOCTEXT("StateMachineInEndStateTooltip", "True if the previous state machine node is in an end state.");
}

void USMGraphK2Node_StateMachineReadNode_InEndState::GetMenuActions(
	FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	GetMenuActions_Internal(ActionRegistrar);
}

#undef LOCTEXT_NAMESPACE
