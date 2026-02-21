// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Graph/Nodes/SMGraphNode_StateMachineParentNode.h"

#include "Configuration/SMEditorSettings.h"
#include "Graph/SMGraph.h"
#include "Graph/SMGraphK2.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "Kismet2/KismetEditorUtilities.h"

#define LOCTEXT_NAMESPACE "SMGraphStateMachineParentNode"

USMGraphNode_StateMachineParentNode::USMGraphNode_StateMachineParentNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), ExpandedGraph(nullptr)
{
}

void USMGraphNode_StateMachineParentNode::PostPlacedNewNode()
{
	SetParentIfNull();
	Super::PostPlacedNewNode();
}

UObject* USMGraphNode_StateMachineParentNode::GetJumpTargetForDoubleClick() const
{
	if (ParentClass.Get())
	{
		if (UBlueprint* Blueprint = UBlueprint::GetBlueprintFromClass(ParentClass.Get()))
		{
			// Only lookup the immediate graph of the blueprint.
			if (UObject* Target = FSMBlueprintEditorUtils::GetRootStateMachineGraph(Blueprint, false))
			{
				return Target;
			}
			// The graph doesn't exist, let's just return the top level one instead and leave it to the user to figure out.
			return FSMBlueprintEditorUtils::GetTopLevelStateMachineGraph(Blueprint);
		}
	}

	return nullptr;
}

void USMGraphNode_StateMachineParentNode::JumpToDefinition() const
{
	if (UObject* HyperlinkTarget = GetJumpTargetForDoubleClick())
	{
		// Automatically set the debug object. Parent debug instances are the same as the child.
		if (UBlueprint* Blueprint = FSMBlueprintEditorUtils::FindBlueprintForNode(this))
		{
			if (USMInstance* CurrentDebugObject = Cast<USMInstance>(Blueprint->GetObjectBeingDebugged()))
			{
				UBlueprint* OtherBlueprint = FSMBlueprintEditorUtils::FindBlueprintForGraph(Cast<UEdGraph>(HyperlinkTarget));
				
				if (OtherBlueprint && Blueprint != OtherBlueprint)
				{
					OtherBlueprint->SetObjectBeingDebugged(CurrentDebugObject);
				}
			}
		}

		FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(HyperlinkTarget);
	}
}

void USMGraphNode_StateMachineParentNode::CreateBoundGraph()
{
	DesiredNodeName = ParentClass.Get() ? ParentClass->GetName() : "Parent State Machine";
	DesiredNodeName.RemoveFromEnd("_C");
	Super::CreateBoundGraph();
}

void USMGraphNode_StateMachineParentNode::UpdateEditState()
{
	if (BoundGraph)
	{
		BoundGraph->bEditable = false;
	}
}

void USMGraphNode_StateMachineParentNode::SetParentIfNull()
{
	if (!ParentClass.Get())
	{
		UBlueprint* Blueprint = FSMBlueprintEditorUtils::FindBlueprintForNodeChecked(this);
		ParentClass = Blueprint->ParentClass;
	}
}

TSet<USMGraph*> USMGraphNode_StateMachineParentNode::GetAllNestedExpandedParents() const
{
	TSet<USMGraph*> Graphs;
	
	if (!ExpandedGraph)
	{
		return Graphs;
	}

	Graphs.Add(ExpandedGraph);

	TArray<USMGraphNode_StateMachineParentNode*> Nodes;
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphNode_StateMachineParentNode>(ExpandedGraph, Nodes);
	for (USMGraphNode_StateMachineParentNode* Node : Nodes)
	{
		Graphs.Append(Node->GetAllNestedExpandedParents());
	}
	
	return Graphs;
}

FLinearColor USMGraphNode_StateMachineParentNode::Internal_GetBackgroundColor() const
{
	const USMEditorSettings* Settings = FSMBlueprintEditorUtils::GetEditorSettings();
	const FLinearColor ColorModifier(0.9f, 0.9f, 0.9f, 0.5f);
	const FLinearColor DefaultColor = Settings->StateMachineParentDefaultColor;
	
	if (IsEndState())
	{
		FLinearColor EndStateColor = Settings->EndStateColor;
		if (EndStateColor.R < 0.2)
		{
			EndStateColor.R = 0.2;
		}
		if (EndStateColor.G < 0.2)
		{
			EndStateColor.G = 0.2;
		}
		if (EndStateColor.B < 0.2)
		{
			EndStateColor.B = 0.2;
		}
		
		return EndStateColor * ColorModifier * DefaultColor;
	}

	// No input -- node unreachable.
	if (!HasInputConnections())
	{
		return DefaultColor * ColorModifier;
	}
	
	return DefaultColor * ColorModifier;
}

#undef LOCTEXT_NAMESPACE
