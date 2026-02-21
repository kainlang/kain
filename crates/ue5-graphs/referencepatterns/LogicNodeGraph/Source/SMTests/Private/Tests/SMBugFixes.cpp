// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMTestHelpers.h"
#include "SMTestContext.h"
#include "Helpers/SMTestBoilerplate.h"

#include "Blueprints/SMBlueprint.h"

#include "Utilities/SMBlueprintEditorUtils.h"
#include "Graph/SMGraph.h"
#include "Graph/Nodes/SMGraphK2Node_StateMachineNode.h"
#include "Graph/Nodes/SMGraphNode_StateNode.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/Nodes/SMGraphNode_StateMachineStateNode.h"

#include "EdGraph/EdGraph.h"
#include "Kismet2/KismetEditorUtilities.h"

#if WITH_DEV_AUTOMATION_TESTS

#if PLATFORM_DESKTOP

/**
 * Test scenario where state machine has duplicate state and transition runtime guids and that they are properly fixed.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDuplicateRuntimeNodeTest, "LogicDriver.BugFixes.CheckDuplicateRuntimeNode", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FDuplicateRuntimeNodeTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(5)

	UEdGraphPin* LastStatePin = nullptr;

	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin);
	if (!NewAsset.SaveAsset(this))
	{
		return false;
	}
	int32 TotalDuplicated = FSMBlueprintEditorUtils::FixUpDuplicateRuntimeGuids(NewBP);

	TestEqual("No duplicates", TotalDuplicated, 0);

	// Set duplicate state nodes.
	{
		TArray<USMGraphNode_StateNodeBase*> StateNodes;
		FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphNode_StateNodeBase>(StateMachineGraph, StateNodes);
		FSMNode_Base* OriginalRuntimeNode = FSMBlueprintEditorUtils::GetRuntimeNodeFromGraph(StateNodes[1]->GetBoundGraph());

		FSMNode_Base* DupeRuntimeNode = FSMBlueprintEditorUtils::GetRuntimeNodeFromGraph(StateNodes[2]->GetBoundGraph());
		DupeRuntimeNode->SetNodeGuid(OriginalRuntimeNode->GetNodeGuid());
		FSMBlueprintEditorUtils::UpdateRuntimeNodeForGraph(DupeRuntimeNode, StateNodes[2]->GetBoundGraph());
	}

	// Set duplicate transition nodes.
	{
		TArray<USMGraphNode_TransitionEdge*> TransitionNodes;
		FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphNode_TransitionEdge>(StateMachineGraph, TransitionNodes);
		FSMNode_Base* OriginalRuntimeNode = FSMBlueprintEditorUtils::GetRuntimeNodeFromGraph(TransitionNodes[0]->GetBoundGraph());

		FSMNode_Base* DupeRuntimeNode = FSMBlueprintEditorUtils::GetRuntimeNodeFromGraph(TransitionNodes[1]->GetBoundGraph());
		DupeRuntimeNode->SetNodeGuid(OriginalRuntimeNode->GetNodeGuid());
		FSMBlueprintEditorUtils::UpdateRuntimeNodeForGraph(DupeRuntimeNode, TransitionNodes[1]->GetBoundGraph());

		DupeRuntimeNode = FSMBlueprintEditorUtils::GetRuntimeNodeFromGraph(TransitionNodes[3]->GetBoundGraph());
		DupeRuntimeNode->SetNodeGuid(OriginalRuntimeNode->GetNodeGuid());
		FSMBlueprintEditorUtils::UpdateRuntimeNodeForGraph(DupeRuntimeNode, TransitionNodes[3]->GetBoundGraph());
	}

	TotalDuplicated = FSMBlueprintEditorUtils::FixUpDuplicateRuntimeGuids(NewBP);
	TestEqual("Duplicates", TotalDuplicated, 3);

	TotalDuplicated = FSMBlueprintEditorUtils::FixUpDuplicateRuntimeGuids(NewBP);
	TestEqual("All duplicates fixed", TotalDuplicated, 0);
	
	TestHelpers::TestLinearStateMachine(this, NewBP, TotalStates);

	int32 ExpectedEntryValue = TotalStates;
	{
		int32 EntryHits = 0; int32 UpdateHits = 0; int32 EndHits = 0;
		USMInstance* TestedStateMachine = TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits, 0, false, false);

		TestFalse("State machine not in last state", TestedStateMachine->IsInEndState());

		TestNotEqual("State Machine generated value", EntryHits, ExpectedEntryValue);
		TestNotEqual("State Machine generated value", EndHits, ExpectedEntryValue);
	}

	// Set more this time and test fix using BP compile instead.
	
	// Set duplicate state nodes.
	{
		TArray<USMGraphNode_StateNodeBase*> StateNodes;
		FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphNode_StateNodeBase>(StateMachineGraph, StateNodes);
		FSMNode_Base* OriginalRuntimeNode = FSMBlueprintEditorUtils::GetRuntimeNodeFromGraph(StateNodes[1]->GetBoundGraph());

		for (int32 i = 2; i < TotalStates + 1 /* Entry state counts so add 1 */; ++i)
		{
			FSMNode_Base* DupeRuntimeNode = FSMBlueprintEditorUtils::GetRuntimeNodeFromGraph(StateNodes[i]->GetBoundGraph());
			DupeRuntimeNode->SetNodeGuid(OriginalRuntimeNode->GetNodeGuid());
			FSMBlueprintEditorUtils::UpdateRuntimeNodeForGraph(DupeRuntimeNode, StateNodes[i]->GetBoundGraph());
		}
	}

	// Set duplicate transition nodes.
	{
		TArray<USMGraphNode_TransitionEdge*> TransitionNodes;
		FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphNode_TransitionEdge>(StateMachineGraph, TransitionNodes);
		FSMNode_Base* OriginalRuntimeNode = FSMBlueprintEditorUtils::GetRuntimeNodeFromGraph(TransitionNodes[0]->GetBoundGraph());

		for (int32 i = 1; i < TotalStates - 1; ++i)
		{
			FSMNode_Base* DupeRuntimeNode = FSMBlueprintEditorUtils::GetRuntimeNodeFromGraph(TransitionNodes[i]->GetBoundGraph());
			DupeRuntimeNode->SetNodeGuid(OriginalRuntimeNode->GetNodeGuid());
			FSMBlueprintEditorUtils::UpdateRuntimeNodeForGraph(DupeRuntimeNode, TransitionNodes[i]->GetBoundGraph());
		}
	}

	// This will compile BP.
	AddExpectedError("has duplicate runtime GUID with", EAutomationExpectedErrorFlags::Contains, 7);
	TestHelpers::TestLinearStateMachine(this, NewBP, TotalStates);
	{
		int32 EntryHits = 0; int32 UpdateHits = 0; int32 EndHits = 0;
		USMInstance* TestedStateMachine = TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits, 0, false, false);

		TestFalse("State machine not in last state", TestedStateMachine->IsInEndState());

		TestNotEqual("State Machine generated value", EntryHits, ExpectedEntryValue);
		TestNotEqual("State Machine generated value", EndHits, ExpectedEntryValue);
	}
	
	TotalDuplicated = FSMBlueprintEditorUtils::FixUpDuplicateRuntimeGuids(NewBP);
	TestEqual("All duplicates fixed", TotalDuplicated, 0);

	return NewAsset.DeleteAsset(this);
}

/**
 * Validate node names that special characters won't cause a crash when copying unrelated object properties during a compile.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInvalidNodeNameTest, "LogicDriver.BugFixes.CheckInvalidNodeName", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FInvalidNodeNameTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(2)

	UEdGraphPin* LastStatePin = nullptr;

	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin);

	USMGraphNode_StateNode* StateNode = CastChecked<USMGraphNode_StateNode>(LastStatePin->GetOwningNode());
	StateNode->GetBoundGraph()->Rename(TEXT("Invalid name: ..."));

	FKismetEditorUtilities::CompileBlueprint(NewBP);

	StateNode->SetNodeClass(USMStateTestInstance::StaticClass());
	StateNode->InitTemplate();
	
	FKismetEditorUtilities::CompileBlueprint(NewBP);

	StateNode->SetNodeClass(USMStateTestInstance2::StaticClass());
	StateNode->InitTemplate();
	
	FKismetEditorUtilities::CompileBlueprint(NewBP);

	StateNode->SetNodeClass(USMStateTestInstance::StaticClass());
	StateNode->InitTemplate();
	
	FKismetEditorUtilities::CompileBlueprint(NewBP);

	// If there hasn't been a crash by this point we're good.
	
	return true;
}

#endif

#endif //WITH_DEV_AUTOMATION_TESTS