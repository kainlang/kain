// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Graph/SMGraph.h"

#include "Graph/Nodes/PropertyNodes/SMGraphK2Node_PropertyNode_Base.h"
#include "Graph/Nodes/SMGraphK2Node_StateMachineEntryNode.h"
#include "Graph/Nodes/SMGraphK2Node_StateMachineNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineEntryNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineStateNode.h"
#include "Graph/Nodes/SMGraphNode_StateNodeBase.h"
#include "Graph/SMPropertyGraph.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "GraphEditAction.h"
#include "K2Node_CallFunction.h"

USMGraph::USMGraph(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), GeneratedContainerNode(nullptr), EntryNode(nullptr)
{
}

USMGraphNode_StateMachineEntryNode* USMGraph::GetEntryNode() const
{
	for (const TObjectPtr<UEdGraphNode>& Node : Nodes)
	{
		if (USMGraphNode_StateMachineEntryNode* RealEntryNode = Cast<USMGraphNode_StateMachineEntryNode>(Node))
		{
			return RealEntryNode;
		}
	}

	return nullptr;
}

USMGraphK2Node_StateMachineNode* USMGraph::GetOwningStateMachineK2Node() const
{
	return Cast<USMGraphK2Node_StateMachineNode>(GetOuter());
}

USMGraphNode_StateMachineStateNode* USMGraph::GetOwningStateMachineNodeWhenNested() const
{
	return Cast<USMGraphNode_StateMachineStateNode>(GetOuter());
}

FSMNode_Base* USMGraph::GetRuntimeNode() const
{
	if (GeneratedContainerNode)
	{
		return GeneratedContainerNode->GetRunTimeNode();
	}
	
	if (!EntryNode)
	{
		return nullptr;
	}

	return &EntryNode->StateMachineNode;
}

bool USMGraph::HasAnyLogicConnections() const
{
	if (!EntryNode)
	{
		return false;
	}

	return EntryNode->GetOutputNode() != nullptr;
}

bool USMGraph::Modify(bool bAlwaysMarkDirty /*= true*/)
{
	bool Rtn = Super::Modify(bAlwaysMarkDirty);

	USMGraphK2Node_StateMachineNode* StateMachineNode = GetOwningStateMachineK2Node();

	if (StateMachineNode)
	{
		StateMachineNode->Modify();
	}

	for (int32 i = 0; i < Nodes.Num(); ++i)
	{
		Nodes[i]->Modify();
	}

	return Rtn;
}

void USMGraph::PostEditUndo()
{
	Super::PostEditUndo();

	if (IsValid(this))
	{
		// If the document is opened when this is called and the graph creation is being undone we will crash.
		NotifyGraphChanged();
	}
}

void USMGraph::NotifyGraphChanged()
{
	Super::NotifyGraphChanged();
}

void USMGraph::NotifyGraphChanged(const FEdGraphEditAction& Action)
{
	Super::NotifyGraphChanged(Action);

	// HACK around UE4 behavior that can't be overridden and also impacts animation graphs.
	// Look for invalid nodes that were placed. This can happen if a k2 function was drag dropped onto the graph.
	if (Action.Action == GRAPHACTION_AddNode)
	{
		UBlueprint* Blueprint = FSMBlueprintEditorUtils::FindBlueprintForGraphChecked(this);

		bool bRenamed = false;
		for (const UEdGraphNode* Node : Action.Nodes)
		{
			if (UK2Node* K2Node = const_cast<UK2Node*>(Cast<UK2Node>(Node)))
			{
				// Function nodes have terrible handling by UE on their drop behavior.
				// They do not belong in this graph but assume it is a k2 graph and cast check
				// their schema to k2 which obviously fails since this isn't a k2 graph.
				if (UK2Node_CallFunction* FunctionNode = Cast<UK2Node_CallFunction>(K2Node))
				{
					for (const TObjectPtr<UEdGraphNode>& OurNode : Nodes)
					{
						if (const USMGraphNode_StateNodeBase* StateNode = Cast<USMGraphNode_StateNodeBase>(OurNode))
						{
							// Attempt to forward this off to a property node in case the user is trying to drop it there.
							// If we're over a pin value it doesn't always drop correctly.
							if (const USMGraphK2Node_PropertyNode_Base* PropertyNode = StateNode->GetPropertyNodeUnderMouse())
							{
								USMPropertyGraph* PropertyGraph = PropertyNode->GetPropertyGraph();
								const_cast<FEdGraphEditAction&>(Action).Graph = PropertyGraph;
								K2Node->Rename(nullptr, PropertyGraph,
									REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
								bRenamed = true;
								PropertyGraph->AddNode(K2Node);
								break;
							}
						}
					}
				}
				if (!bRenamed)
				{
					// If the graph wasn't reassigned we have to rename it to a k2 graph. We're just choosing the top level sm graph
					// because we know it's k2. Even deleting the node UE drop handling will attempt to auto wire it and castcheck
					// the schema to k2 which will crash otherwise.
					K2Node->Rename(nullptr, FSMBlueprintEditorUtils::GetTopLevelStateMachineGraph(Blueprint),
						REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
					// RF_Transient specifically fixes a crash on 4.24 when changing a property on the function AFTER it has been deleted...
					K2Node->SetFlags(RF_Transient);
				}
				// Always remove the node from this graph. It does not belong here.
				RemoveNode(K2Node);
			}
		}
	}
}
