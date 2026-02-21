// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMStateMachine.h"
#include "ExposedFunctions/SMExposedFunctionDefines.h"
#include "SMInstance.h"
#include "SMLogging.h"
#include "SMStateMachineInstance.h"
#include "SMTransactions.h"
#include "SMTransition.h"

#define EXECUTE_ON_REFERENCE(function) \
		if (ReferencedStateMachine) \
		{ \
			return ReferencedStateMachine->GetRootStateMachine().function; \
		} \

#define LOGICDRIVER_FUNCTION_HANDLER_TYPE FSMState_FunctionHandlers

FSMStateMachine::FSMStateMachine() : Super(), bHasAdditionalLogic(false), bReuseCurrentState(false),
                                     bOnlyReuseIfNotEndState(false), bAllowIndependentTick(false),
                                     bCallReferenceTickOnManualUpdate(true),
                                     bWaitForEndState(false),
                                     ReferencedStateMachineClass(nullptr),
                                     ReferencedTemplateName(NAME_None),
                                     DynamicStateMachineReferenceVariable(NAME_None), ReferencedStateMachine(nullptr),
                                     IsReferencedByInstance(nullptr),
                                     IsReferencedByStateMachine(nullptr), TimeSpentWaitingForUpdate(0.f),
                                     bWaitingForTransitionUpdate(false), bCanEvaluateTransitions(true),
                                     bCanTakeTransitions(true), bHasReachedEndStateForRun(false)
{
}

void FSMStateMachine::SetNetworkedConditions(TScriptInterface<ISMStateMachineNetworkedInterface> InNetworkInterface,
                                             bool bEvaluateTransitions, bool bTakeTransitions, bool bCanExecuteStateLogic)
{
	NetworkedInterface = InNetworkInterface;
	bCanEvaluateTransitions = bEvaluateTransitions;
	bCanTakeTransitions = bTakeTransitions;

	for (FSMState_Base* State : States)
	{
		State->SetCanExecuteLogic(bCanExecuteStateLogic);
	}
}

void FSMStateMachine::ProcessStates(float DeltaSeconds, bool bForceTransitionEvaluationOnly, const FGuid& InCurrentRunGuid,
	const FStateScopingArgs& InStateScopingArgs)
{
	EXECUTE_ON_REFERENCE(ProcessStates(DeltaSeconds, bForceTransitionEvaluationOnly, InCurrentRunGuid, InStateScopingArgs));

	// Establish a run id unique to this call. This can allow a manual transition evaluation check
	// during an existing ProcessStates call while also preventing stack overflow.
	const bool bInitialRun = !InCurrentRunGuid.IsValid();
	const FGuid CurrentRunGuid = bInitialRun ? FGuid::NewGuid() : InCurrentRunGuid;

	struct FStateTime
	{
		FDateTime StartTime;
		FDateTime EndTime;
	};

	TArray<FSMState_Base*> ActiveStatesCopy = InStateScopingArgs.ScopedToStates.Num() > 0 ? InStateScopingArgs.ScopedToStates : GetActiveStates();
	TMap<FSMState_Base*, FStateTime> ActiveStatesToActiveTime;
	
	auto AddProcessingState = [this, CurrentRunGuid](FSMState_Base* NewProcessingState)
	{
		TSet<FSMState_Base*>& IsProcessing = ProcessingStates.FindOrAdd(CurrentRunGuid);
		if (!IsProcessing.Contains(NewProcessingState))
		{
			IsProcessing.Add(NewProcessingState);
			return true;
		}
		
		return false;
	};

	auto RemoveProcessingState = [this, CurrentRunGuid](const FSMState_Base* ExistingProcessingState)
	{
		TSet<FSMState_Base*>& IsProcessing = ProcessingStates.FindChecked(CurrentRunGuid);
		IsProcessing.Remove(ExistingProcessingState);
	};
	
	auto AddModifiedDateTracking = [&](FSMState_Base* InState)
	{
		ActiveStatesToActiveTime.Add(InState, FStateTime { InState->GetStartTime(), InState->GetEndTime() });
	};
	
	for (FSMState_Base* CurrentState : ActiveStatesCopy)
	{
		AddModifiedDateTracking(CurrentState);
	}

#define CONTINUE() \
	ActiveStatesCopy.RemoveAt(0, 1, false); \
	continue; \
	
	while (ActiveStatesCopy.Num() > 0)
	{
		FSMState_Base* CurrentState = ActiveStatesCopy[0];
		const FStateTime& ModifiedTime = ActiveStatesToActiveTime.FindChecked(CurrentState);

		// Check if the active status has somehow changed during iteration,
		// such as if an event in OnStateBegin triggered a state change.
		if (CurrentState->GetStartTime() != ModifiedTime.StartTime ||
			CurrentState->GetEndTime() != ModifiedTime.EndTime)
		{
			CONTINUE();
		}

		const bool bReentered = CurrentState->HasBeenReenteredFromParallelState(); // Gets cleared in TryStartState.
		
		// Always start the state before attempting a transition.
		bool bSafeToCheckTransitions = InStateScopingArgs.StatesJustStarted.Contains(CurrentState); // False in default behavior, unless manually activating states.
		const bool bStateJustStarted = bSafeToCheckTransitions ? true : TryStartState(CurrentState, &bSafeToCheckTransitions);

		// Parallel re-entry has started, but it may be slated for another update this cycle. Update the current time so it can
		// run its update logic on its next turn.
		if (bStateJustStarted && bReentered && ActiveStatesCopy.FindLast(CurrentState) > 0)
		{
			AddModifiedDateTracking(CurrentState);
		}
		
		if (!bSafeToCheckTransitions)
		{
			CONTINUE();
		}
		
		if (const TSet<FSMState_Base*>* CurrentRun = ProcessingStates.Find(CurrentRunGuid))
		{
			if (CurrentRun->Contains(CurrentState))
			{
				/*
				 * This can occur when there are multiple active states, and the first one transitions and reentries into the next one.
				 * Without this check that would cause an infinite loop. TODO: may not be needed with new iterative approach for 2.6.
				 */
				
				CONTINUE();
			}
		}

		// Evaluate possible transitions and return the best one. If the state machine is waiting, not allowed to evaluate transitions,
		// or this is a normal update and the state isn't allowed to evaluate, then skip evaluation.
		bool bCanCheckTransitions = !(bWaitingForTransitionUpdate || !bCanEvaluateTransitions ||
			(!bForceTransitionEvaluationOnly && !CurrentState->CanEvaluateTransitionsOnTick()));

		if (bCanCheckTransitions && CurrentState->IsStateMachine())
		{
			if (static_cast<FSMStateMachine*>(CurrentState)->bWaitForEndState)
			{
				bCanCheckTransitions = static_cast<FSMStateMachine*>(CurrentState)->IsStateMachineInEndState();
			}
		}
		
		TArray<TArray<FSMTransition*>> ParallelTransitionChains;
		if (bCanCheckTransitions && CurrentState->GetValidTransition(ParallelTransitionChains))
		{
			bool bSuccess = false;
			int32 ActiveIdxToInsert = 1;
			for (const TArray<FSMTransition*>& TransitionChain : ParallelTransitionChains)
			{
				if (TransitionChain.Num() > 0)
				{
					const bool bAddedProcessingState = AddProcessingState(CurrentState);
					FSMState_Base* DestinationState = nullptr;
					if (TryTakeTransitionChain(TransitionChain, DeltaSeconds, bStateJustStarted, &DestinationState))
					{
						check(DestinationState);
						bSuccess = true;
						
						// These destination states will be processed in the order they are discovered and
						// before the original active states are processed.
						ActiveStatesCopy.Insert(DestinationState, ActiveIdxToInsert);
						AddModifiedDateTracking(DestinationState);
						++ActiveIdxToInsert;
					}
					else if (bAddedProcessingState)
					{
						RemoveProcessingState(CurrentState);
					}
				}
			}

			if (bSuccess)
			{
				//  May remain active in which case we should update.
				if (!CurrentState->IsActive())
				{
					CONTINUE();
				}
			}
		}
		
		if (!bStateJustStarted)
		{
			if (bForceTransitionEvaluationOnly)
			{
				// This is an optimized transition evaluation branch. Forward request directly to nested FSM if present.
				if (CurrentState->IsStateMachine())
				{
					static_cast<FSMStateMachine*>(CurrentState)->ProcessStates(DeltaSeconds, bForceTransitionEvaluationOnly, CurrentRunGuid);
				}
			}
			else
			{
				// No transition found, perform general update.
				AddProcessingState(CurrentState);
				CurrentState->UpdateState(DeltaSeconds);
			}
		}

		CONTINUE();
	}

	if (bInitialRun)
	{
		ProcessingStates.Remove(CurrentRunGuid);
	}
}

bool FSMStateMachine::ProcessTransition(FSMTransition* Transition, FSMState_Base* SourceState, FSMState_Base* DestinationState, const FSMTransitionTransaction* Transaction, float DeltaSeconds, FDateTime* CurrentTime)
{
	EXECUTE_ON_REFERENCE(ProcessTransition(Transition, SourceState, DestinationState, Transaction, DeltaSeconds, CurrentTime));

	check(Transition);
	check(SourceState);
	check(DestinationState);
	
	const bool bServerUpdate = Transaction != nullptr;
	const bool bCanTransitionNow = bCanTakeTransitions || bServerUpdate;

	bWaitingForTransitionUpdate = false;
	
	if (!bServerUpdate && IsNetworked())
	{
		// This is a new transition not being supplied by the server.

		FSMTransitionTransaction NewTransition(Transition->GetGuid());
		{
			NewTransition.Timestamp = CurrentTime ? *CurrentTime : FDateTime::UtcNow();

			// Check if source/destination don't match with previous/next states. This implies a longer
			// transition chain. We need to record these values because clients won't be able to calculate
			// them.
			if (SourceState != Transition->GetFromState() || DestinationState != Transition->GetToState())
			{
				NewTransition.AdditionalGuids.Reserve(2);
				NewTransition.AdditionalGuids.Add(SourceState->GetGuid());
				NewTransition.AdditionalGuids.Add(DestinationState->GetGuid());
			}
			// Record the active time plus the current delta since end state hasn't been called yet.
			NewTransition.ActiveTime = bCanTakeTransitions ? SourceState->GetActiveTime() + DeltaSeconds : SM_ACTIVE_TIME_NOT_SET;
		}
		
		Transition->LastNetworkTimestamp = NewTransition.Timestamp;
		Transition->SetServerTimeInState(SM_ACTIVE_TIME_NOT_SET);
		
		// Don't follow this transition a second time.
		if (!bCanTransitionNow)
		{
			bWaitingForTransitionUpdate = true;
		}

		// Notifies server we are taking a new transition. Important to call this before continuing in case
		// the transition entered logic triggers some state change.
		NetworkedInterface->ServerTakeTransition(MoveTemp(NewTransition));
	}
	else if (bServerUpdate && Transaction)
	{
		if (!Transaction->bIsServer)
		{
			Transition->SetServerTimeInState(Transaction->ActiveTime);
		}
		
		Transition->LastNetworkTimestamp = Transaction->Timestamp;
	}

	// If this was called via server the state is likely still active.
	if (bCanTransitionNow)
	{
		FSMState_Base* LastState = Transition->GetFromState();
		FSMState_Base* ToState = Transition->GetToState();
		
		if (LastState->IsActive() && !LastState->bStayActiveOnStateChange)
		{
			LastState->EndState(DeltaSeconds, Transition);
		}

		Transition->SourceState = SourceState;
		Transition->DestinationState = DestinationState;
		
		Transition->TakeTransition();

		ToState->SetPreviousActiveTransition(Transition);
		
		if (USMInstance* Instance = GetOwningInstance())
		{
			Instance->NotifyTransitionTaken(*Transition);
		}

		SetCurrentState(ToState, LastState, SourceState);

		if (!ActiveStates.Contains(ToState))
		{
			const FString InstanceName = GetOwningInstance() ? GetOwningInstance()->GetName() : "Unknown";
			LD_LOG_ERROR(TEXT("Current state not set for state machine node '%s'. The package '%s' may be getting cleaned up. Check your code for proper UE memory management."),
				*GetNodeName(), *InstanceName);
			return false;
		}
		
		ensure(LastState->bStayActiveOnStateChange || !LastState->IsActive());
	}

	return bCanTransitionNow;
}

bool FSMStateMachine::EvaluateAndTakeTransitionChain(FSMTransition* InFirstTransition)
{
	if (!bCanEvaluateTransitions)
	{
		// Not state change authoritative.
		return false;
	}
	
	check(InFirstTransition && InFirstTransition->GetOwnerNode() == this);
	
	if (InFirstTransition->GetFromState()->IsActive())
	{
		TArray<FSMTransition*> Chain;
		if (InFirstTransition->CanTransition(Chain))
		{
			return TakeTransitionChain(Chain);
		}
	}
	
	return false;
}

bool FSMStateMachine::TakeTransitionChain(const TArray<FSMTransition*>& InTransitionChain)
{
	FSMState_Base* DestinationState;
	if (TryTakeTransitionChain(InTransitionChain, 0.f, false, &DestinationState))
	{
		if (bCanTakeTransitions)
		{
			check(DestinationState);
			ProcessStates(0.f, true, FGuid(), { { DestinationState } });
		}
				
		return true;
	}

	return false;
}

bool FSMStateMachine::TryStartState(FSMState_Base* InState, bool* bOutSafeToCheckTransitions)
{
	check(InState);
	if (bOutSafeToCheckTransitions)
	{
		*bOutSafeToCheckTransitions = true;
	}
	bool bStateStarted = false;
	
	if (!InState->IsActive() || InState->HasBeenReenteredFromParallelState())
	{
		// prevents repeated reentry if this state was ending and triggered a transition which led to processing.
		if (InState->IsStateEnding())
		{
			if (bOutSafeToCheckTransitions)
			{
				*bOutSafeToCheckTransitions = false;
			}
			return bStateStarted;
		}

		if (!InState->IsActive() || !InState->HasBeenReenteredFromParallelState() || InState->bAllowParallelReentry)
		{
			InState->StartState();
			bStateStarted = true;
		}
			
		// Prevents repeated reentry with parallel states.
		InState->NotifyOfParallelReentry(false);
			
		// It's possible the current state is null depending on start state's logic (such as if it is shutting down this state machine).
		if (!ActiveStates.Contains(InState) || !InState->bEvalTransitionsOnStart)
		{
			// Don't perform transition evaluation in same tick unless specified.
			if (bOutSafeToCheckTransitions)
			{
				*bOutSafeToCheckTransitions = false;
			}
			return bStateStarted;
		}
	}

	return bStateStarted;
}

bool FSMStateMachine::TryTakeTransitionChain(const TArray<FSMTransition*>& InTransitionChain, float DeltaSeconds,
	bool bStateJustStarted, FSMState_Base** OutDestinationState)
{
	bool bSuccess = false;
	if (OutDestinationState)
	{
		*OutDestinationState = nullptr;
	}
	if (InTransitionChain.Num() > 0)
	{
		// This specific transition doesn't allow same tick eval with start state.
		if (bStateJustStarted && !FSMTransition::CanEvaluateWithStartState(InTransitionChain))
		{
			return bSuccess;
		}
					
		FSMState_Base* SourceState = InTransitionChain[0]->GetFromState();
		FSMState_Base* DestinationState = FSMTransition::GetFinalStateFromChain(InTransitionChain);
		{
			// If the next state is already active the transition may not allow evaluation. Doesn't apply to self transitions.
			if (DestinationState != SourceState && DestinationState->IsActive() && !FSMTransition::CanChainEvalIfNextStateActive(InTransitionChain))
			{
				return bSuccess;
			}
		}
		
		for (FSMTransition* Transition : InTransitionChain)
		{
			const bool bTransitionProcessed = ProcessTransition(Transition, SourceState, DestinationState, nullptr, DeltaSeconds);
			ensure(!bSuccess || bTransitionProcessed); // Every transition in the chain should be processed.
			if (bTransitionProcessed)
			{
				bSuccess = true;
				if (OutDestinationState)
				{
					*OutDestinationState = DestinationState;
				}
			}
		}
	}

	return bSuccess;
}

bool FSMStateMachine::CanProcessExternalTransition() const
{
	return bCanEvaluateTransitions;
}

void FSMStateMachine::SetReuseCurrentState(bool bValue, bool bOnlyWhenNotInEndState)
{
	EXECUTE_ON_REFERENCE(SetReuseCurrentState(bValue, bOnlyWhenNotInEndState));
	
	bReuseCurrentState = bValue;
	bOnlyReuseIfNotEndState = bOnlyWhenNotInEndState;
}

bool FSMStateMachine::CanReuseCurrentState() const
{
	return bReuseCurrentState && (!IsStateMachineInEndState() || !bOnlyReuseIfNotEndState);
}

void FSMStateMachine::SetClassReference(UClass* ClassReference)
{
	ReferencedStateMachineClass = ClassReference;
}

void FSMStateMachine::SetInstanceReference(USMInstance* InstanceReference)
{
	ReferencedStateMachine = InstanceReference;
	if (ReferencedStateMachine)
	{
		// The reference should inherit the reuse state property.
		ReferencedStateMachine->GetRootStateMachine().SetReuseCurrentState(bReuseCurrentState, bOnlyReuseIfNotEndState);
		
		// Only want the top level instance managing ticks.
		ReferencedStateMachine->SetRegisterTick(bAllowIndependentTick);
		ReferencedStateMachine->SetCanEverTick(bAllowIndependentTick);

		ReferencedStateMachine->SetTickOnManualUpdate(bCallReferenceTickOnManualUpdate);

		ReferencedStateMachine->GetRootStateMachine().SetOwnerNode(this);
	}
}

void FSMStateMachine::SetReferencedTemplateName(const FName& Name)
{
	ReferencedTemplateName = Name;
}

void FSMStateMachine::SetReferencedBy(USMInstance* FromInstance, FSMStateMachine* FromStateMachine)
{
	IsReferencedByInstance = FromInstance;
	IsReferencedByStateMachine = FromStateMachine;
}

ISMStateMachineNetworkedInterface* FSMStateMachine::TryGetNetworkInterfaceIfNetworked() const
{
	if (OwningInstance)
	{
		return OwningInstance->TryGetNetworkInterface();
	}

	return nullptr;
}

const TMap<FString, FSMState_Base*>& FSMStateMachine::GetStateNameMap() const
{
	EXECUTE_ON_REFERENCE(GetStateNameMap());
	return StateNameMap;
}

void FSMStateMachine::AddActiveState(FSMState_Base* State)
{
	SetCurrentState(State, nullptr);
}

void FSMStateMachine::RemoveActiveState(FSMState_Base* State)
{
	if (!ContainsActiveState(State))
	{
		return;
	}
	
	State->EndState(0.f);

	const bool bWasInEndState = IsStateMachineInEndState();
	const int32 NumRemoved = ActiveStates.Remove(State);
	const bool bDeactivatedAllStates = NumRemoved > 0 && ActiveStates.Num() == 0;

	if (bDeactivatedAllStates && !bWasInEndState)
	{
		TryNotifyEndStateReached();
	}
	
	if (USMInstance* Instance = GetOwningInstance())
	{
		Instance->NotifyStateChange(nullptr, State);
	}
}

#if WITH_EDITOR
void FSMStateMachine::ResetGeneratedValues()
{
	FSMState_Base::ResetGeneratedValues();

	for (FSMNode_Base* Node : GetAllNodes())
	{
		Node->ResetGeneratedValues();
	}

	ReferencedStateMachine = nullptr;
	IsReferencedByInstance = nullptr;

	EntryStates.Empty();
	States.Empty();
	Transitions.Empty();
}
#endif

void FSMStateMachine::SetCurrentState(FSMState_Base* ToState, FSMState_Base* FromState, FSMState_Base* SourceState)
{
	bool bDeactivatedAllStates = false;
	const bool bWasInEndState = IsStateMachineInEndState();
	if (FromState && !FromState->bStayActiveOnStateChange)
	{
		const int32 NumRemoved = ActiveStates.Remove(FromState);
		bDeactivatedAllStates = NumRemoved > 0 && ActiveStates.Num() == 0;
	}

	if (ToState)
	{
		ToState->SetPreviousActiveState(SourceState ? SourceState : FromState);
		if (ActiveStates.Contains(ToState))
		{
			// Reentered.
			ToState->NotifyOfParallelReentry();
		}
		else
		{
			ActiveStates.Add(ToState);
		}
	}

	// The state machine may have switched to an end state or it may be ending manually.
	if ((ToState
		&& !(ToState->IsStateMachine() && static_cast<FSMStateMachine*>(ToState)->bWaitForEndState)) // Let the sub state machine handle this case
		|| (bDeactivatedAllStates && !bWasInEndState))
	{
		TryNotifyEndStateReached();
	}
	
	if (USMInstance* Instance = GetOwningInstance())
	{
		Instance->NotifyStateChange(ToState, FromState);
	}
}

void FSMStateMachine::TryNotifyEndStateReached()
{
	// Forward up to reference owner sm directly.
	if (IsReferencedByStateMachine)
	{
		return IsReferencedByStateMachine->TryNotifyEndStateReached();
	}
	
	if (IsStateMachineInEndState())
	{
		bHasReachedEndStateForRun = true;

		// Notify instance if applicable.
		if (USMStateMachineInstance* StateInstance = Cast<USMStateMachineInstance>(GetNodeInstance()))
		{
			StateInstance->OnEndStateReached();
		}

		// If we are waiting for an end state then that means any owner sms may also be waiting for an end state.
		// Recursively call owners so they can each check if they are now in an end state and fire off events.
		if (bWaitForEndState)
		{
			if (FSMStateMachine* OwningStateMachine = static_cast<FSMStateMachine*>(GetOwnerNode()))
			{
				OwningStateMachine->TryNotifyEndStateReached();
			}
		}
	}
}

void FSMStateMachine::Initialize(UObject* Instance)
{
	Super::Initialize(Instance);
	
	if (ReferencedStateMachine)
	{
		// Let the instance's state machine we are referencing know they are being referenced.
		ReferencedStateMachine->GetRootStateMachine().SetReferencedBy(Cast<USMInstance>(Instance), this);
	}
	
	for (FSMNode_Base* Node : GetAllNodes())
	{
		Node->Initialize(Instance);
	}
}

void FSMStateMachine::InitializeFunctionHandlers()
{
	INITIALIZE_NODE_FUNCTION_HANDLER();
}

void FSMStateMachine::InitializeGraphFunctions()
{
	FSMState_Base::InitializeGraphFunctions();

	INITIALIZE_EXPOSED_FUNCTIONS(BeginStateGraphEvaluator);
	INITIALIZE_EXPOSED_FUNCTIONS(UpdateStateGraphEvaluator);
	INITIALIZE_EXPOSED_FUNCTIONS(EndStateGraphEvaluator);

	for (FSMNode_Base* Node : GetAllNodes())
	{
		Node->InitializeGraphFunctions();
	}
}

void FSMStateMachine::Reset()
{
	Super::Reset();
	ClearTemporaryInitialStates();
}

void FSMStateMachine::ResetReadStates()
{
	FSMState_Base::ResetReadStates();
	bHasReachedEndStateForRun = false;
}

bool FSMStateMachine::StartState()
{
	if (!Super::StartState())
	{
		return false;
	}

	if (bHasAdditionalLogic)
	{
		if (CanExecuteLogic())
		{
			PrepareGraphExecution();
			EXECUTE_EXPOSED_FUNCTIONS(BeginStateGraphEvaluator);
		}
		
		// The additional logic will call start on the instance.
		if (ReferencedStateMachine)
		{
			return true;
		}
	}
	
	if (ReferencedStateMachine)
	{
		ReferencedStateMachine->Start();
		return true;
	}

	const TArray<FSMState_Base*> InitialStates = GetInitialStates();
	if (!bReuseCurrentState || ActiveStates.Num() == 0)
	{
		for (FSMState_Base* InitialState : InitialStates)
		{
			SetCurrentState(InitialState, nullptr);
		}
		if (HasTemporaryEntryStates())
		{
			ClearTemporaryInitialStates();
		}
	}

	const bool bHasNoStates = InitialStates.Num() == 0 && ActiveStates.Num() == 0;
	
	if (USMStateMachineInstance* Instance = Cast<USMStateMachineInstance>(GetNodeInstance()))
	{
		Instance->OnStateBegin();
	}
	
	ProcessStates(0.f);
	FirePostStartEvents();

	// Check if the state machine has no initial or active states and notify of end state for consistency.
	// Waiting for an end state has always succeeded in this case so the FSM should officially be put into an end state.
	if (bHasNoStates && ActiveStates.Num() == 0 && !HasStateMachineReachedEndStateForRun())
	{
		TryNotifyEndStateReached();
	}
	
	return true;
}

bool FSMStateMachine::UpdateState(float DeltaSeconds)
{
	if (!Super::UpdateState(DeltaSeconds))
	{
		return false;
	}

	if (bHasAdditionalLogic)
	{
		if (CanExecuteLogic())
		{
			EXECUTE_EXPOSED_FUNCTIONS(UpdateStateGraphEvaluator, (void*)&DeltaSeconds);
		}
		
		// The additional logic will call update on the instance.
		if (ReferencedStateMachine)
		{
			return true;
		}
	}
	
	if (ReferencedStateMachine)
	{
		ReferencedStateMachine->RunUpdateAsReference(DeltaSeconds);
		return true;
	}

	if (USMStateMachineInstance* Instance = Cast<USMStateMachineInstance>(GetNodeInstance()))
	{
		Instance->OnStateUpdate(DeltaSeconds);
	}

	ProcessStates(DeltaSeconds);

	return true;
}

bool FSMStateMachine::EndState(float DeltaSeconds, const FSMTransition* TransitionToTake)
{
	if (!Super::EndState(DeltaSeconds, TransitionToTake))
	{
		return false;
	}

	if (bHasAdditionalLogic)
	{
		if (CanExecuteLogic())
		{
			EXECUTE_EXPOSED_FUNCTIONS(EndStateGraphEvaluator);
		}
		
		// The additional logic will call stop on the instance.
		if (ReferencedStateMachine)
		{
			// Outgoing transitions of this container node still need to run.
			ShutdownTransitions();
			return true;
		}
	}
	
	if (ReferencedStateMachine)
	{
		// Manually set transition since Stop won't provide one.
		ReferencedStateMachine->GetRootStateMachine().SetTransitionToTake(TransitionToTake);
		ReferencedStateMachine->Stop();

		// Outgoing transitions of this container node still need to run.
		ShutdownTransitions();
		return true;
	}

	USMStateMachineInstance* Instance = Cast<USMStateMachineInstance>(GetNodeInstance());
	if (Instance)
	{
		Instance->OnStateEnd();
	}

	TArray<FSMState_Base*> ActiveStatesCopy = GetActiveStates();
	for (FSMState_Base* CurrentState: ActiveStatesCopy)
	{
		CurrentState->EndState(DeltaSeconds);

		if (!CanReuseCurrentState())
		{
			SetCurrentState(nullptr, CurrentState);
		}
	}

	if (Instance)
	{
		Instance->OnStateMachineCompleted();
	}

	ShutdownTransitions();

	return true;
}

void FSMStateMachine::ExecuteInitializeNodes()
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

	if (!IsReferencedByInstance)
	{
		// Don't double call this from a reference.
		if (USMStateMachineInstance* StateInstance = Cast<USMStateMachineInstance>(GetNodeInstance()))
		{
			StateInstance->OnStateInitialized();
		}
	}
}

void FSMStateMachine::ExecuteShutdownNodes()
{
	Super::ExecuteShutdownNodes();

	if (NodeInstance)
	{
		NodeInstance->NativeShutdown();
	}
	
	if (!IsReferencedByInstance)
	{
		if (USMStateMachineInstance* StateInstance = Cast<USMStateMachineInstance>(GetNodeInstance()))
		{
			StateInstance->OnStateShutdown();
		}
	}
}

void FSMStateMachine::OnStartedByInstance(USMInstance* Instance)
{
	if (bHasAdditionalLogic)
	{
		Super::OnStartedByInstance(Instance);
	}

	if (!IsReferencedByInstance && !Instance->GetReferenceOwnerConst())
	{
		// Root state machine calls in FSMs only reflect the primary root state machine so only call this if
		// if this node is not a proxy and the owning instance isn't a reference.
		if (USMStateMachineInstance* StateInstance = Cast<USMStateMachineInstance>(GetNodeInstance()))
		{
			StateInstance->OnRootStateMachineStart();
		}
	}
}

void FSMStateMachine::OnStoppedByInstance(USMInstance* Instance)
{
	if (bHasAdditionalLogic)
	{
		Super::OnStoppedByInstance(Instance);
	}

	if (!IsReferencedByInstance && !Instance->GetReferenceOwnerConst())
	{
		// Root state machine calls in FSMs only reflect the primary root state machine so only call this if
		// if this node is not a proxy and the owning instance isn't a reference.
		if (USMStateMachineInstance* StateInstance = Cast<USMStateMachineInstance>(GetNodeInstance()))
		{
			StateInstance->OnRootStateMachineStop();
		}
	}
}

void FSMStateMachine::CalculatePathGuid(TMap<FString, int32>& MappedPaths, bool bUseGuidCache)
{
	Super::CalculatePathGuid(MappedPaths, bUseGuidCache);

	if (ReferencedStateMachine)
	{
		ReferencedStateMachine->GetRootStateMachine().CalculatePathGuid(MappedPaths, bUseGuidCache && !IsDynamicReference());
	}

	TArray<FSMNode_Base*> Nodes = GetAllNodes();
	for (FSMNode_Base* Node : Nodes)
	{
		Node->CalculatePathGuid(MappedPaths, bUseGuidCache);
	}
}

void FSMStateMachine::RunConstructionScripts()
{
	// Do not run for each reference. This is already called
	// for each reference.

	Super::RunConstructionScripts();

	for (FSMNode_Base* Node : GetAllNodes())
	{
		Node->RunConstructionScripts();
	}
}

void FSMStateMachine::NotifyInstanceStateHasStarted()
{
	// Don't double fire.
	if (!IsReferencedByInstance)
	{
		if (USMInstance* Instance = GetOwningInstance())
		{
			Instance->NotifyStateStarted(*this);
		}
	}
}

void FSMStateMachine::AddInitialState(FSMState_Base* State)
{
	EXECUTE_ON_REFERENCE(AddInitialState(State));

	if (State && !States.Contains(State))
	{
		ensureAlwaysMsgf(false, TEXT("Could not set initial state %s. It is not located in state machine %s"), *State->GetNodeName(), *GetNodeName());
		return;
	}

	EntryStates.AddUnique(State);
}

void FSMStateMachine::AddTemporaryInitialState(FSMState_Base* State)
{
	EXECUTE_ON_REFERENCE(AddTemporaryInitialState(State));

	if (!State)
	{
		return;
	}
	
	if (!ensureAlwaysMsgf(States.Contains(State), TEXT("Could not set temporary initial state %s. It is not located in state machine %s"), *State->GetNodeName(), *GetNodeName()))
	{
		return;
	}

	TemporaryEntryStates.AddUnique(State);
}

void FSMStateMachine::ClearTemporaryInitialStates(bool bRecursive)
{
	EXECUTE_ON_REFERENCE(ClearTemporaryInitialStates(bRecursive));
	
	TemporaryEntryStates.Empty();

	if (bRecursive)
	{
		for (FSMState_Base* State : States)
		{
			if (State->IsStateMachine())
			{
				static_cast<FSMStateMachine*>(State)->ClearTemporaryInitialStates(bRecursive);
			}
		}
	}
}

void FSMStateMachine::SetFromTemporaryInitialStates()
{
	EXECUTE_ON_REFERENCE(SetFromTemporaryInitialStates());
	
	for (auto ActiveStateIt = ActiveStates.CreateIterator(); ActiveStateIt;)
	{
		// Active states that won't be active again need to stop.
		if (!TemporaryEntryStates.Contains(*ActiveStateIt))
		{
			RemoveActiveState(*ActiveStateIt);
		}
		else
		{
			++ActiveStateIt;
		}
	}
	
	for (FSMState_Base* TemporaryEntryState : TemporaryEntryStates)
	{
		if (TemporaryEntryState->IsStateMachine())
		{
			static_cast<FSMStateMachine*>(TemporaryEntryState)->SetFromTemporaryInitialStates();
		}
		
		// Temporary states already active can be ignored.
		if (ActiveStates.Contains(TemporaryEntryState))
		{
			continue;
		}

		AddActiveState(TemporaryEntryState);
	}

	ClearTemporaryInitialStates();
}

bool FSMStateMachine::ContainsActiveState(FSMState_Base* StateToCheck) const
{
	EXECUTE_ON_REFERENCE(ContainsActiveState(StateToCheck));
	return ActiveStates.Contains(StateToCheck);
}

bool FSMStateMachine::HasActiveStates() const
{
	EXECUTE_ON_REFERENCE(HasActiveStates());
	return ActiveStates.Num() > 0;
}

const TArray<FSMState_Base*>& FSMStateMachine::GetEntryStates() const
{
	EXECUTE_ON_REFERENCE(GetEntryStates());

	return EntryStates;
}

TArray<FSMState_Base*> FSMStateMachine::GetInitialStates() const
{
	EXECUTE_ON_REFERENCE(GetInitialStates());
	
	return HasTemporaryEntryStates() ? TemporaryEntryStates : EntryStates;
}

FSMState_Base* FSMStateMachine::GetSingleInitialState() const
{
	const TArray<FSMState_Base*> InitialStates = GetInitialStates();
	if (InitialStates.Num() > 0)
	{
		return InitialStates[0];
	}

	return nullptr;
}

FSMState_Base* FSMStateMachine::GetSingleInitialTemporaryState() const
{
	EXECUTE_ON_REFERENCE(GetSingleInitialTemporaryState());
	return HasTemporaryEntryStates() ? TemporaryEntryStates[0] : nullptr;
}

TArray<FSMState_Base*> FSMStateMachine::GetAllNestedInitialTemporaryStates() const
{
	EXECUTE_ON_REFERENCE(GetAllNestedInitialTemporaryStates());

	TArray<FSMState_Base*> OutStates;
	OutStates.Reserve(TemporaryEntryStates.Num());

	for (FSMState_Base* State : TemporaryEntryStates)
	{
		OutStates.Add(State);
		if (State->IsStateMachine())
		{
			OutStates.Append(static_cast<FSMStateMachine*>(State)->GetAllNestedInitialTemporaryStates());
		}
	}

	return OutStates;
}

FSMState_Base* FSMStateMachine::FindState(const FGuid& StateGuid) const
{
	if (GetGuid() == StateGuid)
	{
		return const_cast<FSMStateMachine*>(this);
	}

	if (ReferencedStateMachine != nullptr)
	{
		return ReferencedStateMachine->FindStateByGuid(StateGuid);
	}

	for (FSMState_Base* State : States)
	{
		if (State->GetGuid() == StateGuid)
		{
			return State;
		}

		if (State->IsStateMachine())
		{
			if (FSMState_Base* FoundState = ((FSMStateMachine*)State)->FindState(StateGuid))
			{
				return FoundState;
			}
		}
	}

	return nullptr;
}

bool FSMStateMachine::HasTemporaryEntryStates() const
{
	EXECUTE_ON_REFERENCE(HasTemporaryEntryStates());
	return TemporaryEntryStates.Num() > 0;
}

FSMState_Base* FSMStateMachine::GetSingleActiveState() const
{
	if (FSMState_Base* State = ReferencedStateMachine ? ReferencedStateMachine->GetRootStateMachine().GetSingleActiveState() : nullptr)
	{
		return State;
	}

	for (FSMState_Base* CurrentState : ActiveStates)
	{
		return CurrentState;
	}

	// Temporary state needs to be counted as current if it is set.
	return ReferencedStateMachine ? ReferencedStateMachine->GetRootStateMachine().GetSingleInitialTemporaryState() : GetSingleInitialTemporaryState();
}

TArray<FSMState_Base*> FSMStateMachine::GetActiveStates() const
{
	EXECUTE_ON_REFERENCE(GetActiveStates());
	
	if (HasActiveStates())
	{
		return ActiveStates;
	}
	
	return TemporaryEntryStates;
}

TArray<FSMState_Base*> FSMStateMachine::GetAllNestedActiveStates() const
{
	EXECUTE_ON_REFERENCE(GetAllNestedActiveStates());

	TArray<FSMState_Base*> OutStates = GetActiveStates();

	for (FSMState_Base* State : States)
	{
		if (State->IsStateMachine())
		{
			OutStates.Append(static_cast<FSMStateMachine*>(State)->GetAllNestedActiveStates());
		}
	}

	return OutStates;
}

bool FSMStateMachine::IsInEndState() const
{
	return IsStateMachineInEndState();
}

bool FSMStateMachine::IsEndState() const
{
	// FSM may be waiting to be considered an end state. Needs to use HasReachedEndStateForRun or it could prematurely
	// return true, such as on initial activation before the first state becomes active.
	if (bWaitForEndState && !HasStateMachineReachedEndStateForRun())
	{
		return false;
	}
	return Super::IsEndState();
}

bool FSMStateMachine::IsNodeInstanceClassCompatible(UClass* NewNodeInstanceClass) const
{
	return NewNodeInstanceClass && NewNodeInstanceClass->IsChildOf<USMStateMachineInstance>();
}

USMNodeInstance* FSMStateMachine::GetNodeInstance() const
{
	if (IsReferencedByStateMachine)
	{
		return IsReferencedByStateMachine->GetNodeInstance();
	}

	return Super::GetNodeInstance();
}

USMNodeInstance* FSMStateMachine::GetOrCreateNodeInstance()
{
	if (IsReferencedByStateMachine)
	{
		return IsReferencedByStateMachine->GetOrCreateNodeInstance();
	}
	
	return FSMState_Base::GetOrCreateNodeInstance();
}

UClass* FSMStateMachine::GetDefaultNodeInstanceClass() const
{
	return USMStateMachineInstance::StaticClass();
}

FSMNode_Base* FSMStateMachine::GetOwnerNode() const
{
	if (IsReferencedByStateMachine)
	{
		return IsReferencedByStateMachine;
	}

	return Super::GetOwnerNode();
}

void FSMStateMachine::SetStartTime(const FDateTime& InStartTime)
{
	EXECUTE_ON_REFERENCE(SetStartTime(InStartTime));
	
	for (FSMState_Base* State : GetInitialStates())
	{
		State->SetStartTime(InStartTime);
	}

	Super::SetStartTime(InStartTime);
}

void FSMStateMachine::SetEndTime(const FDateTime& InEndTime)
{
	EXECUTE_ON_REFERENCE(SetEndTime(InEndTime));
	
	for (FSMState_Base* State : GetActiveStates())
	{
		State->SetEndTime(InEndTime);
	}

	Super::SetEndTime(InEndTime);
}

void FSMStateMachine::SetServerTimeInState(float InTime)
{
	EXECUTE_ON_REFERENCE(SetServerTimeInState(InTime));
	
	Super::SetServerTimeInState(InTime);
	
	for (FSMState_Base* State : GetActiveStates())
	{
		State->SetServerTimeInState(InTime);
	}
}

bool FSMStateMachine::IsStateMachineInEndState() const
{
	EXECUTE_ON_REFERENCE(IsStateMachineInEndState());

	for (const FSMState_Base* CurrentState : ActiveStates)
	{
		if (CurrentState->IsEndState())
		{
			return true;
		}
	}

	// TODO: Do we want to change this to be false? Impacts tests and USMInstance::IsInEndState().
	// USMStateMachineInstance::IsStateMachineInEndState()/IsInEndState() should be more commonly used,
	// and the behavior there has never counted empty active states as an end state unless the FSM stopped itself.
	
	// However, internal processing considers deactivating an FSM as triggering an end state: this triggers if a
	// sub-state deactivates itself and there are no other active states in the FSM, and works with bWaitForEndState.
	return ActiveStates.Num() == 0;
}

void FSMStateMachine::AddState(FSMState_Base* State)
{
	State->SetOwnerNode(this);
	States.AddUnique(State);
	StateNameMap.Add(State->GetNodeName(), State);
}

void FSMStateMachine::AddTransition(FSMTransition* Transition)
{
	Transition->SetOwnerNode(this);
	Transitions.AddUnique(Transition);
}

TArray<FSMNode_Base*> FSMStateMachine::GetAllNodes(const FGetNodeArgs& InArgs) const
{
	TArray<FSMNode_Base*> Results;
	Results.Reserve(States.Num() + Transitions.Num());
	Results.Append(States);
	Results.Append(Transitions);

	if (InArgs.bIncludeSelf)
	{
		Results.Add(const_cast<FSMStateMachine*>(this));
	}

	if (InArgs.bIncludeNested)
	{
		for (FSMState_Base* State : States)
		{
			if (State->IsStateMachine())
			{
				const FSMStateMachine* StateMachine = static_cast<FSMStateMachine*>(State);

				if (StateMachine->ReferencedStateMachine)
				{
					Results.Add(&StateMachine->ReferencedStateMachine->GetRootStateMachine());
					if (!InArgs.bSkipReferences)
					{
						Results.Append(StateMachine->ReferencedStateMachine->GetRootStateMachine().GetAllNodes(InArgs));
					}
				}
				else
				{
					Results.Append(static_cast<FSMStateMachine*>(State)->GetAllNodes(InArgs));
				}
			}
		}
	}

	return Results;
}

TArray<FSMNode_Base*> FSMStateMachine::GetAllNodes(bool bIncludeNested, bool bForwardToReference) const
{
	FGetNodeArgs Args;
	Args.bIncludeNested = bIncludeNested;
	return GetAllNodes(Args);
}

const TArray<FSMState_Base*>& FSMStateMachine::GetStates() const
{
	EXECUTE_ON_REFERENCE(GetStates());
	return States;
}

const TArray<FSMTransition*>& FSMStateMachine::GetTransitions() const
{
	EXECUTE_ON_REFERENCE(GetTransitions());
	return Transitions;
}

#undef LOGICDRIVER_FUNCTION_HANDLER_TYPE