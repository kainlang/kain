// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMGraphFactory.h"

#include "Configuration/SMEditorSettings.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_RuntimeNodeContainer.h"
#include "Graph/Nodes/SMGraphK2Node_StateMachineNode.h"
#include "Graph/Nodes/SMGraphNode_AnyStateNode.h"
#include "Graph/Nodes/SMGraphNode_ConduitNode.h"
#include "Graph/Nodes/SMGraphNode_LinkStateNode.h"
#include "Graph/Nodes/SMGraphNode_RerouteNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineEntryNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineStateNode.h"
#include "Graph/Nodes/SMGraphNode_StateNode.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/Schema/SMGraphK2Schema.h"
#include "Nodes/SlateNodes/SGraphNode_ExecutionEntryNode.h"
#include "Nodes/SlateNodes/SGraphNode_StateMachineEntryNode.h"
#include "Nodes/SlateNodes/SGraphNode_StateMachineNode.h"
#include "Nodes/SlateNodes/SGraphNode_StateMachineStateNode.h"
#include "Nodes/SlateNodes/SGraphNode_StateNode.h"
#include "Nodes/SlateNodes/SGraphNode_TransitionEdge.h"
#include "Pins/SGraphPin_StateMachinePin.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "EdGraphSchema_K2.h"
#include "KismetPins/SGraphPinExec.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

#define LOCTEXT_NAMESPACE "SMGraphFactory"

TSharedPtr<SGraphNode> FSMGraphPanelNodeFactory::CreateNode(UEdGraphNode* Node) const
{
	if (USMGraphK2Node_StateMachineNode* StateMachineNode = Cast<USMGraphK2Node_StateMachineNode>(Node))
	{
		return SNew(SGraphNode_StateMachineNode, StateMachineNode);
	}

	if (USMGraphNode_StateNode* StateNode = Cast<USMGraphNode_StateNode>(Node))
	{
		const USMEditorSettings* Settings = FSMBlueprintEditorUtils::GetEditorSettings();
		return SNew(SGraphNode_StateNode, StateNode).ContentPadding(Settings->StateContentPadding);
	}

	if (USMGraphNode_AnyStateNode* AnyStateNode = Cast<USMGraphNode_AnyStateNode>(Node))
	{
		const USMEditorSettings* Settings = FSMBlueprintEditorUtils::GetEditorSettings();
		return SNew(SGraphNode_StateNode, AnyStateNode).ContentPadding(Settings->StateContentPadding);
	}

	if (USMGraphNode_LinkStateNode* LinkStateNode = Cast<USMGraphNode_LinkStateNode>(Node))
	{
		const USMEditorSettings* Settings = FSMBlueprintEditorUtils::GetEditorSettings();
		return SNew(SGraphNode_StateNode, LinkStateNode).ContentPadding(Settings->StateContentPadding);
	}

	if (USMGraphNode_RerouteNode* RerouteNode = Cast<USMGraphNode_RerouteNode>(Node))
	{
		return SNew(SGraphNode_TransitionEdge, RerouteNode);
	}

	if (USMGraphNode_TransitionEdge* EdgeNode = Cast<USMGraphNode_TransitionEdge>(Node))
	{
		return SNew(SGraphNode_TransitionEdge, EdgeNode);
	}

	if (USMGraphNode_StateMachineEntryNode* EntryNode = Cast<USMGraphNode_StateMachineEntryNode>(Node))
	{
		return SNew(SGraphNode_StateMachineEntryNode, EntryNode);
	}

	if (USMGraphNode_ConduitNode* ConduitNode = Cast<USMGraphNode_ConduitNode>(Node))
	{
		return SNew(SGraphNode_ConduitNode, ConduitNode);
	}

	if (USMGraphNode_StateMachineStateNode* StateMachineStateNode = Cast<USMGraphNode_StateMachineStateNode>(Node))
	{
		return SNew(SGraphNode_StateMachineStateNode, StateMachineStateNode);
	}
	
	if (USMGraphK2Node_RuntimeNode_Base* EntryNode = Cast<USMGraphK2Node_RuntimeNode_Base>(Node))
	{
		if (EntryNode->IsConsideredForEntryConnection())
		{
			return SNew(SGraphNode_ExecutionEntryNode, EntryNode);
		}
	}

	return FGraphPanelNodeFactory::CreateNode(Node);
}

TSharedPtr<SGraphPin> FSMGraphPinFactory::CreatePin(UEdGraphPin* InPin) const
{
	if (InPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
	{
		return SNew(SGraphPinExec, InPin);
	}
	if (InPin->PinType.PinCategory == USMGraphK2Schema::PC_StateMachine)
	{
		return SNew(SSMGraphPin_StateMachinePin, InPin);
	}

	return FGraphPanelPinFactory::CreatePin(InPin);
}

#undef LOCTEXT_NAMESPACE
