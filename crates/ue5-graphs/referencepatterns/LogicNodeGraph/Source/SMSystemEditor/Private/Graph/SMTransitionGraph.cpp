// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Graph/SMTransitionGraph.h"

#include "Graph/Nodes/RootNodes/SMGraphK2Node_TransitionResultNode.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Nodes/FunctionNodes/SMGraphK2Node_FunctionNodes_TransitionInstance.h"
#include "Nodes/FunctionNodes/SMGraphK2Node_StateWriteNodes.h"
#include "Nodes/RootNodes/SMGraphK2Node_TransitionEnteredNode.h"
#include "Nodes/RootNodes/SMGraphK2Node_TransitionInitializedNode.h"
#include "Nodes/RootNodes/SMGraphK2Node_TransitionPostEvaluateNode.h"
#include "Nodes/RootNodes/SMGraphK2Node_TransitionPreEvaluateNode.h"
#include "Nodes/RootNodes/SMGraphK2Node_TransitionShutdownNode.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "EdGraph/EdGraphPin.h"

USMTransitionGraph::USMTransitionGraph(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), ResultNode(nullptr)
{
}

USMGraphNode_TransitionEdge* USMTransitionGraph::GetOwningTransitionNode() const
{
	return Cast<USMGraphNode_TransitionEdge>(GetOuter());
}

USMGraphNode_TransitionEdge* USMTransitionGraph::GetOwningTransitionNodeChecked() const
{
	return CastChecked<USMGraphNode_TransitionEdge>(GetOuter());
}

bool USMTransitionGraph::HasAnyLogicConnections() const
{
	if (bHasLogicConnectionsCached.IsSet())
	{
		return bHasLogicConnectionsCached.GetValue();
	}
	
	// Check if there are any pins wired to this boolean input.
	TArray<USMGraphK2Node_TransitionResultNode*> RootNodeList;

	// We want to find the node even if it's buried in a nested graph.
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphK2Node_TransitionResultNode>(this, RootNodeList);

	for (USMGraphK2Node_RootNode* RootNode : RootNodeList)
	{
		if (RootNode->GetInputPin()->LinkedTo.Num() || RootNode->GetInputPin()->DefaultValue.ToBool())
		{
			bHasLogicConnectionsCached = true;
			return true;
		}
	}

	// Check event triggers.
	if (FSMBlueprintEditorUtils::IsGraphConfiguredForTransitionEvents(this))
	{
		// Check result nodes... not the greatest check since it doesn't verify they're connected to the entry node.
		TArray<USMGraphK2Node_StateWriteNode_TransitionEventReturn*> EventResultList;
		FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphK2Node_StateWriteNode_TransitionEventReturn>(this, EventResultList);
		for (USMGraphK2Node_StateWriteNode_TransitionEventReturn* Node : EventResultList)
		{
			if (Node->GetExecPin()->LinkedTo.Num() > 0 && (Node->GetInputPin()->LinkedTo.Num() || Node->GetInputPin()->DefaultValue.ToBool()))
			{
				bHasLogicConnectionsCached = true;
				return true;
			}
		}
	}

	bHasLogicConnectionsCached = false;
	
	return false;
}

FSMNode_Base* USMTransitionGraph::GetRuntimeNode() const
{
	return ResultNode ? ResultNode->GetRunTimeNode() : nullptr;
}

ESMConditionalEvaluationType USMTransitionGraph::GetConditionalEvaluationType() const
{
	// Check if there are any pins wired to this boolean input.
	TArray<USMGraphK2Node_TransitionResultNode*> RootNodeList;

	// We want to find the node even if it's buried in a nested graph.
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphK2Node_TransitionResultNode>(this, RootNodeList);

	for (const USMGraphK2Node_RootNode* RootNode : RootNodeList)
	{
		UEdGraphPin* Pin = RootNode->GetInputPin();
		
		if (Pin->LinkedTo.Num() == 0)
		{
			return Pin->DefaultValue.ToBool() ? ESMConditionalEvaluationType::SM_AlwaysTrue : ESMConditionalEvaluationType::SM_AlwaysFalse;
		}
		
		if (Pin->LinkedTo.Num() == 1 && Pin->LinkedTo[0]->GetOwningNode()->GetClass() == USMGraphK2Node_TransitionInstance_CanEnterTransition::StaticClass())
		{
			return ESMConditionalEvaluationType::SM_NodeInstance;
		}
	}

	return ESMConditionalEvaluationType::SM_Graph;
}

bool USMTransitionGraph::HasTransitionEnteredLogic() const
{
	return HasNodeWithExecutionLogic<USMGraphK2Node_TransitionEnteredNode>();
}

bool USMTransitionGraph::HasPreEvalLogic() const
{
	return HasNodeWithExecutionLogic<USMGraphK2Node_TransitionPreEvaluateNode>();
}

bool USMTransitionGraph::HasPostEvalLogic() const
{
	return HasNodeWithExecutionLogic<USMGraphK2Node_TransitionPostEvaluateNode>();
}

bool USMTransitionGraph::HasInitLogic() const
{
	return HasNodeWithExecutionLogic<USMGraphK2Node_TransitionInitializedNode>();
}

bool USMTransitionGraph::HasShutdownLogic() const
{
	return HasNodeWithExecutionLogic<USMGraphK2Node_TransitionShutdownNode>();
}

template<typename T>
bool USMTransitionGraph::HasNodeWithExecutionLogic() const
{
	TArray<T*> CompletedNodeList;

	// We want to find the node even if it's buried in a nested graph.
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested<T>(const_cast<USMTransitionGraph*>(this), CompletedNodeList);

	for (USMGraphK2Node_RootNode* Node : CompletedNodeList)
	{
		if (Node->GetOutputNode())
		{
			return true;
		}
	}

	return false;
}