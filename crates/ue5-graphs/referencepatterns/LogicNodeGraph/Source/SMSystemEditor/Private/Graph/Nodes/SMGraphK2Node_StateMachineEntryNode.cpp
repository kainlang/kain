// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Graph/Nodes/SMGraphK2Node_StateMachineEntryNode.h"

#include "EdGraphSchema_K2.h"

#define LOCTEXT_NAMESPACE "SMK2StateMachineEntryNode"

USMGraphK2Node_StateMachineEntryNode::USMGraphK2Node_StateMachineEntryNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCanRenameNode = false;
	StateMachineNode.GenerateNewNodeGuidIfNotSet();
}

void USMGraphK2Node_StateMachineEntryNode::AllocateDefaultPins()
{
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);
}

#undef LOCTEXT_NAMESPACE
