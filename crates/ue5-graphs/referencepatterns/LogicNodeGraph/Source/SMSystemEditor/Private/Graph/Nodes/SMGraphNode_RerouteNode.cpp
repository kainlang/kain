// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Graph/Nodes/SMGraphNode_RerouteNode.h"

#include "Compilers/SMKismetCompiler.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"

#define LOCTEXT_NAMESPACE "SMGraphRerouteNode"

USMGraphNode_RerouteNode::USMGraphNode_RerouteNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	bCanRenameNode = true;
#endif
}

void USMGraphNode_RerouteNode::PostPlacedNewNode()
{
	// Skip state base so we don't create a graph.
	USMGraphNode_Base::PostPlacedNewNode();
}

void USMGraphNode_RerouteNode::PostPasteNode()
{
	// Skip state because it relies on a graph being present.
	USMGraphNode_Base::PostPasteNode();
}

bool USMGraphNode_RerouteNode::GetCanRenameNode() const
{
	if (GetPrimaryTransition() == nullptr)
	{
		return false;
	}

	return Super::GetCanRenameNode();
}

void USMGraphNode_RerouteNode::OnRenameNode(const FString& NewName)
{
	if (USMGraphNode_TransitionEdge* PrimaryTransition = GetPrimaryTransition())
	{
		PrimaryTransition->SetNodeName_Direct(NewName);
	}
}

UEdGraphPin* USMGraphNode_RerouteNode::GetPassThroughPin(const UEdGraphPin* FromPin) const
{
	if(FromPin && Pins.Contains(FromPin))
	{
		return FromPin == Pins[0] ? Pins[1] : Pins[0];
	}

	return nullptr;
}

UObject* USMGraphNode_RerouteNode::GetJumpTargetForDoubleClick() const
{
	if (const USMGraphNode_TransitionEdge* TransitionEdge = GetPrimaryTransition())
	{
		return TransitionEdge->GetJumpTargetForDoubleClick();
	}

	return Super::GetJumpTargetForDoubleClick();
}

void USMGraphNode_RerouteNode::PreCompile(FSMKismetCompilerContext& CompilerContext)
{
	Super::PreCompile(CompilerContext);

	if (!IsThisRerouteValid())
	{
		CompilerContext.MessageLog.Error(TEXT("@@ node is missing a connection."), this);
	}
}

void USMGraphNode_RerouteNode::UpdateTime(float DeltaTime)
{
	Super::UpdateTime(DeltaTime);

	// Transitions need their time manually updated for debugging when connected to a reroute.
	// Reroutes will always handle previous transition and the next transition only if there is no ToState reroute.

	if (USMGraphNode_TransitionEdge* PrevTransition = GetPreviousTransition())
	{
		PrevTransition->UpdateTime(DeltaTime);
	}
	if (USMGraphNode_TransitionEdge* NextTransition = GetNextTransition())
	{
		if (const USMGraphNode_StateNodeBase* NextState = NextTransition->GetToState(true))
		{
			if (!NextState->IsA<USMGraphNode_RerouteNode>())
			{
				NextTransition->UpdateTime(DeltaTime);
			}
		}
	}
}

FString USMGraphNode_RerouteNode::GetNodeName() const
{
	if (const USMGraphNode_TransitionEdge* PrimaryTransition = GetPrimaryTransition())
	{
		return PrimaryTransition->GetTransitionName();
	}
	return TEXT("Reroute");
}

const FSlateBrush* USMGraphNode_RerouteNode::GetNodeIcon() const
{
	return nullptr;
}

bool USMGraphNode_RerouteNode::CanGoToLocalGraph() const
{
	if (const USMGraphNode_TransitionEdge* TransitionEdge = GetPrimaryTransition())
	{
		return TransitionEdge->CanGoToLocalGraph();
	}

	return Super::CanGoToLocalGraph();
}

UClass* USMGraphNode_RerouteNode::GetNodeClass() const
{
	if (const USMGraphNode_TransitionEdge* TransitionEdge = GetPrimaryTransition())
	{
		return TransitionEdge->GetNodeClass();
	}

	return Super::GetNodeClass();
}

TArray<TSubclassOf<USMGraphNode_Base>> USMGraphNode_RerouteNode::GetAllowedDuplicateNodeNameTypes() const
{
	return { USMGraphNode_TransitionEdge::StaticClass(), USMGraphNode_RerouteNode::StaticClass() };
}

bool USMGraphNode_RerouteNode::IsEndState(bool bCheckAnyState) const
{
	return false;
}

USMGraphNode_TransitionEdge* USMGraphNode_RerouteNode::GetPrimaryTransition() const
{
	if (USMGraphNode_TransitionEdge* Transition = GetPreviousTransition())
	{
		return Transition->GetPrimaryReroutedTransition();
	}

	if (USMGraphNode_TransitionEdge* Transition = GetNextTransition())
	{
		return Transition->GetPrimaryReroutedTransition();
	}

	return nullptr;
}

void USMGraphNode_RerouteNode::GetAllReroutedTransitions(TArray<USMGraphNode_TransitionEdge*>& OutTransitions,
	TArray<USMGraphNode_RerouteNode*>& OutRerouteNodes) const
{
	if (USMGraphNode_TransitionEdge* Transition = GetPreviousTransition())
	{
		Transition->GetAllReroutedTransitions(OutTransitions, OutRerouteNodes);
		return;
	}

	if (USMGraphNode_TransitionEdge* Transition = GetNextTransition())
	{
		Transition->GetAllReroutedTransitions(OutTransitions, OutRerouteNodes);
		return;
	}
}

bool USMGraphNode_RerouteNode::IsThisRerouteValid() const
{
	return (GetInputPin()->LinkedTo.Num() == 1 && GetOutputPin()->LinkedTo.Num() == 1) ||
		IsRerouteEmpty();
}

bool USMGraphNode_RerouteNode::IsRerouteEmpty() const
{
	return GetInputPin()->LinkedTo.Num() == 0 && GetOutputPin()->LinkedTo.Num() == 0;
}

void USMGraphNode_RerouteNode::BreakAllOutgoingReroutedConnections()
{
	const UEdGraphSchema* Schema = GetDefault<UEdGraphSchema>();

	const USMGraphNode_RerouteNode* CurrentReroute = this;
	while (CurrentReroute)
	{
		if (const USMGraphNode_TransitionEdge* Transition = CurrentReroute->GetNextTransition())
		{
			const USMGraphNode_RerouteNode* NextReroute = Transition->GetNextRerouteNode();

			// Use default schema to avoid unnecessary construction script usage.
			Schema->BreakPinLinks(*CurrentReroute->GetOutputPin(), true);

			CurrentReroute = NextReroute;
		}
		else
		{
			break;
		}
	}
}

FLinearColor USMGraphNode_RerouteNode::Internal_GetBackgroundColor() const
{
	const FLinearColor FinalColor = FLinearColor(0.45f, 0.45f, 0.45f, 0.7f);
	return FinalColor;
}

#undef LOCTEXT_NAMESPACE
