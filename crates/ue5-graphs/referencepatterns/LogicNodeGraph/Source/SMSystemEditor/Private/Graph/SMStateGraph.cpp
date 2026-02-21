// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Graph/SMStateGraph.h"

#include "Graph/Nodes/RootNodes/SMGraphK2Node_StateEntryNode.h"
#include "Graph/Nodes/SMGraphNode_StateNodeBase.h"

USMStateGraph::USMStateGraph(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), EntryNode(nullptr)
{
}

USMGraphNode_StateNodeBase* USMStateGraph::GetOwningStateNode() const
{
	return Cast<USMGraphNode_StateNodeBase>(GetOuter());
}

FSMNode_Base* USMStateGraph::GetRuntimeNode() const
{
	return EntryNode ? EntryNode->GetRunTimeNode() : nullptr;
}
