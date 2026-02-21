// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMTestHelpers.h"
#include "SMTestContext.h"
#include "Helpers/SMTestBoilerplate.h"

#include "Utilities/SMBlueprintEditorUtils.h"

#include "Blueprints/SMBlueprint.h"

#include "Graph/SMGraph.h"
#include "Graph/SMIntermediateGraph.h"
#include "Graph/Nodes/SMGraphK2Node_StateMachineNode.h"
#include "Graph/Nodes/SMGraphNode_StateNode.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/Nodes/SMGraphNode_StateMachineStateNode.h"
#include "Graph/Nodes/FunctionNodes/SMGraphK2Node_StateReadNodes.h"
#include "Graph/Nodes/FunctionNodes/SMGraphK2Node_FunctionNodes_StateMachineRef.h"

#include "EdGraph/EdGraph.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/KismetEditorUtilities.h"

#if WITH_DEV_AUTOMATION_TESTS

#if PLATFORM_DESKTOP

/**
 * Tests GetStateMachineReference in the intermediate graph and validates it returns the correct reference.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStateRead_GetStateMachineReferenceTest, "LogicDriver.HelperNodes.StateRead_GetStateMachineReference", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FStateRead_GetStateMachineReferenceTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST_NO_STATES();

	UEdGraphPin* LastStatePin = nullptr;

	// Build top level state machine.
	{
		TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, 2, &LastStatePin);
		if (!NewAsset.SaveAsset(this))
		{
			return false;
		}
	}

	// Build a nested state machine.
	UEdGraphPin* EntryPointForNestedStateMachine = LastStatePin;
	UEdGraphPin* LastNestedPin = nullptr;
	USMGraphNode_StateMachineStateNode* NestedStateMachineNode = TestHelpers::BuildNestedStateMachine(this, StateMachineGraph, 4, &EntryPointForNestedStateMachine, &LastNestedPin);

	// Add more top level.
	{
		LastStatePin = NestedStateMachineNode->GetOutputPin();
		TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, 2, &LastStatePin);
		if (!NewAsset.SaveAsset(this))
		{
			return false;
		}

		// Signal the state after the nested state machine to wait for its completion.
		USMGraphNode_TransitionEdge* TransitionFromNestedStateMachine = CastChecked<USMGraphNode_TransitionEdge>(NestedStateMachineNode->GetOutputPin()->LinkedTo[0]->GetOwningNode());
		TestHelpers::OverrideTransitionResultLogic<USMGraphK2Node_StateMachineReadNode_InEndState>(this, TransitionFromNestedStateMachine);
	}
	TestTrue("Nested state machine has correct node count", NestedStateMachineNode->GetBoundGraph()->Nodes.Num() > 1);

	USMBlueprint* NewReferencedBlueprint = FSMBlueprintEditorUtils::ConvertStateMachineToReference(NestedStateMachineNode, false, nullptr, nullptr);
	FKismetEditorUtilities::CompileBlueprint(NewReferencedBlueprint);
	
	// Store handler information so we can delete the object.
	FAssetHandler ReferencedAsset = TestHelpers::CreateAssetFromBlueprint(NewReferencedBlueprint);
	
	TestNotNull("New referenced blueprint created", NewReferencedBlueprint);
	TestHelpers::TestStateMachineConvertedToReference(this, NestedStateMachineNode);

	// Now convert the state machine to a reference.
	NestedStateMachineNode->SetUseIntermediateGraph(true);
	
	// Find the intermediate graph which should have been created.
	TSet<USMIntermediateGraph*> Graphs;
	FSMBlueprintEditorUtils::GetAllGraphsOfClassNested<USMIntermediateGraph>(NestedStateMachineNode->GetBoundGraph(), Graphs);

	TestTrue("Intermediate Graph Found", Graphs.Num() == 1);

	USMGraphK2Node_StateMachineRef_Stop* StopNode = nullptr;
	USMIntermediateGraph* IntermediateGraph = nullptr;
	for (USMIntermediateGraph* Graph : Graphs)
	{
		IntermediateGraph = Graph;
		TArray< USMGraphK2Node_StateMachineRef_Stop*> StopNodes;
		FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphK2Node_StateMachineRef_Stop>(Graph, StopNodes);
		TestTrue("Stop Node Found", StopNodes.Num() == 1);
		StopNode = StopNodes[0];
	}

	if (StopNode == nullptr)
	{
		return false;
	}

	UEdGraphPin* ContextOutPin = nullptr;
	UK2Node_CallFunction* GetContextNode = TestHelpers::CreateContextGetter(this, IntermediateGraph, &ContextOutPin);

	// Add a call to read from the context.
	UK2Node_CallFunction* SetReference = TestHelpers::CreateFunctionCall(IntermediateGraph, USMTestContext::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMTestContext, SetTestReference)));

	USMGraphK2Node_StateReadNode_GetStateMachineReference* GetReference = TestHelpers::CreateNewNode<USMGraphK2Node_StateReadNode_GetStateMachineReference>(this, IntermediateGraph, SetReference->FindPin(FName("Instance")), false);
	TestNotNull("Expected helper node to be created", GetReference);

	UK2Node_DynamicCast* CastNode = TestHelpers::CreateAndLinkPureCastNode(this, IntermediateGraph, ContextOutPin, SetReference->FindPin(TEXT("self"), EGPD_Input));
	TestNotNull("Context linked to member function set reference", CastNode);

	const bool bWired = IntermediateGraph->GetSchema()->TryCreateConnection(StopNode->FindPin(FName("then")), SetReference->GetExecPin());
	TestTrue("Wired execution from stop node to set reference", bWired);

	FKismetEditorUtilities::CompileBlueprint(NewBP);
	
	USMTestContext* Context = NewObject<USMTestContext>();
	USMInstance* StateMachineInstance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);

	const FGuid PathGuid = FSMBlueprintEditorUtils::TryCreatePathGuid(IntermediateGraph);
	
	USMInstance* ReferenceInstance = StateMachineInstance->GetReferencedInstanceByGuid(PathGuid);
	TestNotNull("Real reference exists", ReferenceInstance);
	TestNull("TestReference not set", Context->TestReference);

	TestHelpers::RunAllStateMachinesToCompletion(this, StateMachineInstance, &StateMachineInstance->GetRootStateMachine());

	TestNotNull("TestReference set from blueprint graph", Context->TestReference.Get());
	TestNotEqual("Test reference is not the root instance", StateMachineInstance, Context->TestReference.Get());
	TestEqual("Found reference equals real reference", Context->TestReference.Get(), ReferenceInstance);
	
	ReferencedAsset.DeleteAsset(this);
	return NewAsset.DeleteAsset(this);
}

/**
 * Assemble and run a hierarchical state machine and wait for the internal state machine to finish.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStateRead_InEndStateTest, "LogicDriver.HelperNodes.StateRead_InEndState", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FStateRead_InEndStateTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST_NO_STATES()

	// Total states to test.
	int32 TotalStates = 0;
	int32 TotalTopLevelStates = 0;
	UEdGraphPin* LastStatePin = nullptr;

	// Build top level state machine.
	{
		const int32 CurrentStates = 2;
		TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, CurrentStates, &LastStatePin);
		if (!NewAsset.SaveAsset(this))
		{
			return false;
		}
		TotalStates += CurrentStates;
		TotalTopLevelStates += CurrentStates;
	}

	// Build a nested state machine.
	UEdGraphPin* EntryPointForNestedStateMachine = LastStatePin;
	USMGraphNode_StateMachineStateNode* NestedStateMachineNode = TestHelpers::CreateNewNode<USMGraphNode_StateMachineStateNode>(this, StateMachineGraph, EntryPointForNestedStateMachine);

	UEdGraphPin* LastNestedPin = nullptr;
	{
		const int32 CurrentStates = 10;
		TestHelpers::BuildLinearStateMachine(this, Cast<USMGraph>(NestedStateMachineNode->GetBoundGraph()), CurrentStates, &LastNestedPin);
		LastStatePin = NestedStateMachineNode->GetOutputPin();

		TotalStates += CurrentStates;
		TotalTopLevelStates += 1;
	}

	// Add logic to the state machine transition.
	USMGraphNode_TransitionEdge* TransitionToNestedStateMachine = CastChecked<USMGraphNode_TransitionEdge>(NestedStateMachineNode->GetInputPin()->LinkedTo[0]->GetOwningNode());
	TestHelpers::AddTransitionResultLogic(this, TransitionToNestedStateMachine);

	// Add more top level (states leading from nested state machine).
	{
		const int32 CurrentStates = 10;
		TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, CurrentStates, &LastStatePin);
		if (!NewAsset.SaveAsset(this))
		{
			return false;
		}
		TotalStates += CurrentStates;
		TotalTopLevelStates += CurrentStates;
	}

	// This will run the nested machine only up to the first state.
	TestHelpers::TestLinearStateMachine(this, NewBP, TotalTopLevelStates);

	int32 ExpectedEntryValue = TotalTopLevelStates;

	// Run the same machine until an end state is reached. The result should be the same as the top level machine won't wait for the nested machine.
	{
		int32 EntryHits = 0; int32 UpdateHits = 0; int32 EndHits = 0;
		TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits);

		TestEqual("State Machine generated value", EntryHits, ExpectedEntryValue);
		TestEqual("State Machine generated value", UpdateHits, 0);
		TestEqual("State Machine generated value", EndHits, ExpectedEntryValue);
	}

	// Now let's try waiting for the nested state machine. Clear the graph except for the result node.
	{
		USMGraphNode_TransitionEdge* TransitionFromNestedStateMachine = CastChecked<USMGraphNode_TransitionEdge>(NestedStateMachineNode->GetOutputPin()->LinkedTo[0]->GetOwningNode());
		TestHelpers::OverrideTransitionResultLogic<USMGraphK2Node_StateMachineReadNode_InEndState>(this, TransitionFromNestedStateMachine);

		ExpectedEntryValue = TotalStates;

		// Run the same machine until an end state is reached. This time the result should be modified by all nested states.
		int32 EntryHits = 0; int32 UpdateHits = 0; int32 EndHits = 0;
		TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits);

		TestEqual("State Machine generated value", EntryHits, ExpectedEntryValue);
		TestEqual("State Machine generated value", UpdateHits, 0);
		TestEqual("State Machine generated value", EndHits, ExpectedEntryValue);
	}

	return NewAsset.DeleteAsset(this);
}

/**
 * Test transitioning from a state after a time period.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStateRead_TimeInStateTest, "LogicDriver.HelperNodes.StateRead_TimeInState", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FStateRead_TimeInStateTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(0)
	UEdGraphPin* LastStatePin = nullptr;

	// Build a state machine of only two states.
	{
		const int32 CurrentStates = 2;
		TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, CurrentStates, &LastStatePin);
		if (!NewAsset.SaveAsset(this))
		{
			return false;
		}
		TotalStates += CurrentStates;
	}

	int32 ExpectedEntryValue = TotalStates;

	// Run as normal.
	{
		int32 EntryHits = 0; int32 UpdateHits = 0; int32 EndHits = 0;
		TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits);
	}

	// Now let's try waiting for the first state. Each tick increments the update count by one.
	{
		USMGraphNode_TransitionEdge* TransitionFromNestedStateMachine =
			CastChecked<USMGraphNode_TransitionEdge>(Cast<USMGraphNode_StateNode>(LastStatePin->GetOwningNode())->GetInputPin()->LinkedTo[0]->GetOwningNode());

		UEdGraph* TransitionGraph = TransitionFromNestedStateMachine->GetBoundGraph();
		TransitionGraph->Nodes.Empty();
		TransitionGraph->GetSchema()->CreateDefaultNodesForGraph(*TransitionGraph);

		TestHelpers::AddSpecialFloatTransitionLogic<USMGraphK2Node_StateReadNode_TimeInState>(this, TransitionFromNestedStateMachine);

		// Run again. By default the transition will wait until time in state is greater than the value of USMTestContext::GreaterThanTest.
		int32 EntryHits = 0; int32 UpdateHits = 0; int32 EndHits = 0;
		TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits);

		TestEqual("State Machine generated value", EntryHits, ExpectedEntryValue);
		TestEqual("State Machine generated value", UpdateHits, (int32)USMTestContext::GreaterThanTest + 1);
		TestEqual("State Machine generated value", EndHits, ExpectedEntryValue);
	}

	return NewAsset.DeleteAsset(this);
}

#endif

#endif //WITH_DEV_AUTOMATION_TESTS