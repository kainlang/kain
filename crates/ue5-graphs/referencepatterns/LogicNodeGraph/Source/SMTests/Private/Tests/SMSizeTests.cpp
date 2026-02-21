// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Helpers/SMTestBoilerplate.h"
#include "SMTestContext.h"
#include "SMTestHelpers.h"

#include "Blueprints/SMBlueprint.h"
#include "Blueprints/SMBlueprintGeneratedClass.h"
#include "SMConduit.h"

#include "Graph/Nodes/SMGraphK2Node_StateMachineNode.h"
#include "Graph/Nodes/SMGraphNode_ConduitNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineEntryNode.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/SMGraph.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "EdGraph/EdGraph.h"
#include "Kismet2/KismetEditorUtilities.h"

#if WITH_DEV_AUTOMATION_TESTS

#if PLATFORM_DESKTOP

#if PLATFORM_WINDOWS

#define SIZE_STATE_EXPECTED 408
#define SIZE_CONDUIT_EXPECTED 416
#define SIZE_TRANSITION_EXPECTED 416
#define SIZE_STATE_MACHINE_EXPECTED 736

#define SIZE_GRAPH_PROPERTY_EXPECTED 72
#define SIZE_TEXT_GRAPH_PROPERTY_EXPECTED 128

#else // Linux values

#define SIZE_STATE_EXPECTED 400
#define SIZE_CONDUIT_EXPECTED 408
#define SIZE_TRANSITION_EXPECTED 416
#define SIZE_STATE_MACHINE_EXPECTED 728

#define SIZE_TEXT_GRAPH_PROPERTY_EXPECTED 128
#define SIZE_GRAPH_PROPERTY_EXPECTED 72

#endif

static uint32 GetSizeFromStructProperty(const UBlueprintGeneratedClass* InGeneratedClass, const UScriptStruct* InStruct)
{
	for (TFieldIterator<FProperty> It(InGeneratedClass); It; ++It)
	{
		FProperty* RootProp = *It;

		if (const FStructProperty* RootStructProp = CastField<FStructProperty>(RootProp))
		{
			if (RootStructProp->Struct->IsChildOf(InStruct))
			{
				const FStructProperty* ChildStructProp = FindFProperty<FStructProperty>(InGeneratedClass, *RootStructProp->GetName());
				check(ChildStructProp);
				return ChildStructProp->GetSize();
			}
		}
	}

	return 0;
}

/**
 * Test struct sizes.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSizeStateTest, "LogicDriver.Size.State", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FSizeStateTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(1)

	UEdGraphPin* LastStatePin = nullptr;

	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin);
	FKismetEditorUtilities::CompileBlueprint(NewBP);

	const uint32 TotalSize = GetSizeFromStructProperty(NewBP->GetGeneratedClass(), FSMState::StaticStruct());
	TestEqual("State size correct", TotalSize, SIZE_STATE_EXPECTED);
	
	return NewAsset.DeleteAsset(this);
}

/**
 * Test struct sizes.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSizeConduitTest, "LogicDriver.Size.Conduit", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FSizeConduitTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(1)

	UEdGraphPin* LastStatePin = nullptr;

	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin);
	USMGraphNode_Base* FirstNode = CastChecked<USMGraphNode_Base>(StateMachineGraph->GetEntryNode()->GetOutputNode());
	
	USMGraphNode_ConduitNode* ConduitNode = FSMBlueprintEditorUtils::ConvertNodeTo<USMGraphNode_ConduitNode>(FirstNode);
	
	FKismetEditorUtilities::CompileBlueprint(NewBP);

	const uint32 TotalSize = GetSizeFromStructProperty(NewBP->GetGeneratedClass(), FSMConduit::StaticStruct());
	TestEqual("Conduit size correct", TotalSize, SIZE_CONDUIT_EXPECTED);
	
	return NewAsset.DeleteAsset(this);
}

/**
 * Test struct sizes.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSizeStateMachineTest, "LogicDriver.Size.StateMachine", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FSizeStateMachineTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(1)

	UEdGraphPin* LastStatePin = nullptr;
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin);
	FKismetEditorUtilities::CompileBlueprint(NewBP);

	const uint32 TotalSize = GetSizeFromStructProperty(NewBP->GetGeneratedClass(), FSMStateMachine::StaticStruct());
	TestEqual("State machine size correct", TotalSize, SIZE_STATE_MACHINE_EXPECTED);
	
	return NewAsset.DeleteAsset(this);
}

/**
 * Test struct sizes.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSizeTransitionTest, "LogicDriver.Size.Transition", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FSizeTransitionTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(2)

	UEdGraphPin* LastStatePin = nullptr;

	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin);
	FKismetEditorUtilities::CompileBlueprint(NewBP);

	const uint32 TotalSize = GetSizeFromStructProperty(NewBP->GetGeneratedClass(), FSMTransition::StaticStruct());
	TestEqual("Transition size correct", TotalSize, SIZE_TRANSITION_EXPECTED);
	
	return NewAsset.DeleteAsset(this);
}

/**
 * Test struct sizes.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSizeGraphPropertyTest, "LogicDriver.Size.GraphProperty", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FSizeGraphPropertyTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(1)

	UEdGraphPin* LastStatePin = nullptr;
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMStateTestInstance::StaticClass());
	FKismetEditorUtilities::CompileBlueprint(NewBP);

	const uint32 TotalSize = GetSizeFromStructProperty(NewBP->GetGeneratedClass(), FSMGraphProperty_Runtime::StaticStruct());
	TestEqual("Graph property size correct", TotalSize, SIZE_GRAPH_PROPERTY_EXPECTED);
	
	return NewAsset.DeleteAsset(this);
}

/**
 * Test text graph struct sizes.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSizeTextGraphPropertyTest, "LogicDriver.Size.TextGraphProperty", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FSizeTextGraphPropertyTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(1)

	UEdGraphPin* LastStatePin = nullptr;
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMTextGraphState::StaticClass());
	FKismetEditorUtilities::CompileBlueprint(NewBP);

	{
		const uint32 TotalSize = GetSizeFromStructProperty(NewBP->GetGeneratedClass(), FSMTextGraphProperty_Runtime::StaticStruct());
		TestEqual("Runtime Text Graph property size correct", TotalSize, SIZE_TEXT_GRAPH_PROPERTY_EXPECTED);
	}

	return NewAsset.DeleteAsset(this);
}

#endif

#endif //WITH_DEV_AUTOMATION_TESTS