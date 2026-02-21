// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Helpers/SMTestBoilerplate.h"
#include "SMTestHelpers.h"

#include "Blueprints/SMBlueprintFactory.h"
#include "Graph/Nodes/FunctionNodes/SMGraphK2Node_StateReadNodes.h"
#include "Graph/Nodes/PropertyNodes/SMGraphK2Node_TextPropertyNode.h"
#include "Graph/Nodes/SMGraphK2Node_StateMachineNode.h"
#include "Graph/Nodes/SMGraphNode_ConduitNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineEntryNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineStateNode.h"
#include "Graph/Nodes/SMGraphNode_StateNode.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/SMGraph.h"
#include "Graph/SMTextPropertyGraph.h"
#include "SMTestContext.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "Blueprints/SMBlueprint.h"

#include "EdGraph/EdGraph.h"
#include "Kismet2/KismetEditorUtilities.h"

#if WITH_DEV_AUTOMATION_TESTS

#if PLATFORM_DESKTOP

/**
 * Collapse states down to a nested state machine.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCollapseStateMachineTest, "LogicDriver.Commands.CollapseStateMachine", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCollapseStateMachineTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(5)

	UEdGraphPin* LastStatePin = nullptr;

	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin);
	if (!NewAsset.SaveAsset(this))
	{
		return false;
	}
	TestHelpers::TestLinearStateMachine(this, NewBP, TotalStates);

	// Let the last node on the graph be the node after the new state machine.
	USMGraphNode_StateNodeBase* AfterNode = CastChecked<USMGraphNode_StateNodeBase>(LastStatePin->GetOwningNode());

	// Let the second node from the beginning be the node leading to the new state machine.
	USMGraphNode_StateNodeBase* BeforeNode = AfterNode->GetPreviousState()->GetPreviousState()->GetPreviousState();
	check(BeforeNode);

	// The two states in between will become a state machine.
	TSet<UObject*> SelectedNodes;
	USMGraphNode_StateNodeBase* SMStartNode = BeforeNode->GetNextState();
	USMGraphNode_StateNodeBase* SMEndNode = SMStartNode->GetNextState();
	SelectedNodes.Add(SMStartNode);
	SelectedNodes.Add(SMEndNode);

	TestEqual("Start SM Node connects from before node", BeforeNode, SMStartNode->GetPreviousState());
	TestEqual("Before Node connects to start SM node", SMStartNode, BeforeNode->GetNextState());

	TestEqual("End SM Node connects from after node", AfterNode, SMEndNode->GetNextState());
	TestEqual("After Node connects to end SM node", SMEndNode, AfterNode->GetPreviousState());

	FSMBlueprintEditorUtils::CollapseNodesAndCreateStateMachine(SelectedNodes);

	TotalStates -= 1;

	TestNotEqual("Start SM Node no longer connects to before node", BeforeNode, SMStartNode->GetPreviousState());
	TestNotEqual("Before Node no longer connects to start SM node", SMStartNode, BeforeNode->GetNextState());

	TestNotEqual("End SM Node no longer connects from after node", AfterNode, SMEndNode->GetNextState());
	TestNotEqual("After Node no longer connects to end SM node", SMEndNode, AfterNode->GetPreviousState());

	USMGraphNode_StateMachineStateNode* NewSMNode = Cast<USMGraphNode_StateMachineStateNode>(BeforeNode->GetNextState());
	TestNotNull("State Machine node created in proper location", NewSMNode);

	if (NewSMNode == nullptr)
	{
		return false;
	}

	TestEqual("New SM Node connects to correct node", NewSMNode->GetNextState(), AfterNode);

	TestHelpers::TestLinearStateMachine(this, NewBP, TotalStates);

	return NewAsset.DeleteAsset(this);
}

/**
 * Replace a node in the state machine.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReplaceNodesTest, "LogicDriver.Commands.ReplaceNodes", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FReplaceNodesTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(5)

	UEdGraphPin* LastStatePin = nullptr;

	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin);
	if (!NewAsset.SaveAsset(this))
	{
		return false;
	}
	TestHelpers::TestLinearStateMachine(this, NewBP, TotalStates);

	// Let the last node on the graph be the node after the new node.
	USMGraphNode_StateNodeBase* AfterNode = CastChecked<USMGraphNode_StateNodeBase>(LastStatePin->GetOwningNode());

	// Let node prior to the one we are replacing.
	USMGraphNode_StateNodeBase* BeforeNode = AfterNode->GetPreviousState()->GetPreviousState();
	check(BeforeNode);

	// The node we are replacing is the second to last node.
	USMGraphNode_StateNodeBase* NodeToReplace = AfterNode->GetPreviousState();
	TestTrue("Node is state", NodeToReplace->IsA<USMGraphNode_StateNode>());

	// State machine -- can't easily test converting to reference but that is just setting a null reference.
	USMGraphNode_StateMachineStateNode* StateMachineNode = FSMBlueprintEditorUtils::ConvertNodeTo<USMGraphNode_StateMachineStateNode>(NodeToReplace);
	TestTrue("Node removed", NodeToReplace->GetNextState() == nullptr && NodeToReplace->GetPreviousState() == nullptr && NodeToReplace->GetBoundGraph() == nullptr);
	TestTrue("Node is state machine", StateMachineNode->IsA<USMGraphNode_StateMachineStateNode>());
	TestFalse("Node is not reference", StateMachineNode->IsStateMachineReference());
	TestEqual("Connected to original next node", StateMachineNode->GetNextState(), AfterNode);
	TestEqual("Connected to original previous node", StateMachineNode->GetPreviousState(), BeforeNode);

	int32 A, B;
	TestHelpers::RunStateMachineToCompletion(this, NewBP, TotalStates, A, B);
	
	// Conduit
	NodeToReplace = StateMachineNode;
	USMGraphNode_ConduitNode* ConduitNode = FSMBlueprintEditorUtils::ConvertNodeTo<USMGraphNode_ConduitNode>(NodeToReplace);
	TestTrue("Node removed", NodeToReplace->GetNextState() == nullptr && NodeToReplace->GetPreviousState() == nullptr && NodeToReplace->GetBoundGraph() == nullptr);
	TestTrue("Node is conduit", ConduitNode->IsA<USMGraphNode_ConduitNode>());
	TestEqual("Connected to original next node", ConduitNode->GetNextState(), AfterNode);
	TestEqual("Connected to original previous node", ConduitNode->GetPreviousState(), BeforeNode);

	// Back to state
	NodeToReplace = ConduitNode;
	USMGraphNode_StateNode* StateNode = FSMBlueprintEditorUtils::ConvertNodeTo<USMGraphNode_StateNode>(NodeToReplace);
	TestTrue("Node removed", NodeToReplace->GetNextState() == nullptr && NodeToReplace->GetPreviousState() == nullptr && NodeToReplace->GetBoundGraph() == nullptr);
	TestTrue("Node is state", StateNode->IsA<USMGraphNode_StateNode>());
	TestEqual("Connected to original next node", StateNode->GetNextState(), AfterNode);
	TestEqual("Connected to original previous node", StateNode->GetPreviousState(), BeforeNode);
	TestHelpers::RunStateMachineToCompletion(this, NewBP, TotalStates, A, B);

	
	return NewAsset.DeleteAsset(this);
}

/**
 * Test combining multiple states and variables into one state stack.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStateStackMergeTest, "LogicDriver.Commands.StateStackMerge", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FStateStackMergeTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(3)

	UEdGraphPin* LastStatePin = nullptr;

	// Build single state - state machine.
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMStateTestInstance::StaticClass(), USMTransitionTestInstance::StaticClass());

	USMGraphNode_StateNode* FirstStateNode = CastChecked<USMGraphNode_StateNode>(StateMachineGraph->GetEntryNode()->GetOutputNode());
	USMGraphNode_StateNode* SecondStateNode = CastChecked<USMGraphNode_StateNode>(FirstStateNode->GetNextState());
	USMGraphNode_StateNode* ThirdStateDestNode = CastChecked<USMGraphNode_StateNode>(SecondStateNode->GetNextState());

	/* First state
	 * Node class: USMStateTestInstance
	 * Stack class: USMTextGraphStateExtra */
	FStateStackContainer NewStateStackText(USMTextGraphStateExtra::StaticClass());
	FirstStateNode->StateStack.Add(NewStateStackText); // TODO simplify state stack creation in tests
	FirstStateNode->InitStateStack();
	FirstStateNode->CreateGraphPropertyGraphs();

	// Second state has no node class or stack.
	SecondStateNode->SetNodeClass(USMStateInstance::StaticClass());

	/* Third state (Destination state)
	 * Node class: USMTextGraphState
	 * Stack class: USMStateTestInstance */
	ThirdStateDestNode->SetNodeClass(USMTextGraphState::StaticClass());
	FStateStackContainer NewStateStackInt(USMStateTestInstance::StaticClass());
	ThirdStateDestNode->StateStack.Add(NewStateStackInt);
	ThirdStateDestNode->InitStateStack();
	ThirdStateDestNode->CreateGraphPropertyGraphs();

	FSMBlueprintEditorUtils::ConditionallyCompileBlueprint(NewBP);

	auto FirstStatePropertyNodes = FirstStateNode->GetAllPropertyGraphNodesAsArray();
	auto ThirdStatePropertyNodes = ThirdStateDestNode->GetAllPropertyGraphNodesAsArray();
	
	////////////////////////
	// First state default and graph
	////////////////////////

	const int32 FirstStateDefaultInt = 5;
	{
		FirstStatePropertyNodes[0]->GetSchema()->TrySetDefaultValue(*FirstStatePropertyNodes[0]->GetResultPinChecked(), FString::FromInt(FirstStateDefaultInt));
	}
	

	// graph eval
	const FString FirstStateStringVarDefaultValue = "StringVarDefaultValue";
	{
		FName VarName = "NewStrVar";
		FEdGraphPinType VarType;
		VarType.PinCategory = UEdGraphSchema_K2::PC_String;

		FBlueprintEditorUtils::AddMemberVariable(NewBP, VarName, VarType, FirstStateStringVarDefaultValue);

		// Get class property from new variable.
		FProperty* NewProperty = FSMBlueprintEditorUtils::GetPropertyByName(NewBP, VarName);

		// Place variable getter and wire to result node.
		FSMBlueprintEditorUtils::PlacePropertyOnGraph(FirstStatePropertyNodes[1]->GetGraph(), NewProperty, FirstStatePropertyNodes[1]->GetResultPinChecked(), nullptr);
	}
	
	////////////////////////
	// Third state default
	////////////////////////
	FText ThirdStateDefaultStackTextGraph = FText::FromString("ForStateStackTextGraph");
	{
		USMGraphK2Node_TextPropertyNode* TextPropertyNode = CastChecked<USMGraphK2Node_TextPropertyNode>(ThirdStatePropertyNodes[1]);
		USMTextPropertyGraph* TextPropertyGraph = CastChecked<USMTextPropertyGraph>(TextPropertyNode->GetPropertyGraph());
		TextPropertyGraph->SetNewText(ThirdStateDefaultStackTextGraph);
	}
	
	const int32 ThirdStateDefaultInt = 12;
	{
		ThirdStatePropertyNodes[0]->GetSchema()->TrySetDefaultValue(*ThirdStatePropertyNodes[0]->GetResultPinChecked(), FString::FromInt(ThirdStateDefaultInt));
	}


	TSet<UObject*> NodesToMerge{ FirstStateNode, SecondStateNode, ThirdStateDestNode };
	FSMBlueprintEditorUtils::CombineStates(ThirdStateDestNode, NodesToMerge, true);

	// Verify only the node that had no custom node class remains.
	// The entry point should have moved to the destination state, and the second state should now be connected to the destination state, with the destination state looping back to second state.
	TArray<USMGraphNode_StateNode*> RemainingStates;
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphNode_StateNode>(StateMachineGraph, RemainingStates);
	TestEqual("Only one state removed", RemainingStates.Num(), 2);

	for (USMGraphNode_StateNode* State : RemainingStates)
	{
		TestNotEqual("correct state merged", State, FirstStateNode);
	}
	
	const int TotalExpectedProperties = (1 + 2) + (0) + (1 + 1);

	auto PropertyNodes = ThirdStateDestNode->GetAllPropertyGraphNodesAsArray();
	TestEqual("State stacks added", PropertyNodes.Num(), TotalExpectedProperties);

	////////////////////////
	// Test executing default value.
	////////////////////////

	int32 A, B, C;
	const int32 MaxIterations = 10;
	int32 IterationsRan = 0;
	USMInstance* Instance = TestHelpers::RunStateMachineToCompletion(this, NewBP, A, B, C, MaxIterations, false, false, true, &IterationsRan);
	TestEqual("Looped through to max iterations", IterationsRan, MaxIterations);
	
	Instance->Stop();
	
	USMTextGraphState* NodeInstance = CastChecked<USMTextGraphState>(Instance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());

	int32 ExpectedHits = MaxIterations / 2 + 1; // 2 states for 10 iterations alternating, with the initial start.
	
	// Test original state/stack
	{
		TestEqual("Default exposed value set and evaluated", NodeInstance->EvaluatedText.ToString(), ThirdStateDefaultStackTextGraph.ToString());

		USMStateTestInstance* StateStackInstance = CastChecked<USMStateTestInstance>(NodeInstance->GetStateInStack(0));
		TestEqual("Default exposed value set and evaluated", StateStackInstance->ExposedInt, ThirdStateDefaultInt + 1); // Default gets added to in the context.
		TestEqual("Stack evaluated", StateStackInstance->StateBeginHit.Count, ExpectedHits);
		TestEqual("Stack evaluated", StateStackInstance->StateUpdateHit.Count, 0);
		TestEqual("Stack evaluated", StateStackInstance->StateEndHit.Count, ExpectedHits);
	}

	// State class from first node.
	{
		USMStateTestInstance* StateStackInstance = CastChecked<USMStateTestInstance>(NodeInstance->GetStateInStack(1));
		TestEqual("Default exposed value set and evaluated", StateStackInstance->ExposedInt, FirstStateDefaultInt + 1); // Default gets added to in the context.
		TestEqual("Stack evaluated", StateStackInstance->StateBeginHit.Count, ExpectedHits);
		TestEqual("Stack evaluated", StateStackInstance->StateUpdateHit.Count, 0);
		TestEqual("Stack evaluated", StateStackInstance->StateEndHit.Count, ExpectedHits);
	}
	
	// State stack from first node.
	{
		USMTextGraphStateExtra* StateStackInstance = CastChecked<USMTextGraphStateExtra>(NodeInstance->GetStateInStack(2));
		TestEqual("Default exposed value set and evaluated", StateStackInstance->StringVar, FirstStateStringVarDefaultValue); // This also tests that on state begin is hit.
	}
	
	return NewAsset.DeleteAsset(this);
}


#endif

#endif //WITH_DEV_AUTOMATION_TESTS