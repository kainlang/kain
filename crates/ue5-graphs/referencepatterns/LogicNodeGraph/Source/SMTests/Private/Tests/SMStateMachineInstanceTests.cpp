// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMTestHelpers.h"
#include "SMTestContext.h"
#include "Helpers/SMTestBoilerplate.h"

#include "Blueprints/SMBlueprint.h"

#include "Utilities/SMBlueprintEditorUtils.h"
#include "Graph/SMGraph.h"
#include "Graph/Nodes/SMGraphK2Node_StateMachineNode.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/Nodes/SMGraphNode_StateMachineStateNode.h"

#include "Kismet2/KismetEditorUtilities.h"

#if WITH_DEV_AUTOMATION_TESTS

#if PLATFORM_DESKTOP

/**
 * Test generic state machine node class methods.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStateMachineInstanceMethodsTest, "LogicDriver.StateMachineNodeClass.Methods", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FStateMachineInstanceMethodsTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(2)
	
	UEdGraphPin* LastStatePin = nullptr;
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin);

	const int32 NestedStateCount = 3;
	const USMGraphNode_StateMachineStateNode* NestedFSMNode = TestHelpers::BuildNestedStateMachine(this, StateMachineGraph, NestedStateCount, &LastStatePin, nullptr);

	LastStatePin = NestedFSMNode->GetOutputPin();
	USMGraphNode_StateMachineStateNode* NestedFSMRefNode = TestHelpers::BuildNestedStateMachine(this, StateMachineGraph, NestedStateCount, &LastStatePin, nullptr);

	USMBlueprint* NewReferencedBlueprint = FSMBlueprintEditorUtils::ConvertStateMachineToReference(NestedFSMRefNode, false, nullptr, nullptr);
	FAssetHandler ReferencedAsset = TestHelpers::CreateAssetFromBlueprint(NewReferencedBlueprint);
	FKismetEditorUtilities::CompileBlueprint(NewReferencedBlueprint);
	
	const USMInstance* Instance = TestHelpers::CompileAndCreateStateMachineInstanceFromBP(NewBP);

	TArray<USMStateInstance_Base*> EntryStates;
	Instance->GetRootStateMachineNodeInstance()->GetEntryStates(EntryStates);
	check(EntryStates.Num() == 1);

	TArray<USMStateInstance_Base*> AllStatesToCheck;
	{
		USMStateInstance_Base* EntryState = EntryStates[0];
		USMStateInstance_Base* SecondState = EntryState->GetNextStateByTransitionIndex(0);
		USMStateMachineInstance* NestedStateMachine = CastChecked<USMStateMachineInstance>(SecondState->GetNextStateByTransitionIndex(0));
		USMStateMachineInstance* NestedStateMachineRef = CastChecked<USMStateMachineInstance>(NestedStateMachine->GetNextStateByTransitionIndex(0));

		TArray<USMStateInstance_Base*> NestedEntryStates;
		NestedStateMachine->GetEntryStates(NestedEntryStates);
		check(NestedEntryStates.Num() == 1);
	
		USMStateInstance_Base* NestedEntryState = NestedEntryStates[0];
		USMStateInstance_Base* SecondNestedState = NestedEntryState->GetNextStateByTransitionIndex(0);
		USMStateInstance_Base* ThirdNestedState = SecondNestedState->GetNextStateByTransitionIndex(0);
		check(ThirdNestedState);

		TArray<USMStateInstance_Base*> NestedRefEntryStates;
		NestedStateMachineRef->GetEntryStates(NestedRefEntryStates);
		check(NestedRefEntryStates.Num() == 1);
	
		USMStateInstance_Base* NestedRefEntryState = NestedRefEntryStates[0];
		USMStateInstance_Base* SecondRefNestedState = NestedRefEntryState->GetNextStateByTransitionIndex(0);
		USMStateInstance_Base* ThirdRefNestedState = SecondRefNestedState->GetNextStateByTransitionIndex(0);
		check(ThirdRefNestedState);
		
		AllStatesToCheck.Append({EntryState, SecondState,
			NestedStateMachine, NestedEntryState, SecondNestedState, ThirdNestedState,
			NestedStateMachineRef, NestedRefEntryState,  SecondRefNestedState, ThirdRefNestedState});
	}

	auto TestStateMachineInstance = [&](const USMStateMachineInstance* InStateMachineNodeInstance, const int32 InExpectedStateCount)
	{
		TArray<USMStateInstance_Base*> AllStateInstances;
		InStateMachineNodeInstance->GetAllStateInstances(AllStateInstances);

		TestEqual("Top level states found", AllStateInstances.Num(), InExpectedStateCount);

		for (const USMStateInstance_Base* State : AllStateInstances)
		{
			USMStateInstance_Base* FoundState = InStateMachineNodeInstance->GetContainedStateByName(State->GetNodeName());
			TestNotNull("State found by name", FoundState);
		}

		TArray<USMStateInstance_Base*> FoundEntryStates;
		InStateMachineNodeInstance->GetEntryStates(FoundEntryStates);
		TestEqual("Entry state found", FoundEntryStates.Num(), 1);
	};
	
	TestStateMachineInstance(Instance->GetRootStateMachineNodeInstance(), 4);

	for (USMStateInstance_Base* State : AllStatesToCheck)
	{
		if (const USMStateMachineInstance* StateMachineNodeInstance = Cast<USMStateMachineInstance>(State))
		{
			TestStateMachineInstance(StateMachineNodeInstance, NestedStateCount);
		}
	}
	
	ReferencedAsset.DeleteAsset(this);
	return NewAsset.DeleteAsset(this);
}

bool TestIsInEndState(FAutomationTestBase* Test, bool bTestDeprecatedFunctions, USMGraph* StateMachineGraph, USMBlueprint* NewBP)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	
	UEdGraphPin* LastStatePin = nullptr;

	const int32 NestedStateCount = 2;
	const USMGraphNode_StateMachineStateNode* NestedFSMNode = TestHelpers::BuildNestedStateMachine(Test, StateMachineGraph, NestedStateCount, &LastStatePin, nullptr);
	USMStateMachineInstance* StateMachineNodeTemplate = CastChecked<USMStateMachineInstance>(NestedFSMNode->GetNodeTemplate());
	StateMachineNodeTemplate->SetWaitForEndState(true);

	// Build second state to serve as end state.
	LastStatePin = NestedFSMNode->GetOutputPin();
	TestHelpers::BuildLinearStateMachine(Test, StateMachineGraph, 1, &LastStatePin);
	
	USMInstance* Instance = TestHelpers::CompileAndCreateStateMachineInstanceFromBP(NewBP);
	Instance->Start();

	USMStateMachineInstance* StateMachineNodeInstance =
		CastChecked<USMStateMachineInstance>(Instance->GetSingleActiveStateInstance(false));

	int32 Iteration = 0;
	constexpr int32 MaxIterations = 4;

	bool bReuseCurrentState = false;
	
	Repeat:
	StateMachineNodeInstance->SetReuseCurrentState(bReuseCurrentState);
	
	// First nested state
	{
		Test->TestFalse("Instance in end state", Instance->IsInEndState());
		Test->TestFalse("NestedFSM not in end state", bTestDeprecatedFunctions ?
			StateMachineNodeInstance->IsInEndState() : StateMachineNodeInstance->IsStateMachineInEndState());
		Test->TestFalse("NestedFSM not an end state", StateMachineNodeInstance->IsEndState());
	}
	
	Instance->Update();
	
	TArray<USMStateInstance_Base*> NestedActiveStates;
	StateMachineNodeInstance->GetActiveStates(NestedActiveStates);

	// Second nested state (end state)
	{
		Test->TestEqual("NestedFSM has active state", NestedActiveStates.Num(), 1);
		Test->TestTrue("NestedFSM is still active", StateMachineNodeInstance->IsActive());
		Test->TestTrue("NestedFSM is in an end state", bTestDeprecatedFunctions ?
			StateMachineNodeInstance->IsInEndState() : StateMachineNodeInstance->IsStateMachineInEndState());
	}
	Instance->Update();
	
	const USMStateInstance_Base* EndStateInstance = Instance->GetSingleActiveStateInstance(false);
	check(EndStateInstance);
	
	// Second top level state, nested FSM finished
	{
		StateMachineNodeInstance->GetActiveStates(NestedActiveStates);
		Test->TestEqual("NestedFSM has correct active states", NestedActiveStates.Num(), bReuseCurrentState);
		Test->TestFalse("NestedFSM is no longer active", StateMachineNodeInstance->IsActive());
		Test->TestTrue("NestedFSM is still considered in an end state", bTestDeprecatedFunctions ?
			StateMachineNodeInstance->IsInEndState() : StateMachineNodeInstance->IsStateMachineInEndState());

		Test->TestTrue("Top level SM switched to new state", EndStateInstance != StateMachineNodeInstance);
		Test->TestTrue("Main instance is in end state", Instance->IsInEndState());
	}

	// Loop back to entry state, make sure IsInEndState resets correctly
	if (Iteration < MaxIterations)
	{
		if (Iteration > (MaxIterations / 2))
		{
			bReuseCurrentState = true;
		}
		Iteration++;
		Instance->SwitchActiveState(StateMachineNodeInstance);
		goto Repeat;
	}

	Test->TestTrue("Reuse Current State test", bReuseCurrentState);

	return true;

	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

/**
 * Test IsInEndState functionality. New version should match deprecated functionality.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStateMachineInstanceIsInEndStateTest, "LogicDriver.StateMachineNodeClass.IsInEndState", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FStateMachineInstanceIsInEndStateTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST_NO_STATES()

	constexpr bool bDeprecatedTest = false;
	return TestIsInEndState(this, bDeprecatedTest, StateMachineGraph, NewBP);
}

/**
 * Test IsInEndState functionality of the deprecated function.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStateMachineInstanceIsInEndStateDeprecatedTest, "LogicDriver.StateMachineNodeClass.IsInEndState_Deprecated", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FStateMachineInstanceIsInEndStateDeprecatedTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST_NO_STATES()

	constexpr bool bDeprecatedTest = true;
	return TestIsInEndState(this, bDeprecatedTest, StateMachineGraph, NewBP);
}

#endif

#endif //WITH_DEV_AUTOMATION_TESTS