// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMTestContext.h"

#include "Configuration/SMProjectEditorSettings.h"
#include "ISMEditorGraphNodeInterface.h"
#include "Utilities/SMBlueprintEditorUtils.h"

float USMTestContext::GreaterThanTest = 5;

void USMTestContext::IncreaseUpdateInt(float Value)
{
	TestUpdateFromDeltaSecondsInt += FMath::RoundToInt(Value);
	TimesUpdateHit.Increase();
}

void USMTestContext::IncreaseTransitionInit()
{
	TestTransitionInit.Increase();
}

void USMTestContext::IncreaseTransitionShutdown()
{
	TestTransitionShutdown.Increase();
}

bool USMStateTestInstance::bTestEditorGuids = true;

void USMStateTestInstance::OnStateBeginEventFunc(USMStateInstance_Base* Instance)
{
	check(Instance == this);
	StateBeginEventHit.Increase();
}

void USMStateTestInstance::OnStateUpdateEventFunc(USMStateInstance_Base* Instance, float DeltaSeconds)
{
	check(Instance == this);
	StateUpdateEventHit.Increase();
}

void USMStateTestInstance::OnStateEndEventFunc(USMStateInstance_Base* Instance)
{
	check(Instance == this);
	StateEndEventHit.Increase();
}

void USMStateTestInstance::OnStateBegin_Implementation()
{
	ExposedInt++;
	StateBeginHit.Increase();
}

void USMStateTestInstance::OnStateUpdate_Implementation(float DeltaSeconds)
{
	StateUpdateHit.Increase();
}

void USMStateTestInstance::OnStateEnd_Implementation()
{
	StateEndHit.Increase();
}

void USMStateTestInstance::OnRootStateMachineStart_Implementation()
{
	StateMachineStartHit.Increase();
	OnStateBeginEvent.AddDynamic(this, &USMStateTestInstance::OnStateBeginEventFunc);
	OnStateUpdateEvent.AddDynamic(this, &USMStateTestInstance::OnStateUpdateEventFunc);
	OnStateEndEvent.AddDynamic(this, &USMStateTestInstance::OnStateEndEventFunc);
}

void USMStateTestInstance::OnRootStateMachineStop_Implementation()
{
	OnStateBeginEvent.RemoveDynamic(this, &USMStateTestInstance::OnStateBeginEventFunc);
	OnStateUpdateEvent.RemoveDynamic(this, &USMStateTestInstance::OnStateUpdateEventFunc);
	OnStateEndEvent.RemoveDynamic(this, &USMStateTestInstance::OnStateEndEventFunc);
	StateMachineStopHit.Increase();
}

void USMStateTestInstance::OnStateInitialized_Implementation()
{
	StateInitializedEventHit.Increase();
}

void USMStateTestInstance::OnStateShutdown_Implementation()
{
	StateShutdownEventHit.Increase();
}

void USMStateTestInstance::ConstructionScript_Implementation()
{
	Super::ConstructionScript_Implementation();
	ConstructionScriptHit.Increase();

	// Test Guid in-editor matches run-time. We can't safely test on legacy construction scripts or if we're a
	// reference since those won't have construction scripts run. During editor time there will be no state machine instance
	// for the top-most state machine.
	if (bTestEditorGuids && FSMBlueprintEditorUtils::GetProjectEditorSettings()->EditorNodeConstructionScriptSetting != ESMEditorConstructionScriptProjectSetting::SM_Legacy &&
		(IsEditorExecution() || (ensure(GetStateMachineInstance()) && GetStateMachineInstance()->IsPrimaryReferenceOwner())))
	{
		if (IsEditorExecution())
		{
			GuidSetFromConstruction = GetGuid();
		}
		else
		{
			const FGuid Guid = GetGuid();
			check(Guid == GuidSetFromConstruction);
		}
	}
}

int32 USMStateArrayTestInstance::ExposedIntArrDefaultValue1 = 152;
int32 USMStateArrayTestInstance::ExposedIntArrDefaultValue2 = 273;

USMStateArrayTestInstance::USMStateArrayTestInstance()
{
	ExposedIntArray.Add(ExposedIntArrDefaultValue1);
	ExposedIntArray.Add(ExposedIntArrDefaultValue2);
}

void USMStateConstructionTestInstance::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (IsNodePinChanging())
	{
		PostEditChangeHit.Increase(false);
	}
}

void USMStateConstructionTestInstance::ConstructionScript_Implementation()
{
	Super::ConstructionScript_Implementation();
	SetByConstructionScript = "Test_" + FString::FromInt(ExposedInt);

	{
		TArray<USMTransitionInstance*> Transitions;
		GetOutgoingTransitions(Transitions);

		CanReadNextStates = Transitions.Num();
	}

	{
		TArray<USMTransitionInstance*> Transitions;
		GetIncomingTransitions(Transitions);

		CanReadPreviousStates = Transitions.Num();
	}
}

const FString USMStateStackConstructionTestInstance::StackName1 = "Name1";
const FString USMStateStackConstructionTestInstance::StackName2 = "Name2";
const FString USMStateStackConstructionTestInstance::StackName3 = "Name3";

void USMStateStackConstructionTestInstance::ConstructionScript_Implementation()
{
	Super::ConstructionScript_Implementation();

	if (IsEditorExecution())
	{
		if (RemoveIndex == -1)
		{
			ClearStateStack();
			checkf(GetStateStackCount() == 0, TEXT("Runtime count incorrect"));
		}
		else if (RemoveIndex >= 0)
		{
			const int32 LastStateStackCount = GetStateStackCount();
			RemoveStateFromStack(RemoveIndex);
			checkf(GetStateStackCount() == LastStateStackCount - 1, TEXT("Runtime count incorrect"));
		}
		else if (GetStateStackCount() == 0)
		{
			{
				USMStateConstructionTestInstance* Instance = CastChecked<USMStateConstructionTestInstance>(AddStateToStack(USMStateConstructionTestInstance::StaticClass()));
				Instance->NameSetByCreator = StackName1;
			}
			{
				USMStateConstructionTestInstance* Instance = CastChecked<USMStateConstructionTestInstance>(AddStateToStack(USMStateConstructionTestInstance::StaticClass(), 0));
				Instance->NameSetByCreator = StackName2;
			}
			{
				USMStateConstructionTestInstance* Instance = CastChecked<USMStateConstructionTestInstance>(AddStateToStack(USMStateConstructionTestInstance::StaticClass()));
				Instance->NameSetByCreator = StackName3;
			}

			checkf(GetStateStackCount() == 3, TEXT("Runtime count incorrect"));
		}

		if (RemoveIndex != -1)
		{
			// Only reset when not adding, otherwise the second pass will add them after clear.
			RemoveIndex = -3;
		}
	}
}

int32 USMStateEditorPropertyResetTestInstance::DefaultIntValue = 2002;

USMStateEditorPropertyResetTestInstance::USMStateEditorPropertyResetTestInstance()
{
	IntVar = DefaultIntValue;
}

void USMTransitionConstructionTestInstance::ConstructionScript_Implementation()
{
	ConstructionScriptHit.Increase();
	SetPriorityOrder(5);
	// Helps test make sure reroute nodes are configured correctly.
	ensure(GetOwningStateMachineNodeInstance() != nullptr);
}

void USMTransitionConstructionTestInstance::OnPreCompileValidate_Implementation(USMCompilerLog* CompilerLog) const
{
	Super::OnPreCompileValidate_Implementation(CompilerLog);
	// Helps test make sure reroute nodes are configured correctly.
	ensure(GetOwningStateMachineNodeInstance() != nullptr);
}

bool USMTransitionConstructionTestInstance::CanEnterTransition_Implementation() const
{
	return true;
}

int32 USMStateReadOnlyTestInstance::DefaultReadOnlyValue = 15132;

USMStateReadOnlyTestInstance::USMStateReadOnlyTestInstance()
{
	ReadOnlyInt = DefaultReadOnlyValue;
}

void USMStateManualTransitionTestInstance::OnStateUpdate_Implementation(float DeltaSeconds)
{
	if (GetTimeInState() > 0.1f)
	{
		EvaluateTransitions();
	}
}

void USMStateEvaluateFromManuallyBoundEventTestInstance::OnStateUpdate_Implementation(float DeltaSeconds)
{
	if (GetTimeInState() > 0.1f)
	{
		if (USMTransitionInstance* Transition = GetTransitionByIndex(0))
		{
			Transition->EvaluateFromManuallyBoundEvent();
		}
	}
}

void USMStateMachineTestInstance::OnStateBegin_Implementation()
{
	StateBeginHit.Increase();
}

void USMStateMachineTestInstance::OnStateUpdate_Implementation(float DeltaSeconds)
{
	StateUpdateHit.Increase();
}

void USMStateMachineTestInstance::OnStateEnd_Implementation()
{
	StateEndHit.Increase();
}

void USMStateMachineTestInstance::OnRootStateMachineStart_Implementation()
{
	RootSMStartHit.Increase();
}

void USMStateMachineTestInstance::OnRootStateMachineStop_Implementation()
{
	RootSMStopHit.Increase();
}

void USMStateMachineTestInstance::OnStateInitialized_Implementation()
{
	InitializeHit.Increase();
}

void USMStateMachineTestInstance::OnStateShutdown_Implementation()
{
	ShutdownHit.Increase();
}

void USMStateMachineTestInstance::OnEndStateReached_Implementation()
{
	EndStateReachedHit.Increase();
}

void USMStateMachineTestInstance::OnStateMachineCompleted_Implementation()
{
	StateMachineCompletedHit.Increase();
}

void USMStateMachineReferenceTestInstance::ConstructionScript_Implementation()
{
	Super::ConstructionScript_Implementation();
	SetByConstructionScript = "Test_" + FString::FromInt(ExposedInt);

	{
		TArray<USMTransitionInstance*> Transitions;
		GetOutgoingTransitions(Transitions);

		CanReadNextStates = Transitions.Num();
	}

	{
		TArray<USMTransitionInstance*> Transitions;
		GetIncomingTransitions(Transitions);

		CanReadPreviousStates = Transitions.Num();
	}
}

void USMStateMachineReferenceTestInstance::OnStateBegin_Implementation()
{
	Super::OnStateBegin_Implementation();
	ExposedInt++;
	
	// We should be a reference but not be the same as the owning state machine instance.
	// Since the test object isn't available just run ensures.
	USMInstance* ReferencedInstance = GetStateMachineReference();
	ensure(ReferencedInstance);
	ensure(ReferencedInstance != GetStateMachineInstance());
}

void USMTransitionTestInstance::OnTransitionEnteredEventFunc(USMTransitionInstance* TransitionInstance)
{
	check(TransitionInstance);
	TransitionEnteredEventHit.Increase();

	// Should always be set at this point.
	ensure(GetSourceStateForActiveTransition());
	ensure(GetDestinationStateForActiveTransition());
}

void USMTransitionTestInstance::OnTransitionInitialized_Implementation()
{
	TransitionInitializedHit.Increase();
	OnTransitionEnteredEvent.AddUniqueDynamic(this, &USMTransitionTestInstance::OnTransitionEnteredEventFunc);
}

void USMTransitionTestInstance::OnTransitionShutdown_Implementation()
{
	TransitionShutdownHit.Increase();
	//OnTransitionEnteredEvent.RemoveDynamic(this, &USMTransitionTestInstance::OnTransitionEnteredEventFunc); Can't remove because this will fire before TransitionEntered.
}

void USMTransitionTestInstance::OnRootStateMachineStart_Implementation()
{
	TransitionRootSMStartHit.Increase();
}

void USMTransitionTestInstance::OnRootStateMachineStop_Implementation()
{
	TransitionRootSMStopHit.Increase();
}

#define LOCTEXT_NAMESPACE "SMTextGraphState"

FText USMTextGraphState::DefaultText = LOCTEXT(TEXTGRAPH_NAMESPACE, "ctor default");

USMTextGraphState::USMTextGraphState()
{
	TextGraph.Result = DefaultText;
}

void USMTextGraphState::OnStateBegin_Implementation()
{
	TextGraph.Execute();
	EvaluatedText = TextGraph.Result;
}

FText USMTextGraphArrayState::DefaultText_1 = LOCTEXT("Elem1", "Array default 1");
FText USMTextGraphArrayState::DefaultText_2 = LOCTEXT("Elem2", "Array default 2");

USMTextGraphArrayState::USMTextGraphArrayState()
{
	{
		FSMTextGraphProperty TextGraphProperty;
		TextGraphProperty.Result = DefaultText_1;
		TextGraphArray.Add(MoveTemp(TextGraphProperty));
	}

	{
		FSMTextGraphProperty TextGraphProperty;
		TextGraphProperty.Result = DefaultText_2;
		TextGraphArray.Add(MoveTemp(TextGraphProperty));
	}
}

#undef LOCTEXT_NAMESPACE

void USMTextGraphArrayState::OnStateBegin_Implementation()
{
	Super::OnStateBegin_Implementation();

	EvaluatedTextArray.Reset(TextGraphArray.Num());
	
	for (FSMTextGraphProperty& TextGraph: TextGraphArray)
	{
		TextGraph.Execute();
		EvaluatedTextArray.Add(TextGraph.Result);
	}
}

USMStateMachineTestComponent::USMStateMachineTestComponent(class FObjectInitializer const& ObjectInitializer) : Super(ObjectInitializer)
{
}

void USMStateMachineTestComponent::SetStateMachineClass(UClass* NewClass)
{
	StateMachineClass = NewClass;
}

void USMStateMachineTestComponent::ClearTemplateInstance()
{
	InstanceTemplate = nullptr;
}

void USMStateMachineTestComponent::SetAllowTick(bool bAllowOverride, bool bCanEverTick)
{
	bOverrideTick_DEPRECATED = bAllowOverride;
	bCanEverTick_DEPRECATED = bCanEverTick;
}

void USMStateMachineTestComponent::SetTickInterval(bool bAllowOverride, float TickInterval)
{
	bOverrideTickInterval_DEPRECATED = bAllowOverride;
	TickInterval_DEPRECATED = TickInterval;
}

void USMStateMachineTestComponent::ImportDeprecatedProperties_Public()
{
	ImportDeprecatedProperties();
}

USMStateIsStoppingTestInstance::USMStateIsStoppingTestInstance()
{
}

void USMStateIsStoppingTestInstance::OnStateEnd_Implementation()
{
	Super::OnStateEnd_Implementation();

	const USMInstance* OwningInstance = GetStateMachineInstance();
	check(OwningInstance);

	ensure((bEnsureIsStoppingOnStateEnd && OwningInstance->IsStopping())
		|| (!bEnsureIsStoppingOnStateEnd && !OwningInstance->IsStopping()));
}

USMTestDialogueInstance::USMTestDialogueInstance()
{
	TextVar = NSLOCTEXT("OurNamespace", "OurKey", "C++ Property Name");
}

FText USMTestDialogueInstance::GetTextFunction() const
{
	return NSLOCTEXT("OurNamespace", "OurKey2", "TextFunction");
}

void USMTestNotifyInstance::OnStateMachineStart_Implementation()
{
	Super::OnStateMachineStart_Implementation();
	LastToStateGuid.Invalidate();
	LastFromStateGuid.Invalidate();
	LastStateStartedGuid.Invalidate();
	LastTransitionGuid.Invalidate();
	TimesStateChangedHit = 0;
	TimesStateStartedHit = 0;
	TimesTransitionTakenHit = 0;
	TimesStateMachineStarted++;
}

void USMTestNotifyInstance::OnStateMachineStop_Implementation()
{
	Super::OnStateMachineStop_Implementation();
	TimesStateMachineStopped++;
}

void USMTestNotifyInstance::OnStateMachineStateChanged_Implementation(const FSMStateInfo& ToState,
                                                                      const FSMStateInfo& FromState)
{
	Super::OnStateMachineStateChanged_Implementation(ToState, FromState);
	
	ensureAlways(!(LastToStateGuid == ToState.Guid && LastFromStateGuid == FromState.Guid));
	LastToStateGuid = ToState.Guid;
	LastFromStateGuid = FromState.Guid;

	TimesStateChangedHit++;
}

void USMTestNotifyInstance::OnStateMachineStateStarted_Implementation(const FSMStateInfo& State)
{
	Super::OnStateMachineStateStarted_Implementation(State);

	if (bTestLastStateStarted)
	{
		ensureAlways(LastStateStartedGuid != State.Guid);
		LastStateStartedGuid = State.Guid;
	}
	
	TimesStateStartedHit++;
}

void USMTestNotifyInstance::OnStateMachineTransitionTaken_Implementation(const FSMTransitionInfo& Transition)
{
	Super::OnStateMachineTransitionTaken_Implementation(Transition);

	ensureAlways(LastTransitionGuid != Transition.Guid);
	LastTransitionGuid = Transition.Guid;

	TimesTransitionTakenHit++;
}

#define TEST_RENAME_NODE() \
{\
const TScriptInterface<ISMEditorGraphNodeInterface> GraphNode = GetOwningEditorGraphNode();\
check(GraphNode.GetObject());\
const FString OldNodeName = GraphNode->GetNodeName();\
FText Error;\
if (bShouldResetName)\
{\
	GraphNode->ResetNodeName();\
}\
else\
{\
	GraphNode->SetNodeName(NewNodeName, Error);\
}\
check(OldNodeName != GraphNode->GetNodeName());\
}

void USMTestRenameState::ConstructionScript_Implementation()
{
	Super::ConstructionScript_Implementation();
	bShouldResetName = false;
	TEST_RENAME_NODE()

	bShouldResetName = true;
	TEST_RENAME_NODE();
}

void USMTestRenameTransition::ConstructionScript_Implementation()
{
	Super::ConstructionScript_Implementation();
	bShouldResetName = false;
	TEST_RENAME_NODE()

	bShouldResetName = true;
	TEST_RENAME_NODE();
}
