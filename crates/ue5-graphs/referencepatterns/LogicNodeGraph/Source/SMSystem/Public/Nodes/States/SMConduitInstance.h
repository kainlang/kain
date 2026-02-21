// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMStateInstance.h"

#include "SMConduitInstance.generated.h"

/**
 * Conduits connect transitions. The connected transition chain including the conduit must pass to switch states.
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = LogicDriver, hideCategories = (SMConduitInstance), meta = (DisplayName = "Conduit Class"))
class SMSYSTEM_API USMConduitInstance : public USMStateInstance_Base
{
	GENERATED_BODY()

public:
	USMConduitInstance();

	/** Is this conduit allowed to switch states. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Default")
	bool CanEnterTransition() const;

	/** Called once this conduit has evaluated to true and has been taken. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Node Instance")
	void OnConduitEntered();

	/** Called after the state leading to this node is initialized but before OnStateBegin. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Node Instance")
	void OnConduitInitialized();
	
	/** Called after the state leading to this node has run OnStateEnd but before it has called its shutdown sequence. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Node Instance")
	void OnConduitShutdown();

	/**
	* Should graph properties evaluate during the conduit's initialize sequence.
	*/
	UPROPERTY(EditDefaultsOnly, Category = "Properties", AdvancedDisplay, meta = (InstancedTemplate, HideOnNode, EditCondition = "bAutoEvalExposedProperties", DisplayName = "Auto Eval on Initialize"))
	bool bEvalGraphsOnInitialize;

	/**
	* Should graph properties evaluate when the conduit is being evaluated as a transition.
	*/
	UPROPERTY(EditDefaultsOnly, Category = "Properties", AdvancedDisplay, meta = (InstancedTemplate, HideOnNode, EditCondition = "bAutoEvalExposedProperties", DisplayName = "Auto Eval on Transition Eval"))
	bool bEvalGraphsOnTransitionEval;
	
protected:
	/* Override in native classes to implement. Never call these directly. */

	virtual bool CanEnterTransition_Implementation() const { return false; }
	virtual void OnConduitEntered_Implementation() {}
	virtual void OnConduitInitialized_Implementation() {}
	virtual void OnConduitShutdown_Implementation() {}

public:
	/** Sets whether this node is allowed to evaluate or not. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	void SetCanEvaluate(const bool bValue);
	/** Check whether this node is allowed to evaluate. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	bool GetCanEvaluate() const;
	
	/** Public getter for #bEvalWithTransitions. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance|Defaults")
	bool GetEvalWithTransitions() const;
	/** Public setter for #bEvalWithTransitions. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance|Defaults")
	void SetEvalWithTransitions(const bool bValue);

private:
	friend class USMGraphNode_ConduitNode;
	
	/**
	 * This conduit will be evaluated with inbound and outbound transitions.
	 * If any transition fails the entire transition fails. In that case the
	 * state leading to this conduit will not take this transition.
	 *
	 * This makes the behavior similar to AnimGraph conduits.
	 */
	UPROPERTY(EditDefaultsOnly, Category = Conduit)
	uint8 bEvalWithTransitions: 1;

	/**
	 * If this conduit is allowed to evaluate.
	 */
	UPROPERTY(EditDefaultsOnly, Category = Conduit, meta=(DisplayName = "Can Evaluate"))
	uint8 bCanEvaluate: 1;
};

