// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Helpers/SMTestBoilerplate.h"
#include "SMTestContext.h"
#include "SMTestHelpers.h"

#include "Blueprints/SMBlueprint.h"

#include "Blueprints/SMBlueprintFactory.h"
#include "Graph/Nodes/PropertyNodes/SMGraphK2Node_PropertyNode_Base.h"
#include "Graph/Nodes/SMGraphK2Node_StateMachineNode.h"
#include "Graph/Nodes/SMGraphNode_StateNode.h"
#include "Graph/SMGraph.h"
#include "ISMEditorGraphPropertyNodeInterface.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "Kismet2/KismetEditorUtilities.h"

#if WITH_DEV_AUTOMATION_TESTS

#if PLATFORM_DESKTOP

/**
 * Verify interface to editor node works correctly.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEditorInterfaceAccessTest, "LogicDriver.EditorInterface.AccessNode", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FEditorInterfaceAccessTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(1)

	UEdGraphPin* LastStatePin = nullptr;

	// Build single state - state machine.
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMStateTestInstance::StaticClass(), USMTransitionInstance::StaticClass());

	USMGraphNode_StateNode* StateNode = CastChecked<USMGraphNode_StateNode>(LastStatePin->GetOwningNode());

	const FStateStackContainer NewStateStackText(USMTextGraphStateExtra::StaticClass());
	StateNode->StateStack.Add(NewStateStackText); // TODO simplify state stack creation in tests
	StateNode->InitStateStack();
	StateNode->CreateGraphPropertyGraphs();

	const USMStateTestInstance* StateInstance = CastChecked<USMStateTestInstance>(StateNode->GetNodeTemplate());
	const USMTextGraphStateExtra* StackInstance = CastChecked<USMTextGraphStateExtra>(StateNode->StateStack[0].NodeStackInstanceTemplate);

	FKismetEditorUtilities::CompileBlueprint(NewBP);

	const TScriptInterface<ISMEditorGraphNodeInterface> EditorGraphNode = StateInstance->GetOwningEditorGraphNode();
	check(TestEqual("Editor graph node found", Cast<USMGraphNode_StateNode>(EditorGraphNode.GetObject()), StateNode));

	// Test base state only
	{
		{
			const TScriptInterface<ISMEditorGraphPropertyNodeInterface> GraphProperty = EditorGraphNode->GetEditorGraphProperty(
				GET_MEMBER_NAME_CHECKED(USMStateTestInstance, ExposedInt), StateInstance);

			TestNotNull("Graph property interface found", GraphProperty.GetObject());
		}

		{
			const TArray<TScriptInterface<ISMEditorGraphPropertyNodeInterface>> EditorProperties = EditorGraphNode->
				GetEditorGraphPropertyAsArray(
					GET_MEMBER_NAME_CHECKED(USMStateTestInstance, ExposedInt), StateInstance);

			TestEqual("Graph properties found", EditorProperties.Num(), 1);
		}

		{
			const TArray<TScriptInterface<ISMEditorGraphPropertyNodeInterface>> EditorProperties = EditorGraphNode->
				GetAllEditorGraphProperties(StateInstance);

			TestEqual("Graph properties found", EditorProperties.Num(), 1);
		}
	}

	// Test state stack
	{
		{
			const TScriptInterface<ISMEditorGraphPropertyNodeInterface> GraphProperty = EditorGraphNode->GetEditorGraphProperty(
				GET_MEMBER_NAME_CHECKED(USMTextGraphState, TextGraph), StackInstance);

			TestNotNull("Graph property interface found", GraphProperty.GetObject());
		}

		{
			const TArray<TScriptInterface<ISMEditorGraphPropertyNodeInterface>> EditorProperties = EditorGraphNode->
				GetEditorGraphPropertyAsArray(
					GET_MEMBER_NAME_CHECKED(USMTextGraphState, EvaluatedText), StackInstance);

			// This property is hidden
			TestEqual("Graph properties found", EditorProperties.Num(), 0);
		}

		{
			const TArray<TScriptInterface<ISMEditorGraphPropertyNodeInterface>> EditorProperties = EditorGraphNode->
				GetEditorGraphPropertyAsArray(
					GET_MEMBER_NAME_CHECKED(USMTextGraphStateExtra, StringVar), StackInstance);

			TestEqual("Graph properties found", EditorProperties.Num(), 1);
		}

		{
			const TArray<TScriptInterface<ISMEditorGraphPropertyNodeInterface>> EditorProperties = EditorGraphNode->
				GetAllEditorGraphProperties(StackInstance);

			TestEqual("Graph properties found", EditorProperties.Num(), 2);
		}

		{
			const TArray<TScriptInterface<ISMEditorGraphPropertyNodeInterface>> EditorProperties = EditorGraphNode->
				GetAllEditorGraphProperties(nullptr);

			TestEqual("Graph properties found", EditorProperties.Num(), 3);
		}
	}

	return NewAsset.DeleteAsset(this);
}

/**
 * Verify interface to editor node works correctly for base state nodes.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEditorStateBaseInterfaceAccessTest, "LogicDriver.EditorInterface.AccessStateBase", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FEditorStateBaseInterfaceAccessTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(2)

	UEdGraphPin* LastStatePin = nullptr;

	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMStateTestInstance::StaticClass(), USMTransitionInstance::StaticClass());

	USMGraphNode_StateNode* StateNode = CastChecked<USMGraphNode_StateNode>(LastStatePin->GetOwningNode());

	const USMStateTestInstance* StateInstance = CastChecked<USMStateTestInstance>(StateNode->GetNodeTemplate());
	const USMTransitionInstance* TransitionInstance = CastChecked<USMTransitionInstance>(StateNode->GetPreviousTransition(0)->GetNodeTemplate());

	FKismetEditorUtilities::CompileBlueprint(NewBP);

	const TScriptInterface<ISMEditorGraphNodeInterface> EditorGraphNode = StateInstance->GetOwningEditorGraphNode();
	check(TestEqual("Editor graph node found", Cast<USMGraphNode_StateNode>(EditorGraphNode.GetObject()), StateNode));

	const TScriptInterface<ISMEditorGraphNode_StateBaseInterface> StateBaseInterface = EditorGraphNode->
		AsStateBaseInterface();

	TestNotNull("State base interface valid for state node", StateBaseInterface.GetObject());
	TestNull("State base interface invalid for transition node", TransitionInstance->GetOwningEditorGraphNode()->AsStateBaseInterface().GetObject());

	return NewAsset.DeleteAsset(this);
}

/**
 * Verify ResetProperty properly resets the pin and property value.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEditorInterfaceResetPropertyTest, "LogicDriver.EditorInterface.ResetProperty", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FEditorInterfaceResetPropertyTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(1)

	UEdGraphPin* LastStatePin = nullptr;

	// Build single state - state machine.
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMStateEditorPropertyResetTestInstance::StaticClass(), USMTransitionInstance::StaticClass());
	FKismetEditorUtilities::CompileBlueprint(NewBP);

	USMGraphNode_StateNode* StateNode = CastChecked<USMGraphNode_StateNode>(LastStatePin->GetOwningNode());
	USMStateEditorPropertyResetTestInstance* StateInstance = CastChecked<USMStateEditorPropertyResetTestInstance>(StateNode->GetNodeTemplate());

	check(USMStateEditorPropertyResetTestInstance::DefaultIntValue != 0);
	TestEqual("Value currently set to default", StateInstance->IntVar, USMStateEditorPropertyResetTestInstance::DefaultIntValue);

	const TScriptInterface<ISMEditorGraphNodeInterface> EditorGraphNode = StateInstance->GetOwningEditorGraphNode();
	check(TestEqual("Editor graph node found", Cast<USMGraphNode_StateNode>(EditorGraphNode.GetObject()), StateNode));

	const TScriptInterface<ISMEditorGraphPropertyNodeInterface> GraphProperty = EditorGraphNode->GetEditorGraphProperty(
		GET_MEMBER_NAME_CHECKED(USMStateEditorPropertyResetTestInstance, IntVar), StateInstance);

	check(TestNotNull("Graph property interface found", GraphProperty.GetObject()));

	// Set a new instance value.
	constexpr int32 NewValue = 5;
	StateInstance->IntVar = NewValue;

	// Make sure the pin is correct.
	{
		GraphProperty->RefreshPropertyPinFromValue(); // Only necessary to call if changing the value in C++ outside of the construction script.
		TestEqual("Value changed", StateInstance->IntVar, NewValue);
	}

	const USMGraphK2Node_PropertyNode_Base* PropertyNode = CastChecked<USMGraphK2Node_PropertyNode_Base>(GraphProperty.GetObject());

	// Validate pin is updated
	{
		const FString DefaultPinValue = PropertyNode->GetResultPinChecked()->GetDefaultAsString();
		TestEqual("Pin value is set", DefaultPinValue, FString::FromInt(NewValue));
	}

	GraphProperty->ResetProperty();
	TestEqual("Value currently set to default", StateInstance->IntVar, USMStateEditorPropertyResetTestInstance::DefaultIntValue);

	// Validate pin is updated
	{
		const FString DefaultPinValue = PropertyNode->GetResultPinChecked()->GetDefaultAsString();
		TestEqual("Pin value is set", DefaultPinValue, FString::FromInt(USMStateEditorPropertyResetTestInstance::DefaultIntValue));
	}

	return NewAsset.DeleteAsset(this);
}

#endif

#endif //WITH_DEV_AUTOMATION_TESTS