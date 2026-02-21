// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMState.h"

#include "SMConduit.generated.h"

#define GRAPH_PROPERTY_EVAL_CONDUIT_INIT        100
#define GRAPH_PROPERTY_EVAL_CONDUIT_TRANS_CHECK 101

/**
 * A conduit can either be configured to run as a state or as a transition.
 * Internally it consists of a single transition that must be true before outgoing transitions evaluate.
 */
USTRUCT(BlueprintInternalUseOnly)
struct SMSYSTEM_API FSMConduit : public FSMState_Base
{
	GENERATED_USTRUCT_BODY()

public:

	/** Set from graph execution. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Result, meta = (AlwaysAsPin))
	uint8 bCanEnterTransition: 1;

	/** Set from graph execution or configurable from details panel. Must be true for the conduit to be evaluated. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Transition)
	uint8 bCanEvaluate: 1;
	
	/**
	 * This conduit will be evaluated with inbound and outbound transitions.
	 * If any transition fails the entire transition fails. In that case the
	 * state leading to this conduit will not take this transition.
	 */
	UPROPERTY()
	uint8 bEvalWithTransitions: 1;

	/** The conditional evaluation type which determines the type of evaluation required if any. */
	UPROPERTY()
	ESMConditionalEvaluationType ConditionalEvaluationType;
	
public:
	FSMConduit();

	// FSMNode_Base
	virtual void Initialize(UObject* Instance) override;
protected:
	virtual void InitializeFunctionHandlers() override;
public:
	virtual void InitializeGraphFunctions() override;
	virtual void Reset() override;
	virtual void ExecuteInitializeNodes() override;
	virtual void ExecuteShutdownNodes() override;
	virtual bool CanExecuteGraphProperties(uint32 OnEvent, const USMStateInstance_Base* ForTemplate) const override;
	virtual bool IsNodeInstanceClassCompatible(UClass* NewNodeInstanceClass) const override;
	virtual UClass* GetDefaultNodeInstanceClass() const override;
	// ~FSMNode_Base

	// FSMState_Base
	virtual bool StartState() override;
	virtual bool UpdateState(float DeltaSeconds) override;
	virtual bool EndState(float DeltaSeconds, const FSMTransition* TransitionToTake) override;

	virtual bool IsConduit() const override { return true; }
	
	/** Evaluate the conduit and retrieve the correct condition. */
	virtual bool GetValidTransition(TArray<TArray<FSMTransition*>>& Transitions) override;
	// ~FSMState_Base
	
	/** Should this be considered an extension to a transition? */
	bool IsConfiguredAsTransition() const { return bEvalWithTransitions; }

	/** Signal that this conduit is being entered along with transitions. */
	void EnterConduitWithTransition();

public:
	bool bIsEvaluating;
	
#if WITH_EDITORONLY_DATA
	virtual bool IsDebugActive() const override { return bIsEvaluating ? bIsEvaluating : Super::IsDebugActive(); }
	virtual bool WasDebugActive() const override { return bWasEvaluating ? bWasEvaluating : Super::WasDebugActive(); }
	
	/** Helper to display evaluation color in the editor. */
	mutable bool bWasEvaluating = false;
#endif
	
private:
	// True for GetValidTransitions, prevents stack overflow when looped with other transition based conduits.
	bool bCheckedForTransitions;
};
