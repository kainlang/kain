// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Graph/Schema/SMPropertyGraphSchema.h"

#include "Graph/Nodes/PropertyNodes/SMGraphK2Node_GraphPropertyNode.h"
#include "Graph/SMPropertyGraph.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "Editor.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "SMPropertyGraphSchema"

USMPropertyGraphSchema::USMPropertyGraphSchema(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USMPropertyGraphSchema::CreateDefaultNodesForGraph(UEdGraph& Graph) const
{
	TArray<USMGraphK2Node_GraphPropertyNode*> ExistingPropertyNodes;
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphK2Node_GraphPropertyNode>(&Graph, ExistingPropertyNodes);

	check(ExistingPropertyNodes.Num() <= 1);

	bool bNewGraph = true;
	USMPropertyGraph* PropertyGraph = CastChecked<USMPropertyGraph>(&Graph);

	// Either reuse or create a new result node.
	USMGraphK2Node_GraphPropertyNode* ResultNode = nullptr;
	if (ExistingPropertyNodes.Num() == 1)
	{
		ResultNode = ExistingPropertyNodes[0];
		bNewGraph = false;
	}
	else
	{
		// Create the ResultNode which is also the runtime node container.
		FGraphNodeCreator<USMGraphK2Node_GraphPropertyNode> NodeCreator(Graph);
		ResultNode = NodeCreator.CreateNode();
		ResultNode->SetFlags(RF_Transactional);
		ResultNode->NodePosX = 850;
		NodeCreator.Finalize();
		SetNodeMetaData(ResultNode, FNodeMetadata::DefaultGraphNode);
	}

	PropertyGraph->ResultNode = ResultNode;
	PropertyGraph->SetUsingGraphToEdit(bNewGraph ? ResultNode->GetPropertyNodeConstChecked()->ShouldDefaultToEditMode() : PropertyGraph->IsGraphBeingUsedToEdit());
}

bool USMPropertyGraphSchema::CanDuplicateGraph(UEdGraph* InSourceGraph) const
{
	if (const USMPropertyGraph* PropertyGraph = Cast<USMPropertyGraph>(InSourceGraph))
	{
		return PropertyGraph->AllowsDuplication();
	}

	return false;
}

void USMPropertyGraphSchema::HandleGraphBeingDeleted(UEdGraph& GraphBeingRemoved) const
{
	if (USMPropertyGraph* PropertyGraph = Cast<USMPropertyGraph>(&GraphBeingRemoved))
	{
		PropertyGraph->OnGraphDeleted();
	}
	
	Super::HandleGraphBeingDeleted(GraphBeingRemoved);

	if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(&GraphBeingRemoved))
	{
		// These graphs can be deleted during a compile which will modify the blueprint causing dependencies to be out of date.
		FSMBlueprintEditorUtils::EnsureCachedDependenciesUpToDate(Blueprint);
	}
}

void USMPropertyGraphSchema::GetGraphDisplayInformation(const UEdGraph& Graph, FGraphDisplayInfo& DisplayInfo) const
{
	Super::GetGraphDisplayInformation(Graph, DisplayInfo);

	DisplayInfo.Tooltip = FText::FromName(Graph.GetFName());
	DisplayInfo.DocExcerptName = nullptr;
}

bool USMPropertyGraphSchema::TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const
{
	// Check if the graph is preventing a connection. This can be useful for drag drop operations the graph wants to cancel.
	if (USMPropertyGraph* PropertyGraph = Cast<USMPropertyGraph>(A->GetOwningNode()->GetGraph()))
	{
		if (PropertyGraph->PreventConnections.Contains(A) || PropertyGraph->PreventConnections.Contains(B))
		{
			PropertyGraph->PreventConnections.Empty();
			return false;
		}
	}
	
	return Super::TryCreateConnection(A, B);
}

#undef LOCTEXT_NAMESPACE
