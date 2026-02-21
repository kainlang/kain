// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Graph/Nodes/RootNodes/SMGraphK2Node_IntermediateEntryNode.h"

#include "Graph/SMIntermediateGraph.h"

#include "BlueprintActionFilter.h"
#include "EdGraphSchema_K2.h"

#define LOCTEXT_NAMESPACE "SMIntermediateEntryNode"

USMGraphK2Node_IntermediateEntryNode::USMGraphK2Node_IntermediateEntryNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	StateMachineNode.GenerateNewNodeGuidIfNotSet();
}

void USMGraphK2Node_IntermediateEntryNode::AllocateDefaultPins()
{
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);
}

FText USMGraphK2Node_IntermediateEntryNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FText::FromString(TEXT("On State Begin"));
}

FText USMGraphK2Node_IntermediateEntryNode::GetTooltipText() const
{
	return LOCTEXT("IntermediateEntryNodeTooltip", "Entry point for intermediate graph.");
}

bool USMGraphK2Node_IntermediateEntryNode::IsCompatibleWithGraph(UEdGraph const* Graph) const
{
	return Graph->IsA<USMIntermediateGraph>();
}

#undef LOCTEXT_NAMESPACE
