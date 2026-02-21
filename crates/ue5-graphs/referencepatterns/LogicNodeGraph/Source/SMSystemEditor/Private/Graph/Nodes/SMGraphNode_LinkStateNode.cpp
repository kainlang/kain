// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Graph/Nodes/SMGraphNode_LinkStateNode.h"

#include "Compilers/SMKismetCompiler.h"
#include "Configuration/SMEditorStyle.h"
#include "Graph/SMGraph.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#define LOCTEXT_NAMESPACE "SMGraphLinkStateNode"

USMGraphNode_LinkStateNode::USMGraphNode_LinkStateNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), LinkedState(nullptr)
{
#if WITH_EDITORONLY_DATA
	bCanRenameNode = false;
#endif
}

void USMGraphNode_LinkStateNode::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, TEXT("Transition"), TEXT("In"));
}

void USMGraphNode_LinkStateNode::PostPlacedNewNode()
{
	// Skip state base so we don't create a graph.
	USMGraphNode_Base::PostPlacedNewNode();
}

void USMGraphNode_LinkStateNode::PostPasteNode()
{
	// Skip state because it relies on a graph being present.
	USMGraphNode_Base::PostPasteNode();

	LinkToState(LinkedStateName);
}

void USMGraphNode_LinkStateNode::DestroyNode()
{
	Super::DestroyNode();

	LinkToState(FString());
}

void USMGraphNode_LinkStateNode::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	bPostEditChangeConstructionRequiresFullRefresh = false;
	Super::PostEditChangeProperty(PropertyChangedEvent);
	bPostEditChangeConstructionRequiresFullRefresh = true;

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(USMGraphNode_LinkStateNode, LinkedStateName))
	{
		LinkToState(LinkedStateName);
	}
}

FText USMGraphNode_LinkStateNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FText::FromString(GetStateName());
}

void USMGraphNode_LinkStateNode::OnRenameNode(const FString& NewName)
{
}

UObject* USMGraphNode_LinkStateNode::GetJumpTargetForDoubleClick() const
{
	return LinkedState;
}

FSlateIcon USMGraphNode_LinkStateNode::GetIconAndTint(FLinearColor& OutColor) const
{
	return FSlateIcon(FSMEditorStyle::GetStyleSetName(), TEXT("SMGraph.LinkState"));
}

void USMGraphNode_LinkStateNode::PreCompile(FSMKismetCompilerContext& CompilerContext)
{
	Super::PreCompile(CompilerContext);

	if (LinkedStateName.IsEmpty())
	{
		LinkedState = nullptr;
		if (!HasInputConnections())
		{
			// Only throw a warning if there isn't any inbound transitions since this won't matter.
			// An error will be thrown later if there are connections.
			CompilerContext.MessageLog.Warning(TEXT("No state linked for node: @@."), this);
			return;
		}
	}
	else if (LinkedState == nullptr)
	{
		// Attempt to relink. Maybe the state was added back in to the graph.
		LinkToState(LinkedStateName);
	}

	bool bStateFound = false;
	if (LinkedState)
	{
		TArray<USMGraphNode_StateNodeBase*> States;
		GetAvailableStatesToLink(States);
		bStateFound = States.Contains(LinkedState);
		if (bStateFound)
		{
			// Update the name in case the state was renamed.
			LinkedStateName = LinkedState->GetStateName();

			// Verify this state is linked on the target.
			LinkedState->LinkStates.Add(this);
		}
		else
		{
			LinkedState = nullptr;
		}
	}

	if (!bStateFound && GetLinkedStateFromName(LinkedStateName) == nullptr)
	{
		CompilerContext.MessageLog.Error(TEXT("Invalid state linked for node: @@."), this);
	}
}

const FSlateBrush* USMGraphNode_LinkStateNode::GetNodeIcon() const
{
	if (LinkedState)
	{
		return LinkedState->GetNodeIcon();
	}

	return FSMEditorStyle::Get()->GetBrush(TEXT("SMGraph.LinkState"));
}

void USMGraphNode_LinkStateNode::ResetCachedValues()
{
	Super::ResetCachedValues();
	CachedColor.Reset();
}

FString USMGraphNode_LinkStateNode::GetStateName() const
{
	return FString::Printf(TEXT("Link to %s"), LinkedStateName.IsEmpty() ? TEXT("SELECT STATE") : *LinkedStateName);
}

bool USMGraphNode_LinkStateNode::IsEndState(bool bCheckAnyState) const
{
	if (LinkedState)
	{
		return LinkedState->IsEndState(bCheckAnyState);
	}

	return false;
}

void USMGraphNode_LinkStateNode::LinkToState(const FString& InStateName)
{
	Modify();

	if (LinkedState && LinkedState->LinkStates.Contains(this))
	{
		LinkedState->Modify();
		LinkedState->LinkStates.Remove(this);
	}

	LinkedStateName = InStateName;

	if (LinkedStateName.IsEmpty())
	{
		LinkedState = nullptr;
	}
	else
	{
		LinkedState = GetLinkedStateFromName(LinkedStateName);
	}

	if (LinkedState)
	{
		LinkedState->Modify();
		LinkedState->LinkStates.Add(this);
	}

	RefreshConnectedTransitions();

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(FBlueprintEditorUtils::FindBlueprintForNodeChecked(this));
}

void USMGraphNode_LinkStateNode::GetAvailableStatesToLink(TArray<USMGraphNode_StateNodeBase*>& OutStates) const
{
	OutStates.Reset();

	if (const USMGraph* StateMachineGraph = GetOwningStateMachineGraph())
	{
		TArray<USMGraphNode_StateNodeBase*> States;
		StateMachineGraph->GetNodesOfClass(States);
		States.RemoveAll([](const USMGraphNode_StateNodeBase* StateNode)
		{
			return !StateNode->CanExistAtRuntime();
		});

		OutStates.Append(States);
	}
}

USMGraphNode_StateNodeBase* USMGraphNode_LinkStateNode::GetLinkedStateFromName(const FString& InName) const
{
	TArray<USMGraphNode_StateNodeBase*> States;
	GetAvailableStatesToLink(States);

	if (USMGraphNode_StateNodeBase** SelectedState = States.FindByPredicate([&](const USMGraphNode_StateNodeBase* StateNode)
	{
		return StateNode->GetNodeName() == InName;
	}))
	{
		return *SelectedState;
	}

	return nullptr;
}

bool USMGraphNode_LinkStateNode::IsLinkedStateValid() const
{
	if (!LinkedState)
	{
		return false;
	}

	TArray<USMGraphNode_StateNodeBase*> States;
	GetAvailableStatesToLink(States);
	return States.Contains(LinkedState);
}

FLinearColor USMGraphNode_LinkStateNode::GetStateColor() const
{
	if (CachedColor.IsSet())
	{
		return *CachedColor;
	}

	FLinearColor FinalColor =  FLinearColor(0.45f, 0.45f, 0.45f, 0.7f);

	if (LinkedState)
	{
		FinalColor *= LinkedState->GetBackgroundColor();
	}

	CachedColor = FinalColor;

	return FinalColor;
}

void USMGraphNode_LinkStateNode::OnLinkedStateRenamed()
{
	Modify();
	
	if (LinkedState)
	{
		LinkedStateName = LinkedState->GetNodeName();
	}

	RefreshConnectedTransitions();
}

FLinearColor USMGraphNode_LinkStateNode::Internal_GetBackgroundColor() const
{
	const FLinearColor DefaultColor = GetStateColor();
	return DefaultColor;
}

#undef LOCTEXT_NAMESPACE
