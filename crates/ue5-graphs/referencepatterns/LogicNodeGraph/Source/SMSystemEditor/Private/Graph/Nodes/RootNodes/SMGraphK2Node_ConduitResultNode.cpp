// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Graph/Nodes/RootNodes/SMGraphK2Node_ConduitResultNode.h"

#include "Graph/Schema/SMConduitGraphSchema.h"
#include "Graph/SMConduitGraph.h"

#include "GraphEditorSettings.h"

#define LOCTEXT_NAMESPACE "SMConduitResultNode"

USMGraphK2Node_ConduitResultNode::USMGraphK2Node_ConduitResultNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ConduitNode.GenerateNewNodeGuidIfNotSet();
}

void USMGraphK2Node_ConduitResultNode::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, USMConduitGraphSchema::PC_Boolean, FName("bCanEnterTransition"));
}

FLinearColor USMGraphK2Node_ConduitResultNode::GetNodeTitleColor() const
{
	return GetDefault<UGraphEditorSettings>()->ResultNodeTitleColor;
}

FText USMGraphK2Node_ConduitResultNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Result", "Result");
}

FText USMGraphK2Node_ConduitResultNode::GetTooltipText() const
{
	return LOCTEXT("TransitionToolTip", "This node is evaluated to determine if the conduit will be activated.");
}

bool USMGraphK2Node_ConduitResultNode::IsCompatibleWithGraph(UEdGraph const* Graph) const
{
	return Graph->IsA<USMConduitGraph>();
}

#undef LOCTEXT_NAMESPACE
