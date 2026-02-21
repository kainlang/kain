// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMNode_Info.h"

#include "SMInstance.h"
#include "SMTransition.h"


FSMInfo_Base::FSMInfo_Base(): NodeInstance(nullptr)
{
}

FSMInfo_Base::FSMInfo_Base(const FSMNode_Base& Node)
{
	this->Guid = Node.GetGuid();
	this->OwnerGuid = Node.GetOwnerNode() ? Node.GetOwnerNode()->GetGuid() : FGuid();
	this->NodeName = Node.GetNodeName();

	this->NodeGuid = Node.GetNodeGuid();
	this->OwnerNodeGuid = Node.GetOwnerNodeGuid();

	this->NodeInstance = Node.GetNodeInstance();
}

FString FSMInfo_Base::ToString() const
{
	return !Guid.IsValid() ? "(null)" : "(" + NodeName + ")";
}

FSMTransitionInfo::FSMTransitionInfo() : Super()
{
	Priority = 0;
	LastNetworkTimestamp = FDateTime(0);
	OwningTransition = nullptr;
}

FSMTransitionInfo::FSMTransitionInfo(const FSMTransition& Transition) : Super(Transition)
{
	FromStateGuid = Transition.GetFromState()->GetGuid();
	ToStateGuid = Transition.GetToState()->GetGuid();
	Priority = Transition.Priority;
	LastNetworkTimestamp = Transition.LastNetworkTimestamp;
	OwningTransition = &Transition;
}

FString FSMTransitionInfo::ToString() const
{
	FString Result = "";
	if (!OwningTransition)
	{
		return Result;
	}

	if (USMInstance* Instance = OwningTransition->GetOwningInstance())
	{
		bool bSuccess;
		
		FSMStateInfo FromState;
		Instance->TryGetStateInfo(FromStateGuid, FromState, bSuccess);
		const FString FromStateStr = FromState.ToString();

		FSMStateInfo ToState;
		Instance->TryGetStateInfo(ToStateGuid, ToState, bSuccess);
		const FString ToStateStr = ToState.ToString();

		Result = FString::Printf(TEXT("from %s to %s by transition %s with priority %s."), *FromStateStr, *ToStateStr,
			OwningTransition && OwningTransition->GetNodeInstanceClass() ? *OwningTransition->GetNodeInstanceClass()->GetName() : TEXT("unknown"),
			*FString::FromInt(OwningTransition ? OwningTransition->Priority : 0));
	}

	return Result;
}

FSMStateInfo::FSMStateInfo() : Super(), bIsEndState(false)
{
	OwningState = nullptr;
}

FSMStateInfo::FSMStateInfo(const FSMState_Base& State) : Super(State)
{
	this->bIsEndState = State.IsEndState();
	
	for (FSMTransition* Transition : State.GetOutgoingTransitions())
	{
		OutgoingTransitions.Add(FSMTransitionInfo(*Transition));
	}

	OwningState = &State;
}
