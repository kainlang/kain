// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMStateInstance.h"

#include "SMStateMachineInstance.generated.h"

/**
 * The base class for state machine nodes. These are different from regular state machines (SMInstance) in that they can be assigned to a state machine directly
 * either in the class defaults or in the details panel of a nested state machine node. Think of this as giving a state machine a 'type' which allows you to
 * identify it in rule behavior. This is still considered a state as well which allows access to hooking into Start, Update, and End events even when placed as
 * a nested state machine.
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = LogicDriver, hideCategories = (SMStateMachineInstance), meta = (DisplayName = "State Machine Class", ShortTooltip="State machine classes can be assigned to state machine blueprints or nested FSMs."))
class SMSYSTEM_API USMStateMachineInstance : public USMStateInstance_Base
{
	GENERATED_BODY()

public:
	USMStateMachineInstance();

	// UObject
	virtual void PostLoad() override;
	// ~UObject

	/** Called after the state machine has completed its internal states. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Node Instance")
	void OnStateMachineCompleted();

	/** Called when an end state has been reached. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Node Instance")
	void OnEndStateReached();

	/** Called before OnStateBegin and before transitions are initialized. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Node Instance")
	void OnStateInitialized();

	/** Called after OnStateEnd and after transitions are shutdown. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Node Instance")
	void OnStateShutdown();
	
	/**
	 * Retrieve all contained state instances defined within the state machine graph this instance represents.
	 * These can be states, state machines, and conduits.
	 *
	 * This does not include nested states in sub state machines.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	void GetAllStateInstances(TArray<USMStateInstance_Base*>& StateInstances) const;

	/**
	 * Retrieve an immediate state owned by this state machine node in O(1) time.
	 * This will not retrieve nested states.
	 *
	 * @param StateName The name of the state node to search for.
	 *
	 * @return The state instance or nullptr.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	USMStateInstance_Base* GetContainedStateByName(const FString& StateName) const;
	
	/** Return the entry states of the state machine. Generally one unless parallel states are used. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	void GetEntryStates(TArray<USMStateInstance_Base*>& EntryStates) const;

	/** Return all states active within this state machine node. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	void GetActiveStates(TArray<USMStateInstance_Base*>& ActiveStates) const;

	/** Return an SMInstance reference if one is assigned. This will be null if this is not a state machine reference. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	USMInstance* GetStateMachineReference() const;
	
	// USMNodeInstance
	/** Special handling to retrieve the real FSM node in the event this is a state machine reference. */
	virtual const FSMNode_Base* GetOwningNodeContainer() const override;
	// ~USMNodeInstance

	/** If this state machine node is in an end state. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	bool IsStateMachineInEndState() const;
	
protected:
	/* Override in native classes to implement. Never call these directly. */
	
	virtual void OnStateMachineCompleted_Implementation() {}
	virtual void OnEndStateReached_Implementation() {}
	virtual void OnStateInitialized_Implementation() {}
	virtual void OnStateShutdown_Implementation() {}

#if WITH_EDITORONLY_DATA
public:
	/** Retrieve the node placement rules for this state machine. */
	const FSMStateMachineNodePlacementValidator& GetAllowedNodes() const { return NodePlacementRules; }

	/** @deprecated Use `GetAllowedNodes()` instead. */
	UE_DEPRECATED(5.1, "This function is deprecated. Use `GetAllowedNodes()` instead.")
	const FSMStateMachineNodePlacementValidator& GetAllowedStates() const { return GetAllowedNodes(); }
	
protected:
	/** Define what types of nodes are allowed or disallowed. Default is all. */
	UPROPERTY(EditDefaultsOnly, Category = "Behavior", meta = (InstancedTemplate, ShowOnlyInnerProperties))
	FSMStateMachineNodePlacementValidator NodePlacementRules;

	/** @deprecated Use #NodePlacementRules instead. */
	UPROPERTY()
	FSMStateMachineNodePlacementValidator StatePlacementRules_DEPRECATED;
#endif

public:
	/** Public getter for #bWaitForEndState. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance|Defaults")
	bool GetWaitForEndState() const;
	/** Public setter for #bWaitForEndState. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance|Defaults")
	void SetWaitForEndState(const bool bValue);

	/** Public getter for #bReuseCurrentState. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance|Defaults")
	bool GetReuseCurrentState() const;
	/** Public setter for #bReuseCurrentState. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance|Defaults")
	void SetReuseCurrentState(const bool bValue);

	/** Public getter for #bReuseIfNotEndState. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance|Defaults")
	bool GetReuseIfNotEndState() const;
	/** Public setter for #bReuseIfNotEndState. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance|Defaults")
	void SetReuseIfNotEndState(const bool bValue);
	
private:
	/**
	 * Wait for an end state to be hit before evaluating transitions or being considered an end state itself.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "State Machine")
	uint8 bWaitForEndState: 1;
	
	/**
	 * When true the current state is reused on end/start.
	 * When false the current state is cleared on end and the initial state used on start.
	 * References will inherit this behavior.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "State Machine")
	uint8 bReuseCurrentState: 1;

	/**
	 * Do not reuse if in an end state.
	 * References will inherit this behavior.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "State Machine", meta = (EditCondition = "bReuseCurrentState"))
	uint8 bReuseIfNotEndState: 1;
};

