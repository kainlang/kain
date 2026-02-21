// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Helpers/SMTestBoilerplate.h"
#include "SMTestContext.h"
#include "SMTestHelpers.h"

#include "Configuration/SMProjectEditorSettings.h"
#include "Graph/Nodes/SMGraphK2Node_StateMachineNode.h"
#include "Graph/Nodes/SMGraphNode_StateNode.h"
#include "Graph/SMGraph.h"
#include "Utilities/SMBlueprintEditorUtils.h"
#include "Utilities/SMNodeInstanceUtils.h"

#include "Blueprints/SMBlueprint.h"

#include "Kismet2/KismetEditorUtilities.h"

#if WITH_DEV_AUTOMATION_TESTS

#if PLATFORM_DESKTOP

/**
 * Test OnPreCompileValidate with Log().
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNodeInstanceValidationTest, "LogicDriver.NodeInstance.Validation.PreCompileValidateLog", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FNodeInstanceValidationTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(1)

	USMProjectEditorSettings* Settings = FSMBlueprintEditorUtils::GetMutableProjectEditorSettings();
	const auto CurrentCSSetting = Settings->EditorNodeConstructionScriptSetting;
	Settings->EditorNodeConstructionScriptSetting = ESMEditorConstructionScriptProjectSetting::SM_Standard;

	UEdGraphPin* LastStatePin = nullptr;

	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMTestPreCompileState::StaticClass());
	const USMGraphNode_StateNode* StateNode = FSMBlueprintEditorUtils::GetFirstNodeOfClassNested<USMGraphNode_StateNode>(StateMachineGraph);
	check(StateNode);

	USMTestPreCompileState* NodeTemplate = StateNode->GetNodeTemplateAs<USMTestPreCompileState>();
	check(NodeTemplate);

	// Error
	{
		NodeTemplate->LogMessage = TEXT("An error message!");
		NodeTemplate->LogType = ESMCompilerLogType::Error;

		AddExpectedError(NodeTemplate->LogMessage);
		FKismetEditorUtilities::CompileBlueprint(NewBP);
	}

	// Off
	{
		NewBP->bEnableNodeValidation = false;
		FKismetEditorUtilities::CompileBlueprint(NewBP);
		NewBP->bEnableNodeValidation = true;
	}

	// Warning
	{
		NodeTemplate->LogMessage = TEXT("A warning message!");
		NodeTemplate->LogType = ESMCompilerLogType::Warning;

		AddExpectedError(NodeTemplate->LogMessage);
		FKismetEditorUtilities::CompileBlueprint(NewBP);
	}

	// Note
	{
		NodeTemplate->LogMessage = TEXT("A note message!");
		NodeTemplate->LogType = ESMCompilerLogType::Note;

		FKismetEditorUtilities::CompileBlueprint(NewBP);
	}

	Settings->EditorNodeConstructionScriptSetting = CurrentCSSetting;

	return NewAsset.DeleteAsset(this);
}

/**
 * Test OnPreCompileValidate with LogProperty().
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNodeInstancePropertyValidationTest, "LogicDriver.NodeInstance.Validation.PreCompileValidateLogProperty", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FNodeInstancePropertyValidationTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(1)

	USMProjectEditorSettings* Settings = FSMBlueprintEditorUtils::GetMutableProjectEditorSettings();
	const auto CurrentCSSetting = Settings->EditorNodeConstructionScriptSetting;
	Settings->EditorNodeConstructionScriptSetting = ESMEditorConstructionScriptProjectSetting::SM_Standard;

	UEdGraphPin* LastStatePin = nullptr;

	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMTestPreCompileState::StaticClass());
	const USMGraphNode_StateNode* StateNode = FSMBlueprintEditorUtils::GetFirstNodeOfClassNested<USMGraphNode_StateNode>(StateMachineGraph);
	check(StateNode);

	USMTestPreCompileState* NodeTemplate = StateNode->GetNodeTemplateAs<USMTestPreCompileState>();
	check(NodeTemplate);

	// Property log error
	{
		NodeTemplate->bLogProperty = true;
		NodeTemplate->LogMessage = TEXT("A Property error message!");
		NodeTemplate->LogType = ESMCompilerLogType::Error;

		AddExpectedError(NodeTemplate->LogMessage);
		FKismetEditorUtilities::CompileBlueprint(NewBP);
	}

	// Property log silent
	{
		NodeTemplate->bLogProperty = true;
		NodeTemplate->bLogPropertySilent = true;
		NodeTemplate->LogMessage = TEXT("A Property error message!");
		NodeTemplate->LogType = ESMCompilerLogType::Error;

		FKismetEditorUtilities::CompileBlueprint(NewBP);
	}

	Settings->EditorNodeConstructionScriptSetting = CurrentCSSetting;

	return NewAsset.DeleteAsset(this);
}

#endif

#endif //WITH_DEV_AUTOMATION_TESTS