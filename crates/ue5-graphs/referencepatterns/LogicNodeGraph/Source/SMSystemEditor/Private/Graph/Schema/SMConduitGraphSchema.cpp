// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMConduitGraphSchema.h"

#include "Graph/Nodes/RootNodes/SMGraphK2Node_ConduitResultNode.h"
#include "Graph/Nodes/SMGraphNode_ConduitNode.h"
#include "Graph/Nodes/SMGraphNode_StateNodeBase.h"
#include "Graph/SMConduitGraph.h"

#include "Kismet2/BlueprintEditorUtils.h"

#define LOCTEXT_NAMESPACE "SMConduitGraphSchema"

USMConduitGraphSchema::USMConduitGraphSchema(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USMConduitGraphSchema::CreateDefaultNodesForGraph(UEdGraph& Graph) const
{
	// Create the ResultNode which is also the runtime node container.
	FGraphNodeCreator<USMGraphK2Node_ConduitResultNode> NodeCreator(Graph);
	USMGraphK2Node_ConduitResultNode* ResultNode = NodeCreator.CreateNode();
	
	NodeCreator.Finalize();
	SetNodeMetaData(ResultNode, FNodeMetadata::DefaultGraphNode);

	USMConduitGraph* TypedGraph = CastChecked<USMConduitGraph>(&Graph);
	TypedGraph->ResultNode = ResultNode;
}

void USMConduitGraphSchema::GetGraphDisplayInformation(const UEdGraph& Graph, /*out*/ FGraphDisplayInfo& DisplayInfo) const
{
	DisplayInfo.PlainName = FText::FromString(Graph.GetName());

	if (const USMGraphNode_ConduitNode* ConduitNode = Cast<const USMGraphNode_ConduitNode>(Graph.GetOuter()))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("NodeTitle"), ConduitNode->GetNodeTitle(ENodeTitleType::FullTitle));

		DisplayInfo.PlainName = FText::Format(LOCTEXT("ConduitGraphTitle", "{NodeTitle} (conduit)"), Args);
	}

	DisplayInfo.DisplayName = DisplayInfo.PlainName;
	DisplayInfo.Tooltip = DisplayInfo.PlainName;
}

void USMConduitGraphSchema::HandleGraphBeingDeleted(UEdGraph& GraphBeingRemoved) const
{
	if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(&GraphBeingRemoved))
	{
		if (USMConduitGraph* ConduitGraph = Cast<USMConduitGraph>(&GraphBeingRemoved))
		{
			if (USMGraphNode_ConduitNode* ConduitNode = Cast<USMGraphNode_ConduitNode>(ConduitGraph->GetOuter()))
			{
				// Let the node delete first-- it will trigger graph removal. Helps with undo buffer transaction.
				if (ConduitNode->GetBoundGraph())
				{
					FBlueprintEditorUtils::RemoveNode(Blueprint, ConduitNode, true);
					return;
				}

				// Remove this graph from the parent graph.
				UEdGraph* ParentGraph = ConduitNode->GetGraph();
				ParentGraph->Modify();
				ParentGraph->SubGraphs.Remove(ConduitGraph);
			}
		}
	}

	// Skip transition schema.
	USMGraphK2Schema::HandleGraphBeingDeleted(GraphBeingRemoved);
}

#undef LOCTEXT_NAMESPACE
