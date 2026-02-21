// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Helpers/SMTestBoilerplate.h"
#include "SMTestContext.h"
#include "SMTestHelpers.h"

#include "Configuration/SMProjectEditorSettings.h"
#include "Construction/SMEditorConstructionManager.h"
#include "Construction/SMEditorInstance.h"
#include "Graph/Nodes/PropertyNodes/SMGraphK2Node_TextPropertyNode.h"
#include "Graph/Nodes/SMGraphK2Node_StateMachineNode.h"
#include "Graph/Nodes/SMGraphNode_ConduitNode.h"
#include "Graph/Nodes/SMGraphNode_StateNode.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/SMGraph.h"
#include "Utilities/SMBlueprintEditorUtils.h"
#include "Utilities/SMNodeInstanceUtils.h"

#include "Blueprints/SMBlueprint.h"

#include "K2Node_CallParentFunction.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet2/KismetEditorUtilities.h"

#if WITH_DEV_AUTOMATION_TESTS

#if PLATFORM_DESKTOP

/**
 * Unit test construction behavior.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNodeInstanceConstructionScriptManagerTest, "LogicDriver.ConstructionScript.Manager", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FNodeInstanceConstructionScriptManagerTest::RunTest(const FString& Parameters)
{
	TestFalse("No pending construction scripts", ISMEditorConstructionManager::Get().HasPendingConstructionScripts());

	USMProjectEditorSettings* Settings = FSMBlueprintEditorUtils::GetMutableProjectEditorSettings();
	const auto CurrentCSSetting = Settings->EditorNodeConstructionScriptSetting;
	Settings->EditorNodeConstructionScriptSetting = ESMEditorConstructionScriptProjectSetting::SM_Standard;

	SETUP_NEW_STATE_MACHINE_FOR_TEST(2)

	UEdGraphPin* LastStatePin = nullptr;
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin);
	USMInstance* CDO = CastChecked<USMInstance>(NewBP->GeneratedClass->ClassDefaultObject);
	CDO->SetRootStateMachineNodeClass(USMStateMachineTestInstance::StaticClass());
	
	ISMEditorConstructionManager& Manager = ISMEditorConstructionManager::Get();

	static_cast<FSMEditorConstructionManager&>(Manager).Tick(0.f); // Make sure construction scripts cleared out.
	
	TestFalse("No pending construction scripts", Manager.HasPendingConstructionScripts());
	Manager.RunAllConstructionScriptsForBlueprint(NewBP);
	TestTrue("Has pending construction scripts", Manager.HasPendingConstructionScripts());

	static_cast<FSMEditorConstructionManager&>(Manager).Tick(0.f);
	TestFalse("No pending construction scripts", Manager.HasPendingConstructionScripts());
	
	FSMEditorStateMachine& StateMachine = Manager.CreateEditorStateMachine(NewBP);
	USMStateMachineInstance* RootInstance = Cast<USMStateMachineInstance>(StateMachine.StateMachineEditorInstance->GetRootStateMachine().GetNodeInstance());
	TestNotNull("Root state machine node instance assigned", RootInstance);
	TestEqual("State machine node class assigned", RootInstance->GetClass(), USMStateMachineTestInstance::StaticClass());
	const FGuid RootGuidDuringConstruction = RootInstance->GetGuid();

	FSMEditorStateMachine FindStateMachine;
	const bool bFound = Manager.TryGetEditorStateMachine(NewBP, FindStateMachine);
	TestTrue("Editor state machine found", bFound);
	TestEqual("Instance Matches", FindStateMachine.StateMachineEditorInstance, StateMachine.StateMachineEditorInstance);

	TArray<USMStateInstance_Base*> EntryStates;
	RootInstance->GetEntryStates(EntryStates);

	TestEqual("Entry states assigned", EntryStates.Num(), 1);
	
	for (const FSMNode_Base* Node : StateMachine.EditorInstanceNodeStorage)
	{
		TestNotNull("Node instance assigned", Node->GetNodeInstance());
		UEdGraphNode** GraphNode = StateMachine.RuntimeNodeToGraphNode.Find(Node);
		TestTrue("Editor Graph Node assigned", GraphNode && *GraphNode);
	}

	Manager.CleanupEditorStateMachine(NewBP);

	USMInstance* RuntimeInstance = TestHelpers::CompileAndCreateStateMachineInstanceFromBP(NewBP);
	const FGuid RootGuidRuntime = RuntimeInstance->GetRootStateMachine().GetOrCreateNodeInstance()->GetGuid();

	TestEqual("Editor root guid matches runtime root guid", RootGuidDuringConstruction, RootGuidRuntime);

	Settings->EditorNodeConstructionScriptSetting = CurrentCSSetting;
	
	return true;
}

/**
 * Test construction script editor and runtime optimizations for graphs and on compile.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNodeInstanceConstructionScriptOptimizationTest, "LogicDriver.ConstructionScript.Optimization", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FNodeInstanceConstructionScriptOptimizationTest::RunTest(const FString& Parameters)
{
	FAssetHandler NewNodeAsset;
	if (!TestHelpers::TryCreateNewNodeAsset(this, NewNodeAsset, USMStateInstance::StaticClass(), false))
	{
		return false;
	}
	
	USMNodeBlueprint* NewNodeBP = NewNodeAsset.GetObjectAs<USMNodeBlueprint>();

	auto GetNodeInstanceClass = [NewNodeBP]()
	{
		USMNodeInstance* NodeInstance = CastChecked<USMNodeInstance>(NewNodeBP->GeneratedClass->GetDefaultObject());
		return NodeInstance->GetClass();
	};
	
	auto NodeHasConstructionScripts = [NewNodeBP](ESMExecutionEnvironment Environment)
	{
		USMNodeInstance* NodeInstance = CastChecked<USMNodeInstance>(NewNodeBP->GeneratedClass->GetDefaultObject());

		const FString FieldName = Environment == ESMExecutionEnvironment::EditorExecution ? "bHasEditorConstructionScripts" : "bHasGameConstructionScripts";
		
		FBoolProperty* HasConstructionScriptsProperty = FindFProperty<FBoolProperty>(NodeInstance->GetClass(), *FieldName);
        check(HasConstructionScriptsProperty);

        uint8* CDOContainer = HasConstructionScriptsProperty->ContainerPtrToValuePtr<uint8>(NodeInstance);
        return HasConstructionScriptsProperty->GetPropertyValue(CDOContainer);
	};
	
	UEdGraph* ConstructionScriptGraph = nullptr;
	for (const TObjectPtr<UEdGraph>& FunctionGraph : NewNodeBP->FunctionGraphs)
	{
		if (FunctionGraph->GetFName() == USMNodeInstance::GetConstructionScriptFunctionName())
		{
			ConstructionScriptGraph = FunctionGraph;
			break;
		}
	}

	check(ConstructionScriptGraph);

	// Find and verify default nodes.
	// FunctionEntry -> Parent -> ExecutionEnvironment
	
	UK2Node_FunctionEntry* EntryNode = FSMBlueprintEditorUtils::GetFirstNodeOfClassNested<UK2Node_FunctionEntry>(ConstructionScriptGraph);
	check(EntryNode);

	UEdGraphPin* EntryThenPin = EntryNode->FindPinChecked(UEdGraphSchema_K2::PN_Then);
	check(EntryThenPin->LinkedTo.Num() == 1);

	UK2Node_CallParentFunction* ParentCall = CastChecked<UK2Node_CallParentFunction>(EntryThenPin->LinkedTo[0]->GetOwningNode());
	UK2Node_CallFunction* ExecutionEnvironmentFunction = CastChecked<UK2Node_CallFunction>(ParentCall->GetThenPin()->LinkedTo[0]->GetOwningNode());

	UEdGraphPin* EditorExecutionPin = ExecutionEnvironmentFunction->FindPinChecked(TEXT("EditorExecution"), EGPD_Output);
	UEdGraphPin* GameExecutionPin = ExecutionEnvironmentFunction->FindPinChecked(TEXT("GameExecution"), EGPD_Output);
	
	TestEqual("No connections from editor execution", EditorExecutionPin->LinkedTo.Num(), 0);
	TestEqual("No connections from game execution", GameExecutionPin->LinkedTo.Num(), 0);
	
	// Test default behavior, should be optimized.
	{
		const bool bHasEditorConstructionScripts = FSMNodeInstanceUtils::DoesNodeClassPossiblyHaveConstructionScripts(GetNodeInstanceClass(), ESMExecutionEnvironment::EditorExecution);
		TestFalse("No construction scripts for default behavior", bHasEditorConstructionScripts);
		TestFalse("No construction scripts for default behavior", NodeHasConstructionScripts(ESMExecutionEnvironment::EditorExecution));
		
		const bool bHasGameConstructionScripts = FSMNodeInstanceUtils::DoesNodeClassPossiblyHaveConstructionScripts(GetNodeInstanceClass(), ESMExecutionEnvironment::GameExecution);
		TestFalse("No construction scripts for default behavior", bHasGameConstructionScripts);
		TestFalse("No construction scripts for default behavior", NodeHasConstructionScripts(ESMExecutionEnvironment::GameExecution));
	}

	// Function to use test optimizations.
	UFunction* DummyFunction = USMNodeInstance::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMNodeInstance, EvaluateGraphProperties));
	check(DummyFunction);

	// Test from environment pins.
	UEdGraphNode* CreatedFunctionNode = nullptr;
	check(FSMBlueprintEditorUtils::PlaceFunctionOnGraph(ConstructionScriptGraph, DummyFunction, nullptr, &CreatedFunctionNode, nullptr, 256.f, 48.f))

	UK2Node* CreatedK2Node = CastChecked<UK2Node>(CreatedFunctionNode);
	// 1 editor 0 game
	{
		ConstructionScriptGraph->GetSchema()->TryCreateConnection(EditorExecutionPin, CreatedK2Node->GetExecPin());
		FKismetEditorUtilities::CompileBlueprint(NewNodeBP);
		
		const bool bHasEditorConstructionScripts = FSMNodeInstanceUtils::DoesNodeClassPossiblyHaveConstructionScripts(GetNodeInstanceClass(), ESMExecutionEnvironment::EditorExecution);
		TestTrue("Has construction scripts", bHasEditorConstructionScripts);
		TestTrue("Has construction scripts", NodeHasConstructionScripts(ESMExecutionEnvironment::EditorExecution));

		const bool bHasGameConstructionScripts = FSMNodeInstanceUtils::DoesNodeClassPossiblyHaveConstructionScripts(GetNodeInstanceClass(), ESMExecutionEnvironment::GameExecution);
		TestFalse("No construction scripts", bHasGameConstructionScripts);
		TestFalse("No construction scripts", NodeHasConstructionScripts(ESMExecutionEnvironment::GameExecution));
	}
	
	// 1 editor 1 game
	{
		ConstructionScriptGraph->GetSchema()->TryCreateConnection(GameExecutionPin, CreatedK2Node->GetExecPin());
		FKismetEditorUtilities::CompileBlueprint(NewNodeBP);
		
		const bool bHasEditorConstructionScripts = FSMNodeInstanceUtils::DoesNodeClassPossiblyHaveConstructionScripts(GetNodeInstanceClass(), ESMExecutionEnvironment::EditorExecution);
		TestTrue("Has construction scripts", bHasEditorConstructionScripts);
		TestTrue("Has construction scripts", NodeHasConstructionScripts(ESMExecutionEnvironment::EditorExecution));

		const bool bHasGameConstructionScripts = FSMNodeInstanceUtils::DoesNodeClassPossiblyHaveConstructionScripts(GetNodeInstanceClass(), ESMExecutionEnvironment::GameExecution);
		TestTrue("Has construction scripts", bHasGameConstructionScripts);
		TestTrue("Has construction scripts", NodeHasConstructionScripts(ESMExecutionEnvironment::GameExecution));
	}

	// 0 editor 1 game
	{
		EditorExecutionPin->BreakAllPinLinks();
		FKismetEditorUtilities::CompileBlueprint(NewNodeBP);
		
		const bool bHasEditorConstructionScripts = FSMNodeInstanceUtils::DoesNodeClassPossiblyHaveConstructionScripts(GetNodeInstanceClass(), ESMExecutionEnvironment::EditorExecution);
		TestFalse("No construction scripts", bHasEditorConstructionScripts);
		TestFalse("No construction scripts", NodeHasConstructionScripts(ESMExecutionEnvironment::EditorExecution));

		const bool bHasGameConstructionScripts = FSMNodeInstanceUtils::DoesNodeClassPossiblyHaveConstructionScripts(GetNodeInstanceClass(), ESMExecutionEnvironment::GameExecution);
		TestTrue("Has construction scripts", bHasGameConstructionScripts);
		TestTrue("Has construction scripts", NodeHasConstructionScripts(ESMExecutionEnvironment::GameExecution));
	}

	// Test from parent function.
	{
		ParentCall->GetThenPin()->BreakAllPinLinks();

		// No pins -- optimized.
		{
			FKismetEditorUtilities::CompileBlueprint(NewNodeBP);
			
			const bool bHasEditorConstructionScripts = FSMNodeInstanceUtils::DoesNodeClassPossiblyHaveConstructionScripts(GetNodeInstanceClass(), ESMExecutionEnvironment::EditorExecution);
			TestFalse("No construction scripts for default behavior", bHasEditorConstructionScripts);
			TestFalse("No construction scripts for default behavior", NodeHasConstructionScripts(ESMExecutionEnvironment::EditorExecution));
			
			const bool bHasGameConstructionScripts = FSMNodeInstanceUtils::DoesNodeClassPossiblyHaveConstructionScripts(GetNodeInstanceClass(), ESMExecutionEnvironment::GameExecution);
			TestFalse("No construction scripts for default behavior", bHasGameConstructionScripts);
			TestFalse("No construction scripts for default behavior", NodeHasConstructionScripts(ESMExecutionEnvironment::GameExecution));
		}

		// Connected to a non environment function, all scripts present.
		{
			ConstructionScriptGraph->GetSchema()->TryCreateConnection(ParentCall->GetThenPin(), CreatedK2Node->GetExecPin());
			FKismetEditorUtilities::CompileBlueprint(NewNodeBP);

			const bool bHasEditorConstructionScripts = FSMNodeInstanceUtils::DoesNodeClassPossiblyHaveConstructionScripts(GetNodeInstanceClass(), ESMExecutionEnvironment::EditorExecution);
			TestTrue("Has construction scripts", bHasEditorConstructionScripts);
			TestTrue("Has construction scripts", NodeHasConstructionScripts(ESMExecutionEnvironment::EditorExecution));

			const bool bHasGameConstructionScripts = FSMNodeInstanceUtils::DoesNodeClassPossiblyHaveConstructionScripts(GetNodeInstanceClass(), ESMExecutionEnvironment::GameExecution);
			TestTrue("Has construction scripts", bHasGameConstructionScripts);
			TestTrue("Has construction scripts", NodeHasConstructionScripts(ESMExecutionEnvironment::GameExecution));
		}
	}

	// Test from entry function.
	{
		EntryThenPin->BreakAllPinLinks();

		// No pins -- optimized.
		{
			FKismetEditorUtilities::CompileBlueprint(NewNodeBP);
			
			const bool bHasEditorConstructionScripts = FSMNodeInstanceUtils::DoesNodeClassPossiblyHaveConstructionScripts(GetNodeInstanceClass(), ESMExecutionEnvironment::EditorExecution);
			TestFalse("No construction scripts for default behavior", bHasEditorConstructionScripts);
			TestFalse("No construction scripts for default behavior", NodeHasConstructionScripts(ESMExecutionEnvironment::EditorExecution));
			
			const bool bHasGameConstructionScripts = FSMNodeInstanceUtils::DoesNodeClassPossiblyHaveConstructionScripts(GetNodeInstanceClass(), ESMExecutionEnvironment::GameExecution);
			TestFalse("No construction scripts for default behavior", bHasGameConstructionScripts);
			TestFalse("No construction scripts for default behavior", NodeHasConstructionScripts(ESMExecutionEnvironment::GameExecution));
		}

		// Connected to a non environment function, all scripts present.
		{
			ConstructionScriptGraph->GetSchema()->TryCreateConnection(EntryThenPin, CreatedK2Node->GetExecPin());
			FKismetEditorUtilities::CompileBlueprint(NewNodeBP);

			const bool bHasEditorConstructionScripts = FSMNodeInstanceUtils::DoesNodeClassPossiblyHaveConstructionScripts(GetNodeInstanceClass(), ESMExecutionEnvironment::EditorExecution);
			TestTrue("Has construction scripts", bHasEditorConstructionScripts);
			TestTrue("Has construction scripts", NodeHasConstructionScripts(ESMExecutionEnvironment::EditorExecution));

			const bool bHasGameConstructionScripts = FSMNodeInstanceUtils::DoesNodeClassPossiblyHaveConstructionScripts(GetNodeInstanceClass(), ESMExecutionEnvironment::GameExecution);
			TestTrue("Has construction scripts", bHasGameConstructionScripts);
			TestTrue("Has construction scripts", NodeHasConstructionScripts(ESMExecutionEnvironment::GameExecution));
		}
	}

	return true;
}

/**
 * Check construction script behavior when loading an asset using standard behavior.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNodeInstanceConstructionScriptOnLoadTest, "LogicDriver.ConstructionScript.OnLoad", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FNodeInstanceConstructionScriptOnLoadTest::RunTest(const FString& Parameters)
{
	TestFalse("No pending construction scripts", ISMEditorConstructionManager::Get().HasPendingConstructionScripts());

	USMProjectEditorSettings* Settings = FSMBlueprintEditorUtils::GetMutableProjectEditorSettings();
	const auto CurrentCSSetting = Settings->EditorNodeConstructionScriptSetting;
	Settings->EditorNodeConstructionScriptSetting = ESMEditorConstructionScriptProjectSetting::SM_Standard;

	SETUP_NEW_STATE_MACHINE_FOR_TEST(2);

	const int32 NumPasses = NewBP->NumConstructionScriptPasses;

	USMGraphNode_StateNode* InitialStateGraphNode;
	USMStateConstructionTestInstance* InitialStateGraphNodeNodeInstance;

	int32 ConstructionScriptTimesPreviouslyRan = 0;
	// Initial construction script tests from setting up a new asset.
	{
		UEdGraphPin* LastStatePin = nullptr;
		TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMStateConstructionTestInstance::StaticClass(), USMTransitionConstructionTestInstance::StaticClass());

		InitialStateGraphNode = CastChecked<USMGraphNode_StateNode>(CastChecked<USMGraphNode_StateNode>(LastStatePin->GetOwningNode())->GetPreviousState());

		InitialStateGraphNodeNodeInstance = CastChecked<USMStateConstructionTestInstance>(InitialStateGraphNode->GetNodeTemplate());
		TestEqual("Editor construction has not run yet", InitialStateGraphNodeNodeInstance->ConstructionScriptHit.Count, 0);
		TestTrue("Has pending construction scripts", ISMEditorConstructionManager::Get().HasPendingConstructionScripts());

		static_cast<FSMEditorConstructionManager&>(ISMEditorConstructionManager::Get()).Tick(0.f);

		TestEqual("Editor construction script ran", InitialStateGraphNodeNodeInstance->ConstructionScriptHit.Count, ConstructionScriptTimesPreviouslyRan + NumPasses);
		TestFalse("No pending construction scripts", ISMEditorConstructionManager::Get().HasPendingConstructionScripts());
	}

	NewAsset.SaveAsset(this);

	// Reload the package and verify construction was triggered.
	NewAsset.ReloadAsset(this);
	TestTrue("Has pending construction scripts", ISMEditorConstructionManager::Get().HasPendingConstructionScripts());

	NewBP = NewAsset.GetObjectAs<USMBlueprint>();
	TestFalse("Package not dirty", NewAsset.Package->IsDirty());
	TestFalse("Asset not dirty", NewBP->IsPossiblyDirty());

	Settings->EditorNodeConstructionScriptSetting = CurrentCSSetting;

	return NewAsset.DeleteAsset(this);
}

/**
 * Check construction script behavior when using standard behavior.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNodeInstanceConstructionScriptStandardTest, "LogicDriver.ConstructionScript.Standard", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FNodeInstanceConstructionScriptStandardTest::RunTest(const FString& Parameters)
{
	TestFalse("No pending construction scripts", ISMEditorConstructionManager::Get().HasPendingConstructionScripts());
	
	USMProjectEditorSettings* Settings = FSMBlueprintEditorUtils::GetMutableProjectEditorSettings();
	const auto CurrentCSSetting = Settings->EditorNodeConstructionScriptSetting;
	Settings->EditorNodeConstructionScriptSetting = ESMEditorConstructionScriptProjectSetting::SM_Standard;
	
	SETUP_NEW_STATE_MACHINE_FOR_TEST(2);
	const int32 NumPasses = NewBP->NumConstructionScriptPasses;
	
	USMGraphNode_StateNode* InitialStateGraphNode;
	USMStateConstructionTestInstance* InitialStateGraphNodeNodeInstance;

	auto CalculatedVal = [](int32 Suffix, const FString& Prefix = "Test_") ->FString
	{
		return Prefix + FString::FromInt(Suffix);
	};

	int32 ConstructionScriptTimesPreviouslyRan = 0;
	{
		UEdGraphPin* LastStatePin = nullptr;
		//Verify default instances load correctly.
		TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMStateConstructionTestInstance::StaticClass(), USMTransitionConstructionTestInstance::StaticClass());

		InitialStateGraphNode = CastChecked<USMGraphNode_StateNode>(CastChecked<USMGraphNode_StateNode>(LastStatePin->GetOwningNode())->GetPreviousState());

		const FStateStackContainer NewStateStackText(USMStateConstructionTestInstance::StaticClass());
		InitialStateGraphNode->StateStack.Add(NewStateStackText);
		InitialStateGraphNode->InitStateStack();
		InitialStateGraphNode->CreateGraphPropertyGraphs();
		
		InitialStateGraphNodeNodeInstance = CastChecked<USMStateConstructionTestInstance>(InitialStateGraphNode->GetNodeTemplate());
		TestEqual("Editor construction has not run yet", InitialStateGraphNodeNodeInstance->ConstructionScriptHit.Count, 0);

		TestTrue("Has pending construction scripts", ISMEditorConstructionManager::Get().HasPendingConstructionScripts());
		
		static_cast<FSMEditorConstructionManager&>(ISMEditorConstructionManager::Get()).Tick(0.f);

		// Stack
		{
			USMStateConstructionTestInstance* StackTemplate = CastChecked<USMStateConstructionTestInstance>(InitialStateGraphNode->GetTemplateFromIndex(0));
			TestTrue("Editor construction script ran", StackTemplate->ConstructionScriptHit.Count > ConstructionScriptTimesPreviouslyRan);
			TestEqual("Outgoing states found", StackTemplate->CanReadNextStates, 1);
		}
		
		TestEqual("Editor construction script ran", InitialStateGraphNodeNodeInstance->ConstructionScriptHit.Count, ConstructionScriptTimesPreviouslyRan + NumPasses);
		ConstructionScriptTimesPreviouslyRan = InitialStateGraphNodeNodeInstance->ConstructionScriptHit.Count;
		
		TestEqual("Outgoing states found", InitialStateGraphNodeNodeInstance->CanReadNextStates, 1);
		
		FKismetEditorUtilities::CompileBlueprint(NewBP);  // 2nd construction script

		// Stack
		{
			USMStateConstructionTestInstance* StackTemplate = CastChecked<USMStateConstructionTestInstance>(InitialStateGraphNode->GetTemplateFromIndex(0));
			TestEqual("Editor construction script ran", StackTemplate->ConstructionScriptHit.Count, ConstructionScriptTimesPreviouslyRan + NumPasses);
			TestEqual("Outgoing states found", StackTemplate->CanReadNextStates, 1);
		}
		
		TestEqual("Editor construction script ran", InitialStateGraphNodeNodeInstance->ConstructionScriptHit.Count, ConstructionScriptTimesPreviouslyRan + NumPasses);
		ConstructionScriptTimesPreviouslyRan = InitialStateGraphNodeNodeInstance->ConstructionScriptHit.Count;
		
		USMInstance* TestInstance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, NewObject<USMTestContext>()); // 3rd construction script
		USMStateConstructionTestInstance* InitialNode = CastChecked<USMStateConstructionTestInstance>(TestInstance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());

		// Verify hit on initialize.
		TestEqual("Construction script run", InitialNode->SetByConstructionScript, CalculatedVal(0)); // initial.
		TestEqual("Construction script run", InitialNode->ConstructionScriptHit.Count, 1);

		TestEqual("Outgoing states found", InitialStateGraphNodeNodeInstance->CanReadNextStates, 1);
		
		TArray<USMTransitionInstance*> OutgoingTransitions;
		check(InitialNode->GetOutgoingTransitions(OutgoingTransitions));

		const int32 PriorityOrder = 5;
		
		USMTransitionConstructionTestInstance* TransitionNode = CastChecked<USMTransitionConstructionTestInstance>(OutgoingTransitions[0]);
		TestEqual("Construction script run", TransitionNode->ConstructionScriptHit.Count, 1);
		TestEqual("Priority Set", TransitionNode->GetPriorityOrder(), PriorityOrder);
		TestEqual("Node Priority Set", TransitionNode->GetOwningNodeAs<FSMTransition>()->Priority, PriorityOrder);

		// Compile here so compile isn't triggered twice in RunStateMachineToCompletion.
		FKismetEditorUtilities::CompileBlueprint(NewBP);
		int32 A, B, C;
		TestInstance = TestHelpers::RunStateMachineToCompletion(this, NewBP, A, B, C, 1000, true, true, false);

		// Verify values unchanged.
		InitialNode = CastChecked<USMStateConstructionTestInstance>(TestInstance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
		TestEqual("Construction script run", InitialNode->SetByConstructionScript, CalculatedVal(0));
		TestEqual("Construction script run", InitialNode->ConstructionScriptHit.Count, 1);

		check(InitialNode->GetOutgoingTransitions(OutgoingTransitions));
		
		TransitionNode = CastChecked<USMTransitionConstructionTestInstance>(OutgoingTransitions[0]);
		TestEqual("Construction script run", TransitionNode->ConstructionScriptHit.Count, 1);
		TestEqual("Priority Set", TransitionNode->GetPriorityOrder(), PriorityOrder);
		TestEqual("Node Priority Set", TransitionNode->GetOwningNodeAs<FSMTransition>()->Priority, PriorityOrder);
	}
	
	// Verify pin value for editor state machine next states updated.
	{
		USMGraphK2Node_PropertyNode_Base* GraphPropertyReadNode = InitialStateGraphNode->GetGraphPropertyNode(
			GET_MEMBER_NAME_CHECKED(USMStateConstructionTestInstance, CanReadNextStates));
		check(GraphPropertyReadNode);

		UEdGraphPin* ResultPin = GraphPropertyReadNode->GetResultPinChecked();
		const FString DefaultValue = ResultPin->GetDefaultAsString();
		TestEqual("Default value changed by construction script", DefaultValue, CalculatedVal(1, ""));
	}
	
	// Verify the pin value has updated.
	{
		USMGraphK2Node_PropertyNode_Base* GraphPropertyReadNode = InitialStateGraphNode->GetGraphPropertyNode(
			GET_MEMBER_NAME_CHECKED(USMStateConstructionTestInstance, SetByConstructionScript));
		check(GraphPropertyReadNode);

		UEdGraphPin* ResultPin = GraphPropertyReadNode->GetResultPinChecked();
		FString DefaultValue = ResultPin->GetDefaultAsString();
		TestEqual("Default value changed by construction script", DefaultValue, CalculatedVal(0));

		USMGraphK2Node_PropertyNode_Base* GraphPropertyWriteNode = InitialStateGraphNode->GetGraphPropertyNode(
			GET_MEMBER_NAME_CHECKED(USMStateConstructionTestInstance, ExposedInt));
		check(GraphPropertyReadNode);

		{
			// Verify setting a new write value updates the read value from the construction script.

			int32 NewValue = 5001;

			const UEdGraphSchema_K2* Schema = CastChecked<UEdGraphSchema_K2>(GraphPropertyWriteNode->GetSchema());
			Schema->TrySetDefaultValue(*GraphPropertyWriteNode->GetResultPinChecked(), FString::FromInt(NewValue)); // 4th construction script

			TestEqual("Editor construction script ran after pin modify, stayed the same value because of property reset", InitialStateGraphNodeNodeInstance->ConstructionScriptHit.Count, ConstructionScriptTimesPreviouslyRan + NumPasses);
			ConstructionScriptTimesPreviouslyRan = InitialStateGraphNodeNodeInstance->ConstructionScriptHit.Count;

			TestEqual("PostEditChange Fired", InitialStateGraphNodeNodeInstance->PostEditChangeHit.Count, 1);
			
			static_cast<FSMEditorConstructionManager&>(ISMEditorConstructionManager::Get()).Tick(0.f);
			
			// Tick regenerated the graphs.
			GraphPropertyReadNode = InitialStateGraphNode->GetGraphPropertyNode(
				GET_MEMBER_NAME_CHECKED(USMStateConstructionTestInstance, SetByConstructionScript));
			check(GraphPropertyReadNode);

			ResultPin = GraphPropertyReadNode->GetResultPinChecked();
			
			DefaultValue = ResultPin->GetDefaultAsString();
			TestEqual("Default value changed by construction script", DefaultValue, CalculatedVal(NewValue));

			// Test run-time, compile here so compile isn't triggered twice in RunStateMachineToCompletion.
			FKismetEditorUtilities::CompileBlueprint(NewBP);
			int32 A, B, C;
			USMInstance* TestInstance = TestHelpers::RunStateMachineToCompletion(this, NewBP, A, B, C, 1000, true, true, false);

			USMStateConstructionTestInstance* InitialNode = CastChecked<USMStateConstructionTestInstance>(TestInstance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
			TestEqual("Construction script run", InitialNode->SetByConstructionScript, CalculatedVal(NewValue));
			TestEqual("Construction script run", InitialNode->ConstructionScriptHit.Count, 1);
		}
	}

	Settings->EditorNodeConstructionScriptSetting = CurrentCSSetting;
	
	return true;
}

/**
 * Check construction script behavior when using compile only behavior.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNodeInstanceConstructionScriptCompileTest, "LogicDriver.ConstructionScript.Compile", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FNodeInstanceConstructionScriptCompileTest::RunTest(const FString& Parameters)
{
	USMProjectEditorSettings* Settings = FSMBlueprintEditorUtils::GetMutableProjectEditorSettings();
	const auto CurrentCSSetting = Settings->EditorNodeConstructionScriptSetting;
	Settings->EditorNodeConstructionScriptSetting = ESMEditorConstructionScriptProjectSetting::SM_Compile;
	
	SETUP_NEW_STATE_MACHINE_FOR_TEST(2);
	const int32 NumPasses = NewBP->NumConstructionScriptPasses;
	
	USMGraphNode_StateNode* InitialStateGraphNode;
	USMStateConstructionTestInstance* InitialStateGraphNodeNodeInstance;
	
	auto CalculatedVal = [](int32 Suffix) ->FString
	{
		return "Test_" + FString::FromInt(Suffix);
	};

	int32 ConstructionScriptTimesPreviouslyRan = 0;
	{
		UEdGraphPin* LastStatePin = nullptr;
		//Verify default instances load correctly.
		TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMStateConstructionTestInstance::StaticClass(), USMTransitionConstructionTestInstance::StaticClass());

		InitialStateGraphNode = CastChecked<USMGraphNode_StateNode>(CastChecked<USMGraphNode_StateNode>(LastStatePin->GetOwningNode())->GetPreviousState());

		InitialStateGraphNodeNodeInstance = CastChecked<USMStateConstructionTestInstance>(InitialStateGraphNode->GetNodeTemplate());
		TestEqual("Editor construction has not run yet", InitialStateGraphNodeNodeInstance->ConstructionScriptHit.Count, 0);

		static_cast<FSMEditorConstructionManager&>(ISMEditorConstructionManager::Get()).Tick(0.f);
		
		TestEqual("Editor construction has not run yet", InitialStateGraphNodeNodeInstance->ConstructionScriptHit.Count, 0);
		ConstructionScriptTimesPreviouslyRan = InitialStateGraphNodeNodeInstance->ConstructionScriptHit.Count;
		
		FKismetEditorUtilities::CompileBlueprint(NewBP);  // 2nd construction script

		TestEqual("Editor construction script ran after compile, value increased", InitialStateGraphNodeNodeInstance->ConstructionScriptHit.Count, ConstructionScriptTimesPreviouslyRan + NumPasses);
		ConstructionScriptTimesPreviouslyRan = InitialStateGraphNodeNodeInstance->ConstructionScriptHit.Count;
		
		USMInstance* TestInstance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, NewObject<USMTestContext>()); // 3rd construction script
		USMStateConstructionTestInstance* InitialNode = CastChecked<USMStateConstructionTestInstance>(TestInstance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());

		// Verify hit on initialize.
		TestEqual("Construction script run", InitialNode->SetByConstructionScript, CalculatedVal(0)); // initial.
		TestEqual("Construction script run", InitialNode->ConstructionScriptHit.Count, 1);
		
		TArray<USMTransitionInstance*> OutgoingTransitions;
		check(InitialNode->GetOutgoingTransitions(OutgoingTransitions));

		const int32 PriorityOrder = 5;
		
		USMTransitionConstructionTestInstance* TransitionNode = CastChecked<USMTransitionConstructionTestInstance>(OutgoingTransitions[0]);
		TestEqual("Construction script run", TransitionNode->ConstructionScriptHit.Count, 1);
		TestEqual("Priority Set", TransitionNode->GetPriorityOrder(), PriorityOrder);
		TestEqual("Node Priority Set", TransitionNode->GetOwningNodeAs<FSMTransition>()->Priority, PriorityOrder);
		
		int32 A, B, C;
		TestInstance = TestHelpers::RunStateMachineToCompletion(this, NewBP, A, B, C);

		// Verify values unchanged.
		InitialNode = CastChecked<USMStateConstructionTestInstance>(TestInstance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
		TestEqual("Construction script run", InitialNode->SetByConstructionScript, CalculatedVal(0));
		TestEqual("Construction script run", InitialNode->ConstructionScriptHit.Count, 1);

		check(InitialNode->GetOutgoingTransitions(OutgoingTransitions));
		
		TransitionNode = CastChecked<USMTransitionConstructionTestInstance>(OutgoingTransitions[0]);
		TestEqual("Construction script run", TransitionNode->ConstructionScriptHit.Count, 1);
		TestEqual("Priority Set", TransitionNode->GetPriorityOrder(), PriorityOrder);
		TestEqual("Node Priority Set", TransitionNode->GetOwningNodeAs<FSMTransition>()->Priority, PriorityOrder);
	}

	// Verify the pin value has updated.
	
	USMGraphK2Node_PropertyNode_Base* GraphPropertyReadNode = InitialStateGraphNode->GetGraphPropertyNode(
		GET_MEMBER_NAME_CHECKED(USMStateConstructionTestInstance, SetByConstructionScript));
	check(GraphPropertyReadNode);

	UEdGraphPin* ResultPin = GraphPropertyReadNode->GetResultPinChecked();
	FString DefaultValue = ResultPin->GetDefaultAsString();
	TestEqual("Default value changed by construction script", DefaultValue, CalculatedVal(0));

	USMGraphK2Node_PropertyNode_Base* GraphPropertyWriteNode = InitialStateGraphNode->GetGraphPropertyNode(
		GET_MEMBER_NAME_CHECKED(USMStateConstructionTestInstance, ExposedInt));
	check(GraphPropertyReadNode);

	{
		// Verify setting a new write value updates the read value from the construction script.
		
		int32 NewValue = 5001;

		ConstructionScriptTimesPreviouslyRan = InitialStateGraphNodeNodeInstance->ConstructionScriptHit.Count;
		
		const UEdGraphSchema_K2* Schema = CastChecked<UEdGraphSchema_K2>(GraphPropertyWriteNode->GetSchema());
		Schema->TrySetDefaultValue(*GraphPropertyWriteNode->GetResultPinChecked(), FString::FromInt(NewValue)); // 4th construction script

		TestTrue("Editor construction script ran after pin modify, stayed the same value", InitialStateGraphNodeNodeInstance->ConstructionScriptHit.Count == ConstructionScriptTimesPreviouslyRan);
		ConstructionScriptTimesPreviouslyRan = InitialStateGraphNodeNodeInstance->ConstructionScriptHit.Count;
		TestEqual("PostEditChange Fired", InitialStateGraphNodeNodeInstance->PostEditChangeHit.Count, 1);
		
		static_cast<FSMEditorConstructionManager&>(ISMEditorConstructionManager::Get()).Tick(0.f);
		
		DefaultValue = ResultPin->GetDefaultAsString();
		TestNotEqual("Default value not changed by construction script yet", DefaultValue, CalculatedVal(NewValue));

		FKismetEditorUtilities::CompileBlueprint(NewBP);

		ResultPin = GraphPropertyReadNode->GetResultPinChecked();
		
		DefaultValue = ResultPin->GetDefaultAsString();
		TestEqual("Default value changed by construction script yet", DefaultValue, CalculatedVal(NewValue));
		
		// Test run-time
		int32 A, B, C;
		USMInstance* TestInstance = TestHelpers::RunStateMachineToCompletion(this, NewBP, A, B, C);

		USMStateConstructionTestInstance* InitialNode = CastChecked<USMStateConstructionTestInstance>(TestInstance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
		TestEqual("Construction script run", InitialNode->SetByConstructionScript, CalculatedVal(NewValue));
		TestEqual("Construction script run", InitialNode->ConstructionScriptHit.Count, 1);
	}

	Settings->EditorNodeConstructionScriptSetting = CurrentCSSetting;
	
	return true;
}

/**
 * Check construction script behavior when using legacy behavior.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNodeInstanceConstructionScriptLegacyTest, "LogicDriver.ConstructionScript.Legacy", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FNodeInstanceConstructionScriptLegacyTest::RunTest(const FString& Parameters)
{
	USMProjectEditorSettings* Settings = FSMBlueprintEditorUtils::GetMutableProjectEditorSettings();
	const auto CurrentCSSetting = Settings->EditorNodeConstructionScriptSetting;
	Settings->EditorNodeConstructionScriptSetting = ESMEditorConstructionScriptProjectSetting::SM_Legacy;
	
	SETUP_NEW_STATE_MACHINE_FOR_TEST(1);

	USMGraphNode_StateNode* InitialStateGraphNode;
	USMStateConstructionTestInstance* InitialStateGraphNodeNodeInstance;

	auto CalculatedVal = [](int32 Suffix) ->FString
	{
		return "Test_" + FString::FromInt(Suffix);
	};

	{
		UEdGraphPin* LastStatePin = nullptr;
		//Verify default instances load correctly.
		TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMStateConstructionTestInstance::StaticClass(), USMTransitionInstance::StaticClass());

		InitialStateGraphNode = CastChecked<USMGraphNode_StateNode>(LastStatePin->GetOwningNode());

		InitialStateGraphNodeNodeInstance = CastChecked<USMStateConstructionTestInstance>(InitialStateGraphNode->GetNodeTemplate());
		TestEqual("Editor construction script ran once", InitialStateGraphNodeNodeInstance->ConstructionScriptHit.Count, 1);

		FKismetEditorUtilities::CompileBlueprint(NewBP);

		TestEqual("Editor construction script did not run because of legacy settings", InitialStateGraphNodeNodeInstance->ConstructionScriptHit.Count, 1);
		
		USMInstance* TestInstance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, NewObject<USMTestContext>());
		USMStateConstructionTestInstance* InitialNode = CastChecked<USMStateConstructionTestInstance>(TestInstance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());

		// Verify hit on initialize.
		TestEqual("Construction script not run", InitialNode->SetByConstructionScript, CalculatedVal(0)); // initial.
		TestEqual("Construction script not run", InitialNode->ConstructionScriptHit.Count, 1);

		int32 A, B, C;
		TestInstance = TestHelpers::RunStateMachineToCompletion(this, NewBP, A, B, C, 1000, false);

		// Verify values unchanged.
		InitialNode = CastChecked<USMStateConstructionTestInstance>(TestInstance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
		TestEqual("Construction script not run.", InitialNode->SetByConstructionScript, CalculatedVal(0)); // May change if we change how default graph vals executes.
		TestEqual("Construction script not run", InitialNode->ConstructionScriptHit.Count, 1);
	}

	// Verify the pin value has updated.
	
	USMGraphK2Node_PropertyNode_Base* GraphPropertyReadNode = InitialStateGraphNode->GetGraphPropertyNode(
		GET_MEMBER_NAME_CHECKED(USMStateConstructionTestInstance, SetByConstructionScript));
	check(GraphPropertyReadNode);

	UEdGraphPin* ResultPin = GraphPropertyReadNode->GetResultPinChecked();
	FString DefaultValue = ResultPin->GetDefaultAsString();
	TestEqual("Default value not changed by legacy construction script", DefaultValue, CalculatedVal(0));

	USMGraphK2Node_PropertyNode_Base* GraphPropertyWriteNode = InitialStateGraphNode->GetGraphPropertyNode(
		GET_MEMBER_NAME_CHECKED(USMStateConstructionTestInstance, ExposedInt));
	check(GraphPropertyReadNode);

	{
		// Verify setting a new write value updates the read value from the construction script.
		
		int32 NewValue = 5001;

		const UEdGraphSchema_K2* Schema = CastChecked<UEdGraphSchema_K2>(GraphPropertyWriteNode->GetSchema());
		Schema->TrySetDefaultValue(*GraphPropertyWriteNode->GetResultPinChecked(), FString::FromInt(NewValue));

		TestEqual("Editor construction script did not run after pin modify because of legacy, stayed the same value because of property reset", InitialStateGraphNodeNodeInstance->ConstructionScriptHit.Count, 1);
		TestEqual("PostEditChange Fired", InitialStateGraphNodeNodeInstance->PostEditChangeHit.Count, 1);
		
		DefaultValue = ResultPin->GetDefaultAsString();
		TestEqual("Default value not changed by legacy construction script", DefaultValue, CalculatedVal(0));

		// Test run-time
		int32 A, B, C;
		USMInstance* TestInstance = TestHelpers::RunStateMachineToCompletion(this, NewBP, A, B, C);

		USMStateConstructionTestInstance* InitialNode = CastChecked<USMStateConstructionTestInstance>(TestInstance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
		TestEqual("Construction script not run", InitialNode->SetByConstructionScript, CalculatedVal(0));
		TestEqual("Construction script not run", InitialNode->ConstructionScriptHit.Count, 1);
	}

	Settings->EditorNodeConstructionScriptSetting = CurrentCSSetting;
	
	return true;
}

/**
 * Modify the state stack during construction.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNodeInstanceConstructionScriptStateStackTest, "LogicDriver.ConstructionScript.ModifyStateStack", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FNodeInstanceConstructionScriptStateStackTest::RunTest(const FString& Parameters)
{
	TestFalse("No pending construction scripts", ISMEditorConstructionManager::Get().HasPendingConstructionScripts());

	USMProjectEditorSettings* Settings = FSMBlueprintEditorUtils::GetMutableProjectEditorSettings();
	const auto CurrentCSSetting = Settings->EditorNodeConstructionScriptSetting;
	Settings->EditorNodeConstructionScriptSetting = ESMEditorConstructionScriptProjectSetting::SM_Standard;

	SETUP_NEW_STATE_MACHINE_FOR_TEST(1);
	const int32 NumPasses = NewBP->NumConstructionScriptPasses;

	UEdGraphPin* LastStatePin = nullptr;
	//Verify default instances load correctly.
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMStateStackConstructionTestInstance::StaticClass(), USMTransitionConstructionTestInstance::StaticClass());

	const USMGraphNode_StateNode* InitialStateGraphNode = CastChecked<USMGraphNode_StateNode>(
		CastChecked<USMGraphNode_StateNode>(LastStatePin->GetOwningNode()));

	USMStateStackConstructionTestInstance* InitialStateGraphNodeNodeInstance = CastChecked<USMStateStackConstructionTestInstance>(
		InitialStateGraphNode->GetNodeTemplate());
	TestEqual("Editor construction has not run yet", InitialStateGraphNodeNodeInstance->ConstructionScriptHit.Count, 0);

	TestTrue("Has pending construction scripts", ISMEditorConstructionManager::Get().HasPendingConstructionScripts());

	ISMEditorConstructionManager::Get().RunAllConstructionScriptsForBlueprintImmediately(NewBP);

	// Stack addition
	InitialStateGraphNodeNodeInstance->RemoveIndex = -2;

	TestEqual("3 elements in graph node", InitialStateGraphNode->StateStack.Num(), 3);
	{
		const USMStateConstructionTestInstance* StackTemplate = CastChecked<USMStateConstructionTestInstance>(InitialStateGraphNode->GetTemplateFromIndex(0));
		TestEqual("Second element inserted first", StackTemplate->NameSetByCreator, USMStateStackConstructionTestInstance::StackName2);
		TestTrue("Editor construction script ran", StackTemplate->ConstructionScriptHit.Count > 0);
	}
	{
		const USMStateConstructionTestInstance* StackTemplate = CastChecked<USMStateConstructionTestInstance>(InitialStateGraphNode->GetTemplateFromIndex(1));
		TestEqual("First element pushed to second", StackTemplate->NameSetByCreator, USMStateStackConstructionTestInstance::StackName1);
		TestTrue("Editor construction script ran", StackTemplate->ConstructionScriptHit.Count > 0);
	}
	{
		const USMStateConstructionTestInstance* StackTemplate = CastChecked<USMStateConstructionTestInstance>(InitialStateGraphNode->GetTemplateFromIndex(2));
		TestEqual("Third element added last", StackTemplate->NameSetByCreator, USMStateStackConstructionTestInstance::StackName3);
		TestTrue("Editor construction script ran", StackTemplate->ConstructionScriptHit.Count > 0);
	}

	// Test run-time has added state stacks
	{
		int32 A, B, C;
		const USMInstance* RuntimeInstance = TestHelpers::RunStateMachineToCompletion(this, NewBP, A, B, C, 10, false);

		TArray<USMStateInstance_Base*> StateInstances;
		RuntimeInstance->GetAllStateInstances(StateInstances);
		check(StateInstances.Num() > 0);

		USMStateInstance_Base** RuntimeNodeInstance = StateInstances.FindByPredicate(
			[](const USMStateInstance_Base* StateInstance)
			{
				return StateInstance->IsA<USMStateStackConstructionTestInstance>();
			});

		check(RuntimeNodeInstance);

		const USMStateStackConstructionTestInstance* RuntimeConstructionNode = CastChecked<USMStateStackConstructionTestInstance>(*RuntimeNodeInstance);
		TestEqual("Runtime state stack count correct", RuntimeConstructionNode->GetStateStackCount(), 3);

		TArray<USMStateInstance_Base*> StateStackInstances;
		RuntimeConstructionNode->GetAllStateStackInstances(StateStackInstances);

		check(StateStackInstances.Num() == 3);

		for (USMStateInstance_Base* StateInstance : StateStackInstances)
		{
			const USMStateConstructionTestInstance* ConstructionTestInstance = CastChecked<USMStateConstructionTestInstance>(StateInstance);
			TestEqual("State stack OnStateStart hit", ConstructionTestInstance->StateBeginHit.Count, 1);
		}
	}

	// Stack remove index
	InitialStateGraphNodeNodeInstance->RemoveIndex = 1;

	ISMEditorConstructionManager::Get().RunAllConstructionScriptsForBlueprintImmediately(NewBP);

	TestEqual("1 element removed from graph node", InitialStateGraphNode->StateStack.Num(), 2);

	{
		const USMStateConstructionTestInstance* StackTemplate = CastChecked<USMStateConstructionTestInstance>(InitialStateGraphNode->GetTemplateFromIndex(0));
		TestEqual("Second element inserted first", StackTemplate->NameSetByCreator, USMStateStackConstructionTestInstance::StackName2);
		TestTrue("Editor construction script ran", StackTemplate->ConstructionScriptHit.Count > 0);
	}
	{
		const USMStateConstructionTestInstance* StackTemplate = CastChecked<USMStateConstructionTestInstance>(InitialStateGraphNode->GetTemplateFromIndex(1));
		TestEqual("Third element added last", StackTemplate->NameSetByCreator, USMStateStackConstructionTestInstance::StackName3);
		TestTrue("Editor construction script ran", StackTemplate->ConstructionScriptHit.Count > 0);
	}

	// Stack remove all
	InitialStateGraphNodeNodeInstance->RemoveIndex = -1;

	ISMEditorConstructionManager::Get().RunAllConstructionScriptsForBlueprintImmediately(NewBP);
	TestEqual("All elements removed from graph node", InitialStateGraphNode->StateStack.Num(), 0);

	Settings->EditorNodeConstructionScriptSetting = CurrentCSSetting;

	return true;
}

/**
 * Test node can be renamed during construction scripts.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNodeInstanceConstructionScriptRenameNodeTest, "LogicDriver.ConstructionScript.RenameNode", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FNodeInstanceConstructionScriptRenameNodeTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(2)

	USMProjectEditorSettings* Settings = FSMBlueprintEditorUtils::GetMutableProjectEditorSettings();
	TGuardValue<ESMEditorConstructionScriptProjectSetting> CSSetting(Settings->EditorNodeConstructionScriptSetting, ESMEditorConstructionScriptProjectSetting::SM_Standard);
	
	UEdGraphPin* LastStatePin = nullptr;

	// lazy validation -- node classes will run construction scripts
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMTestRenameState::StaticClass(), USMTestRenameTransition::StaticClass());
	FKismetEditorUtilities::CompileBlueprint(NewBP);

	return true;
}

#endif

#endif //WITH_DEV_AUTOMATION_TESTS