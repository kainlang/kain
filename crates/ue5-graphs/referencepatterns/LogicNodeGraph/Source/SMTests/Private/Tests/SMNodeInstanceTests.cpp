// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Helpers/SMTestBoilerplate.h"
#include "SMTestContext.h"
#include "SMTestHelpers.h"

#include "Blueprints/SMBlueprintFactory.h"
#include "Configuration/SMProjectEditorSettings.h"
#include "Graph/Nodes/PropertyNodes/SMGraphK2Node_PropertyNode_Base.h"
#include "Graph/Nodes/SMGraphK2Node_StateMachineNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineEntryNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineStateNode.h"
#include "Graph/Nodes/SMGraphNode_StateNode.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/Schema/SMGraphK2Schema.h"
#include "Graph/SMGraph.h"
#include "Graph/SMPropertyGraph.h"
#include "Graph/SMStateGraph.h"
#include "Utilities/SMBlueprintEditorUtils.h"
#include "Utilities/SMNodeInstanceUtils.h"

#include "Blueprints/SMBlueprint.h"
#include "Blueprints/SMBlueprintGeneratedClass.h"
#include "SMRuntimeSettings.h"
#include "SMUtils.h"

#include "Kismet2/KismetEditorUtilities.h"

#if WITH_DEV_AUTOMATION_TESTS

#if PLATFORM_DESKTOP

/**
 * Create node class blueprints.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNodeInstanceCreateNodeInstanceTest, "LogicDriver.NodeInstance.CreateBP", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FNodeInstanceCreateNodeInstanceTest::RunTest(const FString& Parameters)
{
	// Create node classes.
	FAssetHandler StateAsset;
	if (!TestHelpers::TryCreateNewNodeAsset(this, StateAsset, USMStateInstance::StaticClass(), true))
	{
		return false;
	}
	
	return StateAsset.DeleteAsset(this);
}

/**
 * Select a node class and test making sure instance nodes are set and hit properly.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNodeInstanceEvalVariableTest, "LogicDriver.NodeInstance.Variables.Eval", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FNodeInstanceEvalVariableTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(1)

	// Build single state - state machine.
	UEdGraphPin* LastStatePin = nullptr;
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMStateTestInstance::StaticClass(), USMTransitionTestInstance::StaticClass());
	if (!NewAsset.SaveAsset(this))
	{
		return false;
	}
	TestHelpers::TestLinearStateMachine(this, NewBP, TotalStates);

	////////////////////////
	// Test setting default value.
	////////////////////////
	const int32 TestDefaultInt = 12;
	
	USMGraphNode_StateNode* StateNode = CastChecked<USMGraphNode_StateNode>(StateMachineGraph->GetEntryNode()->GetOutputNode());
	auto PropertyNodes = StateNode->GetAllPropertyGraphNodesAsArray();
	PropertyNodes[0]->GetSchema()->TrySetDefaultValue(*PropertyNodes[0]->GetResultPinChecked(), FString::FromInt(TestDefaultInt)); // TrySet needed to trigger DefaultValueChanged

	USMInstance* Instance = TestHelpers::TestLinearStateMachine(this, NewBP, TotalStates, false);

	USMStateTestInstance* NodeInstance = CastChecked<USMStateTestInstance>(Instance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
	TestEqual("Default exposed value set and evaluated", NodeInstance->ExposedInt, TestDefaultInt + 1); // Default gets added to in the context.

	TestTrue("Is default value", (*NodeInstance->GetOwningNode()->GetTemplateGraphProperties().CreateConstIterator()).Value.VariableGraphProperties[0].GetIsDefaultValueOnly());

	// Check manual evaluation. Alter the template directly rather than the class even though this isn't normally allowed.
	USMStateInstance* StateInstanceTemplate = CastChecked<USMStateInstance>(StateNode->GetNodeTemplate());
	// This will reset the begin evaluation.
	StateInstanceTemplate->bEvalGraphsOnUpdate = true;

	Instance = TestHelpers::TestLinearStateMachine(this, NewBP, TotalStates, false);
	Instance->Update(0.f); // One more update to trigger Update eval.
	NodeInstance = CastChecked<USMStateTestInstance>(Instance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
	TestEqual("Default exposed value set and evaluated", NodeInstance->ExposedInt, TestDefaultInt); // Verify the value matches the default.

	////////////////////////
	// Test graph evaluation -- needs to be done from a variable.
	////////////////////////
	FName VarName = "NewVar";
	FEdGraphPinType VarType;
	VarType.PinCategory = UEdGraphSchema_K2::PC_Int;

	// Create new variable.
	const int32 TestVarDefaultValue = 15;
	FBlueprintEditorUtils::AddMemberVariable(NewBP, VarName, VarType, FString::FromInt(TestVarDefaultValue));

	// Get class property from new variable.
	FProperty* NewProperty = FSMBlueprintEditorUtils::GetPropertyByName(NewBP, VarName);

	// Place variable getter and wire to result node.
	FSMBlueprintEditorUtils::PlacePropertyOnGraph(PropertyNodes[0]->GetGraph(), NewProperty, PropertyNodes[0]->GetResultPinChecked(), nullptr);

	Instance = TestHelpers::TestLinearStateMachine(this, NewBP, TotalStates);
	NodeInstance = CastChecked<USMStateTestInstance>(Instance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
	TestEqual("Default exposed value set and evaluated", NodeInstance->ExposedInt, TestVarDefaultValue); // Verify the value evaluated properly.

	TestFalse("Is not default value", (*NodeInstance->GetOwningNode()->GetTemplateGraphProperties().CreateConstIterator()).Value.VariableGraphProperties[0].GetIsDefaultValueOnly());
	
	StateInstanceTemplate->bEvalGraphsOnUpdate = false;
	// Begin state.
	{
		StateInstanceTemplate->bEvalGraphsOnStart = false;
		Instance = TestHelpers::CompileAndCreateStateMachineInstanceFromBP(NewBP);
		Instance->Start();
		
		NodeInstance = CastChecked<USMStateTestInstance>(Instance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
		TestNotEqual("Default exposed value not evaluated", NodeInstance->ExposedInt, TestVarDefaultValue);
		
		StateInstanceTemplate->bEvalGraphsOnStart = true;
		Instance = TestHelpers::CompileAndCreateStateMachineInstanceFromBP(NewBP);
		Instance->Start();
		
		NodeInstance = CastChecked<USMStateTestInstance>(Instance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
		TestEqual("Default exposed value set and evaluated", NodeInstance->ExposedInt, TestVarDefaultValue + 1); // Verify the value evaluated properly and was modified.
	}
	
	// Update state.
	{
		StateInstanceTemplate->bEvalGraphsOnStart = false;
		StateInstanceTemplate->bEvalGraphsOnUpdate = false;
		Instance = TestHelpers::CompileAndCreateStateMachineInstanceFromBP(NewBP);
		Instance->Start();
		Instance->Update();
		
		NodeInstance = CastChecked<USMStateTestInstance>(Instance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
		TestNotEqual("Default exposed value not evaluated", NodeInstance->ExposedInt, TestVarDefaultValue);
		
		StateInstanceTemplate->bEvalGraphsOnUpdate = true;
		Instance = TestHelpers::CompileAndCreateStateMachineInstanceFromBP(NewBP);
		Instance->Start();
		Instance->Update();
		
		NodeInstance = CastChecked<USMStateTestInstance>(Instance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
		TestEqual("Default exposed value set and evaluated", NodeInstance->ExposedInt, TestVarDefaultValue); // Verify the value evaluated properly and was modified.

		StateInstanceTemplate->bEvalGraphsOnUpdate = false;
	}

	// End state.
	{
		StateInstanceTemplate->bEvalGraphsOnStart = false;
		StateInstanceTemplate->bEvalGraphsOnUpdate = false;
		StateInstanceTemplate->bEvalGraphsOnEnd = false;
		Instance = TestHelpers::CompileAndCreateStateMachineInstanceFromBP(NewBP);
		Instance->Start();
		Instance->Update();
		Instance->Stop();
		
		NodeInstance = CastChecked<USMStateTestInstance>(Instance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
		TestNotEqual("Default exposed value not evaluated", NodeInstance->ExposedInt, TestVarDefaultValue);
		
		StateInstanceTemplate->bEvalGraphsOnEnd = true;
		Instance = TestHelpers::CompileAndCreateStateMachineInstanceFromBP(NewBP);
		Instance->Start();
		Instance->Update();
		Instance->Stop();
		
		NodeInstance = CastChecked<USMStateTestInstance>(Instance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
		TestEqual("Default exposed value set and evaluated", NodeInstance->ExposedInt, TestVarDefaultValue); // Verify the value evaluated properly and was modified.
		
		StateInstanceTemplate->bEvalGraphsOnEnd = false;
	}

	// State Machine Start.
	{
		StateInstanceTemplate->bEvalGraphsOnRootStateMachineStart = true;
		Instance = TestHelpers::CompileAndCreateStateMachineInstanceFromBP(NewBP);
		Instance->Start();
		
		NodeInstance = CastChecked<USMStateTestInstance>(Instance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
		TestEqual("Default exposed value set and evaluated", NodeInstance->ExposedInt, TestVarDefaultValue + 1); // Verify the value evaluated properly and was modified.

		StateInstanceTemplate->bEvalGraphsOnRootStateMachineStart = false;
	}

	// State Machine Stop.
	{
		StateInstanceTemplate->bEvalGraphsOnRootStateMachineStop = true;
		Instance = TestHelpers::CompileAndCreateStateMachineInstanceFromBP(NewBP);
		Instance->Start();
		Instance->Stop();
		NodeInstance = CastChecked<USMStateTestInstance>(Instance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
		TestEqual("Default exposed value set and evaluated", NodeInstance->ExposedInt, TestVarDefaultValue); // Verify the value evaluated properly and was modified.

		StateInstanceTemplate->bEvalGraphsOnRootStateMachineStop = false;
	}
	
	// Disable auto evaluation all together.
	StateInstanceTemplate->bAutoEvalExposedProperties = false;
	Instance = TestHelpers::TestLinearStateMachine(this, NewBP, TotalStates, false);

	NodeInstance = CastChecked<USMStateTestInstance>(Instance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
	TestNotEqual("Default exposed value not evaluated", NodeInstance->ExposedInt, TestVarDefaultValue);

	// Manual evaluation.
	NodeInstance->EvaluateGraphProperties();
	TestEqual("Default exposed value set and evaluated", NodeInstance->ExposedInt, TestVarDefaultValue); // Verify the value evaluated properly.
	
	return NewAsset.DeleteAsset(this);
}

/**
 * Verify default value optimizations.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNodeInstanceDefaultValueOptimizationTest, "LogicDriver.NodeInstance.Variables.DefaultOptimization", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FNodeInstanceDefaultValueOptimizationTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(1)

	// Build single state - state machine.
	UEdGraphPin* LastStatePin = nullptr;
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMStateTestInstance::StaticClass(), USMTransitionTestInstance::StaticClass());

	////////////////////////
	// Test without optimization
	////////////////////////
	int32 TestDefaultInt = 12;
	
	USMGraphNode_StateNode* StateNode = CastChecked<USMGraphNode_StateNode>(StateMachineGraph->GetEntryNode()->GetOutputNode());
	auto PropertyNodes = StateNode->GetAllPropertyGraphNodesAsArray();
	PropertyNodes[0]->GetSchema()->TrySetDefaultValue(*PropertyNodes[0]->GetResultPinChecked(), FString::FromInt(TestDefaultInt)); // TrySet needed to trigger DefaultValueChanged

	USMInstance* Instance = TestHelpers::TestLinearStateMachine(this, NewBP, TotalStates, false);

	USMStateTestInstance* NodeInstance = CastChecked<USMStateTestInstance>(Instance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
	TestEqual("Default exposed value set and evaluated", NodeInstance->ExposedInt, TestDefaultInt + 1); // Default gets added to in the context.

	TestTrue("Is default value", (*NodeInstance->GetOwningNode()->GetTemplateGraphProperties().CreateConstIterator()).Value.VariableGraphProperties[0].GetIsDefaultValueOnly());
	
	// Check manual evaluation. Alter the template directly rather than the class even though this isn't normally allowed.
	USMStateInstance* StateInstanceTemplate = CastChecked<USMStateInstance>(StateNode->GetNodeTemplate());
	// This will reset the begin evaluation.
	StateInstanceTemplate->bEvalGraphsOnUpdate = true;

	Instance = TestHelpers::TestLinearStateMachine(this, NewBP, TotalStates, false);
	
	NodeInstance = CastChecked<USMStateTestInstance>(Instance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
	NodeInstance->ExposedInt++;
	Instance->Update(0.f); // One more update to trigger Update eval.
	TestEqual("Default exposed value set and evaluated", NodeInstance->ExposedInt, TestDefaultInt);
	
	// Manual evaluation.
	NodeInstance->ExposedInt++;
	NodeInstance->EvaluateGraphProperties();
	TestEqual("Default exposed value set and evaluated", NodeInstance->ExposedInt, TestDefaultInt);

	////////////////////////
	// Test with optimization
	////////////////////////
	{
		NodeInstance->bEvalDefaultProperties = false;
		NodeInstance->ExposedInt++;
		Instance->Update(0.f); // One more update to trigger Update eval
		TestEqual("Default exposed value set and evaluated", NodeInstance->ExposedInt, ++TestDefaultInt);
		
		// Manual evaluation.
		NodeInstance->ExposedInt++;
		NodeInstance->EvaluateGraphProperties();
		TestEqual("Default exposed value set and evaluated", NodeInstance->ExposedInt, ++TestDefaultInt);
	}
	
	return NewAsset.DeleteAsset(this);
}

/**
 * Verify exposed array defaults are set.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNodeInstanceArrayDefaultsTest, "LogicDriver.NodeInstance.Variables.ArrayDefaults", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

	bool FNodeInstanceArrayDefaultsTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(1)

	// Build single state - state machine.
	UEdGraphPin* LastStatePin = nullptr;
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMStateArrayTestInstance::StaticClass(), USMTransitionTestInstance::StaticClass());
	if (!NewAsset.SaveAsset(this))
	{
		return false;
	}

	////////////////////////////////
	// Read default array elements.
	////////////////////////////////
	{
		USMGraphNode_StateNode* StateNode = CastChecked<USMGraphNode_StateNode>(StateMachineGraph->GetEntryNode()->GetOutputNode());
		auto PropertyNodes = StateNode->GetAllPropertyGraphNodesAsArray();

		// Pins
		TestEqual("Array element 1 set", PropertyNodes[0]->GetResultPinChecked()->GetDefaultAsString(), FString::FromInt(USMStateArrayTestInstance::ExposedIntArrDefaultValue1));
		TestEqual("Array element 2 set", PropertyNodes[1]->GetResultPinChecked()->GetDefaultAsString(), FString::FromInt(USMStateArrayTestInstance::ExposedIntArrDefaultValue2));

		FKismetEditorUtilities::CompileBlueprint(NewBP);
	}
	
	// Runtime
	{
		USMInstance* Instance = TestHelpers::TestLinearStateMachine(this, NewBP, TotalStates, false);
		USMStateArrayTestInstance* NodeInstance = CastChecked<USMStateArrayTestInstance>(Instance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
		TestEqual("Default exposed value set and evaluated", NodeInstance->ExposedIntArray[0], USMStateArrayTestInstance::ExposedIntArrDefaultValue1);
		TestEqual("Default exposed value set and evaluated", NodeInstance->ExposedIntArray[1], USMStateArrayTestInstance::ExposedIntArrDefaultValue2);
	}
	
	////////////////////////////////
	// Set default array elements.
	////////////////////////////////
	const int32 NewDefault1 = 18015, NewDefault2 = 9153;
	{
		USMGraphNode_StateNode* StateNode = CastChecked<USMGraphNode_StateNode>(StateMachineGraph->GetEntryNode()->GetOutputNode());
		auto PropertyNodes = StateNode->GetAllPropertyGraphNodesAsArray();
		// Pins

		PropertyNodes[0]->GetSchema()->TrySetDefaultValue(*PropertyNodes[0]->GetResultPinChecked(), FString::FromInt(NewDefault1));
		PropertyNodes[1]->GetSchema()->TrySetDefaultValue(*PropertyNodes[1]->GetResultPinChecked(), FString::FromInt(NewDefault2));

		FKismetEditorUtilities::CompileBlueprint(NewBP);
	}
	// Read new defaults from runtime.
	{
		USMInstance* Instance = TestHelpers::TestLinearStateMachine(this, NewBP, TotalStates, false);
		USMStateArrayTestInstance* NodeInstance = CastChecked<USMStateArrayTestInstance>(Instance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
		TestEqual("Default exposed value set and evaluated", NodeInstance->ExposedIntArray[0], NewDefault1);
		TestEqual("Default exposed value set and evaluated", NodeInstance->ExposedIntArray[1], NewDefault2);
	}
	return true;
}

/**
 * Verify read only variables are on the node but not the runtime property.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNodeInstanceReadOnlyVariableTest, "LogicDriver.NodeInstance.Variables.ReadOnly", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FNodeInstanceReadOnlyVariableTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(1)
	
	// Build single state - state machine.
	UEdGraphPin* LastStatePin = nullptr;
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMStateReadOnlyTestInstance::StaticClass(), USMTransitionTestInstance::StaticClass());
	if (!NewAsset.SaveAsset(this))
	{
		return false;
	}
	
	////////////////////////
	// Test setting default value.
	////////////////////////
	const int32 TestDefaultInt = USMStateReadOnlyTestInstance::DefaultReadOnlyValue;
	
	USMGraphNode_StateNode* StateNode = CastChecked<USMGraphNode_StateNode>(StateMachineGraph->GetEntryNode()->GetOutputNode());
	auto PropertyNodes = StateNode->GetAllPropertyGraphNodesAsArray();

	UEdGraphPin* ResultPin = PropertyNodes[0]->GetResultPinChecked();
	TestEqual("Result pin set to default", ResultPin->GetDefaultAsString(), FString::FromInt(TestDefaultInt));

	TestFalse("Property graph is read-only", PropertyNodes[0]->GetPropertyGraph()->bEditable);
	TestTrue("Property graph is editable desired", PropertyNodes[0]->GetPropertyGraph()->IsGraphEditDesired());
	TestTrue("Property graph variable is read only", PropertyNodes[0]->GetPropertyGraph()->IsVariableReadOnly());
	
	TestTrue("Variable is read only", PropertyNodes[0]->GetPropertyNodeChecked()->IsVariableReadOnly());
	
	TestTrue("Default value is read only", ResultPin->bDefaultValueIsReadOnly);
	TestTrue("Not connectable", ResultPin->bNotConnectable);

	USMInstance* Instance = TestHelpers::TestLinearStateMachine(this, NewBP, TotalStates, false);

	USMStateReadOnlyTestInstance* NodeInstance = CastChecked<USMStateReadOnlyTestInstance>(Instance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
	TestEqual("Default exposed value set on instance", NodeInstance->ReadOnlyInt, TestDefaultInt);

	TestEqual("No graph properties embedded", NodeInstance->GetOwningNode()->GetGraphProperties().Num(), 0);
	
	return NewAsset.DeleteAsset(this);
}

/**
 * Run a state machine consisting of 100 custom state classes with custom transitions.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNodeInstanceRunStateMachineTest, "LogicDriver.NodeInstance.RunStateMachine", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FNodeInstanceRunStateMachineTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(100)
	UEdGraphPin* LastStatePin = nullptr;
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMStateTestInstance::StaticClass(), USMTransitionTestInstance::StaticClass());
	TestHelpers::TestLinearStateMachine(this, NewBP, TotalStates);

	return true;
}

/**
 * Verify node instance struct wrapper methods work properly.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNodeInstanceMethodsTest, "LogicDriver.NodeInstance.Methods", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FNodeInstanceMethodsTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(2)

	{
		UEdGraphPin* LastStatePin = nullptr;
		//Verify default instances load correctly.
		TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMStateInstance::StaticClass(), USMTransitionInstance::StaticClass());
		int32 A, B, C;
		TestHelpers::RunStateMachineToCompletion(this, NewBP, A, B, C);
		FSMBlueprintEditorUtils::RemoveAllNodesFromGraph(StateMachineGraph);
	}
	
	// Load test instances.
	UEdGraphPin* LastStatePin = nullptr;
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMStateTestInstance::StaticClass(), USMTransitionTestInstance::StaticClass());
	FKismetEditorUtilities::CompileBlueprint(NewBP);
	
	USMTestContext* Context = NewObject<USMTestContext>();
	USMInstance* StateMachineInstance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);
	
	FSMState_Base* InitialState = StateMachineInstance->GetRootStateMachine().GetSingleInitialState();
	USMStateInstance_Base* NodeInstance = CastChecked<USMStateInstance_Base>(InitialState->GetNodeInstance());
	InitialState->bAlwaysUpdate = true; // Needed since we are manually switching states later.

	{
		// Test root and entry nodes.
		
		USMStateMachineInstance* RootStateMachineInstance = StateMachineInstance->GetRootStateMachineNodeInstance();
		TestNotNull("Root node not null", RootStateMachineInstance);
		TestEqual("Root node discoverable", RootStateMachineInstance, Cast<USMStateMachineInstance>(StateMachineInstance->GetRootStateMachine().GetNodeInstance()));

		TArray<USMStateInstance_Base*> EntryStates;
		RootStateMachineInstance->GetEntryStates(EntryStates);
		check(EntryStates.Num() == 1);

		TestEqual("Entry states discoverable", EntryStates[0], NodeInstance);
		TestTrue("Entry state is configured", EntryStates[0]->IsEntryState());
	}
	
	TestEqual("Correct state machine", NodeInstance->GetStateMachineInstance(), StateMachineInstance);
	TestEqual("Guids correct", NodeInstance->GetGuid(), InitialState->GetGuid());
	TestEqual("Name correct", NodeInstance->GetNodeName(), InitialState->GetNodeName());
	
	TestFalse("Initial state not active", NodeInstance->IsActive());
	StateMachineInstance->Start();
	TestTrue("Initial state active", NodeInstance->IsActive());

	InitialState->TimeInState = 3;
	TestEqual("Time correct", NodeInstance->GetTimeInState(), InitialState->TimeInState);

	FSMStateInfo StateInfo;
	NodeInstance->GetStateInfo(StateInfo);

	TestEqual("State info guids correct", StateInfo.Guid, InitialState->GetGuid());
	TestEqual("State info instance correct", StateInfo.NodeInstance.Get(), Cast<USMNodeInstance>(NodeInstance));
	TestFalse("Not a state machine", NodeInstance->IsStateMachine());
	TestFalse("Not an end state", NodeInstance->IsEndState());
	TestFalse("Has not updated", NodeInstance->HasUpdated());
	TestNull("No transition to take", NodeInstance->GetTransitionToTake());

	USMStateInstance_Base* NextState = CastChecked<USMStateInstance_Base>(InitialState->GetOutgoingTransitions()[0]->GetToState()->GetNodeInstance());

	// Test searching nodes.
	TArray<USMNodeInstance*> FoundNodes;
	NodeInstance->GetAllNodesOfType(FoundNodes, USMStateInstance::StaticClass());

	TestEqual("All nodes found", FoundNodes.Num(), TotalStates);
	TestEqual("Correct state found", FoundNodes[0], Cast<USMNodeInstance>(NodeInstance));
	TestEqual("Correct state found", FoundNodes[1], Cast<USMNodeInstance>(NextState));
	TestFalse("Initial state not set", NextState->IsEntryState());
	
	// Verify state machine instance methods to retrieve node instances are correct.
	TArray<USMStateInstance_Base*> StateInstances;
	StateMachineInstance->GetAllStateInstances(StateInstances);
	TestEqual("All states found", StateInstances.Num(), StateMachineInstance->GetStateMap().Num());
	for (USMStateInstance_Base* StateInstance : StateInstances)
	{
		USMStateInstance_Base* FoundStateInstance = StateMachineInstance->GetStateInstanceByGuid(StateInstance->GetGuid());
		TestEqual("State instance retrieved from sm instance", FoundStateInstance, StateInstance);
		if (StateInstance->GetOwningNode() != InitialState)
		{
			TestFalse("Initial state not set", NextState->IsEntryState());
		}
	}
	
	TArray<USMTransitionInstance*> TransitionInstances;
	StateMachineInstance->GetAllTransitionInstances(TransitionInstances);
	TestEqual("All transitions found", TransitionInstances.Num(), StateMachineInstance->GetTransitionMap().Num());
	for (USMTransitionInstance* TransitionInstance : TransitionInstances)
	{
		USMTransitionInstance* FoundTransitionInstance = StateMachineInstance->GetTransitionInstanceByGuid(TransitionInstance->GetGuid());
		TestEqual("Transition instance retrieved from sm instance", FoundTransitionInstance, TransitionInstance);
	}
	
	// Test transition instance.
	USMTransitionInstance* NextTransition = CastChecked<USMTransitionInstance>(InitialState->GetOutgoingTransitions()[0]->GetNodeInstance());
	{
		TArray<USMTransitionInstance*> Transitions;
		NodeInstance->GetOutgoingTransitions(Transitions);

		TestEqual("One outgoing transition", Transitions.Num(), 1);
		USMTransitionInstance* TransitionInstance = Transitions[0];

		TestEqual("Transition instance correct", TransitionInstance, NextTransition);
		
		FSMTransitionInfo TransitionInfo;
		TransitionInstance->GetTransitionInfo(TransitionInfo);

		TestEqual("Transition info instance correct", TransitionInfo.NodeInstance.Get(), Cast<USMNodeInstance>(NextTransition));

		TestEqual("Prev state correct", TransitionInstance->GetPreviousStateInstance(), NodeInstance);
		TestEqual("Next state correct", TransitionInstance->GetNextStateInstance(), NextState);

		TestNull("Source state correct", TransitionInstance->GetSourceStateForActiveTransition());
		TestNull("Dest state correct", TransitionInstance->GetDestinationStateForActiveTransition());
	}

	{
		bool bResult = NodeInstance->SwitchToLinkedState(NextState);
		TestTrue("Transition taken", bResult);
	}
	
	TestFalse("State no longer active", NodeInstance->IsActive());
	TestTrue("Node has updated from bAlwaysUpdate", NodeInstance->HasUpdated());
	TestEqual("Transition to take set", NodeInstance->GetTransitionToTake(), NextTransition);

	{
		// Source/Dest states should have updated after taking the transition.
		
		USMTransitionInstance* PreviousTransition = NextTransition;
	
		TestEqual("Source state correct", PreviousTransition->GetSourceStateForActiveTransition(), NodeInstance);
		TestEqual("Dest state correct", PreviousTransition->GetDestinationStateForActiveTransition(), NextState);
	}
	
	USMTransitionInstance* PreviousTransition = CastChecked<USMTransitionInstance>((NextState->GetOwningNodeAs<FSMState_Base>())->GetIncomingTransitions()[0]->GetNodeInstance());
	{
		TestEqual("Previous transition is correct instance", PreviousTransition, NextTransition);
		
		TArray<USMTransitionInstance*> Transitions;
		NextState->GetIncomingTransitions(Transitions);

		TestEqual("One incoming transition", Transitions.Num(), 1);
		USMTransitionInstance* TransitionInstance = Transitions[0];

		TestEqual("Transition instance correct", TransitionInstance, PreviousTransition);

		FSMTransitionInfo TransitionInfo;
		TransitionInstance->GetTransitionInfo(TransitionInfo);

		TestEqual("Transition info instance correct", TransitionInfo.NodeInstance.Get(), Cast<USMNodeInstance>(PreviousTransition));

		TestEqual("Prev state correct", TransitionInstance->GetPreviousStateInstance(), NodeInstance);
		TestEqual("Next state correct", TransitionInstance->GetNextStateInstance(), NextState);
	}

	NodeInstance = CastChecked<USMStateInstance_Base>(StateMachineInstance->GetSingleActiveState()->GetNodeInstance());
	TestTrue("Is end state", NodeInstance->IsEndState());

	// Test State Name Retrieval per FSM.
	{
		StateMachineInstance->Stop();
		StateMachineInstance->Start();

		InitialState = StateMachineInstance->GetRootStateMachine().GetSingleInitialState();
		NodeInstance = CastChecked<USMStateInstance_Base>(InitialState->GetNodeInstance());
		
		bool bResult = NodeInstance->SwitchToLinkedStateByName(NextState->GetNodeName());
		TestTrue("Transition taken", bResult);
		
		TestFalse("State no longer active", NodeInstance->IsActive());
		TestEqual("State switched by name", StateMachineInstance->GetSingleActiveStateInstance(), NextState);
		TestTrue("Node has updated from bAlwaysUpdate", NodeInstance->HasUpdated());
		TestEqual("Transition to take set", NodeInstance->GetTransitionToTake(), NextTransition);

		TestTrue("Next state activated", NodeInstance->GetTransitionByIndex(0)->GetNextStateInstance()->IsActive());
		StateMachineInstance->Update();
		TestTrue("Next state active", NodeInstance->GetTransitionByIndex(0)->GetNextStateInstance()->IsActive());
	}

	// Test Switch to linked state by transition.
	{
		StateMachineInstance->Stop();
		StateMachineInstance->Start();

		InitialState = StateMachineInstance->GetRootStateMachine().GetSingleInitialState();
		NodeInstance = CastChecked<USMStateInstance_Base>(InitialState->GetNodeInstance());

		USMTransitionInstance* TransitionToUse = NodeInstance->GetTransitionByIndex(0);
		bool bResult = NodeInstance->SwitchToLinkedStateByTransition(TransitionToUse);
		TestTrue("Transition taken", bResult);

		USMStateInstance_Base* NextStateInstance = TransitionToUse->GetNextStateInstance();
		
		TestFalse("State no longer active", NodeInstance->IsActive());
		TestEqual("State switched by name", StateMachineInstance->GetSingleActiveStateInstance(), NextState);
		TestTrue("Node has updated from bAlwaysUpdate", NodeInstance->HasUpdated());
		TestEqual("Transition to take set", NodeInstance->GetTransitionToTake(), NextTransition);

		TestTrue("Next state activated", NextStateInstance->IsActive());
		StateMachineInstance->Update();
		TestTrue("Next state active", NextStateInstance->IsActive());

		AddExpectedError("Attempted to switch to linked state by transition", EAutomationExpectedErrorFlags::Contains, 1);
		bResult = NextStateInstance->SwitchToLinkedStateByTransition(TransitionToUse);
		TestFalse("Did not switch to a state from a transition with a different 'FromState'", bResult);
		TestTrue("Next state active", NextStateInstance->IsActive());
		TestFalse("Previous state still not active", NodeInstance->IsActive());
	}
	
	//  Test nested reference FSM can retrieve transitions.
	{
		LastStatePin = nullptr;
		FSMBlueprintEditorUtils::RemoveAllNodesFromGraph(StateMachineGraph, NewBP);
		TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin);

		USMGraphNode_StateMachineStateNode* NestedFSM = FSMBlueprintEditorUtils::ConvertNodeTo<USMGraphNode_StateMachineStateNode>(CastChecked<USMGraphNode_StateNodeBase>(StateMachineGraph->EntryNode->GetOutputNode()));
		FKismetEditorUtilities::CompileBlueprint(NewBP);

		USMBlueprint* NewReferencedBlueprint = FSMBlueprintEditorUtils::ConvertStateMachineToReference(NestedFSM, false, nullptr, nullptr);

		Context = NewObject<USMTestContext>();
		StateMachineInstance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);
		USMStateMachineInstance* FSMClass = CastChecked< USMStateMachineInstance>(StateMachineInstance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());

		TArray<USMTransitionInstance*> Transitions;
		FSMClass->GetOutgoingTransitions(Transitions);
		TestEqual("Outgoing transitions found of reference FSM", Transitions.Num(), 1);
	}
	
	return true;
}

/**
 * Test nested state machines with a state machine class set evaluate graphs properly.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNodeInstanceStateMachineClassTest, "LogicDriver.NodeInstance.StateMachineClass", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FNodeInstanceStateMachineClassTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(2)

	UEdGraphPin* LastStatePin = nullptr;

	// Build state machine.
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMStateTestInstance::StaticClass(), USMTransitionTestInstance::StaticClass());

	USMGraphNode_StateMachineStateNode* NestedFSMNode = FSMBlueprintEditorUtils::ConvertNodeTo<USMGraphNode_StateMachineStateNode>(CastChecked<USMGraphNode_Base>(StateMachineGraph->GetEntryNode()->GetOutputNode()));
	USMGraphNode_StateMachineStateNode* NestedFSMNode2 = FSMBlueprintEditorUtils::ConvertNodeTo<USMGraphNode_StateMachineStateNode>(NestedFSMNode->GetNextState());

	TestHelpers::SetNodeClass(this, NestedFSMNode, USMStateMachineTestInstance::StaticClass());
	TestHelpers::SetNodeClass(this, NestedFSMNode2, USMStateMachineTestInstance::StaticClass());

	/*
	 * This part tests evaluating exposed variable blueprint graphs. There was a bug
	 * when more than one FSM was present that the graphs wouldn't evaluate properly, but default values would.
	 */
	
	// Create and wire a new variable to the first fsm.
	const int32 TestVarDefaultValue = 2;
	{
		FName VarName = "NewVar";
		FEdGraphPinType VarType;
		VarType.PinCategory = UEdGraphSchema_K2::PC_Int;

		// Create new variable.
		FBlueprintEditorUtils::AddMemberVariable(NewBP, VarName, VarType, FString::FromInt(TestVarDefaultValue));
		FProperty* NewProperty = FSMBlueprintEditorUtils::GetPropertyByName(NewBP, VarName);
		
		auto PropertyNode = NestedFSMNode->GetGraphPropertyNode("ExposedInt");

		// Place variable getter and wire to result node.
		TestTrue("Variable placed on graph", FSMBlueprintEditorUtils::PlacePropertyOnGraph(PropertyNode->GetGraph(), NewProperty, PropertyNode->GetResultPinChecked(), nullptr));
	}
	
	// Create and wire a second variable to the first fsm.
	const int32 TestVarDefaultValue2 = 4;
	{
		FName VarName = "NewVar2";
		FEdGraphPinType VarType;
		VarType.PinCategory = UEdGraphSchema_K2::PC_Int;

		// Create new variable.
		FBlueprintEditorUtils::AddMemberVariable(NewBP, VarName, VarType, FString::FromInt(TestVarDefaultValue2));
		FProperty* NewProperty = FSMBlueprintEditorUtils::GetPropertyByName(NewBP, VarName);

		auto PropertyNode = NestedFSMNode2->GetGraphPropertyNode("ExposedInt");

		// Place variable getter and wire to result node.
		TestTrue("Variable placed on graph", FSMBlueprintEditorUtils::PlacePropertyOnGraph(PropertyNode->GetGraph(), NewProperty, PropertyNode->GetResultPinChecked(), nullptr));
	}

	// Set the root class as well.
	CastChecked<USMInstance>(NewBP->GeneratedClass->GetDefaultObject())->SetRootStateMachineNodeClass(USMStateMachineTestInstance::StaticClass());
	
	FKismetEditorUtilities::CompileBlueprint(NewBP);
	
	USMTestContext* Context = NewObject<USMTestContext>();
	USMInstance* StateMachineInstance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);

	FSMState_Base* InitialState = StateMachineInstance->GetRootStateMachine().GetSingleInitialState();
	USMStateMachineTestInstance* NodeInstance = CastChecked<USMStateMachineTestInstance>(InitialState->GetNodeInstance());
	InitialState->bAlwaysUpdate = true; // Needed since we are manually switching states later.

	TestEqual("Correct state machine", NodeInstance->GetStateMachineInstance(), StateMachineInstance);
	TestEqual("Guids correct", NodeInstance->GetGuid(), InitialState->GetGuid());
	TestEqual("Name correct", NodeInstance->GetNodeName(), InitialState->GetNodeName());
	
	TestFalse("Initial state not active", NodeInstance->IsActive());
	
	TestEqual("Exposed var not set", NodeInstance->ExposedInt, 0);
	TestEqual("Root SM start not hit", NodeInstance->RootSMStartHit.Count, 0);
	TestEqual("Root SM end not hit", NodeInstance->RootSMStopHit.Count, 0);
	StateMachineInstance->Start();
	TestEqual("Root SM start hit", NodeInstance->RootSMStartHit.Count, 1);
	TestEqual("Root SM end not hit", NodeInstance->RootSMStopHit.Count, 0);
	TestEqual("Exposed var set", NodeInstance->ExposedInt, TestVarDefaultValue);
	
	TestTrue("Initial state active", NodeInstance->IsActive());
	InitialState->TimeInState = 3;
	TestEqual("Time correct", NodeInstance->GetTimeInState(), InitialState->TimeInState);
	
	FSMStateInfo StateInfo;
	NodeInstance->GetStateInfo(StateInfo);

	TestEqual("State info guids correct", StateInfo.Guid, InitialState->GetGuid());
	TestEqual("State info instance correct", StateInfo.NodeInstance.Get(), Cast<USMNodeInstance>(NodeInstance));
	TestTrue("Is a state machine", NodeInstance->IsStateMachine());
	TestFalse("Has not updated", NodeInstance->HasUpdated());
	TestNull("No transition to take", NodeInstance->GetTransitionToTake());

	USMStateMachineTestInstance* NextState = CastChecked<USMStateMachineTestInstance>(InitialState->GetOutgoingTransitions()[0]->GetToState()->GetNodeInstance());

	// Test transition instance.
	USMTransitionInstance* NextTransition = CastChecked<USMTransitionInstance>(InitialState->GetOutgoingTransitions()[0]->GetNodeInstance());
	{
		TArray<USMTransitionInstance*> Transitions;
		NodeInstance->GetOutgoingTransitions(Transitions);

		TestEqual("One outgoing transition", Transitions.Num(), 1);
		USMTransitionInstance* TransitionInstance = Transitions[0];

		TestEqual("Transition instance correct", TransitionInstance, NextTransition);

		FSMTransitionInfo TransitionInfo;
		TransitionInstance->GetTransitionInfo(TransitionInfo);

		TestEqual("Transition info instance correct", TransitionInfo.NodeInstance.Get(), Cast<USMNodeInstance>(NextTransition));

		TestEqual("Prev state correct", Cast<USMStateMachineTestInstance>(TransitionInstance->GetPreviousStateInstance()), NodeInstance);
		TestEqual("Next state correct", Cast<USMStateMachineTestInstance>(TransitionInstance->GetNextStateInstance()), NextState);
	}

	USMStateMachineTestInstance* RootSMInstance = CastChecked<USMStateMachineTestInstance>(StateMachineInstance->GetRootStateMachineNodeInstance());

	TestEqual("Root end state not reached", RootSMInstance->EndStateReachedHit.Count, 0);
	TestFalse("State machine is not in an end state", RootSMInstance->IsStateMachineInEndState());
	
	TestEqual("Exposed var not set", NextState->ExposedInt, 0);
	StateMachineInstance->Update(0.f);
	TestEqual("Exposed var set", NextState->ExposedInt, TestVarDefaultValue2);

	TestFalse("State no longer active", NodeInstance->IsActive());
	TestTrue("Node has updated from bAlwaysUpdate", NodeInstance->HasUpdated());
	TestEqual("Transition to take set", NodeInstance->GetTransitionToTake(), NextTransition);

	TestEqual("State begin hit", NodeInstance->StateBeginHit.Count, 1);
	TestEqual("State update not hit", NodeInstance->StateUpdateHit.Count, 1);
	TestEqual("State end not hit", NodeInstance->StateEndHit.Count, 1);
	
	NodeInstance = CastChecked<USMStateMachineTestInstance>(StateMachineInstance->GetSingleActiveState()->GetNodeInstance());
	TestTrue("Is end state", NodeInstance->IsEndState());
	TestTrue("State machine is in an end state", RootSMInstance->IsStateMachineInEndState());
	TestEqual("State machine not yet completed", NodeInstance->StateMachineCompletedHit.Count, 0);

	TestEqual("Root end state reached", RootSMInstance->EndStateReachedHit.Count, 1);
	TestEqual("Root state machine not yet completed", RootSMInstance->StateMachineCompletedHit.Count, 0);
	
	TestEqual("State begin hit", NodeInstance->StateBeginHit.Count, 1);
	TestEqual("State update not hit", NodeInstance->StateUpdateHit.Count, 0);
	TestEqual("State end not hit", NodeInstance->StateEndHit.Count, 0);

	TestEqual("Root SM start hit", NodeInstance->RootSMStartHit.Count, 1);
	TestEqual("Root SM end not hit", NodeInstance->RootSMStopHit.Count, 0);

	StateMachineInstance->Stop();

	TestEqual("State machine completed", NodeInstance->StateMachineCompletedHit.Count, 1);
	TestEqual("Root state machine completed", RootSMInstance->StateMachineCompletedHit.Count, 1);
	
	TestEqual("Root SM start hit", NodeInstance->RootSMStartHit.Count, 1);
	TestEqual("Root SM end not hit", NodeInstance->RootSMStopHit.Count, 1);
	
	return true;
}

/**
 * Test nested state machine references with a root state machine node class set.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNodeInstanceStateMachineClassReferenceTest, "LogicDriver.NodeInstance.StateMachineClassReference", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FNodeInstanceStateMachineClassReferenceTest::RunTest(const FString& Parameters)
{
	USMProjectEditorSettings* Settings = FSMBlueprintEditorUtils::GetMutableProjectEditorSettings();
	const auto CurrentCSSetting = Settings->EditorNodeConstructionScriptSetting;
	Settings->EditorNodeConstructionScriptSetting = ESMEditorConstructionScriptProjectSetting::SM_Standard;
	
	SETUP_NEW_STATE_MACHINE_FOR_TEST_NO_STATES()

	UEdGraphPin* LastStatePin = nullptr;

	const int32 NestedStateCount = 1;
	
	USMGraphNode_StateMachineStateNode* NestedFSMNode = TestHelpers::BuildNestedStateMachine(this, StateMachineGraph, NestedStateCount, &LastStatePin, nullptr);
	
	UEdGraphPin* FromPin = NestedFSMNode->GetOutputPin();
	USMGraphNode_StateMachineStateNode* NestedFSMNode2 = TestHelpers::BuildNestedStateMachine(this, StateMachineGraph, NestedStateCount, &FromPin, nullptr);

	TestHelpers::SetNodeClass(this, NestedFSMNode, USMStateMachineReferenceTestInstance::StaticClass());
	TestHelpers::SetNodeClass(this, NestedFSMNode2, USMStateMachineTestInstance::StaticClass());
	TestHelpers::SetNodeClass(this, NestedFSMNode->GetNextTransition(), USMTransitionTestInstance::StaticClass());
	
	// Now convert the state machine to a reference.
	USMBlueprint* NewReferencedBlueprint = FSMBlueprintEditorUtils::ConvertStateMachineToReference(NestedFSMNode, false, nullptr, nullptr);
	TestNotNull("New referenced blueprint created", NewReferencedBlueprint);
	TestHelpers::TestStateMachineConvertedToReference(this, NestedFSMNode);

	FKismetEditorUtilities::CompileBlueprint(NewReferencedBlueprint);

	// Store handler information so we can delete the object.
	FAssetHandler ReferencedAsset = TestHelpers::CreateAssetFromBlueprint(NewReferencedBlueprint);

	FKismetEditorUtilities::CompileBlueprint(NewBP);

	// Create and wire a new variable to the first fsm.
	const int32 TestVarDefaultValue = 2;
	{
		FName VarName = "NewVar";
		FEdGraphPinType VarType;
		VarType.PinCategory = UEdGraphSchema_K2::PC_Int;
		
		auto PropertyNode = NestedFSMNode->GetGraphPropertyNode(GET_MEMBER_NAME_CHECKED(USMStateMachineReferenceTestInstance, ExposedInt));
		
		const UEdGraphSchema_K2* Schema = CastChecked<UEdGraphSchema_K2>(PropertyNode->GetSchema());
		Schema->TrySetDefaultValue(*PropertyNode->GetResultPinChecked(), FString::FromInt(TestVarDefaultValue));
	}

	FKismetEditorUtilities::CompileBlueprint(NewBP);
	
	const FString ConstructionStringValue = "Test_" + FString::FromInt(TestVarDefaultValue);
	// Test exposed variables on nested reference FSM.
	{
		USMGraphK2Node_PropertyNode_Base* GraphPropertyReadNode = NestedFSMNode->GetGraphPropertyNode(
		GET_MEMBER_NAME_CHECKED(USMStateMachineReferenceTestInstance, SetByConstructionScript));
		check(GraphPropertyReadNode);

		UEdGraphPin* ResultPin = GraphPropertyReadNode->GetResultPinChecked();
		FString DefaultValue = ResultPin->GetDefaultAsString();
		TestEqual("Default value set by construction script", DefaultValue, ConstructionStringValue);

		USMStateMachineReferenceTestInstance* EditorNodeInstance = CastChecked<USMStateMachineReferenceTestInstance>(NestedFSMNode->GetNodeTemplate());
		TestEqual("Outgoing states found", EditorNodeInstance->CanReadNextStates, 1);
	}
	
	USMTestContext* Context = NewObject<USMTestContext>();
	USMInstance* StateMachineInstance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);

	// Locate the node instance of the reference.
	
	FSMStateMachine* InitialState = (FSMStateMachine*)StateMachineInstance->GetRootStateMachine().GetSingleInitialState();
	USMStateMachineReferenceTestInstance* NodeInstance = Cast<USMStateMachineReferenceTestInstance>(InitialState->GetNodeInstance());

	TestNotNull("Node instance from reference found", NodeInstance);

	if (!NodeInstance)
	{
		return false;
	}
	
	InitialState->bAlwaysUpdate = true; // Needed since we are manually switching states later.

	TestFalse("Initial state not active", NodeInstance->IsActive());

	TestEqual("Exposed var set to defaults", NodeInstance->ExposedInt, TestVarDefaultValue);
	TestEqual("Default value set by construction script", NodeInstance->SetByConstructionScript, ConstructionStringValue);
	TestEqual("Root SM start not hit", NodeInstance->RootSMStartHit.Count, 0);
	TestEqual("Root SM end not hit", NodeInstance->RootSMStopHit.Count, 0);
	TestEqual("Init not hit", NodeInstance->InitializeHit.Count, 0);
	TestEqual("Shutdown not hit", NodeInstance->ShutdownHit.Count, 0);
	StateMachineInstance->Start();
	TestEqual("Root SM start hit", NodeInstance->RootSMStartHit.Count, 1);
	TestEqual("Root SM end not hit", NodeInstance->RootSMStopHit.Count, 0);
	TestEqual("Init hit", NodeInstance->InitializeHit.Count, 1);
	TestEqual("Shutdown not hit", NodeInstance->ShutdownHit.Count, 0);
	TestEqual("Exposed var increased", NodeInstance->ExposedInt, TestVarDefaultValue + 1);
	
	TestTrue("Initial state active", NodeInstance->IsActive());

	FSMStateInfo StateInfo;
	NodeInstance->GetStateInfo(StateInfo);

	TestEqual("State info instance correct", StateInfo.NodeInstance.Get(), Cast<USMNodeInstance>(NodeInstance));
	TestTrue("Is a state machine", NodeInstance->IsStateMachine());
	TestFalse("Has not updated", NodeInstance->HasUpdated());
	TestNull("No transition to take", NodeInstance->GetTransitionToTake());

	USMStateMachineTestInstance* NextState = CastChecked<USMStateMachineTestInstance>(InitialState->GetOutgoingTransitions()[0]->GetToState()->GetNodeInstance());

	// Test transition instance.
	USMTransitionTestInstance* NextTransition = CastChecked<USMTransitionTestInstance>(InitialState->GetOutgoingTransitions()[0]->GetNodeInstance());
	{
		TArray<USMTransitionInstance*> Transitions;
		NodeInstance->GetOutgoingTransitions(Transitions);

		TestEqual("One outgoing transition", Transitions.Num(), 1);
		USMTransitionInstance* TransitionInstance = Transitions[0];

		TestEqual("Transition instance correct", Cast<USMTransitionTestInstance>(TransitionInstance), NextTransition);

		FSMTransitionInfo TransitionInfo;
		TransitionInstance->GetTransitionInfo(TransitionInfo);

		TestEqual("Transition info instance correct", TransitionInfo.NodeInstance.Get(), Cast<USMNodeInstance>(NextTransition));

		TestEqual("Prev state correct", Cast<USMStateMachineReferenceTestInstance>(TransitionInstance->GetPreviousStateInstance()), NodeInstance);
		TestEqual("Next state correct", Cast<USMStateMachineTestInstance>(TransitionInstance->GetNextStateInstance()), NextState);

		TestTrue("FSM Init hit before transition", NodeInstance->InitializeHit.TimeStamp > 0 && NodeInstance->InitializeHit.TimeStamp < NextTransition->TransitionInitializedHit.TimeStamp);
	}

	NextTransition->bCanTransition = true;
	StateMachineInstance->Update(0.f);

	TestFalse("State no longer active", NodeInstance->IsActive());
	TestTrue("Node has updated from bAlwaysUpdate", NodeInstance->HasUpdated());
	TestEqual("Transition to take set", Cast<USMTransitionTestInstance>(NodeInstance->GetTransitionToTake()), NextTransition);

	TestEqual("State begin hit", NodeInstance->StateBeginHit.Count, 1);
	TestEqual("State update not hit", NodeInstance->StateUpdateHit.Count, 1);
	TestEqual("State end not hit", NodeInstance->StateEndHit.Count, 1);

	TestEqual("Init hit", NodeInstance->InitializeHit.Count, 1);
	TestEqual("Shutdown hit", NodeInstance->ShutdownHit.Count, 1);
	
	// Second node instance test (Normal fsm)
	{
		USMStateMachineTestInstance* SecondNodeInstance = CastChecked<USMStateMachineTestInstance>(StateMachineInstance->GetSingleActiveState()->GetNodeInstance());
		TestTrue("Is end state", SecondNodeInstance->IsEndState());

		TestEqual("State begin hit", SecondNodeInstance->StateBeginHit.Count, 1);
		TestEqual("State update not hit", SecondNodeInstance->StateUpdateHit.Count, 0);
		TestEqual("State end not hit", SecondNodeInstance->StateEndHit.Count, 0);

		TestEqual("Root SM start hit", SecondNodeInstance->RootSMStartHit.Count, 1);
		TestEqual("Root SM end not hit", SecondNodeInstance->RootSMStopHit.Count, 0);

		StateMachineInstance->Stop();

		TestEqual("Root SM start hit", SecondNodeInstance->RootSMStartHit.Count, 1);
		TestEqual("Root SM end not hit", SecondNodeInstance->RootSMStopHit.Count, 1);
	}

	// Check first state reference fsm.
	TestEqual("Root SM start hit", NodeInstance->RootSMStartHit.Count, 1);
	TestEqual("Root SM end not hit", NodeInstance->RootSMStopHit.Count, 1);

	Settings->EditorNodeConstructionScriptSetting = CurrentCSSetting;
	
	ReferencedAsset.DeleteAsset(this);
	return true;
}

/**
 * Test that node coordinates are available at run-time.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNodeInstanceGetNodePositionTest, "LogicDriver.NodeInstance.GetNodePosition", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FNodeInstanceGetNodePositionTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(2)

	UEdGraphPin* LastStatePin = nullptr;

	// Build single state - state machine.
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMStateTestInstance::StaticClass(), USMTransitionTestInstance::StaticClass());
	if (!NewAsset.SaveAsset(this))
	{
		return false;
	}

	USMGraphNode_StateNode* LastState = CastChecked<USMGraphNode_StateNode>(LastStatePin->GetOwningNode());
	LastState->NodePosX = 512;
	LastState->NodePosY = 1024;
	FKismetEditorUtilities::CompileBlueprint(NewBP);

	TArray<USMGraphNode_TransitionEdge*> TransitionsIn;
	LastState->GetInputTransitions(TransitionsIn);
	check(TransitionsIn.Num() == 1);

	{
		// Hacky because the transition should be set from the state which gets updated from slate,
		// but in the test we're just working the UEdNode.
		TransitionsIn[0]->NodePosX = 128;
		TransitionsIn[0]->NodePosY = 256;
	}
	const FVector2D StatePositionTest(LastState->NodePosX, LastState->NodePosY);
	const FVector2D TransitionPositionTest(TransitionsIn[0]->NodePosX, TransitionsIn[0]->NodePosY);
	
	USMInstance* TestInstance = TestHelpers::TestLinearStateMachine(this, NewBP, TotalStates, false);

	USMStateInstance_Base* LastStateInstance = TestInstance->GetSingleActiveStateInstance();
	check(LastStateInstance);

	TArray<USMTransitionInstance*> TransitionInstances;
	LastStateInstance->GetIncomingTransitions(TransitionInstances, false);

	check(TransitionInstances.Num() == 1);
	
	TestEqual("State node position saved in run-time", LastStateInstance->GetNodePosition(), StatePositionTest);
	TestEqual("Transition node position saved in run-time", TransitionInstances[0]->GetNodePosition(), TransitionPositionTest);
	
	return NewAsset.DeleteAsset(this);
}

/**
 * Reset node variables back to their defaults.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNodeInstanceResetVariablesTest, "LogicDriver.NodeInstance.Variables.ResetVariables", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FNodeInstanceResetVariablesTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(1)

	UEdGraphPin* LastStatePin = nullptr;

	// Build single state - state machine.
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMStatePropertyResetTestInstance::StaticClass(), USMTransitionInstance::StaticClass());

	USMGraphNode_StateNode* LastState = CastChecked<USMGraphNode_StateNode>(LastStatePin->GetOwningNode());

	// Build state stack.
	{
		const FStateStackContainer NewStateStackText(USMStateAuxiliaryPropertyResetTestInstance::StaticClass());
		LastState->StateStack.Add(NewStateStackText);
		LastState->InitStateStack();
		LastState->CreateGraphPropertyGraphs();
	}
	
	USMStatePropertyResetTestInstance* StateInstanceTemplate = CastChecked<USMStatePropertyResetTestInstance>(LastState->GetNodeTemplate());
	USMStateAuxiliaryPropertyResetTestInstance* StateStackInstanceTemplate = CastChecked<USMStateAuxiliaryPropertyResetTestInstance>(LastState->StateStack[0].NodeStackInstanceTemplate);
	
	// Set default values
	{
		StateInstanceTemplate->IntVar = 5;
		StateInstanceTemplate->StringVar = "TestString";
		StateInstanceTemplate->ObjectValue = NewObject<UObject>(GetTransientPackage(), USMTestObject::StaticClass(), TEXT("TestObjectName"));

		StateInstanceTemplate->StringVarArray = {"A", "B", "C"};
		// Exposed array set in ctor so property graphs are created properly.
		//StateInstanceTemplate->StringVarArrayExposed = {"A_Exposed", "B_Exposed", "C_Exposed"};
		StateInstanceTemplate->StringVarSet = {"SetA", "SetB", "SetC"};
		StateInstanceTemplate->StringVarMap = {{"A", "1"}, {"B", "2"}};

		StateStackInstanceTemplate->bBoolVar = true;
		StateStackInstanceTemplate->TextVar = NSLOCTEXT("LDTestNodeInstanceNS", "LDTestExperimentalKey", "Our default text");
		StateStackInstanceTemplate->StringVarArray = {"A1", "B1", "C1"};
	}
	
	FKismetEditorUtilities::CompileBlueprint(NewBP);
	
	const USMInstance* OwningStateMachineInstance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, NewObject<USMTestContext>());
	TArray<USMStateInstance_Base*> EntryStateInstances;
	OwningStateMachineInstance->GetRootStateMachineNodeInstance()->GetEntryStates(EntryStateInstances);
	check(EntryStateInstances.Num() == 1);

	USMStatePropertyResetTestInstance* StateInstance = CastChecked<USMStatePropertyResetTestInstance>(EntryStateInstances[0]);
	USMStateAuxiliaryPropertyResetTestInstance* StateStackInstance = CastChecked<USMStateAuxiliaryPropertyResetTestInstance>(StateInstance->GetStateInStack(0));
		
	// Set instance values
	{
		StateInstance->IntVar = 6;
		StateInstance->StringVar = "Adjusted";
		StateInstance->ObjectValue = NewObject<UObject>(GetTransientPackage(), USMTestObject::StaticClass(), TEXT("AdjustedName"));

		StateInstance->StringVarArray = {"A_Adjusted", "B_Adjusted", "C_Adjusted", "D_Adjusted"};
		StateInstance->StringVarArrayExposed = {"A_Exposed_Adjusted", "B_Exposed_Adjusted", "C_Exposed_Adjusted", "D_Exposed_Adjusted"};
		StateInstance->StringVarSet = {"SetA_Adjusted", "SetB_Adjusted", "SetC_Adjusted, SetD_Adjusted"};
		StateInstance->StringVarMap = {{"A_Adjusted", "1_Adjusted"}, {"B_Adjusted", "2_Adjusted"}, {"C_Adjusted", "3_Adjusted"}};
		
		StateStackInstance->bBoolVar = false;
		StateStackInstance->TextVar = NSLOCTEXT("LDTestNodeInstanceNS", "LDTestExperimentalKey2", "Our adjusted text");
		StateStackInstance->StringVarArray = {"A1_Adjusted", "B1_Adjusted", "C1_Adjusted"};
	}

	TestNotEqual("Values changed", StateInstance->IntVar, StateInstanceTemplate->IntVar);
	TestNotEqual("Values changed", StateInstance->StringVar, StateInstanceTemplate->StringVar);
	TestNotEqual("Values changed", StateInstance->ObjectValue, StateInstanceTemplate->ObjectValue);

	TestFalse("Containers changed", TestHelpers::AreContainersIdentical(StateInstance->StringVarArray, StateInstanceTemplate->StringVarArray));
	TestFalse("Containers changed", TestHelpers::AreContainersIdentical(StateInstance->StringVarArrayExposed, StateInstanceTemplate->StringVarArrayExposed));
	TestFalse("Containers changed", TestHelpers::AreContainersIdentical(StateInstance->StringVarSet, StateInstanceTemplate->StringVarSet));
	TestFalse("Containers changed", TestHelpers::AreContainersIdentical(StateInstance->StringVarMap, StateInstanceTemplate->StringVarMap));
	
	TestNotEqual("Values changed", StateStackInstance->bBoolVar, StateStackInstanceTemplate->bBoolVar);
	TestNotEqual("Values changed", StateStackInstance->TextVar.ToString(), StateStackInstanceTemplate->TextVar.ToString());
	TestFalse("Containers changed", TestHelpers::AreContainersIdentical(StateStackInstance->StringVarArray, StateStackInstanceTemplate->StringVarArray));
	
	StateInstance->ResetVariables();
	StateStackInstance->ResetVariables();

	TestEqual("Values reset", StateInstance->IntVar, StateInstanceTemplate->IntVar);
	TestEqual("Values reset", StateInstance->StringVar, StateInstanceTemplate->StringVar);
	TestEqual("Values reset", StateInstance->ObjectValue, StateInstanceTemplate->ObjectValue);

	TestTrue("Containers reset", TestHelpers::AreContainersIdentical(StateInstance->StringVarArray, StateInstanceTemplate->StringVarArray));
	TestTrue("Containers reset", TestHelpers::AreContainersIdentical(StateInstance->StringVarArrayExposed, StateInstanceTemplate->StringVarArrayExposed));
	TestTrue("Containers reset", TestHelpers::AreContainersIdentical(StateInstance->StringVarSet, StateInstanceTemplate->StringVarSet));
	TestTrue("Containers reset", TestHelpers::AreContainersIdentical(StateInstance->StringVarMap, StateInstanceTemplate->StringVarMap));
	
	TestEqual("Values reset", StateStackInstance->bBoolVar, StateStackInstanceTemplate->bBoolVar);
	TestEqual("Values reset", StateStackInstance->TextVar.ToString(), StateStackInstanceTemplate->TextVar.ToString());
	TestTrue("Containers reset", TestHelpers::AreContainersIdentical(StateStackInstance->StringVarArray, StateStackInstanceTemplate->StringVarArray));
	
	return true;
}

/**
 * Check behavior and optimizations around default node classes and loading them on demand.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNodeInstanceOnDemandTest, "LogicDriver.NodeInstance.OnDemand", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FNodeInstanceOnDemandTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(4)

	UEdGraphPin* LastStatePin = nullptr;

	// Build single state - state machine.
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMStateInstance::StaticClass(), USMTransitionInstance::StaticClass());

	UClass* TestNodeClass = USMStateTestInstance::StaticClass();
	USMGraphNode_StateNode* LastState = CastChecked<USMGraphNode_StateNode>(LastStatePin->GetOwningNode());
	TestHelpers::SetNodeClass(this, LastState, TestNodeClass);

	FKismetEditorUtilities::CompileBlueprint(NewBP);

	GetMutableDefault<USMRuntimeSettings>()->bPreloadDefaultNodes = true;
	{
		USMInstance* Instance = USMBlueprintUtils::CreateStateMachineInstance(NewBP->GetGeneratedClass(), NewObject<USMTestContext>());

		for (const auto& KeyVal : Instance->GetNodeMap())
		{
			USMNodeInstance* NodeInstance = KeyVal.Value->GetNodeInstance();
			TestNotNull("Node instance created",  NodeInstance);
		}
	}

	auto TestInstance = [this, TestNodeClass](const USMInstance* Instance, bool bExpectAllValid = false)
	{
		bool bFound = false;
		for (const auto& KeyVal : Instance->GetNodeMap())
		{
			USMNodeInstance* NodeInstance = KeyVal.Value->GetNodeInstance();
			if (NodeInstance || bExpectAllValid)
			{
				if (bExpectAllValid)
				{
					TestNotNull("Instance valid", NodeInstance);
					continue;
				}
				TestFalse("Only 1 node instance exists", bFound);
				TestEqual("Node instance created during initialization", NodeInstance->GetClass(), TestNodeClass);
				bFound = true;
			}
			else
			{
				TestNull("Node instance not created.", KeyVal.Value->GetNodeInstance());
				NodeInstance = KeyVal.Value->GetOrCreateNodeInstance();
				TestNotNull("Node instance created",  NodeInstance);
			}
		}	
	};
	
	GetMutableDefault<USMRuntimeSettings>()->bPreloadDefaultNodes = false;
	{
		USMInstance* Instance = USMBlueprintUtils::CreateStateMachineInstance(NewBP->GetGeneratedClass(), NewObject<USMTestContext>());
		TestInstance(Instance);
	}

	// Test running the state machine and verifying nodes are not created by default.
	{
		USMInstance* Instance = USMBlueprintUtils::CreateStateMachineInstance(NewBP->GetGeneratedClass(), NewObject<USMTestContext>());
		TestHelpers::RunAllStateMachinesToCompletion(this, Instance);
		TestInstance(Instance);

		TArray<USMStateInstance_Base*> States;
		TArray<USMTransitionInstance*> Transitions;
		Instance->GetAllStateInstances(States);
		Instance->GetAllTransitionInstances(Transitions);
		TestInstance(Instance, true);
	}

	// Test preload all nodes.
	{
		USMInstance* Instance = USMBlueprintUtils::CreateStateMachineInstance(NewBP->GetGeneratedClass(), NewObject<USMTestContext>());
		Instance->PreloadAllNodeInstances();
		TestInstance(Instance, true);
	}
	
	return NewAsset.DeleteAsset(this);
}

/**
 * Verify modifying CDO property override values propagates to instances correctly.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNodeInstancePropertyOverridePropagationTest, "LogicDriver.NodeInstance.PropertyOverridePropagation", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FNodeInstancePropertyOverridePropagationTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(1)

	UEdGraphPin* LastStatePin = nullptr;

	// Build single state - state machine.
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMStateTestInstance::StaticClass(), USMTransitionInstance::StaticClass());
	FKismetEditorUtilities::CompileBlueprint(NewBP);
	
	const USMGraphNode_StateNode* StateNode = CastChecked<USMGraphNode_StateNode>(LastStatePin->GetOwningNode());
	const USMStateTestInstance* StateInstance = CastChecked<USMStateTestInstance>(StateNode->GetNodeTemplate());
	
	USMStateTestInstance* CDO = CastChecked<USMStateTestInstance>(USMStateTestInstance::StaticClass()->GetDefaultObject());
	TestEqual("No overrides set", CDO->ExposedPropertyOverrides.Num(), 0);
	TestEqual("No overrides set", StateInstance->ExposedPropertyOverrides.Num(), 0);

	const FName VariableName = "TestName";

	TSharedPtr<ISinglePropertyView> PropView;
	const TSharedPtr<IPropertyHandle> PropertyAdded = FSMNodeInstanceUtils::FindOrAddExposedPropertyOverrideByName(CDO, VariableName, PropView);

	TestTrue("Property added", PropertyAdded.IsValid());

	{
		TestEqual("Overrides set on CDO", CDO->ExposedPropertyOverrides.Num(), 1);
		TestEqual("Overrides set on Instance", StateInstance->ExposedPropertyOverrides.Num(), 1);

		TestEqual("Override name set on CDO", CDO->ExposedPropertyOverrides[0].VariableName, VariableName);
		TestEqual("Overrides name set on instance", StateInstance->ExposedPropertyOverrides[0].VariableName, VariableName);
	}
	
	const FName NewVariableName = "UpdatedName";
	const bool Renamed = FSMNodeInstanceUtils::UpdateExposedPropertyOverrideName(CDO, VariableName, NewVariableName);
	TestTrue("Exposed property renamed", Renamed);

	{
		TestEqual("Overrides set on CDO", CDO->ExposedPropertyOverrides.Num(), 1);
		TestEqual("Overrides set on Instance", StateInstance->ExposedPropertyOverrides.Num(), 1);

		TestEqual("Override name set on CDO", CDO->ExposedPropertyOverrides[0].VariableName, NewVariableName);
		TestEqual("Overrides name set on instance", StateInstance->ExposedPropertyOverrides[0].VariableName, NewVariableName);
	}

	const int32 RemovedCount = FSMNodeInstanceUtils::RemoveExposedPropertyOverrideByName(CDO, NewVariableName);
	TestEqual("Exposed property removed", RemovedCount, 1);
	
	{
		TestEqual("Overrides set on CDO", CDO->ExposedPropertyOverrides.Num(), 0);
		TestEqual("Overrides set on Instance", StateInstance->ExposedPropertyOverrides.Num(), 0);
	}
	
	return NewAsset.DeleteAsset(this);
}

/**
 * Verify metadata returned from nodes.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNodeInstanceVariableMetaDataTest, "LogicDriver.NodeInstance.Variables.MetaData", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FNodeInstanceVariableMetaDataTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(1)

	UEdGraphPin* LastStatePin = nullptr;

	// Build single state - state machine.
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMMetaDataState::StaticClass(), USMTransitionInstance::StaticClass());
	FKismetEditorUtilities::CompileBlueprint(NewBP);
	
	const USMGraphNode_StateNode* StateNode = CastChecked<USMGraphNode_StateNode>(LastStatePin->GetOwningNode());
	USMGraphK2Node_PropertyNode_Base* PropertyNode = StateNode->GetGraphPropertyNode(GET_MEMBER_NAME_CHECKED(USMMetaDataState, Tag));
	check(PropertyNode);

	const FString MetaData = PropertyNode->GetPinMetaData(GET_MEMBER_NAME_CHECKED(USMMetaDataState, Tag), FName("Categories"));

	TestEqual("Meta data extracted from property node", MetaData, TEXT("Quests"));
	
	return true;
}

#endif

#endif //WITH_DEV_AUTOMATION_TESTS