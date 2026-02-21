// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Helpers/SMTestBoilerplate.h"

#include "SMTestContext.h"
#include "SMTestHelpers.h"

#include "Blueprints/SMBlueprint.h"

#include "Configuration/SMProjectEditorSettings.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_ConduitResultNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_TransitionEnteredNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_TransitionInitializedNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_TransitionShutdownNode.h"
#include "Graph/Nodes/SMGraphK2Node_StateMachineNode.h"
#include "Graph/Nodes/SMGraphNode_ConduitNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineEntryNode.h"
#include "Graph/Nodes/SMGraphNode_StateNodeBase.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/SMConduitGraph.h"
#include "Graph/SMGraph.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "Kismet2/KismetEditorUtilities.h"

#if WITH_DEV_AUTOMATION_TESTS

#if PLATFORM_DESKTOP

/**
 * Test conduit functionality.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConduitTest, "LogicDriver.Conduits.Comprehensive", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FConduitTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(5)

	UEdGraphPin* LastStatePin = nullptr;

	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin);

	USMGraphNode_StateNodeBase* FirstNode = CastChecked<USMGraphNode_StateNodeBase>(StateMachineGraph->GetEntryNode()->GetOutputNode());

	// This will become a conduit.
	USMGraphNode_StateNodeBase* SecondNode = CastChecked<USMGraphNode_StateNodeBase>(FirstNode->GetNextState());
	USMGraphNode_ConduitNode* ConduitNode = FSMBlueprintEditorUtils::ConvertNodeTo<USMGraphNode_ConduitNode>(SecondNode);
	ConduitNode->GetNodeTemplateAs<USMConduitInstance>()->SetEvalWithTransitions(false); // Settings make this true by default.
	
	// Eval with the conduit being considered a state. It will end with the active state becoming stuck on a conduit.
	int32 EntryHits = 0; int32 UpdateHits = 0; int32 EndHits = 0;
	int32 MaxIterations = TotalStates * 2;
	USMInstance* Instance = TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits, MaxIterations, false, false);

	TestTrue("State machine still active", Instance->IsActive());
	TestTrue("State machine shouldn't have been able to switch states.", !Instance->IsInEndState());

	TestTrue("Active state is conduit", Instance->GetSingleActiveState()->IsConduit());
	TestEqual("State Machine generated value", EntryHits, 1);
	TestEqual("State Machine generated value", UpdateHits, 0);
	TestEqual("State Machine generated value", EndHits, 1); // Ended state and switched to conduit.
	
	// Set conduit to true and try again.
	USMConduitGraph* Graph = CastChecked<USMConduitGraph>(ConduitNode->GetBoundGraph());
	UEdGraphPin* CanEvalPin = Graph->ResultNode->GetInputPin();
	CanEvalPin->DefaultValue = "True";

	// Eval normally.
	TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits, MaxIterations, true, true);

	// Configure conduit as transition and set to false.
	ConduitNode->GetNodeTemplateAs<USMConduitInstance>()->SetEvalWithTransitions(true);
	CanEvalPin->DefaultValue = "False";
	Instance = TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits, MaxIterations, false, false);

	TestTrue("State machine still active", Instance->IsActive());
	TestTrue("State machine shouldn't have been able to switch states.", !Instance->IsInEndState());

	TestFalse("Active state is not conduit", Instance->GetSingleActiveState()->IsConduit());
	TestEqual("State Machine generated value", EntryHits, 1);
	TestEqual("State Machine generated value", UpdateHits, MaxIterations); // Updates because state not transitioning out.
	TestEqual("State Machine generated value", EndHits, 0); // State should never have ended.

	// Set conduit to true but set the next transition to false. Should have same result as when the conduit was false.
	CanEvalPin->DefaultValue = "True";
	USMGraphNode_TransitionEdge* Transition = CastChecked<USMGraphNode_TransitionEdge>(ConduitNode->GetOutputNode());
	USMTransitionGraph* TransitionGraph = CastChecked<USMTransitionGraph>(Transition->GetBoundGraph());
	UEdGraphPin* TransitionPin = TransitionGraph->ResultNode->GetInputPin();
	TransitionPin->BreakAllPinLinks(true);
	TransitionPin->DefaultValue = "False";
	Instance = TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits, MaxIterations, false, false);

	TestTrue("State machine still active", Instance->IsActive());
	TestTrue("State machine shouldn't have been able to switch states.", !Instance->IsInEndState());

	TestFalse("Active state is not conduit", Instance->GetSingleActiveState()->IsConduit());
	TestEqual("State Machine generated value", EntryHits, 1);
	TestEqual("State Machine generated value", UpdateHits, MaxIterations); // Updates because state not transitioning out.
	TestEqual("State Machine generated value", EndHits, 0); // State should never have ended.

	// Set transition to true and should eval normally.
	TransitionPin->DefaultValue = "True";
	TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits, MaxIterations, true, true);

	// Add another conduit node (false) after the last one configured to run as a transition. Result should be the same as the last failure.
	USMGraphNode_StateNodeBase* ThirdNode = CastChecked<USMGraphNode_StateNodeBase>(ConduitNode->GetNextState());
	USMGraphNode_ConduitNode* NextConduitNode = FSMBlueprintEditorUtils::ConvertNodeTo<USMGraphNode_ConduitNode>(ThirdNode);
	NextConduitNode->GetNodeTemplateAs<USMConduitInstance>()->SetEvalWithTransitions(true);
	Instance = TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits, MaxIterations, false, false);

	TestTrue("State machine still active", Instance->IsActive());
	TestTrue("State machine shouldn't have been able to switch states.", !Instance->IsInEndState());

	TestFalse("Active state is not conduit", Instance->GetSingleActiveState()->IsConduit());
	TestEqual("State Machine generated value", EntryHits, 1);
	TestEqual("State Machine generated value", UpdateHits, MaxIterations); // Updates because state not transitioning out.
	TestEqual("State Machine generated value", EndHits, 0); // State should never have ended.

	// Set new conduit to true and eval again.
	Graph = CastChecked<USMConduitGraph>(NextConduitNode->GetBoundGraph());
	CanEvalPin = Graph->ResultNode->GetInputPin();
	CanEvalPin->DefaultValue = "True";
	TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits, MaxIterations, true, true);

	// Test with evaluation disabled.
	NextConduitNode->GetNodeTemplateAs<USMConduitInstance>()->SetCanEvaluate(false);
	int32 IterationsRan;
	Instance = TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits, MaxIterations, false, false, true, &IterationsRan);
	TestEqual("Iteration max count ran", IterationsRan, MaxIterations);
	TestFalse("State machine did not complete", Instance->IsInEndState());
	FSMConduit* SecondConduit = (FSMConduit*)Instance->GetSingleActiveState()->GetOutgoingTransitions()[0]->GetToState()->GetOutgoingTransitions()[0]->GetToState();
	TestTrue("Conduit found", SecondConduit->IsConduit());
	TestFalse("Second state is conduit that doesn't evaluate which prevented first conduit from passing.", SecondConduit->bCanEvaluate);

	// Restore evaluation.
	NextConduitNode->GetNodeTemplateAs<USMConduitInstance>()->SetCanEvaluate(true);
	FKismetEditorUtilities::CompileBlueprint(NewBP);
	
	// Test correct transition order.
	USMTestContext* Context = NewObject<USMTestContext>();
	Instance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);
	Instance->Start();

	FSMState_Base* CurrentState = Instance->GetRootStateMachine().GetSingleActiveState();
	TArray<TArray<FSMTransition*>> TransitionChain;
	TestTrue("Valid transition found", CurrentState->GetValidTransition(TransitionChain));

	FSMState_Base* SecondState = CurrentState->GetOutgoingTransitions()[0]->GetToState();
	FSMState_Base* ThirdState = SecondState->GetOutgoingTransitions()[0]->GetToState();
	
	TestEqual("Transition to and after conduit found", TransitionChain[0].Num(), 3);
	TestEqual("Correct transition order", TransitionChain[0][0], CurrentState->GetOutgoingTransitions()[0]);
	TestEqual("Correct transition order", TransitionChain[0][1], SecondState->GetOutgoingTransitions()[0]);
	TestEqual("Correct transition order", TransitionChain[0][2], ThirdState->GetOutgoingTransitions()[0]);

	// Test conduit initialize & shutdown
	TestHelpers::AddEventWithLogic<USMGraphK2Node_TransitionInitializedNode>(this, ConduitNode,
		USMTestContext::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMTestContext, IncreaseTransitionInit)));

	TestHelpers::AddEventWithLogic<USMGraphK2Node_TransitionInitializedNode>(this, NextConduitNode,
		USMTestContext::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMTestContext, IncreaseTransitionInit)));

	TestHelpers::AddEventWithLogic<USMGraphK2Node_TransitionShutdownNode>(this, ConduitNode,
		USMTestContext::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMTestContext, IncreaseTransitionShutdown)));
	
	TestHelpers::AddEventWithLogic<USMGraphK2Node_TransitionShutdownNode>(this, NextConduitNode,
		USMTestContext::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMTestContext, IncreaseTransitionShutdown)));

	TestHelpers::AddEventWithLogic<USMGraphK2Node_TransitionEnteredNode>(this, ConduitNode,
		USMTestContext::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMTestContext, IncreaseTransitionTaken)));
	
	TestHelpers::AddEventWithLogic<USMGraphK2Node_TransitionEnteredNode>(this, NextConduitNode,
		USMTestContext::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMTestContext, IncreaseTransitionTaken)));
	
	FKismetEditorUtilities::CompileBlueprint(NewBP);
	
	Context = NewObject<USMTestContext>();
	Instance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);
	Instance->Start();

	// All transition inits should be fired at once.
	TestEqual("InitValue", Context->TestTransitionInit.Count, 2);
	TestEqual("ShutdownValue", Context->TestTransitionShutdown.Count, 0);
	TestEqual("EnteredValue", Context->TestTransitionEntered.Count, 0);
	
	Instance = TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits, MaxIterations, true, false);
	Context = CastChecked<USMTestContext>(Instance->GetContext());
	
	TestEqual("InitValue", Context->TestTransitionInit.Count, 2);
	TestEqual("ShutdownValue", Context->TestTransitionShutdown.Count, 2);
	TestEqual("EnteredValue", Context->TestTransitionEntered.Count, 2);

	// Test having the second conduit go back to the first conduit. When both are set as transitions this caused a stack overflow. Check it's fixed.
	NextConduitNode->GetOutputPin()->BreakAllPinLinks(true);
	TestTrue("Next conduit wired to previous conduit", NextConduitNode->GetSchema()->TryCreateConnection(NextConduitNode->GetOutputPin(), ConduitNode->GetInputPin()));
	USMGraphNode_TransitionEdge* TransitionEdge = CastChecked<USMGraphNode_TransitionEdge>(NextConduitNode->GetOutputNode());
	TestHelpers::AddTransitionResultLogic(this, TransitionEdge);
	TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits, MaxIterations, true, false);

	// Test initial conduit node entry states.
	{
		USMGraphNode_ConduitNode* FirstConduitNode = FSMBlueprintEditorUtils::ConvertNodeTo<USMGraphNode_ConduitNode>(FirstNode);
		FirstConduitNode->GetOutputPin()->BreakAllPinLinks(true);

		TestHelpers::AddEventWithLogic<USMGraphK2Node_TransitionInitializedNode>(this, FirstConduitNode,
			USMTestContext::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMTestContext, IncreaseTransitionInit)));

		TestHelpers::AddEventWithLogic<USMGraphK2Node_TransitionShutdownNode>(this, FirstConduitNode,
			USMTestContext::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMTestContext, IncreaseTransitionShutdown)));

		Instance = TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits, MaxIterations, true, false);
		Context = CastChecked<USMTestContext>(Instance->GetContext());
		
		TestEqual("InitValue", Context->TestTransitionInit.Count, 1);
		TestEqual("ShutdownValue", Context->TestTransitionShutdown.Count, 1);
	}
	
	return true;
}

/**
 * Check conduit optimization type is correct.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConduitOptimizationTest, "LogicDriver.Conduits.Optimization", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FConduitOptimizationTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(3)

	UEdGraphPin* LastStatePin = nullptr;

	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin);

	USMGraphNode_StateNodeBase* FirstNode = CastChecked<USMGraphNode_StateNodeBase>(StateMachineGraph->GetEntryNode()->GetOutputNode());

	// This will become a conduit.
	USMGraphNode_StateNodeBase* SecondNode = CastChecked<USMGraphNode_StateNodeBase>(FirstNode->GetNextState());
	USMGraphNode_ConduitNode* ConduitNode = FSMBlueprintEditorUtils::ConvertNodeTo<USMGraphNode_ConduitNode>(SecondNode);

	USMConduitGraph* ConduitGraph = CastChecked<USMConduitGraph>(ConduitNode->GetBoundGraph());
	
	const int32 MaxIterations = TotalStates;

	ESMConditionalEvaluationType EvaluationType;

	int32 A, B, C;
	int32 IterationsRan;
	// Always false
	{
		// Initial value (should be false)
		EvaluationType = ConduitGraph->GetConditionalEvaluationType();
		TestEqual("Evaluation type is always false", EvaluationType, ESMConditionalEvaluationType::SM_AlwaysFalse);

		// Manually set false.
		ConduitGraph->GetSchema()->TrySetDefaultValue(*ConduitGraph->ResultNode->GetTransitionEvaluationPin(), "False");
		EvaluationType = ConduitGraph->GetConditionalEvaluationType();
		
		USMInstance* Instance = TestHelpers::RunStateMachineToCompletion(this, NewBP, A, B, C, MaxIterations, false, false, true, &IterationsRan);
		TestEqual("Doesn't end because conduit is false.", IterationsRan, MaxIterations);
		TestFalse("State machine never reached end state.", Instance->IsInEndState());
	}

	// Always true
	{
		ConduitGraph->GetSchema()->TrySetDefaultValue(*ConduitGraph->ResultNode->GetTransitionEvaluationPin(), "True");
		EvaluationType = ConduitGraph->GetConditionalEvaluationType();
		TestEqual("Evaluation type is always true", EvaluationType, ESMConditionalEvaluationType::SM_AlwaysTrue);
		TestHelpers::RunStateMachineToCompletion(this, NewBP, A, B, C, MaxIterations, true, true, true, &IterationsRan);
		TestEqual("Expected iterations ran", IterationsRan, 1);
	}
	
	// Node instance evaluation.
	{
		TestHelpers::SetNodeClass(this, ConduitNode, USMConduitTestInstance::StaticClass());
		EvaluationType = ConduitGraph->GetConditionalEvaluationType();
		TestEqual("Evaluation type is for the node instance", EvaluationType, ESMConditionalEvaluationType::SM_NodeInstance);
		USMInstance* Instance = TestHelpers::RunStateMachineToCompletion(this, NewBP, A, B, C, MaxIterations, false, false, true, &IterationsRan);
		TestEqual("Max iterations ran", IterationsRan, MaxIterations);
		TestFalse("State machine didn't finish", Instance->IsInEndState());
		
		USMConduitTestInstance* ConduitInstance = CastChecked<USMConduitTestInstance>(CastChecked<USMStateInstance_Base>(Instance->GetSingleActiveState()->GetNodeInstance())->GetTransitionByIndex(0)->GetNextStateInstance());
		ConduitInstance->bCanTransition = true;

		Instance->Update();
		TestTrue("State machine finished", Instance->IsInEndState());
		
		TestHelpers::SetNodeClass(this, ConduitNode, nullptr);
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
		FSMBlueprintEditorUtils::PlacePropertyOnGraph(ConduitGraph, NewProperty, ConduitGraph->ResultNode->GetTransitionEvaluationPin(), nullptr);
		
		EvaluationType = ConduitGraph->GetConditionalEvaluationType();
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
		FSMBlueprintEditorUtils::PlacePropertyOnGraph(ConduitGraph, NewProperty, ConduitGraph->ResultNode->GetTransitionEvaluationPin(), nullptr);

		EvaluationType = ConduitGraph->GetConditionalEvaluationType();
		TestEqual("Evaluation type is accurate", EvaluationType, ESMConditionalEvaluationType::SM_Graph);

		TestHelpers::RunStateMachineToCompletion(this, NewBP, A, B, C, MaxIterations);
	}
	
	return true;
}

/**
 * Test conduits defaulted to true.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConduitSettingsTest, "LogicDriver.Conduits.DefaultToTrue", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FConduitSettingsTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(2)

	UEdGraphPin* LastStatePin = nullptr;

	const bool bForceConduitsToTrue = false;

	const bool bUserNewConduitValue = FSMBlueprintEditorUtils::GetProjectEditorSettings()->bDefaultNewConduitsToTrue;
	
	// Test default value as false
	{
		FSMBlueprintEditorUtils::GetMutableProjectEditorSettings()->bDefaultNewConduitsToTrue = false;
		
		TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, nullptr, nullptr, bForceConduitsToTrue);

		USMGraphNode_StateNodeBase* FirstNode = CastChecked<USMGraphNode_StateNodeBase>(StateMachineGraph->GetEntryNode()->GetOutputNode());

		// This will become a conduit.
		USMGraphNode_StateNodeBase* SecondNode = CastChecked<USMGraphNode_StateNodeBase>(FirstNode->GetNextState());
		USMGraphNode_ConduitNode* ConduitNode = FSMBlueprintEditorUtils::ConvertNodeTo<USMGraphNode_ConduitNode>(SecondNode);

		USMConduitGraph* ConduitGraph = CastChecked<USMConduitGraph>(ConduitNode->GetBoundGraph());
		
		const FString DefaultValue = ConduitGraph->ResultNode->GetTransitionEvaluationPin()->GetDefaultAsString();
		TestEqual("Default is false", DefaultValue, "");
	}

	// Test default value as true
	{
		FSMBlueprintEditorUtils::GetMutableProjectEditorSettings()->bDefaultNewConduitsToTrue = true;

		LastStatePin = nullptr;
		FSMBlueprintEditorUtils::RemoveAllNodesFromGraph(StateMachineGraph);

		TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, nullptr, nullptr, bForceConduitsToTrue);
		USMGraphNode_StateNodeBase* FirstNode = CastChecked<USMGraphNode_StateNodeBase>(StateMachineGraph->GetEntryNode()->GetOutputNode());

		// This will become a conduit.
		USMGraphNode_StateNodeBase* SecondNode = CastChecked<USMGraphNode_StateNodeBase>(FirstNode->GetNextState());
		USMGraphNode_ConduitNode* ConduitNode = FSMBlueprintEditorUtils::ConvertNodeTo<USMGraphNode_ConduitNode>(SecondNode);

		USMConduitGraph* ConduitGraph = CastChecked<USMConduitGraph>(ConduitNode->GetBoundGraph());
		const FString DefaultValue = ConduitGraph->ResultNode->GetTransitionEvaluationPin()->GetDefaultAsString();
		TestEqual("Default is false", DefaultValue, "True");
	}

	FSMBlueprintEditorUtils::GetMutableProjectEditorSettings()->bDefaultNewConduitsToTrue = bUserNewConduitValue;
	
	return NewAsset.DeleteAsset(this);
}

#endif

#endif //WITH_DEV_AUTOMATION_TESTS