// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMIntermediateGraphSchema.h"

#include "Graph/Nodes/FunctionNodes/SMGraphK2Node_FunctionNodes_StateMachineRef.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_IntermediateEntryNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_StateEndNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_StateUpdateNode.h"
#include "Graph/Nodes/SMGraphNode_StateNodeBase.h"
#include "Graph/SMIntermediateGraph.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "SMInstance.h"

#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"

#define LOCTEXT_NAMESPACE "SMStateGraphSchema"

USMIntermediateGraphSchema::USMIntermediateGraphSchema(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USMIntermediateGraphSchema::CreateDefaultNodesForGraph(UEdGraph& Graph) const
{
	// Create the EntryNode which is also the runtime node container.
	FGraphNodeCreator<USMGraphK2Node_IntermediateEntryNode> StartEntryNodeCreator(Graph);
	USMGraphK2Node_IntermediateEntryNode* StartEntryNode = StartEntryNodeCreator.CreateNode();
	{
		StartEntryNodeCreator.Finalize();
		SetNodeMetaData(StartEntryNode, FNodeMetadata::DefaultGraphNode);

		USMIntermediateGraph* TypedGraph = CastChecked<USMIntermediateGraph>(&Graph);
		TypedGraph->IntermediateEntryNode = StartEntryNode;

		// Create start state machine node.
		FGraphNodeCreator<USMGraphK2Node_StateMachineRef_Start> StartNodeCreator(Graph);
		USMGraphK2Node_StateMachineRef_Start* StartNode = StartNodeCreator.CreateNode();
		StartNode->NodePosX = 600;
		StartNodeCreator.Finalize();
		SetNodeMetaData(StartNode, FNodeMetadata::DefaultGraphNode);

		UFunction* GetContextFunction = USMInstance::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMInstance, GetContext));
		UK2Node_CallFunction* ContextFunctionNode = FSMBlueprintEditorUtils::CreateFunctionCall(&Graph, GetContextFunction);
		ContextFunctionNode->NodePosX = 300;
		ContextFunctionNode->NodePosY = 80;

		// Wire entry to start state.
		TryCreateConnection(StartEntryNode->GetOutputPin(), StartNode->GetExecPin());
		// Wire context to start state.
		TryCreateConnection(ContextFunctionNode->GetReturnValuePin(), StartNode->FindPin(FName("Context")));
	}
	
	{
		// Create the update entry point.
		FGraphNodeCreator<USMGraphK2Node_StateUpdateNode> UpdateEntryNodeCreator(Graph);
		USMGraphK2Node_StateUpdateNode* UpdateEntryNode = UpdateEntryNodeCreator.CreateNode();
		UpdateEntryNode->RuntimeNodeGuid = StartEntryNode->GetRunTimeNodeChecked()->GetNodeGuid();
		UpdateEntryNode->NodePosY = 250;

		UpdateEntryNodeCreator.Finalize();
		SetNodeMetaData(UpdateEntryNode, FNodeMetadata::DefaultGraphNode);

		// Create update state machine node.
		FGraphNodeCreator<USMGraphK2Node_StateMachineRef_Update> UpdateNodeCreator(Graph);
		USMGraphK2Node_StateMachineRef_Update* UpdateNode = UpdateNodeCreator.CreateNode();
		UpdateNode->NodePosX = 600;
		UpdateNode->NodePosY = 250;
		UpdateNodeCreator.Finalize();
		SetNodeMetaData(UpdateNode, FNodeMetadata::DefaultGraphNode);

		// Wire entry to start state.
		TryCreateConnection(UpdateEntryNode->GetOutputPin(), UpdateNode->GetExecPin());
		// Wire delta seconds.
		TryCreateConnection(UpdateEntryNode->FindPin(FName("DeltaSeconds")), UpdateNode->FindPin(FName("DeltaSeconds")));
	}

	{
		// Create the end state entry point.
		FGraphNodeCreator<USMGraphK2Node_StateEndNode> EndEntryNodeCreator(Graph);
		USMGraphK2Node_StateEndNode* EndEntryNode = EndEntryNodeCreator.CreateNode();
		EndEntryNode->RuntimeNodeGuid = StartEntryNode->GetRunTimeNodeChecked()->GetNodeGuid();
		EndEntryNode->NodePosY = 500;

		EndEntryNodeCreator.Finalize();
		SetNodeMetaData(EndEntryNode, FNodeMetadata::DefaultGraphNode);

		// Create stop state machine node.
		FGraphNodeCreator<USMGraphK2Node_StateMachineRef_Stop> StopNodeCreator(Graph);
		USMGraphK2Node_StateMachineRef_Stop* StopNode = StopNodeCreator.CreateNode();
		StopNode->NodePosX = 600;
		StopNode->NodePosY = 500;
		StopNodeCreator.Finalize();
		SetNodeMetaData(StopNode, FNodeMetadata::DefaultGraphNode);

		// Wire entry to start state.
		TryCreateConnection(EndEntryNode->GetOutputPin(), StopNode->GetExecPin());
	}
}

void USMIntermediateGraphSchema::GetGraphDisplayInformation(const UEdGraph& Graph, /*out*/ FGraphDisplayInfo& DisplayInfo) const
{
	Super::GetGraphDisplayInformation(Graph, DisplayInfo);
	DisplayInfo.PlainName = FText::FromString(Graph.GetName());

	if (const USMGraphNode_StateNodeBase* StateNode = Cast<const USMGraphNode_StateNodeBase>(Graph.GetOuter()))
	{
		DisplayInfo.PlainName = FText::Format(LOCTEXT("StateNameGraphTitle", "{0} (intermediate reference)"), FText::FromString(StateNode->GetStateName()));
	}

	DisplayInfo.DisplayName = DisplayInfo.PlainName;
	DisplayInfo.Tooltip = DisplayInfo.PlainName;
}

void USMIntermediateGraphSchema::HandleGraphBeingDeleted(UEdGraph& GraphBeingRemoved) const
{
	if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(&GraphBeingRemoved))
	{
		if (USMIntermediateGraph* StateGraph = Cast<USMIntermediateGraph>(&GraphBeingRemoved))
		{
			if (USMGraphNode_StateNodeBase* StateNode = StateGraph->GetOwningStateNode())
			{
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
