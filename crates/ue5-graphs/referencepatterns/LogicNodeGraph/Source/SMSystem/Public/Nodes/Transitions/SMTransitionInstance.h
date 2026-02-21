// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMNodeInstance.h"
#include "SMNodeRules.h"

#include "SMTransitionInstance.generated.h"

class USMStateInstance_Base;
struct FSMTransitionInfo;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTransitionEnteredSignature, class USMTransitionInstance*, TransitionInstance);

/**
 * Connect states and define conditions to signal when the active state should end and the next state begin.
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = LogicDriver, hideCategories = (SMTransitionInstance), meta = (DisplayName = "Transition Class"))
class SMSYSTEM_API USMTransitionInstance : public USMNodeInstance
{
	GENERATED_BODY()

public:
	USMTransitionInstance();
	
	/** Conditional check to determine if the transition can be taken. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Default")
	bool CanEnterTransition() const;

	/** Called when this transition has been evaluated and taken. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Node Instance")
	void OnTransitionEntered();

	/** Called after the state leading to this node is initialized but before OnStateBegin. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Node Instance")
	void OnTransitionInitialized();

	/** Called after the state leading to this node has run OnStateEnd but before it has called its shutdown sequence. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Node Instance")
	void OnTransitionShutdown();

	/** The state this transition leaves from. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	USMStateInstance_Base* GetPreviousStateInstance() const;

	/** The state this transition leads to. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	USMStateInstance_Base* GetNextStateInstance() const;

	/**
	* Return the state that last triggered this transition.
	* This may be a state prior to a transition conduit.
	* This will be valid during OnTransitionEntered().
	*
	* @return The first state to trigger this transition chain or null.
	*/
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	USMStateInstance_Base* GetSourceStateForActiveTransition() const;

	/**
	* Return the destination state we are transitioning to or last transitioned to.
	* This may be a state after a transition conduit.
	* This will be valid during OnTransitionEntered().
	*
	* @return The final state after the transition chain or null.
	*/
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	USMStateInstance_Base* GetDestinationStateForActiveTransition() const;
	
	/** Return read only information about the owning transition. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	void GetTransitionInfo(FSMTransitionInfo& Transition) const;

	/** Return the last server timestamp of this transition. If not networked this won't be set. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	const FDateTime& GetServerTimestamp() const;
	
	/**
	 * Evaluates the transition's local graph which usually calls #CanEnterTransition of this instance.
	 * This is equivalent to the state machine determining if a transition succeeds.
	 *
	 * This is best used from an external caller when checking a transition result.
	 *
	 * Do NOT call this from within CanEnterTransition of the instance or the local graph
	 * or you may trigger an infinite loop.
	 *
	 * @return The transition result.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	bool DoesTransitionPass() const;

	/**
	 * If the transition was created by an Any State.
	 * @return True if the transition was copied from an Any State.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	bool IsTransitionFromAnyState() const;

	/**
	 * If the transition was created by a Link State.
	 * @return True if the transition was copied from a Link State.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	bool IsTransitionFromLinkState() const;

	/**
	 * Efficiently evaluate and take the transition immediately.
	 * If the transition's CanEnterTransition method returns true the entire transition
	 * chain this transition is part of will be evaluated and taken. Super state and parallel transitions
	 * will not evaluate when this method is called.
	 *
	 * Steps this method performs:
	 * 1. Enables SetCanEvaluate for this transition.
	 * 2. Calls EvaluateAndTakeTransitionChain from the owning state machine instance.
	 * 3. Sets SetCanEvaluate back to the original value.
	 * 
	 * Use at the end of execution from manually bound events.
	 *
	 * @return True if the transition was taken.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	bool EvaluateFromManuallyBoundEvent();
	
	/**
	 * Retrieve all transition instances in the transition stack.
	 *
	 * @param TransitionStackInstances [Out] Transition stack instances in their correct order.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	void GetAllTransitionStackInstances(TArray<USMTransitionInstance*>& TransitionStackInstances) const;
	
	/**
	 * Retrieve a transition instance from within the transition stack.
	 * 
	 * @param Index the index of the array.
	 * @return the transition if the index is valid.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	USMTransitionInstance* GetTransitionInStack(int32 Index) const;

	/**
	 * Retrieve the first stack instance of a given class.
	 *
	 * @param TransitionClass The transition class to search for.
	 * @param bIncludeChildren If children of the given class can be included.
	 * @return the first transition that matches the class.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	USMTransitionInstance* GetTransitionInStackByClass(TSubclassOf<USMTransitionInstance> TransitionClass, bool bIncludeChildren = false) const;

	/**
	 * Retrieve the owning node instance of a transition stack. If this is called from the main node instance
	 * it will return itself.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	USMTransitionInstance* GetStackOwnerInstance() const;
	
	/**
	 * Retrieve all transitions that match the given class.
	 *
	 * @param TransitionClass The transition class to search for.
	 * @param bIncludeChildren If children of the given class can be included.
	 * @param TransitionStackInstances [Out] Transition stack instances matching the given class.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	void GetAllTransitionsInStackOfClass(TSubclassOf<USMTransitionInstance> TransitionClass, TArray<USMTransitionInstance*>& TransitionStackInstances, bool bIncludeChildren = false) const;

	/**
	* Retrieve the index of a transition stack instance.
	* O(n).
	*
	* @param TransitionInstance The transition instance to lookup in the stack.
	* @return The index of the transition in the stack. -1 if not found or is the base transition instance.
	*/
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	int32 GetTransitionIndexInStack(USMTransitionInstance* TransitionInstance) const;
	
	/** The total number of transitions in the transition stack. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	int32 GetTransitionStackCount() const;
	
protected:
	/* Override in native classes to implement. Never call these directly. */
	
	virtual bool CanEnterTransition_Implementation() const { return false; }
	virtual void OnTransitionEntered_Implementation() {}
	virtual void OnTransitionInitialized_Implementation() {}
	virtual void OnTransitionShutdown_Implementation() {}
	
#if WITH_EDITORONLY_DATA
public:
	const FSMTransitionConnectionValidator& GetAllowedConnections() const { return ConnectionRules; }
	bool ShouldHideIconBackground() const { return HasCustomIcon() && !bShowBackgroundOnCustomIcon; }
	bool IsIconHidden() const { return bHideIcon; }
	float GetIconLocationPercentage() const { return IconLocationPercentage; }

	static FName GetAutoPlaceTransitionIfRulesPassPropertyName() { return GET_MEMBER_NAME_CHECKED(USMTransitionInstance, bAutoPlaceTransitionIfRulesPass); }
protected:
	/** Define what types of connections are allowed. Default is all. */
	UPROPERTY(EditDefaultsOnly, AssetRegistrySearchable, Category = "Behavior", meta = (InstancedTemplate, ShowOnlyInnerProperties))
	FSMTransitionConnectionValidator ConnectionRules;

	/** Automatically place the transition when a connection rule passes. */
	UPROPERTY(EditDefaultsOnly, AssetRegistrySearchable, Category = "Behavior", AdvancedDisplay, meta = (InstancedTemplate))
	uint8 bAutoPlaceTransitionIfRulesPass: 1;

	/** Allow the icon background brush to be displayed for custom icons. */
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "Display", meta = (DisplayPriority = 0, NodeBaseOnly, EditCondition="bDisplayCustomIcon"))
	uint8 bShowBackgroundOnCustomIcon: 1;
	
	/** Completely hide the transition icon. If the connection is hovered or selected it will become visible. */
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "Display", meta = (InstancedTemplate))
	uint8 bHideIcon: 1;

	/** Set the position of the icon along the connection. */
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "Display", meta = (ClampMin = "0.0", ClampMax = "1.0", NodeBaseOnly))
	float IconLocationPercentage;
	
#endif

public:
	/** Sets whether this node is allowed to evaluate or not. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	void SetCanEvaluate(const bool bValue);
	/** Check whether this transition is allowed to evaluate. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	bool GetCanEvaluate() const;
	
	/** Public getter for #PriorityOrder. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance|Defaults")
	int32 GetPriorityOrder() const;
	/**
	 * Public setter for #PriorityOrder.
	 * Only valid from the editor construction script.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance|Defaults")
	void SetPriorityOrder(const int32 Value);

	/** Public getter for #bRunParallel. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance|Defaults")
	bool GetRunParallel() const;
	/** Public setter for #bRunParallel. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance|Defaults")
	void SetRunParallel(const bool bValue);

	/** Public getter for #bEvalIfNextStateActive. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance|Defaults")
	bool GetEvalIfNextStateActive() const;
	/** Public setter for #bEvalIfNextStateActive. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance|Defaults")
	void SetEvalIfNextStateActive(const bool bValue);

	/** Public getter for #bCanEvaluateFromEvent. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance|Defaults")
	bool GetCanEvaluateFromEvent() const;
	/** Public setter for #bCanEvaluateFromEvent. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance|Defaults")
	void SetCanEvaluateFromEvent(const bool bValue);

	/** Public getter for #bCanEvalWithStartState. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance|Defaults")
	bool GetCanEvalWithStartState() const;
	/** Public setter for #bCanEvalWithStartState. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance|Defaults")
	void SetCanEvalWithStartState(const bool bValue);
private:
	friend class USMGraphNode_TransitionEdge;
	
	/**
	 * Lower number transitions will be evaluated first.
	 */
	UPROPERTY(EditDefaultsOnly, Category = Transition, meta = (NodeBaseOnly))
	int32 PriorityOrder;

	/**
	 * If this transition evaluates to true it will not prevent the next transition in the priority sequence from being evaluated.
	 * This allows the possibility for multiple active states. Transitions from Conduit nodes are not supported.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Parallel States", meta = (NodeBaseOnly))
	uint8 bRunParallel: 1;

	/**
	 * Should the transition evaluate if already connecting to an active state.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Parallel States", meta = (NodeBaseOnly))
	uint8 bEvalIfNextStateActive: 1;

	/**
	 * If this transition is allowed to evaluate conditionally.
	 */
	UPROPERTY(EditDefaultsOnly, Category = Transition, meta=(DisplayName = "Can Evaluate Conditionally", NodeBaseOnly))
	uint8 bCanEvaluate: 1;
	
	/** If this transition can evaluate from auto-bound events. */
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = Transition, meta = (NodeBaseOnly))
	uint8 bCanEvaluateFromEvent: 1;
	
	/**
	 * Setting to false forces this transition to never evaluate on the same tick as OnStateBegin.
	 * Only used if this transition's from state has bEvalTransitionsOnStart set to true.
	 */
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = Transition, meta = (NoResetToDefault, NodeBaseOnly))
	uint8 bCanEvalWithStartState: 1;
	
public:
	/** Called when this transition has been entered from the previous state. */
	UPROPERTY(BlueprintAssignable, Category = "Logic Driver|Node Instance")
	FOnTransitionEnteredSignature OnTransitionEnteredEvent;
};