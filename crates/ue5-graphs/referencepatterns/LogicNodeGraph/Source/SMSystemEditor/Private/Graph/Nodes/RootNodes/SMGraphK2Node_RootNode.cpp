// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Graph/Nodes/RootNodes/SMGraphK2Node_RootNode.h"

#include "Utilities/SMBlueprintEditorUtils.h"
#include "Configuration/SMEditorStyle.h"

#include "Textures/SlateIcon.h"

#define LOCTEXT_NAMESPACE "SMRootNode"

USMGraphK2Node_RootNode::USMGraphK2Node_RootNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCanRenameNode = false;
	bIsBeingDestroyed = false;
	bAllowMoreThanOneNode = false;
}

void USMGraphK2Node_RootNode::PostPasteNode()
{
	if (bAllowMoreThanOneNode)
	{
		Super::PostPasteNode();
		return;
	}
	
	// Look up root nodes matching this exact node.
	TArray<USMGraphK2Node_RootNode*> RootNodeList;

	FBlueprintEditorUtils::GetAllNodesOfClass<USMGraphK2Node_RootNode>(GetBlueprint(), RootNodeList);

	// This node can't exist more than once. If it does destroy this node.
	if (RootNodeList.ContainsByPredicate([&](const USMGraphK2Node_RootNode* Container)
	{
		return Container != this && Container->GetUniqueID() == GetUniqueID();
	}
	))
	{
		DestroyNode();
	}
	else
	{
		Super::PostPasteNode();
	}
}

void USMGraphK2Node_RootNode::DestroyNode()
{
	bIsBeingDestroyed = true;
	Super::DestroyNode();
}

void USMGraphK2Node_RootNode::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
}

FLinearColor USMGraphK2Node_RootNode::GetNodeTitleColor() const
{
	return FLinearColor::Gray;
}

FSlateIcon USMGraphK2Node_RootNode::GetIconAndTint(FLinearColor& OutColor) const
{
	Super::GetIconAndTint(OutColor);

	static FSlateIcon Icon(FSMEditorStyle::GetStyleSetName(), "ClassIcon.SMBlueprint");
	return Icon;
}

#undef LOCTEXT_NAMESPACE
