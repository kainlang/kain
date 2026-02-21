// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Graph/SMConduitGraph.h"

#include "Graph/Nodes/RootNodes/SMGraphK2Node_ConduitResultNode.h"
#include "Nodes/FunctionNodes/SMGraphK2Node_FunctionNodes_ConduitInstance.h"
#include "Utilities/SMBlueprintEditorUtils.h"

USMConduitGraph::USMConduitGraph(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), ResultNode(nullptr)
{
}

bool USMConduitGraph::HasAnyLogicConnections() const
{
	if (bHasLogicConnectionsCached.IsSet())
	{
		return bHasLogicConnectionsCached.GetValue();
	}
	
	// Check if there are any pins wired to this boolean input.
	TArray<USMGraphK2Node_ConduitResultNode*> RootNodeList;

	// We want to find the node even if it's buried in a nested graph.
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphK2Node_ConduitResultNode>(const_cast<USMConduitGraph*>(this), RootNodeList);

	for (USMGraphK2Node_RootNode* RootNode : RootNodeList)
	{
		if (RootNode->GetInputPin()->LinkedTo.Num() || RootNode->GetInputPin()->DefaultValue.ToBool())
		{
			bHasLogicConnectionsCached = true;
			return true;
		}
	}

	bHasLogicConnectionsCached = false;
	return false;
}

FSMNode_Base* USMConduitGraph::GetRuntimeNode() const
{
	return ResultNode ? ResultNode->GetRunTimeNode() : nullptr;
}

ESMConditionalEvaluationType USMConduitGraph::GetConditionalEvaluationType() const
{
	// Check if there are any pins wired to this boolean input.
	TArray<USMGraphK2Node_ConduitResultNode*> RootNodeList;

	// We want to find the node even if it's buried in a nested graph.
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphK2Node_ConduitResultNode>(this, RootNodeList);

	for (const USMGraphK2Node_RootNode* RootNode : RootNodeList)
	{
		UEdGraphPin* Pin = RootNode->GetInputPin();
		
		if (Pin->LinkedTo.Num() == 0)
		{
			return Pin->DefaultValue.ToBool() ? ESMConditionalEvaluationType::SM_AlwaysTrue : ESMConditionalEvaluationType::SM_AlwaysFalse;
		}
		
		if (Pin->LinkedTo.Num() == 1 && Pin->LinkedTo[0]->GetOwningNode()->GetClass() == USMGraphK2Node_ConduitInstance_CanEnterTransition::StaticClass())
		{
			return ESMConditionalEvaluationType::SM_NodeInstance;
		}
	}

	return ESMConditionalEvaluationType::SM_Graph;
}
