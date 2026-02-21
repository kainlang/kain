// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMStateInstance.h"

#include "ISMEditorGraphNodeInterface.h"
#include "SMInstance.h"
#include "SMLogging.h"
#include "SMState.h"
#include "SMStateMachine.h"
#include "SMTransition.h"
#include "SMTransitionInstance.h"
#include "SMUtils.h"

USMStateInstance_Base::USMStateInstance_Base() : Super(),
                                                 bEvalGraphsOnStart(true), bEvalGraphsOnUpdate(false),
                                                 bEvalGraphsOnEnd(false), bEvalGraphsOnRootStateMachineStart(false),
                                                 bEvalGraphsOnRootStateMachineStop(false),
                                                 bDisableTickTransitionEvaluation(false), bEvalTransitionsOnStart(false),
                                                 bAlwaysUpdate(false),
                                                 bCanBeEndState(true),
                                                 bDefaultToParallel(false),
                                                 bAllowParallelReentry(false),
                                                 bStayActiveOnStateChange(false),
                                                 bExcludeFromAnyState(false)
{
#if WITH_EDITORONLY_DATA
	// TODO: Read editor settings.
	NodeEndStateColor = FLinearColor(1.f, 1.f, 1.f, 0.7f);
	bDisplayNameWidget = true;
	bShowDisplayNameOnly = false;
	bRegisterWithContextMenu = true;
	bHideFromContextMenuIfRulesFail = false;
#endif
}

bool USMStateInstance_Base::IsInEndState() const
{
	if (const FSMState_Base* State = (FSMState_Base*)GetOwningNode())
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return State->IsInEndState();
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	return false;
}

bool USMStateInstance_Base::IsEndState() const
{
	if (const FSMState_Base* State = (FSMState_Base*)GetOwningNode())
	{
		return State->IsEndState();
	}

	return false;
}

void USMStateInstance_Base::GetStateInfo(FSMStateInfo& State) const
{
	if (const FSMState_Base* StateNode = (FSMState_Base*)GetOwningNode())
	{
		State = FSMStateInfo(*StateNode);
	}
	else
	{
		State = FSMStateInfo();
	}
}

bool USMStateInstance_Base::IsStateMachine() const
{
	if (const FSMState_Base* State = (FSMState_Base*)GetOwningNode())
	{
		return State->IsStateMachine();
	}

	return false;
}

bool USMStateInstance_Base::IsEntryState() const
{
	if (const FSMState_Base* State = (FSMState_Base*)GetOwningNode())
	{
		return State->bIsRootNode;
	}

	return false;
}

void USMStateInstance_Base::SetActive(bool bValue, bool bSetAllParents, bool bActivateNow)
{
	USMUtils::ActivateStateNetOrLocal(GetOwningNodeAs<FSMState_Base>(), bValue, bSetAllParents, bActivateNow);
}

void USMStateInstance_Base::EvaluateTransitions()
{
	if (USMInstance* StateMachineInstance = GetStateMachineInstance(true))
	{
		StateMachineInstance->EvaluateTransitions();
	}
}

bool USMStateInstance_Base::GetOutgoingTransitions(TArray<USMTransitionInstance*>& Transitions,
                                                   bool bExcludeAlwaysFalse) const
{
	Transitions.Reset();

	if (const FSMState_Base* Node = (FSMState_Base*)GetOwningNodeContainer())
	{
		for (FSMTransition* Transition : Node->GetOutgoingTransitions())
		{
			if (Transition->bAlwaysFalse && bExcludeAlwaysFalse)
			{
				continue;
			}
			if (USMTransitionInstance* TransitionInstance = Cast<USMTransitionInstance>(
				Transition->GetOrCreateNodeInstance()))
			{
				Transitions.Add(TransitionInstance);
			}
		}
	}

	return Transitions.Num() > 0;
}

bool USMStateInstance_Base::GetIncomingTransitions(TArray<USMTransitionInstance*>& Transitions,
                                                   bool bExcludeAlwaysFalse) const
{
	Transitions.Reset();

	if (const FSMState_Base* Node = (FSMState_Base*)GetOwningNodeContainer())
	{
		for (FSMTransition* Transition : Node->GetIncomingTransitions())
		{
			if (Transition->bAlwaysFalse && bExcludeAlwaysFalse)
			{
				continue;
			}
			if (USMTransitionInstance* TransitionInstance = Cast<USMTransitionInstance>(
				Transition->GetOrCreateNodeInstance()))
			{
				Transitions.Add(TransitionInstance);
			}
		}
	}

	return Transitions.Num() > 0;
}

USMTransitionInstance* USMStateInstance_Base::GetTransitionToTake() const
{
	if (const FSMState_Base* State = (FSMState_Base*)GetOwningNodeContainer())
	{
		if (const FSMTransition* Transition = State->GetTransitionToTake())
		{
			return Cast<USMTransitionInstance>(const_cast<FSMTransition*>(Transition)->GetOrCreateNodeInstance());
		}
	}

	return nullptr;
}

bool USMStateInstance_Base::SwitchToLinkedState(USMStateInstance_Base* NextStateInstance, bool bRequireTransitionToPass,
                                                bool bActivateNow)
{
	if (const FSMState_Base* Node = GetOwningNodeAs<FSMState_Base>())
	{
		if (!Node->IsActive())
		{
			LD_LOG_WARNING(TEXT("Attempted to switch to linked state '%s' but this node '%s' is not currently active."),
			               *NextStateInstance->GetName(), *Node->GetNodeName());
			return false;
		}

		// Find the state in the available transitions.
		for (FSMTransition* Transition : Node->GetOutgoingTransitions())
		{
			if (NextStateInstance == Transition->GetToState()->GetNodeInstance())
			{
				return SwitchToLinkedStateByTransition_Internal(Transition, bRequireTransitionToPass, bActivateNow);
			}
		}

		LD_LOG_WARNING(TEXT("Attempted to switch to linked state '%s' from '%s' but the node could not be found."),
		               *NextStateInstance->GetName(), *Node->GetNodeName());
	}

	return false;
}

bool USMStateInstance_Base::SwitchToLinkedStateByName(const FString& NextStateName, bool bRequireTransitionToPass,
                                                      bool bActivateNow)
{
	if (USMStateInstance_Base* NextState = GetNextStateByName(NextStateName))
	{
		return SwitchToLinkedState(NextState, bRequireTransitionToPass, bActivateNow);
	}

	return false;
}

bool USMStateInstance_Base::SwitchToLinkedStateByTransition(USMTransitionInstance* TransitionInstance,
                                                            bool bRequireTransitionToPass, bool bActivateNow)
{
	if (TransitionInstance != nullptr)
	{
		if (TransitionInstance->GetPreviousStateInstance() != this)
		{
			LD_LOG_WARNING(
				TEXT(
					"Attempted to switch to linked state by transition '%s' from state '%s' but this transition is from state '%s'."
				),
				*TransitionInstance->GetNodeName(), *GetNodeName(),
				*TransitionInstance->GetPreviousStateInstance()->GetNodeName());
			return false;
		}
		return SwitchToLinkedStateByTransition_Internal(TransitionInstance->GetOwningNodeAs<FSMTransition>(),
		                                                bRequireTransitionToPass, bActivateNow);
	}

	return false;
}

bool USMStateInstance_Base::SwitchToLinkedStateByTransitionName(const FString& TransitionName,
                                                                bool bRequireTransitionToPass, bool bActivateNow)
{
	if (USMTransitionInstance* TransitionInstance = GetOutgoingTransitionByName(TransitionName))
	{
		return SwitchToLinkedStateByTransition(TransitionInstance, bRequireTransitionToPass, bActivateNow);
	}

	LD_LOG_WARNING(
		TEXT(
			"Attempted to switch to linked state by transition '%s' from state '%s' but the outgoing transition could not be found."
		),
		*TransitionName, *GetNodeName());
	return false;
}

bool USMStateInstance_Base::SwitchToLinkedStateByTransition_Internal(FSMTransition* Transition,
                                                                     bool bRequireTransitionToPass, bool bActivateNow)
{
	check(Transition);
	if (FSMState_Base* Node = GetOwningNodeAs<FSMState_Base>())
	{
		if (!Node->IsActive())
		{
			LD_LOG_WARNING(
				TEXT("Attempted to switch to linked state by transition %s but this state %s is not currently active."),
				*Transition->GetNodeName(), *Node->GetNodeName());
			return false;
		}

		if (bRequireTransitionToPass && !Transition->DoesTransitionPass())
		{
			return false;
		}

		// Notify the owning state machine to take this transition.
		FSMStateMachine* StateMachineNode = static_cast<FSMStateMachine*>(Node->GetOwnerNode());
		if (StateMachineNode->CanProcessExternalTransition())
		{
			FSMState_Base* DestinationState = Transition->GetToState();

			if (StateMachineNode->ProcessTransition(Transition, Node,
			                                        DestinationState, nullptr, 0.f))
			{
				if (bActivateNow)
				{
					bool bTakeTransitions = false;
					if (StateMachineNode->TryStartState(DestinationState, &bTakeTransitions) && bTakeTransitions)
					{
						const FSMStateMachine::FStateScopingArgs ScopeArgs
							{{DestinationState}, {DestinationState}};
						StateMachineNode->ProcessStates(0.f, true,
						                                FGuid(), ScopeArgs);
					}
				}
			}
			return true;
		}
	}

	return false;
}

USMTransitionInstance* USMStateInstance_Base::GetTransitionByIndex(int32 Index) const
{
	if (const FSMState_Base* Node = (FSMState_Base*)GetOwningNodeContainer())
	{
		const TArray<FSMTransition*>& Transitions = Node->GetOutgoingTransitions();
		if (Index >= 0 && Index < Transitions.Num())
		{
			return Cast<USMTransitionInstance>(Transitions[Index]->GetOrCreateNodeInstance());
		}
	}

	return nullptr;
}

USMTransitionInstance* USMStateInstance_Base::GetOutgoingTransitionByName(const FString& Name) const
{
	if (const FSMState_Base* Node = (FSMState_Base*)GetOwningNodeContainer())
	{
		const TArray<FSMTransition*>& Transitions = Node->GetOutgoingTransitions();
		for (FSMTransition* Transition : Transitions)
		{
			if (Transition->GetNodeName() == Name)
			{
				return Cast<USMTransitionInstance>(Transition->GetOrCreateNodeInstance());
			}
		}
	}

	return nullptr;
}

USMTransitionInstance* USMStateInstance_Base::GetIncomingTransitionByName(const FString& Name) const
{
	if (const FSMState_Base* Node = (FSMState_Base*)GetOwningNodeContainer())
	{
		const TArray<FSMTransition*>& Transitions = Node->GetIncomingTransitions();
		for (FSMTransition* Transition : Transitions)
		{
			if (Transition->GetNodeName() == Name)
			{
				return Cast<USMTransitionInstance>(Transition->GetOrCreateNodeInstance());
			}
		}
	}

	return nullptr;
}

USMStateInstance_Base* USMStateInstance_Base::GetNextStateByTransitionIndex(int32 Index) const
{
	const USMTransitionInstance* FoundTransition = GetTransitionByIndex(Index);
	return FoundTransition ? FoundTransition->GetNextStateInstance() : nullptr;
}

USMStateInstance_Base* USMStateInstance_Base::GetNextStateByName(const FString& StateName) const
{
	if (const USMStateMachineInstance* OwningStateMachineInstance = GetOwningStateMachineNodeInstance())
	{
		// Search for a state in the same FSM scope.
		if (USMStateInstance_Base* NeighborState = OwningStateMachineInstance->GetContainedStateByName(StateName))
		{
			TArray<USMTransitionInstance*> OutgoingTransitions;
			GetOutgoingTransitions(OutgoingTransitions, false);

			// Check if any of this state's outgoing transitions connects to the state in question.
			if (OutgoingTransitions.ContainsByPredicate([NeighborState](const USMTransitionInstance* Transition)
			{
				return Transition->GetNextStateInstance() == NeighborState;
			}))
			{
				return NeighborState;
			}
		}
	}

	return nullptr;
}

USMStateInstance_Base* USMStateInstance_Base::GetPreviousStateByName(const FString& StateName) const
{
	if (const USMStateMachineInstance* OwningStateMachineInstance = GetOwningStateMachineNodeInstance())
	{
		// Search for a state in the same FSM scope.
		if (USMStateInstance_Base* NeighborState = OwningStateMachineInstance->GetContainedStateByName(StateName))
		{
			TArray<USMTransitionInstance*> IncomingTransitions;
			GetIncomingTransitions(IncomingTransitions, false);

			// Check if any of this state's incoming transitions connects to the state in question.
			if (IncomingTransitions.ContainsByPredicate([NeighborState](const USMTransitionInstance* Transition)
			{
				return Transition->GetPreviousStateInstance() == NeighborState;
			}))
			{
				return NeighborState;
			}
		}
	}

	return nullptr;
}

USMStateInstance_Base* USMStateInstance_Base::GetPreviousActiveState() const
{
	if (const FSMState_Base* Node = (FSMState_Base*)GetOwningNode())
	{
		if (FSMState_Base* PreviousEnteredState = Node->GetPreviousActiveState())
		{
			return Cast<USMStateInstance_Base>(PreviousEnteredState->GetOrCreateNodeInstance());
		}
	}

	return nullptr;
}

USMTransitionInstance* USMStateInstance_Base::GetPreviousActiveTransition() const
{
	if (const FSMState_Base* Node = (FSMState_Base*)GetOwningNode())
	{
		if (FSMTransition* PreviousEnteredTransition = Node->GetPreviousActiveTransition())
		{
			return Cast<USMTransitionInstance>(PreviousEnteredTransition->GetOrCreateNodeInstance());
		}
	}

	return nullptr;
}

// Checks every transition for IsTransitionFromAnyState().
static bool AreAllTransitionsFromAnAnyState(const TArray<USMTransitionInstance*>& InTransitions)
{
	if (InTransitions.Num() == 0)
	{
		return false;
	}

	for (const USMTransitionInstance* Transition : InTransitions)
	{
		if (!Transition->IsTransitionFromAnyState())
		{
			return false;
		}
	}

	return true;
}

bool USMStateInstance_Base::AreAllOutgoingTransitionsFromAnAnyState() const
{
	TArray<USMTransitionInstance*> Transitions;
	GetOutgoingTransitions(Transitions);

	return AreAllTransitionsFromAnAnyState(MoveTemp(Transitions));
}

bool USMStateInstance_Base::AreAllIncomingTransitionsFromAnAnyState() const
{
	TArray<USMTransitionInstance*> Transitions;
	GetIncomingTransitions(Transitions);

	return AreAllTransitionsFromAnAnyState(MoveTemp(Transitions));
}

const FDateTime& USMStateInstance_Base::GetStartTime() const
{
	if (const FSMState_Base* Node = (FSMState_Base*)GetOwningNode())
	{
		return Node->GetStartTime();
	}

	static FDateTime EmptyDateTime(0);
	return EmptyDateTime;
}

float USMStateInstance_Base::GetServerTimeInState(bool& bOutUsedLocalTime) const
{
	if (const FSMState_Base* Node = (FSMState_Base*)GetOwningNode())
	{
		const float ServerTimeInState = Node->GetServerTimeInState();
		if (ServerTimeInState >= 0.f)
		{
			bOutUsedLocalTime = false;
			return ServerTimeInState;
		}

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
		const TScriptInterface<ISMStateMachineNetworkedInterface> NetworkedInterface = GetNetworkInterface();
		if (NetworkedInterface.GetObject())
		{
			if (NetworkedInterface->IsConfiguredForNetworking())
			{
				// TODO: Consider a logging option for this state class, and perhaps as a config setting.
				LD_LOG_INFO(
					TEXT("[%s::GetServerTimeInState] Could not establish an accurate server time, using local time."),
					*GetNodeName());
			}
		}
#endif
	}

	bOutUsedLocalTime = true;
	return GetTimeInState();
}

void USMStateInstance_Base::GetAllNodesOfType(TArray<USMNodeInstance*>& OutNodes,
                                              TSubclassOf<USMNodeInstance> NodeClass, bool bIncludeChildren,
                                              const TArray<UClass*>& StopIfTypeIsNot) const
{
	// If true then the node cycles back to itself at some point.
	if (OutNodes.Contains(this))
	{
		return;
	}

	// Only count if this node is of the right type.
	if ((bIncludeChildren && GetClass()->IsChildOf(NodeClass)) || GetClass() == NodeClass)
	{
		OutNodes.Add(const_cast<USMStateInstance_Base*>(this));
	}

	// Look for all connected nodes.
	if (const FSMState_Base* Node = (FSMState_Base*)GetOwningNode())
	{
		for (const FSMTransition* Transition : Node->GetOutgoingTransitions())
		{
			FSMState_Base* NextState = Transition->GetToState();
			if (const USMStateInstance_Base* Instance = Cast<USMStateInstance_Base>(NextState->GetOrCreateNodeInstance()))
			{
				// We break the search when a forbidden type is hit.
				if (StopIfTypeIsNot.Num() > 0)
				{
					bool bAllowed = false;
					for (const UClass* Class : StopIfTypeIsNot)
					{
						if (Instance->GetClass()->IsChildOf(Class))
						{
							bAllowed = true;
							break;
						}
					}

					if (!bAllowed)
					{
						continue;
					}
				}

				Instance->GetAllNodesOfType(OutNodes, NodeClass, bIncludeChildren, StopIfTypeIsNot);
			}
		}
	}
}

#if WITH_EDITORONLY_DATA
bool USMStateInstance_Base::IsRegisteredWithContextMenu() const
{
	return bRegisterWithContextMenu;
}
#endif

bool USMStateInstance_Base::GetAlwaysUpdate() const
{
	GET_NODE_DEFAULT_VALUE(FSMState_Base, bAlwaysUpdate);
}

void USMStateInstance_Base::SetAlwaysUpdate(const bool bValue)
{
	SET_NODE_DEFAULT_VALUE(FSMState_Base, bAlwaysUpdate, bValue);
}

bool USMStateInstance_Base::GetDisableTickTransitionEvaluation() const
{
	GET_NODE_DEFAULT_VALUE(FSMState_Base, bDisableTickTransitionEvaluation);
}

void USMStateInstance_Base::SetDisableTickTransitionEvaluation(const bool bValue)
{
	SET_NODE_DEFAULT_VALUE(FSMState_Base, bDisableTickTransitionEvaluation, bValue);
}

void USMStateInstance_Base::SetDefaultToParallel(const bool bValue)
{
	bDefaultToParallel = bValue;
}

bool USMStateInstance_Base::GetAllowParallelReentry() const
{
	GET_NODE_DEFAULT_VALUE(FSMState_Base, bAllowParallelReentry);
}

void USMStateInstance_Base::SetAllowParallelReentry(const bool bValue)
{
	SET_NODE_DEFAULT_VALUE(FSMState_Base, bAllowParallelReentry, bValue);
}

bool USMStateInstance_Base::GetStayActiveOnStateChange() const
{
	GET_NODE_DEFAULT_VALUE(FSMState_Base, bStayActiveOnStateChange);
}

void USMStateInstance_Base::SetStayActiveOnStateChange(const bool bValue)
{
	SET_NODE_DEFAULT_VALUE(FSMState_Base, bStayActiveOnStateChange, bValue);
}

bool USMStateInstance_Base::GetEvalTransitionsOnStart() const
{
	GET_NODE_DEFAULT_VALUE(FSMState_Base, bEvalTransitionsOnStart);
}

void USMStateInstance_Base::SetEvalTransitionsOnStart(const bool bValue)
{
	SET_NODE_DEFAULT_VALUE(FSMState_Base, bEvalTransitionsOnStart, bValue);
}

void USMStateInstance_Base::SetExcludeFromAnyState(const bool bValue)
{
	bExcludeFromAnyState = bValue;
}

bool USMStateInstance_Base::GetCanBeEndState() const
{
	GET_NODE_DEFAULT_VALUE(FSMState_Base, bCanBeEndState);
}

void USMStateInstance_Base::SetCanBeEndState(const bool bValue)
{
	SET_NODE_DEFAULT_VALUE(FSMState_Base, bCanBeEndState, bValue);
}

USMStateInstance::USMStateInstance() : Super()
{
}

void USMStateInstance::GetAllStateStackInstances(TArray<USMStateInstance_Base*>& StateStackInstances) const
{
	StateStackInstances.Reset();

	if (const FSMNode_Base* State = GetOwningNode())
	{
		const TArray<TObjectPtr<USMNodeInstance>>& StateStack = State->GetStackInstancesConst();
		StateStackInstances.Reserve(StateStack.Num());

		for (const TObjectPtr<USMNodeInstance>& Node : StateStack)
		{
			if (USMStateInstance_Base* StateInstance = Cast<USMStateInstance_Base>(Node))
			{
				StateStackInstances.Add(StateInstance);
			}
		}
	}
}

USMStateInstance_Base* USMStateInstance::GetStateInStack(int32 Index) const
{
	if (const FSMNode_Base* State = GetOwningNode())
	{
		const TArray<TObjectPtr<USMNodeInstance>>& StateStack = State->GetStackInstancesConst();
		if (Index >= 0 && Index < StateStack.Num())
		{
			return Cast<USMStateInstance_Base>(StateStack[Index]);
		}
	}

	return nullptr;
}

USMStateInstance_Base* USMStateInstance::GetStateInStackByClass(TSubclassOf<USMStateInstance> StateClass,
                                                                bool bIncludeChildren) const
{
	if (const FSMNode_Base* State = GetOwningNode())
	{
		const TArray<TObjectPtr<USMNodeInstance>>& StateStack = State->GetStackInstancesConst();
		for (const TObjectPtr<USMNodeInstance>& Node : StateStack)
		{
			if ((bIncludeChildren && Node->GetClass()->IsChildOf(StateClass)) || Node->GetClass() == StateClass)
			{
				return Cast<USMStateInstance_Base>(Node);
			}
		}
	}

	return nullptr;
}

USMStateInstance_Base* USMStateInstance::GetStackOwnerInstance() const
{
	if (const FSMNode_Base* State = GetOwningNode())
	{
		return Cast<USMStateInstance_Base>(const_cast<FSMNode_Base*>(State)->GetOrCreateNodeInstance());
	}

	return nullptr;
}

void USMStateInstance::GetAllStatesInStackOfClass(TSubclassOf<USMStateInstance> StateClass,
                                                  TArray<USMStateInstance_Base*>& StateStackInstances,
                                                  bool bIncludeChildren) const
{
	StateStackInstances.Reset();

	if (const FSMNode_Base* State = GetOwningNode())
	{
		const TArray<TObjectPtr<USMNodeInstance>>& StateStack = State->GetStackInstancesConst();
		for (const TObjectPtr<USMNodeInstance>& Node : StateStack)
		{
			if ((bIncludeChildren && Node->GetClass()->IsChildOf(StateClass)) || Node->GetClass() == StateClass)
			{
				if (USMStateInstance_Base* StateNode = Cast<USMStateInstance_Base>(Node))
				{
					StateStackInstances.Add(StateNode);
				}
			}
		}
	}
}

int32 USMStateInstance::GetStateIndexInStack(USMStateInstance_Base* StateInstance) const
{
	if (const FSMNode_Base* State = GetOwningNode())
	{
		return State->GetStackInstancesConst().IndexOfByKey(StateInstance);
	}

	return INDEX_NONE;
}

int32 USMStateInstance::GetStateStackCount() const
{
	if (const FSMNode_Base* State = GetOwningNode())
	{
		return State->GetStackInstancesConst().Num();
	}

	return 0;
}

USMStateInstance* USMStateInstance::AddStateToStack(TSubclassOf<USMStateInstance> StateClass, int32 StackIndex)
{
	if (const TScriptInterface<ISMEditorGraphNodeInterface> OwningEditorNode = GetOwningEditorGraphNode())
	{
		if (USMStateInstance* NewStackInstance = Cast<USMStateInstance>(
			OwningEditorNode->AddStackNode(StateClass, StackIndex)))
		{
			if (FSMNode_Base* OwningStructNode = GetOwningNodeAs<FSMNode_Base>())
			{
				NewStackInstance->SetOwningNode(OwningStructNode, true);
				USMUtils::InsertOrAddToArray<TObjectPtr<USMNodeInstance>>(OwningStructNode->GetStackInstances(), NewStackInstance,
				                                               StackIndex);
			}

			return NewStackInstance;
		}
	}

	return nullptr;
}

void USMStateInstance::RemoveStateFromStack(int32 StackIndex)
{
	if (const TScriptInterface<ISMEditorGraphNodeInterface> OwningEditorNode = GetOwningEditorGraphNode())
	{
		OwningEditorNode->RemoveStackNode(StackIndex);
		if (FSMNode_Base* OwningStructNode = GetOwningNodeAs<FSMNode_Base>())
		{
			USMUtils::RemoveAtOrPopFromArray(OwningStructNode->GetStackInstances(), StackIndex);
		}
	}
}

void USMStateInstance::ClearStateStack()
{
	if (const TScriptInterface<ISMEditorGraphNodeInterface> OwningEditorNode = GetOwningEditorGraphNode())
	{
		OwningEditorNode->ClearStackNodes();
		if (FSMNode_Base* OwningStructNode = GetOwningNodeAs<FSMNode_Base>())
		{
			OwningStructNode->GetStackInstances().Empty();
		}
	}
}
