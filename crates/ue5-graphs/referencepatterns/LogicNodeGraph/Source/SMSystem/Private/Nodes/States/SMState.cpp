// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMState.h"
#include "SMLogging.h"
#include "SMStateInstance.h"
#include "SMTransition.h"
#include "SMUtils.h"
#include "ExposedFunctions/SMExposedFunctionDefines.h"
#include "SMTransactions.h"

#define LOGICDRIVER_FUNCTION_HANDLER_TYPE FSMState_FunctionHandlers

void FSMState_Base::UpdateReadStates()
{
	Super::UpdateReadStates();
	bIsInEndState = IsEndState();
	bHasUpdated = HasUpdated();
	TimeInState = GetActiveTime();
}

void FSMState_Base::ResetReadStates()
{
	bHasUpdated = false;
	bIsInEndState = false;
	TimeInState = 0.f;
	SetServerTimeInState(SM_ACTIVE_TIME_NOT_SET);
}

FSMState_Base::FSMState_Base() : Super(), bIsRootNode(false), bAlwaysUpdate(false), bEvalTransitionsOnStart(false),
                                 bDisableTickTransitionEvaluation(false), bStayActiveOnStateChange(false),
                                 bAllowParallelReentry(false),
                                 bCanBeEndState(true), bReenteredByParallelState(false), bCanExecuteLogic(true),
                                 bIsStateEnding(false),
                                 PreviousActiveState(nullptr), PreviousActiveTransition(nullptr), StartTime(0),
                                 EndTime(0),
                                 NextTransition(nullptr)
{
	ResetReadStates();
}

void FSMState_Base::Initialize(UObject* Instance)
{
	Super::Initialize(Instance);

	ResetReadStates();
	SortTransitions();
}

void FSMState_Base::InitializeGraphFunctions()
{
	FSMNode_Base::InitializeGraphFunctions();
}

void FSMState_Base::Reset()
{
	Super::Reset();
	ResetReadStates();
}

bool FSMState_Base::IsNodeInstanceClassCompatible(UClass* NewNodeInstanceClass) const
{
	return NewNodeInstanceClass && NewNodeInstanceClass->IsChildOf<USMStateInstance>();
}

UClass* FSMState_Base::GetDefaultNodeInstanceClass() const
{
	return USMStateInstance::StaticClass();
}

void FSMState_Base::ExecuteInitializeNodes()
{
	Super::ExecuteInitializeNodes();
}

void FSMState_Base::GetAllTransitionChains(TArray<FSMTransition*>& OutTransitions) const
{
	for (FSMTransition* Transition : OutgoingTransitions)
	{
		Transition->GetConnectedTransitions(OutTransitions);
	}
}

bool FSMState_Base::StartState()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SMState_Base::StartState"), STAT_SMState_Start, STATGROUP_LogicDriver);

	NextTransition = nullptr;

	if (IsActive() && (!HasBeenReenteredFromParallelState() || !bAllowParallelReentry))
	{
		return false;
	}

	SetStartTime(FDateTime::UtcNow());

	ResetReadStates();

#if WITH_EDITORONLY_DATA
	StartCycle = FPlatformTime::Cycles64();
#endif

	TryResetVariables();

	TryExecuteGraphProperties(GRAPH_PROPERTY_EVAL_ON_START);

	SetActive(true);

	FirePreStartEvents();

	NotifyInstanceStateHasStarted();

	InitializeTransitions();

	return true;
}

bool FSMState_Base::UpdateState(float DeltaSeconds)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SMState_Base::UpdateState"), STAT_SMState_Update, STATGROUP_LogicDriver);

	if (!IsActive())
	{
		return false;
	}

	TimeInState += DeltaSeconds;
	UpdateReadStates();

	TryExecuteGraphProperties(GRAPH_PROPERTY_EVAL_ON_UPDATE);

	if (USMStateInstance_Base* StateInstance = Cast<USMStateInstance_Base>(NodeInstance))
	{
		StateInstance->OnStateUpdateEvent.Broadcast(StateInstance, DeltaSeconds);
	}

	return bHasUpdated = true;
}

bool FSMState_Base::EndState(float DeltaSeconds, const FSMTransition* TransitionToTake)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SMState_Base::EndState"), STAT_SMState_End, STATGROUP_LogicDriver);

	if (!IsActive())
	{
		return false;
	}

	SetEndTime(FDateTime::UtcNow());

	SetTransitionToTake(TransitionToTake);

	if (bAlwaysUpdate && !HasUpdated())
	{
		UpdateState(DeltaSeconds);
	}
	else
	{
		// Record the extra time for accuracy.
		TimeInState += DeltaSeconds;
	}

	UpdateReadStates();

	TryExecuteGraphProperties(GRAPH_PROPERTY_EVAL_ON_END);

	if (USMStateInstance_Base* StateInstance = Cast<USMStateInstance_Base>(NodeInstance))
	{
		StateInstance->OnStateEndEvent.Broadcast(StateInstance);
	}

	SetActive(false);

	return true;
}

void FSMState_Base::OnStartedByInstance(USMInstance* Instance)
{
	// Only execute if allowed and if it's this owning instance starting it.
	// This means reference nodes won't initialize until their owning blueprint is started.
	if (CanExecuteLogic() && Instance == GetOwningInstance())
	{
		UpdateReadStates();

		TryExecuteGraphProperties(GRAPH_PROPERTY_EVAL_ON_ROOT_SM_START);

		EXECUTE_EXPOSED_FUNCTIONS(OnRootStateMachineStartedGraphEvaluator);
	}
}

void FSMState_Base::OnStoppedByInstance(USMInstance* Instance)
{
	// Only execute if allowed and if it's this owning instance.
	if (CanExecuteLogic() && Instance == GetOwningInstance())
	{
		UpdateReadStates();

		TryExecuteGraphProperties(GRAPH_PROPERTY_EVAL_ON_ROOT_SM_STOP);

		EXECUTE_EXPOSED_FUNCTIONS(OnRootStateMachineStoppedGraphEvaluator);
	}
}

bool FSMState_Base::GetValidTransition(TArray<TArray<FSMTransition*>>& Transitions)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SMState_Base::GetValidTransition"), STAT_SMState_GetValidTransition,
	                            STATGROUP_LogicDriver);

	for (FSMTransition* Transition : OutgoingTransitions)
	{
		TArray<FSMTransition*> Chain;
		if (Transition->CanTransition(Chain))
		{
			Transitions.Add(MoveTemp(Chain));

			// Check if blocking or not.
			if (IsConduit() || !Transition->bRunParallel)
			{
				return true;
			}
		}
	}

	return Transitions.Num() > 0;
}

bool FSMState_Base::IsEndState() const
{
	if (!bCanBeEndState)
	{
		return false;
	}

	for (const FSMTransition* Transition : OutgoingTransitions)
	{
		// Look for at least one valid transition.
		if (!Transition->bAlwaysFalse)
		{
			return false;
		}
	}

	return true;
}

bool FSMState_Base::IsInEndState() const
{
	return IsEndState();
}

bool FSMState_Base::HasUpdated() const
{
	return bHasUpdated;
}

void FSMState_Base::SetCanExecuteLogic(bool bValue)
{
	bCanExecuteLogic = bValue;
}

bool FSMState_Base::CanExecuteGraphProperties(uint32 OnEvent, const USMStateInstance_Base* ForTemplate) const
{
	const USMStateInstance_Base* StateInstance = ForTemplate;
	if (StateInstance != nullptr)
	{
		if (!StateInstance->bAutoEvalExposedProperties)
		{
			return false;
		}

		switch (OnEvent)
		{
		case GRAPH_PROPERTY_EVAL_ANY:
			return true;
		case GRAPH_PROPERTY_EVAL_ON_START:
			return StateInstance->bEvalGraphsOnStart;
		case GRAPH_PROPERTY_EVAL_ON_UPDATE:
			return StateInstance->bEvalGraphsOnUpdate;
		case GRAPH_PROPERTY_EVAL_ON_END:
			return StateInstance->bEvalGraphsOnEnd;
		case GRAPH_PROPERTY_EVAL_ON_ROOT_SM_START:
			return StateInstance->bEvalGraphsOnRootStateMachineStart;
		case GRAPH_PROPERTY_EVAL_ON_ROOT_SM_STOP:
			return StateInstance->bEvalGraphsOnRootStateMachineStop;
		}
	}

	return false;
}

bool FSMState_Base::CanEvaluateTransitionsOnTick() const
{
	if (bDisableTickTransitionEvaluation)
	{
		/* Check if any immediate outgoing transition has just completed from an event before returning false. */
		for (FSMTransition* Transition : OutgoingTransitions)
		{
			if (Transition->CanTransitionFromEvent())
			{
				return true;
			}
		}
	}

	return !bDisableTickTransitionEvaluation;
}

void FSMState_Base::SortTransitions()
{
	OutgoingTransitions.Sort([](const FSMTransition& lhs, const FSMTransition& rhs)
	{
		return lhs.Priority < rhs.Priority;
	});

	IncomingTransitions.Sort([](const FSMTransition& lhs, const FSMTransition& rhs)
	{
		return lhs.Priority < rhs.Priority;
	});
}

void FSMState_Base::SetTransitionToTake(const FSMTransition* Transition)
{
	NextTransition = Transition;

	if (NextTransition)
	{
		SetServerTimeInState(NextTransition->GetServerTimeInState());
	}
}

void FSMState_Base::SetPreviousActiveState(FSMState_Base* InPreviousState)
{
	PreviousActiveState = InPreviousState;
}

void FSMState_Base::SetPreviousActiveTransition(FSMTransition* InPreviousTransition)
{
	PreviousActiveTransition = InPreviousTransition;
}

void FSMState_Base::NotifyOfParallelReentry(bool bValue)
{
	bReenteredByParallelState = bValue;
}

void FSMState_Base::SetStartTime(const FDateTime& InStartTime)
{
	StartTime = InStartTime;
}

void FSMState_Base::SetEndTime(const FDateTime& InEndTime)
{
	EndTime = InEndTime;
}

#if WITH_EDITOR
void FSMState_Base::ResetGeneratedValues()
{
	FSMNode_Base::ResetGeneratedValues();

	OutgoingTransitions.Empty();
	IncomingTransitions.Empty();
}
#endif

void FSMState_Base::AddOutgoingTransition(FSMTransition* Transition)
{
	OutgoingTransitions.AddUnique(Transition);
}

void FSMState_Base::AddIncomingTransition(FSMTransition* Transition)
{
	IncomingTransitions.AddUnique(Transition);
}

void FSMState_Base::InitializeTransitions()
{
	ExecuteInitializeNodes();

	TArray<FSMTransition*> AllTransitions;
	GetAllTransitionChains(AllTransitions);

	for (FSMTransition* Transition : AllTransitions)
	{
		Transition->ExecuteInitializeNodes();
	}
}

void FSMState_Base::ShutdownTransitions()
{
	TArray<FSMTransition*> AllTransitions;
	GetAllTransitionChains(AllTransitions);

	for (FSMTransition* Transition : AllTransitions)
	{
		Transition->ExecuteShutdownNodes();
	}

	ExecuteShutdownNodes();
}

void FSMState_Base::NotifyInstanceStateHasStarted()
{
	if (USMInstance* Instance = GetOwningInstance())
	{
		Instance->NotifyStateStarted(*this);
	}
}

void FSMState_Base::FirePreStartEvents()
{
	if (USMStateInstance_Base* StateInstance = Cast<USMStateInstance_Base>(NodeInstance))
	{
		StateInstance->OnStateBeginEvent.Broadcast(StateInstance);
	}
}

void FSMState_Base::FirePostStartEvents()
{
	if (USMStateInstance_Base* StateInstance = Cast<USMStateInstance_Base>(NodeInstance))
	{
		StateInstance->OnPostStateBeginEvent.Broadcast(StateInstance);
	}
}


FSMState::FSMState() : Super()
{
}

void FSMState::Initialize(UObject* Instance)
{
	Super::Initialize(Instance);
}

void FSMState::InitializeFunctionHandlers()
{
	INITIALIZE_NODE_FUNCTION_HANDLER();
}

void FSMState::InitializeGraphFunctions()
{
	FSMState_Base::InitializeGraphFunctions();

	INITIALIZE_EXPOSED_FUNCTIONS(BeginStateGraphEvaluator);
	INITIALIZE_EXPOSED_FUNCTIONS(UpdateStateGraphEvaluator);
	INITIALIZE_EXPOSED_FUNCTIONS(EndStateGraphEvaluator);
}

void FSMState::Reset()
{
	Super::Reset();
}

void FSMState::ExecuteInitializeNodes()
{
	if (IsInitializedForRun())
	{
		return;
	}

	if (NodeInstance)
	{
		NodeInstance->NativeInitialize();
	}

	Super::ExecuteInitializeNodes();

	for (USMNodeInstance* StackInstance : StackNodeInstances)
	{
		if (USMStateInstance* StateInstance = Cast<USMStateInstance>(StackInstance))
		{
			StateInstance->NativeInitialize();
			StateInstance->OnStateInitialized();
		}
	}
}

void FSMState::ExecuteShutdownNodes()
{
	Super::ExecuteShutdownNodes();

	if (NodeInstance)
	{
		NodeInstance->NativeShutdown();
	}

	for (USMNodeInstance* StackInstance : StackNodeInstances)
	{
		if (USMStateInstance* StateInstance = Cast<USMStateInstance>(StackInstance))
		{
			StateInstance->OnStateShutdown();
			StateInstance->NativeShutdown();
		}
	}
}

bool FSMState::StartState()
{
	if (!Super::StartState())
	{
		return false;
	}

	if (CanExecuteLogic())
	{
		PrepareGraphExecution();
		EXECUTE_EXPOSED_FUNCTIONS(BeginStateGraphEvaluator);

		for (USMNodeInstance* StackInstance : StackNodeInstances)
		{
			if (USMStateInstance* StateInstance = Cast<USMStateInstance>(StackInstance))
			{
				StateInstance->OnStateBeginEvent.Broadcast(StateInstance);
				StateInstance->OnStateBegin();
				StateInstance->OnPostStateBeginEvent.Broadcast(StateInstance);
			}
		}
	}

	FirePostStartEvents();

	return true;
}

bool FSMState::UpdateState(float DeltaSeconds)
{
	if (!Super::UpdateState(DeltaSeconds))
	{
		return false;
	}

	if (CanExecuteLogic())
	{
		EXECUTE_EXPOSED_FUNCTIONS(UpdateStateGraphEvaluator, (void*)&DeltaSeconds);

		for (USMNodeInstance* StackInstance : StackNodeInstances)
		{
			if (USMStateInstance* StateInstance = Cast<USMStateInstance>(StackInstance))
			{
				StateInstance->OnStateUpdateEvent.Broadcast(StateInstance, DeltaSeconds);
				StateInstance->OnStateUpdate(DeltaSeconds);
			}
		}
	}

	return true;
}

bool FSMState::EndState(float DeltaSeconds, const FSMTransition* TransitionToTake)
{
	if (!Super::EndState(DeltaSeconds, TransitionToTake))
	{
		return false;
	}

	if (CanExecuteLogic())
	{
		bIsStateEnding = true;
		EXECUTE_EXPOSED_FUNCTIONS(EndStateGraphEvaluator);

		for (USMNodeInstance* StackInstance : StackNodeInstances)
		{
			if (USMStateInstance* StateInstance = Cast<USMStateInstance>(StackInstance))
			{
				StateInstance->OnStateEndEvent.Broadcast(StateInstance);
				StateInstance->OnStateEnd();
			}
		}
		bIsStateEnding = false;
	}

	ShutdownTransitions();

	return true;
}

bool FSMState::TryExecuteGraphProperties(uint32 OnEvent)
{
	bool bResult = Super::TryExecuteGraphProperties(OnEvent);

	for (USMNodeInstance* StackInstance : StackNodeInstances)
	{
		if (USMStateInstance* StateInstance = Cast<USMStateInstance>(StackInstance))
		{
			if (CanExecuteGraphProperties(OnEvent, StateInstance))
			{
				ExecuteGraphProperties(StateInstance, &StateInstance->GetTemplateGuid());
				bResult = true;
			}
		}
	}

	return bResult;
}

void FSMState::OnStartedByInstance(USMInstance* Instance)
{
	Super::OnStartedByInstance(Instance);
	for (USMNodeInstance* StackInstance : StackNodeInstances)
	{
		if (USMStateInstance* StateInstance = Cast<USMStateInstance>(StackInstance))
		{
			StateInstance->OnRootStateMachineStart();
		}
	}
}

void FSMState::OnStoppedByInstance(USMInstance* Instance)
{
	Super::OnStoppedByInstance(Instance);
	for (USMNodeInstance* StackInstance : StackNodeInstances)
	{
		if (USMStateInstance* StateInstance = Cast<USMStateInstance>(StackInstance))
		{
			StateInstance->OnRootStateMachineStop();
		}
	}
}

#undef LOGICDRIVER_FUNCTION_HANDLER_TYPE
