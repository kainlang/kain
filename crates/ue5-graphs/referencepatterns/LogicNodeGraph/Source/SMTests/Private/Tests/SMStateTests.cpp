// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Helpers/SMTestBoilerplate.h"
#include "SMTestContext.h"
#include "SMTestHelpers.h"

#include "Blueprints/SMBlueprint.h"

#include "Blueprints/SMBlueprintFactory.h"
#include "Graph/Nodes/FunctionNodes/SMGraphK2Node_StateReadNodes.h"
#include "Graph/Nodes/SMGraphK2Node_StateMachineNode.h"
#include "Graph/Nodes/SMGraphNode_AnyStateNode.h"
#include "Graph/Nodes/SMGraphNode_LinkStateNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineEntryNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineStateNode.h"
#include "Graph/Nodes/SMGraphNode_StateNodeBase.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/SMGraph.h"
#include "Graph/SMStateGraph.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "EdGraph/EdGraph.h"
#include "Kismet2/KismetEditorUtilities.h"

#include "InputCoreTypes.h"
#include "K2Node_InputKey.h"

#if WITH_DEV_AUTOMATION_TESTS

#if PLATFORM_DESKTOP

/**
 * Test nested state machines' bWaitForEndState flag.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWaitForEndStateTest, "LogicDriver.States.WaitForEndState", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FWaitForEndStateTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST_NO_STATES()

	// Total states to test.
	int32 TotalTopLevelStates = 3;
	int32 TotalNestedStates = 2;
	
	UEdGraphPin* LastStatePin = nullptr;

	// Build state machine first state.
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, 1, &LastStatePin);

	// Connect nested FSM.
	UEdGraphPin* EntryPointForNestedStateMachine = LastStatePin;
	USMGraphNode_StateMachineStateNode* NestedFSM = TestHelpers::BuildNestedStateMachine(this, StateMachineGraph, TotalNestedStates, &EntryPointForNestedStateMachine, nullptr);
	LastStatePin = NestedFSM->GetOutputPin();
	
	NestedFSM->GetNodeTemplateAs<USMStateMachineInstance>()->SetWaitForEndState(false);

	// Third state regular state.
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, 1, &LastStatePin);

	// Test transition evaluation waiting for end state.
	// [A -> [A -> B] -> C
	{
		int32 EntryHits = 0; int32 UpdateHits = 0; int32 EndHits = 0;
		TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits);
		TestEqual("Didn't wait for end state.", EntryHits, TotalTopLevelStates);
		TestEqual("Didn't wait for end state.", EndHits, TotalTopLevelStates);

		NestedFSM->GetNodeTemplateAs<USMStateMachineInstance>()->SetWaitForEndState(true);

		TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits);
		TestEqual("Waited for end state.", EntryHits, TotalTopLevelStates + TotalNestedStates - 1);
		TestEqual("Waited for end state.", EndHits, TotalTopLevelStates + TotalNestedStates - 1);
	}

	USMGraphNode_StateMachineStateNode* EndFSM = FSMBlueprintEditorUtils::ConvertNodeTo<USMGraphNode_StateMachineStateNode>(NestedFSM->GetNextState());
	TestHelpers::BuildLinearStateMachine(this, CastChecked<USMGraph>(EndFSM->GetBoundGraph()), TotalNestedStates, nullptr);

	TotalNestedStates *= 2;
	
	// Test root end state not being considered until fsm is in end state.
	// [A -> [A -> B] -> [A -> B]
	{
		int32 EntryHits = 0; int32 UpdateHits = 0; int32 EndHits = 0;
		// Will hit all states of first FSM, then stop on first state of second fsm.
		TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits);
		TestEqual("Didn't wait for end state.", EntryHits, 4); // [A -> [A -> B] -> [A]
		TestEqual("Didn't wait for end state.", EndHits, 4);

		EndFSM->GetNodeTemplateAs<USMStateMachineInstance>()->SetWaitForEndState(true);

		// Will hit all states of all FSMs. This test doesn't stop until the root state machine is in an end state.
		TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits);
		TestEqual("Waited for end state.", EntryHits, TotalTopLevelStates + TotalNestedStates - 2); // [A -> [A -> B] -> [A -> B]
		TestEqual("Waited for end state.", EndHits, TotalTopLevelStates + TotalNestedStates - 2);
	}
	
	return true;
}

/**
 * Test creating an any state node.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnyStateTest, "LogicDriver.States.AnyStateTest", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FAnyStateTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST_NO_STATES()

	// Total states to test.
	UEdGraphPin* LastStatePin = nullptr;
	
	// Build a state machine of only two states.
	{
		const int32 CurrentStates = 2;
		TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, CurrentStates, &LastStatePin);
	}

	USMGraphNode_StateNodeBase* LastNormalState = CastChecked<USMGraphNode_StateNodeBase>(LastStatePin->GetOwningNode());
	LastNormalState->GetNodeTemplateAs<USMStateInstance_Base>(true)->SetExcludeFromAnyState(false);
	
	// Add any state.
	FGraphNodeCreator<USMGraphNode_AnyStateNode> AnyStateNodeCreator(*StateMachineGraph);
	USMGraphNode_AnyStateNode* AnyState = AnyStateNodeCreator.CreateNode();
	AnyStateNodeCreator.Finalize();

	FString AnyStateInitialStateName = "AnyState_Initial";
	{
		UEdGraphPin* InputPin = AnyState->GetOutputPin();

		// Connect a state to anystate.
		TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, 1, &InputPin);

		AnyState->GetNextState()->GetBoundGraph()->Rename(*AnyStateInitialStateName, nullptr, REN_DontCreateRedirectors);
	}

	USMGraphNode_TransitionEdge* TransitionEdge = AnyState->GetNextTransition();
	TransitionEdge->GetNodeTemplateAs<USMTransitionInstance>()->SetPriorityOrder(1);
	TestTrue("Graph Transition from Any State", TransitionEdge->IsFromAnyState());

	{
		FKismetEditorUtilities::CompileBlueprint(NewBP);
		USMTestContext* Context = NewObject<USMTestContext>();
		USMInstance* Instance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);

		TArray<USMTransitionInstance*> RuntimeTransitions;
		Instance->GetAllTransitionInstances(RuntimeTransitions);
		check(RuntimeTransitions.Num() == 3);
		TestTrue("Runtime transition from Any State", RuntimeTransitions[0]->IsTransitionFromAnyState());
		TestTrue("Runtime transition from Any State", RuntimeTransitions[1]->IsTransitionFromAnyState());
		TestFalse("Runtime transition not from Any State", RuntimeTransitions[2]->IsTransitionFromAnyState());
		
		TArray<USMStateInstance_Base*> RuntimeStates;
		Instance->GetAllStateInstances(RuntimeStates);
		check(RuntimeStates.Num() == 4);
		TestFalse("Runtime state outgoing transitions Any State", RuntimeStates[0]->AreAllOutgoingTransitionsFromAnAnyState());
		TestFalse("Runtime state outgoing transitions Any State", RuntimeStates[1]->AreAllOutgoingTransitionsFromAnAnyState());
		TestTrue("Runtime state outgoing transitions Any State", RuntimeStates[2]->AreAllOutgoingTransitionsFromAnAnyState());
		TestFalse("Runtime state outgoing transitions Any State", RuntimeStates[3]->AreAllOutgoingTransitionsFromAnAnyState());

		TestFalse("Runtime state incoming transitions Any State", RuntimeStates[0]->AreAllIncomingTransitionsFromAnAnyState());
		TestTrue("Runtime state incoming transitions Any State", RuntimeStates[1]->AreAllIncomingTransitionsFromAnAnyState());
		TestFalse("Runtime state incoming transitions Any State", RuntimeStates[2]->AreAllIncomingTransitionsFromAnAnyState());
		TestFalse("Runtime state incoming transitions Any State", RuntimeStates[3]->AreAllIncomingTransitionsFromAnAnyState());
		
		Instance->Start();
		TestEqual("State machine still in initial state", Instance->GetRootStateMachine().GetSingleActiveState(), Instance->GetRootStateMachine().GetSingleInitialState());

		// Any state shouldn't be triggered because priority is lower.
		Instance->Update();
		TestNotEqual("Any state transition not called", Instance->GetRootStateMachine().GetSingleActiveState()->GetNodeName(), AnyStateInitialStateName);

		TestFalse("Not considered end state", Instance->IsInEndState());
		
		// No other transitions left except any state.
		Instance->Update();
		TestEqual("Any state transition called", Instance->GetRootStateMachine().GetSingleActiveState()->GetNodeName(), AnyStateInitialStateName);
		
		Instance->Shutdown();
	}
	
	TransitionEdge->GetNodeTemplateAs<USMTransitionInstance>()->SetPriorityOrder(-1);
	
	{
		FKismetEditorUtilities::CompileBlueprint(NewBP);
		USMTestContext* Context = NewObject<USMTestContext>();
		USMInstance* Instance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);

		Instance->Start();
		TestEqual("State machine still in initial state", Instance->GetRootStateMachine().GetSingleActiveState(), Instance->GetRootStateMachine().GetSingleInitialState());

		// Any state should evaluate first.
		Instance->Update();
		TestEqual("Any state transition called", Instance->GetRootStateMachine().GetSingleActiveState()->GetNodeName(), AnyStateInitialStateName);

		Instance->Shutdown();
	}
	
	// Try reference nodes such as Time in State
	{
		TestHelpers::AddSpecialFloatTransitionLogic<USMGraphK2Node_StateReadNode_TimeInState>(this, TransitionEdge);
		FKismetEditorUtilities::CompileBlueprint(NewBP);
		USMTestContext* Context = NewObject<USMTestContext>();
		USMInstance* Instance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);

		Instance->Start();
		TestEqual("State machine still in initial state", Instance->GetRootStateMachine().GetSingleActiveState(), Instance->GetRootStateMachine().GetSingleInitialState());

		// Any state shouldn't be triggered yet.
		Instance->Update(1);
		TestNotEqual("Any state transition not called", Instance->GetRootStateMachine().GetSingleActiveState()->GetNodeName(), AnyStateInitialStateName);
		TestFalse("Not considered end state because any state is not excluded from end.", Instance->IsInEndState());
		
		Instance->Update(3);
		TestNotEqual("Any state transition not called", Instance->GetRootStateMachine().GetSingleActiveState()->GetNodeName(), AnyStateInitialStateName);

		Instance->Update(3);
		TestNotEqual("Any state transition not called", Instance->GetRootStateMachine().GetSingleActiveState()->GetNodeName(), AnyStateInitialStateName);
		
		Instance->Update(1);
		TestEqual("Any state transition called", Instance->GetRootStateMachine().GetSingleActiveState()->GetNodeName(), AnyStateInitialStateName);

		Instance->Shutdown();
	}

	LastNormalState->GetNodeTemplateAs<USMStateInstance_Base>(true)->SetExcludeFromAnyState(true);

	// Try reference nodes such as Time in State
	{
		TestHelpers::AddSpecialFloatTransitionLogic<USMGraphK2Node_StateReadNode_TimeInState>(this, TransitionEdge);
		FKismetEditorUtilities::CompileBlueprint(NewBP);
		USMTestContext* Context = NewObject<USMTestContext>();
		USMInstance* Instance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);

		Instance->Start();
		TestEqual("State machine still in initial state", Instance->GetRootStateMachine().GetSingleActiveState(), Instance->GetRootStateMachine().GetSingleInitialState());

		// Any state shouldn't be triggered yet.
		Instance->Update(1);
		TestNotEqual("Any state transition not called", Instance->GetRootStateMachine().GetSingleActiveState()->GetNodeName(), AnyStateInitialStateName);
		TestTrue("Considered end state because any state is excluded from end.", Instance->IsInEndState());

		Instance->Update(3);
		TestNotEqual("Any state transition not called", Instance->GetRootStateMachine().GetSingleActiveState()->GetNodeName(), AnyStateInitialStateName);

		// Should not be called because last state is excluded.
		Instance->Update(5);
		TestNotEqual("Any state transition not called", Instance->GetRootStateMachine().GetSingleActiveState()->GetNodeName(), AnyStateInitialStateName);

		Instance->Shutdown();
	}

	// Try input binding.
	{
		USMInstance* CDO = CastChecked<USMInstance>(NewBP->GeneratedClass->GetDefaultObject());
		CDO->SetAutoReceiveInput(ESMStateMachineInput::UseContextController);

		UEdGraph* BoundGraph = TransitionEdge->GetBoundGraph();
		
		FGraphNodeCreator<UK2Node_InputKey> InputKeyCreator(*BoundGraph);
		UK2Node_InputKey* InputKey = InputKeyCreator.CreateNode();
		InputKey->InputKey = FKey(TEXT("One"));
		InputKeyCreator.Finalize();

		// Any function will do.
		UK2Node_CallFunction* CallFunction =
		TestHelpers::CreateFunctionCall(BoundGraph,
			USMInstance::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMInstance, Stop)));

		const bool bResult = InputKey->GetGraph()->GetSchema()->TryCreateConnection(InputKey->GetPressedPin(), CallFunction->GetExecPin());
		ensure(bResult);
		
		// No errors is all that needs to be verified. Specific input results verified in project level tests.
		FKismetEditorUtilities::CompileBlueprint(NewBP);
	}
	
	return NewAsset.DeleteAsset(this);
}

/**
 * Test creating a link state node.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLinkStateTest, "LogicDriver.States.LinkStateTest", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FLinkStateTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST_NO_STATES()

	// Total states to test.
	UEdGraphPin* LastStatePin = nullptr;

	const int32 TotalStates = 3;
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin);

	USMGraphNode_StateNodeBase* InitialState = CastChecked<USMGraphNode_StateNodeBase>(StateMachineGraph->EntryNode->GetOutputNode());

	// The current second state, which we will instead link to.
	USMGraphNode_StateNodeBase* StateToLinkTo = InitialState->GetNextState();

	// Entry -> Initial State -> None
	InitialState->GetSchema()->BreakPinLinks(*InitialState->GetOutputPin(), true);

	// Add link state.
	FGraphNodeCreator<USMGraphNode_LinkStateNode> LinkStateNodeCreator(*StateMachineGraph);
	USMGraphNode_LinkStateNode* LinkState = LinkStateNodeCreator.CreateNode();
	LinkStateNodeCreator.Finalize();

	// Connect to link state.
	check(InitialState->GetSchema()->TryCreateConnection(InitialState->GetOutputPin(), LinkState->GetInputPin()));

	// Test warning when no state is linked.
	{
		AddExpectedError(TEXT("No state linked for"));
		FKismetEditorUtilities::CompileBlueprint(NewBP);
	}

	// Make sure the new transition can transition.
	USMGraphNode_TransitionEdge* TransitionEdge = LinkState->GetPreviousTransition();
	TestHelpers::AddTransitionResultLogic(this, TransitionEdge);

	// Test warning when no state is linked with a transition to it.
	{
		AddExpectedError(TEXT("Invalid state linked for"));
		FKismetEditorUtilities::CompileBlueprint(NewBP);
	}

	// Test proper link.
	{
		LinkState->LinkToState(StateToLinkTo->GetStateName());

		TestEqual("Linked state set", LinkState->GetLinkedState(), StateToLinkTo);
		TestTrue("Linked state valid", LinkState->IsLinkedStateValid());

		FKismetEditorUtilities::CompileBlueprint(NewBP);
	}

	// Test run with transition to link state.
	{
		const int32 ExpectedValue = TotalStates;
		int32 EntryHits = 0; int32 UpdateHits = 0; int32 EndHits = 0;
		USMInstance* Instance = TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits);
		TestEqual("State Machine generated value", EntryHits, ExpectedValue);
		TestEqual("State Machine generated value", UpdateHits, 0);
		TestEqual("State Machine generated value", EndHits, ExpectedValue);

		TArray<USMStateInstance_Base*> EntryStates;
		Instance->GetRootStateMachineNodeInstance()->GetEntryStates(EntryStates);
		check(EntryStates.Num() == 1);
		TestTrue("Transition from Link State", EntryStates[0]->GetTransitionByIndex(0)->IsTransitionFromLinkState());
		TestFalse("Transition not from Link State", EntryStates[0]->GetNextStateByTransitionIndex(0)->GetTransitionByIndex(0)->IsTransitionFromLinkState());
	}

	// Test run with entry state to link state.
	FBlueprintEditorUtils::RemoveNode(NewBP, InitialState);
	check(InitialState->GetSchema()->TryCreateConnection(StateMachineGraph->EntryNode->GetOutputPin(), LinkState->GetInputPin()));

	{
		const int32 ExpectedValue = TotalStates - 1;
		int32 EntryHits = 0; int32 UpdateHits = 0; int32 EndHits = 0;
		TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits);
		TestEqual("State Machine generated value", EntryHits, ExpectedValue);
		TestEqual("State Machine generated value", UpdateHits, 0);
		TestEqual("State Machine generated value", EndHits, ExpectedValue);
	}

	return NewAsset.DeleteAsset(this);
}

/**
 * Run multiple states in parallel.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FParallelStatesTest, "LogicDriver.States.ParallelStates", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FParallelStatesTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST_NO_STATES()

	// Total states to test.
	int32 Rows = 2;
	int32 Branches = 2;
	TArray<UEdGraphPin*> LastStatePins;

	// A -> (B, C) Single
	TestHelpers::BuildBranchingStateMachine(this, StateMachineGraph, Rows, Branches, false, &LastStatePins);

	int32 EntryHits = 0; int32 UpdateHits = 0; int32 EndHits = 0;
	USMInstance* Instance = TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits);
	TestEqual("States hit linearly", EntryHits, Branches);

	// A -> (B, C) Parallel
	LastStatePins.Reset();
	FSMBlueprintEditorUtils::RemoveAllNodesFromGraph(StateMachineGraph, NewBP);
	TestHelpers::BuildBranchingStateMachine(this, StateMachineGraph, Rows, Branches, true, &LastStatePins);
	Instance = TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits, 1000, false);
	TestEqual("States hit parallel", EntryHits, Instance->GetStateMap().Num() - 1);

	// A -> (B, C, D, E) Parallel
	LastStatePins.Reset();
	FSMBlueprintEditorUtils::RemoveAllNodesFromGraph(StateMachineGraph, NewBP);
	Branches = 4;
	TestHelpers::BuildBranchingStateMachine(this, StateMachineGraph, Rows, Branches, true, &LastStatePins);
	Instance = TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits, 1000, false);
	TestEqual("States hit parallel", EntryHits, Instance->GetStateMap().Num() - 1);

	// A -> (B -> (B1 -> ..., B2-> ...), C -> ...) Parallel
	LastStatePins.Reset();
	FSMBlueprintEditorUtils::RemoveAllNodesFromGraph(StateMachineGraph, NewBP);
	Rows = 4;
	Branches = 2;
	TestHelpers::BuildBranchingStateMachine(this, StateMachineGraph, Rows, Branches, true, &LastStatePins);
	Instance = TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits, 1000, false);
	TestEqual("States hit parallel", EntryHits, Instance->GetStateMap().Num() - 1);

	LastStatePins.Reset();
	FSMBlueprintEditorUtils::RemoveAllNodesFromGraph(StateMachineGraph, NewBP);
	Rows = 3;
	Branches = 3;
	TestHelpers::BuildBranchingStateMachine(this, StateMachineGraph, Rows, Branches, true, &LastStatePins);
	Instance = TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits, 1000, false);
	TestEqual("States hit parallel", EntryHits, Instance->GetStateMap().Num() - 1);
	
	{
		TArray<FGuid> ActiveGuids;
		Instance->GetAllActiveStateGuids(ActiveGuids);
		const float TotalActiveEndStates = FMath::Pow((float)Branches, (float)Rows); // Only end states are active.

		TestEqual("Active guids match end states.", ActiveGuids.Num(), (int32)TotalActiveEndStates);

		// Reset and reload. Only end states should be active.
		Instance->Shutdown();
		Instance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, NewObject<USMTestContext>());
		Instance->LoadFromMultipleStates(ActiveGuids);

		TestEqual("All initial states set", Instance->GetRootStateMachine().GetInitialStates().Num(), ActiveGuids.Num());
		Instance->Start();
		TestEqual("All states reloaded", TestHelpers::ArrayContentsInArray(Instance->GetAllActiveStateGuidsCopy(), ActiveGuids), ActiveGuids.Num());
	}

	// Test with leaving states active.
	{
		LastStatePins.Reset();
		FSMBlueprintEditorUtils::RemoveAllNodesFromGraph(StateMachineGraph, NewBP);
		Rows = 3;
		Branches = 3;
		TestHelpers::BuildBranchingStateMachine(this, StateMachineGraph, Rows, Branches, true, &LastStatePins, true);
		Instance = TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits, 1000, false);
		TestEqual("States hit parallel", EntryHits, Instance->GetStateMap().Num() - 1);

		TArray<FGuid> ActiveGuids;
		Instance->GetAllActiveStateGuids(ActiveGuids);

		TestEqual("Active guids match all states.", ActiveGuids.Num(), Instance->GetStateMap().Num() - 1);

		bool bSetActiveNow = true;
		TestActiveNow:
		
		// Reset and reload.
		Instance->Shutdown();
		Instance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, NewObject<USMTestContext>());
		Instance->LoadFromMultipleStates(ActiveGuids);

		TestEqual("All initial states set", Instance->GetRootStateMachine().GetInitialStates().Num(), ActiveGuids.Num());
		Instance->Start();
		TestEqual("All states reloaded", TestHelpers::ArrayContentsInArray(Instance->GetAllActiveStateGuidsCopy(), ActiveGuids), ActiveGuids.Num());

		// Test manually deactivating states.
		{
			auto TestActiveStateIsActive = [&](const USMStateInstance_Base* InState)
			{
				if (bSetActiveNow)
				{
					TestTrue("State is active", InState->IsActive());
				}
				else
				{
					TestFalse("State not active", InState->IsActive());
				}
			};
			
			TArray<USMStateInstance_Base*> StateInstances;
			Instance->GetAllStateInstances(StateInstances);

			StateInstances[1]->SetActive(false, true, bSetActiveNow);
			TestEqual("State active changed", TestHelpers::ArrayContentsInArray(Instance->GetAllActiveStateGuidsCopy(), ActiveGuids), ActiveGuids.Num() - 1);
			TestFalse("State not active", StateInstances[1]->IsActive());
			
			StateInstances[1]->SetActive(true, true, bSetActiveNow);
			TestEqual("State active changed", TestHelpers::ArrayContentsInArray(Instance->GetAllActiveStateGuidsCopy(), ActiveGuids), ActiveGuids.Num());
			TestActiveStateIsActive(StateInstances[1]);
			
			for (USMStateInstance_Base* StateInstance : StateInstances)
			{
				if (StateInstance == Instance->GetRootStateMachineNodeInstance())
				{
					continue;
				}
				StateInstance->SetActive(false, true, bSetActiveNow);
				TestFalse("State not active", StateInstance->IsActive());
			}

			TestEqual("State active changed", Instance->GetAllActiveStateGuidsCopy().Num(), 0);

			for (USMStateInstance_Base* StateInstance : StateInstances)
			{
				if (StateInstance == Instance->GetRootStateMachineNodeInstance())
				{
					continue;
				}
				StateInstance->SetActive(true, true, bSetActiveNow);
				TestActiveStateIsActive(StateInstance);
			}

			TestEqual("State active changed", TestHelpers::ArrayContentsInArray(Instance->GetAllActiveStateGuidsCopy(), ActiveGuids), ActiveGuids.Num());
		}

		if (bSetActiveNow)
		{
			bSetActiveNow = false;
			goto TestActiveNow;
		}
	}

	// Test state re-entry
	{
		LastStatePins.Reset();
		FSMBlueprintEditorUtils::RemoveAllNodesFromGraph(StateMachineGraph, NewBP);

		TestHelpers::BuildBranchingStateMachine(this, StateMachineGraph, 2, 1, true, &LastStatePins, true, true);
		Instance = TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits, 1000, false);
		TestEqual("States hit parallel", EntryHits, Instance->GetStateMap().Num() - 1);
		TestEqual("States hit parallel", UpdateHits, 1);
		TestEqual("States hit parallel", EndHits, 0);
		
		USMTestContext* Context = CastChecked<USMTestContext>(Instance->GetContext());
		
		int32 InitCount = Context->TestTransitionInit.Count;
		int32 ShutdownCount = Context->TestTransitionShutdown.Count;
		TestEqual("States init correct", InitCount, 1);
		TestEqual("States shutdown correct", ShutdownCount, 0);
		
		UpdateHits = Context->TimesUpdateHit.Count;
		
		Instance->Update(1.f);

		EntryHits = Context->GetEntryInt();
		UpdateHits = Context->TimesUpdateHit.Count;
		EndHits = Context->GetEndInt();

		const int32 ExpectedUpdates = 3;
		TestEqual("States hit parallel", EntryHits, Instance->GetStateMap().Num());
		TestEqual("States hit parallel", UpdateHits, ExpectedUpdates); // Each state updates again. Currently we let a state that was re-entered run its update logic in the same tick.
		TestEqual("States hit parallel", EndHits, 0);

		InitCount = Context->TestTransitionInit.Count;
		ShutdownCount = Context->TestTransitionShutdown.Count;
		TestEqual("States init correct", InitCount, 1);
		TestEqual("States shutdown correct", ShutdownCount, 0);
		
		// Without re-entry
		{
			LastStatePins.Reset();
			FSMBlueprintEditorUtils::RemoveAllNodesFromGraph(StateMachineGraph, NewBP);

			TestHelpers::BuildBranchingStateMachine(this, StateMachineGraph, 2, 1, true, &LastStatePins, true, false);
			Instance = TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits, 1000, false);
			
			TestEqual("States hit parallel", EntryHits, Instance->GetStateMap().Num() - 1);
			TestEqual("States hit parallel", UpdateHits, 1);
			TestEqual("States hit parallel", EndHits, 0);

			Instance->Update(1.f);

			Context = CastChecked<USMTestContext>(Instance->GetContext());
			EntryHits = Context->GetEntryInt();
			UpdateHits = Context->TimesUpdateHit.Count;
			EndHits = Context->GetEndInt();

			TestEqual("States hit parallel", EntryHits, Instance->GetStateMap().Num() - 1);
			TestEqual("States hit parallel", UpdateHits, 3); // Each state updates again.
			TestEqual("States hit parallel", EndHits, 0);

			InitCount = Context->TestTransitionInit.Count;
			ShutdownCount = Context->TestTransitionShutdown.Count;
			TestEqual("States init correct", InitCount, 1);
			TestEqual("States shutdown correct", ShutdownCount, 0);
		}

		// Without transition evaluation connecting to an already active state.
		{
			LastStatePins.Reset();
			FSMBlueprintEditorUtils::RemoveAllNodesFromGraph(StateMachineGraph, NewBP);

			TestHelpers::BuildBranchingStateMachine(this, StateMachineGraph, 2, 1, true, &LastStatePins, true, true, false);
			Instance = TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits, 1000, false);

			TestEqual("States hit parallel", EntryHits, Instance->GetStateMap().Num() - 1);
			TestEqual("States hit parallel", UpdateHits, 1);
			TestEqual("States hit parallel", EndHits, 0);

			Instance->Update(1.f);

			Context = CastChecked<USMTestContext>(Instance->GetContext());
			EntryHits = Context->GetEntryInt();
			UpdateHits = Context->TimesUpdateHit.Count;
			EndHits = Context->GetEndInt();

			TestEqual("States hit parallel", EntryHits, Instance->GetStateMap().Num() - 1);
			TestEqual("States hit parallel", UpdateHits, 3); // Each state updates again.
			TestEqual("States hit parallel", EndHits, 0);

			InitCount = Context->TestTransitionInit.Count;
			ShutdownCount = Context->TestTransitionShutdown.Count;
			TestEqual("States init correct", InitCount, 1);
			TestEqual("States shutdown correct", ShutdownCount, 0);
		}
	}
	
	return NewAsset.DeleteAsset(this);
}

/**
 * Test IsEndState is read correctly when bCanBeEndState is false.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCanBeEndStateTest, "LogicDriver.States.CanBeEndState", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCanBeEndStateTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(2)

	UEdGraphPin* LastStatePin = nullptr;

	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin);
	
	// Let the last node on the graph be the node after the new state machine.
	const USMGraphNode_StateNodeBase* LastState = CastChecked<USMGraphNode_StateNodeBase>(LastStatePin->GetOwningNode());
	TestTrue("Is End State", LastState->IsEndState());

	CastChecked<USMStateInstance>(LastState->GetNodeTemplate())->SetCanBeEndState(false);
	TestFalse("Not Is End State", LastState->IsEndState());
	
	TestHelpers::TestLinearStateMachine(this, NewBP, TotalStates);

	return true;
}

#endif

#endif //WITH_DEV_AUTOMATION_TESTS