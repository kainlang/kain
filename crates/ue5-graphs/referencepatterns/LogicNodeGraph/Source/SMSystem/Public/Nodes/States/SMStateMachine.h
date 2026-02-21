// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMState.h"

#include "SMStateMachine.generated.h"

class ISMStateMachineNetworkedInterface;
struct FSMTransitionTransaction;

/**
 * State machines contain states and transitions. When a transition succeeds the current state advances to the next.
 * FSMStateMachine is also considered a state since it inherits from FSMState_Base, making it possible to nest state machines.
 * 
 * When configured as a reference this will defer handling to the SMInstance (or in some cases the RootStateMachine) of the referenced Blueprint.
 */
USTRUCT(BlueprintInternalUseOnly)
struct SMSYSTEM_API FSMStateMachine : public FSMState_Base
{
	GENERATED_USTRUCT_BODY()
	
public:
	/** If this has additional logic associated with it. */
	UPROPERTY()
	uint8 bHasAdditionalLogic: 1;

	/** The current state is not cleared on end and will be resumed on start. */
	UPROPERTY()
	uint8 bReuseCurrentState: 1;

	/** Don't reuse if the state machine is in an end state. */
	UPROPERTY()
	uint8 bOnlyReuseIfNotEndState: 1;

	/** Allows the state machine reference to tick on its own. */
	UPROPERTY()
	uint8 bAllowIndependentTick: 1;

	/** Notifies instance to call tick on manual update. Only valid for references. */
	UPROPERTY()
	uint8 bCallReferenceTickOnManualUpdate: 1;

	/** Wait for an end state to be hit before evaluating transitions or being considered an end state itself. */
	UPROPERTY()
	uint8 bWaitForEndState: 1;

public:
	FSMStateMachine();
	// FSMState_Base
	virtual void Initialize(UObject* Instance) override;
protected:
	virtual void InitializeFunctionHandlers() override;
public:
	virtual void InitializeGraphFunctions() override;
	virtual void Reset() override;
	virtual void ResetReadStates() override;
	virtual bool StartState() override;
	virtual bool UpdateState(float DeltaSeconds) override;
	virtual bool EndState(float DeltaSeconds, const FSMTransition* TransitionToTake = nullptr) override;
	virtual void ExecuteInitializeNodes() override;
	virtual void ExecuteShutdownNodes() override;
	virtual void OnStartedByInstance(USMInstance* Instance) override;
	virtual void OnStoppedByInstance(USMInstance* Instance) override;
	virtual void CalculatePathGuid(TMap<FString, int32>& MappedPaths, bool bUseGuidCache = false) override;
	virtual void RunConstructionScripts() override;
protected:
	virtual void NotifyInstanceStateHasStarted() override;
public:
	/** @deprecated Use IsStateMachineInEndState() instead. */
	UE_DEPRECATED(5.1, "This function is deprecated. Use IsStateMachineInEndState().")
	virtual bool IsInEndState() const override;
	virtual bool IsEndState() const override;
	virtual bool IsStateMachine() const override { return true; }
	virtual bool IsNodeInstanceClassCompatible(UClass* NewNodeInstanceClass) const override;
	virtual USMNodeInstance* GetNodeInstance() const override;
	virtual USMNodeInstance* GetOrCreateNodeInstance() override;
	virtual bool CanEverCreateNodeInstance() const override { return !IsReferencedByInstance; }
	virtual UClass* GetDefaultNodeInstanceClass() const override;
	virtual FSMNode_Base* GetOwnerNode() const override;
	virtual void SetStartTime(const FDateTime& InStartTime) override;
	virtual void SetEndTime(const FDateTime& InEndTime) override;
	virtual void SetServerTimeInState(float InTime) override;
	// ~FSMState_Base

	/** If this state machine is in an end state or has no active states. */
	bool IsStateMachineInEndState() const;

	/**
	 * If the state machine has reached an end state for its current run.
	 * This can be set when an active state's IsEndState() is true, or if the state machine has deactivated all states.
	 */
	bool HasStateMachineReachedEndStateForRun() const { return bHasReachedEndStateForRun; }
	
	/** Add a state to this State Machine. */
	void AddState(FSMState_Base* State);

	/** Add a transition to this State Machine. */
	void AddTransition(FSMTransition* Transition);

	/** The first state to execute. Even with parallel states there is always a single root entry point. */
	void AddInitialState(FSMState_Base* State);

	/** These states will replace the initial state, but once started will be cleared.
	 * @param State A non null state will be added to the set as long as it exists within the state machine. A null value is ignored.
	 */
	void AddTemporaryInitialState(FSMState_Base* State);

	/** Removes all temporary initial states. */
	void ClearTemporaryInitialStates(bool bRecursive = false);

	/** Loads temporary states if not already loaded and start them. */
	void SetFromTemporaryInitialStates();

	/** Checks if the given state is contained within the active states. */
	bool ContainsActiveState(FSMState_Base* StateToCheck) const;
	
	/** Checks if there are any active states. */
	bool HasActiveStates() const;

	/** Checks if there any temporary initial states set. */
	bool HasTemporaryEntryStates() const;
	
	/** The current state of this State Machine. In the event of multiple parallel states the first active state is returned. */
	FSMState_Base* GetSingleActiveState() const;

	/** Returns a copy of all active states specific to this FSM making it safe for iteration modification. */
	TArray<FSMState_Base*> GetActiveStates() const;

	/** Return a list of all active states recursively searching nested state machines. */
	TArray<FSMState_Base*> GetAllNestedActiveStates() const;

	struct FGetNodeArgs
	{
		/** If nested state machines should have their nodes returned as well. */
		bool bIncludeNested;
		/** Don't find nested nodes inside of references. Requires bIncludeNested. */
		bool bSkipReferences;
		/** If this state machine node should be added to the results. */
		bool bIncludeSelf;

		FGetNodeArgs() :
		bIncludeNested(false),
		bSkipReferences(false),
		bIncludeSelf(false) {}
	};

	/**
	 * Retrieve nodes of all types.
	 * @param InArgs Arguments to filter out nodes.
	 */
	TArray<FSMNode_Base*> GetAllNodes(const FGetNodeArgs& InArgs = FGetNodeArgs()) const;

	UE_DEPRECATED(4.26, "Use `GetAllNodes` that takes the FGetNodeArgs arguments instead. `bForwardToReference` is no longer supported.")
	TArray<FSMNode_Base*> GetAllNodes(bool bIncludeNested, bool bForwardToReference = false) const;

	/** Retrieve nodes of all state types. */
	const TArray<FSMState_Base*>& GetStates() const;

	/** Retrieve nodes of only transitions. */
	const TArray<FSMTransition*>& GetTransitions() const;

	/** Returns only the original entry states. */
	const TArray<FSMState_Base*>& GetEntryStates() const;
	
	/** The entry state of this state machine. Returns either the temporary or default. Possible to be greater than 0 when loading from temporary parallel states. */
	TArray<FSMState_Base*> GetInitialStates() const;

	/** Returns either null or the first initial state. Only use this if you know the FSM doesn't have parallel states.  */
	FSMState_Base* GetSingleInitialState() const;

	/** Returns either null or the first initial temporary state. Only use this if you know the FSM doesn't have parallel states.  */
	FSMState_Base* GetSingleInitialTemporaryState() const;

	/** Returns all nested temporary states if they are set. */
	TArray<FSMState_Base*> GetAllNestedInitialTemporaryStates() const;
	
	/** Linear search recursively through all states and state machines. */
	FSMState_Base* FindState(const FGuid& StateGuid) const;
	
	/** Determine how to process transitions and states in different environments. */
	void SetNetworkedConditions(TScriptInterface<ISMStateMachineNetworkedInterface> InNetworkInterface,
		bool bEvaluateTransitions, bool bTakeTransitions, bool bCanExecuteStateLogic);

	struct FStateScopingArgs
	{
		/** Only these specific states should be processed. If empty the Active states are used. */
		TArray<FSMState_Base*> ScopedToStates;

		/**
		 * States just started this frame. States added here will be assumed it's safe to check their transitions this frame.
		 * If empty, states being processed will have TryStartState() called on them.
		 */
		TSet<FSMState_Base*> StatesJustStarted;
	};

	/**
	 * Determine if the current state should be stopped or started or evaluate a transition.
	 * 
	 * @param DeltaSeconds Time since last update.
	 * @param bForceTransitionEvaluationOnly The update (Tick) logic for a state won't be called unless the state is ending and bAlwaysUpdate is checked. Start and End may still be called.
	 * @param InCurrentRunGuid A guid unique to this call stack. Leave empty, for internal use.
	 * @param InStateScopingArgs Limit state processing to select states.
	 */
	void ProcessStates(float DeltaSeconds, bool bForceTransitionEvaluationOnly = false, const FGuid& InCurrentRunGuid = FGuid(),
		const FStateScopingArgs& InStateScopingArgs = FStateScopingArgs());

	/**
	 * Attempt to take a transition. Does not evaluate the transition. Returns true if successful.
	 * 
	 * @param Transition The transition to process.
	 * @param SourceState The original state to transition from.
	 * @param DestinationState the final state to transition to.
	 * @param Transaction A network transaction if one exists. May be null.
	 * @param DeltaSeconds The time in seconds since the last update.
	 * @param CurrentTime The current UTC time. Only utilized in networked environments for recording time stamps.
	 */
	bool ProcessTransition(FSMTransition* Transition, FSMState_Base* SourceState, FSMState_Base* DestinationState,
		const FSMTransitionTransaction* Transaction, float DeltaSeconds, FDateTime* CurrentTime = nullptr);

	/**
	 * Evaluate an entire transition chain discovering the path to take. If an entire chain passes
	 * then switch to the destination state.
	 *
	 * A transition chain is the first path that evaluates to true between two states, consisting of all transitions
	 * and conduits that are configured to eval with transitions.
	 *
	 * This method fails if the state machine isn't state change authoritative. The destination state won't
	 * become active if this state machine doesn't have local state change authority.
	 *
	 * If the method passes each transition taken will be replicated in order.
	 *
	 * @param InFirstTransition The transition struct, which should be the first part of a transition chain.
	 *
	 * @return True if the chain succeeded evaluation.
	 */
	bool EvaluateAndTakeTransitionChain(FSMTransition* InFirstTransition);

	/**
	 * Take a transition chain. Does not evaluate, but adheres to all normal state change behavior.
	 *
	 * @param InTransitionChain The transition chain to take.
	 * @return True if taken.
	 */
	bool TakeTransitionChain(const TArray<FSMTransition*>& InTransitionChain);
	
	/**
	 * Try starting the given state.
	 *
	 * @param InState The state to start.
	 * @param bOutSafeToCheckTransitions [Out] If it is safe to check transitions on this state this frame.
	 * @return True if the state was started, false if it was already started or cannot start.
	 */
	bool TryStartState(FSMState_Base* InState, bool* bOutSafeToCheckTransitions = nullptr);

	/**
	 * Try taking the given transition chain to the end destination state.
	 *
	 * @param InTransitionChain A single transition, or multiple transitions with conduits.
	 * @param DeltaSeconds Delta seconds to apply to ProcessTransition.
	 * @param bStateJustStarted If the source state has started this frame.
	 * @param OutDestinationState [Out] The destination state at the end of chain, only set if the transition chain was taken.
	 *
	 * @return True if the transition chain was taken, false if not.
	 */
	bool TryTakeTransitionChain(const TArray<FSMTransition*>& InTransitionChain, float DeltaSeconds, bool bStateJustStarted = false,
		FSMState_Base** OutDestinationState = nullptr);
	
	/** External callers should check this before calling ProcessTransition(). */
	bool CanProcessExternalTransition() const;
	
	/** State Machine is currently waiting for a transition update from the server. */
	bool IsWaitingForUpdate() const { return bWaitingForTransitionUpdate; }

	/**
	 * When true the current state is reused on exit/reentry.
	 * When false the current state is cleared on end and the initial state used on reentry.
	 */
	void SetReuseCurrentState(bool bValue, bool bOnlyWhenNotInEndState);

	/** Is the current state reused or reset on exit/reentry. */
	bool CanReuseCurrentState() const;

	void SetClassReference(UClass* ClassReference);
	UClass* GetClassReference() const { return ReferencedStateMachineClass; }

	void SetInstanceReference(USMInstance* InstanceReference);
	USMInstance* GetInstanceReference() const { return ReferencedStateMachine; }

	void SetReferencedTemplateName(const FName& Name);
	const FName& GetReferencedTemplateName() const { return ReferencedTemplateName; }
	
	void SetReferencedBy(USMInstance* FromInstance, FSMStateMachine* FromStateMachine);
	
	/** The instance referencing this state machine. */
	USMInstance* GetReferencedByInstance() const { return IsReferencedByInstance; }

	/** The exact state machine referencing this, if any. */
	FSMStateMachine* GetReferencedByStateMachine() const { return IsReferencedByStateMachine; }

	/** Set a variable name of the owning SMInstance to use for a dynamic class lookup. */
	void SetDynamicReferenceVariableName(const FName& InVariableName) { DynamicStateMachineReferenceVariable = InVariableName; }

	/** Get a variable name to use with the owning SMInstance for dynamic class lookup. */
	const FName& GetDynamicReferenceVariableName() const { return DynamicStateMachineReferenceVariable; }

	/** If this is a dynamic state machine reference. */
	bool IsDynamicReference() const { return !DynamicStateMachineReferenceVariable.IsNone() && ReferencedStateMachine != nullptr; }
	
	/** True only if this FSM is networked. */
	bool IsNetworked() const { return NetworkedInterface.GetObject() != nullptr; }

	/** Find the network interface if one is assigned and active. */
	ISMStateMachineNetworkedInterface* TryGetNetworkInterfaceIfNetworked() const;

	/** All contained states mapped out by their name, limited to this FSM scope. */
	const TMap<FString, FSMState_Base*>& GetStateNameMap() const;

	/**
	 * Forcibly add an active state.
	 *
	 * @param State The state to add to the active list.
	 */
	void AddActiveState(FSMState_Base* State);
	
	/**
	 * Forcibly remove an active state.
	 *
	 * @param State The state to remove from the active list.
	 */
	void RemoveActiveState(FSMState_Base* State);

#if WITH_EDITOR
	virtual void ResetGeneratedValues() override;
#endif

protected:
	/**
	 * Switches the current state and notifies the owning instance.
	 *
	 * @param ToState: The state we should be switching to. May be null.
	 * @param FromState: The state we are switching from. If not null it will be removed from the active list if bStayActiveOnStateChange is false.
	 * @param SourceState: The original source state we are transitioning from. This can be different from the FromState if transition conduits are involved.
	 */
	void SetCurrentState(FSMState_Base* ToState, FSMState_Base* FromState, FSMState_Base* SourceState = nullptr);

	/** Check if an end state has been reached, fire notifications, and traverse any owners that are waiting for an end state. */
	void TryNotifyEndStateReached();
	
private:
	UPROPERTY()
	TScriptInterface<ISMStateMachineNetworkedInterface> NetworkedInterface;
	
	TArray<FSMState_Base*> States;
	TArray<FSMTransition*> Transitions;
	
	/** The default root entry point. */
	TArray<FSMState_Base*> EntryStates;

	/** Entry states that are temporary and used for loading purposes. */
	TArray<FSMState_Base*> TemporaryEntryStates;

	/** In most cases this should be of size 0 or 1. Greater than 1 implies the sm is configured for multiple active states.
	 *  Array container needed for exact order when adding parallel states. O(n) operations should be acceptable for average number
	 *  of active states and only on state changes. */
	TArray<FSMState_Base*> ActiveStates;
	
	/** All contained states, mapped by their name. */
	TMap<FString, FSMState_Base*> StateNameMap;

	/** Keeps track of states currently processing for the given FSM scope.
		Helps with possible infinite recursion when using multiple states that can re-enter each other. */
	TMap<FGuid, TSet<FSMState_Base*>> ProcessingStates;
	
	UPROPERTY()
	TObjectPtr<UClass> ReferencedStateMachineClass;

	/** The name of a template archetype to use when constructing a reference. This allows default values be passed into the reference. */
	UPROPERTY()
	FName ReferencedTemplateName;

	/** The name of a variable stored on the owning SMInstance that should be used to find the class for this reference. */
	UPROPERTY()
	FName DynamicStateMachineReferenceVariable;
	
	/** This state machine is referencing an instance. */
	UPROPERTY()
	TObjectPtr<USMInstance> ReferencedStateMachine;
	
	/** This state machine is being referenced from an instance. */
	UPROPERTY()
	TObjectPtr<USMInstance> IsReferencedByInstance;

	/** The state machine referencing this state machine, if any. */
	FSMStateMachine* IsReferencedByStateMachine;

	/** Current time spent waiting for an update. */
	float TimeSpentWaitingForUpdate;

	/** Is currently waiting for an update. */
	uint8 bWaitingForTransitionUpdate: 1;

	/** Can this instance even evaluate transitions. */
	uint8 bCanEvaluateTransitions: 1;

	/** Once evaluated can this instance take the transition. */
	uint8 bCanTakeTransitions: 1;

	/** True if the state machine has reached an end state at least once and is reset on node initialize. */
	uint8 bHasReachedEndStateForRun: 1;
};