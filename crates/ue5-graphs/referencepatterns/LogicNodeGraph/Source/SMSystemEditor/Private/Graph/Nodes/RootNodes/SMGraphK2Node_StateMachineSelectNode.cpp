// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMGraphK2Node_StateMachineSelectNode.h"

#include "Graph/Schema/SMGraphK2Schema.h"
#include "Graph/SMGraphK2.h"
#include "Graph/SMStateGraph.h"
#include "Graph/SMTransitionGraph.h"

#include "EdGraph/EdGraph.h"
#include "GraphEditorSettings.h"

#define LOCTEXT_NAMESPACE "SMStateMachineSelectNode"

USMGraphK2Node_StateMachineSelectNode::USMGraphK2Node_StateMachineSelectNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCanRenameNode = false;
}

void USMGraphK2Node_StateMachineSelectNode::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, USMGraphK2Schema::PC_StateMachine, FName("StateMachine"));
}

FLinearColor USMGraphK2Node_StateMachineSelectNode::GetNodeTitleColor() const
{
	return GetDefault<UGraphEditorSettings>()->ResultNodeTitleColor;
}

FText USMGraphK2Node_StateMachineSelectNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("StateMachineSelect", "State Machine Definition");
}

FText USMGraphK2Node_StateMachineSelectNode::GetTooltipText() const
{
	return LOCTEXT("StateMachineSelectToolTip", "This node selects the State Machine to use.");
}

bool USMGraphK2Node_StateMachineSelectNode::IsCompatibleWithGraph(UEdGraph const* Graph) const
{
	return Graph->IsA<USMGraphK2>() && !Graph->IsA<USMStateGraph>() && !Graph->IsA<USMTransitionGraph>();
}

#undef LOCTEXT_NAMESPACE
