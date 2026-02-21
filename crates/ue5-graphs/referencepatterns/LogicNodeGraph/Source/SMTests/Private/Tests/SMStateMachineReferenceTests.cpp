// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "BlueprintEditor.h"
#include "SMTestHelpers.h"
#include "Helpers/SMTestBoilerplate.h"

#include "Blueprints/SMBlueprintFactory.h"
#include "SMTestContext.h"
#include "Utilities/SMBlueprintEditorUtils.h"
#include "Graph/SMGraph.h"
#include "Graph/Nodes/SMGraphK2Node_StateMachineNode.h"
#include "Graph/Nodes/SMGraphNode_StateNode.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/Nodes/SMGraphNode_StateMachineStateNode.h"
#include "Graph/Nodes/FunctionNodes/SMGraphK2Node_StateReadNodes.h"

#include "Blueprints/SMBlueprint.h"
#include "SMUtils.h"

#include "EdGraph/EdGraph.h"
#include "Kismet2/KismetEditorUtilities.h"

#if WITH_DEV_AUTOMATION_TESTS

#if PLATFORM_DESKTOP

/**
 * Assemble a hierarchical state machine and convert the nested state machine to a reference, then run and wait for the referenced state machine to finish.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReferenceStateMachineTest, "LogicDriver.StateMachineReference.DefaultClass", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FReferenceStateMachineTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST_NO_STATES()

	constexpr int32 TotalStatesBeforeReferences = 10;
	constexpr int32 TotalStatesAfterReferences = 10;
	constexpr int32 TotalNestedStates = 10;
	constexpr int32 TotalReferences = 1;
	constexpr int32 TotalTopLevelStates = TotalStatesBeforeReferences + TotalStatesAfterReferences + TotalReferences;

	TArray<FAssetHandler> ReferencedAssets;
	TArray<USMGraphNode_StateMachineStateNode*> NestedStateMachineNodes;
	
	const int32 TotalStates = TestHelpers::BuildStateMachineWithReferences(this, StateMachineGraph, TotalStatesBeforeReferences, TotalStatesAfterReferences,
		TotalReferences, TotalNestedStates, ReferencedAssets, NestedStateMachineNodes);

	check(ReferencedAssets.Num() == TotalReferences);
	check(NestedStateMachineNodes.Num() == TotalReferences);
	
	USMGraphNode_StateMachineStateNode* NestedStateMachineNode = NestedStateMachineNodes[0];
	TestTrue("Reference is set", NestedStateMachineNode->IsStateMachineReference());
	
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
		UEdGraph* TransitionGraph = TransitionFromNestedStateMachine->GetBoundGraph();
		TransitionGraph->Nodes.Empty();
		TransitionGraph->GetSchema()->CreateDefaultNodesForGraph(*TransitionGraph);

		TestHelpers::AddSpecialBooleanTransitionLogic<USMGraphK2Node_StateMachineReadNode_InEndState>(this, TransitionFromNestedStateMachine);
		ExpectedEntryValue = TotalStates;

		// Run the same machine until an end state is reached. This time the result should be modified by all nested states.
		int32 EntryHits = 0; int32 UpdateHits = 0; int32 EndHits = 0;
		USMInstance* Instance = TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits, 1000, false);
		TArray<USMInstance*> References = Instance->GetAllReferencedInstances();
		TestEqual("Correct references found", References.Num(), 1);

		for (const USMInstance* Reference : References)
		{
			TArray<USMStateInstance_Base*> StateInstances;
			Reference->GetAllStateInstances(StateInstances);

			TestEqual("Correct reference states found", StateInstances.Num(), TotalNestedStates);

			TArray<USMTransitionInstance*> TransitionInstances;
			Reference->GetAllTransitionInstances(TransitionInstances);

			TestEqual("Correct reference transitions found", TransitionInstances.Num(), TotalNestedStates - 1);
		}

		Instance->Shutdown();

		TestEqual("State Machine generated value", EntryHits, ExpectedEntryValue);
		TestEqual("State Machine generated value", UpdateHits, 0);
		TestEqual("State Machine generated value", EndHits, ExpectedEntryValue - 1 /** 1 less since we shutdown after EndHits set. */);
	}

	// Verify it can't reference itself.
	AddExpectedError("Cannot directly reference the same state machine");
	const bool bReferenceSelf = NestedStateMachineNode->ReferenceStateMachine(NewBP);
	TestFalse("State Machine should not have been allowed to reference itself", bReferenceSelf);

	ReferencedAssets[0].DeleteAsset(this);
	return NewAsset.DeleteAsset(this);
}

/**
 * Use a dynamic variable to determine the class of the state machine reference.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDynamicReferenceStateMachineTest, "LogicDriver.StateMachineReference.DynamicClass", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FDynamicReferenceStateMachineTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST_NO_STATES()

	// Create an asset we will dynamically reference.
	FAssetHandler DynamicStateMachineAsset;
	if (!TestHelpers::TryCreateNewStateMachineAsset(this, DynamicStateMachineAsset, false))
	{
		return false;
	}

	constexpr int32 TotalDynamicStates = 15;
	USMBlueprint* DynamicBlueprint = DynamicStateMachineAsset.GetObjectAs<USMBlueprint>();
	{
		UEdGraphPin* LastTopLevelStatePin = nullptr;

		// Build single state - state machine.
		TestHelpers::BuildLinearStateMachine(this, FSMBlueprintEditorUtils::GetRootStateMachineNode(DynamicBlueprint)->GetStateMachineGraph(), TotalDynamicStates, &LastTopLevelStatePin);
		FKismetEditorUtilities::CompileBlueprint(DynamicBlueprint);
	}
	
	constexpr int32 TotalStatesBeforeReferences = 10;
	constexpr int32 TotalStatesAfterReferences = 10;
	constexpr int32 TotalNestedStates = 10;
	constexpr int32 TotalReferences = 1;

	TArray<FAssetHandler> ReferencedAssets;
	TArray<USMGraphNode_StateMachineStateNode*> NestedStateMachineNodes;
	
	const int32 TotalStates = TestHelpers::BuildStateMachineWithReferences(this, StateMachineGraph, TotalStatesBeforeReferences, TotalStatesAfterReferences,
		TotalReferences, TotalNestedStates, ReferencedAssets, NestedStateMachineNodes);
	
	check(ReferencedAssets.Num() == TotalReferences);
	check(NestedStateMachineNodes.Num() == TotalReferences);

	UBlueprint* OriginalReferencedBP = ReferencedAssets[0].GetObjectAs<UBlueprint>();
	check(OriginalReferencedBP);
	
	// The current reference.
	USMGraphNode_StateMachineStateNode* NestedStateMachineNode = NestedStateMachineNodes[0];
	TestTrue("Reference is set", NestedStateMachineNode->IsStateMachineReference());

	// Wait for end state.
	{
		USMGraphNode_TransitionEdge* TransitionFromNestedStateMachine = CastChecked<USMGraphNode_TransitionEdge>(NestedStateMachineNode->GetOutputPin()->LinkedTo[0]->GetOwningNode());
		UEdGraph* TransitionGraph = TransitionFromNestedStateMachine->GetBoundGraph();
		TransitionGraph->Nodes.Empty();
		TransitionGraph->GetSchema()->CreateDefaultNodesForGraph(*TransitionGraph);

		TestHelpers::AddSpecialBooleanTransitionLogic<USMGraphK2Node_StateMachineReadNode_InEndState>(this, TransitionFromNestedStateMachine);
	}
	
	// Add a variable to use for the dynamic reference class.
	const FString BaseName = NestedStateMachineNode->GetBoundGraph()->GetName() + TEXT("DynamicClass");
	const FName DynamicVarName = FBlueprintEditorUtils::FindUniqueKismetName(NewBP, BaseName);
	FEdGraphPinType PinType;
	PinType.PinCategory = UEdGraphSchema_K2::PC_Class;
	PinType.PinSubCategoryObject = USMInstance::StaticClass();
	check(FBlueprintEditorUtils::AddMemberVariable(NewBP, DynamicVarName, PinType));

	NestedStateMachineNode->DynamicClassVariable = DynamicVarName;

	USMInstance* Instance = nullptr;

	// Test with no value assigned to variable.
	{
		AddExpectedError(TEXT("Dynamic state machine reference creation failed"));
		
		int32 ExpectedEntryValue = TotalStates;
		
		int32 EntryHits = 0; int32 UpdateHits = 0; int32 EndHits = 0;
		Instance = TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits, 1000, false);

		TArray<USMInstance*> ReferencedInstances = Instance->GetAllReferencedInstances();
		check(ReferencedInstances.Num() == 1);
		
		TestEqual("Old reference set", ReferencedInstances[0]->GetClass(), static_cast<UClass*>(OriginalReferencedBP->GeneratedClass));

		TestEqual("State Machine generated value", EntryHits, ExpectedEntryValue);
		TestEqual("State Machine generated value", UpdateHits, 0);
		TestEqual("State Machine generated value", EndHits, ExpectedEntryValue - 1 /* Not shutdown so last entry wasn't hit. */);

		Instance->Shutdown();
	}
	{
		// Now change the class.
		FClassProperty* ClassProperty = CastFieldChecked<FClassProperty>(Instance->GetClass()->FindPropertyByName(DynamicVarName));
		ClassProperty->SetObjectPropertyValue_InContainer(Instance, DynamicBlueprint->GeneratedClass);

		// Test reinitialization of same instance.
		{
			Instance->Initialize(NewObject<USMTestContext>());
			TArray<USMInstance*> ReferencedInstances = Instance->GetAllReferencedInstances();
			check(ReferencedInstances.Num() == 1);
			TestEqual("New reference set", ReferencedInstances[0]->GetClass(), static_cast<UClass*>(DynamicBlueprint->GeneratedClass));
		}

		// Test initialization of new instance.
		{
			int32 ExpectedEntryValue = TotalStates + TotalDynamicStates - TotalNestedStates;
			int32 EntryHits = 0; int32 UpdateHits = 0; int32 EndHits = 0;
			Instance = USMBlueprintUtils::CreateStateMachineInstance(static_cast<UClass*>(NewBP->GeneratedClass), NewObject<USMTestContext>(), false);
			ClassProperty->SetObjectPropertyValue_InContainer(Instance, DynamicBlueprint->GeneratedClass);
			
			TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits, 1000,
				false, true, false, nullptr, Instance);
			TArray<USMInstance*> ReferencedInstances = Instance->GetAllReferencedInstances();
		
			TestEqual("New reference set", ReferencedInstances[0]->GetClass(), static_cast<UClass*>(DynamicBlueprint->GeneratedClass));

			TestEqual("State Machine generated value", EntryHits, ExpectedEntryValue);
			TestEqual("State Machine generated value", UpdateHits, 0);
			TestEqual("State Machine generated value", EndHits, ExpectedEntryValue - 1 /* Not shutdown so last entry wasn't hit. */);

			Instance->Shutdown();
		}
	}

	DynamicStateMachineAsset.DeleteAsset(this);
	ReferencedAssets[0].DeleteAsset(this);
	return NewAsset.DeleteAsset(this);
}

#endif

#endif //WITH_DEV_AUTOMATION_TESTS