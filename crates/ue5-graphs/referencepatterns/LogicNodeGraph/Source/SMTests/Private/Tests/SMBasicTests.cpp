// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Helpers/SMTestBoilerplate.h"
#include "SMTestContext.h"
#include "SMTestHelpers.h"

#include "Blueprints/SMBlueprint.h"
#include "Blueprints/SMBlueprintGeneratedClass.h"

#include "Graph/Nodes/SMGraphK2Node_StateMachineNode.h"
#include "Graph/Nodes/SMGraphNode_ConduitNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineEntryNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineStateNode.h"
#include "Graph/Nodes/SMGraphNode_StateNode.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/SMGraph.h"
#include "Graph/SMStateGraph.h"
#include "Utilities/SMBlueprintEditorUtils.h"
#include "Utilities/SMVersionUtils.h"

#include "EdGraph/EdGraph.h"
#include "Kismet2/KismetEditorUtilities.h"

#if WITH_DEV_AUTOMATION_TESTS

#if PLATFORM_DESKTOP

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCreateAssetTest, "LogicDriver.Basic.CreateAsset", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCreateAssetTest::RunTest(const FString& Parameters)
{
	FAssetHandler NewAsset;
	if (!TestHelpers::TryCreateNewStateMachineAsset(this, NewAsset, true))
	{
		return false;
	}

	// Verify correct type created.
	USMBlueprint* NewBP = NewAsset.GetObjectAs<USMBlueprint>();
	TestNotNull("New asset object should be USMBlueprint", NewBP);

	{
		USMBlueprintGeneratedClass* GeneratedClass = Cast<USMBlueprintGeneratedClass>(NewBP->GetGeneratedClass());
		TestNotNull("Generated Class should match expected class", GeneratedClass);

		// Verify new version set correctly.
		TestTrue("Instance version is correctly created", FSMVersionUtils::IsAssetUpToDate(NewBP));
	}

	bool bReverify = false;

Verify:

	TestHelpers::ValidateNewStateMachineBlueprint(this, NewBP);

	// Verify reloading asset works properly.
	if (!bReverify)
	{
		if (!NewAsset.LoadAsset(this))
		{
			return false;
		}

		NewBP = NewAsset.GetObjectAs<USMBlueprint>();
		TestNotNull("New asset object should be USMBlueprint", NewBP);

		USMBlueprintGeneratedClass* GeneratedClass = Cast<USMBlueprintGeneratedClass>(NewBP->GetGeneratedClass());
		TestNotNull("Generated Class should match expected class", GeneratedClass);

		//////////////////////////////////////////////////////////////////////////
		// ** If changing instance version number change this test. **
		//////////////////////////////////////////////////////////////////////////
		// Verify version matches.
		TestTrue("Instance version is correctly created", FSMVersionUtils::IsAssetUpToDate(NewBP));

		bReverify = true;
		goto Verify;
	}

	return NewAsset.DeleteAsset(this);
}

/**
 * Test deleting by both node and graph.
 * Deletion has some circular logic involved so we want to make sure we don't get stuck in a loop and that everything cleans up properly.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeleteDeleteNodeTest, "LogicDriver.Basic.DeleteNode", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FDeleteDeleteNodeTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(0)
	UEdGraphPin* LastStatePin = nullptr;

	// Build a state machine of three states.
	{
		const int32 CurrentStates = 3;
		TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, CurrentStates, &LastStatePin);
		if (!NewAsset.SaveAsset(this))
		{
			return false;
		}
		TotalStates += CurrentStates;
	}

	// Verify works before deleting.
	{
		const int32 ExpectedValue = TotalStates;
		int32 EntryHits = 0; int32 UpdateHits = 0; int32 EndHits = 0;
		TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits);
		TestEqual("State Machine generated value", EntryHits, ExpectedValue);
		TestEqual("State Machine generated value", UpdateHits, 0);
		TestEqual("State Machine generated value", EndHits, ExpectedValue);
	}


	// Test deleting the last node by deleting the node.
	{
		USMGraphNode_StateNode* LastStateNode = CastChecked<USMGraphNode_StateNode>(LastStatePin->GetOwningNode());
		USMStateGraph* StateGraph = CastChecked<USMStateGraph>(LastStateNode->GetBoundGraph());
		USMGraph* OwningGraph = LastStateNode->GetOwningStateMachineGraph();

		// Set last state pin to the previous state.
		LastStatePin = CastChecked<USMGraphNode_TransitionEdge>(LastStateNode->GetInputPin()->LinkedTo[0]->GetOwningNode())->GetInputPin()->LinkedTo[0];

		TestTrue("State Machine Graph should own State Node", OwningGraph->Nodes.Contains(LastStateNode));
		TestTrue("State Machine Graph should have a State Graph subgraph", OwningGraph->SubGraphs.Contains(StateGraph));

		FSMBlueprintEditorUtils::RemoveNode(NewBP, LastStateNode, true);

		TestTrue("State Machine Graph should not own State Node", !OwningGraph->Nodes.Contains(LastStateNode));
		TestTrue("State Machine Graph should not have a State Graph subgraph", !OwningGraph->SubGraphs.Contains(StateGraph));

		TotalStates--;

		// Verify runs without last state.
		{
			const int32 ExpectedValue = TotalStates;
			int32 EntryHits = 0; int32 UpdateHits = 0; int32 EndHits = 0;
			TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits);
			TestEqual("State Machine generated value", EntryHits, ExpectedValue);
			TestEqual("State Machine generated value", UpdateHits, 0);
			TestEqual("State Machine generated value", EndHits, ExpectedValue);
		}
	}

	// Test deleting the last node by deleting the graph.
	{
		USMGraphNode_StateNode* LastStateNode = CastChecked<USMGraphNode_StateNode>(LastStatePin->GetOwningNode());
		USMStateGraph* StateGraph = CastChecked<USMStateGraph>(LastStateNode->GetBoundGraph());
		USMGraph* OwningGraph = LastStateNode->GetOwningStateMachineGraph();

		TestTrue("State Machine Graph should own State Node", OwningGraph->Nodes.Contains(LastStateNode));
		TestTrue("State Machine Graph should have a State Graph subgraph", OwningGraph->SubGraphs.Contains(StateGraph));

		FSMBlueprintEditorUtils::RemoveGraph(NewBP, StateGraph);

		TestTrue("State Machine Graph should not own State Node", !OwningGraph->Nodes.Contains(LastStateNode));
		TestTrue("State Machine Graph should not have a State Graph subgraph", !OwningGraph->SubGraphs.Contains(StateGraph));

		TotalStates--;

		// Verify runs without last state.
		{
			const int32 ExpectedValue = TotalStates;
			int32 EntryHits = 0; int32 UpdateHits = 0; int32 EndHits = 0;
			TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits);
			TestEqual("State Machine generated value", EntryHits, ExpectedValue);
			TestEqual("State Machine generated value", UpdateHits, 0);
			TestEqual("State Machine generated value", EndHits, ExpectedValue);
		}
	}

	return NewAsset.DeleteAsset(this);
}

/**
 * Assemble and run a hierarchical state machine.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssembleStateMachineTest, "LogicDriver.Basic.AssembleStateMachine", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FAssembleStateMachineTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(1)

	UEdGraphPin* LastStatePin = nullptr;

	// Build single state - state machine.
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, 1, &LastStatePin);
	if (!NewAsset.SaveAsset(this))
	{
		return false;
	}
	TestHelpers::TestLinearStateMachine(this, NewBP, TotalStates);

	// Add on a second state.
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, 1, &LastStatePin, USMStateTestInstance::StaticClass(), USMTransitionTestInstance::StaticClass());
	if (!NewAsset.SaveAsset(this))
	{
		return false;
	}
	TotalStates++;
	TestHelpers::TestLinearStateMachine(this, NewBP, TotalStates);

	// Build a nested state machine.
	UEdGraphPin* EntryPointForNestedStateMachine = LastStatePin;
	USMGraphNode_StateMachineStateNode* NestedStateMachineNode = TestHelpers::CreateNewNode<USMGraphNode_StateMachineStateNode>(this, StateMachineGraph, EntryPointForNestedStateMachine);

	UEdGraphPin* LastNestedPin = nullptr;
	{
		TestHelpers::BuildLinearStateMachine(this, Cast<USMGraph>(NestedStateMachineNode->GetBoundGraph()), 1, &LastNestedPin);
		LastStatePin = NestedStateMachineNode->GetOutputPin();
	}

	// Add logic to the state machine transition.
	USMGraphNode_TransitionEdge* TransitionToNestedStateMachine = CastChecked<USMGraphNode_TransitionEdge>(NestedStateMachineNode->GetInputPin()->LinkedTo[0]->GetOwningNode());
	TestHelpers::AddTransitionResultLogic(this, TransitionToNestedStateMachine);

	TotalStates += 1; // Nested machine is a single state.
	TestHelpers::TestLinearStateMachine(this, NewBP, TotalStates);

	// Add more top level.
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, 10, &LastStatePin);
	if (!NewAsset.SaveAsset(this))
	{
		return false;
	}
	TotalStates += 10;

	// This will run the nested machine only up to the first state.
	TestHelpers::TestLinearStateMachine(this, NewBP, TotalStates);

	const int32 ExpectedEntryValue = TotalStates;
	// Run the same machine until an end state is reached.
	{
		int32 EntryHits = 0; int32 UpdateHits = 0; int32 EndHits = 0;
		TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits);

		TestEqual("State Machine generated value", EntryHits, ExpectedEntryValue);
		TestEqual("State Machine generated value", UpdateHits, 0);
		TestEqual("State Machine generated value", EndHits, ExpectedEntryValue);
	}

	// Add to the nested state machine
	{
		TestHelpers::BuildLinearStateMachine(this, Cast<USMGraph>(NestedStateMachineNode->GetBoundGraph()), 10, &LastNestedPin);
		TotalStates += 10;
	}

	// Run the same machine until an end state is reached. The result should be the same as the top level machine won't wait for the nested machine.
	{
		int32 EntryHits = 0; int32 UpdateHits = 0; int32 EndHits = 0;
		TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits);

		TestEqual("State Machine generated value", EntryHits, ExpectedEntryValue);
		TestEqual("State Machine generated value", UpdateHits, 0);
		TestEqual("State Machine generated value", EndHits, ExpectedEntryValue);
	}

	// Run the same machine until an end state is reached. This time we force states to update when ending.
	{
		TArray<USMGraphNode_StateNode*> TopLevelStates;
		FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphNode_StateNode>(StateMachineGraph, TopLevelStates);
		check(TopLevelStates.Num() > 0);

		for (USMGraphNode_StateNode* State : TopLevelStates)
		{
			State->GetNodeTemplateAs<USMStateInstance_Base>(true)->SetAlwaysUpdate(true);
		}

		int32 EntryHits = 0; int32 UpdateHits = 0; int32 EndHits = 0;
		TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits);

		TestEqual("State Machine generated value", EntryHits, ExpectedEntryValue);
		// Last update entry is called after stopping meaning UpdateTime is 0, which we used to test updates.
		TestEqual("State Machine generated value", UpdateHits, ExpectedEntryValue - 1);
		TestEqual("State Machine generated value", EndHits, ExpectedEntryValue);
	}

	return NewAsset.DeleteAsset(this);
}

/**
 * Test a single tick vs double tick to start state and evaluate transitions.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStateNodeSingleTickTest, "LogicDriver.Basic.SingleTick", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FStateNodeSingleTickTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(2)

	UEdGraphPin* LastStatePin = nullptr;

	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin);
	if (!NewAsset.SaveAsset(this))
	{
		return false;
	}
	
	TestHelpers::TestLinearStateMachine(this, NewBP, TotalStates);

	int32 ExpectedEntryValue = TotalStates;
	// Run with normal tick approach.
	{
		int32 EntryHits = 0; int32 UpdateHits = 0; int32 EndHits = 0;
		USMInstance* TestedStateMachine = TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits, 0, false, false);

		TestFalse("State machine not in last state", TestedStateMachine->IsInEndState());
		
		TestNotEqual("State Machine generated value", EntryHits, ExpectedEntryValue);
		TestNotEqual("State Machine generated value", EndHits, ExpectedEntryValue);
	}
	{
		TArray<USMGraphNode_StateNodeBase*> StateNodes;
		FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphNode_StateNodeBase>(StateMachineGraph, StateNodes);
		StateNodes[1]->GetNodeTemplateAs<USMStateInstance_Base>(true)->SetEvalTransitionsOnStart(true);
		
		int32 EntryHits = 0; int32 UpdateHits = 0; int32 EndHits = 0;
		USMInstance* TestedStateMachine = TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits, 0, true, true);

		TestTrue("State machine in last state", TestedStateMachine->IsInEndState());

		TestEqual("State Machine generated value", EntryHits, ExpectedEntryValue);
		TestEqual("State Machine generated value", EndHits, ExpectedEntryValue);

		// Test custom transition values.
		USMGraphNode_TransitionEdge* Transition = StateNodes[1]->GetNextTransition(0);
		Transition->GetNodeTemplateAs<USMTransitionInstance>()->SetCanEvalWithStartState(false);
		TestedStateMachine = TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits, 0, false, false);

		TestFalse("State machine in last state", TestedStateMachine->IsInEndState());

		TestNotEqual("State Machine generated value", EntryHits, ExpectedEntryValue);
		TestNotEqual("State Machine generated value", EndHits, ExpectedEntryValue);
	}
	// Test larger on same tick
	{
		FSMBlueprintEditorUtils::RemoveAllNodesFromGraph(StateMachineGraph);
		ExpectedEntryValue = TotalStates = 10;
		
		LastStatePin = nullptr;
		TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin);

		TArray<USMGraphNode_StateNodeBase*> StateNodes;
		FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphNode_StateNodeBase>(StateMachineGraph, StateNodes);

		for (USMGraphNode_StateNodeBase* Node : StateNodes)
		{
			if (Node->IsA<USMGraphNode_StateMachineEntryNode>())
			{
				continue;
			}
			Node->GetNodeTemplateAs<USMStateInstance_Base>(true)->SetEvalTransitionsOnStart(true);
		}
		
		int32 EntryHits = 0; int32 UpdateHits = 0; int32 EndHits = 0;
		USMInstance* TestedStateMachine = TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits, 0, true, true);

		TestTrue("State machine in last state", TestedStateMachine->IsInEndState());

		TestEqual("State Machine generated value", EntryHits, ExpectedEntryValue);
		TestEqual("State Machine generated value", EndHits, ExpectedEntryValue);
	}
	
	return NewAsset.DeleteAsset(this);
}

static void TestConduit(FAutomationTestBase* Test, const USMOrderTransition* IncomingTransition, const USMOrderConduit* Conduit, const USMOrderTransition* OutgoingTransition, bool bStateMachineCompleted)
{
	const bool bStateCompleted = bStateMachineCompleted || Conduit->IsEntryState();

	Test->TestEqual("Conduit methods hit", Conduit->ConduitInitializedHit.Count, 1);
	Test->TestEqual("Conduit methods hit", Conduit->ConduitShutdownHit.Count, 1);
	Test->TestEqual("Conduit methods hit", Conduit->ConduitEnteredEventHit.Count, bStateCompleted);

	if (bStateMachineCompleted)
	{
		Test->TestTrue("Conduit ran in correct order", 0 < Conduit->Time_Initialize);
		Test->TestTrue("Conduit ran in correct order", Conduit->Time_Initialize < Conduit->Time_Shutdown);
		Test->TestTrue("Conduit ran in correct order", Conduit->Time_Shutdown < Conduit->Time_Entered);
	}

	if (IncomingTransition)
	{
		Test->TestEqual("Transition methods hit", IncomingTransition->TransitionInitializedHit.Count, 1);
		Test->TestEqual("Transition methods hit", IncomingTransition->TransitionShutdownHit.Count, 1);
		Test->TestEqual("Transition methods hit", IncomingTransition->TransitionEnteredEventHit.Count, bStateCompleted);
		Test->TestEqual("Transition methods hit", IncomingTransition->TransitionRootSMStartHit.Count, 1);
		Test->TestEqual("Transition methods hit", IncomingTransition->TransitionRootSMStopHit.Count, 1);

		if (bStateMachineCompleted)
		{
			Test->TestTrue("Transition/Conduit ran in correct order", IncomingTransition->Time_Initialize < Conduit->Time_Initialize);
			Test->TestTrue("Transition/Conduit ran in correct order", IncomingTransition->Time_Entered < Conduit->Time_Entered);
			Test->TestTrue("Transition/Conduit ran in correct order", IncomingTransition->Time_Shutdown < Conduit->Time_Shutdown);
		}
	}

	if (OutgoingTransition)
	{
		Test->TestEqual("Transition methods hit", OutgoingTransition->TransitionInitializedHit.Count, 1);
		Test->TestEqual("Transition methods hit", OutgoingTransition->TransitionShutdownHit.Count, 1);
		Test->TestEqual("Transition methods hit", OutgoingTransition->TransitionEnteredEventHit.Count, bStateCompleted);
		Test->TestEqual("Transition methods hit", OutgoingTransition->TransitionRootSMStartHit.Count, 1);
		Test->TestEqual("Transition methods hit", OutgoingTransition->TransitionRootSMStopHit.Count, 1);

		if (bStateMachineCompleted)
		{
			Test->TestTrue("Transition/Conduit ran in correct order", Conduit->Time_Initialize < OutgoingTransition->Time_Initialize);
			Test->TestTrue("Transition/Conduit ran in correct order", Conduit->Time_Entered < OutgoingTransition->Time_Entered);
			Test->TestTrue("Transition/Conduit ran in correct order", Conduit->Time_Shutdown < OutgoingTransition->Time_Shutdown);
		}
	}
}

struct FSMTestResultData
{
	int32 StatesStarted = 0;
	int32 StatesChanged = 0;
	int32 TransitionsTaken = 0;
};

static void TestStandardOrder(FAutomationTestBase* Test, const USMStateInstance_Base* State, const USMOrderTransition* OutgoingTransition,
	bool bStateMachineCompleted)
{
	check(State);

	const bool bStateCompleted = bStateMachineCompleted || State->IsEntryState();

	if (const USMStateTestInstance* StateTest = Cast<USMStateTestInstance>(State))
	{
		Test->TestEqual("State methods hit", StateTest->StateBeginHit.Count, bStateCompleted);
		Test->TestEqual("State methods hit", StateTest->StateEndHit.Count, bStateCompleted);
		Test->TestEqual("State methods hit", StateTest->StateInitializedEventHit.Count, bStateCompleted);
		Test->TestEqual("State methods hit", StateTest->StateShutdownEventHit.Count, bStateCompleted);
		Test->TestEqual("State methods hit", StateTest->StateMachineStartHit.Count, 1);
		Test->TestEqual("State methods hit", StateTest->StateMachineStopHit.Count, 1);
		
		if (State->HasUpdated())
		{
			Test->TestEqual("State methods hit", StateTest->StateUpdateHit.Count, 1);
		}
		else
		{
			Test->TestEqual("State methods hit", StateTest->StateUpdateHit.Count, 0);
		}
	}

	if (const USMOrderState* OrderState = Cast<USMOrderState>(State))
	{
		if (bStateMachineCompleted)
		{
			Test->TestTrue("State ran in correct order", OrderState->Time_RootStart < OrderState->Time_Initialize);
			Test->TestTrue("State ran in correct order", OrderState->Time_Initialize < OrderState->Time_Start);

			if (State->HasUpdated())
			{
				Test->TestTrue("State ran in correct order", OrderState->Time_Start < OrderState->Time_Update);
				Test->TestTrue("State ran in correct order", OrderState->Time_Update < OrderState->Time_End);
			}

			Test->TestTrue("State ran in correct order", OrderState->Time_Start < OrderState->Time_End);
			Test->TestTrue("State ran in correct order", OrderState->Time_End < OrderState->Time_Shutdown);
		}
		if (OutgoingTransition)
		{
			Test->TestEqual("TransitionInitializedHit method hit", OutgoingTransition->TransitionInitializedHit.Count, bStateCompleted);
			Test->TestEqual("TransitionShutdownHit method hit", OutgoingTransition->TransitionShutdownHit.Count, bStateCompleted);
			Test->TestEqual("TransitionEnteredEventHit method hit", OutgoingTransition->TransitionEnteredEventHit.Count, bStateMachineCompleted);
			Test->TestEqual("TransitionRootSMStartHit method hit", OutgoingTransition->TransitionRootSMStartHit.Count, 1);
			Test->TestEqual("TransitionRootSMStopHit method hit", OutgoingTransition->TransitionRootSMStopHit.Count, 1);

			if (bStateMachineCompleted)
			{
				Test->TestTrue("Transition ran in correct order", OutgoingTransition->Time_RootStart < OutgoingTransition->Time_Initialize);
				Test->TestTrue("Transition/State ran in correct order", OrderState->Time_Initialize < OutgoingTransition->Time_Initialize);
				Test->TestTrue("Transition ran in correct order", OutgoingTransition->Time_Initialize < OutgoingTransition->Time_Shutdown);
				Test->TestTrue("Transition ran in correct order", OutgoingTransition->Time_Shutdown < OutgoingTransition->Time_Entered);
				Test->TestTrue("Transition/State ran in correct order", OutgoingTransition->Time_Shutdown < OrderState->Time_Shutdown);
				Test->TestTrue("Transition ran in correct order", OutgoingTransition->Time_Shutdown < OutgoingTransition->Time_RootStop);
			}
		}
	}
	else if (const USMOrderStateMachine* OrderStateMachine = Cast<USMOrderStateMachine>(State))
	{
		if (bStateMachineCompleted)
		{
			Test->TestTrue("State machine ran in correct order", OrderStateMachine->Time_RootStart < OrderStateMachine->Time_Initialize);
			Test->TestTrue("State machine ran in correct order", OrderStateMachine->Time_Initialize < OrderStateMachine->Time_Start);
			if (State->HasUpdated())
			{
				Test->TestTrue("State machine ran in correct order", OrderStateMachine->Time_Start < OrderStateMachine->Time_Update);
				Test->TestTrue("State machine ran in correct order", OrderStateMachine->Time_Update < OrderStateMachine->Time_End);
			}

			Test->TestTrue("State machine ran in correct order", OrderStateMachine->Time_Start < OrderStateMachine->Time_End);
			Test->TestTrue("State machine ran in correct order", OrderStateMachine->Time_EndState < OrderStateMachine->Time_End);
			Test->TestTrue("State machine ran in correct order", OrderStateMachine->Time_End < OrderStateMachine->Time_OnCompleted);
			Test->TestTrue("State machine ran in correct order", OrderStateMachine->Time_OnCompleted < OrderStateMachine->Time_Shutdown);
		}

		if (OutgoingTransition)
		{
			Test->TestEqual("Transition methods hit", OutgoingTransition->TransitionInitializedHit.Count, bStateCompleted);
			Test->TestEqual("Transition methods hit", OutgoingTransition->TransitionShutdownHit.Count, bStateCompleted);
			Test->TestEqual("Transition methods hit", OutgoingTransition->TransitionEnteredEventHit.Count, bStateMachineCompleted);
			Test->TestEqual("Transition methods hit", OutgoingTransition->TransitionRootSMStartHit.Count, 1);
			Test->TestEqual("Transition methods hit", OutgoingTransition->TransitionRootSMStopHit.Count, 1);

			if (bStateMachineCompleted)
			{
				Test->TestTrue("Transition ran in correct order", OutgoingTransition->Time_RootStart < OutgoingTransition->Time_Initialize);
				Test->TestTrue("Transition/State ran in correct order", OrderStateMachine->Time_Initialize < OutgoingTransition->Time_Initialize);
				Test->TestTrue("Transition ran in correct order", OutgoingTransition->Time_Initialize < OutgoingTransition->Time_Shutdown);
				Test->TestTrue("Transition ran in correct order", OutgoingTransition->Time_Shutdown < OutgoingTransition->Time_Entered);
				Test->TestTrue("Transition/State ran in correct order", OutgoingTransition->Time_Shutdown < OrderStateMachine->Time_Shutdown);
				Test->TestTrue("Transition ran in correct order", OutgoingTransition->Time_Shutdown < OutgoingTransition->Time_RootStop);
			}
		}
	}
	else if (const USMOrderConduit* OrderConduit = Cast<USMOrderConduit>(State))
	{
		TArray<USMTransitionInstance*> IncomingTransitions;
		TArray<USMTransitionInstance*> OutgoingTransitions;
		OrderConduit->GetIncomingTransitions(IncomingTransitions, false);
		OrderConduit->GetOutgoingTransitions(OutgoingTransitions, false);

		// Only test 1 partial chain.
		const USMOrderTransition* PreviousTransition = IncomingTransitions.Num() ? Cast<USMOrderTransition>(IncomingTransitions[0]) : nullptr;
		const USMOrderTransition* NextTransition = OutgoingTransitions.Num() ? Cast<USMOrderTransition>(OutgoingTransitions[0]) : nullptr;

		TestConduit(Test, PreviousTransition, OrderConduit, NextTransition, bStateMachineCompleted);
	}
}

static void TestAllStates(FAutomationTestBase* Test, USMStateInstance_Base* StateToTest, bool bStateMachineCompleted, FSMTestResultData& OutResultData)
{
	if (const USMStateTestInstance* StateTest = Cast<USMStateTestInstance>(StateToTest))
	{
		OutResultData.StatesStarted += StateTest->StateBeginHit.Count > 0;
		OutResultData.StatesChanged += StateTest->StateBeginHit.Count > 0;
	}
	else if (const USMOrderConduit* OrderConduit = Cast<USMOrderConduit>(StateToTest))
	{
		// Only states changed need to be tracked for conduits, other notify events won't fire since we're only
		// testing for conduits that eval with transitions.
		OutResultData.StatesChanged += OrderConduit->Time_Entered > 0;
	}
	else if (const USMOrderStateMachine* StateMachineTest = Cast<USMOrderStateMachine>(StateToTest))
	{
		OutResultData.StatesStarted += StateMachineTest->Time_Start > 0;
		OutResultData.StatesChanged += StateMachineTest->Time_Start > 0;

		if (StateMachineTest->GetOwningNodeAs<FSMStateMachine>() !=
			&StateMachineTest->GetStateMachineInstance(true)->GetRootStateMachine())
		{
			// This is testing when a nested FSM stops as the notify event will fire to a null state.
			OutResultData.StatesChanged += StateMachineTest->Time_End > 0;
		}
	}
	
	TArray<USMTransitionInstance*> TransitionsToTest;
	if (StateToTest->GetOutgoingTransitions(TransitionsToTest))
	{
		for (USMTransitionInstance* TransitionToTest : TransitionsToTest)
		{
			const USMOrderTransition* OrderTransition = CastChecked<USMOrderTransition>(TransitionToTest);
			OutResultData.TransitionsTaken += OrderTransition->TransitionEnteredEventHit.Count > 0;
			TestStandardOrder(Test, StateToTest, OrderTransition, bStateMachineCompleted);
			TestAllStates(Test, TransitionToTest->GetNextStateInstance(), bStateMachineCompleted, OutResultData);
		}
	}
	else
	{
		TestStandardOrder(Test, StateToTest, nullptr, bStateMachineCompleted);
	}
	
	if (const USMOrderStateMachine* StateMachineToTest = Cast<USMOrderStateMachine>(StateToTest))
	{
		TArray<USMStateInstance_Base*> EntryStates;
		StateMachineToTest->GetEntryStates(EntryStates);
		for (USMStateInstance_Base* State : EntryStates)
		{
			TestAllStates(Test, State, StateMachineToTest->GetWaitForEndState(), OutResultData);
		}
	}
};

static void RunOrderTest(FAutomationTestBase* Test, USMBlueprint* NewBP)
{
	CastChecked<USMInstance>(NewBP->GeneratedClass->ClassDefaultObject)->SetRootStateMachineNodeClass(USMOrderStateMachine::StaticClass());
	
	FKismetEditorUtilities::CompileBlueprint(NewBP);

	int32 X, Y, Z;
	int32 IterationsRan = 0;
	USMTestNotifyInstance* Instance = CastChecked<USMTestNotifyInstance>(TestHelpers::RunStateMachineToCompletion(Test, NewBP, X, Y, Z,
		1000, false, true, false, &IterationsRan));
	Instance->Stop();

	USMOrderStateMachine* TestMachine = CastChecked<USMOrderStateMachine>(Instance->GetRootStateMachineNodeInstance());
	TestMachine->SetWaitForEndState(true); // Just to help the test know this has completed.

	FSMTestResultData ResultData;
	TestAllStates(Test, TestMachine, true, ResultData);
	
	Test->TestEqual("NotifyStateChange occurred appropriate amount of times", Instance->TimesStateChangedHit, ResultData.StatesChanged);
	Test->TestEqual("State started occurred appropriate amount of times", Instance->TimesStateStartedHit, ResultData.StatesStarted);
	Test->TestEqual("Transitions taken occurred appropriate amount of times", Instance->TimesTransitionTakenHit, ResultData.TransitionsTaken);
}

static void RunCompleteOrderTest(FAutomationTestBase* Test, USMBlueprint* NewBP)
{
	// Test just the blueprint passed in.
	RunOrderTest(Test, NewBP);

	USMGraph* StateMachineGraph = (*FSMBlueprintEditorUtils::GetRootStateMachineNode(NewBP)).GetStateMachineGraph();
	check(StateMachineGraph);
	USMGraphNode_StateMachineEntryNode* EntryNode = StateMachineGraph->GetEntryNode();
	USMGraphNode_StateMachineStateNode* CollapsedOriginalStateMachineNode;

	// Collapse nodes to sub state machine, duplicate original nodes for top level.
	{
		EntryNode->GetOutputNode()->Rename(TEXT("ENTRY"), EntryNode->GetOuter(), REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders);

		TArray<UEdGraphNode*> OriginalNodes = StateMachineGraph->Nodes;
		
		TSet<UEdGraphNode*> DuplicatedNodes = TestHelpers::DuplicateNodes(StateMachineGraph->Nodes);
		USMGraphNode_StateNodeBase* DuplicatedEntryStateNode = nullptr;
		for (UEdGraphNode* Node : DuplicatedNodes)
		{
			if (Node->GetName().StartsWith(TEXT("ENTRY")))
			{
				DuplicatedEntryStateNode = CastChecked<USMGraphNode_StateNodeBase>(Node);
				break;
			}
		}

		check(DuplicatedEntryStateNode);

		TSet<UObject*> NodesToCollapse;
		NodesToCollapse.Reserve(OriginalNodes.Num());
		for (UEdGraphNode* Node : OriginalNodes)
		{
			NodesToCollapse.Add(Node);
		}
		
		CollapsedOriginalStateMachineNode =
			FSMBlueprintEditorUtils::CollapseNodesAndCreateStateMachine(NodesToCollapse);
		
		check(CollapsedOriginalStateMachineNode && CollapsedOriginalStateMachineNode->GetOwningStateMachineGraph()->GetEntryNode()->GetOutputNode());
		CollapsedOriginalStateMachineNode->SetNodeClass(USMOrderStateMachine::StaticClass());
		CollapsedOriginalStateMachineNode->GetNodeTemplateAs<USMStateMachineInstance>()->SetWaitForEndState(false);
		
		const UEdGraphSchema* Schema = EntryNode->GetGraph()->GetSchema();
		check(Schema->TryCreateConnection(CollapsedOriginalStateMachineNode->GetOutputPin(), DuplicatedEntryStateNode->GetInputPin()));

		TestHelpers::SetNodeClass(Test, CastChecked<USMGraphNode_TransitionEdge>(CollapsedOriginalStateMachineNode->GetOutputNode()),
			USMOrderTransition::StaticClass());
		
		// Run nested fsm connected to original.
		RunOrderTest(Test, NewBP);
	}

	// Wait for end state.
	{
		CollapsedOriginalStateMachineNode->GetNodeTemplateAs<USMStateMachineInstance>()->SetWaitForEndState(true);
		RunOrderTest(Test, NewBP);
	}

	// Convert collapsed FSM to references.
	USMBlueprint* NewReferencedBlueprint;
	{
		CollapsedOriginalStateMachineNode->GetNodeTemplateAs<USMStateMachineInstance>()->SetWaitForEndState(false);
		NewReferencedBlueprint = FSMBlueprintEditorUtils::ConvertStateMachineToReference(CollapsedOriginalStateMachineNode, false, nullptr, nullptr);
		FKismetEditorUtilities::CompileBlueprint(NewReferencedBlueprint);

		// Run the nested FSM as a reference connected to original.
		RunOrderTest(Test, NewBP);
	}

	// Wait for end state with reference.
	{
		CollapsedOriginalStateMachineNode->GetNodeTemplateAs<USMStateMachineInstance>()->SetWaitForEndState(true);
		RunOrderTest(Test, NewBP);
	}

	// Skip wait for end state with intermediate reference.
	{
		CollapsedOriginalStateMachineNode->GetNodeTemplateAs<USMStateMachineInstance>()->SetWaitForEndState(false);
		CollapsedOriginalStateMachineNode->SetUseIntermediateGraph(true);
		RunOrderTest(Test, NewBP);
	}

	// Wait for end state with intermediate reference.
	{
		CollapsedOriginalStateMachineNode->GetNodeTemplateAs<USMStateMachineInstance>()->SetWaitForEndState(true);
		CollapsedOriginalStateMachineNode->SetUseIntermediateGraph(true);
		RunOrderTest(Test, NewBP);
	}
	
	// Cleanup.
	{
		FAssetHandler ReferencedAsset = TestHelpers::CreateAssetFromBlueprint(NewReferencedBlueprint);
		ReferencedAsset.DeleteAsset(Test);
	}
}

/**
 * Test correct order of all operations.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrderOfOperationsStatesTest, "LogicDriver.Basic.OrderOfOperations.States", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FOrderOfOperationsStatesTest::RunTest(const FString& Parameters)
{
	LD::Tests::Setup::ParentAssetClass = USMTestNotifyInstance::StaticClass();
	SETUP_NEW_STATE_MACHINE_FOR_TEST(10)

	UEdGraphPin* LastStatePin = nullptr;

	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMOrderState::StaticClass(), USMOrderTransition::StaticClass());
	RunCompleteOrderTest(this, NewBP);
	
	return NewAsset.DeleteAsset(this);
}

/**
 * Test correct order of all operations.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrderOfOperationsConduitTest, "LogicDriver.Basic.OrderOfOperations.StatesWithConduit", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FOrderOfOperationsConduitTest::RunTest(const FString& Parameters)
{
	LD::Tests::Setup::ParentAssetClass = USMTestNotifyInstance::StaticClass();
	SETUP_NEW_STATE_MACHINE_FOR_TEST(10)

	UEdGraphPin* LastStatePin = nullptr;

	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin,
		USMOrderState::StaticClass(), USMOrderTransition::StaticClass(), false);
	CastChecked<USMInstance>(NewBP->GeneratedClass->ClassDefaultObject)->SetRootStateMachineNodeClass(USMOrderStateMachine::StaticClass());

	{
		USMGraphNode_StateNodeBase* FirstNode = CastChecked<USMGraphNode_StateNodeBase>(StateMachineGraph->GetEntryNode()->GetOutputNode());

		// This will become a conduit.
		USMGraphNode_StateNodeBase* SecondNode = CastChecked<USMGraphNode_StateNodeBase>(FirstNode->GetNextState());
		USMGraphNode_ConduitNode* ConduitNode = FSMBlueprintEditorUtils::ConvertNodeTo<USMGraphNode_ConduitNode>(SecondNode);
		TestHelpers::SetNodeClass(this, ConduitNode, USMOrderConduit::StaticClass());
	}
	
	RunCompleteOrderTest(this, NewBP);
	
	return NewAsset.DeleteAsset(this);
}

/**
 * Test correct order of all operations.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrderOfOperationsLongConduitTest, "LogicDriver.Basic.OrderOfOperations.StatesWithMultipleConduits", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FOrderOfOperationsLongConduitTest::RunTest(const FString& Parameters)
{
	LD::Tests::Setup::ParentAssetClass = USMTestNotifyInstance::StaticClass();
	SETUP_NEW_STATE_MACHINE_FOR_TEST(10)

	UEdGraphPin* LastStatePin = nullptr;

	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin,
		USMOrderState::StaticClass(), USMOrderTransition::StaticClass(), false);
	CastChecked<USMInstance>(NewBP->GeneratedClass->ClassDefaultObject)->SetRootStateMachineNodeClass(USMOrderStateMachine::StaticClass());

	{
		USMGraphNode_StateNodeBase* FirstNode = CastChecked<USMGraphNode_StateNodeBase>(StateMachineGraph->GetEntryNode()->GetOutputNode());

		// This will become a conduit.
		USMGraphNode_StateNodeBase* NextNode = CastChecked<USMGraphNode_StateNodeBase>(FirstNode->GetNextState());
		for (int32 Idx = 0; Idx < 3; ++Idx)
		{
			USMGraphNode_ConduitNode* ConduitNode = FSMBlueprintEditorUtils::ConvertNodeTo<USMGraphNode_ConduitNode>(NextNode);
			TestHelpers::SetNodeClass(this, ConduitNode, USMOrderConduit::StaticClass());
			NextNode = ConduitNode->GetNextState();
		}
	}

	RunCompleteOrderTest(this, NewBP);
	
	return NewAsset.DeleteAsset(this);
}

#endif

#endif //WITH_DEV_AUTOMATION_TESTS