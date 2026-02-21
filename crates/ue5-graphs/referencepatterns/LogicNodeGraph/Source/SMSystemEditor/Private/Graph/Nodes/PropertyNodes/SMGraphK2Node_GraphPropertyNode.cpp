// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMGraphK2Node_GraphPropertyNode.h"

#include "Graph/Nodes/SlateNodes/Properties/SSMGraphProperty.h"
#include "Graph/SMPropertyGraph.h"

#define LOCTEXT_NAMESPACE "SMGraphPropertyNode"

USMGraphK2Node_GraphPropertyNode::USMGraphK2Node_GraphPropertyNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USMGraphK2Node_GraphPropertyNode::AllocateDefaultPins()
{
	USMPropertyGraph* PropertyGraph = CastChecked<USMPropertyGraph>(GetGraph());
	// The GraphProperty on the graph is set prior to this property container being set, so for new node creation use that.
	// It's not a UPROPERTY though so it won't persist on reload which is why reading the stored one on this property is needed.
	FSMGraphProperty_Base* Prop = GetPropertyNode();
	if (!Prop || Prop->VariableName.IsNone())
	{
		Prop = PropertyGraph->TempGraphProperty;
	}
	
	check(Prop);
	
	UEdGraphPin* GraphPin = CreatePin(EGPD_Input, Prop->VariableType, Prop->VariableName);
	
	const bool bIsReadOnly = Prop->IsVariableReadOnly();
	GraphPin->bNotConnectable = bIsReadOnly;
	if (bIsReadOnly)
	{
		// Only set default read only value if we are forcing it to read only.
		GraphPin->bDefaultValueIsReadOnly = true;
	}
}

void USMGraphK2Node_GraphPropertyNode::ConfigureRuntimePropertyNode()
{
	RuntimeGraphProperty.GraphEvaluator = GraphProperty.GraphEvaluator;
	RuntimeGraphProperty.SetGuid(GraphProperty.GetGuid());
	RuntimeGraphProperty.SetOwnerGuid(GraphProperty.GetOwnerGuid());
	if (const FSMGraphProperty_Base* Prop = GetPropertyNode())
	{
		if (const UEdGraphPin* Pin = FindPin(Prop->VariableName, EGPD_Input))
		{
			const bool bIsDefaultValue = Pin->LinkedTo.Num() == 0;
			GraphProperty.SetIsDefaultValueOnly(bIsDefaultValue);
		}
	}
}

FSMGraphProperty_Base_Runtime* USMGraphK2Node_GraphPropertyNode::GetRuntimePropertyNode()
{
	return &RuntimeGraphProperty;
}

TSharedPtr<SSMGraphProperty_Base> USMGraphK2Node_GraphPropertyNode::GetGraphNodeWidget() const
{
	return SNew(SSMGraphProperty)
		.GraphNode(const_cast<USMGraphK2Node_GraphPropertyNode*>(this))
		.WidgetInfo(&GraphProperty.WidgetInfo);
}

void USMGraphK2Node_GraphPropertyNode::DefaultPropertyActionWhenPlaced(TSharedPtr<SWidget> Widget)
{

}

#undef LOCTEXT_NAMESPACE
