// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMTransitionInstance.h"

#include "SMInstance.h"
#include "SMStateInstance.h"
#include "SMTransition.h"

USMTransitionInstance::USMTransitionInstance() : Super(), PriorityOrder(0), bRunParallel(false),
                                                 bEvalIfNextStateActive(true),
                                                 bCanEvaluate(true), bCanEvaluateFromEvent(true),
                                                 bCanEvalWithStartState(true)
{
#if WITH_EDITORONLY_DATA
	bAutoPlaceTransitionIfRulesPass = true;
	IconLocationPercentage = 0.5f;
	bShowBackgroundOnCustomIcon = false;
	bHideIcon = false;
#endif
}

USMStateInstance_Base* USMTransitionInstance::GetPreviousStateInstance() const
{
	if (FSMTransition* Transition = (FSMTransition*)GetOwningNode())
	{
		if (FSMState_Base* PrevState = Transition->GetFromState())
		{
			return Cast<USMStateInstance_Base>(PrevState->GetOrCreateNodeInstance());
		}
	}

	return nullptr;
}

USMStateInstance_Base* USMTransitionInstance::GetNextStateInstance() const
{
	if (FSMTransition* Transition = (FSMTransition*)GetOwningNode())
	{
		if (FSMState_Base* NextState = Transition->GetToState())
		{
			return Cast<USMStateInstance_Base>(NextState->GetOrCreateNodeInstance());
		}
	}

	return nullptr;
}

USMStateInstance_Base* USMTransitionInstance::GetSourceStateForActiveTransition() const
{
	if (FSMTransition* Transition = (FSMTransition*)GetOwningNode())
	{
		if (Transition->SourceState)
		{
			return Cast<USMStateInstance_Base>(Transition->SourceState->GetOrCreateNodeInstance());
		}
	}

	return nullptr;
}

USMStateInstance_Base* USMTransitionInstance::GetDestinationStateForActiveTransition() const
{
	if (FSMTransition* Transition = (FSMTransition*)GetOwningNode())
	{
		if (Transition->DestinationState)
		{
			return Cast<USMStateInstance_Base>(Transition->DestinationState->GetOrCreateNodeInstance());
		}
	}

	return nullptr;
}

void USMTransitionInstance::GetTransitionInfo(FSMTransitionInfo& Transition) const
{
	if (FSMTransition* TransitionNode = (FSMTransition*)GetOwningNode())
	{
		Transition = FSMTransitionInfo(*TransitionNode);
	}
	else
	{
		Transition = FSMTransitionInfo();
	}
}

const FDateTime& USMTransitionInstance::GetServerTimestamp() const
{
	if (FSMTransition* TransitionNode = (FSMTransition*)GetOwningNode())
	{
		return TransitionNode->LastNetworkTimestamp;
	}

	static FDateTime EmptyTimestamp(0);
	return EmptyTimestamp;
}

bool USMTransitionInstance::DoesTransitionPass() const
{
	if (FSMTransition* TransitionNode = (FSMTransition*)GetOwningNode())
	{
		return TransitionNode->DoesTransitionPass();
	}

	return false;
}

bool USMTransitionInstance::IsTransitionFromAnyState() const
{
	GET_NODE_STRUCT_VALUE(FSMTransition, bFromAnyState);
	return false;
}

bool USMTransitionInstance::IsTransitionFromLinkState() const
{
	GET_NODE_STRUCT_VALUE(FSMTransition, bFromLinkState);
	return false;
}

bool USMTransitionInstance::EvaluateFromManuallyBoundEvent()
{
	bool bResult = false;

	const bool bOriginalEvalValue = GetCanEvaluate();
	SetCanEvaluate(true);
	USMInstance* OwningStateMachine = GetStateMachineInstance(true);
	if (OwningStateMachine)
	{
		bResult = OwningStateMachine->EvaluateAndTakeTransitionChain(this);
	}
	SetCanEvaluate(bOriginalEvalValue);

	return bResult;
}

void USMTransitionInstance::GetAllTransitionStackInstances(
	TArray<USMTransitionInstance*>& TransitionStackInstances) const
{
	TransitionStackInstances.Reset();
	
	if (const FSMNode_Base* Transition = GetOwningNode())
	{
		const TArray<USMNodeInstance*>& TransitionStack = Transition->GetStackInstancesConst();
		TransitionStackInstances.Reserve(TransitionStack.Num());
		
		for (USMNodeInstance* Node : TransitionStack)
		{
			if (USMTransitionInstance* TransitionInstance = Cast<USMTransitionInstance>(Node))
			{
				TransitionStackInstances.Add(TransitionInstance);
			}
		}
	}
}

USMTransitionInstance* USMTransitionInstance::GetTransitionInStack(int32 Index) const
{
	if (const FSMNode_Base* Transition = GetOwningNode())
	{
		const TArray<USMNodeInstance*>& Stack = Transition->GetStackInstancesConst();
		if (Index >= 0 && Index < Stack.Num())
		{
			return Cast<USMTransitionInstance>(Stack[Index]);
		}
	}

	return nullptr;
}

USMTransitionInstance* USMTransitionInstance::GetTransitionInStackByClass(
	TSubclassOf<USMTransitionInstance> TransitionClass, bool bIncludeChildren) const
{
	if (const FSMNode_Base* Transition = GetOwningNode())
	{
		const TArray<USMNodeInstance*>& TransitionStack = Transition->GetStackInstancesConst();
		for (USMNodeInstance* Node : TransitionStack)
		{
			if ((bIncludeChildren && Node->GetClass()->IsChildOf(TransitionClass)) || Node->GetClass() == TransitionClass)
			{
				return Cast<USMTransitionInstance>(Node);
			}
		}
	}

	return nullptr;
}

USMTransitionInstance* USMTransitionInstance::GetStackOwnerInstance() const
{
	if (const FSMNode_Base* Transition = GetOwningNode())
	{
		return Cast<USMTransitionInstance>(const_cast<FSMNode_Base*>(Transition)->GetOrCreateNodeInstance());
	}

	return nullptr;
}

void USMTransitionInstance::GetAllTransitionsInStackOfClass(TSubclassOf<USMTransitionInstance> TransitionClass,
	TArray<USMTransitionInstance*>& TransitionStackInstances, bool bIncludeChildren) const
{
	TransitionStackInstances.Reset();
	
	if (const FSMNode_Base* Transition = GetOwningNode())
	{
		const TArray<USMNodeInstance*>& TransitionStack = Transition->GetStackInstancesConst();
		for (USMNodeInstance* Node : TransitionStack)
		{
			if ((bIncludeChildren && Node->GetClass()->IsChildOf(TransitionClass)) || Node->GetClass() == TransitionClass)
			{
				if (USMTransitionInstance* TransitionNode = Cast<USMTransitionInstance>(Node))
				{
					TransitionStackInstances.Add(TransitionNode);
				}
			}
		}
	}
}

int32 USMTransitionInstance::GetTransitionIndexInStack(USMTransitionInstance* TransitionInstance) const
{
	if (const FSMNode_Base* Transition = GetOwningNode())
	{
		return Transition->GetStackInstancesConst().IndexOfByKey(TransitionInstance);
	}

	return INDEX_NONE;
}

int32 USMTransitionInstance::GetTransitionStackCount() const
{
	if (const FSMNode_Base* Transition = GetOwningNode())
	{
		return Transition->GetStackInstancesConst().Num();
	}

	return 0;
}

void USMTransitionInstance::SetCanEvaluate(const bool bValue)
{
	SET_NODE_DEFAULT_VALUE(FSMTransition, bCanEvaluate, bValue);
}

bool USMTransitionInstance::GetCanEvaluate() const
{
	GET_NODE_DEFAULT_VALUE(FSMTransition, bCanEvaluate);
}

int32 USMTransitionInstance::GetPriorityOrder() const
{
	GET_NODE_DEFAULT_VALUE_DIF_VAR(FSMTransition, PriorityOrder, Priority);
}

void USMTransitionInstance::SetPriorityOrder(const int32 Value)
{
	SET_NODE_DEFAULT_VALUE_DIF_VAR(FSMTransition, PriorityOrder, Priority, Value);
}

bool USMTransitionInstance::GetRunParallel() const
{
	GET_NODE_DEFAULT_VALUE(FSMTransition, bRunParallel);
}

void USMTransitionInstance::SetRunParallel(const bool bValue)
{
	SET_NODE_DEFAULT_VALUE(FSMTransition, bRunParallel, bValue);
}

bool USMTransitionInstance::GetEvalIfNextStateActive() const
{
	GET_NODE_DEFAULT_VALUE(FSMTransition, bEvalIfNextStateActive);
}

void USMTransitionInstance::SetEvalIfNextStateActive(const bool bValue)
{
	SET_NODE_DEFAULT_VALUE(FSMTransition, bEvalIfNextStateActive, bValue);
}

bool USMTransitionInstance::GetCanEvaluateFromEvent() const
{
	GET_NODE_DEFAULT_VALUE(FSMTransition, bCanEvaluateFromEvent);
}

void USMTransitionInstance::SetCanEvaluateFromEvent(const bool bValue)
{
	SET_NODE_DEFAULT_VALUE(FSMTransition, bCanEvaluateFromEvent, bValue);
}

bool USMTransitionInstance::GetCanEvalWithStartState() const
{
	GET_NODE_DEFAULT_VALUE(FSMTransition, bCanEvalWithStartState);
}

void USMTransitionInstance::SetCanEvalWithStartState(const bool bValue)
{
	SET_NODE_DEFAULT_VALUE(FSMTransition, bCanEvalWithStartState, bValue);
}
