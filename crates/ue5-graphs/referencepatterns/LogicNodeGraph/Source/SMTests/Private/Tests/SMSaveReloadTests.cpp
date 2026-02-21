// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMTestContext.h"
#include "SMTestHelpers.h"

#include "Blueprints/SMBlueprint.h"

#include "Configuration/SMProjectEditorSettings.h"
#include "Graph/Nodes/FunctionNodes/SMGraphK2Node_StateReadNodes.h"
#include "Graph/Nodes/SMGraphK2Node_StateMachineNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineEntryNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineStateNode.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/SMGraph.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "Kismet2/KismetEditorUtilities.h"

#if WITH_DEV_AUTOMATION_TESTS

#if PLATFORM_DESKTOP

bool TestSaveStateMachineState(FAutomationTestBase* Test, bool bCreateReferences, bool bReuseStates, bool bCreateIntermediateReferenceGraphs = false)
{
	FAssetHandler NewAsset;
	if (!TestHelpers::TryCreateNewStateMachineAsset(Test, NewAsset, false, USMTestNotifyInstance::StaticClass()))
	{
		return false;
	}

	USMProjectEditorSettings* ProjectEditorSettings = FSMBlueprintEditorUtils::GetMutableProjectEditorSettings();
	const bool bUserTemplateSettings = ProjectEditorSettings->bEnableReferenceTemplatesByDefault;
	ProjectEditorSettings->bEnableReferenceTemplatesByDefault = false;
	
	USMBlueprint* NewBP = NewAsset.GetObjectAs<USMBlueprint>();
	
	// Find root state machine.
	USMGraphK2Node_StateMachineNode* RootStateMachineNode = FSMBlueprintEditorUtils::GetRootStateMachineNode(NewBP);

	// Find the state machine graph.
	USMGraph* StateMachineGraph = RootStateMachineNode->GetStateMachineGraph();

	UEdGraphPin* LastTopLevelStatePin = nullptr;

	// Build single state - state machine.
	TestHelpers::BuildLinearStateMachine(Test, StateMachineGraph, 2, &LastTopLevelStatePin);
	if (!NewAsset.SaveAsset(Test))
	{
		return false;
	}

	// Used to keep track of nested state machines and to convert them to references.
	TArray<USMGraphNode_StateMachineStateNode*> StateMachineStateNodes;

	// Build a top level state machine node. When converting to references this will be replaced with a copy of the next reference.
	// Don't add to StateMachineStateNodes list yet this will have special handling.
	UEdGraphPin* EntryPointForNestedStateMachine = LastTopLevelStatePin;
	USMGraphNode_StateMachineStateNode* NestedStateMachineNodeToUseDuplicateReference = TestHelpers::BuildNestedStateMachine(Test, StateMachineGraph, 4, &EntryPointForNestedStateMachine, nullptr);
	NestedStateMachineNodeToUseDuplicateReference->GetNodeTemplateAs<USMStateMachineInstance>()->SetReuseCurrentState(bReuseStates);
	LastTopLevelStatePin = NestedStateMachineNodeToUseDuplicateReference->GetOutputPin();

	// Build a nested state machine.

	UEdGraphPin* LastNestedPin = nullptr;
	EntryPointForNestedStateMachine = LastTopLevelStatePin;
	USMGraphNode_StateMachineStateNode* NestedStateMachineNode = TestHelpers::BuildNestedStateMachine(Test, StateMachineGraph, 4, &EntryPointForNestedStateMachine, &LastNestedPin);
	NestedStateMachineNode->GetNodeTemplateAs<USMStateMachineInstance>()->SetReuseCurrentState(bReuseStates);
	StateMachineStateNodes.Add(NestedStateMachineNode);
	LastTopLevelStatePin = NestedStateMachineNode->GetOutputPin();

	{
		// Signal the state after the first nested state machine to wait for its completion.
		USMGraphNode_TransitionEdge* TransitionFromNestedStateMachine = CastChecked<USMGraphNode_TransitionEdge>(NestedStateMachineNodeToUseDuplicateReference->GetOutputPin()->LinkedTo[0]->GetOwningNode());
		TestHelpers::OverrideTransitionResultLogic<USMGraphK2Node_StateMachineReadNode_InEndState>(Test, TransitionFromNestedStateMachine);
	}
	
	// Add two second level nested state machines.
	UEdGraphPin* EntryPointFor2xNested = LastNestedPin;
	USMGraphNode_StateMachineStateNode* NestedStateMachineNode_2 = TestHelpers::BuildNestedStateMachine(Test, Cast<USMGraph>(NestedStateMachineNode->GetBoundGraph()),
		4, &EntryPointFor2xNested, nullptr);
	NestedStateMachineNode_2->GetNodeTemplateAs<USMStateMachineInstance>()->SetReuseCurrentState(bReuseStates);
	StateMachineStateNodes.Insert(NestedStateMachineNode_2, 0);
	{
		UEdGraphPin* Nested1xPinOut = NestedStateMachineNode_2->GetOutputPin();
		// Add more 1x states nested level (states leading from the second nested state machine).
		{
			TestHelpers::BuildLinearStateMachine(Test, Cast<USMGraph>(NestedStateMachineNode->GetBoundGraph()), 2, &Nested1xPinOut);
			if (!NewAsset.SaveAsset(Test))
			{
				return false;
			}

			// Signal the state after the nested state machine to wait for its completion.
			USMGraphNode_TransitionEdge* TransitionFromNestedStateMachine = CastChecked<USMGraphNode_TransitionEdge>(NestedStateMachineNode_2->GetOutputPin()->LinkedTo[0]->GetOwningNode());
			TestHelpers::OverrideTransitionResultLogic<USMGraphK2Node_StateMachineReadNode_InEndState>(Test, TransitionFromNestedStateMachine);
		}
		// Add another second level nested state machine. (Sibling to above state machine)
		{
			USMGraphNode_StateMachineStateNode* NestedStateMachineNode_2_2 = TestHelpers::BuildNestedStateMachine(Test, Cast<USMGraph>(NestedStateMachineNode->GetBoundGraph()),
				4, &Nested1xPinOut, nullptr);
			NestedStateMachineNode_2_2->GetNodeTemplateAs<USMStateMachineInstance>()->SetReuseCurrentState(bReuseStates);
			StateMachineStateNodes.Insert(NestedStateMachineNode_2_2, 0);
			// Add more 1x states nested level (states leading from the second nested state machine).
			{
				UEdGraphPin* Nested1x_2PinOut = NestedStateMachineNode_2_2->GetOutputPin();
				TestHelpers::BuildLinearStateMachine(Test, Cast<USMGraph>(NestedStateMachineNode->GetBoundGraph()), 2, &Nested1x_2PinOut);
				if (!NewAsset.SaveAsset(Test))
				{
					return false;
				}

				// Signal the state after the nested state machine to wait for its completion.
				USMGraphNode_TransitionEdge* TransitionFromNestedStateMachine = CastChecked<USMGraphNode_TransitionEdge>(NestedStateMachineNode_2_2->GetOutputPin()->LinkedTo[0]->GetOwningNode());
				TestHelpers::OverrideTransitionResultLogic<USMGraphK2Node_StateMachineReadNode_InEndState>(Test, TransitionFromNestedStateMachine);
			}
		}
	}

	// Add more top level (states leading from nested state machine).
	{
		TestHelpers::BuildLinearStateMachine(Test, StateMachineGraph, 2, &LastTopLevelStatePin);
		if (!NewAsset.SaveAsset(Test))
		{
			return false;
		}

		// Signal the state after the second nested state machine to wait for its completion.
		USMGraphNode_TransitionEdge* TransitionFromNestedStateMachine = CastChecked<USMGraphNode_TransitionEdge>(NestedStateMachineNode->GetOutputPin()->LinkedTo[0]->GetOwningNode());
		TestHelpers::OverrideTransitionResultLogic<USMGraphK2Node_StateMachineReadNode_InEndState>(Test, TransitionFromNestedStateMachine);
	}

	TArray<FAssetHandler> ExtraAssets;
	const FName TestStringVarName = "TestStringVar";
	const FString DefaultStringValue = "BaseValue";
	const FString NewStringValue = "OverValue";

	int32 TotalReferences = 0;
	TSet<UClass*> GeneratedReferenceClasses;
	// Convert nested state machines to references.
	if (bCreateReferences)
	{
		// This loop has to run backwards (StateMachineStateNodes should be sorted in reverse) because if a top level sm is converted to a reference first
		// that would invalidate the nested state machine graphs and make converting to references more complicated.
		int32 Index = 0;
		for (USMGraphNode_StateMachineStateNode* NestedSM : StateMachineStateNodes)
		{
			// Now convert the state machine to a reference.
			FString AssetName = "SaveRef_" + FString::FromInt(Index);
			USMBlueprint* NewReferencedBlueprint = FSMBlueprintEditorUtils::ConvertStateMachineToReference(NestedSM, false, &AssetName, nullptr);
			Test->TestNotNull("New referenced blueprint created", NewReferencedBlueprint);
			TestHelpers::TestStateMachineConvertedToReference(Test, NestedSM);

			TotalReferences++;
			GeneratedReferenceClasses.Add(NewReferencedBlueprint->GeneratedClass);
			
			if (bCreateIntermediateReferenceGraphs)
			{
				NestedSM->SetUseIntermediateGraph(true);
			}

			// Add a variable to this blueprint so we can test reading it from the template.
			FEdGraphPinType StringPinType(UEdGraphSchema_K2::PC_String, NAME_None, nullptr, EPinContainerType::None, false, FEdGraphTerminalType());
			FBlueprintEditorUtils::AddMemberVariable(NewReferencedBlueprint, TestStringVarName, StringPinType, DefaultStringValue);
			
			FKismetEditorUtilities::CompileBlueprint(NewReferencedBlueprint);
			
			USMInstance* ReferenceTemplate = NestedSM->GetStateMachineReferenceTemplateDirect();
			Test->TestNull("Template null because it is not enabled", ReferenceTemplate);

			// Manually set and update template value. Normally checking the box will trigger the state machine reference to init.
			NestedSM->bUseTemplate = true;
			NestedSM->InitStateMachineReferenceTemplate();

			ReferenceTemplate = NestedSM->GetStateMachineReferenceTemplateDirect();
			Test->TestNotNull("Template not null because it is enabled", ReferenceTemplate);

			Test->TestNotNull("New referenced template instantiated", ReferenceTemplate);
			Test->TestEqual("Direct template has owner of nested node", ReferenceTemplate->GetOuter(), (UObject*)NestedSM);

			TestHelpers::TestSetTemplate(Test, ReferenceTemplate, DefaultStringValue, NewStringValue);
			
			// Store handler information so we can delete the object.
			FAssetHandler ReferencedAsset = TestHelpers::CreateAssetFromBlueprint(NewReferencedBlueprint);

			ExtraAssets.Add(ReferencedAsset);

			Index++;
		}

		// Replace the second nested state machine node with a copy of the first reference.
		{
			NestedStateMachineNodeToUseDuplicateReference->ReferenceStateMachine(NestedStateMachineNode->GetStateMachineReference());

			// Set template value.
			NestedStateMachineNodeToUseDuplicateReference->bUseTemplate = true;
			NestedStateMachineNodeToUseDuplicateReference->InitStateMachineReferenceTemplate();
			USMInstance* ReferenceTemplate = NestedStateMachineNodeToUseDuplicateReference->GetStateMachineReferenceTemplateDirect();
			TestHelpers::TestSetTemplate(Test, ReferenceTemplate, DefaultStringValue, NewStringValue);
			
			// This is a reference which contains all of the created references so far.
			TotalReferences += StateMachineStateNodes.Num();
			
			// Now add so it can be tested.
			StateMachineStateNodes.Add(NestedStateMachineNodeToUseDuplicateReference);
		}
	}
	
	FKismetEditorUtilities::CompileBlueprint(NewBP);

	// Test running normally, then test manually evaluating transitions.
	{
		int32 EntryVal, UpdateVal, EndVal;
		TestHelpers::RunStateMachineToCompletion(Test, NewBP, EntryVal, UpdateVal, EndVal);

		USMTestContext* Context = NewObject<USMTestContext>();
		USMInstance* StateMachineInstance = TestHelpers::CreateNewStateMachineInstanceFromBP(Test, NewBP, Context);

		StateMachineInstance->Start();
		while (!StateMachineInstance->IsInEndState())
		{
			StateMachineInstance->EvaluateTransitions();
		}
		
		StateMachineInstance->Stop();

		int32 CompareEntry = Context->GetEntryInt();
		int32 CompareUpdate = Context->GetUpdateFromDeltaSecondsInt();
		int32 CompareEnd = Context->GetEndInt();

		Test->TestEqual("Manual transition evaluation matches normal tick", CompareEntry, EntryVal);
		Test->TestEqual("Manual transition evaluation matches normal tick", CompareUpdate, UpdateVal);
		Test->TestEqual("Manual transition evaluation matches normal tick", CompareEnd, EndVal);
	}
	
	// Now increment its states testing that active/current state retrieval works properly.
	{
		// Create a context we will run the state machine for.
		USMTestContext* Context = NewObject<USMTestContext>();
		USMInstance* StateMachineInstance = TestHelpers::CreateNewStateMachineInstanceFromBP(Test, NewBP, Context);

		// Validate instances retrievable
		TArray<USMInstance*> AllReferences = StateMachineInstance->GetAllReferencedInstances(true);
		if (!bCreateReferences)
		{
			Test->TestEqual("", AllReferences.Num(), 0);
		}
		else
		{
			TSet<UClass*> ReferenceClasses;
			for (USMInstance* Ref : AllReferences)
			{
				ReferenceClasses.Add(Ref->GetClass());
			}

			int32 Match = TestHelpers::ArrayContentsInArray(ReferenceClasses.Array(), GeneratedReferenceClasses.Array());
			Test->TestEqual("Unique reference classes found", Match, GeneratedReferenceClasses.Num());

			Test->TestEqual("All nested references found", AllReferences.Num(), TotalReferences);

			AllReferences = StateMachineInstance->GetAllReferencedInstances(false);
			Test->TestEqual("Direct references", AllReferences.Num(), 2);

			// Templates should only be in CDO.
			Test->TestEqual("Instance doesn't have templates stored", StateMachineInstance->ReferenceTemplates.Num(), 0);
			
			// Validate template stored in default object correctly.
			USMInstance* DefaultObject = Cast<USMInstance>(StateMachineInstance->GetClass()->GetDefaultObject(false));
			const int32 TotalTemplates = 2;
			Test->TestEqual("Reference template in CDO", DefaultObject->ReferenceTemplates.Num(), TotalTemplates);
			int32 TemplatesVerified = 0;
			for (UObject* TemplateObject : DefaultObject->ReferenceTemplates)
			{
				if (USMInstance* Template = Cast<USMInstance>(TemplateObject))
				{
					Test->TestEqual("Template outered to instance default object", Template->GetOuter(), (UObject*)DefaultObject);
					bool bStringDefaultValueVerified = false;
					for (TFieldIterator<FStrProperty> It(Template->GetClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
					{
						FString* Ptr = (*It)->ContainerPtrToValuePtr<FString>(Template);
						FString StrValue = *Ptr;
						Test->TestEqual("Instance has template override string value", StrValue, NewStringValue);
						bStringDefaultValueVerified = true;
					}
					Test->TestTrue("Template has string property from template verified.", bStringDefaultValueVerified);
					TemplatesVerified++;
				}
			}
			Test->TestEqual("Templates verified in CDO", TemplatesVerified, TotalTemplates);
			
			// Validate template has applied default values to referenced instance.
			for (USMInstance* ReferenceInstance : AllReferences)
			{
				bool bStringDefaultValueVerified = false;
				for (TFieldIterator<FStrProperty> It(ReferenceInstance->GetClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
				{
					FString* Ptr = (*It)->ContainerPtrToValuePtr<FString>(ReferenceInstance);
					FString StrValue = *Ptr;
					Test->TestEqual("Instance has template override string value", StrValue, NewStringValue);
					bStringDefaultValueVerified = true;
				}
				Test->TestTrue("Instance has string property from template verified.", bStringDefaultValueVerified);
			}
		}
		
		Test->TestNull("No nested active state", StateMachineInstance->GetSingleNestedActiveState());

		FSMState_Base* OriginalInitialState = StateMachineInstance->GetRootStateMachine().GetSingleInitialState();
		Test->TestNotNull("Initial state set", OriginalInitialState);

		// This will test retrieving the nested active state thoroughly.
		int32 StatesHit = TestHelpers::RunAllStateMachinesToCompletion(Test, StateMachineInstance, &StateMachineInstance->GetRootStateMachine(), -1, 0);
		int32 TotalStatesHit = StatesHit;
		
		FSMState_Base* ActiveNestedState = StateMachineInstance->GetSingleNestedActiveState();
		FSMState_Base* SavedActiveState = nullptr;
		Test->TestNotNull("Active nested state not null", ActiveNestedState);
		Test->TestNotEqual("Current active nested state not equal to original", OriginalInitialState, ActiveNestedState);

		StateMachineInstance->Stop();
		ActiveNestedState = StateMachineInstance->GetSingleNestedActiveState();
		Test->TestNull("Active nested state null after stop", ActiveNestedState);

		TArray<FGuid> SavedStateGuids;

		const int32 StatesNotHit = 5;
		// Re-instantiate and abort sooner.
		{
			USMTestContext* NewContext = NewObject<USMTestContext>();
			USMInstance* NewStateMachineInstance = TestHelpers::CreateNewStateMachineInstanceFromBP(Test, NewBP, NewContext);

			OriginalInitialState = NewStateMachineInstance->GetRootStateMachine().GetSingleInitialState();

			NewStateMachineInstance->Start();
			Test->TestTrue("State Machine should have started", NewStateMachineInstance->IsActive());

			// Run but not through all states.
			TestHelpers::RunAllStateMachinesToCompletion(Test, NewStateMachineInstance, &NewStateMachineInstance->GetRootStateMachine(), StatesHit - StatesNotHit, -1);
			SavedActiveState = ActiveNestedState = NewStateMachineInstance->GetSingleNestedActiveState();
			Test->TestNotEqual("Nested state shouldn't equal original state", ActiveNestedState, OriginalInitialState);

			// Top level, nested_1 (exited already) nested_2, nested_2_1 (exited already), nested_2_2
			NewStateMachineInstance->GetAllActiveStateGuids(SavedStateGuids);

			// One state machine is inactive so the states restored depend on if the current state is reused. Reusing states increases count of second nested state machine.
			// First nested state machine is replaced with a reference to second nested state machine when using references.
			int32 ExpectedStates = 3;
			if (bReuseStates)
			{
				ExpectedStates += 2;
				if (bCreateReferences)
				{
					ExpectedStates += 2;
				}
			}
			Test->TestEqual("Current states tracked", SavedStateGuids.Num(), ExpectedStates);
		}

		// Re-instantiate and restore state.
		{
			USMTestContext* NewContext = NewObject<USMTestContext>();
			USMInstance* NewStateMachineInstance = TestHelpers::CreateNewStateMachineInstanceFromBP(Test, NewBP, NewContext);

			OriginalInitialState = NewStateMachineInstance->GetRootStateMachine().GetSingleInitialState();

			// Should be able to locate this instance's active state struct.
			ActiveNestedState = NewStateMachineInstance->FindStateByGuid(ActiveNestedState->GetGuid());
			Test->TestNotNull("Active state found by guid", ActiveNestedState);

			// Restore the state machine active state.
			NewStateMachineInstance->LoadFromState(ActiveNestedState->GetGuid());
			bool bIsStateMachine = NewStateMachineInstance->GetRootStateMachine().GetSingleInitialState()->IsStateMachine();
			Test->TestTrue("Initial top level state should be state machine", bIsStateMachine);
			if (!bIsStateMachine)
			{
				return false;
			}
			
			ActiveNestedState = ((FSMStateMachine*)NewStateMachineInstance->GetRootStateMachine().GetSingleInitialState())->FindState(ActiveNestedState->GetGuid());
			Test->TestNotNull("Active state found by guid from within top level initial state.", ActiveNestedState);

			NewStateMachineInstance->Start();
			Test->TestTrue("State Machine should have started", NewStateMachineInstance->IsActive());
			Test->TestEqual("The first state to start should be equal to the previous saved active state", NewStateMachineInstance->GetSingleNestedActiveState(), ActiveNestedState);

			// Run to the very last state. References won't have states remaining and non references will have the end state left.
			StatesHit = TestHelpers::RunAllStateMachinesToCompletion(Test, NewStateMachineInstance, &NewStateMachineInstance->GetRootStateMachine(), -1, -1);
			
			Test->TestEqual("Correct number of states hit", StatesHit, StatesNotHit);
			
			ActiveNestedState = NewStateMachineInstance->GetSingleNestedActiveState();
			Test->TestNotEqual("Nested state shouldn't equal initial state", ActiveNestedState, NewStateMachineInstance->GetRootStateMachine().GetSingleInitialState());
		}

		// Re-instantiate and restore all states.
		{
			USMTestContext* NewContext = NewObject<USMTestContext>();
			USMInstance* NewStateMachineInstance = TestHelpers::CreateNewStateMachineInstanceFromBP(Test, NewBP, NewContext);

			OriginalInitialState = NewStateMachineInstance->GetRootStateMachine().GetSingleInitialState();

			TArray<FGuid> OriginalStateGuids;
			NewStateMachineInstance->GetAllActiveStateGuids(OriginalStateGuids);

			int32 Match = TestHelpers::ArrayContentsInArray(OriginalStateGuids, SavedStateGuids);
			Test->TestEqual("Original guids don't match saved guids", Match, 0);

			// Should be able to locate this instance's active state struct.
			SavedActiveState = NewStateMachineInstance->FindStateByGuid(SavedActiveState->GetGuid());
			Test->TestNotNull("Active state found by guid", SavedActiveState);

			// Restore the state machine states.
			NewStateMachineInstance->LoadFromMultipleStates(SavedStateGuids);
			Test->TestTrue("Initial top level state should be state machine", NewStateMachineInstance->GetRootStateMachine().GetSingleInitialState()->IsStateMachine());

			SavedActiveState = ((FSMStateMachine*)NewStateMachineInstance->GetRootStateMachine().GetSingleInitialState())->FindState(SavedActiveState->GetGuid());
			Test->TestNotNull("Active state found by guid from within top level initial state.", SavedActiveState);

			NewStateMachineInstance->Start();
			Test->TestTrue("State Machine should have started", NewStateMachineInstance->IsActive());
			Test->TestEqual("The first state to start should be equal to the previous saved active state", NewStateMachineInstance->GetSingleNestedActiveState(), SavedActiveState);

			// Validate all states restored.
			Match = TestHelpers::ArrayContentsInArray(NewStateMachineInstance->GetAllActiveStateGuidsCopy(), SavedStateGuids);
			Test->TestEqual("Restored guids match saved guids", Match, SavedStateGuids.Num());

			// Run to the very last state.
			StatesHit = TestHelpers::RunAllStateMachinesToCompletion(Test, NewStateMachineInstance, &NewStateMachineInstance->GetRootStateMachine(), -1, -1);
			Test->TestEqual("Correct number of states hit", StatesHit, StatesNotHit);

			SavedActiveState = NewStateMachineInstance->GetSingleNestedActiveState();
			Test->TestNotEqual("Nested state shouldn't equal initial state", SavedActiveState, NewStateMachineInstance->GetRootStateMachine().GetSingleInitialState());
		}

		// One last test checking incrementing every state, saving, and reloading.
		{
			for (int32 i = 0; i < TotalStatesHit; ++i)
			{
				int32 EntryHits, UpdateHits, EndHits;
				USMInstance* TestedStateMachine = TestHelpers::RunStateMachineToCompletion(Test, NewBP, EntryHits, UpdateHits, EndHits, i, true, false, false);
				TArray<FGuid> CurrentGuids;
				TestedStateMachine->GetAllActiveStateGuids(CurrentGuids);

				TestedStateMachine = TestHelpers::CreateNewStateMachineInstanceFromBP(Test, NewBP, Context);
				TestedStateMachine->LoadFromMultipleStates(CurrentGuids);

				TArray<FGuid> ReloadedGuids;
				TestedStateMachine->GetAllActiveStateGuids(ReloadedGuids);

				int32 Match = TestHelpers::ArrayContentsInArray(ReloadedGuids, CurrentGuids);
				Test->TestEqual("State machine states reloaded", Match, CurrentGuids.Num());
			}
		}
	}

	for (FAssetHandler& Asset : ExtraAssets)
	{
		Asset.DeleteAsset(Test);
	}

	ProjectEditorSettings->bEnableReferenceTemplatesByDefault = bUserTemplateSettings;
	
	return NewAsset.DeleteAsset(Test);
}

/**
 * Save and restore the state of a hierarchical state machine, then do it again with bReuseCurrentState.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveStateMachineStateTest, "LogicDriver.SaveRestore.StateMachineState", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FSaveStateMachineStateTest::RunTest(const FString& Parameters)
{
	const bool bUseReferences = false;
	return TestSaveStateMachineState(this, bUseReferences, false) &&
		TestSaveStateMachineState(this, bUseReferences, true);
}

/**
 * Save and restore the state of a hierarchical state machine with references.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveStateMachineStateWithReferencesTest, "LogicDriver.SaveRestore.StateMachineStateWithReferences", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FSaveStateMachineStateWithReferencesTest::RunTest(const FString& Parameters)
{
	const bool bUseReferences = true;
	return TestSaveStateMachineState(this, bUseReferences, false) &&
		TestSaveStateMachineState(this, bUseReferences, true);
}

/**
 * Save and restore the state of a hierarchical state machine with references which use intermediate graphs.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveStateMachineStateWithIntermediateReferencesTest, "LogicDriver.SaveRestore.StateMachineStateWithIntermediateReferences", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FSaveStateMachineStateWithIntermediateReferencesTest::RunTest(const FString& Parameters)
{
	const bool bUseReferences = true;
	return TestSaveStateMachineState(this, bUseReferences, false, true) &&
		TestSaveStateMachineState(this, bUseReferences, true, true);
}

#endif

#endif //WITH_DEV_AUTOMATION_TESTS