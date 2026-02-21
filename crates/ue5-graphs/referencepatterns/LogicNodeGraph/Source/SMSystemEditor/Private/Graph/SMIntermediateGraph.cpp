// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Graph/SMIntermediateGraph.h"

#include "Graph/Nodes/RootNodes/SMGraphK2Node_IntermediateEntryNode.h"

USMIntermediateGraph::USMIntermediateGraph(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), IntermediateEntryNode(nullptr)
{
}

FSMNode_Base* USMIntermediateGraph::GetRuntimeNode() const
{
	return IntermediateEntryNode ? IntermediateEntryNode->GetRunTimeNode() : nullptr;
}
