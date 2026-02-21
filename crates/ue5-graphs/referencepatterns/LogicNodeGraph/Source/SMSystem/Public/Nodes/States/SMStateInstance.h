// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMNodeInstance.h"
#include "SMNodeRules.h"

#include "SMStateInstance.generated.h"

class USMTransitionInstance;
struct FSMStateInfo;
struct FSMTransition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStateBeginSignature, class USMStateInstance_Base*, StateInstance);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStateUpdateSignature, class USMStateInstance_Base*, StateInstance, float, DeltaSeconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStateEndSignature, class USMStateInstance_Base*, StateInstance);

/**
 * [Logic Driver] The abstract base class for all state type nodes including state machine nodes and conduits.
 *
 * To expose native member properties on the node they must be marked BlueprintReadWrite and not contain the meta keyword HideOnNode.
 */
UCLASS(Abstract, Blueprintable, BlueprintType, ClassGroup = LogicDriver, hideCategories = (SMStateInstance_Base), meta = (DisplayName = "State Class Base"))
class SMSYSTEM_API USMStateInstance_Base : public USMNodeInstance
{
	GENERATED_BODY()

public:
	USMStateInstance_Base();

	// USMNodeInstance
	/** @deprecated Use either IsEndState() or USMStateMachineInstance::IsStateMachineInEndState() depending on use case. */
	UE_DEPRECATED(5.1, "This function is deprecated. Use either `IsEndState()`, or `USMStateMachineInstance::IsStateMachineInEndState()` depending on use case.")
	virtual bool IsInEndState() const override;
	// ~USMNodeInstance
	
	/** Called when the state is started. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Node Instance")
	void OnStateBegin();

	/**
	 * Called when the state is updated.
	 *
	 * @param DeltaSeconds Time delta in seconds from the last update.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Node Instance")
	void OnStateUpdate(float DeltaSeconds);

	/**
	 * Called when the state is ending. It is not advised to switch states during this event.
	 * The state machine will already be in the process of switching states.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Node Instance")
	void OnStateEnd();

	/** If this state is considered an end state. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	bool IsEndState() const;
	
	/** Return read only information about the owning state. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	void GetStateInfo(FSMStateInfo& State) const;
	
	/** Checks if this state is a state machine. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	bool IsStateMachine() const;

	/** If this state is an entry state within a state machine. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	bool IsEntryState() const;
	
	/**
	 * Force set the active flag of this state. This call is replicated and can be called from the server or from a client.
	 * The caller must have state change authority.
	 *
	 * When calling from a state, it should be done either OnStateBegin or OnStateUpdate. When calling from a transition
	 * it should be done from OnTransitionEntered and limited to the previous state. If this is called from other
	 * state or transition methods it may cause issues with the normal update cycle of a state machine.
	 * 
	 * @param bValue True activates the state, false deactivates the state.
	 * @param bSetAllParents Sets the active state of all super state machines. A parent state machine won't be deactivated unless there are no other states active.
	 * @param bActivateNow If the state is becoming active it will fully activate and run OnStateBegin this tick. If this is false it will do so on the next update cycle.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance", meta = (AdvancedDisplay = "bActivateNow"))
	void SetActive(bool bValue, bool bSetAllParents = false, bool bActivateNow = true);
	
	/**
	 * Signals to the owning state machine to process transition evaluation.
	 * This is similar to calling Update on the owner root state machine, however state update logic (Tick) won't execute.
	 * All transitions are evaluated as normal starting from the root state machine down.
	 * Depending on super state transitions it's possible the state machine this state is part of may exit.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	void EvaluateTransitions();
	
	/**
	 * Return all outgoing transition instances.
	 * @param Transitions The outgoing transitions.
	 * @param bExcludeAlwaysFalse Skip over transitions that can't ever be true.
	 * @return True if any transitions exist.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	bool GetOutgoingTransitions(TArray<USMTransitionInstance*>& Transitions, bool bExcludeAlwaysFalse = true) const;

	/**
	 * Return all incoming transition instances.
	 * @param Transitions The incoming transitions.
	 * @param bExcludeAlwaysFalse Skip over transitions that can't ever be true.
	 * @return True if any transitions exist.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	bool GetIncomingTransitions(TArray<USMTransitionInstance*>& Transitions, bool bExcludeAlwaysFalse = true) const;
	
	/**
	 * The transition this state will be taking. Generally only valid during OnStateEnd and if this state isn't an
	 * end state. May be null.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	USMTransitionInstance* GetTransitionToTake() const;

	/**
	 * Forcibly move to the next state providing this state is active and a transition is directly connecting the states.
	 *
	 * This should be done either OnStateBegin or OnStateUpdate. If this is called from other state methods it may cause
	 * issues with the normal update cycle of a state machine.
	 * 
	 * @param NextStateInstance The state node instance to switch to.
	 * @param bRequireTransitionToPass Will evaluate the transition and only switch states if it passes.
	 * @param bActivateNow If the state switches the destination state will activate and run OnStateBegin this tick. Otherwise, it will do so in the next update cycle.
	 *
	 * @return True if the active state was switched.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance", meta = (AdvancedDisplay = "bActivateNow"))
	bool SwitchToLinkedState(USMStateInstance_Base* NextStateInstance, bool bRequireTransitionToPass = true, bool bActivateNow = true);

	/**
	 * Forcibly move to the next state providing this state is active and a transition is directly connecting the states.
	 *
	 * This should be done either OnStateBegin or OnStateUpdate. If this is called from other state methods it may cause
	 * issues with the normal update cycle of a state machine.
	 * 
	 * @param NextStateName The name of the state to switch to.
	 * @param bRequireTransitionToPass Will evaluate the transition and only switch states if it passes.
	 * @param bActivateNow If the state switches the destination state will activate and run OnStateBegin this tick. Otherwise, it will do so in the next update cycle.
	 *
	 * @return True if the active state was switched.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance", meta = (AdvancedDisplay = "bActivateNow"))
	bool SwitchToLinkedStateByName(const FString& NextStateName, bool bRequireTransitionToPass = true, bool bActivateNow = true);

	/**
	 * Forcibly move to the next state providing this state is active and a transition is directly connecting the states.
	 *
	 * This should be done either OnStateBegin or OnStateUpdate. If this is called from other state methods it may cause
	 * issues with the normal update cycle of a state machine.
	 * 
	 * @param TransitionInstance The transition which should be taken to the next state.
	 * @param bRequireTransitionToPass Will evaluate the transition and only switch states if it passes.
	 * @param bActivateNow If the state switches the destination state will activate and run OnStateBegin this tick. Otherwise, it will do so in the next update cycle.
	 *
	 * @return True if the active state was switched.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance", meta = (AdvancedDisplay = "bActivateNow"))
	bool SwitchToLinkedStateByTransition(USMTransitionInstance* TransitionInstance, bool bRequireTransitionToPass = true, bool bActivateNow = true);

	/**
	 * Forcibly move to the next state providing this state is active and a transition is directly connecting the states.
	 *
	 * This should be done either OnStateBegin or OnStateUpdate. If this is called from other state methods it may cause
	 * issues with the normal update cycle of a state machine.
	 * 
	 * @param TransitionName The outgoing transition name which should be taken to the next state.
	 * @param bRequireTransitionToPass Will evaluate the transition and only switch states if it passes.
	 * @param bActivateNow If the state switches the destination state will activate and run OnStateBegin this tick. Otherwise, it will do so in the next update cycle.
	 *
	 * @return True if the active state was switched.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance", meta = (AdvancedDisplay = "bActivateNow"))
	bool SwitchToLinkedStateByTransitionName(const FString& TransitionName, bool bRequireTransitionToPass = true, bool bActivateNow = true);
	
private:
	bool SwitchToLinkedStateByTransition_Internal(FSMTransition* Transition, bool bRequireTransitionToPass = true, bool bActivateNow = true);
	
public:
	/**
	 * Return an outgoing transition given the transition index.
	 * @param Index The array index of the transition. If transitions have manual priorities they should correlate with the index value.
	 *
	 * @return The transition or nullptr.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	USMTransitionInstance* GetTransitionByIndex(int32 Index) const;

	/**
	 * Return a transition given its name.
	 * O(n) by number of outgoing transitions.
	 * @param Name The name of the transition. The name is auto-generated by default.
	 *
	 * @return The transition or nullptr.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	USMTransitionInstance* GetOutgoingTransitionByName(const FString& Name) const;

	/**
	 * Return an incoming transition given its name.
	 * O(n) by number of incoming transitions.
	 * @param Name The name of the transition. The name is auto-generated by default.
	 *
	 * @return The transition or nullptr.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	USMTransitionInstance* GetIncomingTransitionByName(const FString& Name) const;
	
	/**
	 * Return the next connected state given a transition index.
	 * @param Index The array index of the transition. If transitions have manual priorities they should correlate with the index value.
	 *
	 * @return The connected state or nullptr.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	USMStateInstance_Base* GetNextStateByTransitionIndex(int32 Index) const;

	/**
	 * Return the next state connected by an outgoing transition.
	 * O(n) by number of transitions.
	 * @param StateName Name of the state to search for.
	 *
	 * @return The connected state or nullptr.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	USMStateInstance_Base* GetNextStateByName(const FString& StateName) const;

	/**
	 * Return a previous state connected through an incoming transition.
	 * O(n) by number of transitions.
	 * @param StateName Name of the state to search for.
	 *
	 * @return The connected state or nullptr.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	USMStateInstance_Base* GetPreviousStateByName(const FString& StateName) const;
	
	/**
	 * Retrieve the last active state that transitioned to this state.
	 * This will not count transition conduits.
	 *
	 * @return The state instance last active before this state became active. May be nullptr.
	 * 
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	USMStateInstance_Base* GetPreviousActiveState() const;

	/**
	 * Retrieve the last transition taken to this state.
	 *
	 * @return The transition instance last active before this state became active. May be nullptr.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	USMTransitionInstance* GetPreviousActiveTransition() const;

	/** Checks if every outgoing transition was created by an Any State. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	bool AreAllOutgoingTransitionsFromAnAnyState() const;

	/** Checks if every incoming transition was created by an Any State. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	bool AreAllIncomingTransitionsFromAnAnyState() const;
	
	/**
	 * Retrieve the UTC time when the state last started. If this is called before the state has started once,
	 * or before initialization (such as during an editor construction script) the datetime will be initialized to 0.
	 *
	 * @return DateTime in UTC. This is when the state started locally and won't be in sync with the server.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	const FDateTime& GetStartTime() const;

	/**
	 * Retrieve the time the server spent in the state. This is retrieved from network transactions
	 * and can match the TimeInState from the server.
	 *
	 * This will only match the server after OnStateEnd() is called and provided the client received the replicated
	 * transition. When using client state change authority, `bWaitForTransactionsFromServer` must be set to true
	 * for the client to receive the updated server time.
	 *
	 * When the server time can't be calculated the local time will be returned instead, using GetTimeInState().
	 *
	 * The return value is impacted by bCalculateServerTimeForClients of the state machine component's network settings,
	 * and whether the state machine is networked, a client or server, or is ticking.
	 *
	 * @param bOutUsedLocalTime [Out] Indicates that local time from GetTimeInState() was used.
	 * @return The total time spent in the state, either matching the server or the current local time spent.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	float GetServerTimeInState(UPARAM(DisplayName="UsedLocalTime") bool& bOutUsedLocalTime) const;

	/**
	 * Recursively search connected nodes for nodes matching the given type.
	 * @param OutNodes All found nodes.
	 * @param NodeClass The class type to search for.
	 * @param bIncludeChildren Include children of type NodeClass or require an exact match.
	 * @param StopIfTypeIsNot The search is broken when a node's type is not found in this list. Leave empty to never break the search.
	 */
	void GetAllNodesOfType(TArray<USMNodeInstance*>& OutNodes, TSubclassOf<USMNodeInstance> NodeClass, bool bIncludeChildren = true, const TArray<UClass*>& StopIfTypeIsNot = TArray<UClass*>()) const;

public:
	/**
	 * Should graph properties evaluate when initializing or on state start.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Properties", AdvancedDisplay, meta = (InstancedTemplate, HideOnNode, EditCondition = "bAutoEvalExposedProperties", DisplayName = "Auto Eval on Start"))
	uint8 bEvalGraphsOnStart: 1;

	/**
	 * Should graph properties evaluate on state update.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Properties", AdvancedDisplay, meta = (InstancedTemplate, HideOnNode, EditCondition = "bAutoEvalExposedProperties", DisplayName = "Auto Eval on Update"))
	uint8 bEvalGraphsOnUpdate: 1;

	/**
	 * Should graph properties evaluate on state end.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Properties", AdvancedDisplay, meta = (InstancedTemplate, HideOnNode, EditCondition = "bAutoEvalExposedProperties", DisplayName = "Auto Eval on End"))
	uint8 bEvalGraphsOnEnd: 1;

	/**
	 * Should graph properties evaluate when the root state machine starts.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Properties", AdvancedDisplay, meta = (InstancedTemplate, HideOnNode, EditCondition = "bAutoEvalExposedProperties", DisplayName = "Auto Eval on Root State Machine Start"))
	uint8 bEvalGraphsOnRootStateMachineStart: 1;

	/**
	 * Should graph properties evaluate when the root state machine stops.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Properties", AdvancedDisplay, meta = (InstancedTemplate, HideOnNode, EditCondition = "bAutoEvalExposedProperties", DisplayName = "Auto Eval on Root State Machine Stop"))
	uint8 bEvalGraphsOnRootStateMachineStop: 1;
	
protected:
	/* Override in native classes to implement. Never call these directly. */
	
	virtual void OnStateBegin_Implementation() {}
	virtual void OnStateUpdate_Implementation(float DeltaSeconds) {}
	virtual void OnStateEnd_Implementation() {}
	
#if WITH_EDITORONLY_DATA
public:
	const FLinearColor& GetEndStateColor() const { return NodeEndStateColor; }
	bool ShouldDisplayNameWidget() const { return bDisplayNameWidget; }
	bool ShouldUseDisplayNameOnly() const { return ShouldDisplayNameWidget() && bShowDisplayNameOnly; }
	const FSMStateConnectionValidator& GetAllowedConnections() const { return ConnectionRules; }
	
	/** @deprecated This function is no longer used. Set #bRegisterWithContextMenu from the C++ constructor or use the `NotPlaceable` UClass specifier to prevent registration. */
	UE_DEPRECATED(5.1, "This function is no longer used. Set `bRegisterWithContextMenu` from the C++ constructor or use the `NotPlaceable` UClass specifier to prevent registration.")
	virtual bool IsRegisteredWithContextMenu() const;

	/** @deprecated This function is no longer used. Set #bHideFromContextMenuIfRulesFail from the C++ constructor instead. */
	UE_DEPRECATED(5.1, "This function is no longer used. Set `bHideFromContextMenuIfRulesFail` from the C++ constructor instead.")
	virtual bool HideFromContextMenuIfRulesFail() const { return bHideFromContextMenuIfRulesFail; }
	
	static FName GetHideFromContextMenuIfRulesFailPropertyName() { return GET_MEMBER_NAME_CHECKED(USMStateInstance_Base, bHideFromContextMenuIfRulesFail); }
	static FName GetRegisteredWithContextMenuPropertyName() { return GET_MEMBER_NAME_CHECKED(USMStateInstance_Base, bRegisterWithContextMenu); }
	
protected:
	/** The color this node should be when it is an end state. */
	UPROPERTY(EditDefaultsOnly, Category = "Color", meta = (EditCondition = "bUseCustomColors", DisplayPriority = 2))
	FLinearColor NodeEndStateColor;
	
	/** Define what types of connections are allowed. Default is all. */
	UPROPERTY(EditDefaultsOnly, AssetRegistrySearchable, Category = "Behavior", meta = (InstancedTemplate, ShowOnlyInnerProperties))
	FSMStateConnectionValidator ConnectionRules;

	/**
	 * Restrict placement unless rules can be verified. Ex: If the rule says this can only be connected from a state node, then this won't show
	 * in the context menu unless dragging from a state node.
	 */
	UPROPERTY(EditDefaultsOnly, AssetRegistrySearchable, Category = "Behavior", AdvancedDisplay, meta = (InstancedTemplate, EditCondition = "bRegisterWithContextMenu"))
	uint16 bHideFromContextMenuIfRulesFail: 1;

	/** Allows this node to be displayed in the graph context menu when placing nodes. */
	UPROPERTY(EditDefaultsOnly, AssetRegistrySearchable, Category = "General", AdvancedDisplay, meta = (InstancedTemplate))
	uint16 bRegisterWithContextMenu: 1;
	
	/**
	 * When true only the Name set above will be displayed. The node cannot be renamed.
	 * This allows duplicate names to be displayed in the same graph. The node's internal name and local graph name will still be unique. */
	UPROPERTY(EditDefaultsOnly, Category = "General", AdvancedDisplay, meta = (InstancedTemplate))
	uint16 bShowDisplayNameOnly: 1;

	/** Whether the name of this state should be visible on the node. It can still be changed in the details panel or by renaming the state graph. */
	UPROPERTY(EditDefaultsOnly, Category = "Display", meta = (InstancedTemplate))
	uint16 bDisplayNameWidget: 1;
	
#endif

public:
	/** Public getter for #bAlwaysUpdate. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance|Defaults")
	bool GetAlwaysUpdate() const;
	/** Public setter for #bAlwaysUpdate. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance|Defaults")
	void SetAlwaysUpdate(const bool bValue);

	/** Public getter for #bDisableTickTransitionEvaluation. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance|Defaults")
	bool GetDisableTickTransitionEvaluation() const;
	/** Public setter for #bDisableTickTransitionEvaluation. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance|Defaults")
	void SetDisableTickTransitionEvaluation(const bool bValue);

	/** Public getter for #bDefaultToParallel. Only valid for the editor. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance|Defaults")
	bool GetDefaultToParallel() const { return bDefaultToParallel; }
	/**
	 * Public setter for #bDefaultToParallel.
	 * Only valid from the editor construction script.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance|Defaults")
	void SetDefaultToParallel(const bool bValue);

	/** Public setter for #bAllowParallelReentry. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance|Defaults")
	bool GetAllowParallelReentry() const;
	/** Public setter for #bAllowParallelReentry. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance|Defaults")
	void SetAllowParallelReentry(const bool bValue);

	/** Public setter for #bStayActiveOnStateChange. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance|Defaults")
	bool GetStayActiveOnStateChange() const;
	/** Public setter for #bStayActiveOnStateChange. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance|Defaults")
	void SetStayActiveOnStateChange(const bool bValue);

	/** Public setter for #bEvalTransitionsOnStart. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance|Defaults")
	bool GetEvalTransitionsOnStart() const;
	/** Public setter for #bEvalTransitionsOnStart. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance|Defaults")
	void SetEvalTransitionsOnStart(const bool bValue);

	/** Public getter for #bExcludeFromAnyState. Only valid for the editor. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance|Defaults")
	bool GetExcludeFromAnyState() const { return bExcludeFromAnyState; }
	/**
	 * Public setter for #bExcludeFromAnyState.
	 * Only valid from the editor construction script.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance|Defaults")
	void SetExcludeFromAnyState(const bool bValue);

	/** Public setter for #bCanBeEndState. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance|Defaults")
	bool GetCanBeEndState() const;
	/** Public setter for #bCanBeEndState. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance|Defaults")
	void SetCanBeEndState(const bool bValue);
	
private:
	friend class USMGraphNode_StateNodeBase;

	/**
	 * Prevents conditional transitions for this state from being evaluated on Tick.
	 * This is good to use if the transitions leading out of the state are event based
	 * or if you are manually calling EvaluateTransitions from a state instance.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "State", meta = (NodeBaseOnly))
	uint16 bDisableTickTransitionEvaluation: 1;

	/**
	 * When true and a state is started OnStateBegin fires and then transitions are evaluated in the same tick.
	 * When false and a state is started OnStateBegin fires on the first tick and transitions are evaluated on the second tick.
	 * 
	 * This can be chained with other nodes that have this checked making it possible to evaluate multiple nodes
	 * and transitions in a single tick.
	 *
	 * When using this consider performance implications and any potential infinite loops such as if you are using
	 * a self transition on this state.
	 *
	 * Individual transitions can modify this behavior with bCanEvalWithStartState.
	 */
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "State", meta = (NodeBaseOnly))
	uint16 bEvalTransitionsOnStart: 1;
	
	/**
	 * Always update the state at least once before ending.
	 */
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "State", meta = (NodeBaseOnly))
	uint16 bAlwaysUpdate: 1;

	/**
	 * If this state is allowed to ever be considered an end state. Disabling this will prevent a state machine
	 * that is configured to stop on end state from stopping when reaching this state, even if this state is technically
	 * an end state.
	 */
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "State", meta = (NodeBaseOnly))
	uint16 bCanBeEndState: 1;

	/**
	 * Sets all current and future transitions from this state to run in parallel. Conduit nodes are not supported.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Parallel States", meta = (NodeBaseOnly))
	uint16 bDefaultToParallel: 1;

	/**
	 * If this state can be reentered from a parallel state if this state is already active. This will not call On State End but will call On State Begin.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Parallel States", meta = (NodeBaseOnly))
	uint16 bAllowParallelReentry: 1;

	/**
	 * If the state should remain active even after a transition is taken from this state.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Parallel States", meta = (NodeBaseOnly))
	uint16 bStayActiveOnStateChange: 1;
	
	/**
	 * Prevents the `Any State` node from adding transitions to this node.
	 * This can be useful for maintaining end states.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Any State", meta = (NodeBaseOnly))
	uint16 bExcludeFromAnyState: 1;

public:
	/**
	 * Called right before the state has started. This is meant for an observer to be aware of when a state is about
	 * to start. This will only be called for state stack instances if the state is allowed to execute logic.
	 * Do not change states from within this event, the state change process will still be in progress.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Logic Driver|Node Instance")
	FOnStateBeginSignature OnStateBeginEvent;

	/**
	 * Called right after the state has started. This is meant for an observer to be aware of when a state has started.
	 * If you need to change states this event is safer to use, but ideally state changes should be managed by transitions
	 * or from within the state.
	 *
	 * This will only be called for state stack instances if the state is allowed to execute logic, and will be called before
	 * the primary instance's OnPostStateBeginEvent. It is not safe to change states from this event when broadcast from
	 * a state stack instance.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Logic Driver|Node Instance")
	FOnStateBeginSignature OnPostStateBeginEvent;

	/**
	 * Called before the state has updated. This is meant for an observer to be aware of when a state is updating
	 * and it is not advised to switch states from this event.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Logic Driver|Node Instance")
	FOnStateUpdateSignature OnStateUpdateEvent;

	/**
	 * Called before the state has ended. It is not advised to switch states during this event.
	 * The state machine will already be in the process of switching states.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Logic Driver|Node Instance")
	FOnStateEndSignature OnStateEndEvent;
};

/**
 * The base class for state nodes. This is where most execution logic should be defined.
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = LogicDriver, hideCategories = (SMStateInstance), meta = (DisplayName = "State Class"))
class SMSYSTEM_API USMStateInstance : public USMStateInstance_Base
{
	GENERATED_BODY()

public:
	USMStateInstance();

	/** Called before OnStateBegin and before transitions are initialized. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Node Instance")
	void OnStateInitialized();

	/** Called after OnStateEnd and after transitions are shutdown. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Node Instance")
	void OnStateShutdown();
	
	/**
	 * Retrieve all state instances in the state stack.
	 *
	 * @param StateStackInstances [Out] State stack instances in their correct order.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	void GetAllStateStackInstances(TArray<USMStateInstance_Base*>& StateStackInstances) const;
	
	/**
	 * Retrieve a state instance from within the state stack.
	 * 
	 * @param Index the index of the array.
	 * @return the state if the index is valid.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	USMStateInstance_Base* GetStateInStack(int32 Index) const;

	/**
	 * Retrieve the first stack instance of a given class.
	 *
	 * @param StateClass The state class to search for.
	 * @param bIncludeChildren If children of the given class can be included.
	 * @return the first state that matches the class.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	USMStateInstance_Base* GetStateInStackByClass(TSubclassOf<USMStateInstance> StateClass, bool bIncludeChildren = false) const;

	/**
	 * Retrieve the owning node instance of a state stack. If this is called from the main node instance
	 * it will return itself.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	USMStateInstance_Base* GetStackOwnerInstance() const;
	
	/**
	 * Retrieve all states that match the given class.
	 *
	 * @param StateClass The state class to search for.
	 * @param bIncludeChildren If children of the given class can be included.
	 * @param StateStackInstances [Out] State stack instances matching the given class.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	void GetAllStatesInStackOfClass(TSubclassOf<USMStateInstance> StateClass, TArray<USMStateInstance_Base*>& StateStackInstances, bool bIncludeChildren = false) const;

	/**
	* Retrieve the index of a state stack instance.
	* O(n).
	*
	* @param StateInstance The state instance to lookup in the stack.
	* @return The index of the state in the stack. -1 if not found or is the base state instance.
	*/
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	int32 GetStateIndexInStack(USMStateInstance_Base* StateInstance) const;
	
	/** The total number of states in the state stack. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	int32 GetStateStackCount() const;

	/**
	 * Add a state to the state stack. For use during editor construction scripts only.
	 *
	 * @param StateClass The state class to be created.
	 * @param StackIndex The index to insert the node stack. Leave at -1 to place at the end.
	 *
	 * @return The stack instance created.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance", meta = (DevelopmentOnly))
	USMStateInstance* AddStateToStack(TSubclassOf<USMStateInstance> StateClass, int32 StackIndex = -1);

	/**
	 * Remove a state from the stack by index. For use during editor construction scripts only.
	 *
	 * @param StackIndex The index to remove. Leave at -1 to remove from the end.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance", meta = (DevelopmentOnly))
	void RemoveStateFromStack(int32 StackIndex = -1);

	/**
	 * Remove all states from the state stack. For use during editor construction scripts only.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance", meta = (DevelopmentOnly))
	void ClearStateStack();

protected:
	/* Override in native classes to implement. Never call these directly. */
	
	virtual void OnStateInitialized_Implementation() {}
	virtual void OnStateShutdown_Implementation() {}
};

/**
 * Represents an entry state on the state machine graph. Used for rule behavior.
 */
UCLASS(MinimalAPI, NotBlueprintable, Transient, ClassGroup = LogicDriver, hideCategories = (SMEntryStateInstance), meta = (DisplayName = "Entry State"))
class USMEntryStateInstance final : public USMStateInstance_Base
{
	GENERATED_BODY()

public:
	USMEntryStateInstance()
	{
#if WITH_EDITORONLY_DATA
		bRegisterWithContextMenu = false;
#endif
	}
};

/**
 * Represents an Any State on the state machine graph. Used for rule behavior.
 */
UCLASS(MinimalAPI, NotBlueprintable, Transient, ClassGroup = LogicDriver, hideCategories = (SMAnyStateInstance), meta = (DisplayName = "Any State"))
class USMAnyStateInstance final : public USMStateInstance_Base
{
	GENERATED_BODY()

public:
	USMAnyStateInstance()
	{
#if WITH_EDITORONLY_DATA
		bRegisterWithContextMenu = false;
#endif
	}
};