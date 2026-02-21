// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Blueprints/SMBlueprint.h"
#include "SMTestHelpers.h"
#include "Blueprints/SMBlueprintGeneratedClass.h"
#include "Blueprints/SMBlueprintFactory.h"
#include "Utilities/SMBlueprintEditorUtils.h"
#include "SMTestContext.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Graph/SMGraph.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_StateMachineSelectNode.h"
#include "Graph/Nodes/SMGraphK2Node_StateMachineNode.h"
#include "Graph/Nodes/SMGraphNode_StateNode.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/Nodes/SMGraphNode_StateMachineStateNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineParentNode.h"
#include "Graph/Nodes/SMGraphNode_ConduitNode.h"
#include "Graph/Nodes/FunctionNodes/SMGraphK2Node_StateReadNodes.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_TransitionEnteredNode.h"


#if WITH_DEV_AUTOMATION_TESTS

#if PLATFORM_DESKTOP

bool TestParentStateMachines(FAutomationTestBase* Test, int32 NumParentsCallsInChild = 1, int32 NumChildCallsInGrandChild = 1, int32 NumGrandChildCallsInReference = 1)
{
	FAssetHandler ParentAsset;
	if (!TestHelpers::TryCreateNewStateMachineAsset(Test, ParentAsset, false))
	{
		return false;
	}

	struct FDisableEditorGuidTest
	{
		FDisableEditorGuidTest()
		{
			USMStateTestInstance::bTestEditorGuids = false;
		}
		~FDisableEditorGuidTest()
		{
			USMStateTestInstance::bTestEditorGuids = true;
		}
	};

	// Parent state machines can't safely test editor guids.
	FDisableEditorGuidTest DisableEditorGuidTest;

	int32 TotalParentStates = 3;

	USMBlueprint* ParentBP = ParentAsset.GetObjectAs<USMBlueprint>();
	{
		// Find root state machine.
		USMGraphK2Node_StateMachineNode* ParentRootStateMachineNode = FSMBlueprintEditorUtils::GetRootStateMachineNode(ParentBP);

		// Find the state machine graph.
		USMGraph* ParentStateMachineGraph = ParentRootStateMachineNode->GetStateMachineGraph();

		UEdGraphPin* LastParentTopLevelStatePin = nullptr;

		// Build single state - state machine.
		TestHelpers::BuildLinearStateMachine(Test, ParentStateMachineGraph, TotalParentStates, &LastParentTopLevelStatePin);
		if (!ParentAsset.SaveAsset(Test))
		{
			return false;
		}
		USMGraphNode_TransitionEdge* TransitionEdge =
			CastChecked<USMGraphNode_TransitionEdge>(Cast<USMGraphNode_StateNode>(LastParentTopLevelStatePin->GetOwningNode())->GetInputPin()->LinkedTo[0]->GetOwningNode());

		// Event check so we can test if the parent machine was triggered.
		TestHelpers::AddEventWithLogic<USMGraphK2Node_TransitionEnteredNode>(Test, TransitionEdge,
			USMTestContext::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMTestContext, IncreaseTransitionTaken)));

		FKismetEditorUtilities::CompileBlueprint(ParentBP);
	}

	FAssetHandler ChildAsset;
	if (!TestHelpers::TryCreateNewStateMachineAsset(Test, ChildAsset, false))
	{
		return false;
	}
	int32 TotalChildStates = 1 + (NumParentsCallsInChild * 2);
	USMBlueprint* ChildBP = ChildAsset.GetObjectAs<USMBlueprint>();
	{
		ChildBP->ParentClass = ParentBP->GetGeneratedClass();

		// Find root state machine.
		USMGraphK2Node_StateMachineNode* RootStateMachineNode = FSMBlueprintEditorUtils::GetRootStateMachineNode(ChildBP);

		// Find the state machine graph.
		USMGraph* StateMachineGraph = RootStateMachineNode->GetStateMachineGraph();

		UEdGraphPin* LastTopLevelStatePin = nullptr;

		// Build single state - state machine.
		TestHelpers::BuildLinearStateMachine(Test, StateMachineGraph, 1, &LastTopLevelStatePin, USMStateTestInstance::StaticClass(), USMTransitionTestInstance::StaticClass());
		if (!ChildAsset.SaveAsset(Test))
		{
			return false;
		}

		for (int32 i = 0; i < NumParentsCallsInChild; ++i)
		{
			USMGraphNode_StateMachineParentNode* ParentNode = TestHelpers::CreateNewNode<USMGraphNode_StateMachineParentNode>(Test, StateMachineGraph, LastTopLevelStatePin);

			Test->TestNotNull("Parent Node created", ParentNode);
			Test->TestEqual("Correct parent class defaulted", Cast<USMBlueprintGeneratedClass>(ParentNode->ParentClass), ParentBP->GetGeneratedClass());
			ParentNode->GetNodeTemplateAs<USMStateMachineInstance>()->SetReuseCurrentState(true);
			
			TArray<USMBlueprintGeneratedClass*> ParentClasses;
			FSMBlueprintEditorUtils::TryGetParentClasses(ChildBP, ParentClasses);

			Test->TestTrue("Correct parent class found for child", ParentClasses.Num() == 1 && ParentClasses[0] == ParentBP->GetGeneratedClass());

			// Transition before parent node.
			USMGraphNode_TransitionEdge* TransitionToParent = CastChecked<USMGraphNode_TransitionEdge>(ParentNode->GetInputPin()->LinkedTo[0]->GetOwningNode());
			TestHelpers::AddTransitionResultLogic(Test, TransitionToParent);

			// Add one more state so we can wait for the parent to complete.
			LastTopLevelStatePin = ParentNode->GetOutputPin();

			TestHelpers::BuildLinearStateMachine(Test, StateMachineGraph, 1, &LastTopLevelStatePin);
			{
				// Signal the state after the first nested state machine to wait for its completion.
				USMGraphNode_TransitionEdge* TransitionFromParent = CastChecked<USMGraphNode_TransitionEdge>(ParentNode->GetOutputPin()->LinkedTo[0]->GetOwningNode());
				TestHelpers::OverrideTransitionResultLogic<USMGraphK2Node_StateMachineReadNode_InEndState>(Test, TransitionFromParent);
			}
		}

		int32 EntryHits = 0; int32 UpdateHits = 0; int32 EndHits = 0;
		USMInstance* TestedStateMachine = TestHelpers::RunStateMachineToCompletion(Test, ChildBP, EntryHits, UpdateHits, EndHits);

		int32 ExpectedHits = (TotalParentStates * NumParentsCallsInChild) + TotalChildStates - NumParentsCallsInChild; // No hit for parent node, but instead for each node in parent.

		Test->TestTrue("State machine in last state", TestedStateMachine->IsInEndState());

		Test->TestEqual("State Machine generated value", EntryHits, ExpectedHits);
		Test->TestEqual("State Machine generated value", EndHits, ExpectedHits);
		USMTestContext* Context = Cast<USMTestContext>(TestedStateMachine->GetContext());

		Test->TestEqual("State Machine parent state hit", Context->TestTransitionEntered.Count, NumParentsCallsInChild);
	}

	// Test empty parent
	{
		USMGraphK2Node_StateMachineNode* ParentRootStateMachineNode = FSMBlueprintEditorUtils::GetRootStateMachineNode(ParentBP);
		USMGraphK2Node_StateMachineSelectNode* CachedOutputNode = CastChecked<USMGraphK2Node_StateMachineSelectNode>(ParentRootStateMachineNode->GetOutputNode());
		ParentRootStateMachineNode->BreakAllNodeLinks();

		Test->AddExpectedError("is not connected to any state machine", EAutomationExpectedErrorFlags::Contains, 0);
		Test->AddExpectedError("has no root state machine graph in parent", EAutomationExpectedErrorFlags::Contains, 0); // Will be hit for every time compiled.
		FKismetEditorUtilities::CompileBlueprint(ParentBP);

		int32 ExpectedHits = TotalChildStates - NumParentsCallsInChild; // Empty parent node.
		int32 EntryHits = 0; int32 UpdateHits = 0; int32 EndHits = 0;

		USMInstance* TestedStateMachine = TestHelpers::RunStateMachineToCompletion(Test, ChildBP, EntryHits, UpdateHits, EndHits);
		
		Test->TestEqual("State Machine generated value", EntryHits, ExpectedHits);
		Test->TestEqual("State Machine generated value", EndHits, ExpectedHits);
		USMTestContext* Context = Cast<USMTestContext>(TestedStateMachine->GetContext());

		Test->TestEqual("State Machine parent state hit", Context->TestTransitionEntered.Count, 0);

		// Re-establish link.
		ParentRootStateMachineNode->GetSchema()->TryCreateConnection(ParentRootStateMachineNode->GetOutputPin(), CachedOutputNode->GetInputPin());
		FKismetEditorUtilities::CompileBlueprint(ParentBP);
		// Re-run original test.
		{
			TestedStateMachine = TestHelpers::RunStateMachineToCompletion(Test, ChildBP, EntryHits, UpdateHits, EndHits);

			ExpectedHits = (TotalParentStates * NumParentsCallsInChild) + TotalChildStates - NumParentsCallsInChild; // No hit for parent node, but instead for each node in parent.

			Test->TestTrue("State machine in last state", TestedStateMachine->IsInEndState());
			Test->TestEqual("State Machine generated value", EntryHits, ExpectedHits);
			Test->TestEqual("State Machine generated value", EndHits, ExpectedHits);
			Context = Cast<USMTestContext>(TestedStateMachine->GetContext());

			Test->TestEqual("State Machine parent state hit", Context->TestTransitionEntered.Count, NumParentsCallsInChild);
		}
	}

	// Create GrandChild
	FAssetHandler GrandChildAsset;
	if (!TestHelpers::TryCreateNewStateMachineAsset(Test, GrandChildAsset, false))
	{
		return false;
	}

	int32 TotalGrandChildStates = 1 + (NumChildCallsInGrandChild * 2);
	USMBlueprint* GrandChildBP = GrandChildAsset.GetObjectAs<USMBlueprint>();
	{
		GrandChildBP->ParentClass = ChildBP->GetGeneratedClass();

		// Find root state machine.
		USMGraphK2Node_StateMachineNode* RootStateMachineNode = FSMBlueprintEditorUtils::GetRootStateMachineNode(GrandChildBP);

		// Find the state machine graph.
		USMGraph* StateMachineGraph = RootStateMachineNode->GetStateMachineGraph();

		UEdGraphPin* LastTopLevelStatePin = nullptr;

		// Build single state - state machine.
		TestHelpers::BuildLinearStateMachine(Test, StateMachineGraph, 1, &LastTopLevelStatePin);
		if (!GrandChildAsset.SaveAsset(Test))
		{
			return false;
		}

		for (int32 i = 0; i < NumChildCallsInGrandChild; ++i)
		{
			USMGraphNode_StateMachineParentNode* ParentNode = TestHelpers::CreateNewNode<USMGraphNode_StateMachineParentNode>(Test, StateMachineGraph, LastTopLevelStatePin);
			ParentNode->GetNodeTemplateAs<USMStateMachineInstance>()->SetReuseCurrentState(true);
			
			Test->TestNotNull("Parent Node created", ParentNode);
			Test->TestEqual("Correct parent class defaulted", Cast<USMBlueprintGeneratedClass>(ParentNode->ParentClass), ChildBP->GetGeneratedClass());

			TArray<USMBlueprintGeneratedClass*> ParentClasses;
			FSMBlueprintEditorUtils::TryGetParentClasses(GrandChildBP, ParentClasses);

			Test->TestTrue("Correct parent class found for child", ParentClasses.Num() == 2 && ParentClasses.Contains(ChildBP->GetGeneratedClass()) && ParentClasses.Contains(ParentBP->GetGeneratedClass()));

			// Transition before parent node.
			USMGraphNode_TransitionEdge* TransitionToParent = CastChecked<USMGraphNode_TransitionEdge>(ParentNode->GetInputPin()->LinkedTo[0]->GetOwningNode());
			TestHelpers::AddTransitionResultLogic(Test, TransitionToParent);

			// Add one more state so we can wait for the parent to complete.
			LastTopLevelStatePin = ParentNode->GetOutputPin();

			TestHelpers::BuildLinearStateMachine(Test, StateMachineGraph, 1, &LastTopLevelStatePin, USMStateTestInstance::StaticClass(), USMTransitionTestInstance::StaticClass());
			{
				// Signal the state after the first nested state machine to wait for its completion.
				USMGraphNode_TransitionEdge* TransitionFromParent = CastChecked<USMGraphNode_TransitionEdge>(ParentNode->GetOutputPin()->LinkedTo[0]->GetOwningNode());
				TestHelpers::OverrideTransitionResultLogic<USMGraphK2Node_StateMachineReadNode_InEndState>(Test, TransitionFromParent);
			}
		}

		int32 EntryHits = 0; int32 UpdateHits = 0; int32 EndHits = 0;
		USMInstance* TestedStateMachine = TestHelpers::RunStateMachineToCompletion(Test, GrandChildBP, EntryHits, UpdateHits, EndHits);

		int32 ExpectedHits = ((TotalParentStates * NumParentsCallsInChild) + (TotalChildStates - NumParentsCallsInChild)) * NumChildCallsInGrandChild + TotalGrandChildStates - NumChildCallsInGrandChild; // No hit for either parent nodes, but instead for each node in parent.

		Test->TestTrue("State machine in last state", TestedStateMachine->IsInEndState());

		Test->TestEqual("State Machine generated value", EntryHits, ExpectedHits);
		Test->TestEqual("State Machine generated value", EndHits, ExpectedHits);
		USMTestContext* Context = Cast<USMTestContext>(TestedStateMachine->GetContext());

		Test->TestEqual("State Machine parent state hit", Context->TestTransitionEntered.Count, NumParentsCallsInChild * NumChildCallsInGrandChild); // From grand parent.

		// Test maintaining node guids that were generated from being duplicates.
		{
			TestedStateMachine = TestHelpers::CreateNewStateMachineInstanceFromBP(Test, GrandChildBP, Context);
			TMap<FGuid, FSMNode_Base*> OldNodeMap = TestedStateMachine->GetNodeMap();

			// Recompile which recalculate NodeGuids on duped nodes.
			FKismetEditorUtilities::CompileBlueprint(GrandChildBP);
			Context = NewObject<USMTestContext>();
			TestedStateMachine = TestHelpers::CreateNewStateMachineInstanceFromBP(Test, GrandChildBP, Context);

			const TMap<FGuid, FSMNode_Base*>& NewNodeMap = TestedStateMachine->GetNodeMap();
			for (auto& KeyVal : NewNodeMap)
			{
				Test->TestTrue("Dupe node guids haven't changed", OldNodeMap.Contains(KeyVal.Key));
			}
		}

		// Test saving / restoring
		{
			for (int32 i = 0; i < ExpectedHits; ++i)
			{
				TestedStateMachine = TestHelpers::RunStateMachineToCompletion(Test, GrandChildBP, EntryHits, UpdateHits, EndHits, i, true, false, false);
				TArray<FGuid> CurrentGuids;
				TestedStateMachine->GetAllActiveStateGuids(CurrentGuids);

				TestedStateMachine = TestHelpers::CreateNewStateMachineInstanceFromBP(Test, GrandChildBP, Context);
				TestedStateMachine->LoadFromMultipleStates(CurrentGuids);

				TArray<FGuid> ReloadedGuids;
				TestedStateMachine->GetAllActiveStateGuids(ReloadedGuids);

				int32 Match = TestHelpers::ArrayContentsInArray(ReloadedGuids, CurrentGuids);
				Test->TestEqual("State machine states reloaded", Match, CurrentGuids.Num());
			}
		}
	}

	// Test creating reference to the grand child
	FAssetHandler ReferenceAsset;
	if (!TestHelpers::TryCreateNewStateMachineAsset(Test, ReferenceAsset, false))
	{
		return false;
	}
	int32 TotalRefStates = 1 + NumGrandChildCallsInReference * 2;
	USMBlueprint* ReferenceBP = ReferenceAsset.GetObjectAs<USMBlueprint>();
	{
		// Find root state machine.
		USMGraphK2Node_StateMachineNode* RootStateMachineNode = FSMBlueprintEditorUtils::GetRootStateMachineNode(ReferenceBP);

		// Find the state machine graph.
		USMGraph* StateMachineGraph = RootStateMachineNode->GetStateMachineGraph();

		UEdGraphPin* LastTopLevelStatePin = nullptr;

		// Build single state - state machine.
		TestHelpers::BuildLinearStateMachine(Test, StateMachineGraph, 1, &LastTopLevelStatePin);
		if (!ChildAsset.SaveAsset(Test))
		{
			return false;
		}

		for (int32 i = 0; i < NumGrandChildCallsInReference; ++i)
		{
			USMGraphNode_StateMachineStateNode* ReferenceNode = TestHelpers::CreateNewNode<USMGraphNode_StateMachineStateNode>(Test, StateMachineGraph, LastTopLevelStatePin);
			ReferenceNode->ReferenceStateMachine(GrandChildBP);
			ReferenceNode->GetNodeTemplateAs<USMStateMachineInstance>()->SetReuseCurrentState(true);

			// Transition before reference node.
			{
				USMGraphNode_TransitionEdge* TransitionToReference = CastChecked<USMGraphNode_TransitionEdge>(ReferenceNode->GetInputPin()->LinkedTo[0]->GetOwningNode());
				TestHelpers::AddTransitionResultLogic(Test, TransitionToReference);
			}
			
			// Add one more state so we can wait for the reference to complete.
			LastTopLevelStatePin = ReferenceNode->GetOutputPin();

			TestHelpers::BuildLinearStateMachine(Test, StateMachineGraph, 1, &LastTopLevelStatePin);
			{
				// Signal the state after the first nested state machine to wait for its completion.
				USMGraphNode_TransitionEdge* TransitionFromReference = CastChecked<USMGraphNode_TransitionEdge>(ReferenceNode->GetOutputPin()->LinkedTo[0]->GetOwningNode());
				TestHelpers::OverrideTransitionResultLogic<USMGraphK2Node_StateMachineReadNode_InEndState>(Test, TransitionFromReference);
			}
		}

		int32 EntryHits = 0; int32 UpdateHits = 0; int32 EndHits = 0;
		USMInstance* TestedStateMachine = TestHelpers::RunStateMachineToCompletion(Test, ReferenceBP, EntryHits, UpdateHits, EndHits);

		int32 ExpectedHits = (((TotalParentStates * NumParentsCallsInChild) + (TotalChildStates - NumParentsCallsInChild)) * NumChildCallsInGrandChild + TotalGrandChildStates - NumChildCallsInGrandChild) *
			NumGrandChildCallsInReference + TotalRefStates - NumGrandChildCallsInReference;

		Test->TestTrue("State machine in last state", TestedStateMachine->IsInEndState());

		Test->TestEqual("State Machine generated value", EntryHits, ExpectedHits);
		Test->TestEqual("State Machine generated value", EndHits, ExpectedHits);
		USMTestContext* Context = Cast<USMTestContext>(TestedStateMachine->GetContext());

		Test->TestEqual("State Machine parent state hit", Context->TestTransitionEntered.Count, NumParentsCallsInChild * NumChildCallsInGrandChild * NumGrandChildCallsInReference);


		// Test saving / restoring
		{
			for (int32 i = 0; i < ExpectedHits; ++i)
			{
				TestedStateMachine = TestHelpers::RunStateMachineToCompletion(Test, ReferenceBP, EntryHits, UpdateHits, EndHits, i, true, false, false);
				TArray<FGuid> CurrentGuids;
				TestedStateMachine->GetAllActiveStateGuids(CurrentGuids);

				TestedStateMachine = TestHelpers::CreateNewStateMachineInstanceFromBP(Test, ReferenceBP, Context);
				TestedStateMachine->LoadFromMultipleStates(CurrentGuids);

				TArray<FGuid> ReloadedGuids;
				TestedStateMachine->GetAllActiveStateGuids(ReloadedGuids);

				int32 Match = TestHelpers::ArrayContentsInArray(ReloadedGuids, CurrentGuids);
				Test->TestEqual("State machine states reloaded", Match, CurrentGuids.Num());
			}
		}
	}

	ReferenceAsset.DeleteAsset(Test);
	GrandChildAsset.DeleteAsset(Test);
	ChildAsset.DeleteAsset(Test);
	return ParentAsset.DeleteAsset(Test);
}

/**
 * Test multiple parent state machine calls.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStateMachineParentTest, "LogicDriver.Parents.StateMachineParent", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

	bool FStateMachineParentTest::RunTest(const FString& Parameters)
{
	for (int32 c = 1; c < 4; ++c)
	{
		for (int32 gc = 1; gc < 3; ++gc)
		{
			for (int32 ref = 1; ref < 3; ++ref)
			{
				if (!TestParentStateMachines(this, c, gc, ref))
				{
					return false;
				}
			}
		}
	}

	return true;
}

#endif

#endif //WITH_DEV_AUTOMATION_TESTS