// © 2025 mrbaconvn. All Rights Reserved.


#include "Graph/ComboGraphSchema.h" 
#include "Graph/Node/ComboNodeActions.h" 
#include "GraphData/ComboGraphData.h"
#include "Graph/Node/ComboExecutionNode.h"
#include "Graph/Node/ComboConditionNode.h"
#include "Graph/Node/ComboInputNode.h"
#include "Graph/Node/ComboRootNode.h"
#include "Graph/Node/ComboResetNode.h"
#include "Graph/Node/ComboPortalNode.h"
#include "Graph/Node/ComboPortalJumpNode.h"
#include "GraphData/NodeData/ComboConditionNodeData.h"
#include "GraphData/NodeData/ComboExecutionNodeData.h"
#include "GraphData/NodeData/ComboInputNodeData.h"
#include "GraphData/NodeData/ComboJumpNodeData.h"
#include "GraphData/NodeData/ComboPortalNodeData.h"
#include "GraphData/NodeData/ComboResetNodeData.h"
#include "GraphData/NodeData/ComboRootNodeData.h"

#define LOCTEXT_NAMESPACE "ComboGraphSchema"

#define NEW_COMBO_NODE_CATEGORY LOCTEXT("Action.NewComboExecutionNode.Category", "Combo Nodes")
#define PIN_NULL_ERROR_MSG LOCTEXT("Pin.NullPin", "One or both pins are null")
#define PIN_SAME_ERROR_MSG LOCTEXT("Pin.SameNode", "Both are on the same node")
#define PIN_DIRECTION_ERROR_MSG LOCTEXT("Pin.WrongDirection", "Wrong pin direction")
#define PIN_CONNECT_SUCCESS_MSG LOCTEXT("Pin.Connect", "Make connection")

UComboGraphSchema::UComboGraphSchema()
{
	BlacklistNodeCreation.Add(UComboRootNodeData::StaticClass()); 

	NodeDataToEditorNodeMap.Add(UComboExecutionNodeData::StaticClass(), UComboExecutionNode::StaticClass());
	NodeDataToEditorNodeMap.Add(UComboConditionNodeData::StaticClass(), UComboConditionNode::StaticClass());
	NodeDataToEditorNodeMap.Add(UComboInputNodeData::StaticClass(), UComboInputNode::StaticClass());
	NodeDataToEditorNodeMap.Add(UComboRootNodeData::StaticClass(), UComboRootNode::StaticClass());
	NodeDataToEditorNodeMap.Add(UComboResetNodeData::StaticClass(), UComboResetNode::StaticClass());
	NodeDataToEditorNodeMap.Add(UComboPortalNodeData::StaticClass(), UComboPortalNode::StaticClass());
	NodeDataToEditorNodeMap.Add(UComboJumpNodeData::StaticClass(), UComboPortalJumpNode::StaticClass());
}

bool UComboGraphSchema::SafeDeleteNodeFromGraph(UEdGraph* Graph, UEdGraphNode* Node) const
{
	if(!IsValid(Graph) || !IsValid(Node))
	{
		return false;
	}

	if(!Node->CanUserDeleteNode())
	{
		return false;
	}
	
	Graph->Modify();
	Graph->RemoveNode(Node);
	return true;
}

void UComboGraphSchema::BreakNodeLinks(UEdGraphNode& TargetNode) const
{
	const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "GraphEd_BreakNodeLinks", "Break Node Links"));
	Super::BreakNodeLinks(TargetNode);
}

void UComboGraphSchema::BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotification) const
{
	const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "GraphEd_BreakPinLinks", "Break Pin Links"));
	Super::BreakPinLinks(TargetPin, bSendsNodeNotification);
}

void UComboGraphSchema::BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const
{
	const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "GraphEd_BreakSinglePinLink", "Break Pin Link"));
	Super::BreakSinglePinLink(SourcePin, TargetPin);
}

void UComboGraphSchema::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	TArray<TSubclassOf<UComboGraphNodeData>> NodeClasses;
	NodeDataToEditorNodeMap.GetKeys(NodeClasses);
	
	for (int i = 0; i < NodeClasses.Num(); i++)
	{
		TSubclassOf<UComboGraphNodeData> NodeClass = NodeClasses[i];
		TSubclassOf<UComboNodeBase> EditorNodeClass = NodeDataToEditorNodeMap[NodeClass];
		if (!IsValid(NodeClass) || !IsValid(EditorNodeClass) || BlacklistNodeCreation.Contains(NodeClass))
		{
			continue;
		}
		
		ContextMenuBuilder.AddAction(FNewComboNodeAction::GetNewNodeAction(NodeClass, NEW_COMBO_NODE_CATEGORY, EditorNodeClass->GetDefaultObject<UComboNodeBase>()->GetNodeCreationText()));
	}
}

const FPinConnectionResponse UComboGraphSchema::CanCreateConnection(const UEdGraphPin* PinA, const UEdGraphPin* PinB) const
{
	if (PinA == nullptr || PinB == nullptr)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, PIN_NULL_ERROR_MSG);
	}

	if (PinA->GetOwningNode() == PinB->GetOwningNode())
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, PIN_SAME_ERROR_MSG);
	}

	if (PinA->Direction == PinB->Direction)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, PIN_DIRECTION_ERROR_MSG);
	}

	return FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_A, PIN_CONNECT_SUCCESS_MSG);
}

void UComboGraphSchema::CreateDefaultNodesForGraph(UEdGraph& Graph) const
{
	CreateAndInitComboNode(UComboRootNodeData::StaticClass(), &Graph);
}

UComboNodeBase* UComboGraphSchema::CreateAndInitComboNode(TSubclassOf<UComboGraphNodeData> NodeDataClass, UEdGraph* Graph) const
{
	UComboNodeBase* NewNode = CreateComboNode(NodeDataClass, Graph);
	if (!IsValid(NewNode))
	{
		return nullptr;
	}

	const FScopedTransaction Transaction(LOCTEXT("CreateNewNode", "Create New Combo Node")); 
	NewNode->CreateDefaultPins();
	NewNode->InitNodeData(NewNode);
	
	Graph->Modify();
	Graph->AddNode(NewNode, true, true); 

	return NewNode;
}

UComboNodeBase* UComboGraphSchema::CreateComboNode(TSubclassOf<UComboGraphNodeData> NodeDataClass, UEdGraph* Graph) const
{
	const TSubclassOf<UComboNodeBase>* NodeClass = NodeDataToEditorNodeMap.Find(NodeDataClass);
	if (NodeClass == nullptr)
	{
		return nullptr;
	}

	UComboNodeBase* NewNode = NewObject<UComboNodeBase>(Graph, *NodeClass);
	NewNode->InitNodeData(NewNode);
	NewNode->SetFlags(RF_Transactional);

	return NewNode;
}


#undef LOCTEXT_NAMESPACE