// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Engine/EngineTypes.h"
#include "Misc/DateTime.h"

#include "SMTransactions.generated.h"

#define SM_ACTIVE_TIME_NOT_SET -1.f

UENUM()
enum class ESMTransactionType : uint8
{
	SM_Unknown,
	SM_Transition,
	SM_State,
	SM_FullSync,
	SM_Start,
	SM_Stop,
	SM_Initialize,
	SM_Shutdown
};

USTRUCT()
struct SMSYSTEM_API FSMTransaction_Base
{
	GENERATED_BODY()

	FSMTransaction_Base(): ServerRemoteRoleAtQueueTime(), TransactionType(), bOriginatedFromServer(0),
	                       bOriginatedFromThisClient(0), bRanLocally(0)
	{
	}

	explicit FSMTransaction_Base(ESMTransactionType InType) : ServerRemoteRoleAtQueueTime(), TransactionType(InType),
	                                                          bOriginatedFromServer(0),
	                                                          bOriginatedFromThisClient(0),
	                                                          bRanLocally(0)
	{
	}

	/** The remote role of the server when the call was queued. This is compared to the remote role
	 * at execution time to determine which connections should receive the RPC. */
	TEnumAsByte<ENetRole> ServerRemoteRoleAtQueueTime;
	
	/**
	 * The type of transaction, set automatically if required.
	 * It might be possible to always calculate this locally through RPC rather than send it.
	 */
	UPROPERTY()
	ESMTransactionType TransactionType;

	/**
	 * If the server made the decision to send this transaction. Used to distinguish multicast calls that can be
	 * executed from both the owning client or the server.
	 * TODO: If we switch to direct channel updates this could likely go away, or may need to be replaced by an ID.
	 */
	UPROPERTY()
	uint8 bOriginatedFromServer: 1;

	/** If this client created the call. Only valid for owning client. */
	uint8 bOriginatedFromThisClient: 1;
	
	/** If this transaction has run locally. */
	uint8 bRanLocally: 1;
};

/** Notify of initialization. */
USTRUCT()
struct SMSYSTEM_API FSMInitializeTransaction : public FSMTransaction_Base
{
	GENERATED_BODY()

	FSMInitializeTransaction() : FSMInitializeTransaction(nullptr)
	{
		
	}

	explicit FSMInitializeTransaction(UObject* InContext) : FSMTransaction_Base(ESMTransactionType::SM_Initialize),
	                                                        Context(InContext)
	{
		
	}

	UPROPERTY()
	TObjectPtr<UObject> Context;
};


/** Transition data to send across the network. */
USTRUCT()
struct SMSYSTEM_API FSMTransitionTransaction : public FSMTransaction_Base
{
	GENERATED_BODY()

	FSMTransitionTransaction() : FSMTransitionTransaction(FGuid())
	{
	}

	explicit FSMTransitionTransaction(const FGuid& InGuid)
		: FSMTransaction_Base(ESMTransactionType::SM_Transition), BaseGuid(InGuid),
		  Timestamp(0), ActiveTime(SM_ACTIVE_TIME_NOT_SET), bIsServer(0)
	{
	}

	/** The node path guid. */
	UPROPERTY(meta = (IgnoreForMemberInitializationTest))
	FGuid BaseGuid;

	/**
	 * Additional guids for a transaction. For transitions this can be source and destination states.
	 * When using conduits that information may be required and can't be calculated from a single transition.
	 */
	UPROPERTY()
	TArray<FGuid> AdditionalGuids;

	/** A UTC timestamp. Should be set manually. */
	UPROPERTY()
	FDateTime Timestamp;

	/** Source state's time in state. */
	UPROPERTY()
	float ActiveTime;

	/** Set from server during processing. */
	uint8 bIsServer: 1;

	FORCEINLINE bool AreAdditionalGuidsSetupForTransitions() const { return AdditionalGuids.Num() == 2; }
	FORCEINLINE const FGuid& GetTransitionSourceGuid() const
	{
		check(AreAdditionalGuidsSetupForTransitions());
		return AdditionalGuids[0];
	}

	FORCEINLINE const FGuid& GetTransitionDestinationGuid() const
	{
		check(AreAdditionalGuidsSetupForTransitions());
		return AdditionalGuids[1];
	}
};

/** States that need their active flag changed. */
USTRUCT()
struct SMSYSTEM_API FSMActivateStateTransaction : public FSMTransaction_Base
{
	GENERATED_BODY()

	FSMActivateStateTransaction() : FSMActivateStateTransaction(FGuid(), 0.f, false, false)
	{
	}

	FSMActivateStateTransaction(const FGuid& InGuid, const float InTimeInState,
	                          const bool bInIsActive, const bool bInSetAllParents) : FSMTransaction_Base(ESMTransactionType::SM_State),
	                                                    BaseGuid(InGuid)
	{
		TimeInState = InTimeInState;
		bIsActive = bInIsActive;
		bSetAllParents = bInSetAllParents;
	}

	/** The node path guid. */
	UPROPERTY(meta = (IgnoreForMemberInitializationTest))
	FGuid BaseGuid;

	UPROPERTY()
	float TimeInState;

	UPROPERTY()
	uint8 bIsActive: 1;

	UPROPERTY()
	uint8 bSetAllParents: 1;
};

/** Use for syncing the complete state of a state machine. */
USTRUCT()
struct SMSYSTEM_API FSMFullSyncStateTransaction : public FSMTransaction_Base
{
	GENERATED_BODY()

	FSMFullSyncStateTransaction() : FSMFullSyncStateTransaction(FGuid(), 0.f)
	{
	}

	FSMFullSyncStateTransaction(const FGuid& InGuid, const float InTimeInState) : FSMTransaction_Base(
		                                                                          ESMTransactionType::SM_FullSync),
	                                                                          BaseGuid(InGuid)
	{
		TimeInState = InTimeInState;
	}

	/** The node path guid. */
	UPROPERTY(meta = (IgnoreForMemberInitializationTest))
	FGuid BaseGuid;

	UPROPERTY()
	float TimeInState;
};

/** Use for syncing the complete state of a state machine. */
USTRUCT()
struct SMSYSTEM_API FSMFullSyncTransaction : public FSMTransaction_Base
{
	GENERATED_BODY()
	
	FSMFullSyncTransaction(): FSMTransaction_Base(ESMTransactionType::SM_FullSync), bHasStarted(0), bFromUserLoad(0),
	                          bForceFullRefresh(0)
	{
	}

	/** All states which should be active. */
	UPROPERTY()
	TArray<FSMFullSyncStateTransaction> ActiveStates;

	/** Has the state machine started already. */
	UPROPERTY()
	uint8 bHasStarted: 1;

	/**
	 * User has specified to load these states by calling LoadFromState.
	 * Consider removing and instead relying on a 'start' transaction with guids.
	 */
	UPROPERTY()
	uint8 bFromUserLoad: 1;

	/**
	 * Inform the receiver they should always accept the refresh and also update network settings.
	 * Useful for after a possession change.
	 */
	UPROPERTY()
	uint8 bForceFullRefresh: 1;
};
