// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMStateMachineComponent.h"

#include "SMLogging.h"
#include "SMTransition.h"
#include "SMUtils.h"

#include "Engine/ActorChannel.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "LatentActions.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/PropertyPortFlags.h"

#define LOCTEXT_NAMESPACE "SMStateMachineComponent"

/** When a multicast transaction is received either the server or client may choose to ignore it. */
#define RETURN_OR_EXECUTE_MULTICAST() \
	if (bJustExecutedRPCLocally || IsClientAndShouldSkipMulticastStateChange()) \
	{ \
		return; \
	} \

/** Check if the transaction was originated by the server and allow it, otherwise perform RETURN_OR_EXECUTE_MULTICAST. */
#define RETURN_OR_EXECUTE_MULTICAST_ALWAYS_ALLOW_IF_SERVER_AUTHORED(transaction) \
	if (HasAuthority() || !transaction.bOriginatedFromServer) \
	{ \
		if (HasAuthority() && transaction.bOriginatedFromServer && !HasAuthorityToChangeStates()) \
		{ \
			/* Server is not authoritative but generated a command to send to the client. */ \
			return; \
		} \
		RETURN_OR_EXECUTE_MULTICAST() \
	} \

/** Indicate if the server call made within this scope has been run locally already. */
#define PREPARE_SERVER_CALL(performed_locally) \
	FInitiateServerCall _InitiateServerCallHelper(this, performed_locally); \

/** Make a SERVER_call or queue the outgoing transaction to send to the server later. */
#define CALL_SERVER_OR_QUEUE_OUTGOING_CLIENT(call, transaction) \
	if (!IsConfiguredForNetworking()) \
	{ \
	} \
	else if (bClientJustPreparedRPC) \
	{ \
		QueueOutgoingTransactions(transaction); \
	} \
	else \
	{ \
		call(transaction); \
	} \

/** When a client receives a transaction it should either queue it for later or execute right away. */
#define RETURN_AND_QUEUE_OR_EXECUTE_CLIENT_TRANSACTION(transactions) \
	if (QueueClientPendingTransactions(transactions)) \
	{ \
		return; \
	} \

/** Executes either the multicast or client version of a call. Must be called from server. */
#define EXECUTE_MULTICAST_OR_CLIENT_FROM_SERVER(method_suffix, ...) \
	check(HasAuthority()); \
	if (ShouldMulticast()) \
	{ \
		/* Call on all clients and server. */ \
		MULTICAST_##method_suffix(__VA_ARGS__); \
	} \
	else \
	{ \
		/* Call on owning client only. */ \
		CLIENT_##method_suffix(__VA_ARGS__); \
		/* Run the MULTICAST_Implementation method so the server can execute its body locally if required. */ \
		MULTICAST_##method_suffix##_Implementation(__VA_ARGS__); \
	} \

/**
 * Either run the MULTICAST_ or CLIENT_ method if authority. SERVER_ if client and the client created the RPC, or the local method.
 * This should be called when processing the transaction queue.
 */
#define EXECUTE_QUEUED_TRANSACTION_MULTICAST_CLIENT_SERVER_OR_LOCAL(method_suffix, ...) \
	if (HasAuthority()) \
	{ \
		EXECUTE_MULTICAST_OR_CLIENT_FROM_SERVER(method_suffix, __VA_ARGS__); \
	} \
	else if (bClientSendingOutgoingTransactions) \
	{ \
		SERVER_##method_suffix(__VA_ARGS__); \
	} \
	else \
	{ \
		Do##method_suffix(__VA_ARGS__); \
	} \

struct FInitiateServerCall
{
	USMStateMachineComponent* Component;
	FInitiateServerCall(USMStateMachineComponent* InComponent, bool bPerformedLocally = false)
	{
		Component = InComponent;
		check(Component);
		Component->bJustExecutedRPCLocally = bPerformedLocally;
		Component->bServerJustPreparedRPC = Component->HasAuthority();
		Component->bClientJustPreparedRPC = Component->IsOwningClient();
	}
	~FInitiateServerCall()
	{
		check(Component);
		Component->bJustExecutedRPCLocally = false;
		Component->bServerJustPreparedRPC = false;
		Component->bClientJustPreparedRPC = false;
	}
};

class FSMInitializeComponentAsyncAction : public FPendingLatentAction
{
	/** The instance being initialized */
	TWeakObjectPtr<USMStateMachineComponent> Component;
	/** Function to execute on completion */
	FName ExecutionFunction;
	/** Link to fire on completion */
	int32 OutputLink;
	/** Object to call callback on upon completion */
	FWeakObjectPtr CallbackTarget;

public:
	FSMInitializeComponentAsyncAction(USMStateMachineComponent* InComponent, const FLatentActionInfo& LatentInfo) : Component(InComponent)
		, ExecutionFunction(LatentInfo.ExecutionFunction)
		, OutputLink(LatentInfo.Linkage)
		, CallbackTarget(LatentInfo.CallbackTarget)
	{
	}

	virtual void UpdateOperation(FLatentResponse& Response) override
	{
		const bool bFinished = Component.IsValid() && Component->IsInitialized() && Component->GetInstance() && !Component->GetInstance()->IsInitializingAsync();
		Response.FinishAndTriggerIf(bFinished, ExecutionFunction, OutputLink, CallbackTarget);
	}
};

#define DEFAULT_AUTHORITY SM_Client
#define DEFAULT_EXECUTION SM_ClientAndServer
#define DEFAULT_TICK	  SM_Client
#define DEFAULT_WAIT_RPC  false

USMStateMachineComponent::USMStateMachineComponent(FObjectInitializer const & ObjectInitializer)
{
	R_Instance = nullptr;
	bInitialized = false;
	bAutoActivate = true;
	bWantsInitializeComponent = true;
	bInitializeOnBeginPlay = true;
	bStartOnBeginPlay = false;
	bStopOnEndPlay = false;
	bReuseInstanceAfterShutdown = false;

	bWaitingForInitialize = false;
	bWaitingForStartOnBeginPlay = false;
	bInitializeAsync = false;
	BeginPlayInitializationMode = ESMThreadMode::Blocking;
	
	StateChangeAuthority = DEFAULT_AUTHORITY;
	NetworkStateExecution = DEFAULT_EXECUTION;
	NetworkTickConfiguration = DEFAULT_TICK;
	NetworkTransitionEnteredConfiguration = SM_ClientAndServer;
	ReplicatedInitializationMode = ESMThreadMode::Blocking;
	bWaitForTransactionsFromServer = DEFAULT_WAIT_RPC;
	bCalculateServerTimeForClients = true;
	bUseOwnerNetUpdateFrequency = true;
	ServerNetUpdateFrequency = 100.f;
	ClientNetUpdateFrequency = 100.f;
	bIncludeSimulatedProxies = false;
	bHandleControllerChange = true;
	bAlwaysMulticast = false;
	
	bProcessingRPCs = false;
	bAutomaticallyHandleNewConnections = true;
	bJustExecutedRPCLocally = false;
	bServerJustPreparedRPC = false;
	bClientJustPreparedRPC = false;
	bWaitingForServerSync = false;
	bClientInSync = false;
	bServerInSync = false;
	bProxiesWaitingForOwningSync = false;
	bClientHasPendingFullSyncTransaction = false;
	bQueueClientTransactions = false;
	bClientSendingOutgoingTransactions = false;
	bOwningClientConnected = false;
	bPerformInitialSyncBeforeQueue = false;
	bClientNeedsToSendInitialSync = false;
	bNonAuthServerHasInitialStates = false;
	bHasServerRemoteRoleJustChanged = false;
	
	PrimaryComponentTick.bCanEverTick = true;
	bCanInstanceNetworkTick = true;
	
	InstanceTemplate = nullptr;
	
	SetIsReplicatedByDefault(true);

#if WITH_EDITORONLY_DATA
	bOverrideTick_DEPRECATED = false;
	bOverrideTickInterval_DEPRECATED = false;
	bCanEverTick_DEPRECATED = true;
	TickInterval_DEPRECATED = 0.f;
	
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	bLetInstanceManageTick = false;
	
	NetworkTransitionConfiguration = DEFAULT_AUTHORITY;
	NetworkStateConfiguration = DEFAULT_EXECUTION;
	bTakeTransitionsFromServerOnly = DEFAULT_WAIT_RPC;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
}

void USMStateMachineComponent::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	ImportDeprecatedProperties();
#endif
}

void USMStateMachineComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	/*
	 * Duplicating components won't duplicate the instance properly and the components will still point to the old instance.
	 * This slightly modified snippet from ChildActorComponent.cpp fixes that. Overloading PostDuplicate object doesn't work properly with templates.
	 */
	
	if (Ar.HasAllPortFlags(PPF_DuplicateForPIE))
	{
		// Only templates need them serialized, otherwise they show up as selectable debug objects.
		if (IsTemplate())
		{
			Ar << InstanceTemplate;
		}
	}
	else if (Ar.HasAllPortFlags(PPF_Duplicate))
	{
		if (GIsEditor && Ar.IsLoading() && !IsTemplate())
		{
			// If we're not a template then we do not want the duplicate so serialize manually and destroy the template that was created for us
			Ar.Serialize(&InstanceTemplate, sizeof(UObject*));

			if (USMInstance* UnwantedDuplicate = static_cast<USMInstance*>(FindObjectWithOuter(this, USMInstance::StaticClass())))
			{
				UnwantedDuplicate->MarkAsGarbage();
			}
		}
		else if (!GIsEditor && !Ar.IsLoading() && !GIsDuplicatingClassForReinstancing)
		{
			// Avoid the archiver in the duplicate writer case because we want to avoid the duplicate being created
			Ar.Serialize(&InstanceTemplate, sizeof(UObject*));
		}
		else
		{
			// When we're loading outside of the editor we won't have created the duplicate, so its fine to just use the normal path
			// When we're loading a template then we want the duplicate, so it is fine to use normal archiver
			// When we're saving in the editor we'll create the duplicate, but on loading decide whether to take it or not
			Ar << InstanceTemplate;
		}

		/*
		 * This was responsible for performing our own duplication of actors' inherited components because DuplicateTransient was set.
		 * DuplicateTransient was necessary because of a linker load engine bug impacting 4.24 and 4.25.
		 * On 4.27 it was reported the StaticDuplicateObject method below caused crashes during packaging occasionally for win64 and PS5
		 * because GIsSavingPackage was true. It is unclear how this branch was called during packaging.
		 * 
		 * 4.26 fixes the original issue-- Confirmed for 4.26 through 5.0-EA. We can remove this workaround however we still
		 * need the DuplicateTransient tag. There is an edge case when a native component is inherited by a blueprint, and that
		 * blueprint has a child blueprint. The grandchild blueprint will appear to have the correct template, but it will be
		 * reset to the parent's template on an asset reload. DuplicateTransient prevents that. Testing has shown that this code block
		 * doesn't need to be hit even with DuplicateTransient. Duplication serialization should be taken care of above.
		 *
		 * Commenting it out for now since there have been a variety of issues around duplicating components with sub-objects.
		 *
		if (GIsEditor && Ar.IsSaving() && HasAnyFlags(RF_InheritableComponentTemplate))
		{
			// The component template is serializing for a child to use.
			if (!InstanceTemplate && StateMachineClass)
			{
				 // The template has DuplicateTransient set so it should be null at this point. We can find the right one from the archetype.
				 // DuplicateTransient is required to get around a bug involving deferred loading. When parent component's actor has a property
				 // modified and that actor's properties are referenced in the state machine the component uses, it will crash on editor load.
				if (USMInstance* ArchetypeTemplate = Cast<USMStateMachineComponent>(GetArchetype())->InstanceTemplate)
				{
					if (StateMachineClass == ArchetypeTemplate->GetClass())
					{
						InstanceTemplate = CastChecked<USMInstance>(StaticDuplicateObject(ArchetypeTemplate, this, NAME_None));
					}
				}
			}
		}*/
	}

#if WITH_EDITOR
	if (GIsEditor) // Necessary for new process PIE session.
	{
		if (!Ar.IsPersistent() && InstanceTemplate)
		{
			if (IsTemplate())
			{
				// InstanceTemplate should belong to components that are templates.
				if (InstanceTemplate->GetOuter() != this)
				{
					if (UObject* ExistingTemplate = StaticFindObject(nullptr, this, *InstanceTemplate->GetName()))
					{
						// Find an already existing template we should own... can happen if this is a child component whos class was recompiled.
						InstanceTemplate = CastChecked<USMInstance>(ExistingTemplate);
					}
					else
					{
						// Duplicate the instance. (Works when duplicate is clicked on the component, but not paste)
						InstanceTemplate = CastChecked<USMInstance>(StaticDuplicateObject(InstanceTemplate, this, NAME_None));
					}
				}
			}
			else
			{
				// Because the template may have fixed itself up, the tagged property delta serialized for 
				// the instance may point at a trashed template, so always repoint us to the archetypes template
				InstanceTemplate = CastChecked<USMStateMachineComponent>(GetArchetype())->InstanceTemplate;
			}
		}

		/**
		 * If a component doesn't have a template but is supposed to then try to find its default.
		 * This helps child components not inheriting their template when added to an actor.
		 */
		if (Ar.IsSaving() && IsTemplate(RF_ArchetypeObject) && !InstanceTemplate && StateMachineClass)
		{
			if (USMStateMachineComponent* Archetype = Cast<USMStateMachineComponent>(GetArchetype()))
			{
				if (StateMachineClass == Archetype->StateMachineClass)
				{
					if (USMInstance* Template = Archetype->InstanceTemplate)
					{
						InstanceTemplate = Cast<USMInstance>(StaticDuplicateObject(Template, this, NAME_None));
					}
				}
			}
		}
	}
	
#endif
}

#if WITH_EDITOR

void USMStateMachineComponent::PostEditImport()
{
	Super::PostEditImport();

	// Helps on paste operations.
	if (IsTemplate())
	{
		TArray<UObject*> Instances;
		GetObjectsWithOuter(this, Instances, false);

		for (UObject* Instance : Instances)
		{
			if (Instance->GetClass() == StateMachineClass)
			{
				InstanceTemplate = CastChecked<USMInstance>(Instance);
				break;
			}
		}
	}
	else
	{
		InstanceTemplate = CastChecked<USMStateMachineComponent>(GetArchetype())->InstanceTemplate;
	}
}

void USMStateMachineComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(USMStateMachineComponent, StateMachineClass))
	{
		if (IsTemplate())
		{
			InitInstanceTemplate();
		}
		else
		{
			InstanceTemplate = CastChecked<USMStateMachineComponent>(GetArchetype())->InstanceTemplate;
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif

bool USMStateMachineComponent::ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	bool bWroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);

	if (R_Instance)
	{
		bWroteSomething |= Channel->ReplicateSubobject(R_Instance, *Bunch, *RepFlags);

		for (const FSMReferenceContainer& ReferenceContainer : R_Instance->GetReplicatedReferences())
		{
			if (ReferenceContainer.Reference)
			{
				bWroteSomething |= Channel->ReplicateSubobject(ReferenceContainer.Reference, *Bunch, *RepFlags);
			}
		}
		
		if (bAutomaticallyHandleNewConnections)
		{
			if (HandleNewChannelOpen(Channel, RepFlags))
			{
				HandleChannelClosed(nullptr);
			}
		}
	}

	return bWroteSomething;
}

void USMStateMachineComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(USMStateMachineComponent, R_Instance);

	// These general properties need to be replicated in the event of dynamically creating components
	// from the server. Most of them should be COND_InitialOnly but that does not seem to be recognized
	// within components.
	DOREPLIFETIME(USMStateMachineComponent, StateMachineClass);
	DOREPLIFETIME(USMStateMachineComponent, NetworkTickConfiguration);
	DOREPLIFETIME(USMStateMachineComponent, StateChangeAuthority);
	DOREPLIFETIME(USMStateMachineComponent, NetworkStateExecution);
	DOREPLIFETIME(USMStateMachineComponent, bIncludeSimulatedProxies);
	DOREPLIFETIME(USMStateMachineComponent, bAlwaysMulticast);
	DOREPLIFETIME(USMStateMachineComponent, bWaitForTransactionsFromServer);
	DOREPLIFETIME(USMStateMachineComponent, ReplicatedInitializationMode);
	DOREPLIFETIME(USMStateMachineComponent, NetworkTransitionEnteredConfiguration);
	DOREPLIFETIME(USMStateMachineComponent, bHandleControllerChange);
	DOREPLIFETIME(USMStateMachineComponent, bCalculateServerTimeForClients);
	DOREPLIFETIME(USMStateMachineComponent, bUseOwnerNetUpdateFrequency);
	DOREPLIFETIME(USMStateMachineComponent, ClientNetUpdateFrequency);
	DOREPLIFETIME(USMStateMachineComponent, bInitializeOnBeginPlay);
	DOREPLIFETIME(USMStateMachineComponent, bStartOnBeginPlay);
	DOREPLIFETIME(USMStateMachineComponent, bStopOnEndPlay);
	DOREPLIFETIME(USMStateMachineComponent, BeginPlayInitializationMode);
	DOREPLIFETIME(USMStateMachineComponent, bReuseInstanceAfterShutdown);
}

void USMStateMachineComponent::InitializeComponent()
{
	Super::InitializeComponent();

	if (ComponentToCopy)
	{
		CopySettingsFromOtherComponent(ComponentToCopy);
	}

	if (HasAuthority())
	{
		if (bInitializeOnBeginPlay && !IsConfiguredForNetworking())
		{
			CreateInstance(GetContextForInitialization());
		}
	}
}

void USMStateMachineComponent::BeginPlay()
{
	if (bInitializeOnBeginPlay)
	{
		bInitializeAsync = BeginPlayInitializationMode == ESMThreadMode::Async;
		bWaitingForStartOnBeginPlay = bStartOnBeginPlay;
	
		if (HasAuthority())
		{
			ServerInitialize(GetContextForInitialization());
		}
	}

	// Blueprint BeginPlay is called here.
	Super::BeginPlay();
}

void USMStateMachineComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bStopOnEndPlay && HasAuthorityToChangeStates())
	{
		ServerStop();
	}
	
	Super::EndPlay(EndPlayReason);
	
	if (USMInstance* Instance = GetInstance())
	{
		if (Instance->IsInitializingAsync() && IsInGameThread())
		{
			Instance->CancelAsyncInitialization();
		}
	}
}

void USMStateMachineComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                             FActorComponentTickFunction* ThisTickFunction)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SMStateMachineComponent::Tick"), STAT_SMStateMachineComponent_Tick, STATGROUP_LogicDriver);
	if (R_Instance && CanTickForEnvironment())
	{
		R_Instance->Tick(DeltaTime);
	}

	// If R_Instance tick destroys the actor then we won't be registered.
	if (!IsRegistered())
	{
		return;
	}
	
	if (IsConfiguredForNetworking())
	{
		if (HasAuthority())
		{
			const float NetUpdateFrequency = GetServerUpdateFrequency();
			const float UpdateInterval = 1.f / (NetUpdateFrequency > 0.f ? NetUpdateFrequency : 0.1f); 
			LastNetUpdateTime += DeltaTime;
			if (LastNetUpdateTime >= UpdateInterval)
			{
				LastNetUpdateTime = 0.f;
				if (IsServerAndNeedsToWaitToProcessTransactions())
				{
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
					if (IsInitialized())
					{
						ServerTimeWaitingForClientSync += DeltaTime;
						if (ServerTimeWaitingForClientSync >= LogMessageThrottle)
						{
							LD_LOG_VERBOSE(TEXT("Server is waiting for owning client to connect before processing queued transactions. %s"), *GetInfoString())
							ServerTimeWaitingForClientSync = 0.f;
						}
					}
#endif
				}
				else
				{
					ClientServer_ProcessAllTransactions(OutgoingTransactions);
				}
			}
		}
		else
		{
			const float NetUpdateFrequency = GetClientUpdateFrequency();
			const float UpdateInterval = 1.f / (NetUpdateFrequency > 0.f ? NetUpdateFrequency : 0.1f); 
			LastNetUpdateTime += DeltaTime;
			if (LastNetUpdateTime >= UpdateInterval)
			{
				LastNetUpdateTime = 0.f;
				Client_SendOutgoingTransactions();
			}
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
			if (!bClientInSync && PendingTransactions.Num() > 0 &&
				((ShouldMulticast() || IsOwningClient()) && R_Instance && R_Instance->IsInitialized()))
			{
				ClientTimeNotInSync += DeltaTime;
				if (ClientTimeNotInSync >= LogMessageThrottle)
				{
					LD_LOG_WARNING(TEXT("Client %s has not received initial server sync and has %s pending transactions. %s."),
						*GetName(), *FString::FromInt(PendingTransactions.Num()),
						*GetInfoString());

					ClientTimeNotInSync = 0.f;
				}
			}
#endif
		}
	}
		
	// Blueprint Tick is called here.
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void USMStateMachineComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	DoShutdown();
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

bool USMStateMachineComponent::IsNetworked() const
{
	return GetNetMode() != NM_Standalone;
}

bool USMStateMachineComponent::IsLocallyOwned() const
{
	const APawn* Pawn = Cast<APawn>(GetTopMostParentActor());
	return Pawn ? Pawn->IsLocallyControlled() : false;
}

bool USMStateMachineComponent::IsOwningClient() const
{
	return !HasAuthority() && GetOwnerRole() == ROLE_AutonomousProxy;
}

bool USMStateMachineComponent::IsRemoteRoleOwningClient() const
{
	return HasAuthority() && GetRemoteRole() == ROLE_AutonomousProxy;
}

ENetRole USMStateMachineComponent::GetRemoteRole() const
{
	return GetOwner() ? GetOwner()->GetRemoteRole() : ROLE_None;
}

bool USMStateMachineComponent::IsListenServer() const
{
	return GetNetMode() == NM_ListenServer;
}

bool USMStateMachineComponent::CanTickForEnvironment() const
{
	if (R_Instance == nullptr)
	{
		return false;
	}

	if (IsConfiguredForNetworking())
	{
		return bCanInstanceNetworkTick;
	}

	return !GetLetInstanceManageTick_DEPRECATED() && R_Instance->IsTickable();
}

float USMStateMachineComponent::GetServerUpdateFrequency() const
{
	if (bUseOwnerNetUpdateFrequency)
	{
		const AActor* ActorOwner = GetOwner();
		return ActorOwner ? ActorOwner->NetUpdateFrequency : 0.f;
	}
	
	return ServerNetUpdateFrequency;
}

float USMStateMachineComponent::GetClientUpdateFrequency() const
{
	if (bUseOwnerNetUpdateFrequency)
	{
		const AActor* ActorOwner = GetOwner();
		return ActorOwner ? ActorOwner->NetUpdateFrequency : 0.f;
	}
	
	return ClientNetUpdateFrequency;
}

void USMStateMachineComponent::SetCanInstanceNetworkTick(bool bCanEverTick)
{
	if (HasAuthorityToTick())
	{
		bCanInstanceNetworkTick = bCanEverTick;

		// TODO: deprecate bLetInstanceManageTick
		// Once deprecated SetCanInstanceNetworkTick can be replaced with SetCanEverNetworkTick and that call
		// should be made BlueprintCallable.
		if (GetLetInstanceManageTick_DEPRECATED() && R_Instance)
		{
			R_Instance->SetCanEverTick(bCanInstanceNetworkTick);
		}
	}
}

AActor* USMStateMachineComponent::GetTopMostParentActor() const
{
	AActor* TopMostParentActor = GetOwner();
	for (AActor* ParentActor = TopMostParentActor; ParentActor; ParentActor = ParentActor->GetParentActor())
	{
		// Lookup the parent actor chain until no more actors are found.
		TopMostParentActor = ParentActor;
	}

	return TopMostParentActor;
}

FString USMStateMachineComponent::GetInfoString() const
{
	const FString RoleName = StaticEnum<ENetRole>()->GetValueAsString(GetOwnerRole());
	AActor* ActorOwner = GetTopMostParentActor();
	return FString::Printf(TEXT("\n  Role: %s, Name: %s, ActorOwner: %s, Instance: %s, Initialized: %d, HasAuthorityToChangeStates: %d, HasAuthorityToChangeStatesLocally: %d"),
		*RoleName, *GetName(), ActorOwner ? *ActorOwner->GetName() : TEXT("unknown"),
		R_Instance ? *R_Instance->GetName() : TEXT("Null"), IsInitialized(),
		HasAuthorityToChangeStates(), HasAuthorityToChangeStatesLocally());
}

UObject* USMStateMachineComponent::GetContextForInitialization_Implementation() const
{
	return GetOwner();
}

void USMStateMachineComponent::Internal_OnInstanceInitializedAsync(USMInstance* Instance)
{
	bWaitingForInitialize = false;
	PostInitialize();

	OnStateMachineInitializedAsyncDelegate.ExecuteIfBound(this);
}

void USMStateMachineComponent::Internal_OnReplicatedInstanceInitialized(USMInstance* Instance)
{
	bWaitingForInitialize = false;

	PostInitialize();

	if (Client_DoesClientNeedToSendInitialSync())
	{
		LD_LOG_VERBOSE(TEXT("Client sending initial sync post replication. %s."), *GetInfoString())
		Client_SendInitialSync();
	}
	
	if (!bClientInSync && !bClientHasPendingFullSyncTransaction)
	{
		bWaitingForServerSync = true;
	}
	else if (PendingTransactions.Num() > 0)
	{
		ClientServer_ProcessAllTransactions(PendingTransactions);
	}
}

void USMStateMachineComponent::Internal_OnStateMachineStarted(USMInstance* Instance)
{
	OnStateMachineStartedEvent.Broadcast(Instance);
}

void USMStateMachineComponent::Internal_OnStateMachineUpdated(USMInstance* Instance, float DeltaSeconds)
{
	OnStateMachineUpdatedEvent.Broadcast(Instance, DeltaSeconds);
}

void USMStateMachineComponent::Internal_OnStateMachineStopped(USMInstance* Instance)
{
	OnStateMachineStoppedEvent.Broadcast(Instance);
}

void USMStateMachineComponent::Internal_OnStateMachineShutdown(USMInstance* Instance)
{
	OnStateMachineShutdownEvent.Broadcast(Instance);
}

void USMStateMachineComponent::Internal_OnStateMachineTransitionTaken(USMInstance* Instance, FSMTransitionInfo Transition)
{
	OnStateMachineTransitionTakenEvent.Broadcast(Instance, Transition);
}

void USMStateMachineComponent::Internal_OnStateMachineStateChanged(USMInstance* Instance, FSMStateInfo ToState,
	FSMStateInfo FromState)
{
	OnStateMachineStateChangedEvent.Broadcast(Instance, ToState, FromState);
}

void USMStateMachineComponent::Internal_OnStateMachineStateStarted(USMInstance* Instance, FSMStateInfo State)
{
	OnStateMachineStateStartedEvent.Broadcast(Instance, State);
}

void USMStateMachineComponent::BindToInstanceEvents()
{
	if (R_Instance)
	{
		R_Instance->OnStateMachineStartedEvent.AddUniqueDynamic(this, &USMStateMachineComponent::Internal_OnStateMachineStarted);
		R_Instance->OnStateMachineUpdatedEvent.AddUniqueDynamic(this, &USMStateMachineComponent::Internal_OnStateMachineUpdated);
		R_Instance->OnStateMachineStoppedEvent.AddUniqueDynamic(this, &USMStateMachineComponent::Internal_OnStateMachineStopped);
		R_Instance->OnStateMachineShutdownEvent.AddUniqueDynamic(this, &USMStateMachineComponent::Internal_OnStateMachineShutdown);
		R_Instance->OnStateMachineTransitionTakenEvent.AddUniqueDynamic(this, &USMStateMachineComponent::Internal_OnStateMachineTransitionTaken);
		R_Instance->OnStateMachineStateChangedEvent.AddUniqueDynamic(this, &USMStateMachineComponent::Internal_OnStateMachineStateChanged);
		R_Instance->OnStateMachineStateStartedEvent.AddUniqueDynamic(this, &USMStateMachineComponent::Internal_OnStateMachineStateStarted);
	}
}

void USMStateMachineComponent::PostInitialize()
{
	if (!R_Instance)
	{
		return;
	}

	bCanInstanceNetworkTick = R_Instance->CanEverTick();
	R_Instance->SetRegisterTick(GetLetInstanceManageTick_DEPRECATED());

	if (bHandleControllerChange)
	{
		if (const UWorld* World = GetWorld())
		{
			// Perform on the next tick because this could be occuring from a possession already, such as a spawn.
			World->GetTimerManager().SetTimerForNextTick([WeakPtrThis = MakeWeakObjectPtr(this)]()
			{
				if (WeakPtrThis.IsValid() && IsValid(WeakPtrThis->GetWorld()))
				{
					if (UGameInstance* GameInstance = WeakPtrThis->GetWorld()->GetGameInstance())
					{
						GameInstance->GetOnPawnControllerChanged().AddUniqueDynamic(WeakPtrThis.Get(), &USMStateMachineComponent::OnContextPawnControllerChanged);
					}
				}
			});
		}
	}

	// Configure network settings after initialization.
	ConfigureInstanceNetworkSettings();

	bInitialized = true;
	
	// Allow child blueprint components to run specific initialize logic.
	OnPostInitialize();
	
	OnStateMachineInitializedEvent.Broadcast(R_Instance);

	if (bWaitingForStartOnBeginPlay)
	{
		bWaitingForStartOnBeginPlay = false;
		if (bStartOnBeginPlay && HasAuthorityToChangeStates())
		{
			ServerStart();
		}
	}
}

void USMStateMachineComponent::Initialize(UObject* Context)
{
#if WITH_EDITORONLY_DATA
	SetNetworkDebuggingRoles();
#endif
	ServerInitialize(Context);
}

void USMStateMachineComponent::Start()
{
	ServerStart();
}

void USMStateMachineComponent::Update(float DeltaSeconds)
{
	if (IsConfiguredForNetworking())
	{
		SERVER_Update(DeltaSeconds);
	}
	else
	{
		DoUpdate(DeltaSeconds);
	}
}

void USMStateMachineComponent::Stop()
{
	ServerStop();
}

void USMStateMachineComponent::Restart()
{
	ServerStop();
	ServerStart();
}

void USMStateMachineComponent::Shutdown()
{
	ServerShutdown();
}

void USMStateMachineComponent::InitializeAsync(UObject* Context, const FOnStateMachineComponentInitializedAsync& OnCompletedDelegate)
{
	OnStateMachineInitializedAsyncDelegate = OnCompletedDelegate;
	bInitializeAsync = true;
	Initialize(Context);
}

void USMStateMachineComponent::K2_InitializeAsync(UObject* Context, FLatentActionInfo LatentInfo)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(Context ? Context : GetContextForInitialization(), EGetWorldErrorMode::LogAndReturnNull))
	{
		FLatentActionManager& LatentActionManager = World->GetLatentActionManager();
		FSMInitializeComponentAsyncAction* Action = LatentActionManager.FindExistingAction<FSMInitializeComponentAsyncAction>(LatentInfo.CallbackTarget, LatentInfo.UUID);
		if (Action == nullptr)
		{
			Action = new FSMInitializeComponentAsyncAction(this, LatentInfo);
			LatentActionManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, Action);
		}
	}
	
	InitializeAsync(Context);
}

bool USMStateMachineComponent::IsStateMachineActive() const
{
	if (const USMInstance* Instance = GetInstance())
	{
		return Instance->IsActive();
	}
	
	return false;
}

void USMStateMachineComponent::CopySettingsFromOtherComponent(USMStateMachineComponent* OtherComponent)
{
	if (OtherComponent == nullptr)
	{
		return;
	}

	StateMachineClass = OtherComponent->StateMachineClass;
	bInitializeOnBeginPlay = OtherComponent->bInitializeOnBeginPlay;
	bStartOnBeginPlay = OtherComponent->bStartOnBeginPlay;
	bStopOnEndPlay = OtherComponent->bStopOnEndPlay;
	BeginPlayInitializationMode = OtherComponent->BeginPlayInitializationMode;
	bReuseInstanceAfterShutdown = OtherComponent->bReuseInstanceAfterShutdown;
	
	NetworkStateExecution = OtherComponent->NetworkStateExecution;
	StateChangeAuthority = OtherComponent->StateChangeAuthority;
	NetworkTickConfiguration = OtherComponent->NetworkTickConfiguration;
	bIncludeSimulatedProxies = OtherComponent->bIncludeSimulatedProxies;
	bHandleControllerChange = OtherComponent->bHandleControllerChange;
	bAlwaysMulticast = OtherComponent->bAlwaysMulticast;
	ReplicatedInitializationMode = OtherComponent->ReplicatedInitializationMode;
	NetworkTransitionEnteredConfiguration = OtherComponent->NetworkTransitionEnteredConfiguration;
	bWaitForTransactionsFromServer = OtherComponent->bWaitForTransactionsFromServer;
	bUseOwnerNetUpdateFrequency = OtherComponent->bUseOwnerNetUpdateFrequency;
	ServerNetUpdateFrequency = OtherComponent->ServerNetUpdateFrequency;
	ClientNetUpdateFrequency = OtherComponent->ClientNetUpdateFrequency;
}

void USMStateMachineComponent::ServerInitialize(UObject* Context)
{
	if (Context == nullptr)
	{
		Context = GetContextForInitialization();
	}

	const bool bHasAuth = HasAuthority();
	if (bHasAuth || !IsSimulatedProxy())
	{
		if (bCalledShutdownWhileWaitingForOwningClient && !HasOwningClientConnected())
		{
			LD_LOG_WARNING(TEXT("Calling ServerShutdown while the server is waiting for the owning client may result in\
\ndesync when calling ServerInitialize. To correct, wait to initialize until after the client has connected, or disable `bWaitForOwningClient`. %s."), *GetInfoString())
		
			bCalledShutdownWhileWaitingForOwningClient = false;
		}

		// Server must initialize and replicate to clients.
		const bool bRunLocal = HasAuthority();
		if (bRunLocal)
		{
			DoInitialize(Context);
		}
		else
		{
			PREPARE_SERVER_CALL(bRunLocal);
			CALL_SERVER_OR_QUEUE_OUTGOING_CLIENT(SERVER_Initialize, FSMInitializeTransaction(Context))
		}
	}
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	else
	{
		LD_LOG_WARNING(TEXT("Cannot call ServerInitialize from simulated proxy. %s."), *GetInfoString())
	}
#endif
}

void USMStateMachineComponent::ServerStart()
{
	const bool bHasAuth = HasAuthority();
	if (bHasAuth || !IsSimulatedProxy())
	{
		if (IsServerAndShouldWaitForOwningClient())
		{
			// Check if the client might have already connected. This could happen if Start was called after pawn possession.
			FindAndSetOwningClientConnection();
		}
		
		const bool bRunLocal = !IsConfiguredForNetworking() || (HasAuthorityToChangeStatesLocally() && !IsServerAndShouldWaitForOwningClient() &&
			!IsServerAndNeedsOwningClientSync());
		
		// Check for manually loaded states. This requires LoadFromStates called with bNotify from either
		// an auth client or the server.
		const bool bUserManuallyLoadedNewStates =
			(HasAuthorityToChangeStates() || HasAuthority()) && R_Instance && R_Instance->AreInitialStatesSetFromLoad();
		
		PREPARE_SERVER_CALL(bRunLocal);
		if (!bHasAuth && !bClientInSync)
		{
			// Auth client starting, send entire state to the server.
			bClientNeedsToSendInitialSync = !Client_SendInitialSync();
		}
		else if (IsConfiguredForNetworking() && bHasAuth && bRunLocal && !bUserManuallyLoadedNewStates)
		{
			SERVER_RequestFullSync();
		}

		// User has called LoadFromState
		if (bUserManuallyLoadedNewStates && IsConfiguredForNetworking())
		{
			if (bHasAuth && !HasAuthorityToChangeStates())
			{
				// Need to account for initial sync transaction an auth client will send.
				bNonAuthServerHasInitialStates = true;
			}
			
			FSMFullSyncTransaction FullSyncTransaction;
			ensure(PrepareFullSyncTransaction(FullSyncTransaction));
			FullSyncTransaction.bFromUserLoad = true;
			QueueOutgoingTransactions(FullSyncTransaction);
		}
		
		CALL_SERVER_OR_QUEUE_OUTGOING_CLIENT(SERVER_Start, FSMTransaction_Base(ESMTransactionType::SM_Start))
		
		if (bRunLocal)
		{
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
			if (!R_Instance && !HasAuthority() && IsConfiguredForNetworking())
			{
				LD_LOG_INFO(TEXT("Could not start instance locally from authoritative client. It will start when replicated. %s"), *GetInfoString())
			}
#endif
			DoStart();
		}
	}
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	else
	{
		LD_LOG_WARNING(TEXT("Cannot call ServerStart from simulated proxy. %s."), *GetInfoString())
	}
#endif
}

void USMStateMachineComponent::ServerStop()
{
	const bool bHasAuth = HasAuthority();
	if (bHasAuth || !IsSimulatedProxy())
	{
		const bool bRunLocal = !IsConfiguredForNetworking() || (HasAuthorityToChangeStatesLocally() && !IsServerAndShouldWaitForOwningClient() &&
			!IsServerAndNeedsOwningClientSync());
		
		PREPARE_SERVER_CALL(bRunLocal);
		CALL_SERVER_OR_QUEUE_OUTGOING_CLIENT(SERVER_Stop, FSMTransaction_Base(ESMTransactionType::SM_Stop))
		
		if (bRunLocal)
		{
			// Allow clients to stop immediately if they are completely authoritative.
			DoStop();
		}
	}
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	else
	{
		LD_LOG_WARNING(TEXT("Cannot call ServerStop from simulated proxy. %s."), *GetInfoString())
	}
#endif
}

void USMStateMachineComponent::ServerShutdown()
{
	if (IsBeingDestroyed() || HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed))
	{
		return;
	}

	const bool bHasAuth = HasAuthority();
	if (bHasAuth || !IsSimulatedProxy())
	{
		if (IsServerAndShouldWaitForOwningClient())
		{
			// Calling shutdown while waiting could be dangerous if is Initialize() is called again.
			bCalledShutdownWhileWaitingForOwningClient = true;
		}
		
		const bool bRunLocal = bHasAuth || HasAuthorityToChangeStatesLocally();
		
		PREPARE_SERVER_CALL(bRunLocal);
		CALL_SERVER_OR_QUEUE_OUTGOING_CLIENT(SERVER_Shutdown, FSMTransaction_Base(ESMTransactionType::SM_Shutdown))
		
		if (bRunLocal)
		{
			if (bHasAuth)
			{
				// Won't be processed through tick after shutdown.
				ClientServer_ProcessAllTransactions(OutgoingTransactions);
			}
			// Allow clients to shutdown immediately if they are completely authoritative.
			DoShutdown();
		}
	}
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	else
	{
		LD_LOG_WARNING(TEXT("Cannot call ServerShutdown from simulated proxy. %s."), *GetInfoString())
	}
#endif
}

void USMStateMachineComponent::ServerTakeTransition(const FSMTransitionTransaction& TransitionTransaction)
{
	if (HasAuthorityToChangeStates())
	{
		const bool bRunLocal = HasAuthorityToChangeStatesLocally();
		PREPARE_SERVER_CALL(bRunLocal);
		CALL_SERVER_OR_QUEUE_OUTGOING_CLIENT(SERVER_TakeTransitions, TArray<FSMTransitionTransaction>{TransitionTransaction});

	}
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	else if (!IsSimulatedProxy())
	{
		const FString AuthorityString = StaticEnum<ESMNetworkConfigurationType>()->GetValueAsString(StateChangeAuthority);
		LD_LOG_WARNING(TEXT("Caller of ServerTakeTransition does not have authority to change states. Expected authority: %s. %s."),
			*AuthorityString, *GetInfoString())
	}
#endif
}

void USMStateMachineComponent::ServerActivateState(const FGuid& StateGuid, bool bActive, bool bSetAllParents, bool bActivateNowLocally)
{
	if (!R_Instance)
	{
		return;
	}
	
	if (HasAuthorityToChangeStates())
	{
		if (const FSMState_Base* State = R_Instance->GetStateByGuid(StateGuid))
		{
			const bool bRunLocal = HasAuthorityToChangeStatesLocally();
			
			PREPARE_SERVER_CALL(bRunLocal);
			TArray<FSMActivateStateTransaction> Transactions
			{
				{
					State->GetGuid(),
					bActive ? 0.f : State->GetActiveTime(),
					bActive,
					bSetAllParents
				}
			};
			CALL_SERVER_OR_QUEUE_OUTGOING_CLIENT(SERVER_ActivateStates, MoveTemp(Transactions));

			if (bRunLocal)
			{
				// Allow clients to activate states if they are completely authoritative.
				R_Instance->ActivateStateLocally(StateGuid, bActive, bSetAllParents, bActivateNowLocally);
			}
		}
	}
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT     
	else if (!IsSimulatedProxy())
	{
		const FString AuthorityString = StaticEnum<ESMNetworkConfigurationType>()->GetValueAsString(StateChangeAuthority);
		LD_LOG_WARNING(TEXT("Caller of ServerActivateState does not have authority to change states. Expected authority: %s. %s."),
			*AuthorityString, *GetInfoString())
	}
#endif
}

void USMStateMachineComponent::ServerFullSync()
{
	SERVER_RequestFullSync();
}

bool USMStateMachineComponent::HandleNewChannelOpen(UActorChannel* Channel, FReplicationFlags* RepFlags)
{
	if (!bProcessingRPCs && IsInitialized() && IsActive() && IsRegistered() &&
		!CurrentActorChannels.Contains(Channel))
	{
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
		const FString RepType = RepFlags->bNetOwner ? TEXT("Owner") : RepFlags->bNetSimulated ? TEXT("Simulated") : TEXT("Other");
		LD_LOG_VERBOSE(TEXT("Client '%s' connecting... %s."), *RepType, *GetInfoString());
#endif
			
		CurrentActorChannels.Add(Channel);

		if (RepFlags->bNetOwner)
		{
			const bool bWasWaiting = IsServerAndShouldWaitForOwningClient();
			const bool bWasWaitingForSync = IsServerAndNeedsOwningClientSync();
			bOwningClientConnected = true;
			if (bWasWaiting)
			{
				bPerformInitialSyncBeforeQueue = !bWasWaitingForSync;
				LD_LOG_VERBOSE(TEXT("Owning client has connected and the server has resumed processing. %s."), *GetInfoString());
				return true;
			}
		}
			
		if (!HasAuthorityToChangeStates() && !bServerInSync)
		{
			LD_LOG_VERBOSE(TEXT("Cannot broadcast initial sync. Server is not state change authoritative and is waiting for the owning client. %s."), *GetInfoString());
			bProxiesWaitingForOwningSync = true;
		}
		else if (!IsServerAndShouldWaitForOwningClient() && !IsServerAndNeedsOwningClientSync() && !bPerformInitialSyncBeforeQueue)
		{
			ServerFullSync();
		}

		return true;
	}

	return false;
}

void USMStateMachineComponent::HandleChannelClosed(UActorChannel* Channel)
{
	CurrentActorChannels.Remove(Channel);

	// Multiple null keys present if multiple clients disconnected.
	// Sets allow duplicate keys by default and we can't BaseKeyFuncs::bInAllowDuplicateKeys for UPROPERTIES.
	while (Channel == nullptr && CurrentActorChannels.Contains(Channel))
	{
		CurrentActorChannels.Remove(Channel);
	}
}

bool USMStateMachineComponent::CanExecuteTransitionEnteredLogic() const
{
	return HasAuthorityToExecuteLogicForDomain(NetworkTransitionEnteredConfiguration);
}

bool USMStateMachineComponent::HasAuthorityToChangeStates() const
{
	if (!IsConfiguredForNetworking())
	{
		return true;
	}

	if (IsSimulatedProxy())
	{
		return false;
	}

	const bool bIsLocal = IsLocallyOwned();
	const bool bIsListenServer = IsListenServer();
	const bool bIsProxy = IsSimulatedProxy();
	const bool bHasAuth = !bIsProxy && HasAuthority();
	
	bool bAllow = !bIsProxy;
	if (bAllow)
	{
		if (StateChangeAuthority == SM_Server)
		{
			bAllow = bHasAuth || bIsListenServer;
		}
		else if (StateChangeAuthority == SM_Client)
		{
			bAllow = IsOwningClient() || bIsLocal;
		}
		else if (StateChangeAuthority == SM_ClientAndServer)
		{
			// Listen servers treat this as a proxy and authority, so we're going to disable transition access on the server in this case.
			// Helps with Replication Network Test. Both proxy and owner can progress state faster than intended.
			if (bIsListenServer && !bIsLocal)
			{
				bAllow = false;
			}
		}
	}

	return bAllow;
}

bool USMStateMachineComponent::HasAuthorityToChangeStatesLocally() const
{
	return !IsConfiguredForNetworking() || IsClientAndCanLocallyChangeStates() || IsServerAndCanLocallyChangeStates();
}

bool USMStateMachineComponent::HasAuthorityToExecuteLogic() const
{
	return HasAuthorityToExecuteLogicForDomain(NetworkStateExecution);
}

bool USMStateMachineComponent::HasAuthorityToTick() const
{
	if (!IsConfiguredForNetworking())
	{
		return true;
	}

	const bool bIsLocal = IsLocallyOwned();
	const bool bIsListenServer = IsListenServer();
	const bool bIsProxy = IsSimulatedProxy() && !bIncludeSimulatedProxies;
	const bool bHasAuth = !bIsProxy && HasAuthority();
	
	bool bAllow = !bIsProxy;
	if (bAllow)
	{
		if (NetworkTickConfiguration == SM_Server)
		{
			bAllow = bHasAuth || bIsListenServer;
		}
		else if (NetworkTickConfiguration == SM_Client)
		{
			bAllow = bIsLocal || (bIncludeSimulatedProxies && (!HasAuthority() || bIsListenServer));
		}
	}

	return bAllow;
}

bool USMStateMachineComponent::IsConfiguredForNetworking() const
{
	return IsNetworked() && GetIsReplicated();
}

bool USMStateMachineComponent::HasAuthority() const
{
	return !IsConfiguredForNetworking() || IsListenServer() || GetOwnerRole() == ROLE_Authority;
}

bool USMStateMachineComponent::IsSimulatedProxy() const
{
	return !IsLocallyOwned() && GetOwnerRole() == ROLE_SimulatedProxy;
}

void USMStateMachineComponent::SetCanEverNetworkTick(bool bNewValue)
{
	if (HasAuthorityToTick())
	{
		bCanInstanceNetworkTick = bNewValue;
	}
}

#if WITH_EDITOR

void USMStateMachineComponent::InitInstanceTemplate()
{
	if (IsTemplate())
	{
		Modify();

		if (StateMachineClass == nullptr)
		{
			DestroyInstanceTemplate();
			return;
		}

		const FName TemplateName = *FString::Printf(TEXT("SMCOMP_%s_%s_%s"), *GetName(), *StateMachineClass->GetName(), *FGuid::NewGuid().ToString());
		USMInstance* NewTemplate = NewObject<USMInstance>(this, StateMachineClass, TemplateName, RF_ArchetypeObject | RF_Transactional | RF_Public);

		if (InstanceTemplate)
		{
			InstanceTemplate->Modify();

			if (NewTemplate)
			{
				UEngine::CopyPropertiesForUnrelatedObjects(InstanceTemplate, NewTemplate);
			}

			DestroyInstanceTemplate();
		}

		InstanceTemplate = NewTemplate;
	}
	else
	{
		// Instanced archetypes won't save properly. Clearing it doesn't really matter though, so it gets caught later during initialization.
		if (InstanceTemplate && InstanceTemplate->GetClass() != StateMachineClass)
		{
			InstanceTemplate = nullptr;
		}
	}
}

void USMStateMachineComponent::DestroyInstanceTemplate()
{
	if (InstanceTemplate && IsTemplate())
	{
		InstanceTemplate->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
	}

	InstanceTemplate = nullptr;
}

void USMStateMachineComponent::ImportDeprecatedProperties()
{
	// Begin backwards compatible (1.x) state machine components.
	if (InstanceTemplate == nullptr && StateMachineClass != nullptr && IsTemplate())
	{
		InitInstanceTemplate();

		if (InstanceTemplate)
		{
			if (bOverrideTick_DEPRECATED)
			{
				InstanceTemplate->SetCanEverTick(bCanEverTick_DEPRECATED);
			}

			if (bOverrideTickInterval_DEPRECATED)
			{
				InstanceTemplate->SetTickInterval(TickInterval_DEPRECATED);
			}
		}
	}

	// Begin import of old net properties prior to 2.6.
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (NetworkTransitionConfiguration != DEFAULT_AUTHORITY)
	{
		StateChangeAuthority = NetworkTransitionConfiguration;
		NetworkTransitionConfiguration = DEFAULT_AUTHORITY;
	}

	if (NetworkStateConfiguration != DEFAULT_EXECUTION)
	{
		NetworkStateExecution = NetworkStateConfiguration;
		NetworkStateConfiguration = DEFAULT_EXECUTION;
	}

	if (bTakeTransitionsFromServerOnly != DEFAULT_WAIT_RPC)
	{
		bWaitForTransactionsFromServer = bTakeTransitionsFromServerOnly;
		bTakeTransitionsFromServerOnly = DEFAULT_WAIT_RPC;
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	if (GetLetInstanceManageTick_DEPRECATED())
	{
		LD_LOG_WARNING(TEXT("'bLetInstanceManageTick' on component '%s' with owner '%s' is set to true. This property is deprecated and will no longer be supported in future versions. Remove uses of this property and configure the component to manage tick."),
			*GetName(), GetOwner() ? *GetOwner()->GetName() : TEXT("Null"));
	}
}

#endif

USMInstance* USMStateMachineComponent::CreateInstance(UObject* Context)
{
	if (!StateMachineClass)
	{
		return nullptr;
	}

	if (Context == nullptr)
	{
		LD_LOG_ERROR(TEXT("No context provided to USMStateMachineComponent::CreateInstance."));
		return nullptr;
	}
	
	/*
	 * If the class was overridden in an instance of the owning BP then the template won't match.
	 * It's not possible to just edit the template in the instance when the parent of the template is 'this'.
	 * What happens is the template won't save to the correct archetype and instead just use the CDO.
	 * Setting the parent to the actor owner works, but as soon as the owner is compiled we lose the template.
	 * There isn't currently great support for this scenario in general as evidenced by ChildActorComponents.
	 */

	if (R_Instance == nullptr)
	{
		const USMStateMachineComponent* Archetype = IsTemplate() ? this : CastChecked<USMStateMachineComponent>(GetArchetype());
		USMInstance* Template = Archetype->InstanceTemplate ? Archetype->InstanceTemplate : nullptr;

		if (Template && Template->GetClass() == StateMachineClass)
		{
			R_Instance = NewObject<USMInstance>(Context, StateMachineClass, NAME_None, RF_NoFlags, Template);
		}
		else
		{
			R_Instance = NewObject<USMInstance>(Context, StateMachineClass, NAME_None, RF_NoFlags);
		}
	}

	check(R_Instance);

	R_Instance->SetComponentOwner(this);
	return R_Instance;
}

void USMStateMachineComponent::DoInitialize(UObject* Context)
{
	if (Context == nullptr)
	{
		Context = GetContextForInitialization();
	}

	if (HasAuthority() && HasAuthorityToChangeStates())
	{
		SetServerAsSynced();
	}
	
	if (!R_Instance)
	{
		bool bCanContinue = false;
		if (StateMachineClass && HasAuthority())
		{
			// This branch shouldn't be hit unless the user is manually initializing.
			// ensure(!bInitializeOnBeginPlay); -- Don't verify since this variable is public.
			bCanContinue = CreateInstance(Context) != nullptr;
		}
		
		if (!bCanContinue)
		{
			return;
		}
	}

	check(R_Instance);

	// reattach it to the context if it has changed owner for any reason.
	if (R_Instance->GetOuter() != Context)
	{
		R_Instance->Rename(*R_Instance->GetName(), Context, REN_DoNotDirty | REN_DontCreateRedirectors);
	}
	
	BindToInstanceEvents();

	if (bInitializeAsync)
	{
		bInitializeAsync = false;
		bWaitingForInitialize = true;
		R_Instance->InitializeAsync(Context, FOnStateMachineInstanceInitializedAsync::CreateUObject(this, &USMStateMachineComponent::Internal_OnInstanceInitializedAsync));
	}
	else
	{
		if (!R_Instance->IsInitialized())
		{
			R_Instance->Initialize(Context);
		}
		PostInitialize();
	}
}

void USMStateMachineComponent::DoStart()
{
	if (!R_Instance)
	{
		return;
	}

	R_Instance->Start();
}

void USMStateMachineComponent::DoUpdate(float DeltaTime)
{
	if (!R_Instance)
	{
		return;
	}

	R_Instance->Update(DeltaTime);
}

void USMStateMachineComponent::DoStop()
{
	if (!R_Instance)
	{
		return;
	}

	R_Instance->Stop();
}

void USMStateMachineComponent::DoShutdown()
{
	OnStateMachineInitializedAsyncDelegate.Unbind();
	CurrentActorChannels.Empty();
	
	bInitialized = false;
	bClientInSync = false;
	bServerInSync = false;
	bClientNeedsToSendInitialSync = false;
	bProxiesWaitingForOwningSync = true;

	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			GameInstance->GetOnPawnControllerChanged().RemoveAll(this);
		}
	}

	if (!R_Instance)
	{
		return;
	}

	R_Instance->Shutdown();

	if (!bReuseInstanceAfterShutdown)
	{
		R_Instance->OnStateMachineStartedEvent.RemoveAll(this);
		R_Instance->OnStateMachineUpdatedEvent.RemoveAll(this);
		R_Instance->OnStateMachineStoppedEvent.RemoveAll(this);
		R_Instance->OnStateMachineShutdownEvent.RemoveAll(this);
		R_Instance->OnStateMachineTransitionTakenEvent.RemoveAll(this);
		R_Instance->OnStateMachineStateChangedEvent.RemoveAll(this);
		R_Instance->OnStateMachineStateStartedEvent.RemoveAll(this);

		R_Instance = nullptr;
	}
}

void USMStateMachineComponent::DoFullSync(const FSMFullSyncTransaction& FullSyncTransaction)
{
	if (!R_Instance)
	{
		return;
	}

	if (FullSyncTransaction.bForceFullRefresh)
	{
		ConfigureInstanceNetworkSettings();
	}

	R_Instance->ClearLoadedStates();

	for (const FSMFullSyncStateTransaction& ReplicatedState : FullSyncTransaction.ActiveStates)
	{
		R_Instance->LoadFromState(ReplicatedState.BaseGuid, false, false);
		if (FSMState_Base* State = R_Instance->GetStateByGuid(ReplicatedState.BaseGuid))
		{
			State->SetServerTimeInState(ReplicatedState.TimeInState);
		}
	}

	if (!R_Instance->HasStarted() && FullSyncTransaction.bHasStarted)
	{
		if (FullSyncTransaction.ActiveStates.Num() > 0)
		{
			DoStart();
		}
		else
		{
			// No states means the state machine hasn't officially stopped yet.
			// Such as if all states were manually deactivated. We still need to be in sync
			// with the server start value.
			R_Instance->bHasStarted = true;
		}
	}
	else if (R_Instance->HasStarted() && !FullSyncTransaction.bHasStarted)
	{
		DoStop();
	}
	else if (FullSyncTransaction.bHasStarted)
	{
		// Already started, force correct states while running.
		R_Instance->GetRootStateMachine().SetFromTemporaryInitialStates();
	}
	
	if (HasAuthority() && bProxiesWaitingForOwningSync)
	{
		ServerFullSync();
	}
	SetServerAsSynced();
	SetClientAsSynced();
}

void USMStateMachineComponent::DoTakeTransitions(const TArray<FSMTransitionTransaction>& InTransactions, bool bAsServer)
{
	if (!R_Instance || !R_Instance->IsInitialized() || R_Instance->GetNodeMap().Num() == 0)
	{
		return;
	}

	const TMap<FGuid, FSMTransition*>& TransitionMap = R_Instance->GetTransitionMap();
	const TMap<FGuid, FSMState_Base*>& StateMap = R_Instance->GetStateMap();
	const TMap<FGuid, FSMNode_Base*>& NodeMap = R_Instance->GetNodeMap();

	FDateTime CurrentTime = FDateTime::UtcNow();
	
	for (const FSMTransitionTransaction& NetworkedTransaction : InTransactions)
	{
		if (NetworkedTransaction.bRanLocally)
		{
			// Not checked until now.
			continue;
		}
		
		if (bAsServer)
		{
			const_cast<FSMTransitionTransaction&>(NetworkedTransaction).bIsServer = true;
		}
		
		if (const FSMNode_Base* Node = NodeMap.FindRef(NetworkedTransaction.BaseGuid))
		{
			if (FSMStateMachine* OwningStateMachine = static_cast<FSMStateMachine*>(Node->GetOwnerNode()))
			{
				// Signal the FSM to take the transition.
				if (FSMTransition* Transition = TransitionMap.FindRef(NetworkedTransaction.BaseGuid))
				{
					// Source -> Destination are either the immediate from/to states which can be calculated,
					// or are at different parts in the transition chain when using conduits.
						
					FSMState_Base* SourceState = nullptr;
					FSMState_Base* DestinationState = nullptr;
					if (NetworkedTransaction.AreAdditionalGuidsSetupForTransitions())
					{
						const FGuid& SourceGuid = NetworkedTransaction.GetTransitionSourceGuid();
						const FGuid& DestinationGuid = NetworkedTransaction.GetTransitionDestinationGuid();

						{
							FSMState_Base* const* FoundState = StateMap.Find(SourceGuid);
							if (!ensure(FoundState))
							{
								LD_LOG_ERROR(TEXT("%s Critical failure. Source state is not found from transaction. State guid: %s. %s."),
								*NetworkedTransaction.Timestamp.ToString(), *SourceGuid.ToString(), *GetInfoString());
								continue;
							}
							SourceState = *FoundState;
						}

						{
							FSMState_Base* const* FoundState = StateMap.Find(DestinationGuid);
							if (!ensure(FoundState))
							{
								LD_LOG_ERROR(TEXT("%s Critical failure. Destination state is not found from transaction. State guid: %s. Source state: %s. %s."),
								*NetworkedTransaction.Timestamp.ToString(), *DestinationGuid.ToString(), *SourceState->GetNodeName(), *GetInfoString());
								continue;
							}
							DestinationState = *FoundState;
						}
					}
					else
					{
						SourceState = Transition->GetFromState();
						DestinationState = Transition->GetToState();
					}

					FSMState_Base* FromState = Transition->GetFromState();
					if (!FromState->IsActive())
					{
						if (OwningStateMachine->ContainsActiveState(FromState))
						{
							// Manual state activation may have happened during processing of this transition transaction and the state wasn't started.
							FromState->StartState();
						}
						else
						{
							const bool bHasValidRemoteRole = GetRemoteRole() != ROLE_None;
							if (Transition->bRunParallel && bHasValidRemoteRole)
							{
								// Parallel transitions' FromState may not be active if another parallel transition exited the state already.
							}
							else
							{
								LD_LOG_WARNING(TEXT("Possible Transition Desync: Previous state is not active. Previous State: %s. Next State: %s. Has valid remote role: %d.\n\
  Validate your state change authority is either client XOR server. If you have changed net roles while running or are manually switching states this error might be expected. %s."),
								*FromState->GetNodeName(), *Transition->GetToState()->GetNodeName(), bHasValidRemoteRole, *GetInfoString());
							}
						}
					}
						
					if (OwningStateMachine->ProcessTransition(Transition, SourceState, DestinationState,
						&NetworkedTransaction, 0.f, &CurrentTime))
					{
						OwningStateMachine->ProcessStates(0.f);
					}
				}
			}
		}
	}
}

void USMStateMachineComponent::DoActivateStates(
	const TArray<FSMActivateStateTransaction>& StateTransactions)
{
	if (!R_Instance)
	{
		return;
	}
	
	for (const FSMActivateStateTransaction& StateTransaction : StateTransactions)
	{
		if (StateTransaction.bRanLocally)
		{
			// Not checked until now.
			continue;
		}
		
		if (FSMState_Base* State = R_Instance->GetStateByGuid(StateTransaction.BaseGuid))
		{
			R_Instance->ActivateStateLocally(State->GetGuid(), StateTransaction.bIsActive, StateTransaction.bSetAllParents);
			State->SetServerTimeInState(StateTransaction.TimeInState);
		}
	}

	if (!CanTickForEnvironment() && R_Instance->HasPendingActiveStates())
	{
		// Needed so state becomes active properly, especially if the state is an FSM.
		DoUpdate(0.f);
	}
}

void USMStateMachineComponent::ConfigureInstanceNetworkSettings()
{
	if (R_Instance == nullptr || !IsConfiguredForNetworking())
	{
		return;
	}

	// The authority and environment determine the access this instance will have.
	const bool bIsProxy = IsSimulatedProxy() && !bIncludeSimulatedProxies;
	const bool bHasAuth = !bIsProxy && HasAuthority();

	// Tick Domain
	if (InstanceTemplate == nullptr || InstanceTemplate->CanEverTick())
	{
		bCanInstanceNetworkTick = HasAuthorityToTick();
		if (GetLetInstanceManageTick_DEPRECATED())
		{
			R_Instance->SetCanEverTick(bCanInstanceNetworkTick);
		}
	}

	// Transition Domain
	{
		if (!HasAuthorityToChangeStates())
		{
			R_Instance->SetAllowTransitionsLocally(false, !bWaitForTransactionsFromServer && !bIsProxy);
		}
		else if (bWaitForTransactionsFromServer)
		{
			// Client can evaluate transitions but won't take them.
			R_Instance->SetAllowTransitionsLocally(true, bHasAuth);
		}
		else
		{
			R_Instance->SetAllowTransitionsLocally(true, true);
		}
	}

	// State Domain
	{
		R_Instance->SetAllowStateLogic(HasAuthorityToExecuteLogic());
	}

	// Notify the instance that there is a server instance.
	R_Instance->SetNetworkInterface(this);

	// Refresh instance settings.
	R_Instance->UpdateNetworkConditions();
}

bool USMStateMachineComponent::IsClientAndShouldSkipMulticastStateChange() const
{
	return (IsConfiguredForNetworking() && IsOwningClient() && HasAuthorityToChangeStates()
		&& IsLocallyOwned() && !bWaitForTransactionsFromServer);
}

bool USMStateMachineComponent::IsClientAndCanLocallyChangeStates() const
{
	return IsOwningClient() && HasAuthorityToChangeStates() && !bWaitForTransactionsFromServer;
}

bool USMStateMachineComponent::IsServerAndCanLocallyChangeStates() const
{
	return HasAuthority() && GetOwnerRole() != ROLE_SimulatedProxy && HasAuthorityToChangeStates();
}

bool USMStateMachineComponent::ShouldClientQueueTransaction() const
{
	return !HasAuthority() && IsConfiguredForNetworking() && (R_Instance == nullptr || !IsInitialized() ||
		bWaitingForServerSync || bQueueClientTransactions);
}

bool USMStateMachineComponent::PrepareFullSyncTransaction(FSMFullSyncTransaction& OutFullSyncTransaction) const
{
	if (R_Instance)
	{
		FSMFullSyncTransaction FullSyncTransaction;
		TArray<FSMState_Base*> ActiveStates = R_Instance->HasStarted() ? R_Instance->GetAllActiveStates() :
		R_Instance->GetRootStateMachine().GetAllNestedInitialTemporaryStates();
		
		FullSyncTransaction.ActiveStates.Reserve(ActiveStates.Num());
		for (const FSMState_Base* ActiveState : ActiveStates)
		{
			FSMFullSyncStateTransaction ActiveStateTransaction(ActiveState->GetGuid(), ActiveState->GetActiveTime());
			FullSyncTransaction.ActiveStates.Add(MoveTemp(ActiveStateTransaction));
		}
		
		FullSyncTransaction.bHasStarted = R_Instance->HasStarted();
		FullSyncTransaction.bOriginatedFromServer = HasAuthority();
		OutFullSyncTransaction = MoveTemp(FullSyncTransaction);
		return true;
	}

	return false;
}

void USMStateMachineComponent::ClearFullSyncTransactions(TArray<TSharedPtr<FSMTransaction_Base>>& InOutTransactions, bool bIgnoreUserAdded)
{
	InOutTransactions.RemoveAll([bIgnoreUserAdded](const TSharedPtr<FSMTransaction_Base>& Transaction)
	{
		if (Transaction.IsValid() && Transaction->TransactionType == ESMTransactionType::SM_FullSync)
		{
			const TSharedPtr<FSMFullSyncTransaction> FullSyncTransaction = StaticCastSharedPtr<FSMFullSyncTransaction>(Transaction);
			return !FullSyncTransaction->bFromUserLoad || !bIgnoreUserAdded;
		}
		
		return false;
	});
}

bool USMStateMachineComponent::HasAuthorityToExecuteLogicForDomain(ESMNetworkConfigurationType Configuration) const
{
	if (!IsConfiguredForNetworking())
	{
		return true;
	}

	const bool bIsLocal = IsLocallyOwned();
	const bool bIsListenServer = IsListenServer();
	const bool bIsProxy = IsSimulatedProxy() && !bIncludeSimulatedProxies;
	const bool bHasAuth = !bIsProxy && HasAuthority();
	
	bool bAllow = !bIsProxy;
	if (bAllow)
	{
		if (Configuration == SM_Server)
		{
			bAllow = bHasAuth || bIsListenServer;
		}
		else if (Configuration == SM_Client)
		{
			bAllow = bIsLocal || (bIncludeSimulatedProxies && (!HasAuthority() || bIsListenServer));
		}
	}

	return bAllow;
}

void USMStateMachineComponent::SetClientAsSynced()
{
	if (HasAuthority())
	{
		return;
	}
	bWaitingForServerSync = false;
	bClientInSync = true;
	bQueueClientTransactions = false;
	bClientHasPendingFullSyncTransaction = false;
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	ClientTimeNotInSync = 0.f;
#endif
}

void USMStateMachineComponent::SetServerAsSynced()
{
	if (!HasAuthority())
	{
		return;
	}

	bServerInSync = true;
	bProxiesWaitingForOwningSync = false;
}

bool USMStateMachineComponent::HasOwningClientConnected() const
{
	return bOwningClientConnected;
}

void USMStateMachineComponent::FindAndSetOwningClientConnection()
{
	bOwningClientConnected = false;
	if (const AActor* PrimaryActorOwner = GetTopMostParentActor())
	{
		for (const UActorChannel* Channel : CurrentActorChannels)
		{
			if (Channel && Channel->Actor == PrimaryActorOwner &&
				Channel->Actor->GetRemoteRole() == ROLE_AutonomousProxy)
			{
				bOwningClientConnected = true;
				break;
			}
		}
	}
}

bool USMStateMachineComponent::IsServerAndShouldWaitForOwningClient() const
{
	return IsConfiguredForNetworking() && HasAuthority() && !HasOwningClientConnected() &&
		IsRemoteRoleOwningClient() && !IsListenServer();
}

bool USMStateMachineComponent::IsServerAndNeedsOwningClientSync() const
{
	return IsConfiguredForNetworking() && HasAuthority() && !HasAuthorityToChangeStates() && !bServerInSync;
}

bool USMStateMachineComponent::IsServerAndNeedsToWaitToProcessTransactions() const
{
	return IsServerAndShouldWaitForOwningClient() || IsServerAndNeedsOwningClientSync();
}

void USMStateMachineComponent::Server_PrepareTransitionTransactionsForClients(const TArray<FSMTransitionTransaction>& InTransactions)
{
	const FDateTime CurrentTime = FDateTime::UtcNow();

	// Record the current time. Const cast necessary -- SERVER_ call args must be const, but we want to record the time stamp for the server only.
	for (FSMTransitionTransaction& Transaction : const_cast<TArray<FSMTransitionTransaction>&>(InTransactions))
	{
		Transaction.Timestamp = CurrentTime;

		if (!R_Instance)
		{
			continue;
		}
		
		// Update transactions with current server times.
		FSMState_Base* SourceState = nullptr;
		if (Transaction.AreAdditionalGuidsSetupForTransitions())
		{
			SourceState = R_Instance->GetStateByGuid(Transaction.GetTransitionSourceGuid());
		}
		else if (FSMTransition* Transition = R_Instance->GetTransitionByGuid(Transaction.BaseGuid))
		{
			SourceState = Transition->GetFromState();
		}
		
		if (SourceState)
		{
			if (NetworkTickConfiguration == SM_Client && bCalculateServerTimeForClients)
			{
				// Attempt to calculate the time. This is likely slightly off from when using the Update/Tick method.
				FTimespan TimeDifference = CurrentTime - SourceState->GetStartTime();
				Transaction.ActiveTime = TimeDifference >= 0.f ? TimeDifference.GetTotalSeconds() : SM_ACTIVE_TIME_NOT_SET;
			}
			else
			{
				// If the server is ticking then the active time will be accurate.
				Transaction.ActiveTime = SourceState->GetActiveTime();
			}
		}
		else
		{
			LD_LOG_ERROR(TEXT("Server could not locate source state for transition %s."), *Transaction.BaseGuid.ToString());
		}
	}
}

void USMStateMachineComponent::Server_PrepareStateTransactionsForClients(
	const TArray<FSMActivateStateTransaction>& InTransactions)
{
	if (!R_Instance)
	{
		return;
	}
	
	// Record the current time. Const cast necessary -- SERVER_ call args must be const, but we want to record the time stamp for the server only.
	for (FSMActivateStateTransaction& Transaction : const_cast<TArray<FSMActivateStateTransaction>&>(InTransactions))
	{
		if (const FSMState_Base* State = R_Instance->GetStateByGuid(Transaction.BaseGuid))
		{
			Transaction.TimeInState = State->GetActiveTime();
		}
	}
}

void USMStateMachineComponent::ClientServer_ProcessAllTransactions(TArray<TSharedPtr<FSMTransaction_Base>>& InOutTransactions)
{
	struct FQueuedTransactionHelper
	{
		USMStateMachineComponent* Component;

		explicit FQueuedTransactionHelper(USMStateMachineComponent* InComponent)
		{
			Component = InComponent;
			check(Component);
			Component->bProcessingRPCs = true;
		}
		~FQueuedTransactionHelper()
		{
			check(Component);
			Component->bProcessingRPCs = false;
		}
	};
	
	FQueuedTransactionHelper TransactionHelper(this);
	
	TArray<FSMTransitionTransaction> TransitionTransactions;
	TArray<FSMActivateStateTransaction> StateTransactions;

	auto ProcessStates = [&]()
	{
		if (StateTransactions.Num())
		{
			if (HasAuthority())
			{
				Server_PrepareStateTransactionsForClients(StateTransactions);
			}
			EXECUTE_QUEUED_TRANSACTION_MULTICAST_CLIENT_SERVER_OR_LOCAL(ActivateStates, StateTransactions);
			StateTransactions.Reset();
		}
	};

	auto ProcessPendingTransitions = [&]()
	{
		if (TransitionTransactions.Num())
		{
			if (HasAuthority())
			{
				Server_PrepareTransitionTransactionsForClients(TransitionTransactions);
			}
			EXECUTE_QUEUED_TRANSACTION_MULTICAST_CLIENT_SERVER_OR_LOCAL(TakeTransitions, TransitionTransactions);
			TransitionTransactions.Reset();
		}
	};

	auto ProcessAllPendingTransactions = [&]()
	{
		ProcessStates();
		ProcessPendingTransitions();
	};

	// Disable before iteration or transactions may be added.
	bQueueClientTransactions = false;

	// If the client should call its post full sync routine.
	bool bClientPostFullSyncReady = false;

	if (HasAuthority() && bPerformInitialSyncBeforeQueue)
	{
		// Special instructions to perform a full sync prior to running the queue.
		// This assumes that there has been no activity on the client and servers
		// just to signal that the client can start accepting server transactions.
		
		bPerformInitialSyncBeforeQueue = false;
		
		FSMFullSyncTransaction FullSyncTransaction;
		FullSyncTransaction.bOriginatedFromServer = true;
		EXECUTE_MULTICAST_OR_CLIENT_FROM_SERVER(FullSync, MoveTemp(FullSyncTransaction));

		// Pending full syncs may be out of date with the new initial sync.
		ClearFullSyncTransactions(InOutTransactions);
	}

	const ENetRole CurrentRemoteRole = GetRemoteRole();

	// Iterate and build up transactions that can be sent together.
	// When a different type is detected send all built up transactions
	// to preserve RPC order.
	
	const int32 TransactionsStartNum = InOutTransactions.Num();
	for (auto TransactionIt = InOutTransactions.CreateIterator(); TransactionIt; ++TransactionIt)
	{
		// Don't use address of shared ptr, add reference to prevent possible memory stomp.
		const TSharedPtr<FSMTransaction_Base> Transaction = *TransactionIt;
		if (HasAuthority())
		{
			// The remote role can change if a controller Possessed or UnPossessed the owning pawn after this
			// transaction was queued, likely in the same frame. In this case we will simply always do multicasts to
			// ensure the correct connection receives the RPC.
			bHasServerRemoteRoleJustChanged = Transaction->ServerRemoteRoleAtQueueTime != CurrentRemoteRole;
		}
		
		switch (Transaction->TransactionType)
		{
		case ESMTransactionType::SM_Initialize:
			{
				ProcessAllPendingTransactions();
				const TSharedPtr<FSMInitializeTransaction> InitializePtr = StaticCastSharedPtr<FSMInitializeTransaction>(Transaction);

				// Initialize has special handling where only the server should be executing this transaction.
				// The client will always initialize upon initial instance replication.
				ensure(!Transaction->bRanLocally);
				if (HasAuthority())
				{
					DoInitialize(InitializePtr->Context);
				}
				else
				{
					SERVER_Initialize(*InitializePtr);
				}
				break;
			}
		case ESMTransactionType::SM_Start:
			{
				ProcessAllPendingTransactions();
				PREPARE_SERVER_CALL(Transaction->bRanLocally);
				EXECUTE_QUEUED_TRANSACTION_MULTICAST_CLIENT_SERVER_OR_LOCAL(Start, *Transaction);
				break;
			}
		case ESMTransactionType::SM_Stop:
			{
				ProcessAllPendingTransactions();
				PREPARE_SERVER_CALL(Transaction->bRanLocally);
				EXECUTE_QUEUED_TRANSACTION_MULTICAST_CLIENT_SERVER_OR_LOCAL(Stop, *Transaction);
				break;
			}
		case ESMTransactionType::SM_Shutdown:
			{
				ProcessAllPendingTransactions();
				PREPARE_SERVER_CALL(Transaction->bRanLocally);
				EXECUTE_QUEUED_TRANSACTION_MULTICAST_CLIENT_SERVER_OR_LOCAL(Shutdown, *Transaction);

				if (!HasAuthority())
				{
					// At this point we won't have an instance any more and can't process any other transactions.
					// It's possible there's still a queue, but the client should get back to it when the instance
					// re-initializes through replication.
					InOutTransactions.RemoveAt(0, TransactionIt.GetIndex() + 1);
					return;
				}
				
				break;
			}
		case ESMTransactionType::SM_Transition:
			{
				ProcessStates();
				TSharedPtr<FSMTransitionTransaction> TransitionPtr = StaticCastSharedPtr<FSMTransitionTransaction>(Transaction);
				TransitionTransactions.Add(*TransitionPtr);
				break;
			}
		case ESMTransactionType::SM_State:
			{
				ProcessPendingTransitions();
				TSharedPtr<FSMActivateStateTransaction> StatePtr = StaticCastSharedPtr<FSMActivateStateTransaction>(Transaction);
				StateTransactions.Add(*StatePtr);
				break;
			}
		case ESMTransactionType::SM_FullSync:
			{
				ProcessAllPendingTransactions();
				const TSharedPtr<FSMFullSyncTransaction> FullSyncPtr = StaticCastSharedPtr<FSMFullSyncTransaction>(Transaction);
				EXECUTE_QUEUED_TRANSACTION_MULTICAST_CLIENT_SERVER_OR_LOCAL(FullSync, *FullSyncPtr);
				bClientHasPendingFullSyncTransaction = false;
				bClientPostFullSyncReady = TransactionIt.GetIndex() == InOutTransactions.Num() - 1;
				break;
			}
		default:
			{
				ensureMsgf(false, TEXT("Unknown transaction type found for ClientServer_ProcessAllTransactions."));
				break;
			}
		}
	}

	ProcessAllPendingTransactions();

	// Don't empty, it's possible sending an RPC has detected a new client connection and may have added to pending transactions.
	// We guard against this through bProcessingRPCs, but as a precaution we don't empty to avoid clearing out transactions
	// that still need to be taken. Client could likely call Empty(), but keeping code branch consistent.
	{
		const int32 TransactionsEndNum = InOutTransactions.Num();
		check(TransactionsStartNum <= TransactionsEndNum);
		InOutTransactions.RemoveAt(0, TransactionsStartNum);

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
		if (TransactionsEndNum > TransactionsStartNum)
		{
			LD_LOG_WARNING(TEXT("ClientServer_ProcessAllTransactions has ended with more transactions than when it started.\
 If you are manually adding new connections you should wait until `IsProcessingRPCs()` is false. %s."),
				*GetInfoString())
		}
#endif
	}
	
	if (!HasAuthority() && bClientPostFullSyncReady && bClientInSync)
	{
		TryStartClientPostFullSync();
	}
}

void USMStateMachineComponent::Client_SendOutgoingTransactions()
{
	if (OutgoingTransactions.Num() > 0)
	{
		bClientSendingOutgoingTransactions = true;
		ClientServer_ProcessAllTransactions(OutgoingTransactions);
		bClientSendingOutgoingTransactions = false;
	}
}

bool USMStateMachineComponent::Client_DoesClientNeedToSendInitialSync() const
{
	return IsOwningClient() && !bClientInSync && (bClientNeedsToSendInitialSync || HasAuthorityToChangeStates());
}

bool USMStateMachineComponent::Client_SendInitialSync()
{
	if (PendingTransactions.Num() > 0)
	{
		LD_LOG_WARNING(TEXT("Client is sending initial sync, but there are pending transactions to process. The client may be out of sync. %s."), *GetInfoString())
	}
	
	FSMFullSyncTransaction FullSyncTransaction;
	if (PrepareFullSyncTransaction(FullSyncTransaction))
	{
		SetClientAsSynced();
		SERVER_FullSync(FullSyncTransaction);
		return true;
	}

	return false;
}

void USMStateMachineComponent::SERVER_Initialize_Implementation(const FSMInitializeTransaction& Transaction)
{
	QueueOutgoingTransactions(Transaction);
	if (IsServerAndNeedsToWaitToProcessTransactions())
	{
		// Tick update won't execute this call in this case.
		ClientServer_ProcessAllTransactions(OutgoingTransactions);
	}
}

void USMStateMachineComponent::SERVER_Start_Implementation(const FSMTransaction_Base& Transaction)
{
	QueueOutgoingTransactions(Transaction);
}

void USMStateMachineComponent::SERVER_Update_Implementation(float DeltaTime)
{
	DoUpdate(DeltaTime);
}

void USMStateMachineComponent::SERVER_Stop_Implementation(const FSMTransaction_Base& Transaction)
{
	QueueOutgoingTransactions(Transaction);
}

void USMStateMachineComponent::SERVER_Shutdown_Implementation(const FSMTransaction_Base& Transaction)
{
	QueueOutgoingTransactions(Transaction);
}

void USMStateMachineComponent::SERVER_TakeTransitions_Implementation(const TArray<FSMTransitionTransaction>& TransitionTransactions)
{
	QueueOutgoingTransactions(TransitionTransactions);
}

void USMStateMachineComponent::SERVER_ActivateStates_Implementation(const TArray<FSMActivateStateTransaction>& StateTransactions)
{
	QueueOutgoingTransactions(StateTransactions);
}

void USMStateMachineComponent::SERVER_RequestFullSync_Implementation(bool bForceFullRefresh)
{
	FSMFullSyncTransaction FullSyncTransaction;
	if (PrepareFullSyncTransaction(FullSyncTransaction))
	{
		FullSyncTransaction.bForceFullRefresh = bForceFullRefresh;
		QueueOutgoingTransactions(MoveTemp(FullSyncTransaction));
	}
}

void USMStateMachineComponent::SERVER_FullSync_Implementation(const FSMFullSyncTransaction& FullSyncTransaction)
{
	if ((!FullSyncTransaction.bOriginatedFromServer && IsServerAndNeedsOwningClientSync()) || FullSyncTransaction.bForceFullRefresh)
	{
		if (bNonAuthServerHasInitialStates)
		{
			SetServerAsSynced();
			bNonAuthServerHasInitialStates = false;
		}
		else
		{
			DoFullSync(FullSyncTransaction);
		}
	}
	else
	{
		QueueOutgoingTransactions(FullSyncTransaction);
	}
}

void USMStateMachineComponent::REP_OnInstanceLoaded()
{
#if WITH_EDITORONLY_DATA
	SetNetworkDebuggingRoles();
#endif

	if (R_Instance)
	{
		// The component owner used to be replicated, but if a component was dynamically created and
		// initialized in the same net batch the client component owner wouldn't be found.
		R_Instance->SetComponentOwner(this);
	}
	
	// Ideally this check would be under GetLifetimeReplicatedProps using
	// ShouldMultiCast() ? COND_None : COND_OwnerOnly,
	// but per channel property replication can't be configured dynamically.
	const bool bShouldProxyReplicate = ShouldMulticast() || IsOwningClient();
	if (R_Instance && bShouldProxyReplicate)
	{
		// Register tick won't have been replicated.
		R_Instance->SetRegisterTick(GetLetInstanceManageTick_DEPRECATED());

		if (R_Instance->HaveAllReferencesReplicated())
		{
			WaitOrProcessInstanceReplicatedBeforeBeginPlay();
		}
		else
		{
			R_Instance->OnReferencesReplicatedEvent.BindLambda([this]()
			{
				WaitOrProcessInstanceReplicatedBeforeBeginPlay();
			});
		}
	}
	else if (R_Instance == nullptr && IsInitialized())
	{
		LD_LOG_WARNING(TEXT("Shutting down state machine through R_Instance replication instead of RPC. This may happen if Shutdown() was called after the owner role has changed. %s"), *GetInfoString())
		DoShutdown();
	}
}

void USMStateMachineComponent::MULTICAST_Start_Implementation(const FSMTransaction_Base& Transaction)
{
	RETURN_OR_EXECUTE_MULTICAST_ALWAYS_ALLOW_IF_SERVER_AUTHORED(Transaction);
	RETURN_AND_QUEUE_OR_EXECUTE_CLIENT_TRANSACTION(Transaction)

	if (!HasAuthority() && HasAuthorityToChangeStates() && Transaction.bOriginatedFromServer)
	{
		// Non auth server sent the command. This means the server hasn't executed it yet.
		SERVER_Start(FSMTransaction_Base(ESMTransactionType::SM_Start));
	}
	
	DoStart();
}

void USMStateMachineComponent::CLIENT_Start_Implementation(const FSMTransaction_Base& Transaction)
{
	MULTICAST_Start_Implementation(Transaction);
}

void USMStateMachineComponent::MULTICAST_Stop_Implementation(const FSMTransaction_Base& Transaction)
{
	RETURN_OR_EXECUTE_MULTICAST_ALWAYS_ALLOW_IF_SERVER_AUTHORED(Transaction);
	RETURN_AND_QUEUE_OR_EXECUTE_CLIENT_TRANSACTION(Transaction)

	if (!HasAuthority() && HasAuthorityToChangeStates() && Transaction.bOriginatedFromServer)
	{
		// Non auth server sent the command. This means the server hasn't executed it yet.
		SERVER_Stop(FSMTransaction_Base(ESMTransactionType::SM_Stop));
	}
	
	DoStop();
}

void USMStateMachineComponent::CLIENT_Stop_Implementation(const FSMTransaction_Base& Transaction)
{
	MULTICAST_Stop_Implementation(Transaction);
}

void USMStateMachineComponent::MULTICAST_Shutdown_Implementation(const FSMTransaction_Base& Transaction)
{
	RETURN_OR_EXECUTE_MULTICAST_ALWAYS_ALLOW_IF_SERVER_AUTHORED(Transaction);

	// Execute pending now so they don't build up over a future instance replication.
	ClientServer_ProcessAllTransactions(PendingTransactions);
	DoShutdown();
}

void USMStateMachineComponent::CLIENT_Shutdown_Implementation(const FSMTransaction_Base& Transaction)
{
	MULTICAST_Shutdown_Implementation(Transaction);
}

void USMStateMachineComponent::MULTICAST_TakeTransitions_Implementation(
	const TArray<FSMTransitionTransaction>& Transactions)
{
	RETURN_OR_EXECUTE_MULTICAST();
	RETURN_AND_QUEUE_OR_EXECUTE_CLIENT_TRANSACTION(Transactions);

	// Always process on clients, server should only process if it hasn't done so already.
	DoTakeTransitions(Transactions, HasAuthority());
}

void USMStateMachineComponent::CLIENT_TakeTransitions_Implementation(
	const TArray<FSMTransitionTransaction>& Transactions)
{
	MULTICAST_TakeTransitions_Implementation(Transactions);
}

void USMStateMachineComponent::MULTICAST_ActivateStates_Implementation(const TArray<FSMActivateStateTransaction>& StateTransactions)
{
	RETURN_OR_EXECUTE_MULTICAST();
	RETURN_AND_QUEUE_OR_EXECUTE_CLIENT_TRANSACTION(StateTransactions);

	DoActivateStates(StateTransactions);
}

void USMStateMachineComponent::CLIENT_ActivateStates_Implementation(
	const TArray<FSMActivateStateTransaction>& StateTransactions)
{
	MULTICAST_ActivateStates_Implementation(StateTransactions);
}

void USMStateMachineComponent::MULTICAST_FullSync_Implementation(
	const FSMFullSyncTransaction& FullSyncTransaction)
{
	// Server version if authoritative client initiated.
	if (HasAuthority() && (!FullSyncTransaction.bOriginatedFromServer || FullSyncTransaction.bFromUserLoad ||
		FullSyncTransaction.bForceFullRefresh) &&
		!FullSyncTransaction.bRanLocally)
	{
		if (HasAuthorityToChangeStates() && !FullSyncTransaction.bFromUserLoad)
		{
			LD_LOG_WARNING(TEXT("Server received a full sync notice from an authoritative client, but the server is also configured as an authority. %s"),
				*GetInfoString())
		}
		else
		{
			LD_LOG_VERBOSE(TEXT("Server received and executed full sync. UserLoad: %d. %s."), FullSyncTransaction.bFromUserLoad, *GetInfoString());
			DoFullSync(FullSyncTransaction);
		}
		
		return;
	}

	const bool bForceRefresh = FullSyncTransaction.bForceFullRefresh;
	if (!bForceRefresh)
	{
		RETURN_OR_EXECUTE_MULTICAST_ALWAYS_ALLOW_IF_SERVER_AUTHORED(FullSyncTransaction);
	}

	// Client version.
	if (!HasAuthority() &&
		// Only force update if not already in sync or client is configured to always accept server state.
		// Force updates are only performed on new connections, so existing clients won't need them.
		((!bClientInSync || !HasAuthorityToChangeStates() || FullSyncTransaction.bFromUserLoad) || bForceRefresh))
	{
		if (IsValid(R_Instance) && IsInitialized())
		{
			LD_LOG_VERBOSE(TEXT("Client received full sync from server. %s."), *GetInfoString());
			
			// We have likely been waiting for this transaction.
			PendingTransactions.Empty();
			DoFullSync(FullSyncTransaction);
			TryStartClientPostFullSync();
		}
		else
		{
			LD_LOG_VERBOSE(TEXT("Client received full sync from server but is not initialized and is queuing the task. %s."), *GetInfoString());
			
			// Queue the transaction to be processed after we finish initializing.
			bClientHasPendingFullSyncTransaction = true;
			// We can clear out anything before since we now have the entire system state.
			PendingTransactions.Empty(1);
			QueueClientPendingTransactions(FullSyncTransaction);
		}
	}
}

void USMStateMachineComponent::CLIENT_FullSync_Implementation(const FSMFullSyncTransaction& FullSyncTransaction)
{
	MULTICAST_FullSync_Implementation(FullSyncTransaction);
}

void USMStateMachineComponent::WaitOrProcessInstanceReplicatedBeforeBeginPlay()
{
	if (!IsValid(R_Instance))
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		LD_LOG_ERROR(TEXT("SMStateMachineComponent::WaitOrProcessInstanceReplicatedBeforeBeginPlay - World is invalid for %s."), *GetName());
		return;
	}

	// Initialize after begin play has finished to avoid garbage collection checks and possible RPC issues.
	if (!World->HasBegunPlay())
	{
		// Wait each tick. While this isn't exactly efficient, this call prevents us from having per frame logic under component tick.
		// The OnBeginPlay delegate of the world can't be used either as it is not always fired in the case of replication.
		World->GetTimerManager().SetTimerForNextTick([WeakPtrThis = MakeWeakObjectPtr(this)]()
		{
			if (WeakPtrThis.IsValid())
			{
				WeakPtrThis->WaitOrProcessInstanceReplicatedBeforeBeginPlay();
			}
		});
		return;
	}

	BindToInstanceEvents();
	
	// Initialize the replicated instance with proper function calls and context.
	switch (ReplicatedInitializationMode)
	{
	case ESMThreadMode::Blocking:
		{
			R_Instance->Initialize(R_Instance->GetContext());
			Internal_OnReplicatedInstanceInitialized(R_Instance);
			break;
		}
	case ESMThreadMode::Async:
		{
			R_Instance->InitializeAsync(R_Instance->GetContext(),
				FOnStateMachineInstanceInitializedAsync::CreateUObject(this, &USMStateMachineComponent::Internal_OnReplicatedInstanceInitialized));
			break;
		}
	}
}

void USMStateMachineComponent::TryStartClientPostFullSync()
{
	ensure(bClientInSync);
	ensure(PendingTransactions.Num() == 0);

	if (R_Instance && !R_Instance->HasStarted() && bStartOnBeginPlay && IsClientAndCanLocallyChangeStates())
	{
		ServerStart();
	}
}

void USMStateMachineComponent::OnContextPawnControllerChanged(APawn* Pawn, AController* NewController)
{
	if (bHandleControllerChange && IsInitialized() && IsConfiguredForNetworking() && HasAuthority() && Pawn == GetTopMostParentActor())
	{
#if WITH_EDITORONLY_DATA
		SetNetworkDebuggingRoles();
#endif
		ConfigureInstanceNetworkSettings();

		if (bServerInSync)
		{
			// The owning client may have changed if possession is around a simulated proxy.
			FindAndSetOwningClientConnection();
			SERVER_RequestFullSync(/*bForceFullRefresh*/ true);
		}
		// Else if the server is not in sync then we should either let the process happen normally, or more likely this
		// occurred because a simulated proxy set to client authority was possessed. This case should be handled manually
		// by overloading pawn possession methods. There is no warning here because this can also occur commonly in normal
		// operation too, such as shutting down PIE.
	}
}

#if WITH_EDITORONLY_DATA
void USMStateMachineComponent::SetNetworkDebuggingRoles()
{
	NetworkRole = GetOwnerRole();
	RemoteRole = GetOwner() ? GetOwner()->GetRemoteRole() : ROLE_None;
}
#endif

#undef LOCTEXT_NAMESPACE
