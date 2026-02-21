// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMGraphK2Node_StateEndNode.h"

#include "Graph/Nodes/FunctionNodes/SMGraphK2Node_FunctionNodes_StateInstance.h"
#include "Graph/SMStateGraph.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"

#define LOCTEXT_NAMESPACE "SMStateEndNode"

USMGraphK2Node_StateEndNode::USMGraphK2Node_StateEndNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USMGraphK2Node_StateEndNode::AllocateDefaultPins()
{
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);
}

FText USMGraphK2Node_StateEndNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FText::FromString(TEXT("On State End"));
}

FText USMGraphK2Node_StateEndNode::GetTooltipText() const
{
	return LOCTEXT("StateEndNodeTooltip", "Called when the state completes. It is not advised to switch states during this event.\
\nThe state machine will already be in the process of switching states.");
}

bool USMGraphK2Node_StateEndNode::IsCompatibleWithGraph(UEdGraph const* Graph) const
{
	return Graph->IsA<USMStateGraph>();
}

bool USMGraphK2Node_StateEndNode::IsCompatibleWithInstanceGraphNodeClass(
	TSubclassOf<USMGraphK2Node_FunctionNode_NodeInstance> InGraphNodeClass) const
{
	return InGraphNodeClass == USMGraphK2Node_StateInstance_End::StaticClass();
}

#undef LOCTEXT_NAMESPACE
