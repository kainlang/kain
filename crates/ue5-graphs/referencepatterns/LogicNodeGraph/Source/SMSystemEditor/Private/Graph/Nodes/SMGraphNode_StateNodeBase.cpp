// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Graph/Nodes/SMGraphNode_StateNodeBase.h"

#include "Blueprints/SMBlueprintEditor.h"
#include "Configuration/SMEditorSettings.h"
#include "Graph/Nodes/SMGraphNode_LinkStateNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineEntryNode.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/Schema/SMStateGraphSchema.h"
#include "Graph/SMGraph.h"
#include "Graph/SMStateGraph.h"
#include "Utilities/SMBlueprintEditorUtils.h"
#include "Utilities/SMNodeInstanceUtils.h"

#include "SMUtils.h"

#include "Kismet2/Kismet2NameValidators.h"
#include "ScopedTransaction.h"
#include "UObject/UObjectThreadContext.h"

#define LOCTEXT_NAMESPACE "SMGraphStateNodeBase"

USMGraphNode_StateNodeBase::USMGraphNode_StateNodeBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), bAlwaysUpdate_DEPRECATED(false), bDisableTickTransitionEvaluation_DEPRECATED(false),
	  bEvalTransitionsOnStart_DEPRECATED(false), bExcludeFromAnyState_DEPRECATED(false),
	  bCanTransitionToSelf(false), bRequestInitialAnimation(false)
{
}

void USMGraphNode_StateNodeBase::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, TEXT("Transition"), TEXT("In"));
	CreatePin(EGPD_Output, TEXT("Transition"), TEXT("Out"));
}

FText USMGraphNode_StateNodeBase::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	FString StateName = GetStateName();
	if (const USMStateInstance_Base* StateInstance = Cast<USMStateInstance_Base>(NodeInstanceTemplate))
	{
		if (StateInstance->ShouldUseDisplayNameOnly() && TitleType == ENodeTitleType::FullTitle)
		{
			StateName = FSMNodeInstanceUtils::GetNodeDisplayName(StateInstance);
		}
	}
	
	return FText::FromString(StateName);
}

bool USMGraphNode_StateNodeBase::GetCanRenameNode() const
{
	// Generally want this to be true even if the slate node doesn't allow it so the option shows up in the details panel.
	
	return Super::GetCanRenameNode();
}

void USMGraphNode_StateNodeBase::OnRenameNode(const FString& NewName)
{
	Super::OnRenameNode(NewName);

	for (const TObjectPtr<USMGraphNode_LinkStateNode>& LinkState : LinkStates)
	{
		if (LinkState)
		{
			LinkState->OnLinkedStateRenamed();
		}
	}
}

void USMGraphNode_StateNodeBase::AutowireNewNode(UEdGraphPin* FromPin)
{
	Super::AutowireNewNode(FromPin);

	if (FromPin != nullptr)
	{
		UEdGraphPin* InputPin = GetInputPin();

		if (InputPin && GetSchema()->TryCreateConnection(FromPin, InputPin))
		{
			FromPin->GetOwningNode()->NodeConnectionListChanged();
		}
	}
}

void USMGraphNode_StateNodeBase::PostPlacedNewNode()
{
	SetToCurrentVersion();
	
	// Create a new state machine graph
	check(BoundGraph == nullptr);
	BoundGraph = FBlueprintEditorUtils::CreateNewGraph(
		this,
		NAME_None,
		USMStateGraph::StaticClass(),
		USMStateGraphSchema::StaticClass());
	check(BoundGraph);

	// Find an interesting name
	const TSharedPtr<INameValidatorInterface> NameValidator = FNameValidatorFactory::MakeValidator(this);
	FBlueprintEditorUtils::RenameGraphWithSuggestion(BoundGraph, NameValidator, *GetFriendlyNodeName().ToString());

	// Initialize the state machine graph
	const UEdGraphSchema* Schema = BoundGraph->GetSchema();
	Schema->CreateDefaultNodesForGraph(*BoundGraph);

	// Add the new graph as a child of our parent graph
	UEdGraph* ParentGraph = GetGraph();

	// TODO: This search is slow on large state machines!
	if (ParentGraph->SubGraphs.Find(BoundGraph) == INDEX_NONE)
	{
		ParentGraph->Modify();
		ParentGraph->SubGraphs.Add(BoundGraph);
	}

	if (bGenerateTemplateOnNodePlacement)
	{
		InitTemplate();
	}
}

void USMGraphNode_StateNodeBase::PostPasteNode()
{
	RemoveInvalidLinkStates();
	
	// Look for a unique name. We have to perform manual handling for this before passing it off to Unreal.
	// Every copy resets the unique name count, so State_1 pasted might be State_1_1. This makes it so it is State_2.
	// t-155
	const FString UniqueName = FSMBlueprintEditorUtils::FindUniqueName(GetStateName(), GetOwningStateMachineGraph());
	
	const TSharedPtr<INameValidatorInterface> NameValidator = FNameValidatorFactory::MakeValidator(this);
	FBlueprintEditorUtils::RenameGraphWithSuggestion(BoundGraph, NameValidator, UniqueName);
	
	TArray<UEdGraphNode*> ContainedNodes;
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested<UEdGraphNode>(BoundGraph, ContainedNodes);

	for (UEdGraphNode* GraphNode : ContainedNodes)
	{
		GraphNode->CreateNewGuid();
		GraphNode->PostPasteNode();
		// Required to correct context display issues.
		GraphNode->ReconstructNode();
	}

	Super::PostPasteNode();

	bRequestInitialAnimation = true;
}

void USMGraphNode_StateNodeBase::DestroyNode()
{
	Modify();
	if (BoundGraph)
	{
		BoundGraph->Modify();
	}

	for (auto It = LinkStates.CreateIterator(); It; ++It)
	{
		if (It->Get() && (*It)->GetLinkedState() == this)
		{
			(*It)->LinkToState(FString());
		}
	}
	
	UEdGraph* GraphToRemove = BoundGraph;

	BoundGraph = nullptr;
	Super::DestroyNode();

	if (GraphToRemove)
	{
		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(this);
		FBlueprintEditorUtils::RemoveGraph(Blueprint, GraphToRemove, EGraphRemoveFlags::Recompile);
	}
}

void USMGraphNode_StateNodeBase::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	// Template has been changed.
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(USMGraphNode_StateNodeBase, NodeInstanceTemplate))
	{
		//  Check if it's a property we care about.
		FEditPropertyChain::TDoubleLinkedListNode* MemberNode = PropertyChangedEvent.PropertyChain.GetActiveMemberNode();
		if (MemberNode && MemberNode->GetNextNode() && MemberNode->GetValue())
		{
			const FName Name = PropertyChangedEvent.PropertyChain.GetActiveMemberNode()->GetNextNode()->GetValue()->GetFName();
			if (Name == GET_MEMBER_NAME_CHECKED(USMStateInstance_Base, bDefaultToParallel))
			{
				for (int32 Idx = 0; Idx < GetOutputPin()->LinkedTo.Num(); ++Idx)
				{
					if (USMGraphNode_TransitionEdge* Transition = GetNextTransition(Idx))
					{
						if (USMTransitionInstance* Instance = Transition->GetNodeTemplateAs<USMTransitionInstance>())
						{
							Instance->SetRunParallel(ShouldDefaultTransitionsToParallel());
						}
					}
				}
			}
		}
	}
}

void USMGraphNode_StateNodeBase::PinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::PinConnectionListChanged(Pin);
}

void USMGraphNode_StateNodeBase::ImportDeprecatedProperties()
{
	Super::ImportDeprecatedProperties();

	if (USMStateInstance_Base* StateInstance = Cast<USMStateInstance_Base>(GetNodeTemplate()))
	{
		StateInstance->SetAlwaysUpdate(bAlwaysUpdate_DEPRECATED);
		StateInstance->SetDisableTickTransitionEvaluation(bDisableTickTransitionEvaluation_DEPRECATED);
		StateInstance->SetEvalTransitionsOnStart(bEvalTransitionsOnStart_DEPRECATED);
		StateInstance->SetExcludeFromAnyState(bExcludeFromAnyState_DEPRECATED);
	}
}

void USMGraphNode_StateNodeBase::OnNodeMoved(const FVector2D& NewPosition)
{
	{
		// Transition position managed by state.
		TArray<USMGraphNode_TransitionEdge*> AllTransitions;
		GetOutputTransitions(AllTransitions);
		GetInputTransitions(AllTransitions);
		for (USMGraphNode_TransitionEdge* Transition : AllTransitions)
		{
			Transition->SetReadOnlyNodePosition();
		}
	}

	Super::OnNodeMoved(NewPosition);
}

void USMGraphNode_StateNodeBase::OnBoundGraphRenamed(UObject* OldOuter, const FName OldName)
{
	Super::OnBoundGraphRenamed(OldOuter, OldName);

	RefreshConnectedTransitions();
}

void USMGraphNode_StateNodeBase::PreCompile(FSMKismetCompilerContext& CompilerContext)
{
	Super::PreCompile(CompilerContext);

	// Verify linked states are correct and cleanup if necessary.
	RemoveInvalidLinkStates();
}

void USMGraphNode_StateNodeBase::ResetNodeName()
{
	FScopedTransaction Transaction(LOCTEXT("ResetNodeName", "Reset Node Name"));
	
	// Initial node name when placed.
	const TSharedPtr<INameValidatorInterface> NameValidator = FNameValidatorFactory::MakeValidator(this);
	FBlueprintEditorUtils::RenameGraphWithSuggestion(BoundGraph, NameValidator, *GetFriendlyNodeName().ToString());

	// Read template data.
	Super::ResetNodeName();
}

void USMGraphNode_StateNodeBase::SetAnyStateTags(const FGameplayTagContainer& InAnyStateTags)
{
	if (AnyStateTags != InAnyStateTags)
	{
		FProperty* Property = StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(USMGraphNode_StateNodeBase, AnyStateTags));
		check(Property);

		PreEditChange(Property);
		
		AnyStateTags = InAnyStateTags;

		FPropertyChangedEvent ChangeEvent(Property, EPropertyChangeType::ValueSet);
		PostEditChangeProperty(ChangeEvent);

		if (FSMBlueprintEditor* BlueprintEditor = FSMBlueprintEditorUtils::GetStateMachineEditor(this))
		{
			// Tag container won't refresh otherwise.
			BlueprintEditor->RefreshInspector();
		}
	}
}

void USMGraphNode_StateNodeBase::SetRuntimeDefaults(FSMState_Base& State) const
{
	State.SetNodeName(GetStateName());

	State.NodePosition = NodePosition;
	State.bHasInputEvents = FSMBlueprintEditorUtils::DoesGraphHaveInputEvents(GetBoundGraph());
	
	if (const USMStateInstance_Base* StateInstance = Cast<USMStateInstance_Base>(GetNodeTemplate()))
	{
		State.bAlwaysUpdate = StateInstance->GetAlwaysUpdate();
		State.bDisableTickTransitionEvaluation = StateInstance->GetDisableTickTransitionEvaluation();
		State.bAllowParallelReentry = StateInstance->GetAllowParallelReentry();
		State.bStayActiveOnStateChange = StateInstance->GetStayActiveOnStateChange();
		State.bEvalTransitionsOnStart = StateInstance->GetEvalTransitionsOnStart();
		State.bCanBeEndState = StateInstance->GetCanBeEndState();
	}
}

bool USMGraphNode_StateNodeBase::IsEndState(bool bCheckAnyState) const
{
	if (const USMStateInstance_Base* StateInstance = Cast<USMStateInstance_Base>(GetNodeTemplate()))
	{
		if (!StateInstance->GetCanBeEndState())
		{
			return false;
		}
	}
	
	// Must have entry.
	if (!HasInputConnections())
	{
		return false;
	}

	// Check Any States since they add transitions to this node on compile.
	if (bCheckAnyState && FSMBlueprintEditorUtils::IsNodeImpactedFromAnyStateNode(this))
	{
		return false;
	}

	// If no output definitely end state.
	if (const UEdGraphPin* OutputPin = GetOutputPin())
	{
		if (OutputPin->LinkedTo.Num() == 0)
		{
			return true;
		}

		for (const UEdGraphPin* Pin : OutputPin->LinkedTo)
		{
			if (const USMGraphNode_TransitionEdge* Transition = Cast<USMGraphNode_TransitionEdge>(Pin->GetOwningNode()))
			{
				// Transitioning to self doesn't count.
				if (Transition->GetFromState() == Transition->GetToState())
				{
					continue;
				}

				// There has to be some way out of here...
				if (Transition->PossibleToTransition())
				{
					return false;
				}
			}
		}
	}

	return true;
}

bool USMGraphNode_StateNodeBase::HasInputConnections() const
{
	if (UEdGraphPin* Pin = GetInputPin())
	{
		if (Pin->LinkedTo.Num() == 0)
		{
			for (const TObjectPtr<USMGraphNode_LinkStateNode>& LinkState : LinkStates)
			{
				if (LinkState && LinkState->HasInputConnections())
				{
					return true;
				}
			}

			return false;
		}

		for (const UEdGraphPin* InputPin : Pin->LinkedTo)
		{
			if (InputPin->GetOwningNode()->IsA<USMGraphNode_StateMachineEntryNode>())
			{
				return true;
			}

			if (const USMGraphNode_TransitionEdge* Transition = Cast<USMGraphNode_TransitionEdge>(InputPin->GetOwningNode()))
			{
				// Ignore self and input connections which can't transition.
				if (Transition->GetFromState() == Transition->GetToState() || !Transition->PossibleToTransition())
				{
					continue;
				}

				return true;
			}
		}
	}

	return false;
}

bool USMGraphNode_StateNodeBase::HasOutputConnections() const
{
	if (UEdGraphPin* Pin = GetOutputPin())
	{
		return Pin->LinkedTo.Num() > 0;
	}

	return false;
}

bool USMGraphNode_StateNodeBase::ShouldDefaultTransitionsToParallel() const
{
	if (USMStateInstance_Base* StateInstance = Cast<USMStateInstance_Base>(GetNodeTemplate()))
	{
		return StateInstance->GetDefaultToParallel();
	}

	return false;
}

bool USMGraphNode_StateNodeBase::ShouldExcludeFromAnyState() const
{
	if (USMStateInstance_Base* StateInstance = Cast<USMStateInstance_Base>(GetNodeTemplate()))
	{
		return StateInstance->GetExcludeFromAnyState();
	}

	return false;
}

bool USMGraphNode_StateNodeBase::HasTransitionToNode(const UEdGraphNode* Node) const
{
	if (UEdGraphPin* OutputPin = GetOutputPin())
	{
		for (const UEdGraphPin* Pin : OutputPin->LinkedTo)
		{
			if (Pin == nullptr)
			{
				continue;
			}
			if (const USMGraphNode_TransitionEdge* Transition = Cast<USMGraphNode_TransitionEdge>(Pin->GetOwningNode()))
			{
				if (Transition->GetToState() == Node)
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool USMGraphNode_StateNodeBase::HasTransitionFromNode(const UEdGraphNode* Node) const
{
	if (UEdGraphPin* InputPin = GetInputPin())
	{
		for (const UEdGraphPin* Pin : InputPin->LinkedTo)
		{
			if (Pin == nullptr)
			{
				continue;
			}
			if (const USMGraphNode_TransitionEdge* Transition = Cast<USMGraphNode_TransitionEdge>(Pin->GetOwningNode()))
			{
				if (Transition->GetFromState() == Node)
				{
					return true;
				}
			}
		}
	}

	return false;
}

USMGraphNode_Base* USMGraphNode_StateNodeBase::GetPreviousNode(int32 Index) const
{
	if (UEdGraphPin* InputPin = GetInputPin())
	{
		if (InputPin->LinkedTo.Num() <= Index || InputPin->LinkedTo[Index] == nullptr)
		{
			return nullptr;
		}

		return Cast<USMGraphNode_Base>(InputPin->LinkedTo[Index]->GetOwningNodeUnchecked());
	}

	return nullptr;
}

USMGraphNode_Base* USMGraphNode_StateNodeBase::GetNextNode(int32 Index) const
{
	if (UEdGraphPin* OutputPin = GetOutputPin())
	{
		if (OutputPin->LinkedTo.Num() <= Index || OutputPin->LinkedTo[Index] == nullptr)
		{
			return nullptr;
		}

		return Cast<USMGraphNode_Base>(OutputPin->LinkedTo[Index]->GetOwningNodeUnchecked());
	}

	return nullptr;
}

USMGraphNode_StateNodeBase* USMGraphNode_StateNodeBase::GetPreviousState(int32 Index /*= 0*/, bool bIncludeReroute /*= false*/, bool bIncludeEntryState /*= false*/) const
{
	if (const USMGraphNode_TransitionEdge* Transition = GetPreviousTransition(Index))
	{
		return Transition->GetFromState(bIncludeReroute);
	}

	if (bIncludeEntryState)
	{
		return Cast<USMGraphNode_StateMachineEntryNode>(GetPreviousNode(Index));
	}

	return nullptr;
}

USMGraphNode_StateNodeBase* USMGraphNode_StateNodeBase::GetNextState(int32 Index /*= 0*/, bool bIncludeReroute /*= false*/) const
{
	if (const USMGraphNode_TransitionEdge* Transition = GetNextTransition(Index))
	{
		return Transition->GetToState(bIncludeReroute);
	}

	return nullptr;
}

USMGraphNode_TransitionEdge* USMGraphNode_StateNodeBase::GetPreviousTransition(int32 Index) const
{
	if (UEdGraphPin* InputPin = GetInputPin())
	{
		if (InputPin->LinkedTo.Num() <= Index || InputPin->LinkedTo[Index] == nullptr)
		{
			return nullptr;
		}

		if (USMGraphNode_TransitionEdge* Transition = Cast<USMGraphNode_TransitionEdge>(InputPin->LinkedTo[Index]->GetOwningNode()))
		{
			return Transition;
		}
	}
	return nullptr;
}

USMGraphNode_TransitionEdge* USMGraphNode_StateNodeBase::GetNextTransition(int32 Index) const
{
	if (UEdGraphPin* OutputPin = GetOutputPin())
	{
		if (OutputPin->LinkedTo.Num() <= Index || OutputPin->LinkedTo[Index] == nullptr)
		{
			return nullptr;
		}

		if (USMGraphNode_TransitionEdge* Transition = Cast<USMGraphNode_TransitionEdge>(OutputPin->LinkedTo[Index]->GetOwningNode()))
		{
			return Transition;
		}
	}

	return nullptr;
}

void USMGraphNode_StateNodeBase::GetInputTransitions(TArray<USMGraphNode_TransitionEdge*>& OutTransitions) const
{
	if (UEdGraphPin* InputPin = GetInputPin())
	{
		for (int32 Idx = 0; Idx < InputPin->LinkedTo.Num(); ++Idx)
		{
			if (InputPin->LinkedTo[Idx] == nullptr)
			{
				// Could be null during a paste.
				continue;
			}
			if (USMGraphNode_TransitionEdge* Transition = Cast<USMGraphNode_TransitionEdge>(InputPin->LinkedTo[Idx]->GetOwningNode()))
			{
				OutTransitions.AddUnique(Transition);
			}
		}
	}
}

void USMGraphNode_StateNodeBase::GetOutputTransitions(TArray<USMGraphNode_TransitionEdge*>& OutTransitions) const
{
	if (UEdGraphPin* OutputPin = GetOutputPin())
	{
		for (int32 Idx = 0; Idx < OutputPin->LinkedTo.Num(); ++Idx)
		{
			if (OutputPin->LinkedTo[Idx] == nullptr)
			{
				// Could be null during a paste.
				continue;
			}
			if (USMGraphNode_TransitionEdge* Transition = Cast<USMGraphNode_TransitionEdge>(OutputPin->LinkedTo[Idx]->GetOwningNode()))
			{
				OutTransitions.AddUnique(Transition);
			}
		}
	}
}

int32 USMGraphNode_StateNodeBase::GetNumInputConnections() const
{
	if (const UEdGraphPin* InputPin = GetInputPin())
	{
		return InputPin->LinkedTo.Num();
	}

	return 0;
}

int32 USMGraphNode_StateNodeBase::GetNumOutputConnections() const
{
	if (const UEdGraphPin* OutputPin = GetOutputPin())
	{
		return OutputPin->LinkedTo.Num();
	}

	return 0;
}

UEdGraphPin* USMGraphNode_StateNodeBase::GetConnectedEntryPin() const
{
	if (UEdGraphPin* InputPin = GetInputPin())
	{
		for (int32 Idx = 0; Idx < InputPin->LinkedTo.Num(); ++Idx)
		{
			if (InputPin->LinkedTo[Idx]->GetOwningNode()->IsA<USMGraphNode_StateMachineEntryNode>())
			{
				return InputPin->LinkedTo[Idx];
			}
		}
	}

	return nullptr;
}

FLinearColor USMGraphNode_StateNodeBase::GetBackgroundColorForNodeInstance(const USMNodeInstance* NodeInstance) const
{
	const USMEditorSettings* Settings = FSMBlueprintEditorUtils::GetEditorSettings();
	const FLinearColor* CustomColor = GetCustomBackgroundColor(NodeInstance);
	const FLinearColor ColorModifier = !CustomColor ? FLinearColor(0.6f, 0.6f, 0.6f, 0.5f) : *CustomColor;
	const FLinearColor EndStateColor = !CustomColor ? Settings->EndStateColor * ColorModifier : CastChecked<USMStateInstance_Base>(NodeInstance)->GetEndStateColor();

	if (IsEndState())
	{
		return EndStateColor;
	}

	const FLinearColor DefaultColor = Settings->StateDefaultColor;

	// No input -- node unreachable.
	if (!HasInputConnections())
	{
		return DefaultColor * ColorModifier;
	}

	// State is active
	if (FSMBlueprintEditorUtils::GraphHasAnyLogicConnections(BoundGraph))
	{
		return CustomColor ? *CustomColor * FLinearColor(1.f, 1.f, 1.f, 1.2f) : Settings->StateWithLogicColor * ColorModifier;
	}

	return DefaultColor * ColorModifier;
}

void USMGraphNode_StateNodeBase::RemoveInvalidLinkStates()
{
	for (auto It = LinkStates.CreateIterator(); It; ++It)
	{
		if (!It->Get()
			|| (It->Get()->GetLinkedState() != this)
			|| (GetGraph() && !GetGraph()->Nodes.Contains(*It)))
		{
			It.RemoveCurrent();
		}
	}
}

void USMGraphNode_StateNodeBase::RefreshConnectedTransitions()
{
	TSet<USMGraphNode_TransitionEdge*> TransitionsChecked;
	{
		TArray<USMGraphNode_TransitionEdge*> Transitions;
		GetOutputTransitions(Transitions);

		for (USMGraphNode_TransitionEdge* Transition : Transitions)
		{
			if (TransitionsChecked.Contains(Transition))
			{
				continue;
			}
			Transition->UpdatePrimaryTransition();
			TransitionsChecked.Add(Transition);
		}
	}

	{
		TArray<USMGraphNode_TransitionEdge*> Transitions;
		GetInputTransitions(Transitions);

		for (USMGraphNode_TransitionEdge* Transition : Transitions)
		{
			if (TransitionsChecked.Contains(Transition))
			{
				continue;
			}
			Transition->UpdatePrimaryTransition();
			TransitionsChecked.Add(Transition);
		}
	}
}

FLinearColor USMGraphNode_StateNodeBase::Internal_GetBackgroundColor() const
{
	return GetBackgroundColorForNodeInstance(NodeInstanceTemplate);
}

#undef LOCTEXT_NAMESPACE
