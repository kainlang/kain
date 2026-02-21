// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "SMNode_Info.generated.h"

struct FSMState_Base;
struct FSMTransition;
struct FSMNode_Base;

USTRUCT(BlueprintInternalUseOnly)
struct SMSYSTEM_API FSMInfo_Base
{
	GENERATED_USTRUCT_BODY()

	FSMInfo_Base();
	FSMInfo_Base(const FSMNode_Base& Node);
	virtual ~FSMInfo_Base() = default;
	
	/** Friendly name of this node. Not guaranteed to be unique. */
	UPROPERTY(BlueprintReadOnly, Category = "State Machines")
	FString NodeName;
	
	/** Unique identifier calculated from a node's position in an instance. The PathGuid of FSMNode_Base. Compatible with TryGetInfo. */
	UPROPERTY(BlueprintReadOnly, Category = "State Machines")
	FGuid Guid;

	/** The state machine's PathGuid owning this node. Compatible with TryGetStateInfo.*/
	UPROPERTY(BlueprintReadOnly, Category = "State Machines")
	FGuid OwnerGuid;

	/** Guid assigned to this node during creation. May not be unique if this node is referenced multiple times. */
	UPROPERTY(BlueprintReadOnly, Category = "State Machines")
	FGuid NodeGuid;

	/** Guid assigned to the parent node during creation. May not be unique if this node is referenced multiple times. */
	UPROPERTY(BlueprintReadOnly, Category = "State Machines")
	FGuid OwnerNodeGuid;

	/**
	 * The node instance for this class. This will either be a default StateInstance or TransitionInstance, or a user defined one.
	 * WARNING: This may now be null since the instance is only loaded on demand.
	 *
	 * @deprecated Use USMInstance::GetNodeInstanceByGuid() on the root state machine instance and pass in the Guid.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "State Machines", meta=(DeprecatedProperty, DeprecationMessage="Use GetNodeInstanceByGuid() on the root state machine instance and pass in the Guid."))
	TObjectPtr<class USMNodeInstance> NodeInstance;

	virtual FString ToString() const;
};

/**
 * [Logic Driver] Read only information of a transition.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Transition Information"))
struct SMSYSTEM_API FSMTransitionInfo : public FSMInfo_Base
{
	GENERATED_USTRUCT_BODY()

	FSMTransitionInfo();
	FSMTransitionInfo(const FSMTransition& Transition);

	/** Use TryGetStateInfo from the instance to retrieve this state information. */
	UPROPERTY(BlueprintReadOnly, Category = "State Machines")
	FGuid FromStateGuid;

	/** Use TryGetStateInfo from the instance to retrieve this state information. */
	UPROPERTY(BlueprintReadOnly, Category = "State Machines")
	FGuid ToStateGuid;

	/** The assigned transition priority. */
	UPROPERTY(BlueprintReadOnly, Category = "State Machines")
	int32 Priority;

	/** The last networked timestamp. Only valid in network environments. */
	UPROPERTY(BlueprintReadOnly, Category = "State Machines")
	FDateTime LastNetworkTimestamp;

	const FSMTransition* OwningTransition;

	virtual FString ToString() const override;
};

/**
 * [Logic Driver] Read only information of a state.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "State Information"))
struct SMSYSTEM_API FSMStateInfo : public FSMInfo_Base
{
	GENERATED_USTRUCT_BODY()

	FSMStateInfo();
	FSMStateInfo(const FSMState_Base& State);

	/** All of the transitions leading out of this state. */
	UPROPERTY(BlueprintReadOnly, Category = "State Machines")
	TArray<FSMTransitionInfo> OutgoingTransitions;

	/** If this state is considered an end state. */
	UPROPERTY(BlueprintReadOnly, Category = "State Machines")
	bool bIsEndState;

	const FSMState_Base* OwningState;
};

/**
 * [Logic Driver] History summary for a state.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "State History"))
struct SMSYSTEM_API FSMStateHistory
{
	GENERATED_BODY()

	FSMStateHistory(): StartTime(ForceInitToZero), TimeInState(0), ServerTimeInState(0)
	{
	}

	FSMStateHistory(const FGuid& InStateGuid, const FDateTime& InStartTime, float InTimeInState, float InServerTimeInState):
		StateGuid(InStateGuid),
		StartTime(InStartTime),
		TimeInState(InTimeInState),
		ServerTimeInState(InServerTimeInState)
	{
	}

	/** The state guid which can be used with the owning USMInstance to lookup the full state object. */
	UPROPERTY(BlueprintReadOnly, Category = "State Machines")
	FGuid StateGuid;

	/** The timestamp from when the state started. */
	UPROPERTY(BlueprintReadOnly, Category = "State Machines")
	FDateTime StartTime;

	/** The total time spent in the state. */
	UPROPERTY(BlueprintReadOnly, Category = "State Machines")
	float TimeInState;

	/** The total time spent in the state according to the server. */
	UPROPERTY(BlueprintReadOnly, Category = "State Machines")
	float ServerTimeInState;

	bool operator==(const FSMStateHistory& Other) const
	{
		return this->StateGuid == Other.StateGuid &&
			this->StartTime == Other.StartTime &&
			this->TimeInState == Other.TimeInState &&
			this->ServerTimeInState == Other.ServerTimeInState;
	}
};
