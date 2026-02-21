// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMStateGraphSchema.h"

#include "Graph/SMStateGraph.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_StateEntryNode.h"
#include "Graph/Nodes/SMGraphNode_StateNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_StateUpdateNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_StateEndNode.h"

#include "Kismet2/BlueprintEditorUtils.h"

#define LOCTEXT_NAMESPACE "SMStateGraphSchema"

USMStateGraphSchema::USMStateGraphSchema(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USMStateGraphSchema::CreateDefaultNodesForGraph(UEdGraph& Graph) const
{
	// Create the EntryNode which is also the runtime node container.
	FGraphNodeCreator<USMGraphK2Node_StateEntryNode> NodeCreator(Graph);
	USMGraphK2Node_StateEntryNode* EntryNode = NodeCreator.CreateNode();
	
	NodeCreator.Finalize();
	SetNodeMetaData(EntryNode, FNodeMetadata::DefaultGraphNode);

	USMStateGraph* TypedGraph = CastChecked<USMStateGraph>(&Graph);
	TypedGraph->EntryNode = EntryNode;
	// Do not make containers ghost nodes or the state won't be compiled properly
	// and reference nodes won't be able to map properly.

	// Create the update entry point.
	FGraphNodeCreator<USMGraphK2Node_StateUpdateNode> UpdateNodeCreator(Graph);
	USMGraphK2Node_StateUpdateNode* UpdateNode = UpdateNodeCreator.CreateNode();
	UpdateNode->RuntimeNodeGuid = EntryNode->GetRunTimeNodeChecked()->GetNodeGuid();
	UpdateNode->NodePosY = 250;
	UpdateNode->MakeAutomaticallyPlacedGhostNode();

	UpdateNodeCreator.Finalize();
	SetNodeMetaData(UpdateNode, FNodeMetadata::DefaultGraphNode);

	// Create the end state entry point.
	FGraphNodeCreator<USMGraphK2Node_StateEndNode> EndNodeCreator(Graph);
	USMGraphK2Node_StateEndNode* EndNode = EndNodeCreator.CreateNode();
	EndNode->RuntimeNodeGuid = EntryNode->GetRunTimeNodeChecked()->GetNodeGuid();
	EndNode->NodePosY = 500;
	EndNode->MakeAutomaticallyPlacedGhostNode();

	EndNodeCreator.Finalize();
	SetNodeMetaData(EndNode, FNodeMetadata::DefaultGraphNode);
}

void USMStateGraphSchema::GetGraphDisplayInformation(const UEdGraph& Graph, /*out*/ FGraphDisplayInfo& DisplayInfo) const
{
	Super::GetGraphDisplayInformation(Graph, DisplayInfo);
	DisplayInfo.PlainName = FText::FromString(Graph.GetName());

	if (const USMGraphNode_StateNode* StateNode = Cast<const USMGraphNode_StateNode>(Graph.GetOuter()))
	{
		DisplayInfo.PlainName = FText::Format(LOCTEXT("StateNameGraphTitle", "{0} (state)"), FText::FromString(StateNode->GetStateName()));
	}

	DisplayInfo.DisplayName = DisplayInfo.PlainName;
	DisplayInfo.Tooltip = DisplayInfo.PlainName;
}

void USMStateGraphSchema::HandleGraphBeingDeleted(UEdGraph& GraphBeingRemoved) const
{
	if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(&GraphBeingRemoved))
	{
		if (USMStateGraph* StateGraph = Cast<USMStateGraph>(&GraphBeingRemoved))
		{
			if (USMGraphNode_StateNodeBase* StateNode = StateGraph->GetOwningStateNode())
			{
				// If UE4 creates a function graph based on this state graph (such as from CreateEvent node)
				// and user deletes the graph this will fail.

				// Let the node delete first-- it will trigger graph removal. Helps with undo buffer transaction.
				if (StateNode->GetBoundGraph() == &GraphBeingRemoved)
				{
					FBlueprintEditorUtils::RemoveNode(Blueprint, StateNode, true);
					return;
				}

				// Remove this graph from the parent graph.
				UEdGraph* ParentGraph = StateNode->GetGraph();
				ParentGraph->Modify();
				ParentGraph->SubGraphs.Remove(StateGraph);
			}
		}
	}

	Super::HandleGraphBeingDeleted(GraphBeingRemoved);
}

#undef LOCTEXT_NAMESPACE
