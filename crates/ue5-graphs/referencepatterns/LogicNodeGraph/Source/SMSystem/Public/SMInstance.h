// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "ISMStateMachineInterface.h"
#include "SMNode_Info.h"
#include "SMStateMachine.h"
#include "SMStateMachineInstance.h"

#include "Async/AsyncWork.h"
#include "Engine/LatentActionManager.h"
#include "HAL/CriticalSection.h"
#include "Tickable.h"

#include "SMInstance.generated.h"

class FSMCachedPropertyData;
class USMTransitionInstance;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStateMachineInitializedSignature, class USMInstance*, Instance);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStateMachineStartedSignature, class USMInstance*, Instance);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStateMachineUpdatedSignature, class USMInstance*, Instance, float, DeltaSeconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStateMachineStoppedSignature, class USMInstance*, Instance);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStateMachineShutdownSignature, class USMInstance*, Instance);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStateMachineTransitionTakenSignature, class USMInstance*, Instance, struct FSMTransitionInfo, Transition);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnStateMachineStateChangedSignature, class USMInstance*, Instance, struct FSMStateInfo, NewState, struct FSMStateInfo, PreviousState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStateMachineStateStartedSignature, class USMInstance*, Instance, struct FSMStateInfo, State);

DECLARE_DELEGATE_OneParam(FOnStateMachineInstanceInitializedAsync, USMInstance* /* Instance */);
DECLARE_DELEGATE(FOnReferencesReplicated);

USTRUCT()
struct FSMDebugStateMachine
{
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA
	SMSYSTEM_API const FSMNode_Base* GetRuntimeNode(const FGuid& Guid) const;
	SMSYSTEM_API void UpdateRuntimeNode(FSMNode_Base* RuntimeNode);

	/** All states including nested state machine states. These are only NodeGuids and not PathGuids. */
	TMap<FGuid, TArray<FSMNode_Base*>> MappedNodes;
#endif
};

USTRUCT()
struct FSMReferenceContainer
{
	GENERATED_BODY()

	FSMReferenceContainer() : Reference(nullptr) {}
	
	UPROPERTY(meta = (IgnoreForMemberInitializationTest))
	FGuid PathGuid;

	UPROPERTY()
	TObjectPtr<USMInstance> Reference;
};

USTRUCT()
struct FSMGuidMap
{
	GENERATED_BODY()

	/** A NodeGuid to a PathGuid. */
	UPROPERTY()
	TMap<FGuid, FGuid> NodeToPathGuids;
};

/**
 * The base class all blueprint state machines inherit from.
 * Each instance contains a "Root" state machine which is the top-level FSM defined in the graph and accessible through GetRootStateMachineNodeInstance().
 *
 * Instances need to be initialized before they can be used. The process is:
 * - Initialize(): Loads and maps out all contained states and transitions.
 * - LoadFromMultipleStates(): [Optional] Only needed if any saved states need to be loaded.
 * - Start(): Begins processing the state machine.
 * - Stop(): Stops updating and running the state machine.
 * - Shutdown(): Clears internal resources and empties the state machine. Initialize must be called again before use. This generally isn't necessary to call.
 *
 * When using state machine components most methods should be called from the component instead.
 *
 * @see [Instantiation](https://logicdriver.com/docs/pages/instantiation/)
 * @see [Order of Operations](https://logicdriver.com/docs/pages/orderofoperations/)
 * @see USMStateMachineComponent
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = LogicDriver, hideCategories = (SMInstance), AutoExpandCategories =("State Machine Instance|Tick", "State Machine Instance|Logging"), meta = (DisplayName = "State Machine Instance"))
class SMSYSTEM_API USMInstance : public UObject, public FTickableGameObject, public ISMStateMachineInterface, public ISMInstanceInterface
{
	GENERATED_BODY()

private:
	class FInitializeInstanceAsyncTask : public FNonAbandonableTask
	{
	public:
		TWeakObjectPtr<USMInstance> Instance;
		TWeakObjectPtr<UObject> Context;
		
		FInitializeInstanceAsyncTask(USMInstance* InInstance, UObject* InContext)
		{
			Instance = InInstance;
			Context = InContext;
		}
 
		FORCEINLINE TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(InitializeStateMachineInstanceAsyncTask, STATGROUP_ThreadPoolAsyncTasks);
		}
 
		void DoWork();
	};
	
public:
	friend class USMStateMachineComponent;
	
	USMInstance();
	// FTickableGameObject
	/** The native tick is required to update the state machine. */
	UFUNCTION(BlueprintNativeEvent, Category = "State Machine Instances")
	void Tick(float DeltaTime) override;
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	virtual bool IsTickable() const override;
	virtual bool IsTickableInEditor() const override;
	virtual ETickableTickType GetTickableTickType() const override;
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	virtual bool IsTickableWhenPaused() const override { return bCanTickWhenPaused; }
	virtual UWorld* GetTickableGameObjectWorld() const override;
	virtual TStatId GetStatId() const override;
	// ~FTickableGameObject

	// UObject
	virtual bool IsSupportedForNetworking() const override { return true; }
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty> & OutLifetimeProps) const override;
	virtual void BeginDestroy() override;
	virtual UWorld* GetWorld() const override;
	virtual int32 GetFunctionCallspace(UFunction* Function, FFrame* Stack) override;
	virtual bool CallRemoteFunction(UFunction* Function, void* Parms, FOutParmRec* OutParms, FFrame* Stack) override;
	// ~UObject

	// ISMInstanceInterface
	/** The object which this state machine is running for. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	virtual UObject* GetContext() const override;
	// ~ISMInstanceInterface

	/** The component owning this instance. Will be null during initialization or if this state machine was created without a component. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	USMStateMachineComponent* GetComponentOwner() const { return ComponentOwner; }

	/**
	 * Override the component owner. Should not be necessary to ever call unless you have a custom component
	 * which is managing initialization or replication.
	 */
	void SetComponentOwner(USMStateMachineComponent* InComponent) { ComponentOwner = InComponent; }

	// USMStateMachineInterface
	/**
	 * Prepare the state machine for use.
	 *
	 * @param Context The context to use for the state machine.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	virtual void Initialize(UObject* Context) override;

	/**
	 * Start the root state machine. This is a local call only and is not replicated.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	virtual void Start() override;

	/** Manual way of updating the root state machine if tick is disabled. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	virtual void Update(float DeltaSeconds=0.f) override;
	
	/**
	 * Complete the state machine's current state and force the machine to end regardless of if the state is an end state.
	 * This is a local call only and is not replicated.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	virtual void Stop() override;

	/**
	 * Forcibly restart the state machine and place it back into an entry state.
	 * This is a local call only and is not replicated.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	virtual void Restart() override;
	
	/**
	 * Shutdown this instance. Calls Stop(). Must call Initialize() again before use.
	 * If the goal is to restart the state machine later call Stop() instead.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	virtual void Shutdown() override;

	// ~USMStateMachineInterface

	/**
	 * Calls Start() locally or on the component owner if valid.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	void ReplicatedStart();

	/**
	 * Calls Stop() locally or on the component owner if valid.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	void ReplicatedStop();

	/**
	 * Calls Restart() locally or on the component owner if valid.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	void ReplicatedRestart();
	
	/**
	 * Sets a new context and starts the state machine.
	 * The state machine should be stopped prior to calling.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	void StartWithNewContext(UObject* Context);
	
	/**
	 * Signals to the owning state machine to process transition evaluation.
	 * This is similar to calling Update on the owner root state machine, however state update logic (Tick) won't execute.
	 * All transitions are evaluated as normal starting from the root state machine down.
	 * Depending on super state transitions it's possible the state machine this state is part of may exit.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	void EvaluateTransitions();

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
	 * This method is for advanced usage and not required for normal operation.
	 *
	 * @param InFirstTransitionInstance The transition instance, which should be the first part of a transition chain.
	 * @return True if the chain succeeded evaluation.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	UPARAM(DisplayName="Success") bool EvaluateAndTakeTransitionChain(USMTransitionInstance* InFirstTransitionInstance);
	bool EvaluateAndTakeTransitionChain(FSMTransition& InFirstTransition);

	/**
	 * Evaluate an entire transition chain discovering the path to take. This will not switch states or take the
	 * transition chain. It will only discover the first valid transition chain at the moment of execution.
	 *
	 * A transition chain is the first path that evaluates to true between two states, consisting of all transitions
	 * and conduits that are configured to eval with transitions. In many cases it is of size 0 or 1.
	 *
	 * This should only be used for conditional (tick) evaluation. If an event has triggered a transition but the state
	 * machine has not taken it yet, this method may clear the event status preventing the transition from being taken
	 * natively.
	 *
	 * If the transition chain returned should be taken, use TakeTransitionChain().
	 *
	 * This method is for advanced usage and not required for normal operation.
	 * 
	 * @param InFirstTransitionInstance The transition instance, which should be the first part of a transition chain.
	 * @param OutTransitionChain The first valid transition chain where every node has evaluated to true. Only transitions are returned.
	 * @param OutDestinationState The destination state at the end of the chain.
	 * @param bRequirePreviousStateActive If the previous state is not active then cancel evaluation.
	 * @return True if a valid transition chain was found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	UPARAM(DisplayName="Success") bool EvaluateAndFindTransitionChain(USMTransitionInstance* InFirstTransitionInstance,
		TArray<USMTransitionInstance*>& OutTransitionChain, USMStateInstance_Base*& OutDestinationState, bool bRequirePreviousStateActive = true);

	/**
	 * Tell the state machine to take a specific transition chain. The chain is assumed to be true and will not be
	 * evaluated. There is no integrity checking to make sure the chain properly connects two states. All other
	 * state change behavior is respected including requiring the previous state active.
	 *
	 * This method is for advanced usage and not required for normal operation.
	 *
	 * @param InTransitionChain The transition chain to be taken consisting of transitions and conduits.
	 * @return True if the chain was taken and the state switched.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	UPARAM(DisplayName="Success") bool TakeTransitionChain(const TArray<USMTransitionInstance*>& InTransitionChain);
	
	/**
	 * Ensure all default node instances are loaded into memory. Default node classes are normally loaded on demand to save on memory
	 * and initialization time. Preloading isn't necessary unless most default nodes need to be accessed programatically, such as calling
	 * GetNodeInstance() from a local state graph.
	 *
	 * This has no effect on nodes assigned a custom node class.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	void PreloadAllNodeInstances();

	/**
	 * Activate or deactivate a single state locally. This call is not replicated. Use the SetActive method from
	 * the state instance to allow replication.
	 *
	 * @param StateGuid The guid of the specific state to activate.
	 * @param bActive Add or remove the state from the owning state machine's active states.
	 * @param bSetAllParents Sets the active state of all super state machines. A parent state machine won't be deactivated unless there are no other states active.
	 * @param bActivateNow If the state is becoming active it will run OnStateBegin this tick. If this is false it will run on the next update cycle.
	 */
	void ActivateStateLocally(const FGuid& StateGuid, bool bActive, bool bSetAllParents, bool bActivateNow = true);

	/**
	 * Switch the activate state. Does not take any transition. Handles replication and requires state change authority.
	 *
	 * @param NewStateInstance The state to switch to. Null is accepted if you wish to deactivate all other states.
	 * @param bDeactivateOtherStates If other states should be deactivated first. A state won't be deactivated if it is a super state machine to the new state.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	void SwitchActiveState(USMStateInstance_Base* NewStateInstance, bool bDeactivateOtherStates = true);

	/**
	 * Switch to a state instance by its fully qualified name in a state machine blueprint.
	 * Does not take any transition. Handles replication and requires state change authority.
	 * 
	 * A top level state name of "Locomotion" would be found by searching "Locomotion".
	 * A nested state of name "Jump" within a "Locomotion" state machine would be found by "Locomotion.Jump".
	 *
	 * It is not necessary to include the "Root" state machine node in the search.
	 *
	 * The search is performed iteratively and the performance is roughly O(n) by number of nodes in the path.
	 * The lookup of each state is O(1).
	 *
	 * @param InFullPath The full path to the node. When the state machine type is known a UI dropdown is available.
	 * Cast the target and refresh the node to update the UI.
	 * @param bDeactivateOtherStates If other states should be deactivated first. A state won't be deactivated if it is a super state machine to the new state.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances", meta = (UseLogicDriverStatePicker="InFullPath"))
	void SwitchActiveStateByQualifiedName(const FString& InFullPath, bool bDeactivateOtherStates = true);
	
	/** If there are states that need their active state changed. */
	bool HasPendingActiveStates() const { return StatesPendingActivation.Num() > 0; }

	/** If this state machine instance is in an update cycle. */
	bool IsUpdating() const { return bIsUpdating; }

	/**
	 * If the state machine instance is currently in the process of stopping.
	 * Note that this will be true if the state machine was stopped via `#bStopOnEndState`.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	bool IsStopping() const { return bIsStopping; }
	
private:
	/**
	 * States that need to be activated or deactivated, stored to help the update cycle
	 * in the event an end state is detected prematurely.
	 */
	TArray<FSMState_Base*> StatesPendingActivation;
	
public:
	/**
	 * Sets a temporary initial state of the guid's owning state machine. When the state machine starts it will default to this state.
	 * With AllParents to true this is useful for restoring a single active state of a state machine from GetSingleActiveStateGuid().
	 * If there are multiple active states, or the state of non active sub state machines is important use LoadFromMultipleStates() instead.
	 *
	 * This should only be called on an initialized state machine that is stopped.
	 *
	 * When using with replication this should be called from the server, or from a client that has state change authority.
	 * 
	 * @param FromGuid The state guid to load.
	 * @param bAllParents Recursively set the initial state of all parent state machines.
	 * @param bNotify Calls OnStateMachineInitialStateLoaded on this instance and sets AreInitialStatesSetFromLoad().
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	void LoadFromState(const FGuid& FromGuid, bool bAllParents = true, bool bNotify = true);

	/**
	 * Set all owning parents' temporary initial state to the given guids. Useful for restoring multiple states within a state machine.
	 * Best used when restoring from GetAllActiveStateGuids(). When the state machine starts it will default to these states.
	 *
	 * This should only be called on an initialized state machine that is stopped.
	 *
	 * When using with replication this should be called from the server, or from a client that has state change authority.
	 * 
	 * @param FromGuids Array of state guids to load.
	 * @param bNotify Calls OnStateMachineInitialStateLoaded on this instance and sets AreInitialStatesSetFromLoad().
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	void LoadFromMultipleStates(const TArray<FGuid>& FromGuids, bool bNotify = true);

	/**
	 * Checks if initial entry states have been set through LoadFromState() or LoadFromMultipleStates().
	 * This will be true if at least one state was loaded this way and will become false once Stop() is called.
	 *
	 * In a replicated environment this is only accurate when called from the machine that performed the initial load.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	bool AreInitialStatesSetFromLoad() const { return bLoadFromStatesCalled; }

	/**
	 * Clear all temporary initial states loaded through LoadFromState(). Do not use while the state machine
	 * is active and replicated.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	void ClearLoadedStates();

protected:
	/**
	 * Called after an initial state has been set with LoadFromState() or LoadFromMultipleStates().
	 * This may be called multiple times depending whether there is more than one state being loaded
	 * or if parent state machines are also being loaded.
	 *
	 * @param StateGuid The guid of the state being loaded. The state instance can be retrieved with GetStateInstanceByGuid.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "State Machine Instances")
	void OnStateMachineInitialStateLoaded(const FGuid& StateGuid);
	virtual void OnStateMachineInitialStateLoaded_Implementation(const FGuid& StateGuid);

	/** Called after Initialize sequence completed for methods that must run on the GameThread. */
	virtual void FinishInitialize();

	/**
	 * Check if in an end state and stop.
	 * @return true if stopped.
	 */
	bool HandleStopOnEndState();
	
public:
	/**
	 * Prepare the state machine for use on a separate thread. If this object needs to be destroyed before the process completes
	 * you should call Shutdown() or CancelAsyncInitialization() to attempt to safely exit the thread.
	 *
	 * @param Context The context to use for the state machine.
	 * @param OnCompletedDelegate A callback delegate for when initialization completes. This only will fire if initialization was a success.
	 */
	void InitializeAsync(UObject* Context, const FOnStateMachineInstanceInitializedAsync& OnCompletedDelegate = FOnStateMachineInstanceInitializedAsync());

	/**
	 * Prepare the state machine for use on a separate thread. If this object needs to be destroyed before the process completes
	 * you should call Shutdown() or CancelAsyncInitialization() to attempt to safely exit the thread.
	 *
	 * @param Context The context to use for the state machine. This object must have a valid world assigned for a latent task to process correctly.
	 */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "Initialize Async", Latent, LatentInfo="LatentInfo"), Category="Logic Driver|State Machine Instances")
	void K2_InitializeAsync(UObject* Context, FLatentActionInfo LatentInfo);

	/** Attempt to cancel the async initialization task. This will perform a blocking wait for the task to finish if necessary. */
	void CancelAsyncInitialization();

	/**
	* Wait blocking for the async task to complete.
	* The object may not be fully initialized after this as FinishInitialize must occur
	* on the game thread and is scheduled to run through Unreal's task graph system.
	*
	* Code that should execute after initialization is completed ideally should be done through the
	* call back of InitializeAsync(). However if bCallFinishInitialize is set, it should
	* be safe to execute code on this instance after this call. Check IsInitialized() to validate
	* the instance has successfully initialized.
	*
	* @param bCallFinishInitialize Call FinishInitialize immediately after the task completes if required on this thread. (Only valid if on GameThread)
	*/
	void WaitForAsyncInitializationTask(bool bCallFinishInitialize = true);
	
protected:
	/** Async objects need their async flags removed so they can be garbage collected. */
	virtual void CleanupAsyncObjects();

	/** Wait for the async task to complete and free memory on completion.  */
	void CleanupAsyncInitializationTask();

private:
	/** When the engine is about to collect garbage. */
	void OnPreGarbageCollect();

	/** Cleanup our handles to GC delegates. */
	void CleanupGCDelegates();
	
private:
	FDelegateHandle OnPreGarbageCollectHandle;
	
	/** The async initialization task. */
	TUniquePtr<FAsyncTask<FInitializeInstanceAsyncTask>> AsyncInitializationTask;

	/** Set from caller of initialize async function. */
	FOnStateMachineInstanceInitializedAsync OnStateMachineInitializedAsyncDelegate;

public:
	/** Retrieve the lowest level single active state including all nested state machines. Returns read only information. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances", meta = (DisplayName = "Try Get Single Nested Active State"))
	void TryGetNestedActiveState(FSMStateInfo& FoundState, bool& bSuccess) const;

	/**
	 * Return the current top level active state. Do not use if there are multiple active states.
	 * @return null or the first active top level state.
	 */
	FSMState_Base* GetSingleActiveState() const;

	/**
	 * Retrieve the first lowest level active state including all nested state machines. Do not use if there are multiple active states.
	 * @return null or the first active state.
	 */
	FSMState_Base* GetSingleNestedActiveState() const;

	/** Recursively retrieve all active states. */
	TArray<FSMState_Base*> GetAllActiveStates() const;

	/**
	 * Retrieve the first active state Guid. If a state is not active an invalid Guid will be returned.
	 * For multiple states GetAllActiveStateGuids() should be called instead.
	 * @param bCheckNested Check nested state machines.
	 * @return the first active state guid.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	FGuid GetSingleActiveStateGuid(bool bCheckNested=true) const;
	
	/**
	 * Recursively retrieve the guids of all active states. Useful if saving the current state of a state machine.
	 * @param ActiveGuids [Out] All current ActiveGuids. May be empty. Resets on method start.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	void GetAllActiveStateGuids(TArray<FGuid>& ActiveGuids) const;

	/**
	 * Convenience method wrapper for GetAllActiveStateGuids().
	 * 
	 * Recursively retrieve the guids of all active states. Useful if saving the current state of a state machine.
	 * @return All current ActiveGuids. May be empty.
	 */
	TArray<FGuid> GetAllActiveStateGuidsCopy() const;
	
	/**
	 * Locate the first active state instance. For multiple states GetAllActiveStateInstances() should be called instead.
	 * 
	 * @param bCheckNested Check nested state machines.
	 * @return A single active state's node instance.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	USMStateInstance_Base* GetSingleActiveStateInstance(bool bCheckNested = true) const;
	
	/**
	 * Recursively retrieve all active state instances.
	 * @param ActiveStateInstances [Out] All active state instances. May be empty. Resets on method start.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	void GetAllActiveStateInstances(TArray<USMStateInstance_Base*>& ActiveStateInstances) const;
	
	/** Find all referenced instances. IncludeChildren will return all nested state machine references.*/
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	TArray<USMInstance*> GetAllReferencedInstances(bool bIncludeChildren = false) const;

	/** Find all internal state machine structs which contain references. */
	TArray<FSMStateMachine*> GetStateMachinesWithReferences(bool bIncludeChildren = false) const;
	
	/** Quickly returns read only information of the state belonging to the given guid. This always executes from the primary instance. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	void TryGetStateInfo(const FGuid& Guid, FSMStateInfo& StateInfo, bool& bSuccess) const;

	/** Quickly returns read only information of the transition belonging to the given guid. This always executes from the primary instance. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	void TryGetTransitionInfo(const FGuid& Guid, FSMTransitionInfo& TransitionInfo, bool& bSuccess) const;

	/** Quickly return a referenced instance given a state machine guid. This always executes from the primary instance. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	USMInstance* GetReferencedInstanceByGuid(const FGuid& Guid) const;

	/** Quickly return a state instance given the state guid. This always executes from the primary instance. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	USMStateInstance_Base* GetStateInstanceByGuid(const FGuid& Guid) const;

	/** Quickly return a transition instance given a transition guid. This always executes from the primary instance. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	USMTransitionInstance* GetTransitionInstanceByGuid(const FGuid& Guid) const;

	/** Quickly return any type of node instance. These could be transitions or states. This always executes from the primary instance. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	USMNodeInstance* GetNodeInstanceByGuid(const FGuid& Guid) const;

	/**
	 * Return a state instance by its fully qualified name in a state machine blueprint.
	 * A top level state name of "Locomotion" would be found by searching "Locomotion".
	 * A nested state of name "Jump" within a "Locomotion" state machine would be found by "Locomotion.Jump".
	 *
	 * It is not necessary to include the "Root" state machine node in the search.
	 *
	 * The search is performed iteratively and the performance is roughly O(n) by number of nodes in the path.
	 * The lookup of each state is O(1).
	 *
	 * @param InFullPath The full path to the node. When the state machine type is known a UI dropdown is available.
	 * Cast the target and refresh the node to update the UI.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances", meta = (UseLogicDriverStatePicker="InFullPath"))
	USMStateInstance_Base* GetStateInstanceByQualifiedName(const FString& InFullPath) const;
	
	/** Quick lookup of a state by guid. Includes all nested. */
	FSMState_Base* GetStateByGuid(const FGuid& Guid) const;

	/** Quick lookup of any transition by guid. Includes all nested.  */
	FSMTransition* GetTransitionByGuid(const FGuid& Guid) const;

	/** Quick lookup of any node by guid. Includes all nested.  */
	FSMNode_Base* GetNodeByGuid(const FGuid& Guid) const;
	
	/** Linear search all state machines for a contained node. */
	FSMState_Base* FindStateByGuid(const FGuid& Guid) const;

	/** A map of path guids which should be redirected to other path guids. */
	UFUNCTION(BlueprintGetter, BlueprintPure, Category = "Logic Driver|State Machine Instances")
	TMap<FGuid, FGuid>& GetGuidRedirectMap() { return PathGuidRedirectMap; }

	/** Directly set the guid redirect map. */
	UFUNCTION(BlueprintSetter, Category = "Logic Driver|State Machine Instances")
	void SetGuidRedirectMap(const TMap<FGuid, FGuid>& InGuidMap) { PathGuidRedirectMap = InGuidMap; }

	/**
	 * Find a redirected path guid.
	 *
	 * @param InPathGuid The old path guid which should point to a new guid.
	 *
	 * @return The redirected guid, or the original guid.
	 */
	FGuid GetRedirectedGuid(const FGuid& InPathGuid) const;

public:
	/** The root state machine which may contain nested state machines. */
	FSMStateMachine& GetRootStateMachine() { return RootStateMachine; }

	/**
	 * Return the root USMStateMachineInstance node.
	 * 
	 * This is relative to the SMInstance you are calling it from. If this is a state machine reference
	 * it will return the reference's root state machine node instance.
	 *
	 * To retrieve the primary root state machine node instance, call this method from GetPrimaryReferenceOwner().
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	USMStateMachineInstance* GetRootStateMachineNodeInstance() const;
	
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	bool IsActive() const;

	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	bool CanEverTick() const { return bCanEverTick; }

	/**
	 * Set the conditional tick state. If false IsTickable() will return false.
	 * This will also update the component owner network settings if this call is made from the primary instance.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	void SetCanEverTick(bool Value);

	/** If the tick function has been registered. */
	bool IsTickRegistered() const { return bTickRegistered; }
	
	/** When false prevents the tick function from ever being registered. Can only be called along with initialize and cannot be changed. */
	void SetRegisterTick(bool Value);

	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	void SetTickOnManualUpdate(bool Value);

	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	bool CanTickOnManualUpdate() const { return bCallTickOnManualUpdate; }
	
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	void SetCanTickWhenPaused(bool Value);

#if WITH_EDITORONLY_DATA
	void SetCanTickInEditor(bool Value);
#endif
	
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	void SetTickBeforeBeginPlay(bool Value);
	
	/** Time in seconds between native ticks. This mostly affects the "Update" rate of the state machine. Overloaded Ticks won't be affected. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	void SetTickInterval(float Value);

	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	void SetAutoManageTime(bool Value);

	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	bool CanAutoManageTime() const { return bAutoManageTime; }
	
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	float GetTickInterval() const { return TickInterval; }

	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	void SetStopOnEndState(bool Value);

	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	bool GetStopOnEndState() const { return bStopOnEndState; }

	/** True if the root state machine is in an end state. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	bool IsInEndState() const;

	/** Sets a new context. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	void SetContext(UObject* Context);

	/** Get all mapped PathGuids to nodes. */
	const TMap<FGuid, FSMNode_Base*>& GetNodeMap() const;

	/** Get all mapped PathGuids to states. */
	const TMap<FGuid, FSMState_Base*>& GetStateMap() const;

	/** Get all mapped PathGuids to transitions. */
	const TMap<FGuid, FSMTransition*>& GetTransitionMap() const;

	/** Retrieve an ordered history of states, oldest to newest, not including active state(s). This always executes from the primary instance. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	const TArray<FSMStateHistory>& GetStateHistory() const;

	/**
	 * Sets the maximum number of states to record into history.
	 * Resizes the array removing older entries if needed.
	 * @param NewSize The number of states to record. Set to -1 for no limit.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	void SetStateHistoryMaxCount(int32 NewSize);

	/**
	 * Return the maximum history count.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	int32 GetStateHistoryMaxCount() const;

	/**
	 * Empty the state history array.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	void ClearStateHistory();
	
	/**
	 * Retrieve all state instances throughout the entire state machine blueprint.
	 * These can be states, state machines, and conduits.
	 *
	 * This includes all nested states in sub state machines and references.
	 * This does not include State Stack instances.
	 *
	 * To retrieve only top level states call GetRootStateMachineNodeInstance() and
	 * from there call 'GetAllStateInstances'.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	void GetAllStateInstances(TArray<USMStateInstance_Base*>& StateInstances) const;

	/**
	 * Retrieve all transition instances throughout the entire state machine blueprint.
	 *
	 * This includes all transitions nested in sub state machines and references.
	 * This does not include Transition Stack instances.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	void GetAllTransitionInstances(TArray<USMTransitionInstance*>& TransitionInstances) const;
	
	/** Notifies this instance there is a server interface. */
	void SetNetworkInterface(TScriptInterface<ISMStateMachineNetworkedInterface> InNetworkInterface);

	/** Return the server interface if there is one. This may be null. Always executes from the primary instance. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	TScriptInterface<ISMStateMachineNetworkedInterface> GetNetworkInterface() const;

	/** Return the server interface if there is one. This may be null. Always executes from the primary instance. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances", meta = (DisplayName = "TryGetNetworkInterface"))
	void K2_TryGetNetworkInterface(TScriptInterface<ISMStateMachineNetworkedInterface>& Interface, bool& bIsValid);
	
	/** Return the network interface if it set and if networking is enabled. Always executes from the primary instance. */
	ISMStateMachineNetworkedInterface* TryGetNetworkInterface() const;
	
	/** Updates all needed nodes with the current network conditions. Best to be called after Initialization and before Start. */
	void UpdateNetworkConditions();

	/** Copy network settings from the other instance. */
	void CopyNetworkConditionsFrom(USMInstance* OtherInstance, bool bUpdateNodes = false);

	/**
	 * Notifies state machines they are allowed to take transitions locally.
	 * @param bCanEvaluateTransitions If a state machine can check transition results.
	 * @param bCanTakeTransitions If a state machine can take a transition after evaluating it.
	 */
	void SetAllowTransitionsLocally(bool bCanEvaluateTransitions, bool bCanTakeTransitions);

	/** Notifies state machines if they are allowed to execute state logic locally. */
	void SetAllowStateLogic(bool bAllow);

	/** True if the instance has started. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	bool HasStarted() const { return bHasStarted; }

	/** If this instance is fully initialized. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	bool IsInitialized() const { return bInitialized; }

	/** True only during async initialization. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	bool IsInitializingAsync() const { return bInitializingAsync; }
	
	/** If this is an archetype object used as custom defaults for a reference. */
	bool IsReferenceTemplate() const;

	/** Notify this instance is a reference owned by another instance. */
	void SetReferenceOwner(USMInstance* Owner);

	/** Record a reference to be replicated to the client. */
	void AddReplicatedReference(const FGuid& PathGuid, USMInstance* NewReference);

	/** Find a replicated reference by its path guid. */
	USMInstance* FindReplicatedReference(const FGuid& PathGuid) const;

	/** Return all references set to be replicated. */
	const TArray<FSMReferenceContainer>& GetReplicatedReferences() const { return ReplicatedReferences; }

	/** Checks if all references on the primary instance have been received. */
	bool HaveAllReferencesReplicated() const;

	/** If this instance is allowed to replicate if it is a reference. */
	bool CanReplicateAsReference() const { return bCanReplicateAsReference; }
	
private:
	UFUNCTION()
	void REP_OnReplicatedReferencesLoaded();

private:
	FOnReferencesReplicated OnReferencesReplicatedEvent;
	
	/** Set at initialization time on the primary instance, containing all nested references. Used for reference replicated variables. */
	UPROPERTY(ReplicatedUsing=REP_OnReplicatedReferencesLoaded, Transient)
	TArray<FSMReferenceContainer> ReplicatedReferences;
	
public:
	/** Get the instance owning this reference. If null this is not a reference. */
	const USMInstance* GetReferenceOwnerConst() const { return ReferenceOwner; }

	/** Look up the owners to find the top-most instance. */
	const USMInstance* GetPrimaryReferenceOwnerConst() const;
	
	/** Check if this is the top most instance. */
	bool IsPrimaryReferenceOwner() const { return this == GetPrimaryReferenceOwnerConst(); }
	
	/** Get the instance owning this reference. If null this is not a reference. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	USMInstance* GetReferenceOwner() const { return ReferenceOwner; }

	/** Look up the owners to find the top-most instance. Could be this instance. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Logic Driver|State Machine Instances")
	USMInstance* GetPrimaryReferenceOwner();
	
	/** The custom state machine node class assigned to the root state machine node. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Logic Driver|State Machine Instances")
	TSubclassOf<USMStateMachineInstance> GetRootStateMachineNodeClass() const
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return StateMachineClass;
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	/** @deprecated Use GetRootStateMachineNodeClass() instead. */
	UE_DEPRECATED(5.1, "This function is deprecated. Use `GetRootStateMachineNodeClass` instead.")
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Logic Driver|State Machine Instances", meta = (DeprecatedFunction, DeprecationMessage = "Use `GetRootStateMachineNodeClass` instead."))
	TSubclassOf<USMStateMachineInstance> GetStateMachineClass() const { return GetRootStateMachineNodeClass(); }

	/** The default root node name to assign. Should never be changed. */
	static FString GetRootNodeNameDefault() { return TEXT("Root"); }

	/** The name of the StateMachineClass property, which represents the RootStateMachineNodeClass. */
	static FName GetStateMachineClassPropertyName()
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return GET_MEMBER_NAME_CHECKED(USMInstance, StateMachineClass);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
	
	/**
	 * Sets the root state machine node class.
	 *
	 * @param NewStateMachineNodeClass The state machine node class to set.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	void SetRootStateMachineNodeClass(TSubclassOf<USMStateMachineInstance> NewStateMachineNodeClass)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		StateMachineClass = NewStateMachineNodeClass;
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
	
	/** @deprecated Use SetRootStateMachineNodeClass() instead. */
	UE_DEPRECATED(5.1, "This function is deprecated. Use `SetRootStateMachineNodeClass` instead.")
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances", meta = (DeprecatedFunction, DeprecationMessage = "Use `SetRootStateMachineNodeClass` instead."))
	void SetStateMachineClass(TSubclassOf<USMStateMachineInstance> NewStateMachineClass) { SetRootStateMachineNodeClass(NewStateMachineClass); }
	
	/**
	 * Called at the beginning of Initialize().
	 * Most information will not be available at this stage other than the context.
	 *
	 * Warning: When using async initialization this does not run on the game thread.
	 * Make sure your code is thread safe!
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "State Machine Instances")
	void OnPreStateMachineInitialized();
	
	/** Called when the instance is first initialized.  */
	UFUNCTION(BlueprintNativeEvent, Category = "State Machine Instances")
	void OnStateMachineInitialized();

	/** Called right before the root state machine starts its initial state.  */
	UFUNCTION(BlueprintNativeEvent, Category = "State Machine Instances")
	void OnStateMachineStart();

	/** Called right before the root state machine updates.  */
	UFUNCTION(BlueprintNativeEvent, Category = "State Machine Instances")
	void OnStateMachineUpdate(float DeltaSeconds);
	
	/** Called right after the instance has been stopped. */
	UFUNCTION(BlueprintNativeEvent, Category = "State Machine Instances")
	void OnStateMachineStop();

	/** Called right after the instance has been shutdown. This will not fire if this object is being destroyed. */
	UFUNCTION(BlueprintNativeEvent, Category = "State Machine Instances")
	void OnStateMachineShutdown();

	/** Called when a transition has evaluated to true and is being taken. */
	UFUNCTION(BlueprintNativeEvent, Category = "State Machine Instances")
	void OnStateMachineTransitionTaken(const FSMTransitionInfo& Transition);
	
	/** Called when a state machine has switched states. */
	UFUNCTION(BlueprintNativeEvent, Category = "State Machine Instances")
	void OnStateMachineStateChanged(const FSMStateInfo& ToState, const FSMStateInfo& FromState);

	/** Called when a state has started. This happens after OnStateMachineStateChanged and all previous transitions have evaluated. */
	UFUNCTION(BlueprintNativeEvent, Category = "State Machine Instances")
	void OnStateMachineStateStarted(const FSMStateInfo& State);

	/** Send transition events. */
	void NotifyTransitionTaken(const FSMTransition& Transition);

	/** Send state change events. */
	void NotifyStateChange(FSMState_Base* ToState, FSMState_Base* FromState);

	/** Send state started events. */
	void NotifyStateStarted(const FSMState_Base& State);
	
public:
	/** Used to identify the root state machine during initialization. This is not a calculated value and represents the NodeGuid. */
	UPROPERTY()
	FGuid RootStateMachineGuid;

	/**
	 * Called at the beginning of Initialize().
	 * Most information will not be available at this stage other than the context.
	 *
	 * Warning: This will not fire when the state machine is initialized async, as
	 * the broadcast is not thread safe.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Logic Driver|State Machine Instances")
	FOnStateMachineInitializedSignature OnPreStateMachineInitializedEvent;
	
	/** Called when the state machine is first initialized. */
	UPROPERTY(BlueprintAssignable, Category = "Logic Driver|State Machine Instances")
	FOnStateMachineInitializedSignature OnStateMachineInitializedEvent;

	/** Called right before the state machine is started. */
	UPROPERTY(BlueprintAssignable, Category = "Logic Driver|State Machine Instances")
	FOnStateMachineStartedSignature OnStateMachineStartedEvent;

	/** Called right before the state machine is updated. */
	UPROPERTY(BlueprintAssignable, Category = "Logic Driver|State Machine Instances")
	FOnStateMachineUpdatedSignature OnStateMachineUpdatedEvent;
	
	/** Called right after the state machine has ended. */
	UPROPERTY(BlueprintAssignable, Category = "Logic Driver|State Machine Instances")
	FOnStateMachineStoppedSignature OnStateMachineStoppedEvent;

	/** Called right after the state machine has shutdown. */
	UPROPERTY(BlueprintAssignable, Category = "Logic Driver|State Machine Instances")
	FOnStateMachineShutdownSignature OnStateMachineShutdownEvent;

	/** Called when a transition has evaluated to true and is being taken. */
	UPROPERTY(BlueprintAssignable, Category = "Logic Driver|State Machine Instances")
	FOnStateMachineTransitionTakenSignature OnStateMachineTransitionTakenEvent;

	/** Called when a state machine has switched states. */
	UPROPERTY(BlueprintAssignable, Category = "Logic Driver|State Machine Instances")
	FOnStateMachineStateChangedSignature OnStateMachineStateChangedEvent;

	/** Called when a state has started. This happens after OnStateMachineStateChanged and all previous transitions have evaluated. */
	UPROPERTY(BlueprintAssignable, Category = "Logic Driver|State Machine Instances")
	FOnStateMachineStateStartedSignature OnStateMachineStateStartedEvent;
	
#if WITH_EDITORONLY_DATA
	FSMDebugStateMachine& GetDebugStateMachine() { return DebugStateMachine; }
	const FSMDebugStateMachine& GetDebugStateMachineConst() const { return DebugStateMachine; }
#endif
	
	bool IsLoggingEnabled() const { return bEnableLogging; }

protected:
	virtual void Tick_Implementation(float DeltaTime);
	virtual void OnPreStateMachineInitialized_Implementation() {}
	virtual void OnStateMachineInitialized_Implementation() {}
	virtual void OnStateMachineStart_Implementation() {}
	virtual void OnStateMachineUpdate_Implementation(float DeltaSeconds) {}
	virtual void OnStateMachineStop_Implementation() {}
	virtual void OnStateMachineShutdown_Implementation() {}
	virtual void OnStateMachineTransitionTaken_Implementation(const FSMTransitionInfo& Transition) {}
	virtual void OnStateMachineStateChanged_Implementation(const FSMStateInfo& ToState, const FSMStateInfo& FromState) {}
	virtual void OnStateMachineStateStarted_Implementation(const FSMStateInfo& State) {}

public:
	static FName GetInternalEventUpdateFunctionName() { return GET_FUNCTION_NAME_CHECKED(USMInstance, Internal_EventUpdate); }
	static FName GetInternalEvaluateAndTakeTransitionChainFunctionName() { return GET_FUNCTION_NAME_CHECKED(USMInstance, Internal_EvaluateAndTakeTransitionChainByGuid); }
	static FName GetInternalEventCleanupFunctionName() { return GET_FUNCTION_NAME_CHECKED(USMInstance, Internal_EventCleanup); }

	/** Call from an FSM reference. Avoids the update cancelling out if already in progress
	 * and keeps behavior consistent with normal nested FSMs. */
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly, Category = "Logic Driver|State Machine Instances")
	void RunUpdateAsReference(float DeltaSeconds);

protected:
	/** Internal update logic. Can be called during an update and used by event triggers. */
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly, Category = "Logic Driver|State Machine Instances")
	void Internal_Update(float DeltaSeconds);

	/** Internal logic for taking a transition chain by the path guid. */
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly, Category = "Logic Driver|State Machine Instances")
	bool Internal_EvaluateAndTakeTransitionChainByGuid(const FGuid& PathGuid);

	/** Internal logic from when an auto-bound event might trigger an update. */
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly, Category = "Logic Driver|State Machine Instances")
	void Internal_EventUpdate();
	
	/** Internal cleanup logic after an auto-bound event fires. */
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly, Category = "Logic Driver|State Machine Instances")
	void Internal_EventCleanup(const FGuid& PathGuid);

	/** Assemble a complete map of all nested nodes and state machines. Builds out GuidNodeMap and StateMachineGuids. InstancesMapped keeps track
	 * of all instances built to prevent stack overflow in the event of state machine references that self reference. */
	void BuildStateMachineMap(FSMStateMachine* StateMachine);
	void BuildStateMachineMap(FSMStateMachine* StateMachine, TSet<USMInstance*>& InstancesMapped);

	/** Logs a warning if not initialized. */
	bool CheckIsInitialized() const;

	/** Records time running so delta time can be established if not ticking or providing accurate delta seconds. */
	void UpdateTime();

	/** Record the given state into the state history. */
	void RecordPreviousStateHistory(FSMState_Base* PreviousState);

	/** Makes sure state history current count doesn't exceed max count. */
	void TrimStateHistory();

protected:
	/** The component owning this instance. */
	UPROPERTY()
	TObjectPtr<USMStateMachineComponent> ComponentOwner;

	/** Pointer to server object to notify of active transitions. */
	UPROPERTY()
	TScriptInterface<ISMStateMachineNetworkedInterface> NetworkInterface;
	
	/** Flattened map of all node Path Guids -> Node references */
	TMap<FGuid, FSMNode_Base*> GuidNodeMap;
	
	/** Flattened map of all state Path Guids -> State references. */
	TMap<FGuid, FSMState_Base*> GuidStateMap;
	
	/** Flattened map of all transition Path Guids -> Transition references. */
	TMap<FGuid, FSMTransition*> GuidTransitionMap;

	/** Map of all StateMachine Path Guids */
	TSet<FGuid> StateMachineGuids;
	
	/** Top level state machine */
	UPROPERTY()
	FSMStateMachine RootStateMachine;

	/** The context to run the state machine in. */
	UPROPERTY(Replicated, Transient, meta = (DisplayName=Context))
	TObjectPtr<UObject> R_StateMachineContext;
	
	/** If this instance is owned by another instance making this a reference. */
	UPROPERTY()
	TObjectPtr<USMInstance> ReferenceOwner;

	/**
	 * The custom state machine node class assigned to the root state machine.
	 * This is used for rule evaluation, for exposing variables on state machine references, or for compile validation.
	 */
	UE_DEPRECATED(5.1, "Do not access directly, please use `GetRootStateMachineNodeClass` or `SetRootStateMachineNodeClass` instead. This property may eventually be renamed and become private.")
	UPROPERTY(EditDefaultsOnly, Category = "State Machine Instance", meta = (DisplayName = "Node Class", ToolTip = "The custom state machine node class assigned to the root state machine. This is used for rule evaluation, for exposing variables on state machine references, or for compile validation."))
	TSubclassOf<USMStateMachineInstance> StateMachineClass;

	/** Automatically calculate delta seconds if none are provided. Requires context with a valid World. */
	UPROPERTY(EditAnywhere, Replicated, Category = "State Machine Instance")
	uint8 bAutoManageTime: 1;

	/** Should this instance stop itself once an end state has been reached. An Update call is required for this to occur. */
	UPROPERTY(EditAnywhere, Replicated, Category = "State Machine Instance")
	uint8 bStopOnEndState: 1;
	
	/** Should this instance tick. By default it will update the state machine. */
	UPROPERTY(EditAnywhere, Replicated, Category = "State Machine Instance|Tick")
	uint8 bCanEverTick: 1;

#if WITH_EDITORONLY_DATA
	/** Should this instance tick in editor. */
	UPROPERTY(EditAnywhere, Category = "State Machine Instance|Tick")
	uint8 bCanTickInEditor: 1;
#endif
	
	/** Should this instance tick when the game is paused. */
	UPROPERTY(EditAnywhere, Replicated, Category = "State Machine Instance|Tick")
	uint8 bCanTickWhenPaused: 1;

	/**
	 * Setting to false physically prevents the tick function from being registered and the instance from ever ticking.
	 * This is different from bCanEverTick in that it cannot be changed and it also offers slightly better performance.
	 *
	 * This is generally best used with non-component state machines created by CreateStateMachineInstance.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "State Machine Instance|Tick")
	uint8 bTickRegistered: 1;

	/**
	 * Allow the state machine to tick before it is initialized.
	 * This likely isn't necessary as CreateStateMachineInstance will initialize the state machine.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "State Machine Instance|Tick")
	uint8 bTickBeforeInitialize: 1;

	/** When false IsTickable checks if the world has started play. */
	UPROPERTY(EditDefaultsOnly, Category = "State Machine Instance|Tick")
	uint8 bTickBeforeBeginPlay: 1;

	/** Time in seconds between native ticks. This mostly affects the "Update" rate of the state machine. Overloaded Ticks won't be affected. */
	UPROPERTY(EditAnywhere, Replicated, Category = "State Machine Instance|Tick", DisplayName = "Tick Interval", meta = (ClampMin = "0.0", DisplayAfter = "bCanEverTick"))
	float TickInterval;

	/** Time since the last valid tick occurred. */
	float TimeSinceAllowedTick = 0.f;
	float WorldSeconds = 0.f;
	float WorldTimeDelta = 0.f;
	
	///////////////////////
	/// Input
	///////////////////////
public:
	/** Get the AutoReceiveInput type. */
	TEnumAsByte<ESMStateMachineInput::Type> GetInputType() const { return AutoReceiveInput; }

	/** Get the input priority. */
	int32 GetInputPriority() const { return InputPriority; }

	/** Get the block input value. */
	bool GetBlockInput() const { return bBlockInput; }
	
	/** Attempt to find a valid player controller for this state machine. Requires input enabled and may return null. */
	APlayerController* GetInputController() const;

	/** Sets the auto receive input functionality. Should be done prior to initialization. */
	void SetAutoReceiveInput(ESMStateMachineInput::Type InInputType);

	/** Sets the input priority. Should be done prior to initialization. */
	void SetInputPriority(int32 InInputPriority);

	/** Sets the input priority. Should be done prior to initialization. */
	void SetBlockInput(bool bNewValue);

	/**
	 * Retrieve the input component this state machine created with #AutoReceiveInput.
	 * The input component will only be valid if AutoReceiveInput is not disabled
	 * and this state machine is initialized.
	 *
	 * @return The UInputComponent or nullptr.
	 */
	UFUNCTION(BlueprintCallable, Category = "State Machine Instance|Input")
	UInputComponent* GetInputComponent() const { return InputComponent; }

private:
	UFUNCTION()
	void OnContextPawnRestarted(APawn* Pawn);
	
protected:
	UPROPERTY(Transient)
	TObjectPtr<UInputComponent> InputComponent;

	/**
	 * Automatically registers this state machine to receive input from a player.
	 * Input events placed in the event graph will always fire and execute.
	 *
	 * Input placed in node graphs will ALWAYS fire (consume input if checked)
	 * but only execute their logic while the node is initialized.
	 *
	 * Example: State A is entered. Any input events in state A and all outbound
	 * transitions including conduits will fire and execute when the key is pressed.
	 * Once State A exits all of the input events will still fire when pressed, but
	 * any blueprint logic will not execute.
	 *
	 * If consuming input is a problem, either uncheck the ConsumeInput option on
	 * the input event, or create a custom node class and use input actions there.
	 */
	UPROPERTY(EditAnywhere, Replicated, Category = "State Machine Instance|Input")
	TEnumAsByte<ESMStateMachineInput::Type> AutoReceiveInput;
	
	/** The priority of this input component when pushed in to the stack. */
	UPROPERTY(EditAnywhere, Replicated, Category = "State Machine Instance|Input")
	int32 InputPriority;

	/** Whether any components lower on the input stack should be allowed to receive input. */
	UPROPERTY(EditAnywhere, Replicated, Category = "State Machine Instance|Input")
	uint8 bBlockInput: 1;
	///////////////////////
	/// End Input
	///////////////////////

	/** Ordered history of states, oldest to newest, not including active state(s). */
	UPROPERTY(VisibleInstanceOnly, Category = "State Machine Instance|History")
	TArray<FSMStateHistory> StateHistory;

	/** The total number of states to keep in history. Set to -1 for no limit. */
	UPROPERTY(EditAnywhere, Category = "State Machine Instance|History", meta = (ClampMin = "-1"))
	int32 StateHistoryMaxCount = 20;
	
	/** Enable info logging for the state machine. */
	UPROPERTY(EditDefaultsOnly, Category = "State Machine Instance|Logging")
	uint16 bEnableLogging: 1;

	/** Log when a state change occurs. This includes when a state machine starts or exits where transitions won't apply. */
	UPROPERTY(EditDefaultsOnly, Category = "State Machine Instance|Logging", meta = (EditCondition = "bEnableLogging"))
	uint16 bLogStateChange: 1;

	/** Log whenever a transition occurs. */
	UPROPERTY(EditDefaultsOnly, Category = "State Machine Instance|Logging", meta = (EditCondition = "bEnableLogging"))
	uint16 bLogTransitionTaken: 1;

	/**
	 * If this specific instance should be replicated if it is referenced by another state machine. Requires a component
	 * and proper network setup. This allows RPCs and replicated variables defined on this blueprint to work and is not
	 * required just for the state machine to function on the network.
	 *
	 * The primary instance will always be replicated when the component replicates.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "State Machine Instance|Replication")
	uint16 bCanReplicateAsReference: 1;
	
	/** The Update method will call Tick only if Update was not called by native Tick. */
	uint16 bCallTickOnManualUpdate: 1;

	/** True if currently ticking. */
	uint16 bIsTicking: 1;

	/** True if currently updating. */
	uint16 bIsUpdating: 1;

	/** Should this instance be allowed to process transitions. */
	uint16 bCanEvaluateTransitionsLocally: 1;

	/** Should this instance take transitions or only notify the server. */
	uint16 bCanTakeTransitionsLocally: 1;

	/** Should this instance be allowed to execute state logic. */
	uint16 bCanExecuteStateLogic: 1;

	/** True once the instance has started. */
	uint16 bHasStarted: 1;

	/** This will be true if at least one initial state was set from user load and will always be set to false on Stop(). */
	uint16 bLoadFromStatesCalled: 1;

	/** Signal the state machine has been initialized. Normally set automatically when calling Initialize(). */
	uint16 bInitialized: 1;

	/**
	 * If the state machine is waiting for a stop command. Only used for networking. Needs to be under instance for
	 * more precise control,
	 * similar to how individual transitions know when they need to wait.
	 */
	uint16 bWaitingForStop: 1;

	/**
	 * If the state machine instance is currently stopping.
	 */
	uint16 bIsStopping: 1;
	
private:
	/** True only during async initialization. */
	uint16 bInitializingAsync: 1;

	/**
	 * A map of PathGuids which should be redirected to other PathGuids. A PathGuid is the guid generated at run-time during initialization
	 * which is unique per node based on the node's path in the state machine. The generated PathGuid is deterministic and has support for
	 * references and parent graphs.
	 *
	 * Guid redirects aren't necessary unless you modify a node's path in the state machine and have to support loading data from a previous version
	 * which may have used the old guids.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintGetter = GetGuidRedirectMap, BlueprintSetter = SetGuidRedirectMap, Category = "State Machine Instance|Versioning", meta = (DisplayName = "Guid Redirect Map"))
	TMap<FGuid, FGuid> PathGuidRedirectMap;

public:
	/** Retrieve the guid cache of all root FSMs and their contained nodes. */
	const TMap<FGuid, FSMGuidMap>& GetRootPathGuidCache() const;

	/** Update the guid cache. */
	void SetRootPathGuidCache(const TMap<FGuid, FSMGuidMap>& InGuidCache);

	/**
	 * Retrieve the cached property data which is only needed during initialization.
	 * If called after initialization the cache may be recalculated.
	 */
	TSharedPtr<FSMCachedPropertyData, ESPMode::ThreadSafe> GetCachedPropertyData();

private:
	/** Allocate and populate cached property data. */
	void CreateCachedPropertyData();
	
private:
	/**
	 * A flattened map of Root FSM (this instance plus references) path guids each containing all of their immediate node to path guids.
	 */
	UPROPERTY()
	TMap<FGuid, FSMGuidMap> RootPathGuidCache;

	/** Cached property data for this state machine instance. Once mapped it includes properties of this instance and all contained node classes. */
	TSharedPtr<FSMCachedPropertyData, ESPMode::ThreadSafe> CachedPropertyData;

public:
	/*
	 * Archetype objects used for instantiating references. Only valid from the CDO.
	 * DuplicateTransient is used to prevent loading defaults in non nativized packages.
	 * Cannot set Transient or CDO won't be created properly in BP Nativization. Instead
	 * this value is manually cleaned on compile. 
	 */
	UPROPERTY(Instanced, DuplicateTransient, meta = (NoLogicDriverExport))
	TArray<TObjectPtr<UObject>> ReferenceTemplates;

public:
	/**
	 * NodeGuid -> ExposedNodeFunctions. This is mapped by NODE guid - not path guid - and should always be called
	 * from nodes directly owned by this instance. IE nodes contained in a reference sm won't be found unless called
	 * on the reference.
	 *
	 * Pointers to the contained exposed function array are set on each node instance using it. This is done to offset
	 * struct blueprint storage costs during compile, allowing larger state machines.
	 *
	 * This map is set by the compiler and should not be modified post initialize as there will
	 * be the potential for memory stomps on initialized nodes.
	 */
	TMap<FGuid, FSMExposedNodeFunctions>& GetNodeExposedFunctions() { return NodeExposedFunctions; }

	/**
	 * Signal a node isn't thread safe to prevent it from loading async.
	 */
	void AddNonThreadSafeNode(FSMNode_Base* InNode);

private:
	UPROPERTY(DuplicateTransient, meta = (BlueprintCompilerGeneratedDefaults))
	TMap<FGuid, FSMExposedNodeFunctions> NodeExposedFunctions;

	TArray<FSMNode_Base*> NonThreadSafeNodes;
	FCriticalSection CriticalSection;

#if WITH_EDITORONLY_DATA
	FSMDebugStateMachine DebugStateMachine;
#endif
};
