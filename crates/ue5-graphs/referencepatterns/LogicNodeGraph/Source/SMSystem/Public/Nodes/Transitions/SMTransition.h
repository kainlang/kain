// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMNode_Base.h"

#include "SMTransition.generated.h"

struct FSMState_Base;
struct FSMState;

/**
 * Transitions determine when an FSM can exit one state and advance to the next.
 */
USTRUCT(BlueprintInternalUseOnly)
struct SMSYSTEM_API FSMTransition : public FSMNode_Base
{
	GENERATED_USTRUCT_BODY()

public:

	/** Lower number means this transition is checked sooner. */
	UPROPERTY(EditAnywhere, Category = "State Machines")
	int32 Priority;

	/** Set from graph execution. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Result, meta = (AlwaysAsPin))
	uint16 bCanEnterTransition: 1;

	/** Set from graph execution when updated by event. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Result, meta = (AlwaysAsPin))
	uint16 bCanEnterTransitionFromEvent: 1;

	/** Set internally and from auto bound events. True only during evaluation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Result, meta = (AlwaysAsPin))
	uint16 bIsEvaluating: 1;
	
	/** Set from graph execution or configurable from details panel. Must be true for the transition to be evaluated conditionally. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Transition)
	uint16 bCanEvaluate: 1;

	/** Allows auto-bound events to evaluate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Transition)
	uint16 bCanEvaluateFromEvent: 1;

	/**
	 * This transition will not prevent the next transition in the priority sequence from being evaluated.
	 * This allows the possibility for multiple active states.
	 */
	UPROPERTY()
	uint16 bRunParallel: 1;

	/**
	 * If the transition should still evaluate if already connecting to an active state.
	 */
	UPROPERTY()
	uint16 bEvalIfNextStateActive: 1;

	/** Secondary check state machine will make if a state is evaluating transitions on the same tick as Start State. */
	UPROPERTY()
	uint16 bCanEvalWithStartState: 1;
	
	/** The transition can never be taken conditionally or from an event. */
	UPROPERTY()
	uint16 bAlwaysFalse: 1;

	/** The transition has been created by an Any State. */
	UPROPERTY()
	uint16 bFromAnyState: 1;

	/** The transition has been created by a Link State. */
	UPROPERTY()
	uint16 bFromLinkState: 1;

	/** Guid to the state this transition is from. Kismet compiler will convert this into a state link. */
	UPROPERTY()
	FGuid FromGuid;
	
	/** Guid to the state this transition is leading to. Kismet compiler will convert this into a state link. */
	UPROPERTY()
	FGuid ToGuid;
	
	/** The conditional evaluation type which determines the type of evaluation required if any. */
	UPROPERTY()
	ESMConditionalEvaluationType ConditionalEvaluationType;

	/** Last recorded timestamp from a network transaction. */
	FDateTime LastNetworkTimestamp;
	
	/** Original state transitioning from. */
	FSMState_Base* SourceState;

	/** Destination state transitioning to. */
	FSMState_Base* DestinationState;
public:
	virtual void UpdateReadStates() override;

public:
	FSMTransition();

	// FSMNode_Base
	virtual void Initialize(UObject* Instance) override;
protected:
	virtual void InitializeFunctionHandlers() override;
public:
	virtual void InitializeGraphFunctions() override;
	virtual void Reset() override;
	virtual bool IsNodeInstanceClassCompatible(UClass* NewNodeInstanceClass) const override;
	virtual UClass* GetDefaultNodeInstanceClass() const override;
	virtual void ExecuteInitializeNodes() override;
	virtual void ExecuteShutdownNodes() override;
	// ~FSMNode_Base
	
	/** Will execute any transition tunnel logic. */
	void TakeTransition();

	/** Execute the graph and return the result. Only determines if this transition passes. */
	bool DoesTransitionPass();

	/** Checks if this transition has been notified it can pass from an event. */
	bool CanTransitionFromEvent();
	
	/**
	 * Checks the execution tree in the event of conduits.
	 * @param Transitions All transitions that pass.
	 * @return True if a valid path exists.
	 */
	bool CanTransition(TArray<FSMTransition*>& Transitions);

	/**
	 * Retrieve all transitions in a chain. If the length is more than one that implies a transition conduit is in use.
	 * @param Transitions All transitions connected to this transition, ordered by traversal.
	 */
	void GetConnectedTransitions(TArray<FSMTransition*>& Transitions) const;

	/** If the transition is allowed to evaluate conditionally. This has to be true in order for the transition to be taken. */
	bool CanEvaluateConditionally() const;

	/** If the transition is allowed to evaluate from an event. */
	bool CanEvaluateFromEvent() const;

	FORCEINLINE FSMState_Base* GetFromState() const { return FromState; }
	FORCEINLINE FSMState_Base* GetToState() const { return ToState; }

	/** Sets the state leading to this transition. This will update the state with this transition. */
	void SetFromState(FSMState_Base* State);
	void SetToState(FSMState_Base* State);

#if WITH_EDITORONLY_DATA
	virtual bool IsDebugActive() const override { return bIsEvaluating ? bIsEvaluating : Super::IsDebugActive(); }
	virtual bool WasDebugActive() const override { return bWasEvaluating ? bWasEvaluating : Super::WasDebugActive(); }
	/** Helper to display evaluation color in the editor. */
	mutable bool bWasEvaluating = false;
#endif

#if WITH_EDITOR
	virtual void ResetGeneratedValues() override;
#endif
	
	/** Checks to make sure every transition is allowed to evaluate with the start state. */
	static bool CanEvaluateWithStartState(const TArray<FSMTransition*>& TransitionChain);

	/** Get the final state a transition chain will reach. Attempts to find a non-conduit first. */
	static FSMState_Base* GetFinalStateFromChain(const TArray<FSMTransition*>& TransitionChain);

	/** Checks if any transition allows evaluation if the next state is active. */
	static bool CanChainEvalIfNextStateActive(const TArray<FSMTransition*>& TransitionChain);
private:
	FSMState_Base* FromState;
	FSMState_Base* ToState;
};