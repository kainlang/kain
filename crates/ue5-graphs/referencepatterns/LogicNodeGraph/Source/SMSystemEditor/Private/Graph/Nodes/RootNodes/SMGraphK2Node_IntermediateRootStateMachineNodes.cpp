// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMGraphK2Node_IntermediateRootStateMachineNodes.h"

#include "Graph/Nodes/FunctionNodes/SMGraphK2Node_FunctionNodes_StateInstance.h"
#include "Graph/SMConduitGraph.h"
#include "Graph/SMIntermediateGraph.h"
#include "Graph/SMStateGraph.h"
#include "Graph/SMTransitionGraph.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "Blueprints/SMBlueprint.h"

#include "BlueprintActionFilter.h"

#define LOCTEXT_NAMESPACE "SMIntermediateNodes"

USMGraphK2Node_IntermediateStateMachineStartNode::USMGraphK2Node_IntermediateStateMachineStartNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USMGraphK2Node_IntermediateStateMachineStartNode::AllocateDefaultPins()
{
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);
}

UK2Node::ERedirectType USMGraphK2Node_IntermediateStateMachineStartNode::DoPinsMatchForReconstruction(
	const UEdGraphPin* NewPin, int32 NewPinIndex, const UEdGraphPin* OldPin, int32 OldPinIndex) const
{
	ERedirectType RedirectType = Super::DoPinsMatchForReconstruction(NewPin, NewPinIndex, OldPin, OldPinIndex);

	/*
	 * Output pin used to be incorrectly named PN_Execute.
	 */
	if ((RedirectType == ERedirectType::ERedirectType_None) && (NewPin != nullptr) && (OldPin != nullptr))
	{
		if (OldPin->Direction == EEdGraphPinDirection::EGPD_Output && OldPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec && OldPin->GetFName() == UEdGraphSchema_K2::PN_Execute)
		{
			RedirectType = ERedirectType::ERedirectType_Name;
		}
	}

	return RedirectType;
}

FText USMGraphK2Node_IntermediateStateMachineStartNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if ((TitleType == ENodeTitleType::MenuTitle || TitleType == ENodeTitleType::ListView))
	{
		return LOCTEXT("AddOnRootStateMachineStartEvent", "Add Event On Root State Machine Start");
	}
	
	return FText::FromString(TEXT("On Root State Machine Start"));
}

FText USMGraphK2Node_IntermediateStateMachineStartNode::GetTooltipText() const
{
	return LOCTEXT("IntermediateStateMachineStartTooltip", "Called when the immediate owning state machine blueprint is starting.\
 \nIf this is part of a reference then it will be called when the reference starts. If this is for a state machine node\
\nthen it will only be called when the top level state machine starts.");
}

FText USMGraphK2Node_IntermediateStateMachineStartNode::GetMenuCategory() const
{
	return FText::FromString(STATE_MACHINE_HELPER_CATEGORY);
}

bool USMGraphK2Node_IntermediateStateMachineStartNode::IsCompatibleWithGraph(UEdGraph const* Graph) const
{
	return (Graph->IsA<USMIntermediateGraph>() || Graph->IsA<USMStateGraph>() || Graph->IsA<USMTransitionGraph>() ||
	Graph->IsA<USMConduitGraph>()) && !FSMBlueprintEditorUtils::IsNodeAlreadyPlaced<USMGraphK2Node_IntermediateStateMachineStartNode>(Graph);
}

void USMGraphK2Node_IntermediateStateMachineStartNode::GetMenuActions(
	FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	GetMenuActions_Internal(ActionRegistrar);
}

bool USMGraphK2Node_IntermediateStateMachineStartNode::IsActionFilteredOut(FBlueprintActionFilter const& Filter)
{
	for (UBlueprint* Blueprint : Filter.Context.Blueprints)
	{
		if (!Cast<USMBlueprint>(Blueprint))
		{
			return true;
		}
	}

	for (const UEdGraph* Graph : Filter.Context.Graphs)
	{
		if (!(Graph->IsA<USMIntermediateGraph>() || Graph->IsA<USMStateGraph>() || Graph->IsA<USMTransitionGraph>()
			|| Graph->IsA<USMConduitGraph>()) || FSMBlueprintEditorUtils::IsNodeAlreadyPlaced<USMGraphK2Node_IntermediateStateMachineStartNode>(Graph))
		{
			return true;
		}
	}

	return false;
}

void USMGraphK2Node_IntermediateStateMachineStartNode::PostPlacedNewNode()
{
	if (USMGraphK2Node_RuntimeNodeContainer* Container = GetRuntimeContainer())
	{
		RuntimeNodeGuid = Container->GetRunTimeNodeChecked()->GetNodeGuid();
	}
}

bool USMGraphK2Node_IntermediateStateMachineStartNode::IsCompatibleWithInstanceGraphNodeClass(
	TSubclassOf<USMGraphK2Node_FunctionNode_NodeInstance> InGraphNodeClass) const
{
	return InGraphNodeClass == USMGraphK2Node_StateInstance_StateMachineStart::StaticClass();
}

USMGraphK2Node_IntermediateStateMachineStopNode::USMGraphK2Node_IntermediateStateMachineStopNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USMGraphK2Node_IntermediateStateMachineStopNode::AllocateDefaultPins()
{
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);
}

FText USMGraphK2Node_IntermediateStateMachineStopNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if ((TitleType == ENodeTitleType::MenuTitle || TitleType == ENodeTitleType::ListView))
	{
		return LOCTEXT("AddOnRootStateMachineStopEvent", "Add Event On Root State Machine Stop");
	}
	
	return FText::FromString(TEXT("On Root State Machine Stop"));
}

FText USMGraphK2Node_IntermediateStateMachineStopNode::GetTooltipText() const
{
	return LOCTEXT("IntermediateStateMachineStopTooltip", "Called when the immediate owning state machine blueprint has stopped.\
 \nIf this is part of a reference then it will be called when the reference stops. If this is for a state machine node\
\nthen it will only be called when the top level state machine stops.");
}

FText USMGraphK2Node_IntermediateStateMachineStopNode::GetMenuCategory() const
{
	return FText::FromString(STATE_MACHINE_HELPER_CATEGORY);
}

bool USMGraphK2Node_IntermediateStateMachineStopNode::IsCompatibleWithGraph(UEdGraph const* Graph) const
{
	return (Graph->IsA<USMIntermediateGraph>() || Graph->IsA<USMStateGraph>() || Graph->IsA<USMTransitionGraph>() ||
		Graph->IsA<USMConduitGraph>()) && !FSMBlueprintEditorUtils::IsNodeAlreadyPlaced<USMGraphK2Node_IntermediateStateMachineStopNode>(Graph);
}

void USMGraphK2Node_IntermediateStateMachineStopNode::GetMenuActions(
	FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	GetMenuActions_Internal(ActionRegistrar);
}

bool USMGraphK2Node_IntermediateStateMachineStopNode::IsActionFilteredOut(FBlueprintActionFilter const& Filter)
{
	for (UBlueprint* Blueprint : Filter.Context.Blueprints)
	{
		if (!Cast<USMBlueprint>(Blueprint))
		{
			return true;
		}
	}

	for (const UEdGraph* Graph : Filter.Context.Graphs)
	{
		if (!(Graph->IsA<USMIntermediateGraph>() || Graph->IsA<USMStateGraph>() || Graph->IsA<USMTransitionGraph>()
			|| Graph->IsA<USMConduitGraph>()) || FSMBlueprintEditorUtils::IsNodeAlreadyPlaced<USMGraphK2Node_IntermediateStateMachineStopNode>(Graph))
		{
			return true;
		}
	}

	return false;
}

void USMGraphK2Node_IntermediateStateMachineStopNode::PostPlacedNewNode()
{
	if (USMGraphK2Node_RuntimeNodeContainer* Container = GetRuntimeContainer())
	{
		RuntimeNodeGuid = Container->GetRunTimeNodeChecked()->GetNodeGuid();
	}
}

bool USMGraphK2Node_IntermediateStateMachineStopNode::IsCompatibleWithInstanceGraphNodeClass(
	TSubclassOf<USMGraphK2Node_FunctionNode_NodeInstance> InGraphNodeClass) const
{
	return InGraphNodeClass == USMGraphK2Node_StateInstance_StateMachineStop::StaticClass();
}

#undef LOCTEXT_NAMESPACE
