// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMInstance.h"
#include "SMTransactions.h"

#include "Components/ActorComponent.h"

#include "SMStateMachineComponent.generated.h"

class AActor;
class UActorChannel;

DECLARE_DELEGATE_OneParam(FOnStateMachineComponentInitializedAsync, USMStateMachineComponent* /* Component */);

UENUM(BlueprintType)
enum class ESMThreadMode : uint8
{
	/**
	 * Run single threaded blocking in the game thread.
	 */
	Blocking,
	
	/**
	 * Run asynchronous out of the game thread.
	 */
	Async
};

/**
 * Actor Component wrapper for a State Machine Instance. Supports Replication. Will default state machine context to the owning actor of this component.
 * Call Start() when ready.
 */
UCLASS(Blueprintable, ClassGroup = LogicDriver, meta = (BlueprintSpawnableComponent, DisplayName = "State Machine Component"))
class SMSYSTEM_API USMStateMachineComponent : public UActorComponent, public ISMStateMachineInterface, public ISMStateMachineNetworkedInterface
{
	GENERATED_UCLASS_BODY()

public:
	// UObject
	virtual void PostLoad() override;
	virtual void Serialize(FArchive& Ar) override;
#if WITH_EDITOR
	virtual void PostEditImport() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	// ~UObject
	
	// UActorComponent
	virtual bool ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void InitializeComponent() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	// ~ UActorComponent

	// ISMStateMachineInstance

	/**
	 * Prepare the state machine for use.
	 * 
	 * This cannot occur during automatic Component Activation when working with Listen servers and playing in the editor. The game
	 * will incorrectly report as Stand Alone.
	 *
	 * @param Context The context to use for the state machine. A null context will imply the owner of the component should be used.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Components")
	virtual void Initialize(UObject* Context = nullptr) override;

	/** Start the root state machine. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Components")
	virtual void Start() override;

	/**
	 * Manual way of updating the root state machine if tick is disabled. Not used by default and for custom update implementations.
	 * This will either call update locally if not replicated or call update on the server. For more control in a network environment
	 * calling Update from GetInstance() may be more appropriate.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Components")
	virtual void Update(float DeltaSeconds) override;

	/** This will complete the state machine's current state and force the machine to end regardless of if the state is an end state. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Components")
	virtual void Stop() override;
	
	/** Forcibly restart the state machine and place it back into an entry state. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Instances")
	virtual void Restart() override;
	
	/**
	 * Shutdown this instance. Calls Stop. Must call initialize again before use.
	 * If the goal is to restart the state machine later call Stop instead.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Components")
	virtual void Shutdown() override;
	
	// ~ISMStateMachineInstance

	/**
	 * Prepare the state machine for use on a separate thread.
	 * 
	 * @param Context The context to use for the state machine. A null context will imply the owner of the component should be used.
	 */
	void InitializeAsync(UObject* Context, const FOnStateMachineComponentInitializedAsync& OnCompletedDelegate = FOnStateMachineComponentInitializedAsync());

	/** Prepare the state machine for use on a separate thread. */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "Initialize Async", Latent, LatentInfo="LatentInfo"), Category="Logic Driver|State Machine Components")
	void K2_InitializeAsync(UObject* Context, FLatentActionInfo LatentInfo);

	/** If the state machine component has fully initialized. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Components")
	bool IsInitialized() const { return bInitialized; }

	/** Checks if the instance is initialized and active. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Components")
	bool IsStateMachineActive() const;
	
	/**
	 * Sets relevant settings from another state machine component, ideally used with or immediately after component creation.
	 * Does not copy state machine instance data.
	 *
	 * @param OtherComponent A state machine component for settings to be copied from. Accepts null.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Components")
	void CopySettingsFromOtherComponent(USMStateMachineComponent* OtherComponent);
	
	// ISMStateMachineNetworkedInstance

	/**
	 * Signal to the server to initialize. Can be called by either the server or owning client that has state change authority.
	 * Only the server will perform initialization directly from this call. The client will always initialize post instance replication.
	 */
	virtual void ServerInitialize(UObject* Context) override;
	
	/** Signal to the server to start processing. Can be called by either the server or owning client that has state change authority. */
	virtual void ServerStart() override;

	/** Signal to the server to stop processing. Can be called by either the server or owning client that has state change authority. */
	virtual void ServerStop() override;

	/** Signal to the server to shutdown. Can be called by either the server or owning client that has state change authority. */
	virtual void ServerShutdown() override;

	/** Provide a transition for the server to take. */
	virtual void ServerTakeTransition(const FSMTransitionTransaction& TransitionTransaction) override;
	
	/**
	 * Signal to the server to activate a specific state. Can be called by either the server or owning client that has state change authority.
	 *
	 * @param StateGuid The guid of the specific state to activate.
	 * @param bActive Add or remove the state from the owning state machine's active states.
	 * @param bSetAllParents Sets the active state of all super state machines. A parent state machine won't be deactivated unless there are no other states active.
	 * @param bActivateNowLocally If the state is becoming active and this component has authority to activate locally it will run OnStateBegin this tick.
	 */
	virtual void ServerActivateState(const FGuid& StateGuid, bool bActive, bool bSetAllParents, bool bActivateNowLocally) override;

	/** Notify the server to update all clients with its current states. */
	virtual void ServerFullSync() override;

	/**
	 * Handle when a new connection is added, ensuring the connection receives the initial transaction if required.
	 * This is called automatically when using bAutomaticallyHandleNewConnections.
	 * 
	 * Requires the component registered, active, initialized.
	 *
	 * @param Channel The newly added actor channel.
	 * @param RepFlags The replication flags for the channel.
	 * @return If the channel was added.
	 */
	virtual bool HandleNewChannelOpen(UActorChannel* Channel, FReplicationFlags* RepFlags) override;

	/**
	 * Handle when a channel is closed. Called automatically with bAutomaticallyHandleNewConnections.
	 * Default behavior just removes the channel and all null channels from the set.
	 *
	 * @param Channel The channel to be removed. If null the connection set is cleaned of null channels.
	 */
	virtual void HandleChannelClosed(UActorChannel* Channel) override;
	
	/** If transition enter logic can currently execute. */
	virtual bool CanExecuteTransitionEnteredLogic() const override;

	/** If this connection has the authority to change states. */
	virtual bool HasAuthorityToChangeStates() const override;

	/** If the caller is both allowed to change states and can do so without server supervision. */
	virtual bool HasAuthorityToChangeStatesLocally() const override;
	
	/** If this connection has the authority to execute logic. */
	virtual bool HasAuthorityToExecuteLogic() const override;

	/** If this connection has authority to tick. Even if it does, ticking may be disabled by the user. */
	virtual bool HasAuthorityToTick() const override;
	
	/** Checks if this component is networked and replicated. */
	virtual bool IsConfiguredForNetworking() const override;
	
	/** If the this instance has authority (Such as an instance running on a server) */
	virtual bool HasAuthority() const override;

	/** If this is only a simulated instance. */
	virtual bool IsSimulatedProxy() const override;

	/** Sets bCanInstanceNetworkTick directly. Requires tick authority. */
	virtual void SetCanEverNetworkTick(bool bNewValue) override;
	
	// ~ISMStateMachineNetworkedInstance

	/** If this is a networked environment. */
	bool IsNetworked() const;

	/** If this belongs to a player controlled on this client. */
	bool IsLocallyOwned() const;

	/** If this is the client that owns this component. */
	bool IsOwningClient() const;

	/** If this is the authority for an owning client. */
	bool IsRemoteRoleOwningClient() const;

	/** Return the remote role of the owner. */
	ENetRole GetRemoteRole() const;
	
	/** If this is the listen server. */
	bool IsListenServer() const;
	
	/** Checks tick settings depending on if this is a networked environment or not. */
	bool CanTickForEnvironment() const;

	/** Retrieve the correct update frequency to use. */
	float GetServerUpdateFrequency() const;

	/** Retrieve the correct update frequency to use. */
	float GetClientUpdateFrequency() const;

	/**
	 * Special override to change instance tick settings when networked. Requires tick authority to make any changes.
	 * Calling USMInstance::SetCanEverTick() on the primary instance will update the network tick, which may make
	 * this call unnecessary.
	 *
	 * This call will likely be deprecated in the future and replaced with SetCanEverNetworkTick().
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Components")
	void SetCanInstanceNetworkTick(bool bCanEverTick);

	/** Find the highest level owning actor of this component. Useful if this component is used within a child actor component. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Components")
	AActor* GetTopMostParentActor() const;
	
	/** Retrieve the real state machine instance this component wraps. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|State Machine Components")
	USMInstance* GetInstance() const { return R_Instance; }

	/** Retrieve the archetype template the state machine instance is based on. Only valid for the CDO. */
	USMInstance* GetTemplateForInstance() const { return InstanceTemplate; }

	/**
	 * The context to use for initialization. Defaults to GetOwner().
	 * For native implementations overload GetContextForInitialization_Implementation.
	 * @return The context to use when initializing the state machine instance.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "State Machine Components")
	UObject* GetContextForInitialization() const;

	/** Create a string containing relevant information about this component. */
	FString GetInfoString() const;
	
	/** Called when the state machine is first initialized. */
	UPROPERTY(BlueprintAssignable, Category = "Logic Driver|State Machine Components")
	FOnStateMachineInitializedSignature OnStateMachineInitializedEvent;

	/** Called right before the state machine is started. */
	UPROPERTY(BlueprintAssignable, Category = "Logic Driver|State Machine Components")
	FOnStateMachineStartedSignature OnStateMachineStartedEvent;

	/** Called right before the state machine is updated. */
	UPROPERTY(BlueprintAssignable, Category = "Logic Driver|State Machine Components")
	FOnStateMachineUpdatedSignature OnStateMachineUpdatedEvent;
	
	/** Called right after the state machine has ended. */
	UPROPERTY(BlueprintAssignable, Category = "Logic Driver|State Machine Components")
	FOnStateMachineStoppedSignature OnStateMachineStoppedEvent;

	/** Called right after the state machine has shutdown. */
	UPROPERTY(BlueprintAssignable, Category = "Logic Driver|State Machine Instances")
	FOnStateMachineShutdownSignature OnStateMachineShutdownEvent;

	/** Called when a transition has evaluated to true and is being taken. */
	UPROPERTY(BlueprintAssignable, Category = "Logic Driver|State Machine Components")
	FOnStateMachineTransitionTakenSignature OnStateMachineTransitionTakenEvent;

	/** Called when a state machine has switched states. */
	UPROPERTY(BlueprintAssignable, Category = "Logic Driver|State Machine Components")
	FOnStateMachineStateChangedSignature OnStateMachineStateChangedEvent;

	/** Called when a state has started. This happens after OnStateMachineStateChanged and all previous transitions have evaluated. */
	UPROPERTY(BlueprintAssignable, Category = "Logic Driver|State Machine Components")
	FOnStateMachineStateStartedSignature OnStateMachineStateStartedEvent;
	
protected:
	/** Callback from when the instance initializes, used for async initialization. */
	UFUNCTION()
	void Internal_OnInstanceInitializedAsync(USMInstance* Instance);

	/** When the instance has initialized from replication. */
	UFUNCTION()
	void Internal_OnReplicatedInstanceInitialized(USMInstance* Instance);
	
	UFUNCTION()
	void Internal_OnStateMachineStarted(USMInstance* Instance);

	UFUNCTION()
	void Internal_OnStateMachineUpdated(USMInstance* Instance, float DeltaSeconds);
	
	UFUNCTION()
	void Internal_OnStateMachineStopped(USMInstance* Instance);

	UFUNCTION()
	void Internal_OnStateMachineShutdown(USMInstance* Instance);

	UFUNCTION()
	void Internal_OnStateMachineTransitionTaken(USMInstance* Instance, FSMTransitionInfo Transition);

	UFUNCTION()
	void Internal_OnStateMachineStateChanged(USMInstance* Instance, FSMStateInfo ToState, FSMStateInfo FromState);

	UFUNCTION()
	void Internal_OnStateMachineStateStarted(USMInstance* Instance, FSMStateInfo State);

private:
	/** Bind to SMInstance events so the component can fire its version of each event. */
	void BindToInstanceEvents();

protected:
	/** Called after the state machine has initialized either locally or by replication. */
	virtual void PostInitialize();

	/** Called after the state machine has initialized either locally or by replication. */
	UFUNCTION(BlueprintImplementableEvent, Category = "State Machine Components")
	void OnPostInitialize();
	
#if WITH_EDITOR
	/** Initialize the USMInstance template based on the current StateMachineClass. */
	void InitInstanceTemplate();
	/** Remove the current USMInstance template. */
	void DestroyInstanceTemplate();
	/** Tick overrides are deprecated in favor of modifying the USMInstance template. */
	void ImportDeprecatedProperties();
#endif

	USMInstance* CreateInstance(UObject* Context);
	virtual void DoInitialize(UObject* Context);

	// DoMethod names need to match their MULTICAST_Method or CLIENT_Method counterparts.
	// Provide empty overloads if necessary.
	
	virtual void DoStart();
	FORCEINLINE void DoStart(const FSMTransaction_Base& Transaction) { DoStart(); }
	
	virtual void DoUpdate(float DeltaTime);
	
	virtual void DoStop();
	FORCEINLINE void DoStop(const FSMTransaction_Base& Transaction) { DoStop(); }
	
	virtual void DoShutdown();
	FORCEINLINE void DoShutdown(const FSMTransaction_Base& Transaction) { DoShutdown();}

	/** Sync all states locally. */
	void DoFullSync(const FSMFullSyncTransaction& FullSyncTransaction);

	/** Perform local processing of transactions. */
	void DoTakeTransitions(const TArray<FSMTransitionTransaction>& InTransactions, bool bAsServer = false);

	/** Perform local processing of transactions. */
	void DoActivateStates(const TArray<FSMActivateStateTransaction>& StateTransactions);

protected:
	/** Configure instance specific network properties. */
	virtual void ConfigureInstanceNetworkSettings();

	/** Checks if this instance can skip server authored state changes. */
	bool IsClientAndShouldSkipMulticastStateChange() const;

	/** If the caller is the client and can process AND change states. */
	bool IsClientAndCanLocallyChangeStates() const;

	/** If the caller is the server and can change states. */
	bool IsServerAndCanLocallyChangeStates() const;
	
	/** Determine if transactions should be queued by the local client. */
	bool ShouldClientQueueTransaction() const;
	
	/** Resets pending transactions and sync requests. */
	void SetClientAsSynced();

	/** Sets the server as being up to date with the client. */
	void SetServerAsSynced();

	/** Checks if the owning client has connected to the server. */
	bool HasOwningClientConnected() const;

	/** Checks if the owning client has already connected and sets the internal flag. */
	void FindAndSetOwningClientConnection();

	/** If this instance should be waiting for the owning client to connect. */
	bool IsServerAndShouldWaitForOwningClient() const;

	/** If this instance should be waiting for the owning client to connect and provide its initial sync. */
	bool IsServerAndNeedsOwningClientSync() const;

	/** Checks if the server should be waiting before processing transactions. This does not check frequency. */
	bool IsServerAndNeedsToWaitToProcessTransactions() const;
	
	/** Create a full sync transaction from this instance. */
	bool PrepareFullSyncTransaction(FSMFullSyncTransaction& OutFullSyncTransaction) const;
	
	/**
	 * Clears all transactions matching the type from the input array.
	 *
	 * @param InOutTransactions Transactions to search and modify.
	 * @param bIgnoreUserAdded Do not clear full sync transactions triggered from user loading states.
	 */
	void ClearFullSyncTransactions(TArray<TSharedPtr<FSMTransaction_Base>>& InOutTransactions, bool bIgnoreUserAdded = true);

	/** Check for logic execution authority against specific domain. */
	bool HasAuthorityToExecuteLogicForDomain(ESMNetworkConfigurationType Configuration) const;

	/** If multicast should be used, otherwise client calls will be used. */
	FORCEINLINE bool ShouldMulticast() const { return bIncludeSimulatedProxies || bHasServerRemoteRoleJustChanged || bAlwaysMulticast; }
	
	/** Update transaction array to be replicated to clients. */
	void Server_PrepareTransitionTransactionsForClients(const TArray<FSMTransitionTransaction>& InTransactions);

	/** Update transaction array to be replicated to clients. */
	void Server_PrepareStateTransactionsForClients(const TArray<FSMActivateStateTransaction>& InTransactions);

	/**
	 * Server: Iterate through the outgoing queue sending all transactions to the correct proxies.
	 * Client: Iterate through the pending queue running on methods locally. The client will be assumed to be in sync after.
	 *
	 * @param InOutTransactions Transactions in will be processed and the array cleared.
	 */
	void ClientServer_ProcessAllTransactions(TArray<TSharedPtr<FSMTransaction_Base>>& InOutTransactions);

	/** Special handling when the client is executing queued transactions the server should execute. */
	void Client_SendOutgoingTransactions();

	/** If the owning client is responsible for sending the initial sync. */
	bool Client_DoesClientNeedToSendInitialSync() const;

	/** Call from the owning client that is state change authoritative. Return false if failed. */
	bool Client_SendInitialSync();
	
	///////////////////////
	/// Server
	///////////////////////
	/** Signal the server to initialize state machine. */
	UFUNCTION(Server, Reliable)
	void SERVER_Initialize(const FSMInitializeTransaction& Transaction);

	/** Signal the server to start the state machine. */
	UFUNCTION(Server, Reliable)
	void SERVER_Start(const FSMTransaction_Base& Transaction);

	/** Update the server state machine. */
	UFUNCTION(Server, Reliable)
	void SERVER_Update(float DeltaTime);

	/** Signal the server to end the state machine.*/
	UFUNCTION(Server, Reliable)
	void SERVER_Stop(const FSMTransaction_Base& Transaction);

	/** Signal the server to shutdown the state machine.*/
	UFUNCTION(Server, Reliable)
	void SERVER_Shutdown(const FSMTransaction_Base& Transaction);

	/** Signal the server of transition transactions. */
	UFUNCTION(Server, Reliable)
	void SERVER_TakeTransitions(const TArray<FSMTransitionTransaction>& TransitionTransactions);

	/** Signal the server to activate a specific state. */
	UFUNCTION(Server, Reliable)
	void SERVER_ActivateStates(const TArray<FSMActivateStateTransaction>& StateTransactions);

	/**
	 * Signal the server that it should force all clients to sync.
	 * @param bForceFullRefresh Signal that the refresh should always be accepted and the local network settings should be refreshed.
	 */
	UFUNCTION(Server, Reliable)
	void SERVER_RequestFullSync(bool bForceFullRefresh = false);

	/** Signal to the server that it should accept the current state of the owning client. */
	UFUNCTION(Server, Reliable)
	void SERVER_FullSync(const FSMFullSyncTransaction& FullSyncTransaction);
	
	/** When the StateMachineInstance is loaded from the server. */
	UFUNCTION()
	virtual void REP_OnInstanceLoaded();
	
	template<typename T>
	void QueueOutgoingTransactions(const TArray<T>& InTransactions)
	{
		if (IsConfiguredForNetworking())
		{
			OutgoingTransactions.Reserve(OutgoingTransactions.Num() + InTransactions.Num());
			for (const T& Transaction : InTransactions)
			{
				TSharedPtr<T> SharedPtr = MakeShared<T>(Transaction);
				// Use either manual overrides or global settings set from macros.
				SharedPtr->bRanLocally = SharedPtr->bRanLocally || bJustExecutedRPCLocally;
				SharedPtr->bOriginatedFromServer = SharedPtr->bOriginatedFromServer || bServerJustPreparedRPC;
				SharedPtr->bOriginatedFromThisClient = bClientJustPreparedRPC;
				SharedPtr->ServerRemoteRoleAtQueueTime = GetRemoteRole();
				OutgoingTransactions.Add(MoveTemp(SharedPtr));
			}
		}
		else if (!bJustExecutedRPCLocally)
		{
			ClientServer_ProcessAllTransactions(OutgoingTransactions);
		}
	}

	template<typename T>
	void QueueOutgoingTransactions(const T& InTransaction)
	{
		QueueOutgoingTransactions<T>(TArray<T> {InTransaction});
	}
	
	template<typename T>
	bool QueueClientPendingTransactions(const TArray<T>& InTransactions)
	{
		if (ShouldClientQueueTransaction())
		{
			PendingTransactions.Reserve(PendingTransactions.Num() + InTransactions.Num());
			for (const T& Transaction : InTransactions)
			{
				PendingTransactions.Add(MakeShared<T>(Transaction));
			}
			bQueueClientTransactions = true;
			return true;
		}
		return false;
	}

	template<typename T>
	bool QueueClientPendingTransactions(const T& InTransaction)
	{
		return QueueClientPendingTransactions<T>(TArray<T> {InTransaction});
	}
	
private:
	/** Signal to all clients to start the state machine. */
	UFUNCTION(NetMulticast, Reliable)
	void MULTICAST_Start(const FSMTransaction_Base& Transaction);

	UFUNCTION(Client, Reliable)
	void CLIENT_Start(const FSMTransaction_Base& Transaction);
	
	/** Signal to all clients to stop the state machine. */
	UFUNCTION(NetMulticast, Reliable)
	void MULTICAST_Stop(const FSMTransaction_Base& Transaction);

	UFUNCTION(Client, Reliable)
	void CLIENT_Stop(const FSMTransaction_Base& Transaction);
	
	/** Signal to all clients to shutdown the state machine. */
	UFUNCTION(NetMulticast, Reliable)
	void MULTICAST_Shutdown(const FSMTransaction_Base& Transaction);

	UFUNCTION(Client, Reliable)
	void CLIENT_Shutdown(const FSMTransaction_Base& Transaction);
	
	/** Signal to all clients to process transitions. */
	UFUNCTION(NetMulticast, Reliable)
	void MULTICAST_TakeTransitions(const TArray<FSMTransitionTransaction>& Transactions);

	UFUNCTION(Client, Reliable)
	void CLIENT_TakeTransitions(const TArray<FSMTransitionTransaction>& Transactions);
	
	/** Signal to all clients to activate a specific state. */
	UFUNCTION(NetMulticast, Reliable)
	void MULTICAST_ActivateStates(const TArray<FSMActivateStateTransaction>& StateTransactions);

	UFUNCTION(Client, Reliable)
	void CLIENT_ActivateStates(const TArray<FSMActivateStateTransaction>& StateTransactions);
	
	/** Signal to all clients to force accept the current state of the server. */
	UFUNCTION(NetMulticast, Reliable)
	void MULTICAST_FullSync(const FSMFullSyncTransaction& FullSyncTransaction);

	UFUNCTION(Client, Reliable)
	void CLIENT_FullSync(const FSMFullSyncTransaction& FullSyncTransaction);
	
	/** Perform instance initialization after replication but wait (non-blocking) until after begin play is called. */
	void WaitOrProcessInstanceReplicatedBeforeBeginPlay();

	/** Call after full sync is complete and there are no more pending transactions. */
	void TryStartClientPostFullSync();

	UFUNCTION()
	void OnContextPawnControllerChanged(APawn* Pawn, AController* NewController);
	
private:
	friend struct FInitiateServerCall;
	friend struct FQueuedTransactionHelper;
	
	UPROPERTY(Transient)
	TSet<TObjectPtr<UActorChannel>> CurrentActorChannels;

	/** Transactions the server is preparing to send. */
	TArray<TSharedPtr<FSMTransaction_Base>> OutgoingTransactions;
	
	/** Transactions which could not be processed yet. */
	TArray<TSharedPtr<FSMTransaction_Base>> PendingTransactions;

	/** Time spent since last update. */
	float LastNetUpdateTime = 0.f;
	
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
public:
	/** Throttle time for messages that may have a high frequency. */
	float LogMessageThrottle = 2.f;
private:
	
	/** Time spent waiting for a full sync. */
	float ClientTimeNotInSync = 0.f;

	/** Time server spent waiting for a full sync. */
	float ServerTimeWaitingForClientSync = 0.f;
#endif

#if WITH_EDITORONLY_DATA
	// For debugging

	void SetNetworkDebuggingRoles();

	ENetRole NetworkRole;
	ENetRole RemoteRole;
#endif
	
public:
	/** True while ClientServer_ProcessAllTransactions is processing. */
	bool IsProcessingRPCs() const { return bProcessingRPCs; }
	
	/**
	 * New connections generally need to receive an initial sync transaction or they will
	 * not function. This is performed automatically by default.
	 * 
	 * When true, new connections are automatically determined through ReplicateSubObjects().
	 * When false, you must manually call HandleNewChannelOpen() and HandleChannelClosed().
	 */
	UPROPERTY()
	uint32 bAutomaticallyHandleNewConnections: 1;

private:
	/** True during RPC processing. */
	uint32 bProcessingRPCs: 1;
	
	/** If the server should skip transaction processing for this frame in scope. */
	uint32 bJustExecutedRPCLocally: 1;

	/** If the server is originating an RPC transaction for a frame in scope. */
	uint32 bServerJustPreparedRPC: 1;

	/** If the client is originating an RPC transaction for a frame in scope. */
	uint32 bClientJustPreparedRPC: 1;

	/** If this is a client that needs replicated states from the server. */
	uint32 bWaitingForServerSync: 1;

	/** The client is estimated to be in sync with the server. */
	uint32 bClientInSync: 1;

	/** The server is estimated to be in sync with the client. */
	uint32 bServerInSync: 1;

	/** At least one proxy requires a sync, but the server needs initial data from the owning client. */
	uint32 bProxiesWaitingForOwningSync: 1;

	/** If the client has queued a full sync transaction. */
	uint32 bClientHasPendingFullSyncTransaction: 1;
	
	/** The client is queuing transactions. */
	uint32 bQueueClientTransactions: 1;

	/** When the client is sending its outgoing transactions. */
	uint32 bClientSendingOutgoingTransactions: 1;

	/** If the owning client has connected to the server. */
	uint32 bOwningClientConnected: 1;

	/** User called shutdown before the owning connected and the server was waiting. */
	uint32 bCalledShutdownWhileWaitingForOwningClient: 1;
	
	/** Perform an initial (empty) full sync and then execute the remaining queue.  */
	uint32 bPerformInitialSyncBeforeQueue: 1;

	/** True only if auth client still needs to send an initial sync to the server. */
	uint32 bClientNeedsToSendInitialSync: 1;

	/** Special flag if a non-auth server was started with custom loaded states.
	 * This will tell the server to ignore the initial sync from the auth client. */
	uint32 bNonAuthServerHasInitialStates: 1;

	/** If the server has had its remote role suddenly changed. */
	uint32 bHasServerRemoteRoleJustChanged: 1;
	
	///////////////////////
	/// End Server
	///////////////////////

public:
	/** The state machine class to use for this instance. */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "State Machine Components", meta = (ExposeOnSpawn = true))
	TSubclassOf<USMInstance> StateMachineClass;
	
	/**
	 * Automatically initialize the state machine when the component begins play. This will set the Context to the owning actor of this component.
	 * This happens in two stages: On InitializeComponent the state machine is instantiated, on BeginPlay the state machine is initialized.
	 */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "State Machine Components", meta = (ExposeOnSpawn = true))
	uint8 bInitializeOnBeginPlay: 1;
	
	/** Automatically start the state machine when the component begins play. */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "State Machine Components", meta = (EditCondition = "bInitializeOnBeginPlay", ExposeOnSpawn = true))
	uint8 bStartOnBeginPlay: 1;

	/** Automatically stop the state machine when the component ends play. */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "State Machine Components", meta = (ExposeOnSpawn = true))
	uint8 bStopOnEndPlay: 1;
	
	/**
	 * Configure multi-threaded options to use with InitializeOnBeginPlay.
	 * Running async can reduce blocking operations on the game thread but increase total initialization time per component.
	 */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "State Machine Components", meta = (EditCondition = "bInitializeOnBeginPlay", ExposeOnSpawn = true))
	ESMThreadMode BeginPlayInitializationMode;

	/**
	 * If set to false when shutdown is called the internal reference to the state machine instance is cleared so it can be garbage collected.
	 * If set to true the instance will not be freed and will be re-used when initializing.
	 *
	 * When used with replication and re-initializing the component, this setting may be better set to false so the instance can run
	 * its first time OnRep initialize logic.
	 */
	UPROPERTY(Replicated, EditDefaultsOnly, AdvancedDisplay, Category = "State Machine Components")
	uint8 bReuseInstanceAfterShutdown: 1;	

private:
	bool GetLetInstanceManageTick_DEPRECATED() const
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return bLetInstanceManageTick;
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
public:
	/**
	 * The default behavior is to let the actor component tick the state machine when it ticks. This legacy option allows the instance to register as a tickable object instead.
	 * @deprecated This property is deprecated and will no longer be supported in future versions. Remove uses of this property and configure the component to manage tick.
	 */
	UE_DEPRECATED(5.1, "This property is deprecated and will no longer be supported in future versions. Remove uses of this property and configure the component to manage tick.")
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, AdvancedDisplay, Category = "State Machine Components", DisplayName = "Let Instance Manage Tick DEPRECATED",
		meta = (DeprecatedProperty, DeprecationMessage = "This property is deprecated and will no longer be supported in future versions. Remove uses of this property and configure the component to manage tick.",
			Tooltip = "This property is deprecated and will no longer be supported in future versions. Remove uses of this property and configure the component to manage tick.",
			EditCondition = "bLetInstanceManageTick", EditConditionHides))
	uint8 bLetInstanceManageTick: 1;
	
	/** 
	 * The authoritative domain to determine the state of the state machine. This impacts evaluating transitions, activating states, start, and stop.
	 * Requires Replication and a networked environment.
	 * 
	 * Client - The client is allowed to evaluate and send state changes to the server. The server will only process and broadcast changes from the client.
	 * Server - Only the server will determine state changes which it will then send to the client.
	 * ClientAndServer - This is unsupported and is completely up to your implementation to determine which state the state machine is in for all connections.
	 * 
	 * @warning Client authority on an actor without a player controller will not work. Remote calls to a server can only be made with a player controller.
	 */
	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadWrite, Category = ComponentReplication, meta = (EditCondition = "bReplicates", DisplayAfter = "bReplicates"))
	TEnumAsByte<ESMNetworkConfigurationType> StateChangeAuthority;

#if WITH_EDITORONLY_DATA
	/**
	 * @deprecated Use #StateChangeAuthority instead.
	 */
	UE_DEPRECATED(4.26, "Use `StateChangeAuthority` instead.")
	UPROPERTY()
	TEnumAsByte<ESMNetworkConfigurationType> NetworkTransitionConfiguration;
#endif
	
	/** Determine which domain to tick. The state machine must allow ticking for this to take effect. */
	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadWrite, Category = ComponentReplication, meta = (EditCondition = "bReplicates", DisplayAfter = "bReplicates"))
	TEnumAsByte<ESMNetworkConfigurationType> NetworkTickConfiguration;
	
	/** 
	 * The domain which primary state logic can be executed on. Requires Replication and a networked environment.
	 * Client - Only the client will execute state logic.
	 * Server - Only the server will execute state logic.
	 * ClientAndServer - Both the server and client will execute state logic.
	 *
	 * This impacts OnStateBegin, OnStateUpdate, and OnStateEnd.
	 * All other optional execution nodes will always execute across all domains.
	 */
	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadWrite, Category = ComponentReplication, meta = (EditCondition = "bReplicates", DisplayAfter = "bReplicates"))
	TEnumAsByte<ESMNetworkConfigurationType> NetworkStateExecution;

#if WITH_EDITORONLY_DATA
	/**
	 * @deprecated Use #NetworkStateExecution instead.
	 */
	UE_DEPRECATED(4.26, "Use `NetworkStateExecution` instead.")
	UPROPERTY()
	TEnumAsByte<ESMNetworkConfigurationType> NetworkStateConfiguration;
#endif
	
	/**
	 * Include simulated proxies when broadcasting changes and executing client logic. The default behavior only includes autonomous proxies
	 * such as actors possessed by a player controller. Client driven transitions will not work without a player controller.
	 */
	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadWrite, Category = ComponentReplication, meta = (EditCondition = "bReplicates", DisplayAfter = "bReplicates"))
	uint8 bIncludeSimulatedProxies: 1;
	
	/**
	 * Configure multi-threaded options to use when the instance is replicated to proxies.
	 * Running async can reduce blocking operations on the game thread but increase total initialization time per component.
	 */
	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadWrite, AdvancedDisplay, Category = ComponentReplication, meta = (EditCondition = "bReplicates"))
	ESMThreadMode ReplicatedInitializationMode;
	
	/**
	 * The domain to execute OnTransitionEntered logic.
	 * This fires for transitions when one is being taken to the next state.
	 *
	 * This setting respects bIncludeSimulatedProxies.
	 */
	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadWrite, AdvancedDisplay, Category = ComponentReplication, meta = (EditCondition = "bReplicates"))
	TEnumAsByte<ESMNetworkConfigurationType> NetworkTransitionEnteredConfiguration;
	
	/** 
	 * When true, if the client initiates a change it will only notify the server and not make the change until the server updates the client.
	 * When false, the client will make the change immediately.
	 *
	 * This is generally not needed unless you want to retrieve the time the server spent in a state during OnStateEnd.
	 */
	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadWrite, AdvancedDisplay, Category = ComponentReplication, meta = (EditCondition = "bReplicates && StateChangeAuthority != ESMNetworkConfigurationType::SM_Server"))
	uint8 bWaitForTransactionsFromServer: 1;

#if WITH_EDITORONLY_DATA
	/**
	 * @deprecated Use #bWaitForTransactionsFromServer instead.
	 */
	UE_DEPRECATED(4.26, "Use `bWaitForTransactionsFromServer` instead.")
	UPROPERTY()
	uint8 bTakeTransitionsFromServerOnly: 1;
#endif
	
	/**
	 * Attempt to automatically handle when the owning pawn is possessed or unpossessed by a player controller.
	 * This will force a full refresh using the current server states and update all connected clients' network settings.
	 *
	 * This works best when the authority is set to server.
	 *
	 * This may not work correctly when set to a client authority and going to or from a simulated proxy. It is not possible
	 * to have a client driven state machine owned by the server. In this case you may want to disable this option and handle
	 * OnPossess and OnUnPossess of the pawn, manually shutting down and initializing the state machine as desired.
	 */
	UPROPERTY(Replicated, EditDefaultsOnly, AdvancedDisplay, Category = ComponentReplication, meta = (EditCondition = "bReplicates"))
	uint8 bHandleControllerChange: 1;

	/**
	 * Calculate the server time spent in states when NetworkTickConfiguration is set to client only.
	 * This only impacts the client value of `GetServerTimeInState` and has no effect if the server
	 * is ticking.
	 *
	 * When true and the server is not ticking, it will take a measurement from the timestamp of when
	 * the state first started compared to when the server received the request to end the state.
	 * 
	 * If only using auto-bound events, or the state machine is being manually updated,
	 * this may not be necessary and disabling could increase accuracy.
	 */
	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadWrite, AdvancedDisplay, Category = ComponentReplication, meta = (EditCondition = "bReplicates"))
	uint8 bCalculateServerTimeForClients: 1;
	
	/** Uses the NetUpdateFrequency of the component owner. */
	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadWrite, AdvancedDisplay, Category = ComponentReplication, meta = (EditCondition = "bReplicates"))
	uint8 bUseOwnerNetUpdateFrequency: 1;

	/**
	 * The update rate (per second) to use for server RPC processing.
	 * 
	 * A lower frequency means less remote calls but larger average payload size. Total bandwidth over time may be less due to better transaction packing.
	 * A higher frequency means more frequent remote calls but smaller average payload size. Total bandwidth used may be more over time.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, AdvancedDisplay, Category = ComponentReplication, meta = (EditCondition = "bReplicates && !bUseOwnerNetUpdateFrequency"))
	float ServerNetUpdateFrequency;

	/**
	 * The update rate (per second) for the client to use if it is performing RPC processing.
	 * 
	 * A lower frequency means less remote calls but larger average payload size. Total bandwidth over time may be less due to better transaction packing.
	 * A higher frequency means more frequent remote calls but smaller average payload size. Total bandwidth used may be more over time.
	 */
	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadWrite, AdvancedDisplay, Category = ComponentReplication, meta = (EditCondition = "bReplicates && !bUseOwnerNetUpdateFrequency"))
	float ClientNetUpdateFrequency;

	/**
	 * Configure whether the server should always use mutlicast RPCs regardless of what bIncludeSimulatedProxies is set to.
	 * This can support the case where the state machine needs to be replicated to simulated proxies, but the proxies should not execute state logic.
	 */
	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadWrite, AdvancedDisplay, Category = ComponentReplication, meta = (EditCondition = "bReplicates && !bIncludeSimulatedProxies"))
	uint8 bAlwaysMulticast: 1;

private:
	/**
	 * Provide an existing component to copy certain settings from during dynamic component creation.
	 * This is only read during initialize component. Does not copy state machine instance data.
	 * Setting this will override all other settings when called through the blueprint node Add State Machine Component.
	 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "State Machine Components", meta = (ExposeOnSpawn = true, AllowPrivateAccess))
	TObjectPtr<USMStateMachineComponent> ComponentToCopy;
	
	uint8 bWaitingForInitialize: 1;
	uint8 bWaitingForStartOnBeginPlay: 1;
	uint8 bInitializeAsync: 1;
	
	/** Set from caller of initialize async function. */
	FOnStateMachineComponentInitializedAsync OnStateMachineInitializedAsyncDelegate;

protected:
	/** The actual state machine instance. */
	UPROPERTY(Transient, ReplicatedUsing=REP_OnInstanceLoaded)
	TObjectPtr<USMInstance> R_Instance;

	/** The template to use when initializing the state machine. Only valid within the CDO. */
	UPROPERTY(VisibleDefaultsOnly, Instanced, DuplicateTransient, Category = "State Machine Components", meta = (DisplayName=Template, DisplayThumbnail=false))
	TObjectPtr<USMInstance> InstanceTemplate;

	/** Set from the template and adjusted for the network configuration. */
	UPROPERTY(Transient)
	uint8 bCanInstanceNetworkTick: 1;

	/** If the component is initialized. */
	uint8 bInitialized: 1;
	
#if WITH_EDITORONLY_DATA
	/**
	 * If false the default setting will be used. When replicated this component may still perform some level of override depending on the NetworkTickConfiguration.
	 *
	 * @deprecated Use bCanEverTick on the instance template instead.
	 */
	UPROPERTY()
	uint8 bOverrideTick_DEPRECATED: 1;

	/**
	 * Allow the machine to tick. Overrides default State Machine blueprint configuration.
	 *
	 * @deprecated Use bCanEverTick on the instance template instead.
	 */
	UPROPERTY()
	uint8 bCanEverTick_DEPRECATED: 1;

	/**
	 * If false the default setting will be used.
	 *
	 * @deprecated Use TickInterval on the instance template instead.
	 */
	UPROPERTY()
	uint8 bOverrideTickInterval_DEPRECATED: 1;

	/**
	 * Time in seconds between native ticks. This mostly affects the "Update" rate of the state machine. Overloaded Ticks won't be affected.
	 * Overrides default state machine blueprint configuration.
	 *
	 * @deprecated Use TickInterval on the instance template instead.
	 */
	UPROPERTY()
	float TickInterval_DEPRECATED;
#endif
};
