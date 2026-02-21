// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMInstance.h"

#include "Blueprints/SMBlueprintGeneratedClass.h"
#include "SMCachedPropertyData.h"
#include "SMLogging.h"
#include "SMStateInstance.h"
#include "SMStateMachineComponent.h"
#include "SMTransition.h"
#include "SMTransitionInstance.h"
#include "SMUtils.h"

#include "Async/TaskGraphInterfaces.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/Engine.h"
#include "Engine/InputDelegateBinding.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "LatentActions.h"
#include "Misc/ScopeLock.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UObjectThreadContext.h"

#define LOCTEXT_NAMESPACE "SMInstance"

// Execute the function on the top most reference owner.
#define EXECUTE_ON_PRIMARY_CONST(function) \
		if (const USMInstance* Primary = GetPrimaryReferenceOwnerConst()) \
		{ \
			if (Primary != this) \
			{ \
				return Primary->function; \
			} \
		} \

#define EXECUTE_ON_PRIMARY(function) \
		if (USMInstance* Primary = GetPrimaryReferenceOwner()) \
		{ \
			if (Primary != this) \
			{ \
				return Primary->function; \
			} \
		} \
		

class FSMInitializeInstanceAsyncAction : public FPendingLatentAction
{
	/** The instance being initialized */
	TWeakObjectPtr<USMInstance> Instance;
	/** Function to execute on completion */
	FName ExecutionFunction;
	/** Link to fire on completion */
	int32 OutputLink;
	/** Object to call callback on upon completion */
	FWeakObjectPtr CallbackTarget;

public:
	FSMInitializeInstanceAsyncAction(USMInstance* InInstance, const FLatentActionInfo& LatentInfo) : Instance(InInstance)
		, ExecutionFunction(LatentInfo.ExecutionFunction)
		, OutputLink(LatentInfo.Linkage)
		, CallbackTarget(LatentInfo.CallbackTarget)
	{
	}

	virtual void UpdateOperation(FLatentResponse& Response) override
	{
		const bool bFinished = Instance.IsValid() && Instance->IsInitialized() && !Instance->IsInitializingAsync();
		Response.FinishAndTriggerIf(bFinished, ExecutionFunction, OutputLink, CallbackTarget);
	}
};

#if WITH_EDITORONLY_DATA
const FSMNode_Base* FSMDebugStateMachine::GetRuntimeNode(const FGuid& Guid) const
{
	if (const TArray<FSMNode_Base*>* Nodes = MappedNodes.Find(Guid))
	{
		if (Nodes->Num() == 0)
		{
			return nullptr;
		}

		if (Nodes->Num() == 1)
		{
			return (*Nodes)[0];
		}

		// In the case of duplicate nodes find the most recent active one.
		// This can occur when referencing parent state machine nodes multiple times
		// and from Any State transitions.
		FSMNode_Base* LastActiveNode = nullptr;
		for (FSMNode_Base* Node : *Nodes)
		{
			if (Node->IsDebugActive())
			{
				return Node;
			}
			if (Node->WasDebugActive())
			{
				LastActiveNode = Node;
			}
		}

		return LastActiveNode ? LastActiveNode : (*Nodes)[0];
	}

	return nullptr;
}

void FSMDebugStateMachine::UpdateRuntimeNode(FSMNode_Base* RuntimeNode)
{
	TArray<FSMNode_Base*>& Nodes = MappedNodes.FindOrAdd(RuntimeNode->GetNodeGuid());
	Nodes.Add(RuntimeNode);
}
#endif

void USMInstance::FInitializeInstanceAsyncTask::DoWork()
{
	if (Instance.IsValid())
	{
		Instance->Initialize(Context.Get());
	}
}

USMInstance::USMInstance() : Super()
{
	bAutoManageTime = true;
	bStopOnEndState = false;
	
	bCanEverTick = true;
#if WITH_EDITORONLY_DATA
	bCanTickInEditor = false;
#endif
	
	bCanTickWhenPaused = false;
	bTickRegistered = true;
	bTickBeforeInitialize = false;
	bTickBeforeBeginPlay = false;
	TickInterval = 0.f;

	AutoReceiveInput = ESMStateMachineInput::Disabled;
	InputPriority = 3;
	bBlockInput = false;
	
	bEnableLogging = false;
	bLogStateChange = true;
	bLogTransitionTaken = true;

	bCanReplicateAsReference = false;
	
	bCallTickOnManualUpdate = false;
	bIsTicking = false;
	bIsUpdating = false;
	bCanEvaluateTransitionsLocally = true;
	bCanTakeTransitionsLocally = true;
	bCanExecuteStateLogic = true;
	bHasStarted = false;
	bLoadFromStatesCalled = false;
	bInitialized = false;
	bInitializingAsync = false;
	bWaitingForStop = false;
	bIsStopping = false;
}

bool USMInstance::IsTickable() const
{
	// Don't check CDO.
	// On IsPendingKillOrUnreachable can cause tick lookup function to crash debug / package builds.
	// Intermittently IsTemplate may fail in this scenario so it should be checked last.
	if (!IsValid(this) || IsUnreachable() || (!IsInitialized() && !bTickBeforeInitialize) || !CanEverTick() || IsTemplate())
	{
		return false;
	}

	UWorld* ThisWorld = GetWorld();

	// Well, we tried.
	if (!ThisWorld)
	{
		return true;
	}

	return bTickBeforeBeginPlay || ThisWorld->HasBegunPlay();
}

bool USMInstance::IsTickableInEditor() const
{
#if WITH_EDITORONLY_DATA
	return bCanTickInEditor;
#else
	return false;
#endif
}

ETickableTickType USMInstance::GetTickableTickType() const
{
	if (!bTickRegistered || IsTemplate())
	{
		return ETickableTickType::Never;
	}

	return ETickableTickType::Conditional;
}

UWorld* USMInstance::GetTickableGameObjectWorld() const
{
	return GetWorld();
}

TStatId USMInstance::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(SMInstance, STATGROUP_Tickables);
}

void USMInstance::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	if (const UBlueprintGeneratedClass* BPClass = Cast<UBlueprintGeneratedClass>(GetClass()))
	{
		BPClass->GetLifetimeBlueprintReplicationList(OutLifetimeProps);
	}
	
	DOREPLIFETIME(USMInstance, R_StateMachineContext);
	DOREPLIFETIME(USMInstance, ReplicatedReferences);

	DOREPLIFETIME(USMInstance, bAutoManageTime);
	DOREPLIFETIME(USMInstance, bStopOnEndState);
	DOREPLIFETIME(USMInstance, bCanEverTick);
	DOREPLIFETIME(USMInstance, bCanTickWhenPaused);
	DOREPLIFETIME(USMInstance, TickInterval);
	DOREPLIFETIME(USMInstance, AutoReceiveInput);
	DOREPLIFETIME(USMInstance, InputPriority);
	DOREPLIFETIME(USMInstance, bBlockInput);
}

void USMInstance::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

UWorld* USMInstance::GetWorld() const
{
	// Check if the context has its own world to use.
	UObject* Context = GetContext();
	return Context ? Context->GetWorld() : nullptr;
}

int32 USMInstance::GetFunctionCallspace(UFunction* Function, FFrame* Stack)
{
	if (UObject* Context = GetContext())
	{
		return Context->GetFunctionCallspace(Function, Stack);
	}

	return Super::GetFunctionCallspace(Function, Stack);
}

bool USMInstance::CallRemoteFunction(UFunction* Function, void* Parms, FOutParmRec* OutParms, FFrame* Stack)
{
	check(!HasAnyFlags(RF_ClassDefaultObject));
	if (AActor* Context = Cast<AActor>(GetContext()))
	{
		UNetDriver* NetDriver = Context->GetNetDriver();
		if (NetDriver)
		{
			NetDriver->ProcessRemoteFunction(Context, Function, Parms, OutParms, Stack, this);
			return true;
		}
	}
	return false;
}

UObject* USMInstance::GetContext() const
{
	return R_StateMachineContext;
}

void USMInstance::Initialize(UObject* Context)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SMInstance::Initialize"), STAT_SMInstance_Initialize, STATGROUP_LogicDriver);

	if (IsInitialized())
	{
		LD_LOG_WARNING(TEXT("State machine %s is currently initialized. Call Shutdown() before initializing."), *GetName());
		return;
	}

	if (!IsValid(Context))
	{
		LD_LOG_ERROR(TEXT("Context provided to state machine %s is invalid."), *GetName());
		return;
	}

#if WITH_EDITORONLY_DATA
	if (const UBlueprint* Blueprint = UBlueprint::GetBlueprintFromClass(GetClass()))
	{
		// If this blueprint isn't compiled and it contains a call to a parent there's a good chance the class
		// data is stale. When exposed functions execute UE may crash. GitHub #200.
		if (Blueprint->IsPossiblyDirty())
		{
			if (const USMBlueprintGeneratedClass* GeneratedClass = Cast<USMBlueprintGeneratedClass>(Blueprint->GeneratedClass))
			{
				if (GeneratedClass->bHasStaleParentData)
				{
					LD_LOG_ERROR(TEXT("Cannot initialize state machine blueprint %s. It needs to be recompiled."), *GetName());
					return;
				}
			}
		}
	}
#endif

	// New objects can't be instantiated while garbage collection is active.
	check(!IsGarbageCollecting());

	const bool bIsPrimaryReferenceOwner = IsPrimaryReferenceOwner();

	// Cache graph property data used during node creation.
	CreateCachedPropertyData();

	// Context is what the instance will run under. This also sets the World the state machine operates in.
	SetContext(Context);

	// Let child classes perform any needed setup.
	OnPreStateMachineInitialized();
	if (IsInGameThread())
	{
		OnPreStateMachineInitializedEvent.Broadcast(this);
	}
	
	// Locate the properties for this state machine. This could be either from a blueprint or native class.
	TSet<FStructProperty*> Properties;
	if (!USMUtils::TryGetStateMachinePropertiesForClass(GetClass(), Properties, RootStateMachineGuid))
	{
		LD_LOG_WARNING(TEXT("Could not locate properties for state machine %s. Does the state machine have at least one entry state?"), *GetName());
		return;
	}

	// The RootGuid will have either been set by the compiler or when locating the parent class.
	if (!ensure(RootStateMachineGuid.IsValid()))
	{
		LD_LOG_ERROR(TEXT("State machine %s has an invalid guid."), *GetName());
		return;
	}
	RootStateMachine.SetNodeGuid(RootStateMachineGuid);
	RootStateMachine.SetNodeName(GetRootNodeNameDefault());
	RootStateMachine.SetNodeInstanceClass(GetRootStateMachineNodeClass());

	// Build the run-time state machine.
	if (!USMUtils::GenerateStateMachine(this, RootStateMachine, Properties))
	{
		LD_LOG_ERROR(TEXT("Error generating state machine %s. Please try recompiling the blueprint."), *GetName());
		return;
	}

	// Initialize the compiled state machine.
	{
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SMInstance::InitializeRootStateMachine"), STAT_SMInstance_Initialize_RootStateMachine, STATGROUP_LogicDriver);
		RootStateMachine.Initialize(this);
	}

	// Final initialization is performed by top most owner.
	if (bIsPrimaryReferenceOwner)
	{
		// Calculate path guids now that the instance is initialized and all node owners set.
		{
			DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SMInstance::CalculatePathGuid"), STAT_SMInstance_Initialize_CalculatePathGuid, STATGROUP_LogicDriver);
			TMap<FString, int32> Paths;
			RootStateMachine.CalculatePathGuid(Paths, true);
		}

		/* Build out a map of the state machine to use with node retrieval. */
		BuildStateMachineMap(&RootStateMachine);

		if (IsInitializingAsync())
		{
			TWeakObjectPtr<USMInstance> OurInstance = MakeWeakObjectPtr(this);
			FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
				FSimpleDelegateGraphTask::FDelegate::CreateLambda([OurInstance] ()
			{
				// Instance could be invalid if gc'd before the task was executed.
				// Instance could be initialized if user called WaitForAsyncInitializationTask(true).
				if (OurInstance.IsValid() && !OurInstance->IsInitialized())
				{
					OurInstance->FinishInitialize();
					OurInstance->CleanupAsyncInitializationTask();
				}
			}),
			TStatId(),
			nullptr,
			ENamedThreads::GameThread);
		}
		else if (IsInGameThread())
		{
			FinishInitialize();
		}
	}
}

void USMInstance::Start()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SMInstance::Start"), STAT_SMInstance_Start, STATGROUP_LogicDriver);
	
	if (!CheckIsInitialized())
	{
		return;
	}

	if (bHasStarted || (RootStateMachine.IsActive() && RootStateMachine.GetSingleActiveState() != nullptr))
	{
		// Don't log any more. This can happen frequently and by default, such as through transition replication.
		LD_LOG_VERBOSE(TEXT("Attempted to start State Machine Instance %s when it was already running."), *GetName());
		return;
	}

#if WITH_EDITOR
	// Make sure the debug object is set. When there is no OnStateBegin logic we optimize out k2 nodes since the local
	// graph doesn't have anything to do. Without a k2 node FKismetDebugUtilities::OnScriptException won't fire which
	// sets the debug object internally.
	if (UBlueprint* BlueprintObj = UBlueprint::GetBlueprintFromClass(GetClass()))
	{
		UObject* ObjectBeingDebugged = BlueprintObj->GetObjectBeingDebugged();
		if (ObjectBeingDebugged == nullptr)
		{
			const FString& PathToDebug = BlueprintObj->GetObjectPathToDebug();
			if (UObject* ObjectToDebug = FindObjectSafe<UObject>(nullptr, *PathToDebug))
			{
				ObjectBeingDebugged = ObjectToDebug;
				BlueprintObj->SetObjectBeingDebugged(ObjectBeingDebugged);
			}
		}
	}
#endif

	StatesPendingActivation.Empty();
	bHasStarted = true;
	
	TimeSinceAllowedTick = 0.f;
	OnStateMachineStart();
	OnStateMachineStartedEvent.Broadcast(this);

	// Let states run any initialization logic.
	FSMStateMachine::FGetNodeArgs Args;
	Args.bIncludeNested = true;
	Args.bSkipReferences = true;
	Args.bIncludeSelf = false;
	for (FSMNode_Base* Node : RootStateMachine.GetAllNodes(MoveTemp(Args)))
	{
		Node->OnStartedByInstance(this);
	}
	
	RootStateMachine.StartState();

	// Checks for case where the state machine starts and finishes and destroys itself in 1 frame.
	if (!IsValid(this) || IsUnreachable() || HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed))
	{
		return;
	}

	UpdateTime();
}

void USMInstance::Update(float DeltaSeconds)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SMInstance::Update"), STAT_SMInstance_Update, STATGROUP_LogicDriver);
	
	if (IsUpdating() || !HasStarted() || !CheckIsInitialized())
	{
		return;
	}

	if (HandleStopOnEndState())
	{
		return;
	}
	
	if (!RootStateMachine.IsActive())
	{
		return;
	}
	
	// Begin update. This way if tick updates again we will cancel out.
	bIsUpdating = true;

	UpdateTime();

	if (bAutoManageTime && DeltaSeconds == 0.f)
	{
		DeltaSeconds = WorldTimeDelta;
	}
	
	if (!bIsTicking && bCallTickOnManualUpdate)
	{
		Tick(DeltaSeconds);
	}

	Internal_Update(DeltaSeconds);

	// End update.
	bIsUpdating = false;
}

void USMInstance::Stop()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SMInstance::Stop"), STAT_SMInstance_Stop, STATGROUP_LogicDriver);
	
	bWaitingForStop = false;

	if (!CheckIsInitialized())
	{
		return;
	}

	if (!bHasStarted)
	{
		LD_LOG_VERBOSE(TEXT("Attempted to stop State Machine Instance when it was not already running."));
		return;
	}

	bIsStopping = true;
	
	if (RootStateMachine.IsActive())
	{
		RootStateMachine.EndState(0.f);
	}
	
	StatesPendingActivation.Empty();
	bLoadFromStatesCalled = false;
	bHasStarted = false;

	// Let states run any shutdown logic.
	FSMStateMachine::FGetNodeArgs Args;
	Args.bIncludeNested = true;
	Args.bSkipReferences = true;
	Args.bIncludeSelf = false;
	for (FSMNode_Base* Node : RootStateMachine.GetAllNodes(MoveTemp(Args)))
	{
		Node->OnStoppedByInstance(this);
	}
	
	OnStateMachineStop();
	OnStateMachineStoppedEvent.Broadcast(this);

	bIsStopping = false;
}

void USMInstance::Restart()
{
	Stop();
	Start();
}

void USMInstance::Shutdown()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SMInstance::Shutdown"), STAT_SMInstance_Shutdown, STATGROUP_LogicDriver);
	
	CancelAsyncInitialization();
	
	if (!IsInitialized())
	{
		return;
	}
	
	OnStateMachineInitializedAsyncDelegate.Unbind();
	NonThreadSafeNodes.Empty();

	const UObject* Context = GetContext();
	const bool bContextDestroyed = !IsValid(Context) || Context->IsUnreachable();
	if (IsActive() && !bContextDestroyed)
	{
		// Don't stop if context is being destroyed, this can cause blueprint runtime errors
		// if unbinding autobound transitions from the context.
		Stop();
	}
	
	UWorld* World = GetWorld();
	if (World)
	{
		USMUtils::DisableInput(World, InputComponent);
	}

	if (APawn* Pawn = Cast<APawn>(GetContext()))
	{
		Pawn->ReceiveRestartedDelegate.RemoveDynamic(this, &USMInstance::OnContextPawnRestarted);
	}
	
#if WITH_EDITOR
	// If we're running in an editor window the shutdown sequence changes.
	const bool bIsEditorWorld = World && World->IsEditorWorld() && !World->IsGameWorld();
	const bool bIsBeingDestroyed = bContextDestroyed || !IsValid(this) || IsUnreachable();

#endif
	
	for (FSMNode_Base* Node : RootStateMachine.GetAllNodes())
	{
#if WITH_EDITOR
		if (bIsEditorWorld || bIsBeingDestroyed)
		{
			Node->EditorShutdown();
			continue;
		}
#endif	
		Node->Reset();
	}

	ReplicatedReferences.Empty();

	StateMachineGuids.Empty();
	GuidNodeMap.Empty();
	GuidStateMap.Empty();
	GuidTransitionMap.Empty();

	bInitialized = false;

	if (IsValid(this) && !IsUnreachable() && !HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed) &&
		!FUObjectThreadContext::Get().IsRoutingPostLoad)
	{
		OnStateMachineShutdown();
	}
	OnStateMachineShutdownEvent.Broadcast(this);
}

void USMInstance::ReplicatedStart()
{
	if (USMStateMachineComponent* Component = GetComponentOwner())
	{
		Component->Start();
	}
	else
	{
		Start();
	}
}

void USMInstance::ReplicatedStop()
{
	if (USMStateMachineComponent* Component = GetComponentOwner())
	{
		Component->Stop();
	}
	else
	{
		Stop();
	}
}

void USMInstance::ReplicatedRestart()
{
	if (USMStateMachineComponent* Component = GetComponentOwner())
	{
		Component->Restart();
	}
	else
	{
		Restart();
	}
}

void USMInstance::StartWithNewContext(UObject* Context)
{
	SetContext(Context);
	Start();
}

void USMInstance::EvaluateTransitions()
{
	EXECUTE_ON_PRIMARY(EvaluateTransitions());
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SMInstance::EvaluateTransitions"), STAT_SMInstance_EvaluateTransitions, STATGROUP_LogicDriver);
	
	GetRootStateMachine().ProcessStates(0.f, true);
}

bool USMInstance::EvaluateAndTakeTransitionChain(USMTransitionInstance* FirstTransitionInstance)
{
	EXECUTE_ON_PRIMARY(EvaluateAndTakeTransitionChain(FirstTransitionInstance));
	
	if (FirstTransitionInstance)
	{
		if (FSMTransition* Transition = FirstTransitionInstance->GetOwningNodeAs<FSMTransition>())
		{
			return EvaluateAndTakeTransitionChain(*Transition);
		}
	}
	
	return false;
}

bool USMInstance::EvaluateAndTakeTransitionChain(FSMTransition& InFirstTransition)
{
	EXECUTE_ON_PRIMARY(EvaluateAndTakeTransitionChain(InFirstTransition));
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SMInstance::EvaluateAndTakeTransitionChain"), STAT_SMInstance_EvaluateAndTakeTransitionChain, STATGROUP_LogicDriver);
	
	if (FSMStateMachine* StateMachineOwner = static_cast<FSMStateMachine*>(InFirstTransition.GetOwnerNode()))
	{
		return StateMachineOwner->EvaluateAndTakeTransitionChain(&InFirstTransition);
	}
	
	return false;
}

bool USMInstance::EvaluateAndFindTransitionChain(USMTransitionInstance* InFirstTransitionInstance,
	TArray<USMTransitionInstance*>& OutTransitionChain, USMStateInstance_Base*& OutDestinationState,
	bool bRequirePreviousStateActive)
{
	EXECUTE_ON_PRIMARY(EvaluateAndFindTransitionChain(InFirstTransitionInstance, OutTransitionChain, OutDestinationState));

	OutTransitionChain.Reset();
	OutDestinationState = nullptr;
	
	if (InFirstTransitionInstance == nullptr)
	{
		return false;
	}

	if (FSMTransition* FirstTransition = InFirstTransitionInstance->GetOwningNodeAs<FSMTransition>())
	{
		if (bRequirePreviousStateActive && !FirstTransition->GetFromState()->IsActive())
		{
			return false;
		}
		
		TArray<FSMTransition*> Chain;
		if (FirstTransition->CanTransition(Chain))
		{
			FSMState_Base* DestinationState = FSMTransition::GetFinalStateFromChain(Chain);
			OutDestinationState = CastChecked<USMStateInstance_Base>(DestinationState->GetOrCreateNodeInstance());

			OutTransitionChain.Reserve(Chain.Num());
			for (FSMTransition* Transition : Chain)
			{
				OutTransitionChain.Add(CastChecked<USMTransitionInstance>(Transition->GetOrCreateNodeInstance()));
			}

			return true;
		}
	}
	
	return false;
}

bool USMInstance::TakeTransitionChain(const TArray<USMTransitionInstance*>& InTransitionChain)
{
	EXECUTE_ON_PRIMARY(TakeTransitionChain(InTransitionChain));

	if (InTransitionChain.Num() == 0)
	{
		return false;
	}

	if (const FSMTransition* FirstTransition = InTransitionChain[0]->GetOwningNodeAs<FSMTransition>())
	{
		if (FSMStateMachine* StateMachineOwner = static_cast<FSMStateMachine*>(FirstTransition->GetOwnerNode()))
		{
			TArray<FSMTransition*> Chain;
			Chain.Reserve(InTransitionChain.Num());
			for (const USMTransitionInstance* TransitionInstance : InTransitionChain)
			{
				Chain.Add(TransitionInstance->GetOwningNodeAs<FSMTransition>());
			}
			return StateMachineOwner->TakeTransitionChain(Chain);
		}
	}

	return false;
}

void USMInstance::PreloadAllNodeInstances()
{
	FSMStateMachine::FGetNodeArgs Args;
	Args.bIncludeNested = true;
	Args.bIncludeSelf = true;
	for (FSMNode_Base* Node : RootStateMachine.GetAllNodes(MoveTemp(Args)))
	{
		ensureMsgf(Node->GetOrCreateNodeInstance(), TEXT("Preload node instance failed for node %s."), *Node->GetNodeName());
	}
}

void USMInstance::ActivateStateLocally(const FGuid& StateGuid, bool bActive, bool bSetAllParents, bool bActivateNow)
{
	EXECUTE_ON_PRIMARY(ActivateStateLocally(StateGuid, bActive, bSetAllParents, bActivateNow));

	// Build out a list of all states that need activation. This is done to create a proper order of FSM -> sub state
	// activation, and is especially important for references so they can start through the reference and fire appropriate
	// events.
	TArray<FSMState_Base*> StatesToActivate;
	{
		FSMState_Base* State = GetStateByGuid(StateGuid);
		while (State)
		{
			StatesToActivate.Insert(State, 0);
			if (!bSetAllParents || State->GetOwnerNode() == nullptr)
			{
				break;
			}
			
			FSMStateMachine* StateMachineOwner = static_cast<FSMStateMachine*>(State->GetOwnerNode());
			State = (bActive || /* Deactivating */ !StateMachineOwner->HasActiveStates()) ? StateMachineOwner : nullptr;
		}
	}
	
	for (FSMState_Base* State : StatesToActivate)
	{
		if (FSMStateMachine* StateMachineOwner = static_cast<FSMStateMachine*>(State->GetOwnerNode()))
		{
			if (!bActivateNow)
			{
				StatesPendingActivation.Add(State);
			}
			if (bActive)
			{
				if (!StateMachineOwner->ContainsActiveState(State))
				{
					StateMachineOwner->AddActiveState(State);

					if (bActivateNow)
					{
						bool bTakeTransitions = false;
						if (StateMachineOwner->TryStartState(State, &bTakeTransitions) && bTakeTransitions)
						{
							const FSMStateMachine::FStateScopingArgs ScopeArgs
							{ { State }, { State } };
							StateMachineOwner->ProcessStates(0.f, true,
								FGuid(), ScopeArgs);
						}
					}
				}
			}
			else
			{
				StateMachineOwner->RemoveActiveState(State);
			}
		}
	}
}

void USMInstance::SwitchActiveState(USMStateInstance_Base* NewStateInstance, bool bDeactivateOtherStates)
{
	if (bDeactivateOtherStates)
	{
		TSet<FSMStateMachine*> OwningStateMachines;
		if (NewStateInstance)
		{
			if (const FSMNode_Base* OwningState = NewStateInstance->GetOwningNodeAs<FSMState_Base>())
			{
				// Find all super state machines to the new state.
				while (FSMStateMachine* OwningStateMachine = static_cast<FSMStateMachine*>(OwningState->GetOwnerNode()))
				{
					OwningStateMachines.Add(OwningStateMachine);
					OwningState = OwningState->GetOwnerNode();
				}
			}
		}
		
		// Always deactivate other states if they share the same scope or are below the new state.
		// Do not deactivate if they are one of the super state machines to the new state.
		TArray<FSMState_Base*> ActiveStates = GetAllActiveStates();
		for (FSMState_Base* State : ActiveStates)
		{
			if (State->IsStateMachine() && OwningStateMachines.Contains(static_cast<FSMStateMachine*>(State)))
			{
				continue;
			}
			
			USMUtils::ActivateStateNetOrLocal(State, false);
		}
	}

	if (NewStateInstance)
	{
		USMUtils::ActivateStateNetOrLocal(NewStateInstance->GetOwningNodeAs<FSMState_Base>(), true, true);
	}
}

void USMInstance::SwitchActiveStateByQualifiedName(const FString& InFullPath, bool bDeactivateOtherStates)
{
	if (USMStateInstance_Base* NewState = GetStateInstanceByQualifiedName(InFullPath))
	{
		SwitchActiveState(NewState, bDeactivateOtherStates);
	}
}

void USMInstance::LoadFromState(const FGuid& FromGuid, bool bAllParents, bool bNotify)
{
	if (!FromGuid.IsValid())
	{
		return;
	}

	if (FSMState_Base* State = GetStateByGuid(FromGuid))
	{
		if (FSMStateMachine* ParentSM = static_cast<FSMStateMachine*>(State->GetOwnerNode()))
		{
			ensureAlways(State != ParentSM);

			// Don't set when parent is a reference as it will just be forwarded back to this state.
			if (ParentSM->GetInstanceReference() == nullptr)
			{
				ParentSM->AddTemporaryInitialState(State);
			}

			if (bNotify)
			{
				bLoadFromStatesCalled = true;
				OnStateMachineInitialStateLoaded(FromGuid);
			}
			
			if (bAllParents && ParentSM->GetNodeGuid() != RootStateMachineGuid)
			{
				LoadFromState(ParentSM->GetGuid(), bAllParents);
			}
		}
	}
}

void USMInstance::LoadFromMultipleStates(const TArray<FGuid>& FromGuids, bool bNotify)
{
	for (const FGuid& Guid : FromGuids)
	{
		LoadFromState(Guid, false, bNotify);
	}
}

void USMInstance::ClearLoadedStates()
{
	for (const TTuple<FGuid, FSMState_Base*>& State : GetStateMap())
	{
		if (State.Value->IsStateMachine())
		{
			static_cast<FSMStateMachine*>(State.Value)->ClearTemporaryInitialStates(true);
		}
	}
}

void USMInstance::OnStateMachineInitialStateLoaded_Implementation(const FGuid& StateGuid)
{
}

void USMInstance::FinishInitialize()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SMInstance::FinishInitialize"), STAT_SMInstance_FinishInitialize, STATGROUP_LogicDriver);

	check(IsInGameThread());
	CleanupGCDelegates();

	if (IsInitialized())
	{
		LD_LOG_VERBOSE(TEXT("SMInstance::FinishInitialize called after the state machine %s was initialized. This could happen if FinishInitialize was called manually."), *GetName());
		return;
	}
	
	if (IsInitializingAsync())
	{
		if (!AsyncInitializationTask.IsValid())
		{
			LD_LOG_INFO(TEXT("SMInstance::FinishInitialize called with invalid async task for state machine %s. This could happen if an async initialization was cancelled."), *GetName());
			return;
		}
		
		// Some nodes may not have been able to be initialized.
		for (FSMNode_Base* NonInitializedNode : NonThreadSafeNodes)
		{
			NonInitializedNode->CreateNodeInstance();
		}

		NonThreadSafeNodes.Empty();
	}

	if (IsPrimaryReferenceOwner())
	{
		TArray<USMInstance*> References = GetAllReferencedInstances(true);
		for (USMInstance* Reference : References)
		{
			Reference->FinishInitialize();
		}
	}
	else if (!GetRootStateMachine().GetNodeGuid().IsValid())
	{
		// We're a reference that doesn't have a valid root node guid, indicating this reference has not successfully gone
		// through the first part of the initialize sequence which can happen if there is no entry state.
		// This reference can't safely initialize.
		return;
	}
	
	// Configure input.
	if (GetWorld() && AutoReceiveInput != ESMStateMachineInput::Disabled && UInputDelegateBinding::SupportsInputDelegate(GetClass()))
	{
		if (APlayerController* PlayerController = GetInputController())
		{
			USMUtils::EnableInputForObject(PlayerController, this, InputComponent, InputPriority, bBlockInput, !R_StateMachineContext || !R_StateMachineContext->IsA<APawn>());
		}

		if (AutoReceiveInput == ESMStateMachineInput::UseContextController)
		{
			// Context controller could change throughout the game.
			if (APawn* Pawn = Cast<APawn>(GetContext()))
			{
				Pawn->ReceiveRestartedDelegate.AddUniqueDynamic(this, &USMInstance::OnContextPawnRestarted);
			}
		}
	}
	
	// Graph functions require game thread.
	{
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SMInstance::RootStateMachine.InitializeGraphFunctions"), STAT_SMInstance_RootStateMachine_InitializeGraphFunctions, STATGROUP_LogicDriver);
		RootStateMachine.InitializeGraphFunctions();
	}
	
	// Construction scripts need to run after all nodes are initialized.
	RootStateMachine.RunConstructionScripts();
	
#if WITH_EDITORONLY_DATA
	// Load debug object for this instance.
	DebugStateMachine = FSMDebugStateMachine();
	for (const auto& KeyVal : GuidNodeMap)
	{
		DebugStateMachine.UpdateRuntimeNode(KeyVal.Value);
	}
#endif

	bInitialized = true;
	if (bInitializingAsync)
	{
		if (IsPrimaryReferenceOwner())
		{
			CleanupAsyncObjects();
		}
		
		bInitializingAsync = false;
		OnStateMachineInitializedAsyncDelegate.ExecuteIfBound(this);
	}
	
	OnStateMachineInitialized();
	OnStateMachineInitializedEvent.Broadcast(this);

	// Not needed after initialize.
	CachedPropertyData.Reset();
}

bool USMInstance::HandleStopOnEndState()
{
	if (bWaitingForStop)
	{
		return true;
	}
	bool bStopped = false;
	if (bStopOnEndState && IsInEndState() && !HasPendingActiveStates())
	{
		// If internal states need to update they still will.
		if (ISMStateMachineNetworkedInterface* NetworkObject = TryGetNetworkInterface())
		{
			bWaitingForStop = true;
			if (NetworkObject->HasAuthorityToChangeStates())
			{
				// Only signal to stop if we're allowed to. If the client is authoritative but is only following
				// server transactions then the client should request the stop.
				NetworkObject->ServerStop();
			}
			bStopped = true;
		}
		else
		{
			Stop();
			bStopped = true;
		}
	}

	return bStopped;
}

void USMInstance::InitializeAsync(UObject* Context, const FOnStateMachineInstanceInitializedAsync& OnCompletedDelegate)
{
	if (IsInitializingAsync())
	{
		LD_LOG_ERROR(TEXT("SMInstance::InitializeAsync - Cannot initialize state machine instance `%s` async, an async initialization is already in progress."), *GetName());
		return;
	}

	if (!IsValid(Context))
	{
		LD_LOG_ERROR(TEXT("Context provided to state machine %s is invalid."), *GetName());
		return;
	}
	
	bInitializingAsync = true;
	OnStateMachineInitializedAsyncDelegate = OnCompletedDelegate;
	NonThreadSafeNodes.Reset();

	OnPreGarbageCollectHandle = FCoreUObjectDelegates::GetPreGarbageCollectDelegate().AddUObject(this, &USMInstance::OnPreGarbageCollect);
	
	AsyncInitializationTask = MakeUnique<FAsyncTask<FInitializeInstanceAsyncTask>>(this, Context);
	if (IsInGameThread())
	{
		AsyncInitializationTask->StartBackgroundTask();
	}
	else
	{
		LD_LOG_INFO(TEXT("SMInstance::InitializeAsync - Called from outside of the gamethread for state machine `%s'. This process will run synchronously in the current thread."), *GetName());
		AsyncInitializationTask->StartSynchronousTask();
	}
}

void USMInstance::K2_InitializeAsync(UObject* Context, FLatentActionInfo LatentInfo)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(Context, EGetWorldErrorMode::LogAndReturnNull))
	{
		FLatentActionManager& LatentActionManager = World->GetLatentActionManager();
		FSMInitializeInstanceAsyncAction* Action = LatentActionManager.FindExistingAction<FSMInitializeInstanceAsyncAction>(LatentInfo.CallbackTarget, LatentInfo.UUID);
		if (Action == nullptr)
		{
			Action = new FSMInitializeInstanceAsyncAction(this, LatentInfo);
			LatentActionManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, Action);
		}
	}
	
	InitializeAsync(Context);
}

void USMInstance::CancelAsyncInitialization()
{
	if (AsyncInitializationTask.IsValid() && !AsyncInitializationTask->IsDone())
	{
		AsyncInitializationTask->Cancel();
	}
	
	CleanupAsyncInitializationTask();
}

void USMInstance::WaitForAsyncInitializationTask(bool bCallFinishInitialize)
{
	if (AsyncInitializationTask.IsValid() && !AsyncInitializationTask->IsDone())
	{
		AsyncInitializationTask->EnsureCompletion();
	}
	
	if (bCallFinishInitialize && !IsInitialized() && IsInitializingAsync() && IsInGameThread())
	{
		FinishInitialize();
		CleanupAsyncInitializationTask();
	}
	else if (IsInGameThread())
	{
		CleanupGCDelegates();
	}
}

void USMInstance::CleanupAsyncObjects()
{
	if (!IsValid(this) || IsUnreachable() || HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed))
	{
		// Not safe to check references if we're being destroyed. But if we're being destroyed then
		// async flags shouldn't have to be cleared anyway.
		return;
	}

	// Clear all async flags so objects can be gc'd normally.
	TArray<USMInstance*> References = GetAllReferencedInstances(true);
	for (const USMInstance* Reference : References)
	{
		Reference->AtomicallyClearInternalFlags(EInternalObjectFlags::Async);
		ForEachObjectWithOuter(Reference, [](const UObject* Object)
		{
			Object->AtomicallyClearInternalFlags(EInternalObjectFlags::Async);
		}, /* bIncludeNestedObjects*/ true);
	}
		
	this->AtomicallyClearInternalFlags(EInternalObjectFlags::Async);
	ForEachObjectWithOuter(this, [](const UObject* Object)
	{
		Object->AtomicallyClearInternalFlags(EInternalObjectFlags::Async);
	}, /* bIncludeNestedObjects*/ true);
}

void USMInstance::CleanupAsyncInitializationTask()
{
	bInitializingAsync = false;
	
	if (AsyncInitializationTask.IsValid())
	{
		AsyncInitializationTask->EnsureCompletion();
		AsyncInitializationTask.Reset();
	}

	CleanupAsyncObjects();
	CleanupGCDelegates();
}

void USMInstance::OnPreGarbageCollect()
{
	LD_LOG_VERBOSE(TEXT("Engine is garbage collecting during initialization of state machine %s. Waiting for task to finish..."), *GetName())
	WaitForAsyncInitializationTask();
}

void USMInstance::CleanupGCDelegates()
{
	if (OnPreGarbageCollectHandle.IsValid())
	{
		FCoreUObjectDelegates::GetPreGarbageCollectDelegate().Remove(OnPreGarbageCollectHandle);
		OnPreGarbageCollectHandle.Reset();
	}
}

void USMInstance::TryGetNestedActiveState(FSMStateInfo& FoundState, bool& bSuccess) const
{
	if (FSMState_Base* State = GetSingleNestedActiveState())
	{
		FoundState = *State;
		bSuccess = true;
		return;
	}

	bSuccess = false;
}

FSMState_Base* USMInstance::GetSingleActiveState() const
{
	return RootStateMachine.GetSingleActiveState();
}

FSMState_Base* USMInstance::GetSingleNestedActiveState() const
{
	FSMState_Base* CurrentState = RootStateMachine.GetSingleActiveState();

	if (CurrentState != nullptr)
	{
		while (CurrentState->IsStateMachine())
		{
			FSMState_Base* SubCurrentState = ((FSMStateMachine*)CurrentState)->GetSingleActiveState();
			if (SubCurrentState == nullptr || SubCurrentState == CurrentState)
			{
				// This could be an empty state machine in which case return itself.
				// The state equal to itself isn't possible, but maybe a user did something weird.
				break;
			}

			CurrentState = SubCurrentState;
		}
	}

	return CurrentState;
}

TArray<FSMState_Base*> USMInstance::GetAllActiveStates() const
{
	return RootStateMachine.GetAllNestedActiveStates();
}

FGuid USMInstance::GetSingleActiveStateGuid(bool bCheckNested) const
{
	if (FSMState_Base* CurrentState = bCheckNested ? GetSingleNestedActiveState() : GetSingleActiveState())
	{
		return CurrentState->GetGuid();
	}

	return FGuid();
}

void USMInstance::GetAllActiveStateGuids(TArray<FGuid>& ActiveGuids) const
{
	TArray<FSMState_Base*> CurrentStates = GetAllActiveStates();
	ActiveGuids.Reset(CurrentStates.Num());
	
	for (const FSMState_Base* State : CurrentStates)
	{
		ActiveGuids.AddUnique(State->GetGuid());
	}
}

TArray<FGuid> USMInstance::GetAllActiveStateGuidsCopy() const
{
	TArray<FGuid> OutGuids;
	GetAllActiveStateGuids(OutGuids);

	return OutGuids;
}

USMStateInstance_Base* USMInstance::GetSingleActiveStateInstance(bool bCheckNested) const
{
	if (FSMState_Base* CurrentState = bCheckNested ? GetSingleNestedActiveState() : GetSingleActiveState())
	{
		return Cast<USMStateInstance_Base>(CurrentState->GetOrCreateNodeInstance());
	}

	return nullptr;
}

void USMInstance::GetAllActiveStateInstances(TArray<USMStateInstance_Base*>& ActiveStateInstances) const
{
	TArray<FSMState_Base*> ActiveStates = GetAllActiveStates();
	ActiveStateInstances.Reset(ActiveStates.Num());
	
	for (FSMState_Base* State : ActiveStates)
	{
		if (USMStateInstance_Base* StateInstance = Cast<USMStateInstance_Base>(State->GetOrCreateNodeInstance()))
		{
			ActiveStateInstances.Add(StateInstance);
		}
	}
}

TArray<USMInstance*> USMInstance::GetAllReferencedInstances(bool bIncludeChildren) const
{
	TArray<USMInstance*> ReturnValue;

	for (const FGuid& StateMachineGuid : StateMachineGuids)
	{
		if (const FSMStateMachine* StateMachine = static_cast<FSMStateMachine*>(GetStateByGuid(StateMachineGuid)))
		{
			USMInstance* InstanceReference = StateMachine->GetInstanceReference();
			if (!InstanceReference)
			{
				continue;
			}

			// Verify we directly own this instance and it isn't a grand child.
			if (!bIncludeChildren && InstanceReference->GetRootStateMachine().GetReferencedByInstance() != this)
			{
				continue;
			}

			ReturnValue.Add(InstanceReference);
		}
	}

	return ReturnValue;
}

TArray<FSMStateMachine*> USMInstance::GetStateMachinesWithReferences(bool bIncludeChildren) const
{
	TArray<FSMStateMachine*> ReturnValue;

	for (const FGuid& StateMachineGuid : StateMachineGuids)
	{
		if (FSMStateMachine* StateMachine = static_cast<FSMStateMachine*>(GetStateByGuid(StateMachineGuid)))
		{
			USMInstance* InstanceReference = StateMachine->GetInstanceReference();
			if (!InstanceReference)
			{
				continue;
			}

			// Verify we directly own this instance and it isn't a grand child.
			if (!bIncludeChildren && InstanceReference->GetRootStateMachine().GetReferencedByInstance() != this)
			{
				continue;
			}

			ReturnValue.AddUnique(StateMachine);
		}
	}

	return ReturnValue;
}

void USMInstance::TryGetStateInfo(const FGuid& Guid, FSMStateInfo& StateInfo, bool& bSuccess) const
{
	EXECUTE_ON_PRIMARY_CONST(TryGetStateInfo(Guid, StateInfo, bSuccess));
	
	if (const FSMState_Base* FoundState = GetStateByGuid(Guid))
	{
		StateInfo = FSMStateInfo(*FoundState);
		bSuccess = true;
		return;
	}

	bSuccess = false;
}

void USMInstance::TryGetTransitionInfo(const FGuid& Guid, FSMTransitionInfo& TransitionInfo, bool& bSuccess) const
{
	EXECUTE_ON_PRIMARY_CONST(TryGetTransitionInfo(Guid, TransitionInfo, bSuccess));

	if (const FSMTransition* FoundTransition = GetTransitionByGuid(Guid))
	{
		TransitionInfo = FSMTransitionInfo(*FoundTransition);
		bSuccess = true;
		return;
	}

	bSuccess = false;
}

USMInstance* USMInstance::GetReferencedInstanceByGuid(const FGuid& Guid) const
{
	EXECUTE_ON_PRIMARY_CONST(GetReferencedInstanceByGuid(Guid));
	
	if (FSMState_Base* State = GetStateByGuid(Guid))
	{
		if (State->IsStateMachine())
		{
			return static_cast<FSMStateMachine*>(State)->GetInstanceReference();
		}
	}

	return nullptr;
}

USMStateInstance_Base* USMInstance::GetStateInstanceByGuid(const FGuid& Guid) const
{
	EXECUTE_ON_PRIMARY_CONST(GetStateInstanceByGuid(Guid));

	if (FSMState_Base* State = GetStateByGuid(Guid))
	{
		return Cast<USMStateInstance_Base>(State->GetOrCreateNodeInstance());
	}

	return nullptr;
}

USMTransitionInstance* USMInstance::GetTransitionInstanceByGuid(const FGuid& Guid) const
{
	EXECUTE_ON_PRIMARY_CONST(GetTransitionInstanceByGuid(Guid));

	if (FSMTransition* Transition = GetTransitionByGuid(Guid))
	{
		return Cast<USMTransitionInstance>(Transition->GetOrCreateNodeInstance());
	}

	return nullptr;
}

USMNodeInstance* USMInstance::GetNodeInstanceByGuid(const FGuid& Guid) const
{
	EXECUTE_ON_PRIMARY_CONST(GetNodeInstanceByGuid(Guid));

	if (FSMNode_Base* Node = GetNodeByGuid(Guid))
	{
		return Node->GetOrCreateNodeInstance();
	}

	return nullptr;
}

USMStateInstance_Base* USMInstance::GetStateInstanceByQualifiedName(const FString& InFullPath) const
{
	EXECUTE_ON_PRIMARY_CONST(GetStateInstanceByQualifiedName(InFullPath));

	TArray<FString> OutNames;
	const int32 NodePathSize = InFullPath.ParseIntoArray(OutNames, TEXT("."));

	if (NodePathSize == 0)
	{
		if (!InFullPath.IsEmpty())
		{
			OutNames.Add(InFullPath);
		}
		else
		{
			LD_LOG_ERROR(TEXT("SMInstance::GetStateInstanceByQualifiedName: No input provided."));
			return nullptr;
		}
	}

	USMStateInstance_Base* CurrentNode = GetRootStateMachineNodeInstance();
	if (CurrentNode)
	{
		while (OutNames.Num() > 0)
		{
			FString& NodeName = OutNames[0];

			if (CurrentNode->IsStateMachine())
			{
				USMStateMachineInstance* StateMachineNode = CastChecked<USMStateMachineInstance>(CurrentNode);
				if (const USMInstance* ReferenceInstance = StateMachineNode->GetStateMachineReference())
				{
					CurrentNode = ReferenceInstance->GetRootStateMachineNodeInstance()->GetContainedStateByName(NodeName);
				}
				else
				{
					CurrentNode = StateMachineNode->GetContainedStateByName(NodeName);
					if (!CurrentNode)
					{
						const FString RootNodeName = GetRootNodeNameDefault();
						if (!CurrentNode && NodeName == RootNodeName && StateMachineNode->GetNodeName() == RootNodeName)
						{
							CurrentNode = StateMachineNode;
						}
					}
				}
			}

			if (!CurrentNode)
			{
				LD_LOG_ERROR(TEXT("SMInstance::GetStateInstanceByQualifiedName: Could not find node %s in state machine blueprint %s."),
					*NodeName, *GetName());
				return nullptr;
			}
		
			if (CurrentNode->GetNodeName() != NodeName)
			{
				LD_LOG_ERROR(TEXT("SMInstance::GetStateInstanceByQualifiedName: Found node %s but expected node %s in state machine blueprint %s."),
					*CurrentNode->GetNodeName(), *NodeName, *GetName());
				return nullptr;
			}
		
			OutNames.RemoveAt(0);
		}
	}
	
	return CurrentNode;
}

FSMState_Base* USMInstance::GetStateByGuid(const FGuid& Guid) const
{
	if (FSMState_Base* const* State = GuidStateMap.Find(GetRedirectedGuid(Guid)))
	{
		return *State;
	}

	return nullptr;
}

FSMTransition* USMInstance::GetTransitionByGuid(const FGuid& Guid) const
{
	if (FSMTransition* const* Transition = GuidTransitionMap.Find(GetRedirectedGuid(Guid)))
	{
		return *Transition;
	}

	return nullptr;
}

FSMNode_Base* USMInstance::GetNodeByGuid(const FGuid& Guid) const
{
	if (FSMNode_Base* const* Value = GetNodeMap().Find(GetRedirectedGuid(Guid)))
	{
		return *Value;
	}

	return nullptr;
}

FSMState_Base* USMInstance::FindStateByGuid(const FGuid& Guid) const
{
	const FGuid GuidToUse = GetRedirectedGuid(Guid);
	if (RootStateMachineGuid == GuidToUse)
	{
		return const_cast<FSMStateMachine*>(&RootStateMachine);
	}
	return RootStateMachine.FindState(GuidToUse);
}

FGuid USMInstance::GetRedirectedGuid(const FGuid& InPathGuid) const
{
	if (const FGuid* RedirectedGuid = PathGuidRedirectMap.Find(InPathGuid))
	{
		return *RedirectedGuid;
	}

	return InPathGuid;
}

USMStateMachineInstance* USMInstance::GetRootStateMachineNodeInstance() const
{
	return Cast<USMStateMachineInstance>(const_cast<FSMStateMachine&>(RootStateMachine).GetOrCreateNodeInstance());
}

bool USMInstance::IsActive() const
{
	return bInitialized ? RootStateMachine.IsActive() : false;
}

void USMInstance::SetCanEverTick(bool Value)
{
	bCanEverTick = Value;

	// Only update networking settings on the primary. It's possible tick settings could be manually changed on
	// references. Once bLetInstanceManageTick is deprecated and if bAllowIndependentTick is also deprecated, then
	// this restriction may be removed. SetCanEverTick in that case should probably update all instances.
	if (IsPrimaryReferenceOwner())
	{
		if (ISMStateMachineNetworkedInterface* NetworkObject = TryGetNetworkInterface())
		{
			NetworkObject->SetCanEverNetworkTick(Value);
		}
	}
}

void USMInstance::SetRegisterTick(bool Value)
{
	bTickRegistered = Value;
}

void USMInstance::SetTickOnManualUpdate(bool Value)
{
	bCallTickOnManualUpdate = Value;
}

void USMInstance::SetCanTickWhenPaused(bool Value)
{
	bCanTickWhenPaused = Value;
}

#if WITH_EDITORONLY_DATA
void USMInstance::SetCanTickInEditor(bool Value)
{
	bCanTickInEditor = Value;
}
#endif

void USMInstance::SetTickBeforeBeginPlay(bool Value)
{
	bTickBeforeBeginPlay = Value;
}

void USMInstance::SetTickInterval(float Value)
{
	TickInterval = Value;
}

void USMInstance::SetAutoManageTime(bool Value)
{
	bAutoManageTime = Value;
}

void USMInstance::SetStopOnEndState(bool Value)
{
	bStopOnEndState = Value;
}

bool USMInstance::IsInEndState() const
{
	return RootStateMachine.IsStateMachineInEndState();
}

void USMInstance::SetContext(UObject* Context)
{
	R_StateMachineContext = Context;
	if (IsPrimaryReferenceOwner() && R_StateMachineContext && GetOuter() != R_StateMachineContext)
	{
		Rename(nullptr, R_StateMachineContext, REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
	}
}

const TMap<FGuid, FSMNode_Base*>& USMInstance::GetNodeMap() const
{
	ensureMsgf(IsPrimaryReferenceOwner(), TEXT("`GetNodeMap` is no longer populated on references. Call from `GetPrimaryReferenceOwner` instead."));
	return GuidNodeMap;
}

const TMap<FGuid, FSMState_Base*>& USMInstance::GetStateMap() const
{
	ensureMsgf(IsPrimaryReferenceOwner(), TEXT("`GetStateMap` is no longer populated on references. Call from `GetPrimaryReferenceOwner` instead."));
	return GuidStateMap;
}

const TMap<FGuid, FSMTransition*>& USMInstance::GetTransitionMap() const
{
	ensureMsgf(IsPrimaryReferenceOwner(), TEXT("`GetTransitionMap` is no longer populated on references. Call from `GetPrimaryReferenceOwner` instead."));
	return GuidTransitionMap;
}

const TArray<FSMStateHistory>& USMInstance::GetStateHistory() const
{
	EXECUTE_ON_PRIMARY_CONST(GetStateHistory());
	return StateHistory;
}

void USMInstance::SetStateHistoryMaxCount(int32 NewSize)
{
	EXECUTE_ON_PRIMARY(SetStateHistoryMaxCount(NewSize));
	StateHistoryMaxCount = NewSize;
	TrimStateHistory();
}

int32 USMInstance::GetStateHistoryMaxCount() const
{
	EXECUTE_ON_PRIMARY_CONST(GetStateHistoryMaxCount());
	return StateHistoryMaxCount;
}

void USMInstance::ClearStateHistory()
{
	EXECUTE_ON_PRIMARY(ClearStateHistory());
	StateHistory.Empty();
}

void USMInstance::GetAllStateInstances(TArray<USMStateInstance_Base*>& StateInstances) const
{
	if (IsPrimaryReferenceOwner())
	{
		// Primary reference owners have all nodes already mapped out.

		const TMap<FGuid, FSMState_Base*>& StateMap = GetStateMap();
		StateInstances.Reset(StateMap.Num());

		for (const TTuple<FGuid, FSMState_Base*>& KeyVal : StateMap)
		{
			if (!KeyVal.Value->CanEverCreateNodeInstance())
			{
				// Prevents references from being counted twice.
				continue;
			}
			if (USMStateInstance_Base* StateInstance = Cast<USMStateInstance_Base>(KeyVal.Value->GetOrCreateNodeInstance()))
			{
				StateInstances.Add(StateInstance);
			}
		}
	}
	else
	{
		// We are a reference and don't have nodes mapped out, so iterate the root state machine down building a list.

		FSMStateMachine::FGetNodeArgs GetNodeArgs;
		GetNodeArgs.bIncludeNested = true;
		GetNodeArgs.bIncludeSelf = true;
		TArray<FSMNode_Base*> AllNodes = RootStateMachine.GetAllNodes(MoveTemp(GetNodeArgs));

		StateInstances.Reset();

		for (FSMNode_Base* Node : AllNodes)
		{
			if (!Node || !Node->CanEverCreateNodeInstance() /* Prevents references from being counted twice. */ ||
				!Node->GetNodeInstanceClass() ||
				!Node->GetNodeInstanceClass()->IsChildOf(USMStateInstance_Base::StaticClass()))
			{
				continue;
			}

			if (USMStateInstance_Base* StateInstance = Cast<USMStateInstance_Base>(Node->GetOrCreateNodeInstance()))
			{
				StateInstances.Add(StateInstance);
			}
		}
	}
}

void USMInstance::GetAllTransitionInstances(TArray<USMTransitionInstance*>& TransitionInstances) const
{
	if (IsPrimaryReferenceOwner())
	{
		// Primary reference owners have all nodes already mapped out.

		const TMap<FGuid, FSMTransition*>& TransitionMap = GetTransitionMap();
		TransitionInstances.Reset(TransitionMap.Num());

		for (const TTuple<FGuid, FSMTransition*>& KeyVal : TransitionMap)
		{
			if (USMTransitionInstance* TransitionInstance = Cast<USMTransitionInstance>(KeyVal.Value->GetOrCreateNodeInstance()))
			{
				TransitionInstances.Add(TransitionInstance);
			}
		}
	}
	else
	{
		// We are a reference and don't have nodes mapped out, so iterate the root state machine down building a list.

		FSMStateMachine::FGetNodeArgs GetNodeArgs;
		GetNodeArgs.bIncludeNested = true;
		GetNodeArgs.bIncludeSelf = true;
		TArray<FSMNode_Base*> AllNodes = RootStateMachine.GetAllNodes(MoveTemp(GetNodeArgs));

		TransitionInstances.Reset();

		for (FSMNode_Base* Node : AllNodes)
		{
			if (!Node ||
				!Node->GetNodeInstanceClass() ||
				!Node->GetNodeInstanceClass()->IsChildOf(USMTransitionInstance::StaticClass()))
			{
				continue;
			}

			if (USMTransitionInstance* TransitionInstance = Cast<USMTransitionInstance>(Node->GetOrCreateNodeInstance()))
			{
				TransitionInstances.Add(TransitionInstance);
			}
		}
	}
}

void USMInstance::SetNetworkInterface(TScriptInterface<ISMStateMachineNetworkedInterface> InNetworkInterface)
{
	NetworkInterface = InNetworkInterface;
}

TScriptInterface<ISMStateMachineNetworkedInterface> USMInstance::GetNetworkInterface() const
{
	EXECUTE_ON_PRIMARY_CONST(GetNetworkInterface());
	return NetworkInterface;
}

void USMInstance::K2_TryGetNetworkInterface(TScriptInterface<ISMStateMachineNetworkedInterface>& Interface,
	bool& bIsValid)
{
	EXECUTE_ON_PRIMARY(K2_TryGetNetworkInterface(Interface, bIsValid));
	if (NetworkInterface.GetObject())
	{
		Interface = NetworkInterface;
		bIsValid = true;
		return;
	}

	bIsValid = false;
}

ISMStateMachineNetworkedInterface* USMInstance::TryGetNetworkInterface() const
{
	EXECUTE_ON_PRIMARY_CONST(TryGetNetworkInterface());
	if (NetworkInterface.GetObject() && NetworkInterface->IsConfiguredForNetworking())
	{
		return Cast<ISMStateMachineNetworkedInterface>(NetworkInterface.GetObject());
	}

	return nullptr;
}

void USMInstance::UpdateNetworkConditions()
{
	for (const auto& StateMachineGuid : StateMachineGuids)
	{
		FSMStateMachine* Node = static_cast<FSMStateMachine*>(GuidNodeMap.FindRef(StateMachineGuid));

		if (USMInstance* ReferencedStateMachine = Node->GetInstanceReference())
		{
			// The referenced instance will inherit the owning instance's network settings.
			ReferencedStateMachine->CopyNetworkConditionsFrom(this, true);
		}
		else
		{
			Node->SetNetworkedConditions(GetNetworkInterface(), bCanEvaluateTransitionsLocally, bCanTakeTransitionsLocally, bCanExecuteStateLogic);
		}
	}
}

void USMInstance::CopyNetworkConditionsFrom(USMInstance* OtherInstance, bool bUpdateNodes)
{
	check(OtherInstance);
	
	SetNetworkInterface(OtherInstance->NetworkInterface);
	SetAllowTransitionsLocally(OtherInstance->bCanEvaluateTransitionsLocally, OtherInstance->bCanTakeTransitionsLocally);
	SetAllowStateLogic(OtherInstance->bCanExecuteStateLogic);

	if (bUpdateNodes)
	{
		UpdateNetworkConditions();
	}
}

void USMInstance::SetAllowTransitionsLocally(bool bCanEvaluateTransitions, bool bCanTakeTransitions)
{
	bCanEvaluateTransitionsLocally = bCanEvaluateTransitions;
	bCanTakeTransitionsLocally = bCanTakeTransitions;
}

void USMInstance::SetAllowStateLogic(bool bAllow)
{
	bCanExecuteStateLogic = bAllow;
}

bool USMInstance::IsReferenceTemplate() const
{
	return HasAnyFlags(RF_ArchetypeObject) && !HasAnyFlags(RF_ClassDefaultObject) && GetName().StartsWith("TEMPLATE");
}

void USMInstance::SetReferenceOwner(USMInstance* Owner)
{
	ReferenceOwner = Owner;
	if (ReferenceOwner && ReferenceOwner != GetOuter())
	{
		Rename(nullptr, ReferenceOwner, REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
	}
}

void USMInstance::AddReplicatedReference(const FGuid& PathGuid, USMInstance* NewReference)
{
	EXECUTE_ON_PRIMARY(AddReplicatedReference(PathGuid, NewReference));
	FSMReferenceContainer ReferenceContainer;
	ReferenceContainer.PathGuid = PathGuid;
	ReferenceContainer.Reference = NewReference;
	ReplicatedReferences.Add(MoveTemp(ReferenceContainer));
}

USMInstance* USMInstance::FindReplicatedReference(const FGuid& PathGuid) const
{
	EXECUTE_ON_PRIMARY_CONST(FindReplicatedReference(PathGuid));
	if (const FSMReferenceContainer* Container = ReplicatedReferences.FindByPredicate([PathGuid](const FSMReferenceContainer& ReferenceContainer)
	{
		return ReferenceContainer.PathGuid == PathGuid;
	}))
	{
		return Container->Reference;
	}

	return nullptr;
}

bool USMInstance::HaveAllReferencesReplicated() const
{
	for (const FSMReferenceContainer& ReferenceContainer : ReplicatedReferences)
	{
		if (ReferenceContainer.Reference == nullptr)
		{
			return false;
		}
	}

	return true;
}

void USMInstance::REP_OnReplicatedReferencesLoaded()
{
	if (HaveAllReferencesReplicated())
	{
		OnReferencesReplicatedEvent.ExecuteIfBound();
	}
}

const USMInstance* USMInstance::GetPrimaryReferenceOwnerConst() const
{
	const USMInstance* Parent = ReferenceOwner;
	while (Parent)
	{
		const USMInstance* Next = Parent->GetReferenceOwnerConst();
		if (!Next)
		{
			return Parent;
		}
		Parent = Next;
	}

	return this;
}

USMInstance* USMInstance::GetPrimaryReferenceOwner()
{
	USMInstance* Parent = ReferenceOwner;
	while (Parent)
	{
		USMInstance* Next = Parent->GetReferenceOwner();
		if (!Next)
		{
			return Parent;
		}
		Parent = Next;
	}

	return this;
}

void USMInstance::NotifyTransitionTaken(const FSMTransition& Transition)
{
	const FSMTransitionInfo TransitionInfo(Transition);

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	if (IsLoggingEnabled() && bLogTransitionTaken)
	{
		const FString ContextName = GetContext() ? GetContext()->GetName() : TEXT("NoContext");
		LD_LOG_INFO(TEXT("[%s-%s] Transition taken: %s"), *ContextName, *GetName(), *TransitionInfo.ToString());
	}
#endif
	
	OnStateMachineTransitionTaken(TransitionInfo);
	OnStateMachineTransitionTakenEvent.Broadcast(this, TransitionInfo);

	if (ReferenceOwner)
	{
		ReferenceOwner->NotifyTransitionTaken(Transition);
	}
}

void USMInstance::NotifyStateChange(FSMState_Base* ToState, FSMState_Base* FromState)
{
	const FSMStateInfo ToStateInfo(ToState ? *ToState : FSMState_Base());
	const FSMStateInfo FromStateInfo(FromState ? *FromState : FSMState_Base());

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	if (IsLoggingEnabled() && bLogStateChange)
	{
		const FString ContextName = GetContext() ? GetContext()->GetName() : TEXT("NoContext");
		LD_LOG_INFO(TEXT("[%s-%s] State change: from %s to %s"), *ContextName, *GetName(), *FromStateInfo.ToString(), *ToStateInfo.ToString());
	}
#endif

	RecordPreviousStateHistory(FromState);
	
	OnStateMachineStateChanged(ToStateInfo, FromStateInfo);
	OnStateMachineStateChangedEvent.Broadcast(this, ToStateInfo, FromStateInfo);

	if (ReferenceOwner)
	{
		ReferenceOwner->NotifyStateChange(ToState, FromState);
	}
}

void USMInstance::NotifyStateStarted(const FSMState_Base& State)
{
	const FSMStateInfo StateInfo(State);
	OnStateMachineStateStarted(StateInfo);
	OnStateMachineStateStartedEvent.Broadcast(this, StateInfo);

	if (!IsPrimaryReferenceOwner())
	{
		EXECUTE_ON_PRIMARY(NotifyStateStarted(State));
	}
}

void USMInstance::Tick_Implementation(float DeltaTime)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SMInstance::Tick"), STAT_SMInstance_Tick, STATGROUP_LogicDriver);
	
	if (!CanEverTick() || bIsTicking)
	{
		return;
	}

	// Check if we are allowed to tick depending on the interval.
	TimeSinceAllowedTick += DeltaTime;
	if (TimeSinceAllowedTick < TickInterval)
	{
		return;
	}

	// Signal we are ticking in case an update tries to call tick manually.
	bIsTicking = true;

	// It's possible we're not initialized but still ticking. This saves us a call and a warning.
	if (IsInitialized())
	{
		Update(TimeSinceAllowedTick);
	}

	TimeSinceAllowedTick = 0.f;

	bIsTicking = false;
}

void USMInstance::RunUpdateAsReference(float DeltaSeconds)
{
	Internal_Update(DeltaSeconds);
}

void USMInstance::Internal_Update(float DeltaSeconds)
{
	// It's okay to do a full update if this wasn't called by Update.
	if (!IsUpdating())
	{
		return Update(DeltaSeconds);
	}
	
	// Perform after end state check and before update routine.
	StatesPendingActivation.Empty();
	
	OnStateMachineUpdate(DeltaSeconds);
	OnStateMachineUpdatedEvent.Broadcast(this, DeltaSeconds);

	RootStateMachine.UpdateState(DeltaSeconds);
	
	// Run again after updating as the state machine could have moved into an end state.
	if (HandleStopOnEndState())
	{
		return;
	}
}

bool USMInstance::Internal_EvaluateAndTakeTransitionChainByGuid(const FGuid& PathGuid)
{
	EXECUTE_ON_PRIMARY(Internal_EvaluateAndTakeTransitionChainByGuid(PathGuid))
	
	if (FSMTransition* Transition = GetTransitionByGuid(PathGuid))
	{
		return EvaluateAndTakeTransitionChain(*Transition);
	}

	return false;
}

void USMInstance::Internal_EventUpdate()
{
	EXECUTE_ON_PRIMARY(Internal_EventUpdate());
	Internal_Update(0.f);
}

void USMInstance::Internal_EventCleanup(const FGuid& PathGuid)
{
	EXECUTE_ON_PRIMARY(Internal_EventCleanup(PathGuid))
	
	if (FSMTransition* Transition = GetTransitionByGuid(PathGuid))
	{
		// Auto-bound events will set bIsEvaluating to true primarily for debugging. However if two events fire at the exact same time
		// it won't be set to false unless this cleanup method is run.
		Transition->bIsEvaluating = false;
	}
}

void USMInstance::BuildStateMachineMap(FSMStateMachine* StateMachine)
{
	TSet<USMInstance*> InstancesMapped;
	BuildStateMachineMap(StateMachine, InstancesMapped);
}

void USMInstance::BuildStateMachineMap(FSMStateMachine* StateMachine, TSet<USMInstance*>& InstancesMapped)
{
	InstancesMapped.Add(this);

	const FGuid& StateMachineGuid = StateMachine->GetGuid();

	// Reference self.
	ensureMsgf(!StateMachineGuids.Contains(StateMachineGuid), TEXT("State machine %s already contains state machine guid %s"), *GetName(), *StateMachineGuid.ToString());
	StateMachineGuids.Add(StateMachineGuid);

	// This check prevents the state machine referenced from overriding the parent duplicate that points to the reference.
	if (!GuidNodeMap.Contains(StateMachineGuid))
	{
		GuidNodeMap.Add(StateMachineGuid, StateMachine);
		GuidStateMap.Add(StateMachineGuid, StateMachine);
	}

	// Build out guids of all contained nodes in references.
	if (USMInstance* ReferencedStateMachine = StateMachine->GetInstanceReference())
	{
		if (!InstancesMapped.Contains(ReferencedStateMachine))
		{
			InstancesMapped.Add(ReferencedStateMachine);
			BuildStateMachineMap(&ReferencedStateMachine->GetRootStateMachine(), InstancesMapped);
		}
		return;
	}

	for (FSMTransition* Transition : StateMachine->GetTransitions())
	{
		const FGuid& Guid = Transition->GetGuid();
		/*
		 * Unique GUID check 2:
		 * The PathGuid at this stage should always be unique and the ensure should never be tripped.
		 * The Guid here is calculated based on the path of the node in the state machine which allows multiple same reference calls to exist in the same blueprint.
		 *
		 * If this is triggered please check to make sure the state machine blueprint in question doesn't do anything abnormal such as use circular referencing.
		 */
		ensureMsgf(!GuidNodeMap.Contains(Guid), TEXT("State machine %s already contains transition guid %s"), *GetName(), *Guid.ToString());

		GuidNodeMap.Add(Guid, Transition);
		GuidTransitionMap.Add(Guid, Transition);
	}

	for (FSMState_Base* State : StateMachine->GetStates())
	{
		const FGuid& Guid = State->GetGuid();
		/*
		 * Unique GUID check 2:
		 * The PathGuid at this stage should always be unique and the ensure should never be tripped.
		 * The Guid here is calculated based on the path of the node in the state machine which allows multiple same reference calls to exist in the same blueprint.
		 *
		 * If this is triggered please check to make sure the state machine blueprint in question doesn't do anything abnormal such as use circular referencing.
		 */
		ensureMsgf(!GuidNodeMap.Contains(Guid), TEXT("State machine %s already contains state guid %s"), *GetName(), *Guid.ToString());

		GuidNodeMap.Add(Guid, State);
		GuidStateMap.Add(Guid, State);

		if (State->IsStateMachine())
		{
			BuildStateMachineMap((FSMStateMachine*)State, InstancesMapped);
		}
	}
}

bool USMInstance::CheckIsInitialized() const
{
	if (!IsInitialized())
	{
		LD_LOG_WARNING(TEXT("Attempted to use State Machine Instance %s when it wasn't initialized."), *GetName());
		return false;
	}

	if (IsUnreachable())
	{
		// This happens quite a bit in normal practice.
		//LD_LOG_INFO(TEXT("Attempted to use State Machine Instance when it was pending kill or unreachable"));
		return false;
	}

	return true;
}

void USMInstance::UpdateTime()
{
	if (const UWorld* World = GetWorld())
	{
		const float NewTime = bCanTickWhenPaused ? World->GetUnpausedTimeSeconds() : World->GetTimeSeconds();
		WorldTimeDelta = NewTime - WorldSeconds;
		WorldSeconds = NewTime;
	}
	else
	{
		WorldTimeDelta = WorldSeconds = 0.f;
	}
}

void USMInstance::RecordPreviousStateHistory(FSMState_Base* PreviousState)
{
	EXECUTE_ON_PRIMARY(RecordPreviousStateHistory(PreviousState));
	
	if (!PreviousState || StateHistoryMaxCount == 0)
	{
		return;
	}

	const FSMStateHistory StateHistoryInfo
	{
		PreviousState->GetGuid(),
		PreviousState->GetStartTime(),
		PreviousState->GetActiveTime(),
		PreviousState->GetServerTimeInState()
	};
	
	StateHistory.Add(StateHistoryInfo);
	TrimStateHistory();
}

void USMInstance::TrimStateHistory()
{
	EXECUTE_ON_PRIMARY(TrimStateHistory());
	
	const int32 CountToRemove = StateHistory.Num() - StateHistoryMaxCount;
	if (CountToRemove > 0)
	{
		StateHistory.RemoveAt(0, CountToRemove);
	}
}

APlayerController* USMInstance::GetInputController() const
{
	APlayerController* PlayerController = nullptr;
	if (AutoReceiveInput != ESMStateMachineInput::Disabled && GetWorld())
	{
		if (AutoReceiveInput == ESMStateMachineInput::UseContextController)
		{
			PlayerController = USMUtils::FindControllerFromContext<APlayerController>(GetContext());
		}
		else
		{
			const int32 PlayerIndex = static_cast<int32>(AutoReceiveInput.GetValue()) - ESMStateMachineInput::Player0;
			PlayerController = UGameplayStatics::GetPlayerController(this, PlayerIndex);
		}
	}

	return PlayerController;
}

void USMInstance::SetAutoReceiveInput(ESMStateMachineInput::Type InInputType)
{
	AutoReceiveInput = InInputType;
}

void USMInstance::SetInputPriority(int32 InInputPriority)
{
	InputPriority = InInputPriority;
}

void USMInstance::SetBlockInput(bool bNewValue)
{
	bBlockInput = bNewValue;
}

void USMInstance::OnContextPawnRestarted(APawn* Pawn)
{
	if (Pawn)
	{
		USMUtils::HandlePawnControllerChange(Pawn, Pawn->GetController(), this, InputComponent, InputPriority, bBlockInput);
	}
}

const TMap<FGuid, FSMGuidMap>& USMInstance::GetRootPathGuidCache() const
{
	return RootPathGuidCache;
}

void USMInstance::SetRootPathGuidCache(const TMap<FGuid, FSMGuidMap>& InGuidCache)
{
	RootPathGuidCache = InGuidCache;
}

TSharedPtr<FSMCachedPropertyData, ESPMode::ThreadSafe> USMInstance::GetCachedPropertyData()
{
	if (!CachedPropertyData.IsValid())
	{
		CreateCachedPropertyData();
	}
	
	return CachedPropertyData;
}

void USMInstance::CreateCachedPropertyData()
{
	CachedPropertyData = MakeShared<FSMCachedPropertyData, ESPMode::ThreadSafe>();

	TSet<FProperty*> GraphStructPropertiesForStateMachine;
	USMUtils::TryGetGraphPropertiesForClass(GetClass(), GraphStructPropertiesForStateMachine, CachedPropertyData);

	// Map out the graph property guids once so they can be quickly looked up later.
	TMap<FGuid, FSMGraphProperty_Base_Runtime*> MappedGraphPropertyInstances;
	for (FProperty* Prop : GraphStructPropertiesForStateMachine)
	{
		TArray<FSMGraphProperty_Base_Runtime*> GraphPropertyInstances;
		USMUtils::BlueprintPropertyToNativeProperty(Prop, this, GraphPropertyInstances);

		for (FSMGraphProperty_Base_Runtime* FoundInstance : GraphPropertyInstances)
		{
			// MappedGraphPropertyInstances needs to be set per instance/reference since they contain graph property instance data.
			MappedGraphPropertyInstances.Add(FoundInstance->GetGuid(), FoundInstance);
		}
	}

	CachedPropertyData->SetMappedGraphPropertyInstances(MoveTemp(MappedGraphPropertyInstances));
}

void USMInstance::AddNonThreadSafeNode(FSMNode_Base* InNode)
{
	FScopeLock ScopedLock(&CriticalSection);
	NonThreadSafeNodes.Add(InNode);
}

#undef LOCTEXT_NAMESPACE
