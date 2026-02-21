// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Helpers/SMTestBoilerplate.h"
#include "SMTestContext.h"
#include "SMTestHelpers.h"

#include "Blueprints/SMBlueprint.h"
#include "Blueprints/SMBlueprintGeneratedClass.h"

#include "Blueprints/SMBlueprintFactory.h"
#include "Configuration/SMProjectEditorSettings.h"
#include "Graph/Nodes/FunctionNodes/SMGraphK2Node_FunctionNodes_TransitionEvent.h"
#include "Graph/Nodes/FunctionNodes/SMGraphK2Node_StateWriteNodes.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_TransitionEnteredNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_TransitionInitializedNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_TransitionPostEvaluateNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_TransitionPreEvaluateNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_TransitionShutdownNode.h"
#include "Graph/Nodes/SMGraphK2Node_StateMachineNode.h"
#include "Graph/Nodes/SMGraphNode_RerouteNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineEntryNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineStateNode.h"
#include "Graph/Nodes/SMGraphNode_StateNode.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/SMGraph.h"
#include "Graph/SMStateGraph.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "K2Node_CallFunction.h"
#include "Kismet2/KismetEditorUtilities.h"

#if WITH_DEV_AUTOMATION_TESTS

#if PLATFORM_DESKTOP

/**
 * Disable tick on a state and manually evaluate from the state instance.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStateManualTransitionTest, "LogicDriver.Transitions.ManualTransition.EvaluateTransitions", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FStateManualTransitionTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(2)
	UEdGraphPin* LastStatePin = nullptr;

	// Test manually calling EvaluateTransitions
	{
		TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin);

		USMGraphNode_StateNode* FirstStateNode = CastChecked<USMGraphNode_StateNode>(StateMachineGraph->GetEntryNode()->GetOutputNode());
		FirstStateNode->GetNodeTemplateAs<USMStateInstance_Base>(true)->SetDisableTickTransitionEvaluation(true);

		int32 EntryHits = 0; int32 UpdateHits = 0; int32 EndHits = 0;
		const int32 MaxIterations = 3;
		USMInstance* Instance = TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits, MaxIterations, false, false);

		TestTrue("State machine still active", Instance->IsActive());
		TestTrue("State machine shouldn't have been able to switch states.", !Instance->IsInEndState());

		TestEqual("State Machine generated value", EntryHits, 1);
		TestEqual("State Machine generated value", UpdateHits, MaxIterations);
		TestEqual("State Machine generated value", EndHits, 0);

		USMStateInstance* StateInstance = CastChecked<USMStateInstance>(Instance->GetSingleActiveState()->GetNodeInstance());

		// Test DoesTransitionPass API
		{
			USMTransitionInstance* NextTransition = StateInstance->GetTransitionByIndex(0);
			check(NextTransition);

			TestTrue("DoesTransitionPass works and doesn't explode!", NextTransition->DoesTransitionPass());
		}

		StateInstance->EvaluateTransitions();

		TestTrue("State machine should have now switched states.", Instance->IsInEndState());
		TestTrue("State machine should still be active.", Instance->IsActive());

		USMTestContext* Context = CastChecked<USMTestContext>(StateInstance->GetContext());
		TestEqual("Update should NOT have been called from manual transition evaluation.", Context->GetUpdateFromDeltaSecondsInt(), MaxIterations);
	}

	// Test the state calling EvaluateTransitions
	{
		LastStatePin = nullptr;
		FSMBlueprintEditorUtils::RemoveAllNodesFromGraph(StateMachineGraph, NewBP);
		
		TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMStateManualTransitionTestInstance::StaticClass());

		USMGraphNode_StateNode* FirstStateNode = CastChecked<USMGraphNode_StateNode>(StateMachineGraph->GetEntryNode()->GetOutputNode());
		FirstStateNode->GetNodeTemplateAs<USMStateInstance_Base>(true)->SetDisableTickTransitionEvaluation(true);

		int32 EntryHits = 0; int32 UpdateHits = 0; int32 EndHits = 0;
		TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits);
	}
	
	return true;
}

/**
 * Disable tick on a state and have the state call EvaluateFromManuallyBoundEvent on it's outgoing transition.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEvaluateFromManuallyBoundEventTest, "LogicDriver.Transitions.ManualTransition.EvaluateFromManuallyBoundEvent", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FEvaluateFromManuallyBoundEventTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(2)
	UEdGraphPin* LastStatePin = nullptr;

	// Test manually calling EvaluateFromManuallyBoundEvent
	{
		TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin);

		USMGraphNode_StateNode* FirstStateNode = CastChecked<USMGraphNode_StateNode>(StateMachineGraph->GetEntryNode()->GetOutputNode());
		FirstStateNode->GetNodeTemplateAs<USMStateInstance_Base>(true)->SetDisableTickTransitionEvaluation(true);

		int32 EntryHits = 0; int32 UpdateHits = 0; int32 EndHits = 0;
		const int32 MaxIterations = 3;
		USMInstance* Instance = TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits, MaxIterations, false, false);

		TestTrue("State machine still active", Instance->IsActive());
		TestTrue("State machine shouldn't have been able to switch states.", !Instance->IsInEndState());

		TestEqual("State Machine generated value", EntryHits, 1);
		TestEqual("State Machine generated value", UpdateHits, MaxIterations);
		TestEqual("State Machine generated value", EndHits, 0);

		USMStateInstance* StateInstance = CastChecked<USMStateInstance>(Instance->GetSingleActiveState()->GetNodeInstance());

		// Test DoesTransitionPass API
		USMTransitionInstance* NextTransition = StateInstance->GetTransitionByIndex(0);
		check(NextTransition);

		TestTrue("DoesTransitionPass works and doesn't explode!", NextTransition->DoesTransitionPass());
		NextTransition->EvaluateFromManuallyBoundEvent();

		TestTrue("State machine should have now switched states.", Instance->IsInEndState());
		TestTrue("State machine should still be active.", Instance->IsActive());

		USMTestContext* Context = CastChecked<USMTestContext>(StateInstance->GetContext());
		TestEqual("Update should NOT have been called from manual transition evaluation.", Context->GetUpdateFromDeltaSecondsInt(), MaxIterations);
	}

	// Test the state's next transition calling EvaluateFromManuallyBoundEvent
	{
		LastStatePin = nullptr;
		FSMBlueprintEditorUtils::RemoveAllNodesFromGraph(StateMachineGraph, NewBP);

		TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMStateEvaluateFromManuallyBoundEventTestInstance::StaticClass());

		USMGraphNode_StateNode* FirstStateNode = CastChecked<USMGraphNode_StateNode>(StateMachineGraph->GetEntryNode()->GetOutputNode());
		FirstStateNode->GetNodeTemplateAs<USMStateInstance_Base>(true)->SetDisableTickTransitionEvaluation(true);

		int32 EntryHits = 0; int32 UpdateHits = 0; int32 EndHits = 0;
		TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits);
	}

	return true;
}

/**
 * Test CanEvaluate of transitions.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransitionCanEvaluateTest, "LogicDriver.Transitions.CanEvaluate", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTransitionCanEvaluateTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST_NO_STATES()
	UEdGraphPin* LastStatePin = nullptr;
	
	// Build a state machine of only two states.
	{
		const int32 CurrentStates = 2;
		TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, CurrentStates, &LastStatePin);
		if (!NewAsset.SaveAsset(this))
		{
			return false;
		}
	}

	{
		USMGraphNode_TransitionEdge* TransitionEdge =
			CastChecked<USMGraphNode_TransitionEdge>(Cast<USMGraphNode_StateNode>(LastStatePin->GetOwningNode())->GetInputPin()->LinkedTo[0]->GetOwningNode());

		UK2Node_CallFunction* PreEvalNode = TestHelpers::AddEventWithLogic<USMGraphK2Node_TransitionPreEvaluateNode>(this, TransitionEdge,
			USMTestContext::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMTestContext, IncreaseTransitionPreEval)));

		TestHelpers::AddEventWithLogic<USMGraphK2Node_TransitionPostEvaluateNode>(this, TransitionEdge,
			USMTestContext::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMTestContext, IncreaseTransitionPostEval)));

		USMGraphK2Node_StateWriteNode_CanEvaluate* SetCanEvaluate = TestHelpers::CreateNewNode<USMGraphK2Node_StateWriteNode_CanEvaluate>(this,
			TransitionEdge->GetBoundGraph(), PreEvalNode->GetThenPin(), false);

		TestNotNull("Can Evaluate Write not should exist", SetCanEvaluate);
		if (!SetCanEvaluate)
		{
			return false;
		}

		// First run will never swith states.
		{
			FKismetEditorUtilities::CompileBlueprint(NewBP);

			// Create a context we will run the state machine for.
			USMTestContext* Context = NewObject<USMTestContext>();
			USMInstance* StateMachineInstance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);

			// Test initial values

			TestEqual("PreEvalValue", Context->TestTransitionPreEval.Count, 0);
			TestEqual("PostEvalValue", Context->TestTransitionPostEval.Count, 0);

			// Test in start state.

			StateMachineInstance->Start();
			TestTrue("State Machine should have started", StateMachineInstance->IsActive());

			TestEqual("PreEvalValue", Context->TestTransitionPreEval.Count, 0);
			TestEqual("PostEvalValue", Context->TestTransitionPostEval.Count, 0);

			// Test pre/post eval after trying to change. But eval is never run.

			TArray<TArray<FSMTransition*>> TransitionChain;
			const bool bFoundTransition = StateMachineInstance->GetRootStateMachine().GetSingleActiveState()->GetValidTransition(TransitionChain);

			TestFalse("No valid transition should exist", bFoundTransition);
			TestEqual("PreEvalValue", Context->TestTransitionPreEval.Count, 1);
			TestEqual("PostEvalValue", Context->TestTransitionPostEval.Count, 1);

			TestTrue("PostEval should have occurred after PreEval", Context->TestTransitionPreEval.TimeStamp < Context->TestTransitionPostEval.TimeStamp);

			StateMachineInstance->Shutdown();
		}

		SetCanEvaluate->GetInputPin()->DefaultValue = "true";

		// Second run should work normally.
		{
			FKismetEditorUtilities::CompileBlueprint(NewBP);

			// Create a context we will run the state machine for.
			USMTestContext* Context = NewObject<USMTestContext>();
			USMInstance* StateMachineInstance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);

			// Test initial values

			TestEqual("PreEvalValue", Context->TestTransitionPreEval.Count, 0);
			TestEqual("PostEvalValue", Context->TestTransitionPostEval.Count, 0);

			// Test in start state.

			StateMachineInstance->Start();
			TestTrue("State Machine should have started", StateMachineInstance->IsActive());

			TestEqual("PreEvalValue", Context->TestTransitionPreEval.Count, 0);
			TestEqual("PostEvalValue", Context->TestTransitionPostEval.Count, 0);

			// Test pre/post eval after trying to change. But eval is never run.

			TArray<TArray<FSMTransition*>> TransitionChain;
			const bool bFoundTransition = StateMachineInstance->GetRootStateMachine().GetSingleActiveState()->GetValidTransition(TransitionChain);
			TestTrue("Transition found", bFoundTransition);
			FSMTransition* ValidTransition = TransitionChain[0][0];

			TestNotNull("Transition should evaluate", ValidTransition);
			TestEqual("PreEvalValue", Context->TestTransitionPreEval.Count, 1);
			TestEqual("PostEvalValue", Context->TestTransitionPostEval.Count, 1);

			TestTrue("PostEval should have occurred after PreEval", Context->TestTransitionPreEval.TimeStamp < Context->TestTransitionPostEval.TimeStamp);

			StateMachineInstance->Shutdown();
		}
	}

	return NewAsset.DeleteAsset(this);
}

/**
 * Disable tick on a state machine and make sure Eval Transition on Start functions.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEvalTransitionsOnStart, "LogicDriver.Transitions.EvalTransitionsOnStart", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FEvalTransitionsOnStart::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(3)
	UEdGraphPin* LastStatePin = nullptr;

	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin);

	TArray<USMGraphNode_StateNodeBase*> States;
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested(NewBP, States);

	for (const USMGraphNode_StateNodeBase* State : States)
	{
		if (State->IsA<USMGraphNode_StateMachineEntryNode>())
		{
			continue;
		}
		State->GetNodeTemplateAs<USMStateInstance_Base>(true)->SetEvalTransitionsOnStart(true);
	}

	CastChecked<USMInstance>(NewBP->GetGeneratedClass()->ClassDefaultObject)->SetCanEverTick(false);

	// Verify everything runs in 1 tick
	{
		int32 EntryHits = 0; int32 UpdateHits = 0; int32 EndHits = 0;
		const int32 MaxIterations = 1;
		USMInstance* Instance = TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits, MaxIterations);
	}

	// From the initial state, manually switch to the next state through switch to linked state and verify everything completes
	{
		const USMGraphNode_StateNode* FirstStateNode = CastChecked<USMGraphNode_StateNode>(StateMachineGraph->GetEntryNode()->GetOutputNode());
		FirstStateNode->GetNodeTemplateAs<USMStateInstance_Base>(true)->SetEvalTransitionsOnStart(false);

		FKismetEditorUtilities::CompileBlueprint(NewBP);

		USMInstance* Instance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, NewObject<USMTestContext>());
		Instance->Start();

		TArray<USMStateInstance_Base*> EntryStates;
		Instance->GetRootStateMachineNodeInstance()->GetEntryStates(EntryStates);
		check(EntryStates.Num());

		USMStateInstance_Base* FirstState = Instance->GetSingleActiveStateInstance();
		TestEqual("Initial state active", FirstState, EntryStates[0]);
		TestFalse("State machine not in end state", Instance->IsInEndState());

		FirstState->SwitchToLinkedState(FirstState->GetNextStateByTransitionIndex(0), false, true);

		TestTrue("State machine in end state", Instance->IsInEndState());
	}

	// From the initial state, manually switch to the next state through the SMInstance and verify everything completes
	{
		const USMGraphNode_StateNode* FirstStateNode = CastChecked<USMGraphNode_StateNode>(StateMachineGraph->GetEntryNode()->GetOutputNode());
		FirstStateNode->GetNodeTemplateAs<USMStateInstance_Base>(true)->SetEvalTransitionsOnStart(false);

		FKismetEditorUtilities::CompileBlueprint(NewBP);

		USMInstance* Instance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, NewObject<USMTestContext>());
		Instance->Start();

		TArray<USMStateInstance_Base*> EntryStates;
		Instance->GetRootStateMachineNodeInstance()->GetEntryStates(EntryStates);
		check(EntryStates.Num());

		USMStateInstance_Base* FirstState = Instance->GetSingleActiveStateInstance();
		TestEqual("Initial state active", FirstState, EntryStates[0]);
		TestFalse("State machine not in end state", Instance->IsInEndState());

		Instance->SwitchActiveState(FirstState->GetNextStateByTransitionIndex(0));

		TestTrue("State machine in end state", Instance->IsInEndState());
	}

	// Event based transitions with bEvalTransitionsOnStart are tested in both other automation test and functional tests.

	return true;
}

/**
 * Test optional transition event nodes.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransitionOptionalEventNodesTest, "LogicDriver.Transitions.OptionalEventNodes", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTransitionOptionalEventNodesTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST_NO_STATES()

	// Total states to test.
	UEdGraphPin* LastStatePin = nullptr;

	// Build a state machine of only two states.
	{
		const int32 CurrentStates = 2;
		TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, CurrentStates, &LastStatePin);
		if (!NewAsset.SaveAsset(this))
		{
			return false;
		}
	}

	{
		USMGraphNode_TransitionEdge* TransitionEdge =
			CastChecked<USMGraphNode_TransitionEdge>(Cast<USMGraphNode_StateNode>(LastStatePin->GetOwningNode())->GetInputPin()->LinkedTo[0]->GetOwningNode());

		TestHelpers::AddEventWithLogic<USMGraphK2Node_TransitionInitializedNode>(this, TransitionEdge,
			USMTestContext::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMTestContext, IncreaseTransitionInit)));

		TestHelpers::AddEventWithLogic<USMGraphK2Node_TransitionShutdownNode>(this, TransitionEdge,
			USMTestContext::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMTestContext, IncreaseTransitionShutdown)));

		TestHelpers::AddEventWithLogic<USMGraphK2Node_TransitionPreEvaluateNode>(this, TransitionEdge,
			USMTestContext::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMTestContext, IncreaseTransitionPreEval)));

		TestHelpers::AddEventWithLogic<USMGraphK2Node_TransitionPostEvaluateNode>(this, TransitionEdge,
			USMTestContext::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMTestContext, IncreaseTransitionPostEval)));

		TestHelpers::AddEventWithLogic<USMGraphK2Node_TransitionEnteredNode>(this, TransitionEdge,
			USMTestContext::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMTestContext, IncreaseTransitionTaken)));

		FKismetEditorUtilities::CompileBlueprint(NewBP);

		// Create a context we will run the state machine for.
		USMTestContext* Context = NewObject<USMTestContext>();
		USMInstance* StateMachineInstance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);

		// Test initial values

		TestEqual("InitValue", Context->TestTransitionInit.Count, 0);
		TestEqual("ShutdownValue", Context->TestTransitionShutdown.Count, 0);
		TestEqual("PreEvalValue", Context->TestTransitionPreEval.Count, 0);
		TestEqual("PostEvalValue", Context->TestTransitionPostEval.Count, 0);
		TestEqual("TransitionEntered", Context->TestTransitionEntered.Count, 0);

		// Test in start state.

		StateMachineInstance->Start();
		TestTrue("State Machine should have started", StateMachineInstance->IsActive());

		TestEqual("InitValue", Context->TestTransitionInit.Count, 1);
		TestEqual("ShutdownValue", Context->TestTransitionShutdown.Count, 0);
		TestEqual("PreEvalValue", Context->TestTransitionPreEval.Count, 0);
		TestEqual("PostEvalValue", Context->TestTransitionPostEval.Count, 0);
		TestEqual("TransitionEntered", Context->TestTransitionEntered.Count, 0);

		// Test pre/post eval
		TArray<TArray<FSMTransition*>> TransitionChain;
		const bool bFoundTransition = StateMachineInstance->GetRootStateMachine().GetSingleActiveState()->GetValidTransition(TransitionChain);
		TestTrue("Transition found", bFoundTransition);
		FSMTransition* ValidTransition = TransitionChain[0][0];

		TestEqual("InitValue", Context->TestTransitionInit.Count, 1);
		TestEqual("ShutdownValue", Context->TestTransitionShutdown.Count, 0);
		TestEqual("PreEvalValue", Context->TestTransitionPreEval.Count, 1);
		TestEqual("PostEvalValue", Context->TestTransitionPostEval.Count, 1);
		TestEqual("TransitionEntered", Context->TestTransitionEntered.Count, 0);

		TestTrue("PostEval should have occurred after PreEval", Context->TestTransitionPreEval.TimeStamp < Context->TestTransitionPostEval.TimeStamp);

		// Test after taking the transition.

		StateMachineInstance->GetRootStateMachine().ProcessTransition(ValidTransition, ValidTransition->GetFromState(), ValidTransition->GetToState(), nullptr, 1.f);

		TestEqual("InitValue", Context->TestTransitionInit.Count, 1);
		TestEqual("ShutdownValue", Context->TestTransitionShutdown.Count, 1);
		TestEqual("PreEvalValue", Context->TestTransitionPreEval.Count, 1);
		TestEqual("PostEvalValue", Context->TestTransitionPostEval.Count, 1);
		TestEqual("TransitionEntered", Context->TestTransitionEntered.Count, 1);

		TestTrue("TransitionEntered should have occurred after Shutdown", Context->TestTransitionShutdown.TimeStamp < Context->TestTransitionEntered.TimeStamp);

		// Should shut down the state machine now.
		StateMachineInstance->Update(1.f);

		TestTrue("State Machine should be in end state", StateMachineInstance->IsInEndState());

		TestEqual("InitValue", Context->TestTransitionInit.Count, 1);
		TestEqual("ShutdownValue", Context->TestTransitionShutdown.Count, 1);
		TestEqual("PreEvalValue", Context->TestTransitionPreEval.Count, 1);
		TestEqual("PostEvalValue", Context->TestTransitionPostEval.Count, 1);
		TestEqual("TransitionEntered", Context->TestTransitionEntered.Count, 1);

		StateMachineInstance->Shutdown();
		TestFalse("State Machine should have stopped", StateMachineInstance->IsActive());
	}

	return NewAsset.DeleteAsset(this);
}

/**
 * Test automatically binding to a multi-cast delegate on the context.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransitionEventAutoBindContextUpdateTest, "LogicDriver.Transitions.Events.AutoBind.Update", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTransitionEventAutoBindContextUpdateTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST_NO_STATES()

	// Total states to test.
	UEdGraphPin* LastStatePin = nullptr;
	
	const int32 CurrentStates = 3;
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, CurrentStates, &LastStatePin);

	USMGraphNode_TransitionEdge* TransitionEdge =
		CastChecked<USMGraphNode_TransitionEdge>(CastChecked<USMGraphNode_StateNode>(StateMachineGraph->GetEntryNode()->GetOutputNode())->GetNextTransition());
	TransitionEdge->GetNodeTemplateAs<USMTransitionInstance>()->SetCanEvaluate(false);

	USMGraphNode_StateNodeBase* MiddleState = CastChecked<USMGraphNode_StateNode>(StateMachineGraph->GetEntryNode()->GetOutputNode())->GetNextState();
	
	USMGraphK2Node_StateWriteNode_TransitionEventReturn* EventTriggerNode = nullptr;
	
	{
		TransitionEdge->DelegateOwnerInstance = SMDO_Context;
		TransitionEdge->DelegateOwnerClass = USMTestContext::StaticClass();
		TransitionEdge->DelegatePropertyName = GET_MEMBER_NAME_CHECKED(USMTestContext, TransitionEvent);

		TransitionEdge->InitTransitionDelegate();

		TArray<USMGraphK2Node_StateWriteNode_TransitionEventReturn*> EventReturn;
		FSMBlueprintEditorUtils::GetAllNodesOfClassNested(TransitionEdge->GetTransitionGraph(), EventReturn);
		check(EventReturn.Num() == 1);

		EventTriggerNode = EventReturn[0];
		check(EventTriggerNode);
	}
	
	// Event binding no targeted update.
	{
		EventTriggerNode->bEventTriggersTargetedUpdate = false;
		EventTriggerNode->bEventTriggersFullUpdate = false;
		
		FKismetEditorUtilities::CompileBlueprint(NewBP);

		// Create a context we will run the state machine for.
		USMTestContext* Context = NewObject<USMTestContext>();
		USMInstance* Instance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);

		Instance->Start();
		Instance->Update(0.f);
		TestEqual("State machine still in initial state", Instance->GetRootStateMachine().GetSingleActiveState(), Instance->GetRootStateMachine().GetSingleInitialState());
		TestFalse("State machine shouldn't have switched states due to transition evaluation being false.", Instance->IsInEndState());
		
		Context->TransitionEvent.Broadcast();
		TestEqual("State machine hasn't switched states", Instance->GetRootStateMachine().GetSingleActiveState(), Instance->GetRootStateMachine().GetSingleInitialState());
		Instance->EvaluateTransitions();
		
		TestNotEqual("State machine switched states", Instance->GetRootStateMachine().GetSingleActiveState(), Instance->GetRootStateMachine().GetSingleInitialState());
		
		TestFalse("State machine not in end state.", Instance->IsInEndState());
	}

	// Full update
	{
		EventTriggerNode->bEventTriggersTargetedUpdate = false;
		EventTriggerNode->bEventTriggersFullUpdate = true;
		
		FKismetEditorUtilities::CompileBlueprint(NewBP);

		// Create a context we will run the state machine for.
		USMTestContext* Context = NewObject<USMTestContext>();
		USMInstance* Instance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);

		Instance->Start();
		Instance->Update(0.f);
		TestEqual("State machine still in initial state", Instance->GetRootStateMachine().GetSingleActiveState(), Instance->GetRootStateMachine().GetSingleInitialState());
		TestFalse("State machine shouldn't have switched states due to transition evaluation being false.", Instance->IsInEndState());
		
		Context->TransitionEvent.Broadcast();
		TestNotEqual("State machine has switched states", Instance->GetRootStateMachine().GetSingleActiveState(), Instance->GetRootStateMachine().GetSingleInitialState());

		TestFalse("State machine not in end state.", Instance->IsInEndState());
	}

	// Targeted + Full update.
	{
		EventTriggerNode->bEventTriggersTargetedUpdate = true;
		EventTriggerNode->bEventTriggersFullUpdate = true;
		
		FKismetEditorUtilities::CompileBlueprint(NewBP);

		// Create a context we will run the state machine for.
		USMTestContext* Context = NewObject<USMTestContext>();
		USMInstance* Instance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);

		Instance->Start();
		Instance->Update(0.f);
		TestEqual("State machine still in initial state", Instance->GetRootStateMachine().GetSingleActiveState(), Instance->GetRootStateMachine().GetSingleInitialState());
		TestFalse("State machine shouldn't have switched states due to transition evaluation being false.", Instance->IsInEndState());
		
		Context->TransitionEvent.Broadcast();
		TestNotEqual("State machine has switched states", Instance->GetRootStateMachine().GetSingleActiveState(), Instance->GetRootStateMachine().GetSingleInitialState());

		TestTrue("State machine switched to end state from double update.", Instance->IsInEndState());
	}

	// Event binding with targeted update.
	{
		EventTriggerNode->bEventTriggersTargetedUpdate = true;
		EventTriggerNode->bEventTriggersFullUpdate = false;
		
		FKismetEditorUtilities::CompileBlueprint(NewBP);

		// Create a context we will run the state machine for.
		USMTestContext* Context = NewObject<USMTestContext>();
		USMInstance* Instance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);

		Instance->Start();
		Instance->Update(0.f);
		TestEqual("State machine still in initial state", Instance->GetRootStateMachine().GetSingleActiveState(), Instance->GetRootStateMachine().GetSingleInitialState());
		TestFalse("State machine shouldn't have switched states due to transition evaluation being false.", Instance->IsInEndState());
		
		Context->TransitionEvent.Broadcast();
		TestNotEqual("State machine switched states", Instance->GetRootStateMachine().GetSingleActiveState(), Instance->GetRootStateMachine().GetSingleInitialState());

		TestFalse("State machine not at end.", Instance->IsInEndState());
	}
	
	// Event binding targeted update with eval transitions on start.
	{
		EventTriggerNode->bEventTriggersTargetedUpdate = true;
		EventTriggerNode->bEventTriggersFullUpdate = false;

		MiddleState->GetNodeTemplateAs<USMStateInstance_Base>(true)->SetEvalTransitionsOnStart(true);
		
		FKismetEditorUtilities::CompileBlueprint(NewBP);

		// Create a context we will run the state machine for.
		USMTestContext* Context = NewObject<USMTestContext>();
		USMInstance* Instance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);

		Instance->Start();
		Instance->Update(0.f);
		TestEqual("State machine still in initial state", Instance->GetRootStateMachine().GetSingleActiveState(), Instance->GetRootStateMachine().GetSingleInitialState());
		TestFalse("State machine shouldn't have switched states due to transition evaluation being false.", Instance->IsInEndState());
		
		Context->TransitionEvent.Broadcast();
		TestNotEqual("State machine switched states", Instance->GetRootStateMachine().GetSingleActiveState(), Instance->GetRootStateMachine().GetSingleInitialState());

		TestTrue("State machine switched to end state from eval with transitions.", Instance->IsInEndState());
	}

	return NewAsset.DeleteAsset(this);
}

/**
 * Test automatically binding to a multi-cast delegate on the context.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransitionEventAutoBindContextTest, "LogicDriver.Transitions.Events.AutoBind.Context", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTransitionEventAutoBindContextTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST_NO_STATES()

	// Total states to test.
	UEdGraphPin* LastStatePin = nullptr;
	
	// Build a state machine of only two states.
	{
		const int32 CurrentStates = 2;
		TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, CurrentStates, &LastStatePin);
	}

	USMGraphNode_TransitionEdge* TransitionEdge =
		CastChecked<USMGraphNode_TransitionEdge>(Cast<USMGraphNode_StateNode>(LastStatePin->GetOwningNode())->GetInputPin()->LinkedTo[0]->GetOwningNode());
	TransitionEdge->GetNodeTemplateAs<USMTransitionInstance>()->SetCanEvaluate(false);
	TestHelpers::SetNodeClass(this, TransitionEdge, USMTransitionTestInstance::StaticClass());
	
	// Validate transition can't evaluate.
	{
		int32 a, b, c;
		USMInstance* Instance = TestHelpers::RunStateMachineToCompletion(this, NewBP, a, b, c, 5, false, false);
		TestFalse("State machine shouldn't have switched states due to transition evaluation being false.", Instance->IsInEndState());
	}
	
	// Setup a transition event binding.
	{
		TransitionEdge->DelegateOwnerInstance = ESMDelegateOwner::SMDO_Context;
		TransitionEdge->DelegateOwnerClass = USMTestContext::StaticClass();
		TransitionEdge->DelegatePropertyName = GET_MEMBER_NAME_CHECKED(USMTestContext, TransitionEvent);

		TransitionEdge->InitTransitionDelegate();

		FKismetEditorUtilities::CompileBlueprint(NewBP);

		// Create a context we will run the state machine for.
		USMTestContext* Context = NewObject<USMTestContext>();
		USMInstance* Instance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);

		Instance->Start();
		Instance->Update(0.f);
		TestEqual("State machine still in initial state", Instance->GetRootStateMachine().GetSingleActiveState(), Instance->GetRootStateMachine().GetSingleInitialState());
		TestFalse("State machine shouldn't have switched states due to transition evaluation being false.", Instance->IsInEndState());
		
		Context->TransitionEvent.Broadcast();
		TestNotEqual("State machine switched states", Instance->GetRootStateMachine().GetSingleActiveState(), Instance->GetRootStateMachine().GetSingleInitialState());
		TestTrue("State machine now in end state.", Instance->IsInEndState());

		TArray<USMTransitionInstance*> TransitionInstances;
		Instance->GetAllTransitionInstances(TransitionInstances);
		check(TransitionInstances.Num() == 1);
		const USMTransitionTestInstance* TestTransitionInstance = CastChecked<USMTransitionTestInstance>(TransitionInstances[0]);
		TestEqual("Transition initialized event hit", TestTransitionInstance->TransitionInitializedHit.Count, 1);
		TestEqual("Transition shutdown event hit", TestTransitionInstance->TransitionShutdownHit.Count, 1);
		TestEqual("Transition root sm start hit", TestTransitionInstance->TransitionRootSMStartHit.Count, 1);
		TestEqual("Transition entered event hit", TestTransitionInstance->TransitionEnteredEventHit.Count, 1);
		
		Instance->Shutdown();
		TestFalse("State Machine should have stopped", Instance->IsActive());
	}

	// Test disabling autobound event evaluation.
	{
		TransitionEdge->GetNodeTemplateAs<USMTransitionInstance>()->SetCanEvaluateFromEvent(false);
		
		FKismetEditorUtilities::CompileBlueprint(NewBP);

		// Create a context we will run the state machine for.
		USMTestContext* Context = NewObject<USMTestContext>();
		USMInstance* Instance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);

		Instance->Start();
		Instance->Update(0.f);
		TestEqual("State machine still in initial state", Instance->GetRootStateMachine().GetSingleActiveState(), Instance->GetRootStateMachine().GetSingleInitialState());
		TestFalse("State machine shouldn't have switched states due to transition evaluation being false.", Instance->IsInEndState());

		// Shouldn't cause evaluation.
		Context->TransitionEvent.Broadcast();
		
		TestEqual("State machine still in initial state", Instance->GetRootStateMachine().GetSingleActiveState(), Instance->GetRootStateMachine().GetSingleInitialState());
		TestFalse("State machine shouldn't have switched states due to transition evaluation being false.", Instance->IsInEndState());
	}

	// Test disabling tick evaluation
	{
		TransitionEdge->GetNodeTemplateAs<USMTransitionInstance>()->SetCanEvaluateFromEvent(true);
		
		USMGraphNode_StateNodeBase* FirstState = CastChecked<USMGraphNode_StateNodeBase>(StateMachineGraph->GetEntryNode()->GetOutputNode());
		FirstState->GetNodeTemplateAs<USMStateInstance_Base>(true)->SetDisableTickTransitionEvaluation(true);
		FKismetEditorUtilities::CompileBlueprint(NewBP);

		// Create a context we will run the state machine for.
		USMTestContext* Context = NewObject<USMTestContext>();
		USMInstance* Instance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);

		Instance->Start();
		Instance->Update(0.f);
		TestEqual("State machine still in initial state", Instance->GetRootStateMachine().GetSingleActiveState(), Instance->GetRootStateMachine().GetSingleInitialState());
		TestFalse("State machine shouldn't have switched states due to transition evaluation being false.", Instance->IsInEndState());

		// Should cause evaluation.
		Context->TransitionEvent.Broadcast();

		TestNotEqual("State machine switched states", Instance->GetRootStateMachine().GetSingleActiveState(), Instance->GetRootStateMachine().GetSingleInitialState());
		TestTrue("State machine now in end state.", Instance->IsInEndState());

		Instance->Shutdown();
		TestFalse("State Machine should have stopped", Instance->IsActive());
	}

	return NewAsset.DeleteAsset(this);
}

/**
 * Test automatically binding to a multi-cast delegate for the previous state.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransitionEventAutoBindPreviousStateTest, "LogicDriver.Transitions.Events.AutoBind.PreviousState", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTransitionEventAutoBindPreviousStateTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST_NO_STATES()

	// Total states to test.
	UEdGraphPin* LastStatePin = nullptr;
	
	// Build a state machine of only two states.
	{
		const int32 CurrentStates = 2;
		TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, CurrentStates, &LastStatePin);
	}

	USMGraphNode_TransitionEdge* TransitionEdge =
		CastChecked<USMGraphNode_TransitionEdge>(Cast<USMGraphNode_StateNode>(LastStatePin->GetOwningNode())->GetInputPin()->LinkedTo[0]->GetOwningNode());
	TransitionEdge->GetNodeTemplateAs<USMTransitionInstance>()->SetCanEvaluate(false);

	TestHelpers::SetNodeClass(this, TransitionEdge->GetFromState(), USMStateTestInstance::StaticClass());
	TestHelpers::SetNodeClass(this, TransitionEdge, USMTransitionTestInstance::StaticClass());
	
	// Validate transition can't evaluate.
	{
		int32 a, b, c;
		USMInstance* Instance = TestHelpers::RunStateMachineToCompletion(this, NewBP, a, b, c, 5, false, false);
		TestFalse("State machine shouldn't have switched states due to transition evaluation being false.", Instance->IsInEndState());
	}
	
	// Setup a transition event binding.
	{
		TransitionEdge->DelegateOwnerInstance = ESMDelegateOwner::SMDO_PreviousState;
		TransitionEdge->DelegatePropertyName = GET_MEMBER_NAME_CHECKED(USMStateTestInstance, StateEvent);

		TransitionEdge->InitTransitionDelegate();

		FKismetEditorUtilities::CompileBlueprint(NewBP);

		// Create a context we will run the state machine for.
		USMTestContext* Context = NewObject<USMTestContext>();
		USMInstance* Instance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);

		Instance->Start();
		Instance->Update(0.f);
		TestEqual("State machine still in initial state", Instance->GetRootStateMachine().GetSingleActiveState(), Instance->GetRootStateMachine().GetSingleInitialState());
		TestFalse("State machine shouldn't have switched states due to transition evaluation being false.", Instance->IsInEndState());
		
		CastChecked<USMStateTestInstance>(Instance->GetSingleActiveState()->GetNodeInstance())->StateEvent.Broadcast();
		TestNotEqual("State machine switched states", Instance->GetRootStateMachine().GetSingleActiveState(), Instance->GetRootStateMachine().GetSingleInitialState());
		TestTrue("State machine now in end state.", Instance->IsInEndState());

		TArray<USMTransitionInstance*> TransitionInstances;
		Instance->GetAllTransitionInstances(TransitionInstances);
		check(TransitionInstances.Num() == 1);
		const USMTransitionTestInstance* TestTransitionInstance = CastChecked<USMTransitionTestInstance>(TransitionInstances[0]);
		TestEqual("Transition initialized event hit", TestTransitionInstance->TransitionInitializedHit.Count, 1);
		TestEqual("Transition shutdown event hit", TestTransitionInstance->TransitionShutdownHit.Count, 1);
		TestEqual("Transition root sm start hit", TestTransitionInstance->TransitionRootSMStartHit.Count, 1);
		TestEqual("Transition entered event hit", TestTransitionInstance->TransitionEnteredEventHit.Count, 1);
		
		Instance->Shutdown();
		TestFalse("State Machine should have stopped", Instance->IsActive());
	}

	// Test disabling autobound event evaluation.
	{
		TransitionEdge->GetNodeTemplateAs<USMTransitionInstance>()->SetCanEvaluateFromEvent(false);
		
		FKismetEditorUtilities::CompileBlueprint(NewBP);

		// Create a context we will run the state machine for.
		USMTestContext* Context = NewObject<USMTestContext>();
		USMInstance* Instance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);

		Instance->Start();
		Instance->Update(0.f);
		TestEqual("State machine still in initial state", Instance->GetRootStateMachine().GetSingleActiveState(), Instance->GetRootStateMachine().GetSingleInitialState());
		TestFalse("State machine shouldn't have switched states due to transition evaluation being false.", Instance->IsInEndState());

		// Shouldn't cause evaluation.
		CastChecked<USMStateTestInstance>(Instance->GetSingleActiveState()->GetNodeInstance())->StateEvent.Broadcast();
		
		TestEqual("State machine still in initial state", Instance->GetRootStateMachine().GetSingleActiveState(), Instance->GetRootStateMachine().GetSingleInitialState());
		TestFalse("State machine shouldn't have switched states due to transition evaluation being false.", Instance->IsInEndState());
	}

	// Test disabling tick evaluation
	{
		TransitionEdge->GetNodeTemplateAs<USMTransitionInstance>()->SetCanEvaluateFromEvent(true);
		
		USMGraphNode_StateNodeBase* FirstState = CastChecked<USMGraphNode_StateNodeBase>(StateMachineGraph->GetEntryNode()->GetOutputNode());
		FirstState->GetNodeTemplateAs<USMStateInstance_Base>(true)->SetDisableTickTransitionEvaluation(true);
		FKismetEditorUtilities::CompileBlueprint(NewBP);

		// Create a context we will run the state machine for.
		USMTestContext* Context = NewObject<USMTestContext>();
		USMInstance* Instance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);

		Instance->Start();
		Instance->Update(0.f);
		TestEqual("State machine still in initial state", Instance->GetRootStateMachine().GetSingleActiveState(), Instance->GetRootStateMachine().GetSingleInitialState());
		TestFalse("State machine shouldn't have switched states due to transition evaluation being false.", Instance->IsInEndState());

		// Should cause evaluation.
		CastChecked<USMStateTestInstance>(Instance->GetSingleActiveState()->GetNodeInstance())->StateEvent.Broadcast();

		TestNotEqual("State machine switched states", Instance->GetRootStateMachine().GetSingleActiveState(), Instance->GetRootStateMachine().GetSingleInitialState());
		TestTrue("State machine now in end state.", Instance->IsInEndState());

		Instance->Shutdown();
		TestFalse("State Machine should have stopped", Instance->IsActive());
	}

	return NewAsset.DeleteAsset(this);
}

/**
 * Check transition optimization type is correct.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransitionOptimizationTest, "LogicDriver.Transitions.Optimization", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTransitionOptimizationTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(2)
	const int32 MaxIterations = TotalStates;
	
	UEdGraphPin* LastStatePin = nullptr;
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin);

	USMGraphNode_TransitionEdge* TransitionEdge =
		CastChecked<USMGraphNode_TransitionEdge>(CastChecked<USMGraphNode_StateNode>(LastStatePin->GetOwningNode())->GetInputPin()->LinkedTo[0]->GetOwningNode());

	USMTransitionGraph* TransitionGraph = TransitionEdge->GetTransitionGraph();
	ESMConditionalEvaluationType EvaluationType;

	int32 A, B, C;
	int32 IterationsRan;
	// Node instance evaluation.
	{
		TestHelpers::SetNodeClass(this, TransitionEdge, USMTransitionTestInstance::StaticClass());
		EvaluationType = TransitionGraph->GetConditionalEvaluationType();
		TestEqual("Evaluation type is for the node instance", EvaluationType, ESMConditionalEvaluationType::SM_NodeInstance);
		USMInstance* Instance = TestHelpers::RunStateMachineToCompletion(this, NewBP, A, B, C, MaxIterations, false, false, true, &IterationsRan);
		TestEqual("Max iterations ran", IterationsRan, MaxIterations);
		TestFalse("State machine didn't finish", Instance->IsInEndState());
		
		USMTransitionTestInstance* TransitionInstance = CastChecked<USMTransitionTestInstance>(CastChecked<USMStateInstance_Base>(Instance->GetSingleActiveState()->GetNodeInstance())->GetTransitionByIndex(0));
		TransitionInstance->bCanTransition = true;

		Instance->Update();
		TestTrue("State machine finished", Instance->IsInEndState());
		
		TestHelpers::SetNodeClass(this, TransitionEdge, nullptr);
	}
	
	// Always true
	{
		TransitionGraph->ResultNode->BreakAllNodeLinks();
		TransitionGraph->GetSchema()->TrySetDefaultValue(*TransitionGraph->ResultNode->GetTransitionEvaluationPin(), "True");
		EvaluationType = TransitionGraph->GetConditionalEvaluationType();
		TestEqual("Evaluation type is always true", EvaluationType, ESMConditionalEvaluationType::SM_AlwaysTrue);
		TestHelpers::RunStateMachineToCompletion(this, NewBP, A, B, C, MaxIterations, true, true, true, &IterationsRan);
		TestEqual("Expected iterations ran", IterationsRan, 1);
	}

	// Always false
	{
		TransitionGraph->GetSchema()->TrySetDefaultValue(*TransitionGraph->ResultNode->GetTransitionEvaluationPin(), "False");
		EvaluationType = TransitionGraph->GetConditionalEvaluationType();
		TestEqual("Evaluation type is always false", EvaluationType, ESMConditionalEvaluationType::SM_AlwaysFalse);

		TestHelpers::RunStateMachineToCompletion(this, NewBP, A, B, C, MaxIterations, true, true, true, &IterationsRan);
		TestEqual("First state is considered the end state since the transition is always false", IterationsRan, 0);
	}

	// Graph evaluation false
	{
		FName VarName = "NewVar";
		FEdGraphPinType VarType;
		VarType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

		// Create new variable.
		FBlueprintEditorUtils::AddMemberVariable(NewBP, VarName, VarType, "False");

		FProperty* NewProperty = FSMBlueprintEditorUtils::GetPropertyByName(NewBP, VarName);

		// Place variable getter and wire to result node.
		FSMBlueprintEditorUtils::PlacePropertyOnGraph(TransitionGraph, NewProperty, TransitionGraph->ResultNode->GetTransitionEvaluationPin(), nullptr);
		
		EvaluationType = TransitionGraph->GetConditionalEvaluationType();
		TestEqual("Evaluation type is graph evaluation", EvaluationType, ESMConditionalEvaluationType::SM_Graph);

		USMInstance* Instance = TestHelpers::RunStateMachineToCompletion(this, NewBP, A, B, C, MaxIterations, false, false);
		TestFalse("Instance not finished", Instance->IsInEndState());

		FBlueprintEditorUtils::RemoveMemberVariable(NewBP, VarName);
	}

	// Graph evaluation true
	{
		FName VarName = "NewVar";
		FEdGraphPinType VarType;
		VarType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

		// Create new variable.
		FBlueprintEditorUtils::AddMemberVariable(NewBP, VarName, VarType, "True");

		FProperty* NewProperty = FSMBlueprintEditorUtils::GetPropertyByName(NewBP, VarName);

		// Place variable getter and wire to result node.
		FSMBlueprintEditorUtils::PlacePropertyOnGraph(TransitionGraph, NewProperty, TransitionGraph->ResultNode->GetTransitionEvaluationPin(), nullptr);

		EvaluationType = TransitionGraph->GetConditionalEvaluationType();
		TestEqual("Evaluation type is accurate", EvaluationType, ESMConditionalEvaluationType::SM_Graph);

		TestHelpers::RunStateMachineToCompletion(this, NewBP, A, B, C, MaxIterations);
	}
	
	return NewAsset.DeleteAsset(this);
}

/**
 * Test rerouting a transition.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransitionRerouteNodeTest, "LogicDriver.Transitions.RerouteNode", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTransitionRerouteNodeTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST_NO_STATES()

	TGuardValue<ESMEditorConstructionScriptProjectSetting> ConstructionScriptGuard(
		FSMBlueprintEditorUtils::GetMutableProjectEditorSettings()->EditorNodeConstructionScriptSetting, ESMEditorConstructionScriptProjectSetting::SM_Standard);

	// Total states to test.
	UEdGraphPin* LastStatePin = nullptr;

	const int32 TotalStates = 2;
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, nullptr);

	USMGraphNode_StateNodeBase* InitialState = CastChecked<USMGraphNode_StateNodeBase>(StateMachineGraph->EntryNode->GetOutputNode());

	// The current second state, which we will instead link to.
	const USMGraphNode_StateNodeBase* SecondState = InitialState->GetNextState();

	// Entry -> Initial State -> None
	InitialState->GetSchema()->BreakPinLinks(*InitialState->GetOutputPin(), true);

	// Add reroute node.
	FGraphNodeCreator<USMGraphNode_RerouteNode> RerouteNodeCreator(*StateMachineGraph);
	const USMGraphNode_RerouteNode* RerouteNode = RerouteNodeCreator.CreateNode();
	RerouteNodeCreator.Finalize();

	// Connect to reroute node.
	check(InitialState->GetSchema()->TryCreateConnection(InitialState->GetOutputPin(), RerouteNode->GetInputPin()));

	// Test warning when no state is connected reroute.
	{
		AddExpectedError(TEXT("has no Next State"));
		AddExpectedError(TEXT("node is missing a connection"));
		AddExpectedError(TEXT("has no graph for end node"));
		FKismetEditorUtilities::CompileBlueprint(NewBP);
	}

	check(InitialState->GetSchema()->TryCreateConnection(RerouteNode->GetOutputPin(), SecondState->GetInputPin()));

	USMGraphNode_TransitionEdge* TransitionEdge = InitialState->GetNextTransition()->GetPrimaryReroutedTransition();
	TransitionEdge->SetNodeClass(USMTransitionConstructionTestInstance::StaticClass());

	USMTransitionConstructionTestInstance* TransitionTestInstance = TransitionEdge->GetNodeTemplateAs<USMTransitionConstructionTestInstance>();
	check(TransitionTestInstance);

	TransitionTestInstance->ConstructionScriptHit.Count = 0;

	FKismetEditorUtilities::CompileBlueprint(NewBP);

	const int32 ConstructionScriptPasses = NewBP->NumConstructionScriptPasses;
	
	TestEqual("Construction script ran for Rerouted Transition", TransitionTestInstance->ConstructionScriptHit.Count, ConstructionScriptPasses);

	int32 A,B,C;
	TestHelpers::RunStateMachineToCompletion(this, NewBP, A, B, C);

	// Add second reroute.

	FGraphNodeCreator<USMGraphNode_RerouteNode> RerouteNode2Creator(*StateMachineGraph);
	const USMGraphNode_RerouteNode* RerouteNode2 = RerouteNode2Creator.CreateNode();
	RerouteNode2Creator.Finalize();

	InitialState->GetSchema()->BreakPinLinks(*RerouteNode->GetOutputPin(), true);
	check(InitialState->GetSchema()->TryCreateConnection(RerouteNode->GetOutputPin(), RerouteNode2->GetInputPin()));
	check(InitialState->GetSchema()->TryCreateConnection(RerouteNode2->GetOutputPin(), SecondState->GetInputPin()));

	TransitionTestInstance->ConstructionScriptHit.Count = 0;
	FKismetEditorUtilities::CompileBlueprint(NewBP);
	TestEqual("Construction script ran for Rerouted Transition", TransitionTestInstance->ConstructionScriptHit.Count, ConstructionScriptPasses);

	// Test reroute rename
	{
		const FString State1NewName = TEXT("State1CustomName");
		InitialState->SetNodeName_Direct(State1NewName);
		TestTrue(TEXT("Reroute renamed"), RerouteNode2->GetNodeName().StartsWith(State1NewName));
	}
	
	TestHelpers::RunStateMachineToCompletion(this, NewBP, A, B, C);

	return NewAsset.DeleteAsset(this);
}

/**
 * Test project settings that impact transitions.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransitionSettingsTest, "LogicDriver.Transitions.SettingsTests", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTransitionSettingsTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(2)

	UEdGraphPin* LastStatePin = nullptr;
	const bool bForceTransitionsToTrue = false;

	const bool bUserNewTransitionValue = FSMBlueprintEditorUtils::GetProjectEditorSettings()->bDefaultNewTransitionsToTrue;
	
	// Test default value as false
	{
		FSMBlueprintEditorUtils::GetMutableProjectEditorSettings()->bDefaultNewTransitionsToTrue = false;
		
		TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, nullptr, nullptr, bForceTransitionsToTrue);

		USMGraphNode_StateNode* FirstStateNode = CastChecked<USMGraphNode_StateNode>(StateMachineGraph->GetEntryNode()->GetOutputNode());

		const FString DefaultValue = FirstStateNode->GetNextTransition()->GetTransitionGraph()->ResultNode->GetTransitionEvaluationPin()->GetDefaultAsString();
		TestEqual("Default is false", DefaultValue, "");
	}

	// Test default value as true
	{
		FSMBlueprintEditorUtils::GetMutableProjectEditorSettings()->bDefaultNewTransitionsToTrue = true;

		LastStatePin = nullptr;
		FSMBlueprintEditorUtils::RemoveAllNodesFromGraph(StateMachineGraph);

		TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, nullptr, nullptr, bForceTransitionsToTrue);

		USMGraphNode_StateNode* FirstStateNode = CastChecked<USMGraphNode_StateNode>(StateMachineGraph->GetEntryNode()->GetOutputNode());

		const FString DefaultValue = FirstStateNode->GetNextTransition()->GetTransitionGraph()->ResultNode->GetTransitionEvaluationPin()->GetDefaultAsString();
		TestEqual("Default is false", DefaultValue, "True");
	}

	FSMBlueprintEditorUtils::GetMutableProjectEditorSettings()->bDefaultNewTransitionsToTrue = bUserNewTransitionValue;
	
	return NewAsset.DeleteAsset(this);
}

/**
 * Test renaming a transition.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransitionNamesTest, "LogicDriver.Transitions.Rename", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTransitionNamesTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(3)
	UEdGraphPin* LastStatePin = nullptr;
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMStateTestInstance::StaticClass(), USMTransitionTestInstance::StaticClass());

	TArray<USMGraphNode_TransitionEdge*> Transitions;
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested(StateMachineGraph, Transitions);

	check(Transitions.Num() == 2);

	TestEqual("Transition name is default", Transitions[0]->GetTransitionName(), Transitions[0]->GetDefaultTransitionName());
	TestEqual("Transition name is default", Transitions[1]->GetTransitionName(), Transitions[1]->GetDefaultTransitionName());

	// Test default renaming
	{
		USMGraphNode_StateNodeBase* FirstState = Transitions[0]->GetFromState();
		check(FirstState);

		const FString StateName1 = "StateCustomName1";
		FirstState->SetNodeName_Direct(StateName1);

		const FString DefaultName = Transitions[0]->GetDefaultTransitionName();
		TestTrue("Default name adjusted from state rename", DefaultName.StartsWith(StateName1));
		TestEqual("Transition default name is changed", Transitions[0]->GetTransitionName(), DefaultName);
	}

	// Test custom renaming
	const FString TransitionName1 = "TransitionCustomName1";
	const FString TransitionName2 = "TransitionCustomName2";
	
	Transitions[0]->SetNodeName_Direct(TransitionName1);
	Transitions[1]->SetNodeName_Direct(TransitionName2);

	TestEqual("Transition name is changed", Transitions[0]->GetTransitionName(), TransitionName1);
	TestEqual("Transition name is changed", Transitions[1]->GetTransitionName(), TransitionName2);

	FKismetEditorUtilities::CompileBlueprint(NewBP);
	
	USMInstance* Instance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, NewObject<USMTestContext>());

	TArray<USMStateInstance_Base*> EntryStates;
	Instance->GetRootStateMachineNodeInstance()->GetEntryStates(EntryStates);
	check(EntryStates.Num() == 1);

	const USMTransitionInstance* FirstTransition = EntryStates[0]->GetTransitionByIndex(0);
	const USMTransitionInstance* SecondTransition = FirstTransition->GetNextStateInstance()->GetTransitionByIndex(0);
	TestEqual("Run time transition named correctly", FirstTransition->GetNodeName(), TransitionName1);
	TestEqual("Run time transition named correctly", SecondTransition->GetNodeName(), TransitionName2);

	Instance->Start();

	{
		USMStateInstance_Base* StateInstance = Instance->GetSingleActiveStateInstance();
		check(StateInstance);

		TestNotNull("Outgoing transition found by name", StateInstance->GetOutgoingTransitionByName(TransitionName1));
		const bool bSwitched = StateInstance->SwitchToLinkedStateByTransitionName(TransitionName1);
		TestTrue("State switch by transition name", bSwitched);
	}

	{
		USMStateInstance_Base* StateInstance = Instance->GetSingleActiveStateInstance();
		check(StateInstance);

		TestNotNull("Incoming transition found by name", StateInstance->GetIncomingTransitionByName(TransitionName1));
		
		TestNotNull("Outgoing transition found by name", StateInstance->GetOutgoingTransitionByName(TransitionName2));
		const bool bSwitched = StateInstance->SwitchToLinkedStateByTransitionName(TransitionName2);
		TestTrue("State switch by transition name", bSwitched);
	}

	{
		const USMStateInstance_Base* StateInstance = Instance->GetSingleActiveStateInstance();
		check(StateInstance);
		TestNotNull("Incoming transition found by name", StateInstance->GetIncomingTransitionByName(TransitionName2));
	}
	
	return true;
}

#endif

#endif //WITH_DEV_AUTOMATION_TESTS