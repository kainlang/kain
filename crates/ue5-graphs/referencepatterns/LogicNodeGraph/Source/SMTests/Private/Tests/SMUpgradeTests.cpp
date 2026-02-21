// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Blueprints/SMBlueprint.h"
#include "Blueprints/SMBlueprintGeneratedClass.h"

#include "Blueprints/SMBlueprintFactory.h"
#include "Configuration/SMProjectEditorSettings.h"
#include "Graph/Nodes/PropertyNodes/SMGraphK2Node_TextPropertyNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_ConduitResultNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_IntermediateEntryNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_IntermediateRootStateMachineNodes.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_StateEndNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_StateEntryNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_StateMachineSelectNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_StateUpdateNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_TransitionEnteredNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_TransitionInitializedNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_TransitionPostEvaluateNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_TransitionPreEvaluateNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_TransitionShutdownNode.h"
#include "Graph/Nodes/SMGraphK2Node_StateMachineNode.h"
#include "Graph/Nodes/SMGraphNode_ConduitNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineEntryNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineStateNode.h"
#include "Graph/Nodes/SMGraphNode_StateNodeBase.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/Schema/SMGraphK2Schema.h"
#include "Graph/SMConduitGraph.h"
#include "Graph/SMGraph.h"
#include "Graph/SMIntermediateGraph.h"
#include "Graph/SMStateGraph.h"
#include "Graph/SMTextPropertyGraph.h"
#include "SMTestContext.h"
#include "SMTestHelpers.h"
#include "Utilities/SMBlueprintEditorUtils.h"
#include "Utilities/SMVersionUtils.h"

#include "Kismet2/KismetEditorUtilities.h"

#if WITH_DEV_AUTOMATION_TESTS

#if PLATFORM_DESKTOP

/**
 * Validate old blueprints can be updated properly.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpdateBlueprintVersionTest, "LogicDriver.Upgrade.UpdateBlueprintVersion", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FUpdateBlueprintVersionTest::RunTest(const FString& Parameters)
{
	FAssetHandler NewSMAsset;
	if (!TestHelpers::TryCreateNewStateMachineAsset(this, NewSMAsset, false))
	{
		return false;
	}

	USMBlueprint* NewBP = NewSMAsset.GetObjectAs<USMBlueprint>();
	NewBP->AssetVersion = 0;
	{
		// Verify new version set correctly.
		TestFalse("Instance version is not correctly created", FSMVersionUtils::IsAssetUpToDate(NewBP));
		NewSMAsset.SaveAsset(this);
		TestFalse("Asset saved and not dirty", NewBP->GetOutermost()->IsDirty());
	}

	FAssetHandler NewNodeAsset;
	TestHelpers::TryCreateNewNodeAsset(this, NewNodeAsset, USMStateInstance::StaticClass(), false);
	USMNodeBlueprint* NewNodeBP = NewNodeAsset.GetObjectAs<USMNodeBlueprint>();
	NewNodeBP->AssetVersion = 0;
	{
		// Verify new version set correctly.
		TestFalse("Instance version is not correctly created", FSMVersionUtils::IsAssetUpToDate(NewNodeBP));
		NewNodeAsset.SaveAsset(this);
		TestFalse("Asset saved and not dirty", NewNodeBP->GetOutermost()->IsDirty());
	}
	
	FSMVersionUtils::UpdateBlueprintsToNewVersion();

	TestTrue("SM Asset dirty after update", NewBP->GetOutermost()->IsDirty());
	TestTrue("SM Asset up to date", FSMVersionUtils::IsAssetUpToDate(NewBP));

	TestTrue("Node Asset dirty after update", NewNodeBP->GetOutermost()->IsDirty());
	TestTrue("Node Asset up to date", FSMVersionUtils::IsAssetUpToDate(NewNodeBP));

	NewNodeAsset.DeleteAsset(this);
	return NewSMAsset.DeleteAsset(this);
}

/**
* Sanity checks for version calculations.
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVersionComparisonTest, "LogicDriver.Upgrade.VersionComparison", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FVersionComparisonTest::RunTest(const FString& Parameters)
{
	auto CreateVersion = [&](const FString& VersionName, int32 TestMajor, int32 TestMinor, int32 TestPatch, const FString& TestCommit = FString())
	{
		const FSMVersionUtils::FVersion Version(VersionName);
		TestEqual("Major", Version.Major, TestMajor);
		TestEqual("Minor", Version.Minor, TestMinor);
		TestEqual("Patch", Version.Patch, TestPatch);
		TestEqual("Commit", Version.Commit, TestCommit);
		return Version;
	};
	
	{
		const FSMVersionUtils::FVersion Version123 = CreateVersion("1.2.3", 1, 2, 3);
		const FSMVersionUtils::FVersion Version124 = CreateVersion("1.2.4", 1, 2, 4);
		const FSMVersionUtils::FVersion Version250 = CreateVersion("2.5.0", 2, 5, 0);
		const FSMVersionUtils::FVersion Version300 = CreateVersion("3.0.0", 3, 0, 0);
		
		TestTrue("Version comparison works", Version123 < Version124);
		TestTrue("Version comparison works", Version124 < Version250);
		TestFalse("Version comparison works", Version250 < Version124);
		TestTrue("Version comparison works", Version250 < Version300);
		
		TestNotEqual("Version not equal", Version123, Version250);

		TestTrue("Version comparison works", Version124 >= Version123);
		TestTrue("Version comparison works", Version250 >= Version124);
		TestFalse("Version comparison works", Version124 >= Version250);
		TestTrue("Version comparison works", Version300 >= Version250);
	}
	{
		const FSMVersionUtils::FVersion Version23 = CreateVersion("2.3", 2, 3, 0);
		const FSMVersionUtils::FVersion Version0 = CreateVersion("", 0, 0, 0);
		TestTrue("Version comparison works", Version0 < Version23);
		TestFalse("Version comparison works", Version23 < Version0);
	}

	// Test commit
	{
		const FSMVersionUtils::FVersion Version28Commit1 = CreateVersion("2.8.0.444afb30", 2, 8, 0, "444afb30");
		const FSMVersionUtils::FVersion Version28Commit2 = CreateVersion("2.8.0.860f570f", 2, 8, 0, "860f570f");

		TestTrue("Commit doesn't affect comparisons", Version28Commit1 == Version28Commit2);
		
		TestEqual("ToStringWithCommit", Version28Commit1.ToString(true), "2.8.0.444afb30");
		TestEqual("ToStringWithOutCommit", Version28Commit1.ToString(false), "2.8.0");
	}
	
	return true;
}

/**
* Validate construction script settings update.
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpdateConstructionScriptVersionTest, "LogicDriver.Upgrade.UpdateConstructionScriptVersion", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FUpdateConstructionScriptVersionTest::RunTest(const FString& Parameters)
{
	USMProjectEditorSettings* Settings = GetMutableDefault<USMProjectEditorSettings>();
	const ESMEditorConstructionScriptProjectSetting SavedCSSetting = Settings->EditorNodeConstructionScriptSetting;

	{
		Settings->EditorNodeConstructionScriptSetting = ESMEditorConstructionScriptProjectSetting::SM_Standard;
		FSMVersionUtils::UpdateProjectToNewVersion("2.0");

		TestEqual("CS not updated", Settings->EditorNodeConstructionScriptSetting, ESMEditorConstructionScriptProjectSetting::SM_Legacy);
	}

	{
		Settings->EditorNodeConstructionScriptSetting = ESMEditorConstructionScriptProjectSetting::SM_Standard;
		FSMVersionUtils::UpdateProjectToNewVersion("2.4.7");

		TestEqual("CS updated", Settings->EditorNodeConstructionScriptSetting, ESMEditorConstructionScriptProjectSetting::SM_Legacy);
	}

	{
		Settings->EditorNodeConstructionScriptSetting = ESMEditorConstructionScriptProjectSetting::SM_Standard;
		FSMVersionUtils::UpdateProjectToNewVersion("1.4");

		TestEqual("CS not updated", Settings->EditorNodeConstructionScriptSetting, ESMEditorConstructionScriptProjectSetting::SM_Standard);
	}

	{
		Settings->EditorNodeConstructionScriptSetting = ESMEditorConstructionScriptProjectSetting::SM_Standard;
		FSMVersionUtils::UpdateProjectToNewVersion("1.4.1");

		TestEqual("CS not updated", Settings->EditorNodeConstructionScriptSetting, ESMEditorConstructionScriptProjectSetting::SM_Standard);
	}

	{
		Settings->EditorNodeConstructionScriptSetting = ESMEditorConstructionScriptProjectSetting::SM_Standard;
		FSMVersionUtils::UpdateProjectToNewVersion("2.5.0");

		TestEqual("CS not updated", Settings->EditorNodeConstructionScriptSetting, ESMEditorConstructionScriptProjectSetting::SM_Standard);
	}

	{
		Settings->EditorNodeConstructionScriptSetting = ESMEditorConstructionScriptProjectSetting::SM_Standard;
		FSMVersionUtils::UpdateProjectToNewVersion("2.5.1");

		TestEqual("CS not updated", Settings->EditorNodeConstructionScriptSetting, ESMEditorConstructionScriptProjectSetting::SM_Standard);
	}

	{
		Settings->EditorNodeConstructionScriptSetting = ESMEditorConstructionScriptProjectSetting::SM_Standard;
		FSMVersionUtils::UpdateProjectToNewVersion("2.6.1");

		TestEqual("CS not updated", Settings->EditorNodeConstructionScriptSetting, ESMEditorConstructionScriptProjectSetting::SM_Standard);
	}

	{
		Settings->EditorNodeConstructionScriptSetting = ESMEditorConstructionScriptProjectSetting::SM_Standard;
		FSMVersionUtils::UpdateProjectToNewVersion("3.0.0");

		TestEqual("CS not updated", Settings->EditorNodeConstructionScriptSetting, ESMEditorConstructionScriptProjectSetting::SM_Standard);
	}

	Settings->EditorNodeConstructionScriptSetting = SavedCSSetting;
	Settings->SaveConfig();
	
	return true;
}

/**
 * Validate pre 2.4 nodes have their old property guids updated to account for template guids.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpdateStackGuidTest, "LogicDriver.Upgrade.UpdateStackGuid", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
	bool FUpdateStackGuidTest::RunTest(const FString& Parameters)
{
	FAssetHandler NewAsset;
	if (!TestHelpers::TryCreateNewStateMachineAsset(this, NewAsset, false))
	{
		return false;
	}

	USMBlueprint* NewBP = NewAsset.GetObjectAs<USMBlueprint>();

	// Find root state machine.
	USMGraphK2Node_StateMachineNode* RootStateMachineNode = FSMBlueprintEditorUtils::GetRootStateMachineNode(NewBP);

	// Find the state machine graph.
	USMGraph* StateMachineGraph = RootStateMachineNode->GetStateMachineGraph();

	// Total states to test.
	const int32 TotalStates = 1;

	// Load default instances.
	UEdGraphPin* LastStatePin = nullptr;
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMTextGraphStateExtra::StaticClass());

	TArray<USMGraphNode_StateNodeBase*> StateNodes;
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphNode_StateNodeBase>(NewBP, StateNodes);

	USMGraphNode_StateNodeBase* StateNode = StateNodes[1];
	{
		// Force convert BP to old version.
		StateNode->bTEST_ForceNoTemplateGuid = true;
		StateNode->bNeedsStateStackConversion = true;
		StateNode->bRequiresGuidRegeneration = true;
		FKismetEditorUtilities::CompileBlueprint(NewBP);
		// These will have been cleared, reset to maintain for next compile.
		StateNode->bNeedsStateStackConversion = true;
		StateNode->bRequiresGuidRegeneration = true;
	}

	// Test default values.
	{
		const FString DefaultStr = "ForStateStackString";
		const FText DefaultTextGraph = FText::FromString("ForStateStackTextGraph");

		// Set graph property values.
		auto PropertyNodes = StateNode->GetAllPropertyGraphNodesAsArray();
		for (USMGraphK2Node_PropertyNode_Base* PropertyNode : PropertyNodes)
		{
			if (USMGraphK2Node_TextPropertyNode* TextPropertyNode = Cast<USMGraphK2Node_TextPropertyNode>(PropertyNode))
			{
				USMTextPropertyGraph* TextPropertyGraph = CastChecked<USMTextPropertyGraph>(TextPropertyNode->GetPropertyGraph());
				TextPropertyGraph->SetNewText(DefaultTextGraph);
			}
			else
			{
				PropertyNode->GetSchema()->TrySetDefaultValue(*PropertyNode->GetResultPinChecked(), DefaultStr); // TrySet needed to trigger DefaultValueChanged
			}
		}

		// Test values run on old guids.
		{
			USMInstance* Instance = TestHelpers::TestLinearStateMachine(this, NewBP, TotalStates, false);

			USMTextGraphStateExtra* NodeInstance = CastChecked<USMTextGraphStateExtra>(Instance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());

			TestEqual("Default exposed value set and evaluated", NodeInstance->EvaluatedText.ToString(), DefaultTextGraph.ToString()); // This also tests that on state begin is hit.
			TestEqual("Default exposed value set and evaluated", NodeInstance->StringVar, DefaultStr);

			Instance->Stop();
		}

		TestFalse("State stack conversion set to false after compile", StateNode->bNeedsStateStackConversion);

		TArray<FGuid> OldGuids;
		StateNode->GetAllPropertyGraphs().GetKeys(OldGuids);
		TestEqual("Guid count matches", OldGuids.Num(), PropertyNodes.Num());

		// Test values run on new guids.
		{
			StateNode->bTEST_ForceNoTemplateGuid = false;
			StateNode->bNeedsStateStackConversion = true;
			StateNode->bRequiresGuidRegeneration = true;

			USMInstance* Instance = TestHelpers::TestLinearStateMachine(this, NewBP, TotalStates, false);

			USMTextGraphStateExtra* NodeInstance = CastChecked<USMTextGraphStateExtra>(Instance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());

			TestEqual("Default exposed value set and evaluated", NodeInstance->EvaluatedText.ToString(), DefaultTextGraph.ToString()); // This also tests that on state begin is hit.
			TestEqual("Default exposed value set and evaluated", NodeInstance->StringVar, DefaultStr);

			Instance->Stop();
		}

		TArray<FGuid> NewGuids;
		StateNode->GetAllPropertyGraphs().GetKeys(NewGuids);
		TestEqual("Guid count matches", NewGuids.Num(), OldGuids.Num());

		for (const FGuid& OldGuid : OldGuids)
		{
			TestFalse("Old guid is not in new guids", NewGuids.Contains(OldGuid));
		}
	}

	// Test variable values.
	{
		{
			// Force convert BP to old version.
			StateNode->bTEST_ForceNoTemplateGuid = true;
			StateNode->bNeedsStateStackConversion = true;
			StateNode->bRequiresGuidRegeneration = true;
			FKismetEditorUtilities::CompileBlueprint(NewBP);
			// These will have been cleared, reset to maintain for next compile.
			StateNode->bNeedsStateStackConversion = true;
			StateNode->bRequiresGuidRegeneration = true;
		}
		
		const FString TestStringDefaultValue = "StringVarDefaultValue";
		const FText DefaultTextGraph = FText::FromString("ForStateStackTextGraph");

		// Set graph property values.
		auto PropertyNodes = StateNode->GetAllPropertyGraphNodesAsArray();
		for (USMGraphK2Node_PropertyNode_Base* PropertyNode : PropertyNodes)
		{
			if (USMGraphK2Node_TextPropertyNode* TextPropertyNode = Cast<USMGraphK2Node_TextPropertyNode>(PropertyNode))
			{
				// Text graph property doesn't need to test variable evaluation since the default evaluation is equivalent.
			}
			else
			{
				FName VarName = "NewStrVar";
				FEdGraphPinType VarType;
				VarType.PinCategory = UEdGraphSchema_K2::PC_String;

				FBlueprintEditorUtils::AddMemberVariable(NewBP, VarName, VarType, TestStringDefaultValue);

				// Get class property from new variable.
				FProperty* NewProperty = FSMBlueprintEditorUtils::GetPropertyByName(NewBP, VarName);

				// Place variable getter and wire to result node.
				FSMBlueprintEditorUtils::PlacePropertyOnGraph(PropertyNode->GetGraph(), NewProperty, PropertyNode->GetResultPinChecked(), nullptr);
			}
		}

		// Test values run on old guids.
		{
			USMInstance* Instance = TestHelpers::TestLinearStateMachine(this, NewBP, TotalStates, false);

			USMTextGraphStateExtra* NodeInstance = CastChecked<USMTextGraphStateExtra>(Instance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
			TestEqual("Default exposed value set and evaluated", NodeInstance->EvaluatedText.ToString(), DefaultTextGraph.ToString()); // This also tests that on state begin is hit.
			TestEqual("Default exposed value set and evaluated", NodeInstance->StringVar, TestStringDefaultValue);

			Instance->Stop();
		}

		TestFalse("State stack conversion set to false after compile", StateNode->bNeedsStateStackConversion);

		TArray<FGuid> OldGuids;
		StateNode->GetAllPropertyGraphs().GetKeys(OldGuids);
		TestEqual("Guid count matches", OldGuids.Num(), PropertyNodes.Num());

		// Test values run on new guids.
		{
			StateNode->bTEST_ForceNoTemplateGuid = false;
			StateNode->bNeedsStateStackConversion = true;
			StateNode->bRequiresGuidRegeneration = true;

			USMInstance* Instance = TestHelpers::TestLinearStateMachine(this, NewBP, TotalStates, false);

			USMTextGraphStateExtra* NodeInstance = CastChecked<USMTextGraphStateExtra>(Instance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
			TestEqual("Default exposed value set and evaluated", NodeInstance->EvaluatedText.ToString(), DefaultTextGraph.ToString()); // This also tests that on state begin is hit.
			TestEqual("Variable exposed value set and evaluated", NodeInstance->StringVar, TestStringDefaultValue);

			Instance->Stop();
		}

		TArray<FGuid> NewGuids;
		StateNode->GetAllPropertyGraphs().GetKeys(NewGuids);
		TestEqual("Guid count matches", NewGuids.Num(), OldGuids.Num());

		for (const FGuid& OldGuid : OldGuids)
		{
			TestFalse("Old guid is not in new guids", NewGuids.Contains(OldGuid));
		}

		FKismetEditorUtilities::CompileBlueprint(NewBP);

		TArray<FGuid> NewGuids2;
		StateNode->GetAllPropertyGraphs().GetKeys(NewGuids2);
		TestEqual("Guid count matches", NewGuids.Num(), NewGuids2.Num());

		for (const FGuid& NewGuid : NewGuids2)
		{
			TestTrue("New guid has not changed on a new compile", NewGuids.Contains(NewGuid));
		}

		// Test values still remain the same.
		{
			StateNode->bTEST_ForceNoTemplateGuid = false;
			StateNode->bNeedsStateStackConversion = true;
			StateNode->bRequiresGuidRegeneration = true;

			USMInstance* Instance = TestHelpers::TestLinearStateMachine(this, NewBP, TotalStates, false);

			USMTextGraphStateExtra* NodeInstance = CastChecked<USMTextGraphStateExtra>(Instance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
			TestEqual("Default exposed value set and evaluated", NodeInstance->EvaluatedText.ToString(), DefaultTextGraph.ToString()); // This also tests that on state begin is hit.
			TestEqual("Variable exposed value set and evaluated", NodeInstance->StringVar, TestStringDefaultValue);

			Instance->Stop();
		}
	}
	
	return true;
}

/**
 * Validate pre 2.3 nodes have their templates setup properly and deprecated node values are imported.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpdateNodeTemplateTest, "LogicDriver.Upgrade.UpdateNodeTemplate", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
	bool FUpdateNodeTemplateTest::RunTest(const FString& Parameters)
{
	FAssetHandler NewAsset;
	if (!TestHelpers::TryCreateNewStateMachineAsset(this, NewAsset, false))
	{
		return false;
	}

	USMBlueprint* NewBP = NewAsset.GetObjectAs<USMBlueprint>();

	// Find root state machine.
	USMGraphK2Node_StateMachineNode* RootStateMachineNode = FSMBlueprintEditorUtils::GetRootStateMachineNode(NewBP);

	// Find the state machine graph.
	USMGraph* StateMachineGraph = RootStateMachineNode->GetStateMachineGraph();

	// Total states to test.
	int32 TotalStates = 3;

	// Load default instances.
	UEdGraphPin* LastStatePin = nullptr;
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin);

	// Test importing state values.
	{
		USMGraphNode_StateNodeBase* FirstState = CastChecked<USMGraphNode_StateNodeBase>(StateMachineGraph->EntryNode->GetOutputNode());
		// Default templates
		{
			FirstState->DestroyTemplate();

			TestFalse("Default value correct", FirstState->bDisableTickTransitionEvaluation_DEPRECATED);
			TestFalse("Default value correct", FirstState->bEvalTransitionsOnStart_DEPRECATED);
			TestFalse("Default value correct", FirstState->bExcludeFromAnyState_DEPRECATED);
			TestFalse("Default value correct", FirstState->bAlwaysUpdate_DEPRECATED);

			FirstState->bDisableTickTransitionEvaluation_DEPRECATED = true;
			FirstState->bEvalTransitionsOnStart_DEPRECATED = true;
			FirstState->bExcludeFromAnyState_DEPRECATED = true;
			FirstState->bAlwaysUpdate_DEPRECATED = true;

			FirstState->ForceSetVersion(0);
			FirstState->ConvertToCurrentVersion(true);
			TestNull("Template still null since this wasn't a load.", FirstState->GetNodeTemplate());

			FirstState->ConvertToCurrentVersion(false);
			TestNotNull("Template created.", FirstState->GetNodeTemplate());

			TestTrue("Default value imported", FirstState->GetNodeTemplateAs<USMStateInstance_Base>(true)->GetDisableTickTransitionEvaluation());
			TestTrue("Default value imported", FirstState->GetNodeTemplateAs<USMStateInstance_Base>(true)->GetEvalTransitionsOnStart());
			TestTrue("Default value imported", FirstState->GetNodeTemplateAs<USMStateInstance_Base>(true)->GetExcludeFromAnyState());
			TestTrue("Default value imported", FirstState->GetNodeTemplateAs<USMStateInstance_Base>(true)->GetAlwaysUpdate());

			// Test runtime with default values.
			{
				FKismetEditorUtilities::CompileBlueprint(NewBP);
				USMTestContext* Context = NewObject<USMTestContext>();
				USMInstance* StateMachineInstance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);

				USMStateInstance_Base* StateInstance = CastChecked<USMStateInstance_Base>(StateMachineInstance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());

				// Default class templates don't get compiled into the CDO, but the Getters will retrieve the struct version which should be the new values.
				TestTrue("Default value imported to runtime", StateInstance->GetDisableTickTransitionEvaluation());
				TestTrue("Default value imported to runtime", StateInstance->GetEvalTransitionsOnStart());
				TestFalse("Default value NOT imported to runtime", StateInstance->GetExcludeFromAnyState()); // Not stored on node.
				TestTrue("Default value imported to runtime", StateInstance->GetAlwaysUpdate());
			}
		}

		// Existing templates
		{
			const int32 TestInt = 7;
			{
				// Apply user template to a node that already has a default template created.
				FirstState->SetNodeClass(USMStateTestInstance::StaticClass());
				FirstState->GetNodeTemplateAs<USMStateTestInstance>(true)->ExposedInt = TestInt;

				// Defaults already set since we are applying the node class after the initial template was created. Old values should be copied to new template.
				TestTrue("Default value imported", FirstState->GetNodeTemplateAs<USMStateInstance_Base>(true)->GetDisableTickTransitionEvaluation());
				TestTrue("Default value imported", FirstState->GetNodeTemplateAs<USMStateInstance_Base>(true)->GetEvalTransitionsOnStart());
				TestTrue("Default value imported", FirstState->GetNodeTemplateAs<USMStateInstance_Base>(true)->GetExcludeFromAnyState());
				TestTrue("Default value imported", FirstState->GetNodeTemplateAs<USMStateInstance_Base>(true)->GetAlwaysUpdate());

				TestEqual("Edited value maintained", FirstState->GetNodeTemplateAs<USMStateTestInstance>(true)->ExposedInt, TestInt);
			}

			// Recreate so there are no existing values to be copied.
			{
				FirstState->DestroyTemplate();
				FirstState->SetNodeClass(USMStateTestInstance::StaticClass());
				FirstState->GetNodeTemplateAs<USMStateTestInstance>(true)->ExposedInt = TestInt;
				FirstState->SetPinsFromGraphProperties(false);
			}

			FirstState->ForceSetVersion(0);
			FirstState->ConvertToCurrentVersion(true);
			TestFalse("Default value not imported since it's not load", FirstState->GetNodeTemplateAs<USMStateInstance_Base>(true)->GetDisableTickTransitionEvaluation());
			TestFalse("Default value not imported since it's not load", FirstState->GetNodeTemplateAs<USMStateInstance_Base>(true)->GetEvalTransitionsOnStart());
			TestFalse("Default value not imported since it's not load", FirstState->GetNodeTemplateAs<USMStateInstance_Base>(true)->GetExcludeFromAnyState());
			TestFalse("Default value not imported since it's not load", FirstState->GetNodeTemplateAs<USMStateInstance_Base>(true)->GetAlwaysUpdate());

			TestEqual("Edited value maintained", FirstState->GetNodeTemplateAs<USMStateTestInstance>(true)->ExposedInt, TestInt);

			FirstState->ForceSetVersion(0);
			FirstState->ConvertToCurrentVersion(false);
			TestNotNull("Template created.", FirstState->GetNodeTemplate());

			TestTrue("Default value imported", FirstState->GetNodeTemplateAs<USMStateInstance_Base>(true)->GetDisableTickTransitionEvaluation());
			TestTrue("Default value imported", FirstState->GetNodeTemplateAs<USMStateInstance_Base>(true)->GetEvalTransitionsOnStart());
			TestTrue("Default value imported", FirstState->GetNodeTemplateAs<USMStateInstance_Base>(true)->GetExcludeFromAnyState());
			TestTrue("Default value imported", FirstState->GetNodeTemplateAs<USMStateInstance_Base>(true)->GetAlwaysUpdate());

			TestEqual("Edited value maintained", FirstState->GetNodeTemplateAs<USMStateTestInstance>(true)->ExposedInt, TestInt);

			// Test runtime with default values.
			{
				FKismetEditorUtilities::CompileBlueprint(NewBP);
				USMTestContext* Context = NewObject<USMTestContext>();
				USMInstance* StateMachineInstance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);

				USMStateTestInstance* StateInstance = CastChecked<USMStateTestInstance>(StateMachineInstance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());

				// User templates get copied to the CDO so their values should match the node values.
				TestTrue("Default value imported to runtime", StateInstance->GetDisableTickTransitionEvaluation());
				TestTrue("Default value imported to runtime", StateInstance->GetEvalTransitionsOnStart());
				TestTrue("Default value imported to runtime", StateInstance->GetExcludeFromAnyState());
				TestTrue("Default value imported to runtime", StateInstance->GetAlwaysUpdate());

				TestEqual("Edited value maintained", StateInstance->ExposedInt, TestInt);
			}
		}
	}

	// Test importing transition values.
	{
		const int32 PriorityOrder = 4;
		USMGraphNode_TransitionEdge* Transition = CastChecked<USMGraphNode_TransitionEdge>(CastChecked<USMGraphNode_StateNodeBase>(StateMachineGraph->EntryNode->GetOutputNode())->GetNextTransition());
		// Default templates.
		{
			Transition->DestroyTemplate();

			TestEqual("Default value correct", Transition->PriorityOrder_DEPRECATED, 0);
			TestTrue("Default value correct", Transition->bCanEvaluate_DEPRECATED);
			TestTrue("Default value correct", Transition->bCanEvaluateFromEvent_DEPRECATED);
			TestTrue("Default value correct", Transition->bCanEvalWithStartState_DEPRECATED);

			Transition->PriorityOrder_DEPRECATED = PriorityOrder;
			Transition->bCanEvaluate_DEPRECATED = false;
			Transition->bCanEvaluateFromEvent_DEPRECATED = false;
			Transition->bCanEvalWithStartState_DEPRECATED = false;

			Transition->ForceSetVersion(0);
			Transition->ConvertToCurrentVersion(true);
			TestNull("Template still null since this wasn't a load.", Transition->GetNodeTemplate());

			Transition->ConvertToCurrentVersion(false);
			TestNotNull("Template created.", Transition->GetNodeTemplate());

			TestEqual("Default value imported", Transition->GetNodeTemplateAs<USMTransitionInstance>(true)->GetPriorityOrder(), PriorityOrder);
			TestFalse("Default value imported", Transition->GetNodeTemplateAs<USMTransitionInstance>(true)->GetCanEvaluate());
			TestFalse("Default value imported", Transition->GetNodeTemplateAs<USMTransitionInstance>(true)->GetCanEvaluateFromEvent());
			TestFalse("Default value imported", Transition->GetNodeTemplateAs<USMTransitionInstance>(true)->GetCanEvalWithStartState());

			// Test runtime with default values.
			{
				FKismetEditorUtilities::CompileBlueprint(NewBP);
				USMTestContext* Context = NewObject<USMTestContext>();
				USMInstance* StateMachineInstance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);

				USMStateInstance_Base* StateInstance = CastChecked<USMStateInstance_Base>(StateMachineInstance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
				TArray<USMTransitionInstance*> Transitions;
				StateInstance->GetOutgoingTransitions(Transitions, false);
				USMTransitionInstance* TransitionInstance = Transitions[0];

				// Default class templates don't get compiled into the CDO, but the Getters will retrieve the struct version which should be the new values.
				TestEqual("Default value imported to runtime", TransitionInstance->GetPriorityOrder(), PriorityOrder);
				TestFalse("Default value imported to runtime", TransitionInstance->GetCanEvaluate());
				TestFalse("Default value imported to runtime", TransitionInstance->GetCanEvaluateFromEvent());
				TestFalse("Default value imported to runtime", TransitionInstance->GetCanEvalWithStartState());
			}
		}

		// Existing templates
		{
			const int32 TestInt = 7;
			{
				// Apply user template to a node that already has a default template created.
				Transition->SetNodeClass(USMTransitionTestInstance::StaticClass());
				Transition->GetNodeTemplateAs<USMTransitionTestInstance>(true)->IntValue = TestInt;

				// Defaults already set since we are applying the node class after the initial template was created. Old values should be copied to new template.
				TestEqual("Default value imported", Transition->GetNodeTemplateAs<USMTransitionInstance>(true)->GetPriorityOrder(), PriorityOrder);
				TestFalse("Default value imported", Transition->GetNodeTemplateAs<USMTransitionInstance>(true)->GetCanEvaluate());
				TestFalse("Default value imported", Transition->GetNodeTemplateAs<USMTransitionInstance>(true)->GetCanEvaluateFromEvent());
				TestFalse("Default value imported", Transition->GetNodeTemplateAs<USMTransitionInstance>(true)->GetCanEvalWithStartState());

				TestEqual("Edited value maintained", Transition->GetNodeTemplateAs<USMTransitionTestInstance>(true)->IntValue, TestInt);
			}

			// Recreate so there are no existing values to be copied.
			{
				Transition->DestroyTemplate();
				Transition->SetNodeClass(USMTransitionTestInstance::StaticClass());
				Transition->GetNodeTemplateAs<USMTransitionTestInstance>(true)->IntValue = TestInt;
			}

			Transition->ForceSetVersion(0);
			Transition->ConvertToCurrentVersion(true);
			TestEqual("Default value not imported since it's not load", Transition->GetNodeTemplateAs<USMTransitionInstance>(true)->GetPriorityOrder(), 0);
			TestTrue("Default value not imported since it's not load", Transition->GetNodeTemplateAs<USMTransitionInstance>(true)->GetCanEvaluate());
			TestTrue("Default value not imported since it's not load", Transition->GetNodeTemplateAs<USMTransitionInstance>(true)->GetCanEvaluateFromEvent());
			TestTrue("Default value not imported since it's not load", Transition->GetNodeTemplateAs<USMTransitionInstance>(true)->GetCanEvalWithStartState());

			TestEqual("Edited value maintained", Transition->GetNodeTemplateAs<USMTransitionTestInstance>(true)->IntValue, TestInt);

			Transition->ForceSetVersion(0);
			Transition->ConvertToCurrentVersion(false);
			TestNotNull("Template created.", Transition->GetNodeTemplate());

			TestEqual("Default value imported", Transition->GetNodeTemplateAs<USMTransitionInstance>(true)->GetPriorityOrder(), PriorityOrder);
			TestFalse("Default value imported", Transition->GetNodeTemplateAs<USMTransitionInstance>(true)->GetCanEvaluate());
			TestFalse("Default value imported", Transition->GetNodeTemplateAs<USMTransitionInstance>(true)->GetCanEvaluateFromEvent());
			TestFalse("Default value imported", Transition->GetNodeTemplateAs<USMTransitionInstance>(true)->GetCanEvalWithStartState());

			TestEqual("Edited value maintained", Transition->GetNodeTemplateAs<USMTransitionTestInstance>(true)->IntValue, TestInt);

			// Test runtime with default values.
			{
				FKismetEditorUtilities::CompileBlueprint(NewBP);
				USMTestContext* Context = NewObject<USMTestContext>();
				USMInstance* StateMachineInstance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);

				USMStateInstance_Base* StateInstance = CastChecked<USMStateInstance_Base>(StateMachineInstance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
				TArray<USMTransitionInstance*> Transitions;
				StateInstance->GetOutgoingTransitions(Transitions, false);
				USMTransitionTestInstance* TransitionInstance = CastChecked< USMTransitionTestInstance>(Transitions[0]);

				// Default class templates don't get compiled into the CDO, so the values should still be default in runtime.
				TestEqual("Default value imported to runtime", TransitionInstance->GetPriorityOrder(), PriorityOrder);
				TestFalse("Default value imported to runtime", TransitionInstance->GetCanEvaluate());
				TestFalse("Default value imported to runtime", TransitionInstance->GetCanEvaluateFromEvent());
				TestFalse("Default value imported to runtime", TransitionInstance->GetCanEvalWithStartState());

				TestEqual("Edited value maintained", TransitionInstance->IntValue, TestInt);
			}
		}
	}

	// Test importing conduit values.
	{
		USMGraphNode_StateNodeBase* SecondState = CastChecked<USMGraphNode_StateNodeBase>(StateMachineGraph->EntryNode->GetOutputNode())->GetNextState();
		USMGraphNode_ConduitNode* ConduitNode = FSMBlueprintEditorUtils::ConvertNodeTo<USMGraphNode_ConduitNode>(SecondState);

		// Default template.
		{
			ConduitNode->DestroyTemplate();

			TestFalse("Default value correct", ConduitNode->bDisableTickTransitionEvaluation_DEPRECATED);
			TestFalse("Default value correct", ConduitNode->bEvalTransitionsOnStart_DEPRECATED);
			TestFalse("Default value correct", ConduitNode->bExcludeFromAnyState_DEPRECATED);
			TestFalse("Default value correct", ConduitNode->bAlwaysUpdate_DEPRECATED);

			TestFalse("Default value correct", ConduitNode->bEvalWithTransitions_DEPRECATED);

			ConduitNode->bDisableTickTransitionEvaluation_DEPRECATED = true;
			ConduitNode->bEvalTransitionsOnStart_DEPRECATED = true;
			ConduitNode->bExcludeFromAnyState_DEPRECATED = true;
			ConduitNode->bAlwaysUpdate_DEPRECATED = true;
			ConduitNode->bEvalWithTransitions_DEPRECATED = true;

			ConduitNode->ForceSetVersion(0);
			ConduitNode->ConvertToCurrentVersion(true);
			TestNull("Template still null since this wasn't a load.", ConduitNode->GetNodeTemplate());

			ConduitNode->ConvertToCurrentVersion(false);
			TestNotNull("Template created.", ConduitNode->GetNodeTemplate());

			TestTrue("Default value imported", ConduitNode->GetNodeTemplateAs<USMStateInstance_Base>(true)->GetDisableTickTransitionEvaluation());
			TestTrue("Default value imported", ConduitNode->GetNodeTemplateAs<USMStateInstance_Base>(true)->GetEvalTransitionsOnStart());
			TestTrue("Default value imported", ConduitNode->GetNodeTemplateAs<USMStateInstance_Base>(true)->GetExcludeFromAnyState());
			TestTrue("Default value imported", ConduitNode->GetNodeTemplateAs<USMStateInstance_Base>(true)->GetAlwaysUpdate());

			TestTrue("Default value imported", ConduitNode->GetNodeTemplateAs<USMConduitInstance>(true)->GetEvalWithTransitions());

			// Test runtime with default values.
			{
				FKismetEditorUtilities::CompileBlueprint(NewBP);
				USMTestContext* Context = NewObject<USMTestContext>();
				USMInstance* StateMachineInstance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);

				USMStateInstance_Base* StateInstance = CastChecked<USMStateInstance_Base>(StateMachineInstance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
				USMConduitInstance* ConduitInstance = CastChecked<USMConduitInstance>(StateInstance->GetNextStateByTransitionIndex(0));

				// Default class templates don't get compiled into the CDO, but the Getters will retrieve the struct version which should be the new values.
				TestTrue("Default value imported to runtime", ConduitInstance->GetDisableTickTransitionEvaluation());
				TestTrue("Default value imported to runtime", ConduitInstance->GetEvalTransitionsOnStart());
				TestFalse("Default value NOT imported to runtime", ConduitInstance->GetExcludeFromAnyState());
				TestTrue("Default value imported to runtime", ConduitInstance->GetAlwaysUpdate());

				TestTrue("Default value imported to runtime", ConduitInstance->GetEvalWithTransitions());
			}
		}

		// Existing templates
		{
			const int32 TestInt = 7;
			{
				// Apply user template to a node that already has a default template created.
				ConduitNode->SetNodeClass(USMConduitTestInstance::StaticClass());
				ConduitNode->GetNodeTemplateAs<USMConduitTestInstance>(true)->IntValue = TestInt;
				ConduitNode->SetPinsFromGraphProperties(false);

				// Defaults already set since we are applying the node class after the initial template was created. Old values should be copied to new template.
				TestTrue("Default value imported", ConduitNode->GetNodeTemplateAs<USMStateInstance_Base>(true)->GetDisableTickTransitionEvaluation());
				TestTrue("Default value imported", ConduitNode->GetNodeTemplateAs<USMStateInstance_Base>(true)->GetEvalTransitionsOnStart());
				TestTrue("Default value imported", ConduitNode->GetNodeTemplateAs<USMStateInstance_Base>(true)->GetExcludeFromAnyState());
				TestTrue("Default value imported", ConduitNode->GetNodeTemplateAs<USMStateInstance_Base>(true)->GetAlwaysUpdate());

				TestTrue("Default value imported", ConduitNode->GetNodeTemplateAs<USMConduitInstance>(true)->GetEvalWithTransitions());

				TestEqual("Edited value maintained", ConduitNode->GetNodeTemplateAs<USMConduitTestInstance>(true)->IntValue, TestInt);
			}

			// Recreate so there are no existing values to be copied.
			{
				ConduitNode->DestroyTemplate();
				ConduitNode->SetNodeClass(USMConduitTestInstance::StaticClass());
				ConduitNode->GetNodeTemplateAs<USMConduitTestInstance>(true)->IntValue = TestInt;
				ConduitNode->SetPinsFromGraphProperties(false);
			}

			ConduitNode->ForceSetVersion(0);
			ConduitNode->ConvertToCurrentVersion(true);
			TestFalse("Default value not imported since it's not load", ConduitNode->GetNodeTemplateAs<USMStateInstance_Base>(true)->GetDisableTickTransitionEvaluation());
			TestFalse("Default value not imported since it's not load", ConduitNode->GetNodeTemplateAs<USMStateInstance_Base>(true)->GetEvalTransitionsOnStart());
			TestFalse("Default value not imported since it's not load", ConduitNode->GetNodeTemplateAs<USMStateInstance_Base>(true)->GetExcludeFromAnyState());
			TestFalse("Default value not imported since it's not load", ConduitNode->GetNodeTemplateAs<USMStateInstance_Base>(true)->GetAlwaysUpdate());

			TestFalse("Default value not imported since it's not load", ConduitNode->GetNodeTemplateAs<USMConduitInstance>(true)->GetEvalWithTransitions());

			TestEqual("Edited value maintained", ConduitNode->GetNodeTemplateAs<USMConduitTestInstance>(true)->IntValue, TestInt);

			ConduitNode->ForceSetVersion(0);
			ConduitNode->ConvertToCurrentVersion(false);
			TestNotNull("Template created.", ConduitNode->GetNodeTemplate());

			TestTrue("Default value imported", ConduitNode->GetNodeTemplateAs<USMStateInstance_Base>(true)->GetDisableTickTransitionEvaluation());
			TestTrue("Default value imported", ConduitNode->GetNodeTemplateAs<USMStateInstance_Base>(true)->GetEvalTransitionsOnStart());
			TestTrue("Default value imported", ConduitNode->GetNodeTemplateAs<USMStateInstance_Base>(true)->GetExcludeFromAnyState());
			TestTrue("Default value imported", ConduitNode->GetNodeTemplateAs<USMStateInstance_Base>(true)->GetAlwaysUpdate());

			TestTrue("Default value imported", ConduitNode->GetNodeTemplateAs<USMConduitInstance>(true)->GetEvalWithTransitions());

			TestEqual("Edited value maintained", ConduitNode->GetNodeTemplateAs<USMConduitTestInstance>(true)->IntValue, TestInt);

			// Test runtime with default values.
			{
				FKismetEditorUtilities::CompileBlueprint(NewBP);
				USMTestContext* Context = NewObject<USMTestContext>();
				USMInstance* StateMachineInstance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);

				USMStateInstance_Base* StateInstance = CastChecked<USMStateInstance_Base>(StateMachineInstance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
				USMConduitTestInstance* ConduitInstance = CastChecked<USMConduitTestInstance>(StateInstance->GetNextStateByTransitionIndex(0));

				// Default class templates don't get compiled into the CDO, so the values should still be default in runtime.
				TestTrue("Default value imported to runtime", ConduitInstance->GetDisableTickTransitionEvaluation());
				TestTrue("Default value imported to runtime", ConduitInstance->GetEvalTransitionsOnStart());
				TestTrue("Default value imported to runtime", ConduitInstance->GetExcludeFromAnyState());
				TestTrue("Default value imported to runtime", ConduitInstance->GetAlwaysUpdate());

				TestEqual("Edited value maintained", ConduitInstance->IntValue, TestInt);

				TestTrue("Default value imported to runtime", ConduitInstance->GetEvalWithTransitions());
			}
		}
	}

	// Test importing state machine values.
	{
		USMGraphNode_StateNodeBase* ThirdState = CastChecked<USMGraphNode_StateNodeBase>(StateMachineGraph->EntryNode->GetOutputNode())->GetNextState()->GetNextState();
		USMGraphNode_StateMachineStateNode* FSMNode = FSMBlueprintEditorUtils::ConvertNodeTo<USMGraphNode_StateMachineStateNode>(ThirdState);

		// Default template.
		{
			FSMNode->DestroyTemplate();

			TestFalse("Default value correct", FSMNode->bDisableTickTransitionEvaluation_DEPRECATED);
			TestFalse("Default value correct", FSMNode->bEvalTransitionsOnStart_DEPRECATED);
			TestFalse("Default value correct", FSMNode->bExcludeFromAnyState_DEPRECATED);
			TestFalse("Default value correct", FSMNode->bAlwaysUpdate_DEPRECATED);

			TestFalse("Default value correct", FSMNode->bReuseIfNotEndState_DEPRECATED);
			TestFalse("Default value correct", FSMNode->bReuseCurrentState_DEPRECATED);

			FSMNode->bDisableTickTransitionEvaluation_DEPRECATED = true;
			FSMNode->bEvalTransitionsOnStart_DEPRECATED = true;
			FSMNode->bExcludeFromAnyState_DEPRECATED = true;
			FSMNode->bAlwaysUpdate_DEPRECATED = true;

			FSMNode->bReuseIfNotEndState_DEPRECATED = true;
			FSMNode->bReuseCurrentState_DEPRECATED = true;

			FSMNode->ForceSetVersion(0);
			FSMNode->ConvertToCurrentVersion(true);
			TestNull("Template still null since this wasn't a load.", FSMNode->GetNodeTemplate());

			FSMNode->ConvertToCurrentVersion(false);
			TestNotNull("Template created.", FSMNode->GetNodeTemplate());

			TestTrue("Default value imported", FSMNode->GetNodeTemplateAs<USMStateInstance_Base>(true)->GetDisableTickTransitionEvaluation());
			TestTrue("Default value imported", FSMNode->GetNodeTemplateAs<USMStateInstance_Base>(true)->GetEvalTransitionsOnStart());
			TestTrue("Default value imported", FSMNode->GetNodeTemplateAs<USMStateInstance_Base>(true)->GetExcludeFromAnyState());
			TestTrue("Default value imported", FSMNode->GetNodeTemplateAs<USMStateInstance_Base>(true)->GetAlwaysUpdate());

			TestTrue("Default value imported", FSMNode->GetNodeTemplateAs<USMStateMachineInstance>(true)->GetReuseIfNotEndState());
			TestTrue("Default value imported", FSMNode->GetNodeTemplateAs<USMStateMachineInstance>(true)->GetReuseCurrentState());

			// Test runtime with default values.
			{
				FKismetEditorUtilities::CompileBlueprint(NewBP);
				USMTestContext* Context = NewObject<USMTestContext>();
				USMInstance* StateMachineInstance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);

				USMStateInstance_Base* StateInstance = CastChecked<USMStateInstance_Base>(StateMachineInstance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
				USMStateMachineInstance* FSMInstance = CastChecked<USMStateMachineInstance>(StateInstance->GetNextStateByTransitionIndex(0)->GetNextStateByTransitionIndex(0));

				// Default class templates don't get compiled into the CDO, but the Getters will retrieve the struct version which should be the new values.
				TestTrue("Default value imported to runtime", FSMInstance->GetDisableTickTransitionEvaluation());
				TestTrue("Default value imported to runtime", FSMInstance->GetEvalTransitionsOnStart());
				TestFalse("Default value NOT imported to runtime", FSMInstance->GetExcludeFromAnyState());
				TestTrue("Default value imported to runtime", FSMInstance->GetAlwaysUpdate());

				TestTrue("Default value imported to runtime", FSMInstance->GetReuseIfNotEndState());
				TestTrue("Default value imported to runtime", FSMInstance->GetReuseCurrentState());
			}
		}

		// Existing templates
		{
			const int32 TestInt = 7;
			{
				// Apply user template to a node that already has a default template created.
				FSMNode->SetNodeClass(USMStateMachineTestInstance::StaticClass());
				FSMNode->GetNodeTemplateAs<USMStateMachineTestInstance>(true)->ExposedInt = TestInt;
				FSMNode->SetPinsFromGraphProperties(false);

				// Defaults already set since we are applying the node class after the initial template was created. Old values should be copied to new template.
				TestTrue("Default value imported", FSMNode->GetNodeTemplateAs<USMStateMachineTestInstance>(true)->GetDisableTickTransitionEvaluation());
				TestTrue("Default value imported", FSMNode->GetNodeTemplateAs<USMStateMachineTestInstance>(true)->GetEvalTransitionsOnStart());
				TestTrue("Default value imported", FSMNode->GetNodeTemplateAs<USMStateMachineTestInstance>(true)->GetExcludeFromAnyState());
				TestTrue("Default value imported", FSMNode->GetNodeTemplateAs<USMStateMachineTestInstance>(true)->GetAlwaysUpdate());

				TestTrue("Default value imported", FSMNode->GetNodeTemplateAs<USMStateMachineInstance>(true)->GetReuseIfNotEndState());
				TestTrue("Default value imported", FSMNode->GetNodeTemplateAs<USMStateMachineInstance>(true)->GetReuseCurrentState());

				TestEqual("Edited value maintained", FSMNode->GetNodeTemplateAs<USMStateMachineTestInstance>(true)->ExposedInt, TestInt);
			}

			// Recreate so there are no existing values to be copied.
			{
				FSMNode->DestroyTemplate();
				FSMNode->SetNodeClass(USMStateMachineTestInstance::StaticClass());
				FSMNode->GetNodeTemplateAs<USMStateMachineTestInstance>(true)->ExposedInt = TestInt;
				FSMNode->SetPinsFromGraphProperties(false);
			}

			FSMNode->ForceSetVersion(0);
			FSMNode->ConvertToCurrentVersion(true);
			TestFalse("Default value not imported since it's not load", FSMNode->GetNodeTemplateAs<USMStateMachineTestInstance>(true)->GetDisableTickTransitionEvaluation());
			TestFalse("Default value not imported since it's not load", FSMNode->GetNodeTemplateAs<USMStateMachineTestInstance>(true)->GetEvalTransitionsOnStart());
			TestFalse("Default value not imported since it's not load", FSMNode->GetNodeTemplateAs<USMStateMachineTestInstance>(true)->GetExcludeFromAnyState());
			TestFalse("Default value not imported since it's not load", FSMNode->GetNodeTemplateAs<USMStateMachineTestInstance>(true)->GetAlwaysUpdate());

			TestFalse("Default value not imported since it's not load", FSMNode->GetNodeTemplateAs<USMStateMachineTestInstance>(true)->GetReuseIfNotEndState());
			TestFalse("Default value not imported since it's not load", FSMNode->GetNodeTemplateAs<USMStateMachineTestInstance>(true)->GetReuseCurrentState());

			TestEqual("Edited value maintained", FSMNode->GetNodeTemplateAs<USMStateMachineTestInstance>(true)->ExposedInt, TestInt);

			FSMNode->ForceSetVersion(0);
			FSMNode->ConvertToCurrentVersion(false);
			TestNotNull("Template created.", FSMNode->GetNodeTemplate());

			TestTrue("Default value imported", FSMNode->GetNodeTemplateAs<USMStateMachineTestInstance>(true)->GetDisableTickTransitionEvaluation());
			TestTrue("Default value imported", FSMNode->GetNodeTemplateAs<USMStateMachineTestInstance>(true)->GetEvalTransitionsOnStart());
			TestTrue("Default value imported", FSMNode->GetNodeTemplateAs<USMStateMachineTestInstance>(true)->GetExcludeFromAnyState());
			TestTrue("Default value imported", FSMNode->GetNodeTemplateAs<USMStateMachineTestInstance>(true)->GetAlwaysUpdate());

			TestTrue("Default value imported", FSMNode->GetNodeTemplateAs<USMStateMachineInstance>(true)->GetReuseIfNotEndState());
			TestTrue("Default value imported", FSMNode->GetNodeTemplateAs<USMStateMachineInstance>(true)->GetReuseCurrentState());

			TestEqual("Edited value maintained", FSMNode->GetNodeTemplateAs<USMStateMachineTestInstance>(true)->ExposedInt, TestInt);

			// Test runtime with default values.
			{
				FKismetEditorUtilities::CompileBlueprint(NewBP);
				USMTestContext* Context = NewObject<USMTestContext>();
				USMInstance* StateMachineInstance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);

				USMStateInstance_Base* StateInstance = CastChecked<USMStateInstance_Base>(StateMachineInstance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
				USMStateMachineTestInstance* FSMInstance = CastChecked<USMStateMachineTestInstance>(StateInstance->GetNextStateByTransitionIndex(0)->GetNextStateByTransitionIndex(0));

				// Default class templates don't get compiled into the CDO, so the values should still be default in runtime.
				TestTrue("Default value imported to runtime", FSMInstance->GetDisableTickTransitionEvaluation());
				TestTrue("Default value imported to runtime", FSMInstance->GetEvalTransitionsOnStart());
				TestTrue("Default value imported to runtime", FSMInstance->GetExcludeFromAnyState());
				TestTrue("Default value imported to runtime", FSMInstance->GetAlwaysUpdate());

				TestTrue("Default value imported to runtime", FSMInstance->GetReuseIfNotEndState());
				TestTrue("Default value imported to runtime", FSMInstance->GetReuseCurrentState());

				TestEqual("Edited value maintained", FSMInstance->ExposedInt, TestInt);
			}
		}
	}

	return true;
}

/**
 * Validate components import their deprecated values correctly.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpdateComponentTest, "LogicDriver.Upgrade.UpdateComponent", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

	bool FUpdateComponentTest::RunTest(const FString& Parameters)
{
	FAssetHandler NewAsset;
	if (!TestHelpers::TryCreateNewStateMachineAsset(this, NewAsset, false))
	{
		return false;
	}
	
	USMBlueprint* NewBP = NewAsset.GetObjectAs<USMBlueprint>();

	USMStateMachineTestComponent* TestComponent = NewObject<USMStateMachineTestComponent>(GetTransientPackage(), NAME_None, RF_ArchetypeObject | RF_Public);
	TestComponent->SetStateMachineClass(NewBP->GetGeneratedClass());

	bool bOverrideCanEverTick = true;
	bool bCanEverTick = false;

	bool bOverrideTickinterval = true;
	float TickInterval = 0.5f;

	// Test valid changes that will be imported.
	TestComponent->SetAllowTick(bOverrideCanEverTick, bCanEverTick);
	TestComponent->SetTickInterval(bOverrideTickinterval, TickInterval);

	TestComponent->ImportDeprecatedProperties_Public();

	USMInstance* Template = TestComponent->GetTemplateForInstance();
	TestNotNull("Instance template created", Template);

	TestEqual("CanTick", Template->CanEverTick(), bCanEverTick);
	TestEqual("TickInterval", Template->GetTickInterval(), TickInterval);

	// Prepare for changed values but without allowing override.
	bOverrideCanEverTick = false;
	TestComponent->SetAllowTick(bOverrideCanEverTick, bCanEverTick);
	bOverrideTickinterval = false;
	TestComponent->SetTickInterval(bOverrideTickinterval, TickInterval);

	// This shouldn't work and values should remain the same because we have a template and class set.
	TestComponent->ImportDeprecatedProperties_Public();
	Template = TestComponent->GetTemplateForInstance();
	TestNotNull("Instance template created", Template);
	TestEqual("CanTick", Template->CanEverTick(), bCanEverTick);
	TestEqual("TickInterval", Template->GetTickInterval(), TickInterval);

	// Clear and rerun, values should be default since overrides are disabled.
	TestComponent->ClearTemplateInstance();
	TestComponent->ImportDeprecatedProperties_Public();
	Template = TestComponent->GetTemplateForInstance();
	TestNotNull("Instance template created", Template);
	TestEqual("CanTick", Template->CanEverTick(), !bCanEverTick);
	TestEqual("TickInterval", Template->GetTickInterval(), 0.f);

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
#define DEFAULT_AUTHORITY SM_Client
#define DEFAULT_EXECUTION SM_ClientAndServer
#define DEFAULT_WAIT_RPC  false

#define CHANGED_AUTHORITY SM_ClientAndServer
#define CHANGED_EXECUTION SM_Client
#define CHANGED_WAIT_RPC  true
	
	// Net Properties
	
	// Test defaults.
	TestEqual("Deprecated property is default", TestComponent->NetworkTransitionConfiguration, DEFAULT_AUTHORITY); 
	TestEqual("Deprecated property is default", TestComponent->NetworkStateConfiguration, DEFAULT_EXECUTION);
	TestEqual("Deprecated property is default", TestComponent->bTakeTransitionsFromServerOnly, DEFAULT_WAIT_RPC);

	TestEqual("Updated property is default", TestComponent->StateChangeAuthority, DEFAULT_AUTHORITY); 
	TestEqual("Updated property is default", TestComponent->NetworkStateExecution, DEFAULT_EXECUTION);
	TestEqual("Updated property is default", TestComponent->bWaitForTransactionsFromServer, DEFAULT_WAIT_RPC);
	
	TestComponent->NetworkTransitionConfiguration = CHANGED_AUTHORITY;
	TestComponent->NetworkStateConfiguration = CHANGED_EXECUTION;
	TestComponent->bTakeTransitionsFromServerOnly = CHANGED_WAIT_RPC;

	// Test deprecated values imported.
	TestComponent->ImportDeprecatedProperties_Public();

	TestEqual("Updated property is set", TestComponent->StateChangeAuthority, CHANGED_AUTHORITY); 
	TestEqual("Updated property is set", TestComponent->NetworkStateExecution, CHANGED_EXECUTION);
	TestEqual("Updated property is set", TestComponent->bWaitForTransactionsFromServer, CHANGED_WAIT_RPC);

	TestEqual("Deprecated property is default", TestComponent->NetworkTransitionConfiguration, DEFAULT_AUTHORITY); 
	TestEqual("Deprecated property is default", TestComponent->NetworkStateConfiguration, DEFAULT_EXECUTION);
	TestEqual("Deprecated property is default", TestComponent->bTakeTransitionsFromServerOnly, DEFAULT_WAIT_RPC);

	// Test no change.
	TestComponent->ImportDeprecatedProperties_Public();

	TestEqual("Updated property is set", TestComponent->StateChangeAuthority, CHANGED_AUTHORITY); 
	TestEqual("Updated property is set", TestComponent->NetworkStateExecution, CHANGED_EXECUTION);
	TestEqual("Updated property is set", TestComponent->bWaitForTransactionsFromServer, CHANGED_WAIT_RPC);

	TestEqual("Deprecated property is default", TestComponent->NetworkTransitionConfiguration, DEFAULT_AUTHORITY); 
	TestEqual("Deprecated property is default", TestComponent->NetworkStateConfiguration, DEFAULT_EXECUTION);
	TestEqual("Deprecated property is default", TestComponent->bTakeTransitionsFromServerOnly, DEFAULT_WAIT_RPC);
	
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	return NewAsset.DeleteAsset(this);
}

/**
 * Test the new pin names load correctly.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSMPinConversionTest, "LogicDriver.Upgrade.PinConversion", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FSMPinConversionTest::RunTest(const FString& Parameters)
{
	FAssetHandler NewAsset;
	if (!TestHelpers::TryCreateNewStateMachineAsset(this, NewAsset, false))
	{
		return false;
	}

	USMBlueprint* NewBP = NewAsset.GetObjectAs<USMBlueprint>();

	// Find root state machine.
	USMGraphK2Node_StateMachineNode* RootStateMachineNode = FSMBlueprintEditorUtils::GetRootStateMachineNode(NewBP);

	// Find the state machine graph.
	USMGraph* StateMachineGraph = RootStateMachineNode->GetStateMachineGraph();

	// Total states to test.
	UEdGraphPin* LastStatePin = nullptr;

	{
		TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, 2, &LastStatePin, USMStateTestInstance::StaticClass(), USMTransitionTestInstance::StaticClass());
	}

	USMGraphNode_StateNodeBase* FirstNode = CastChecked<USMGraphNode_StateNodeBase>(StateMachineGraph->GetEntryNode()->GetOutputNode());
	{
		USMGraphNode_TransitionEdge* Transition = CastChecked<USMGraphNode_TransitionEdge>(FirstNode->GetOutputPin()->LinkedTo[0]->GetOwningNode());
		
		TestHelpers::AddEventWithLogic<USMGraphK2Node_TransitionPreEvaluateNode>(this, Transition,
			USMTestContext::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMTestContext, IncreaseTransitionPreEval)));

		TestHelpers::AddEventWithLogic<USMGraphK2Node_TransitionPostEvaluateNode>(this, Transition,
			USMTestContext::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMTestContext, IncreaseTransitionPostEval)));
	}
	
	// Use a conduit
	USMGraphNode_ConduitNode* SecondNodeConduit = FSMBlueprintEditorUtils::ConvertNodeTo<USMGraphNode_ConduitNode>(CastChecked<USMGraphNode_StateNodeBase>(FirstNode->GetNextState()));
	{
		SecondNodeConduit->SetNodeClass(USMConduitTestInstance::StaticClass());
		USMConduitGraph* Graph = CastChecked<USMConduitGraph>(SecondNodeConduit->GetBoundGraph());
		UEdGraphPin* CanEvalPin = Graph->ResultNode->GetInputPin();
		CanEvalPin->BreakAllPinLinks();
		CanEvalPin->DefaultValue = "True";

		// Pin was cleared out during conversion.
		LastStatePin = SecondNodeConduit->GetOutputPin();
	}
	
	UEdGraphPin* LastNestedPin = nullptr;
	USMGraphNode_StateMachineStateNode* ThirdNodeRef = TestHelpers::BuildNestedStateMachine(this, StateMachineGraph, 1, &LastStatePin, &LastNestedPin);
	{
		FString AssetName = "PinTestRef_1";
		UBlueprint* RefBlueprint = FSMBlueprintEditorUtils::ConvertStateMachineToReference(ThirdNodeRef, false, &AssetName, nullptr);
		FKismetEditorUtilities::CompileBlueprint(RefBlueprint);
		LastStatePin = ThirdNodeRef->GetOutputPin();
	}
	
	USMGraphNode_StateMachineStateNode* FourthNodeIntermediateRef = TestHelpers::BuildNestedStateMachine(this, StateMachineGraph, 1, &LastStatePin, &LastNestedPin);
	{
		FString AssetName = "PinTestRef_2";
		UBlueprint* RefBlueprint = FSMBlueprintEditorUtils::ConvertStateMachineToReference(FourthNodeIntermediateRef, false, &AssetName, nullptr);
		FKismetEditorUtilities::CompileBlueprint(RefBlueprint);
		
		FourthNodeIntermediateRef->SetUseIntermediateGraph(true);
		LastStatePin = FourthNodeIntermediateRef->GetOutputPin();
		
		TestHelpers::AddEventWithLogic<USMGraphK2Node_IntermediateStateMachineStartNode>(this, FourthNodeIntermediateRef,
			USMTestContext::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMTestContext, IncreaseEntryInt)));
	}

	// Add one last state
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, 1, &LastStatePin, USMStateTestInstance::StaticClass(), USMTransitionTestInstance::StaticClass());

	{
		USMGraphNode_TransitionEdge* Transition = CastChecked<USMGraphNode_TransitionEdge>(SecondNodeConduit->GetOutputPin()->LinkedTo[0]->GetOwningNode());
		TestHelpers::SetNodeClass(this, Transition, USMTransitionTestInstance::StaticClass());

		TestHelpers::AddTransitionResultLogic(this, Transition);
		
		TestHelpers::AddEventWithLogic<USMGraphK2Node_TransitionPreEvaluateNode>(this, Transition,
			USMTestContext::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMTestContext, IncreaseTransitionPreEval)));

		TestHelpers::AddEventWithLogic<USMGraphK2Node_TransitionPostEvaluateNode>(this, Transition,
			USMTestContext::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMTestContext, IncreaseTransitionPostEval)));
	}
	
	{
		// Signal the state after the nested state machine to wait for its completion.
		USMGraphNode_TransitionEdge* TransitionFromNestedStateMachine = CastChecked<USMGraphNode_TransitionEdge>(ThirdNodeRef->GetOutputPin()->LinkedTo[0]->GetOwningNode());

		TestHelpers::SetNodeClass(this, TransitionFromNestedStateMachine, USMTransitionTestInstance::StaticClass());
		TestHelpers::AddTransitionResultLogic(this, TransitionFromNestedStateMachine);
		
		TestHelpers::AddEventWithLogic<USMGraphK2Node_TransitionPreEvaluateNode>(this, TransitionFromNestedStateMachine,
			USMTestContext::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMTestContext, IncreaseTransitionPreEval)));

		TestHelpers::AddEventWithLogic<USMGraphK2Node_TransitionPostEvaluateNode>(this, TransitionFromNestedStateMachine,
			USMTestContext::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMTestContext, IncreaseTransitionPostEval)));
	}

	{
		// Signal the state after the nested state machine to wait for its completion.
		USMGraphNode_TransitionEdge* TransitionFromNestedStateMachine = CastChecked<USMGraphNode_TransitionEdge>(FourthNodeIntermediateRef->GetOutputPin()->LinkedTo[0]->GetOwningNode());

		TestHelpers::SetNodeClass(this, TransitionFromNestedStateMachine, USMTransitionTestInstance::StaticClass());
		TestHelpers::AddTransitionResultLogic(this, TransitionFromNestedStateMachine);
		
		TestHelpers::AddEventWithLogic<USMGraphK2Node_TransitionPreEvaluateNode>(this, TransitionFromNestedStateMachine,
			USMTestContext::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMTestContext, IncreaseTransitionPreEval)));

		TestHelpers::AddEventWithLogic<USMGraphK2Node_TransitionPostEvaluateNode>(this, TransitionFromNestedStateMachine,
			USMTestContext::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMTestContext, IncreaseTransitionPostEval)));
	}

	// Run as normal.
	int32 EntryHits = 0; int32 UpdateHits = 0; int32 EndHits = 0;
	USMInstance* Instance = TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits);

	USMTestContext* Context = CastChecked<USMTestContext>(Instance->GetContext());
	int32 PreEval = Context->TestTransitionPreEval.Count;
	int32 PostEval = Context->TestTransitionPostEval.Count;

	TestTrue("Pre/Post Evals hit", PreEval > 0 && PostEval > 0);
	
	// Rename all of the pins to pre 2.1 pin names.
	FName OldPinName = FName("");
	{
		{
			TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_StateEntryNode>(this, FirstNode->GetBoundGraph(), USMGraphK2Schema::PN_Then, &OldPinName);
			TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_StateUpdateNode>(this, FirstNode->GetBoundGraph(), USMGraphK2Schema::PN_Then, &OldPinName);
			TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_StateEndNode>(this, FirstNode->GetBoundGraph(), USMGraphK2Schema::PN_Then, &OldPinName);
		}

		{
			TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionEnteredNode>(this, FirstNode->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then, &OldPinName);
			TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionInitializedNode>(this, FirstNode->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then, &OldPinName);
			TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionShutdownNode>(this, FirstNode->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then, &OldPinName);
			TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionPreEvaluateNode>(this, FirstNode->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then, &OldPinName);
			TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionPostEvaluateNode>(this, FirstNode->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then, &OldPinName);
		}
		
		{
			TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_IntermediateStateMachineStartNode>(this, SecondNodeConduit->GetBoundGraph(), USMGraphK2Schema::PN_Then, &OldPinName);
		}

		{
			TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionEnteredNode>(this, SecondNodeConduit->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then, &OldPinName);
			TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionInitializedNode>(this, SecondNodeConduit->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then, &OldPinName);
			TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionShutdownNode>(this, SecondNodeConduit->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then, &OldPinName);
			TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionPreEvaluateNode>(this, SecondNodeConduit->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then, &OldPinName);
			TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionPostEvaluateNode>(this, SecondNodeConduit->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then, &OldPinName);
		}

		{
			TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionEnteredNode>(this, ThirdNodeRef->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then, &OldPinName);
			TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionInitializedNode>(this, ThirdNodeRef->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then, &OldPinName);
			TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionShutdownNode>(this, ThirdNodeRef->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then, &OldPinName);
			TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionPreEvaluateNode>(this, ThirdNodeRef->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then, &OldPinName);
			TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionPostEvaluateNode>(this, ThirdNodeRef->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then, &OldPinName);
		}
		
		{
			TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_IntermediateEntryNode>(this, FourthNodeIntermediateRef->GetBoundGraph(), USMGraphK2Schema::PN_Then, &OldPinName);
			TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_StateUpdateNode>(this, FourthNodeIntermediateRef->GetBoundGraph(), USMGraphK2Schema::PN_Then, &OldPinName);
			TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_StateEndNode>(this, FourthNodeIntermediateRef->GetBoundGraph(), USMGraphK2Schema::PN_Then, &OldPinName);

			TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_IntermediateStateMachineStartNode>(this, FourthNodeIntermediateRef->GetBoundGraph(), USMGraphK2Schema::PN_Then, &OldPinName);
		}

		{
			TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionEnteredNode>(this, FourthNodeIntermediateRef->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then, &OldPinName);
			TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionInitializedNode>(this, FourthNodeIntermediateRef->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then, &OldPinName);
			TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionShutdownNode>(this, FourthNodeIntermediateRef->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then, &OldPinName);
			TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionPreEvaluateNode>(this, FourthNodeIntermediateRef->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then, &OldPinName);
			TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionPostEvaluateNode>(this, FourthNodeIntermediateRef->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then, &OldPinName);
		}
	}

	// Verify it still works.
	int32 EntryHits2 = 0; int32 UpdateHits2 = 0; int32 EndHits2 = 0;
	Instance = TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits2, UpdateHits2, EndHits2);

	Context = CastChecked<USMTestContext>(Instance->GetContext());
	TestEqual("Hits match", Context->TestTransitionPreEval.Count, PreEval);
	TestEqual("Hits match", Context->TestTransitionPostEval.Count, PostEval);

	TestEqual("Hits match", EntryHits2, EntryHits);
	TestEqual("Hits match", UpdateHits2, UpdateHits);
	TestEqual("Hits match", EndHits2, EndHits);
	
	if (!NewAsset.SaveAsset(this))
	{
		return false;
	}

	if (!NewAsset.LoadAsset(this))
	{
		return false;
	}

	NewBP = NewAsset.GetObjectAs<USMBlueprint>();
	FSMBlueprintEditorUtils::ReconstructAllNodes(NewBP);
	
	RootStateMachineNode = FSMBlueprintEditorUtils::GetRootStateMachineNode(NewBP);
	StateMachineGraph = RootStateMachineNode->GetStateMachineGraph();
	FirstNode = CastChecked<USMGraphNode_StateNodeBase>(StateMachineGraph->GetEntryNode()->GetOutputNode());

	// Verify pins have been correctly renamed.
	{
		TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_StateEntryNode>(this, FirstNode->GetBoundGraph(), USMGraphK2Schema::PN_Then);
		TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_StateUpdateNode>(this, FirstNode->GetBoundGraph(), USMGraphK2Schema::PN_Then);
		TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_StateEndNode>(this, FirstNode->GetBoundGraph(), USMGraphK2Schema::PN_Then);
	}

	{
		TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionEnteredNode>(this, FirstNode->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then);
		TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionInitializedNode>(this, FirstNode->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then);
		TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionShutdownNode>(this, FirstNode->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then);
		TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionPreEvaluateNode>(this, FirstNode->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then);
		TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionPostEvaluateNode>(this, FirstNode->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then);
	}

	{
		TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_IntermediateStateMachineStartNode>(this, SecondNodeConduit->GetBoundGraph(), USMGraphK2Schema::PN_Then);
	}

	{
		TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionEnteredNode>(this, SecondNodeConduit->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then);
		TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionInitializedNode>(this, SecondNodeConduit->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then);
		TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionShutdownNode>(this, SecondNodeConduit->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then);
		TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionPreEvaluateNode>(this, SecondNodeConduit->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then);
		TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionPostEvaluateNode>(this, SecondNodeConduit->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then);
	}

	{
		TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionEnteredNode>(this, ThirdNodeRef->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then);
		TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionInitializedNode>(this, ThirdNodeRef->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then);
		TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionShutdownNode>(this, ThirdNodeRef->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then);
		TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionPreEvaluateNode>(this, ThirdNodeRef->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then);
		TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionPostEvaluateNode>(this, ThirdNodeRef->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then);
	}

	{
		TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_IntermediateEntryNode>(this, FourthNodeIntermediateRef->GetBoundGraph(), USMGraphK2Schema::PN_Then);
		TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_StateUpdateNode>(this, FourthNodeIntermediateRef->GetBoundGraph(), USMGraphK2Schema::PN_Then);
		TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_StateEndNode>(this, FourthNodeIntermediateRef->GetBoundGraph(), USMGraphK2Schema::PN_Then);

		TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_IntermediateStateMachineStartNode>(this, FourthNodeIntermediateRef->GetBoundGraph(), USMGraphK2Schema::PN_Then);
	}

	{
		TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionEnteredNode>(this, FourthNodeIntermediateRef->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then);
		TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionInitializedNode>(this, FourthNodeIntermediateRef->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then);
		TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionShutdownNode>(this, FourthNodeIntermediateRef->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then);
		TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionPreEvaluateNode>(this, FourthNodeIntermediateRef->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then);
		TestHelpers::VerifyNodeWiredFromPin<USMGraphK2Node_TransitionPostEvaluateNode>(this, FourthNodeIntermediateRef->GetNextTransition()->GetBoundGraph(), USMGraphK2Schema::PN_Then);
	}

	Instance = TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits2, UpdateHits2, EndHits2);

	Context = CastChecked<USMTestContext>(Instance->GetContext());
	TestEqual("Hits match", Context->TestTransitionPreEval.Count, PreEval);
	TestEqual("Hits match", Context->TestTransitionPostEval.Count, PostEval);
	
	TestEqual("Hits match", EntryHits2, EntryHits);
	TestEqual("Hits match", UpdateHits2, UpdateHits);
	TestEqual("Hits match", EndHits2, EndHits);
	
	return NewAsset.DeleteAsset(this);
}

#endif

#endif //WITH_DEV_AUTOMATION_TESTS