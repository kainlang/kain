// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMTextPropertyGraphSchema.h"

#include "Graph/Nodes/PropertyNodes/SMGraphK2Node_TextPropertyNode.h"
#include "Graph/SMTextPropertyGraph.h"

#include "Utilities/SMBlueprintEditorUtils.h"

#include "K2Node_FormatText.h"

#define LOCTEXT_NAMESPACE "SMTextPropertyGraphSchema"

USMTextPropertyGraphSchema::USMTextPropertyGraphSchema(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USMTextPropertyGraphSchema::CreateDefaultNodesForGraph(UEdGraph& Graph) const
{
	TArray<USMGraphK2Node_TextPropertyNode*> ExistingPropertyNodes;
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphK2Node_TextPropertyNode>(&Graph, ExistingPropertyNodes);

	check(ExistingPropertyNodes.Num() <= 1);
	
	// Create a format text node.
	FGraphNodeCreator<UK2Node_FormatText> FormatNodeCreator(Graph);
	UK2Node_FormatText* FormatNode = FormatNodeCreator.CreateNode();
	FormatNode->SetFlags(RF_Transactional);
	FormatNode->NodePosX = 100;
	FormatNode->NodePosY = 100;

	FormatNodeCreator.Finalize();
	SetNodeMetaData(FormatNode, FNodeMetadata::DefaultGraphNode);

	bool bNewGraph = true;
	
	// Either reuse or create a new result node.
	USMGraphK2Node_TextPropertyNode* ResultNode = nullptr;
	if (ExistingPropertyNodes.Num() == 1)
	{
		ResultNode = ExistingPropertyNodes[0];
		bNewGraph = false;
	}
	else
	{
		// Create the ResultNode which is also the runtime node container.
		FGraphNodeCreator<USMGraphK2Node_TextPropertyNode> NodeCreator(Graph);
		ResultNode = NodeCreator.CreateNode();
		ResultNode->SetFlags(RF_Transactional);
		ResultNode->NodePosX = 850;
		NodeCreator.Finalize();
		SetNodeMetaData(ResultNode, FNodeMetadata::DefaultGraphNode);
	}
	
	// Link the pins.
	{
		// Find the format node output pin.
		UEdGraphPin* FormatOutPin = nullptr;
		for (UEdGraphPin* Pin : FormatNode->GetAllPins())
		{
			if (Pin->Direction == EGPD_Output)
			{
				FormatOutPin = Pin;
				break;
			}
		}

		check(FormatOutPin);

		const bool bResult = Graph.GetSchema()->TryCreateConnection(FormatOutPin, ResultNode->GetInputPin());
		ensure(bResult);
	}
	
	USMTextPropertyGraph* PropertyGraph = CastChecked<USMTextPropertyGraph>(&Graph);
	PropertyGraph->ResultNode = ResultNode;
	PropertyGraph->FormatTextNode = FormatNode;

	PropertyGraph->SetUsingGraphToEdit(bNewGraph ? ResultNode->GetPropertyNodeConstChecked()->ShouldDefaultToEditMode() : PropertyGraph->IsGraphBeingUsedToEdit());
}

void USMTextPropertyGraphSchema::GetGraphDisplayInformation(const UEdGraph& Graph, FGraphDisplayInfo& DisplayInfo) const
{
	Super::GetGraphDisplayInformation(Graph, DisplayInfo);

	DisplayInfo.Tooltip = FText::FromName(Graph.GetFName());
	DisplayInfo.DocExcerptName = nullptr;
}

#undef LOCTEXT_NAMESPACE
