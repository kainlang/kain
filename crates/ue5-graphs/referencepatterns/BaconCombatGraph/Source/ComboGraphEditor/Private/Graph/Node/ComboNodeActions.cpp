// © 2025 mrbaconvn. All Rights Reserved.


#include "Graph/Node/ComboNodeActions.h"
#include "Graph/ComboGraphSchema.h"
#include "Graph/Node/ComboNodeBase.h"

UEdGraphNode* FNewComboNodeAction::PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode)
{
	if (!IsValid(ParentGraph))
	{
		return nullptr;
	}

	const UComboGraphSchema* Schema = Cast<UComboGraphSchema>(ParentGraph->GetSchema());
	if (!IsValid(Schema))
	{
		return nullptr;
	}

	UComboNodeBase* NewComboNode = Schema->CreateAndInitComboNode(NodeClass, ParentGraph);
	if (!IsValid(NewComboNode))
	{
		return nullptr;
	}

	NewComboNode->NodePosX = Location.X;
	NewComboNode->NodePosY = Location.Y; 
	NewComboNode->InitPinConnection(FromPin); 

	return NewComboNode;
}

TSharedPtr<FNewComboNodeAction> FNewComboNodeAction::GetNewNodeAction(TSubclassOf<UComboGraphNodeData> NodeClass, FText Category, FText Description)
{
	TSharedPtr<FNewComboNodeAction> NewNodeAction(new FNewComboNodeAction(
		Category,
		Description,
		Description,
		0
	));
	NewNodeAction->NodeClass = NodeClass;
	return NewNodeAction;
}