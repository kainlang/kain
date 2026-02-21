// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMTestHelpers.h"
#include "SMTestContext.h"
#include "Helpers/SMTestBoilerplate.h"

#include "Utilities/SMBlueprintEditorUtils.h"
#include "Blueprints/SMBlueprintFactory.h"
#include "Graph/SMGraph.h"
#include "Graph/SMStateGraph.h"
#include "Graph/SMTextPropertyGraph.h"
#include "Graph/Nodes/SMGraphK2Node_StateMachineNode.h"
#include "Graph/Nodes/SMGraphNode_StateNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineEntryNode.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/Nodes/SMGraphNode_ConduitNode.h"
#include "Graph/Nodes/PropertyNodes/SMGraphK2Node_TextPropertyNode.h"

#include "Blueprints/SMBlueprint.h"
#include "SMUtils.h"

#include "K2Node_CommutativeAssociativeBinaryOperator.h"

#if WITH_DEV_AUTOMATION_TESTS

#if PLATFORM_DESKTOP

/**
 * Verify states and variables can be added to the stack properly.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStateStackTest, "LogicDriver.StateStack.Comprehensive", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FStateStackTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(1)

	UEdGraphPin* LastStatePin = nullptr;

	// Build single state - state machine.
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMStateTestInstance::StaticClass(), USMTransitionTestInstance::StaticClass());

	USMGraphNode_StateNode* StateNode = CastChecked<USMGraphNode_StateNode>(StateMachineGraph->GetEntryNode()->GetOutputNode());

	TestEqual("Empty state stack", StateNode->StateStack.Num(), 0);
	auto PropertyNodes = StateNode->GetAllPropertyGraphNodesAsArray();
	TestEqual("Initial state property node only", PropertyNodes.Num(), 1);
	
	// Add a state stack
	
	FStateStackContainer NewStateStackText(USMTextGraphStateExtra::StaticClass());
	StateNode->StateStack.Add(NewStateStackText); // TODO simplify state stack creation in tests
	FStateStackContainer NewStateStackInt(USMStateTestInstance::StaticClass());
	StateNode->StateStack.Add(NewStateStackInt);
	StateNode->InitStateStack();
	StateNode->CreateGraphPropertyGraphs();
	FSMBlueprintEditorUtils::ConditionallyCompileBlueprint(NewBP);

	PropertyNodes = StateNode->GetAllPropertyGraphNodesAsArray();
	TestEqual("State stacks added", PropertyNodes.Num(), 4);

	TestEqual("First property graph is for original state", PropertyNodes[0]->GetOwningTemplate()->GetClass(), USMStateTestInstance::StaticClass());
	
	TestEqual("Next property graph is for state stack", PropertyNodes[1]->GetOwningTemplate()->GetClass(), USMTextGraphStateExtra::StaticClass());
	TestEqual("Next property graph is for state stack", PropertyNodes[2]->GetOwningTemplate()->GetClass(), USMTextGraphStateExtra::StaticClass());
	
	TestEqual("Last property graph is for state state", PropertyNodes[3]->GetOwningTemplate()->GetClass(), USMStateTestInstance::StaticClass());
	
	////////////////////////
	// Test setting default value.
	////////////////////////

	// State value
	const int32 StateDefaultInt = 12;
	PropertyNodes[0]->GetSchema()->TrySetDefaultValue(*PropertyNodes[0]->GetResultPinChecked(), FString::FromInt(StateDefaultInt)); // TrySet needed to trigger DefaultValueChanged

	// State stack string value
	FString DefaultStackStr = "ForStateStackString";
	PropertyNodes[1]->GetSchema()->TrySetDefaultValue(*PropertyNodes[1]->GetResultPinChecked(), DefaultStackStr); // TrySet needed to trigger DefaultValueChanged
	
	// State stack text graph value
	FText DefaultStackTextGraph = FText::FromString("ForStateStackTextGraph");
	USMGraphK2Node_TextPropertyNode* TextPropertyNode = Cast<USMGraphK2Node_TextPropertyNode>(PropertyNodes[2]);
	if (!TestNotNull("TextProperty in correct index", TextPropertyNode))
	{
		return false;
	}
	USMTextPropertyGraph* TextPropertyGraph = CastChecked<USMTextPropertyGraph>(TextPropertyNode->GetPropertyGraph());
	TextPropertyGraph->SetNewText(DefaultStackTextGraph);

	////////////////////////
	// Test executing default value.
	////////////////////////
	USMInstance* Instance = TestHelpers::TestLinearStateMachine(this, NewBP, TotalStates, false);

	USMStateTestInstance* NodeInstance = CastChecked<USMStateTestInstance>(Instance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
	TestEqual("Default exposed value set and evaluated", NodeInstance->ExposedInt, StateDefaultInt + 1); // Default gets added to in the context.
	
	USMTextGraphStateExtra* StateStackInstance = CastChecked<USMTextGraphStateExtra>(NodeInstance->GetStateInStack(0));
	TestEqual("Default exposed value set and evaluated", StateStackInstance->EvaluatedText.ToString(), DefaultStackTextGraph.ToString()); // This also tests that on state begin is hit.
	TestEqual("Default exposed value set and evaluated", StateStackInstance->StringVar, DefaultStackStr);

	USMStateTestInstance* LastStateStackInstance = CastChecked<USMStateTestInstance>(NodeInstance->GetStateInStack(1));
	TestEqual("Stack evaluated", LastStateStackInstance->StateBeginHit.Count, 1);
	TestEqual("Stack evaluated", LastStateStackInstance->StateUpdateHit.Count, 1);
	TestEqual("Stack evaluated", LastStateStackInstance->StateEndHit.Count, 0);

	Instance->Stop();
	TestEqual("Stack evaluated", LastStateStackInstance->StateEndHit.Count, 1);

	TestEqual("Stack evaluated initialize", LastStateStackInstance->StateInitializedEventHit.Count, 1);
	TestEqual("Stack evaluated shutdown", LastStateStackInstance->StateShutdownEventHit.Count, 1);
	
	////////////////////////
	// Test graph evaluation -- needs to be done from a variable.
	////////////////////////

	// Create new variable.
	const int32 TestVarDefaultValue = 15;
	{
		FName VarName = "NewVar";
		FEdGraphPinType VarType;
		VarType.PinCategory = UEdGraphSchema_K2::PC_Int;

		FBlueprintEditorUtils::AddMemberVariable(NewBP, VarName, VarType, FString::FromInt(TestVarDefaultValue));

		// Get class property from new variable.
		FProperty* NewProperty = FSMBlueprintEditorUtils::GetPropertyByName(NewBP, VarName);

		// Place variable getter and wire to result node.
		FSMBlueprintEditorUtils::PlacePropertyOnGraph(PropertyNodes[0]->GetGraph(), NewProperty, PropertyNodes[0]->GetResultPinChecked(), nullptr);
	}

	const FString TestStringDefaultValue = "StringVarDefaultValue";
	{
		FName VarName = "NewStrVar";
		FEdGraphPinType VarType;
		VarType.PinCategory = UEdGraphSchema_K2::PC_String;
		
		FBlueprintEditorUtils::AddMemberVariable(NewBP, VarName, VarType, TestStringDefaultValue);

		// Get class property from new variable.
		FProperty* NewProperty = FSMBlueprintEditorUtils::GetPropertyByName(NewBP, VarName);

		// Place variable getter and wire to result node.
		FSMBlueprintEditorUtils::PlacePropertyOnGraph(PropertyNodes[1]->GetGraph(), NewProperty, PropertyNodes[1]->GetResultPinChecked(), nullptr);
	}

	// Test results

	{
		Instance = TestHelpers::TestLinearStateMachine(this, NewBP, TotalStates);
		NodeInstance = CastChecked<USMStateTestInstance>(Instance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
		TestEqual("Default exposed value set and evaluated", NodeInstance->ExposedInt, TestVarDefaultValue + 1);

		StateStackInstance = CastChecked<USMTextGraphStateExtra>(NodeInstance->GetStateInStack(0));
		TestEqual("Default exposed value set and evaluated", StateStackInstance->StringVar, TestStringDefaultValue);

		TArray<USMStateInstance_Base*> AllStackInstances;
		NodeInstance->GetAllStateStackInstances(AllStackInstances);
		TestEqual("Stack instances found", AllStackInstances.Num(), 2);
	}

	// Test specific results
	{
		Instance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, NewObject<USMTestContext>());

		NodeInstance = CastChecked<USMStateTestInstance>(Instance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
		TestNotEqual("Default exposed value not evaluated", NodeInstance->ExposedInt, TestVarDefaultValue);

		StateStackInstance = CastChecked<USMTextGraphStateExtra>(NodeInstance->GetStateInStack(0));
		TestNotEqual("Default exposed value not set and evaluated", StateStackInstance->StringVar, TestStringDefaultValue);

		// Evaluate just this node instance.
		{
			NodeInstance->EvaluateGraphProperties(true);
			TestEqual("Default exposed value evaluated", NodeInstance->ExposedInt, TestVarDefaultValue);

			// Verify first stack not evaluated.
			StateStackInstance = CastChecked<USMTextGraphStateExtra>(NodeInstance->GetStateInStack(0));
			TestNotEqual("Default exposed value not set and evaluated", StateStackInstance->StringVar, TestStringDefaultValue);
		}

		// Evaluate second state stack.
		{
			USMStateInstance_Base* SecondStateStackInstance = NodeInstance->GetStateInStack(1);
			check(SecondStateStackInstance);
			SecondStateStackInstance->EvaluateGraphProperties(true);

			// Verify first stack still not evaluated.
			TestNotEqual("Default exposed value not set and evaluated", StateStackInstance->StringVar, TestStringDefaultValue);
		}

		// Evaluate first state stack which should now work.
		{
			StateStackInstance->EvaluateGraphProperties(true);
			TestEqual("Default exposed value set and evaluated", StateStackInstance->StringVar, TestStringDefaultValue);
		}
	}

	
	USMTextGraphStateExtra* StackTextInstance = CastChecked<USMTextGraphStateExtra>(NodeInstance->GetStateInStack(0));
	USMStateTestInstance* StackTestInstance = CastChecked<USMStateTestInstance>(NodeInstance->GetStateInStack(1));
	// Test class search.
	
	{
		USMStateTestInstance* ClassFoundInstance = Cast<USMStateTestInstance>(NodeInstance->GetStateInStackByClass(USMStateTestInstance::StaticClass()));
		TestEqual("State stack found by class", ClassFoundInstance, StackTestInstance);
	}
	{
		USMTextGraphState* ClassFoundInstance = Cast<USMTextGraphState>(NodeInstance->GetStateInStackByClass(USMTextGraphState::StaticClass()));
		TestNull("Didn't find because child not searched for", ClassFoundInstance);
	}
	{
		USMTextGraphState* ClassFoundInstance = Cast<USMTextGraphState>(NodeInstance->GetStateInStackByClass(USMTextGraphState::StaticClass(), true));
		TestEqual("State stack found by child", ClassFoundInstance, Cast<USMTextGraphState>(StackTextInstance));
	}

	{
		TArray<USMStateInstance_Base*> FoundClassInstances;
		NodeInstance->GetAllStatesInStackOfClass(USMStateTestInstance::StaticClass(), FoundClassInstances);
		TestEqual("1 result found", FoundClassInstances.Num(), 1);
		TestTrue("Found stack instance", FoundClassInstances.Contains(StackTestInstance));
		
		NodeInstance->GetAllStatesInStackOfClass(USMStateTestInstance::StaticClass(), FoundClassInstances, true);
		TestEqual("1 result found even though children included", 1, FoundClassInstances.Num());
		TestTrue("Found stack instance", FoundClassInstances.Contains(StackTestInstance));
		
		NodeInstance->GetAllStatesInStackOfClass(USMStateInstance::StaticClass(), FoundClassInstances, true);
		TestEqual("All results found", FoundClassInstances.Num(), 2);
		TestTrue("Found stack instance", FoundClassInstances.Contains(StackTestInstance));
		TestTrue("Found stack instance", FoundClassInstances.Contains(StackTextInstance));

		// Test index lookup.
		{
			int32 Index = NodeInstance->GetStateIndexInStack(FoundClassInstances[0]);
			TestEqual("Index found", Index, 0);
		}
		{
			int32 Index = NodeInstance->GetStateIndexInStack(FoundClassInstances[1]);
			TestEqual("Index found", Index, 1);
		}
		{
			int32 Index = NodeInstance->GetStateIndexInStack(NodeInstance);
			TestEqual("Index not found", Index, INDEX_NONE);
		}
		{
			int32 Index = NodeInstance->GetStateIndexInStack(nullptr);
			TestEqual("Index not found", Index, INDEX_NONE);
		}
	}

	TestEqual("Stack could find node instance", StackTestInstance->GetStackOwnerInstance(), Cast<USMStateInstance_Base>(NodeInstance));
	TestEqual("Stack could find node instance", StackTextInstance->GetStackOwnerInstance(), Cast<USMStateInstance_Base>(NodeInstance));
	TestEqual("Node instance found itself", NodeInstance->GetStackOwnerInstance(), Cast<USMStateInstance_Base>(NodeInstance));
	
	return NewAsset.DeleteAsset(this);
}

#endif

#endif //WITH_DEV_AUTOMATION_TESTS