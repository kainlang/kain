// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Helpers/SMTestBoilerplate.h"
#include "SMTestContext.h"
#include "SMTestHelpers.h"

#include "Configuration/SMProjectEditorSettings.h"
#include "Graph/Nodes/SMGraphK2Node_StateMachineNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineEntryNode.h"
#include "Graph/Nodes/SMGraphNode_StateNode.h"
#include "Graph/Schema/SMGraphK2Schema.h"
#include "Graph/SMGraph.h"
#include "Utilities/SMBlueprintEditorUtils.h"
#include "Utilities/SMNodeInstanceUtils.h"
#include "Utilities/SMPropertyUtils.h"
#include "Utilities/SMTextUtils.h"

#include "Configuration/SMTextGraphEditorSettings.h"
#include "Graph/Nodes/PropertyNodes/SMGraphK2Node_TextPropertyNode.h"
#include "Graph/SMTextPropertyGraph.h"

#include "Blueprints/SMBlueprint.h"
#include "SMUtils.h"

#include "ISinglePropertyView.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "PropertyHandle.h"

#if WITH_DEV_AUTOMATION_TESTS

#if PLATFORM_DESKTOP

static bool TestTextAndLocalization(const FText& InTextA, const FText& InTextB)
{
	return LD::TextUtils::DoesTextValueAndLocalizationMatch(InTextA, InTextB);
}

/**
 * Test text graph properties and make sure they format variables correctly.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSMTextGraphPropertyBlueprintVariableTest, "LogicDriver.TextGraphProperty.BlueprintVariableFormat", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FSMTextGraphPropertyBlueprintVariableTest::RunTest(const FString& Parameters)
{
	FAssetHandler NewAsset;
	if (!TestHelpers::TryCreateNewStateMachineAsset(this, NewAsset, false))
	{
		return false;
	}

	USMBlueprint* NewBP = NewAsset.GetObjectAs<USMBlueprint>();

	// Create variables
	
	FName StrVar = "StrVar";
	FString StrVarValue = "TestString";
	FEdGraphPinType StrPinType;
	StrPinType.PinCategory = USMGraphK2Schema::PC_String;
	FBlueprintEditorUtils::AddMemberVariable(NewBP, StrVar, StrPinType, StrVarValue);
	
	FName IntVar = "IntVar";
	FString IntVarValue = "5";
	FEdGraphPinType IntPinType;
	IntPinType.PinCategory = USMGraphK2Schema::PC_Int;
	FBlueprintEditorUtils::AddMemberVariable(NewBP, IntVar, IntPinType, IntVarValue);

	USMTestObject* TestObject = NewObject<USMTestObject>();
	TestObject->AddToRoot();
	
	FName ObjVar = "ObjVar";
	FEdGraphPinType ObjPinType;
	ObjPinType.PinCategory = USMGraphK2Schema::PC_Object;
	ObjPinType.PinSubCategoryObject = TestObject->GetClass();
	FBlueprintEditorUtils::AddMemberVariable(NewBP, ObjVar, ObjPinType);

	FText NewText = FText::FromString("Hello, {StrVar}! How about {IntVar}? What about no parsing like `{IntVar}? But can I parse the object with a custom to text method? Object: {ObjVar}");
	FText ExpectedText = FText::FromString(FString::Printf(TEXT("Hello, %s! How about %s? What about no parsing like {%s}? But can I parse the object with a custom to text method? Object: %s"),
		*StrVarValue, *IntVarValue, *IntVar.ToString(), *TestObject->CustomToText().ToString()));
	FText ExpectedTextGlobalSetting = FText::FromString(FString::Printf(TEXT("Hello, %s! How about %s? What about no parsing like {%s}? But can I parse the object with a custom to text method? Object: %s"),
		*StrVarValue, *IntVarValue, *IntVar.ToString(), *TestObject->GlobalCustomToText().ToString()));

	// Find root state machine.
	USMGraphK2Node_StateMachineNode* RootStateMachineNode = FSMBlueprintEditorUtils::GetRootStateMachineNode(NewBP);

	// Find the state machine graph.
	USMGraph* StateMachineGraph = RootStateMachineNode->GetStateMachineGraph();

	// Total states to test.
	int32 TotalStates = 1;

	UEdGraphPin* LastStatePin = nullptr;

	// Build single state - state machine.
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMTextGraphState::StaticClass(), USMTransitionTestInstance::StaticClass());
	if (!NewAsset.SaveAsset(this))
	{
		return false;
	}

	USMGraphNode_StateNode* StateNode = CastChecked<USMGraphNode_StateNode>(StateMachineGraph->GetEntryNode()->GetOutputNode());
	auto PropertyNodes = StateNode->GetAllPropertyGraphNodesAsArray();

	TestEqual("Only one property exposed on node", PropertyNodes.Num(), 1);

	USMGraphK2Node_TextPropertyNode* TextPropertyNode = CastChecked<USMGraphK2Node_TextPropertyNode>(PropertyNodes[0]);
	USMTextPropertyGraph* PropertyGraph = CastChecked<USMTextPropertyGraph>(TextPropertyNode->GetPropertyGraph());
	
	USMNodeInstance* NodeTemplate = StateNode->GetNodeTemplate();
	FProperty* TextGraphFProperty = NodeTemplate->GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(USMTextGraphState, TextGraph));
	check(TextGraphFProperty);
	
	TArray<FSMGraphProperty_Base*> GraphProperties;

	// Get the real graph property.
	USMUtils::BlueprintPropertyToNativeProperty(TextGraphFProperty, NodeTemplate, GraphProperties);
	check(GraphProperties.Num() == 1);

	FSMTextGraphProperty* TextGraphProperty = static_cast<FSMTextGraphProperty*>(GraphProperties[0]);
	
	// Test old text conversion.
	{
		TextGraphProperty->TextSerializer.ToTextFunctionNames.Add(GET_FUNCTION_NAME_CHECKED(USMTestObject, CustomToText));
		
		TestEqual("Default text graph value set", PropertyGraph->GetRichTextBody().ToString(), USMTextGraphState::DefaultText.ToString());
	
		PropertyGraph->SetNewText(NewText);

		// Run and check results.
		FKismetEditorUtilities::CompileBlueprint(NewBP);
		USMTestContext* Context = NewObject<USMTestContext>();
		USMInstance* Instance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);

		FObjectProperty* ObjProperty = FindFieldChecked<FObjectProperty>(Instance->GetClass(), ObjVar);
		ObjProperty->SetObjectPropertyValue_InContainer(Instance, TestObject);

		Instance->Start();
	
		USMTextGraphState* NodeInstance = CastChecked<USMTextGraphState>(Instance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
		TestEqual("Text graph evaluated manually", NodeInstance->EvaluatedText.ToString(), ExpectedText.ToString());

		Instance->Shutdown();
		Instance->ConditionalBeginDestroy();
	}

	// Test dynamic text conversion.
	{
		// Dynamic function on node defaults -- overwrites global setting
		{
			TextGraphProperty->TextSerializer.ToTextFunctionNames.Empty();
			TextGraphProperty->TextSerializer.ToTextDynamicFunctionName = GET_FUNCTION_NAME_CHECKED(USMTestObject, CustomToText);
			
			// Run and check results.
			FKismetEditorUtilities::CompileBlueprint(NewBP);
			USMTestContext* Context = NewObject<USMTestContext>();
			USMInstance* Instance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);

			FObjectProperty* ObjProperty = FindFieldChecked<FObjectProperty>(Instance->GetClass(), ObjVar);
			ObjProperty->SetObjectPropertyValue_InContainer(Instance, TestObject);

			Instance->Start();

			USMTextGraphState* NodeInstance = CastChecked<USMTextGraphState>(Instance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
			TestEqual("Text graph evaluated manually", NodeInstance->EvaluatedText.ToString(), ExpectedText.ToString());

			Instance->Shutdown();
			Instance->ConditionalBeginDestroy();
		}
		
		// Dynamic function on global setting
		{
			USMTextGraphEditorSettings* TextGraphEditorSettings = GetMutableDefault<USMTextGraphEditorSettings>();
			const FName ExistingSetting = TextGraphEditorSettings->ToTextDynamicFunctionName;
			TextGraphEditorSettings->ToTextDynamicFunctionName = GET_FUNCTION_NAME_CHECKED(USMTestObject, GlobalCustomToText);

			TextGraphProperty->TextSerializer.ToTextDynamicFunctionName = NAME_None;
			FKismetEditorUtilities::CompileBlueprint(NewBP);

			USMTestContext* Context = NewObject<USMTestContext>();
			USMInstance* Instance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);

			FObjectProperty* ObjProperty = FindFieldChecked<FObjectProperty>(Instance->GetClass(), ObjVar);
			ObjProperty->SetObjectPropertyValue_InContainer(Instance, TestObject);

			Instance->Start();

			USMTextGraphState* NodeInstance = CastChecked<USMTextGraphState>(Instance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
			TestEqual("Text graph evaluated manually", NodeInstance->EvaluatedText.ToString(), ExpectedTextGlobalSetting.ToString());

			Instance->Shutdown();
			Instance->ConditionalBeginDestroy();

			TextGraphEditorSettings->ToTextDynamicFunctionName = ExistingSetting;
		}
	}

	TestObject->RemoveFromRoot();
	
	return NewAsset.DeleteAsset(this);
}

/**
 * Test text graph properties and make sure they format native variables correctly.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSMTextGraphPropertyNativeVariableTest, "LogicDriver.TextGraphProperty.NativeVariableFormat", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FSMTextGraphPropertyNativeVariableTest::RunTest(const FString& Parameters)
{
	FAssetHandler NewAsset;
	if (!TestHelpers::TryCreateNewStateMachineAsset(this, NewAsset, false, USMTestDialogueInstance::StaticClass()))
	{
		return false;
	}

	USMBlueprint* NewBP = NewAsset.GetObjectAs<USMBlueprint>();

	USMTestObject* TestObject = NewObject<USMTestObject>();
	TestObject->AddToRoot();

	// Used for extracting expected values.
	USMTestDialogueInstance* DummyInstance = NewObject<USMTestDialogueInstance>();
	
	FText NewText = FText::FromString(FString::Printf(TEXT("Native property: {%s}, native function: {%s}."),
		GET_MEMBER_NAME_STRING_CHECKED(USMTestDialogueInstance, TextVar), GET_FUNCTION_NAME_STRING_CHECKED(USMTestDialogueInstance, GetTextFunction)));
	FText ExpectedText = FText::FromString(FString::Printf(TEXT("Native property: %s, native function: %s."),
		*DummyInstance->TextVar.ToString(), *DummyInstance->GetTextFunction().ToString()));

	// Find root state machine.
	USMGraphK2Node_StateMachineNode* RootStateMachineNode = FSMBlueprintEditorUtils::GetRootStateMachineNode(NewBP);

	// Find the state machine graph.
	USMGraph* StateMachineGraph = RootStateMachineNode->GetStateMachineGraph();

	// Total states to test.
	int32 TotalStates = 1;

	UEdGraphPin* LastStatePin = nullptr;

	// Build single state - state machine.
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMTextGraphState::StaticClass(), USMTransitionTestInstance::StaticClass());
	if (!NewAsset.SaveAsset(this))
	{
		return false;
	}

	USMGraphNode_StateNode* StateNode = CastChecked<USMGraphNode_StateNode>(StateMachineGraph->GetEntryNode()->GetOutputNode());
	auto PropertyNodes = StateNode->GetAllPropertyGraphNodesAsArray();

	TestEqual("Only one property exposed on node", PropertyNodes.Num(), 1);

	USMGraphK2Node_TextPropertyNode* TextPropertyNode = CastChecked<USMGraphK2Node_TextPropertyNode>(PropertyNodes[0]);
	USMTextPropertyGraph* PropertyGraph = CastChecked<USMTextPropertyGraph>(TextPropertyNode->GetPropertyGraph());
	
	USMNodeInstance* NodeTemplate = StateNode->GetNodeTemplate();
	FProperty* TextGraphFProperty = NodeTemplate->GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(USMTextGraphState, TextGraph));
	check(TextGraphFProperty);
	
	TArray<FSMGraphProperty_Base*> GraphProperties;

	// Get the real graph property.
	USMUtils::BlueprintPropertyToNativeProperty(TextGraphFProperty, NodeTemplate, GraphProperties);
	check(GraphProperties.Num() == 1);

	FSMTextGraphProperty* TextGraphProperty = static_cast<FSMTextGraphProperty*>(GraphProperties[0]);
	
	{
		PropertyGraph->SetNewText(NewText);

		// Run and check results.
		FKismetEditorUtilities::CompileBlueprint(NewBP);
		USMTestContext* Context = NewObject<USMTestContext>();
		USMInstance* Instance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);

		Instance->Start();
	
		USMTextGraphState* NodeInstance = CastChecked<USMTextGraphState>(Instance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
		TestEqual("Text graph evaluated manually", NodeInstance->EvaluatedText.ToString(), ExpectedText.ToString());

		Instance->Shutdown();
		Instance->ConditionalBeginDestroy();
	}

	return NewAsset.DeleteAsset(this);
}

/**
 * Test text graph properties and make sure they format variables correctly while used in a state machine reference.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSMTextGraphPropertyVariableInReferenceTest, "LogicDriver.TextGraphProperty.VariableFormatInReference", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FSMTextGraphPropertyVariableInReferenceTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST_NO_STATES()

	// TODO: Combine logic with FSMTextGraphPropertyVariableTest since this is mostly the same apart from using references.

	constexpr int32 TotalStatesBeforeReferences = 1;
	constexpr int32 TotalStatesAfterReferences = 0;
	constexpr int32 TotalNestedStates = 1;
	constexpr int32 TotalReferences = 1;

	TArray<FAssetHandler> ReferencedAssets;
	TArray<USMGraphNode_StateMachineStateNode*> NestedStateMachineNodes;
	
	TestHelpers::BuildStateMachineWithReferences(this, StateMachineGraph, TotalStatesBeforeReferences, TotalStatesAfterReferences,
		TotalReferences, TotalNestedStates, ReferencedAssets, NestedStateMachineNodes);

	check(ReferencedAssets.Num() == 1);
	
	USMBlueprint* ReferencedBP = ReferencedAssets[0].GetObjectAs<USMBlueprint>();

	// Create variables

	FName StrVar = "StrVar";
	FString StrVarValue = "TestString";
	FEdGraphPinType StrPinType;
	StrPinType.PinCategory = USMGraphK2Schema::PC_String;
	FBlueprintEditorUtils::AddMemberVariable(ReferencedBP, StrVar, StrPinType, StrVarValue);

	FName IntVar = "IntVar";
	FString IntVarValue = "5";
	FEdGraphPinType IntPinType;
	IntPinType.PinCategory = USMGraphK2Schema::PC_Int;
	FBlueprintEditorUtils::AddMemberVariable(ReferencedBP, IntVar, IntPinType, IntVarValue);

	USMTestObject* TestObject = NewObject<USMTestObject>();
	TestObject->AddToRoot();

	FName ObjVar = "ObjVar";
	FEdGraphPinType ObjPinType;
	ObjPinType.PinCategory = USMGraphK2Schema::PC_Object;
	ObjPinType.PinSubCategoryObject = TestObject->GetClass();
	FBlueprintEditorUtils::AddMemberVariable(ReferencedBP, ObjVar, ObjPinType);

	FText NewText = FText::FromString("Hello, {StrVar}! How about {IntVar}? What about no parsing like `{IntVar}? But can I parse the object with a custom to text method? Object: {ObjVar}");
	FText ExpectedText = FText::FromString(FString::Printf(TEXT("Hello, %s! How about %s? What about no parsing like {%s}? But can I parse the object with a custom to text method? Object: %s"),
		*StrVarValue, *IntVarValue, *IntVar.ToString(), *TestObject->CustomToText().ToString()));
	FText ExpectedTextGlobalSetting = FText::FromString(FString::Printf(TEXT("Hello, %s! How about %s? What about no parsing like {%s}? But can I parse the object with a custom to text method? Object: %s"),
		*StrVarValue, *IntVarValue, *IntVar.ToString(), *TestObject->GlobalCustomToText().ToString()));

	// Find root state machine.
	USMGraphK2Node_StateMachineNode* ReferencedRootStateMachineNode = FSMBlueprintEditorUtils::GetRootStateMachineNode(ReferencedBP);

	// Find the state machine graph.
	USMGraph* ReferencedStateMachineGraph = ReferencedRootStateMachineNode->GetStateMachineGraph();

	USMGraphNode_StateNode* StateNode = CastChecked<USMGraphNode_StateNode>(ReferencedStateMachineGraph->GetEntryNode()->GetOutputNode());
	TestHelpers::SetNodeClass(this, StateNode, USMTextGraphState::StaticClass());
	FKismetEditorUtilities::CompileBlueprint(ReferencedBP);

	auto PropertyNodes = StateNode->GetAllPropertyGraphNodesAsArray();

	TestEqual("Only one property exposed on node", PropertyNodes.Num(), 1);

	USMGraphK2Node_TextPropertyNode* TextPropertyNode = CastChecked<USMGraphK2Node_TextPropertyNode>(PropertyNodes[0]);
	USMTextPropertyGraph* PropertyGraph = CastChecked<USMTextPropertyGraph>(TextPropertyNode->GetPropertyGraph());

	USMNodeInstance* NodeTemplate = StateNode->GetNodeTemplate();
	FProperty* TextGraphFProperty = NodeTemplate->GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(USMTextGraphState, TextGraph));
	check(TextGraphFProperty);

	TArray<FSMGraphProperty_Base*> GraphProperties;

	// Get the real graph property.
	USMUtils::BlueprintPropertyToNativeProperty(TextGraphFProperty, NodeTemplate, GraphProperties);
	check(GraphProperties.Num() == 1);

	FSMTextGraphProperty* TextGraphProperty = static_cast<FSMTextGraphProperty*>(GraphProperties[0]);

	// Test old text conversion.
	{
		TextGraphProperty->TextSerializer.ToTextFunctionNames.Add(GET_FUNCTION_NAME_CHECKED(USMTestObject, CustomToText));

		TestEqual("Default text graph value set", PropertyGraph->GetRichTextBody().ToString(), USMTextGraphState::DefaultText.ToString());

		PropertyGraph->SetNewText(NewText);

		// Run and check results.
		FKismetEditorUtilities::CompileBlueprint(ReferencedBP);
		FKismetEditorUtilities::CompileBlueprint(NewBP);
		USMTestContext* Context = NewObject<USMTestContext>();
		USMInstance* Instance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);

		USMInstance* Reference = Instance->GetAllReferencedInstances(true)[0];

		FObjectProperty* ObjProperty = FindFieldChecked<FObjectProperty>(Reference->GetClass(), ObjVar);
		ObjProperty->SetObjectPropertyValue_InContainer(Reference, TestObject);

		Instance->Start();
		Instance->Update();

		USMTextGraphState* NodeInstance = CastChecked<USMTextGraphState>(Instance->GetSingleActiveStateInstance());
		TestEqual("Text graph evaluated manually", NodeInstance->EvaluatedText.ToString(), ExpectedText.ToString());

		Instance->Shutdown();
		Instance->ConditionalBeginDestroy();
	}

	TestObject->RemoveFromRoot();

	ReferencedAssets[0].DeleteAsset(this);
	return NewAsset.DeleteAsset(this);
}

/**
 * Test text graph properties and make sure some rich style formatting is correct. This won't test style sets or decorators
 * but checks how Logic Driver's variable processing and formatting works when combined with rich text.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSMTextGraphRichStyleTest, "LogicDriver.TextGraphProperty.RichStyle", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FSMTextGraphRichStyleTest::RunTest(const FString& Parameters)
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
	int32 TotalStates = 1;

	UEdGraphPin* LastStatePin = nullptr;

	// Build single state - state machine.
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMTextGraphState::StaticClass(), USMTransitionTestInstance::StaticClass());
	if (!NewAsset.SaveAsset(this))
	{
		return false;
	}

	USMGraphNode_StateNode* StateNode = CastChecked<USMGraphNode_StateNode>(StateMachineGraph->GetEntryNode()->GetOutputNode());
	auto PropertyNodes = StateNode->GetAllPropertyGraphNodesAsArray();

	TestEqual("Only one property exposed on node", PropertyNodes.Num(), 1);

	USMGraphK2Node_TextPropertyNode* TextPropertyNode = CastChecked<USMGraphK2Node_TextPropertyNode>(PropertyNodes[0]);
	static_cast<FSMTextGraphProperty*>(TextPropertyNode->GetPropertyNodeChecked())->TextSerializer.ToTextFunctionNames.Empty();
	static_cast<FSMTextGraphProperty*>(TextPropertyNode->GetPropertyNodeChecked())->TextSerializer.ToTextDynamicFunctionName = GET_FUNCTION_NAME_CHECKED(USMTestObject, CustomToText);

	USMTextPropertyGraph* PropertyGraph = CastChecked<USMTextPropertyGraph>(TextPropertyNode->GetPropertyGraph());

	// Don't use actual variables in the BP or a GUID will be created we can't easily account for.

	// Test standard variable and rich text.
	{
		const FText NewText = FText::FromString("Hello, {StrVar}! <RichStyle>Text</>");
		const FString ExpectedText = TEXT("Hello, <property id=\"property\" buttonstyle=\"SMExtendedEditor.Graph.Property.Button\" textstyle=\"SMExtendedEditor.Graph.Property.Text\" property=\"StrVar\"></>! <RichStyle>Text</>");

		PropertyGraph->SetNewText(NewText);

		const FString PlainText = PropertyGraph->GetPlainTextBody().ToString();
		const FString RichText = PropertyGraph->GetRichTextBody().ToString();

		TestEqual("Rich string is correct", RichText, ExpectedText);
	}

	// Test property inside of rich text which won't change.
	{
		const FText NewText = FText::FromString("Hello, {StrVar}! <RichStyle>{NotChanged}Text</>");
		const FString ExpectedText = TEXT("Hello, <property id=\"property\" buttonstyle=\"SMExtendedEditor.Graph.Property.Button\" textstyle=\"SMExtendedEditor.Graph.Property.Text\" property=\"StrVar\"></>! <RichStyle>{NotChanged}Text</>");

		PropertyGraph->SetNewText(NewText);

		const FString PlainText = PropertyGraph->GetPlainTextBody().ToString();
		const FString RichText = PropertyGraph->GetRichTextBody().ToString();

		TestEqual("Rich string is correct", RichText, ExpectedText);
	}

	// Test property that has a new line.
	{
		const FText NewText = FText::FromString("Hello, {Str\nVar}!");
		const FString ExpectedText = NewText.ToString();

		PropertyGraph->SetNewText(NewText);

		const FString PlainText = PropertyGraph->GetPlainTextBody().ToString();
		const FString RichText = PropertyGraph->GetRichTextBody().ToString();

		TestEqual("Rich string is correct", RichText, ExpectedText);
	}

	AddExpectedError(TEXT("has a variable parsing error."), EAutomationExpectedErrorFlags::MatchType::Contains, 2);

	// Test property with parsing error - nested brackets.
	{
		const FText NewText = FText::FromString("Hello, {StrVar {StrVar}}!");
		const FString ExpectedText = NewText.ToString();

		PropertyGraph->SetNewText(NewText);

		const FString PlainText = PropertyGraph->GetPlainTextBody().ToString();
		const FString RichText = PropertyGraph->GetRichTextBody().ToString();

		TestEqual("Rich string is correct", RichText, ExpectedText);

		FKismetEditorUtilities::CompileBlueprint(NewBP);
	}

	// Test property with parsing error - missing bracket.
	{
		const FText NewText = FText::FromString("Hello, {StrVar!");
		const FString ExpectedText = NewText.ToString();

		PropertyGraph->SetNewText(NewText);

		const FString PlainText = PropertyGraph->GetPlainTextBody().ToString();
		const FString RichText = PropertyGraph->GetRichTextBody().ToString();

		TestEqual("Rich string is correct", RichText, ExpectedText);

		FKismetEditorUtilities::CompileBlueprint(NewBP);
	}

	return NewAsset.DeleteAsset(this);
}

/**
 * Test text graph properties as an array and that they can read their defaults.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSMTextGraphPropertyArrayTest, "LogicDriver.TextGraphProperty.Array", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FSMTextGraphPropertyArrayTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(1)
	UEdGraphPin* LastStatePin = nullptr;

	// Build single state - state machine.
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin,
		USMTextGraphArrayState::StaticClass(), USMTransitionTestInstance::StaticClass());

	FKismetEditorUtilities::CompileBlueprint(NewBP);
	
	const USMGraphNode_StateNode* StateNode = CastChecked<USMGraphNode_StateNode>(StateMachineGraph->GetEntryNode()->GetOutputNode());
	auto PropertyNodes = StateNode->GetAllPropertyGraphNodesAsArray();

	TestEqual("Two properties exposed on node", PropertyNodes.Num(), 2);
	
	{
		const USMGraphK2Node_TextPropertyNode* TextPropertyNode = CastChecked<USMGraphK2Node_TextPropertyNode>(PropertyNodes[0]);
		const USMTextPropertyGraph* PropertyGraph = CastChecked<USMTextPropertyGraph>(TextPropertyNode->GetPropertyGraph());
		TestTrue("Text and localization correct for element 1",
			TestTextAndLocalization(PropertyGraph->GetFormatTextNodeText(), USMTextGraphArrayState::DefaultText_1));
	}
	{
		
		const USMGraphK2Node_TextPropertyNode* TextPropertyNode = CastChecked<USMGraphK2Node_TextPropertyNode>(PropertyNodes[1]);
		const USMTextPropertyGraph* PropertyGraph = CastChecked<USMTextPropertyGraph>(TextPropertyNode->GetPropertyGraph());
		TestTrue("Text and localization correct for element 2",
			TestTextAndLocalization(PropertyGraph->GetFormatTextNodeText(), USMTextGraphArrayState::DefaultText_2));
	}
	// Run and check results.
	FKismetEditorUtilities::CompileBlueprint(NewBP);
	USMTestContext* Context = NewObject<USMTestContext>();
	USMInstance* Instance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);

	Instance->Start();

	USMTextGraphArrayState* NodeInstance = CastChecked<USMTextGraphArrayState>(Instance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());
	check(TestEqual("Text graph evaluated manually", NodeInstance->EvaluatedTextArray.Num(), 2));
	
	TestEqual("Text graph evaluated elem 1", NodeInstance->EvaluatedTextArray[0].ToString(), USMTextGraphArrayState::DefaultText_1.ToString());
	TestEqual("Text graph evaluated elem 2", NodeInstance->EvaluatedTextArray[1].ToString(), USMTextGraphArrayState::DefaultText_2.ToString());
	
	Instance->Shutdown();
	
	return NewAsset.DeleteAsset(this);
}

static USMGraphK2Node_TextPropertyNode* GetCurrentInstanceNode(USMBlueprint* NewBP)
{
	const USMGraph* StateMachineGraph = FSMBlueprintEditorUtils::GetRootStateMachineGraph(NewBP);
	check(StateMachineGraph);

	const USMGraphNode_StateNode* StateNode = CastChecked<USMGraphNode_StateNode>(StateMachineGraph->GetEntryNode()->GetOutputNode());
	auto PropertyNodes = StateNode->GetAllPropertyGraphNodesAsArray();

	USMGraphK2Node_TextPropertyNode* TextPropertyNode = CastChecked<USMGraphK2Node_TextPropertyNode>(PropertyNodes[0]);
	return TextPropertyNode;
}

static USMTextPropertyGraph* GetCurrentInstanceGraph(USMBlueprint* NewBP)
{
	const USMGraphK2Node_TextPropertyNode* TextPropertyNode = GetCurrentInstanceNode(NewBP);
	USMTextPropertyGraph* PropertyGraph = CastChecked<USMTextPropertyGraph>(TextPropertyNode->GetPropertyGraph());
	return PropertyGraph;
}

static FText GetCurrentInstanceText(USMBlueprint* NewBP)
{
	const USMTextPropertyGraph* PropertyGraph = GetCurrentInstanceGraph(NewBP);
	return PropertyGraph->GetFormatTextNodeText();
}

static void TestLocalizationPropagation(FAutomationTestBase* InTest, USMBlueprint* StateMachineBP, USMNodeBlueprint* NodeBP, const FText& NewCDOText, bool bShouldMatch,
	FText* SetInstanceText = nullptr, bool bRestoreOldValue = true)
{
	FKismetEditorUtilities::CompileBlueprint(StateMachineBP);
	
	const USMGraph* StateMachineGraph = FSMBlueprintEditorUtils::GetRootStateMachineGraph(StateMachineBP);
	check(StateMachineGraph);
	
	// Validate instance values before CDO change
	{
		const USMGraphNode_StateNode* StateNode = CastChecked<USMGraphNode_StateNode>(StateMachineGraph->GetEntryNode()->GetOutputNode());
		auto PropertyNodes = StateNode->GetAllPropertyGraphNodesAsArray();
	
		const USMGraphK2Node_TextPropertyNode* TextPropertyNode = CastChecked<USMGraphK2Node_TextPropertyNode>(PropertyNodes[0]);
		USMTextPropertyGraph* PropertyGraph = CastChecked<USMTextPropertyGraph>(TextPropertyNode->GetPropertyGraph());
		
		InTest->TestEqual("Default text graph value set", PropertyGraph->GetRichTextBody().ToString(), USMTextGraphState::DefaultText.ToString());
		InTest->TestTrue("Text graph is default", TextPropertyNode->IsValueSetToDefault());
		
		if (SetInstanceText)
		{
			PropertyGraph->SetNewText(*SetInstanceText);
		}
	}
	
	USMTextGraphState* CDO = CastChecked<USMTextGraphState>(NodeBP->GeneratedClass->ClassDefaultObject);
	InTest->TestTrue("Default CDO text graph value set", TestTextAndLocalization(CDO->TextGraph.Result, USMTextGraphState::DefaultText));

	const TSharedPtr<ISinglePropertyView> PropView = LD::PropertyUtils::CreatePropertyViewForProperty(
		CDO, GET_MEMBER_NAME_CHECKED(USMTextGraphState, TextGraph));

	const TSharedPtr<IPropertyHandle> CDOResultHandle = PropView->GetPropertyHandle()->GetChildHandle(GET_MEMBER_NAME_CHECKED(FSMTextGraphProperty, Result));
	check(CDOResultHandle.IsValid());
	CDOResultHandle->SetValue(NewCDOText);
	
	// Validate instance values after CDO change
	{
		const USMGraphNode_StateNode* StateNode = CastChecked<USMGraphNode_StateNode>(StateMachineGraph->GetEntryNode()->GetOutputNode());
		auto PropertyNodes = StateNode->GetAllPropertyGraphNodesAsArray();
		
		const USMGraphK2Node_TextPropertyNode* TextPropertyNode = CastChecked<USMGraphK2Node_TextPropertyNode>(PropertyNodes[0]);
		const USMTextPropertyGraph* PropertyGraph = CastChecked<USMTextPropertyGraph>(TextPropertyNode->GetPropertyGraph());

		if (bShouldMatch)
		{
			InTest->TestTrue("Text and localization correct", TestTextAndLocalization(PropertyGraph->GetFormatTextNodeText(), NewCDOText));
			InTest->TestEqual("Default text graph value set", PropertyGraph->GetRichTextBody().ToString(), NewCDOText.ToString());
			InTest->TestTrue("Text graph is default", TextPropertyNode->IsValueSetToDefault());
		}
		else
		{
			// Presumably instance values have changed here.
			InTest->TestFalse("Text and localization correct", TestTextAndLocalization(PropertyGraph->GetFormatTextNodeText(), NewCDOText));
			InTest->TestFalse("Text graph is default", TextPropertyNode->IsValueSetToDefault());
		}
	}

	// Restore old CDO value so future tests complete properly.
	if (bRestoreOldValue)
	{
		CDOResultHandle->SetValue(USMTextGraphState::DefaultText);
	}

	if (SetInstanceText)
	{
		// Validate instance values are still correct
		const USMGraphNode_StateNode* StateNode = CastChecked<USMGraphNode_StateNode>(StateMachineGraph->GetEntryNode()->GetOutputNode());
		auto PropertyNodes = StateNode->GetAllPropertyGraphNodesAsArray();

		const USMGraphK2Node_TextPropertyNode* TextPropertyNode = CastChecked<USMGraphK2Node_TextPropertyNode>(PropertyNodes[0]);
		const USMTextPropertyGraph* PropertyGraph = CastChecked<USMTextPropertyGraph>(TextPropertyNode->GetPropertyGraph());
	
		InTest->TestEqual("Default text graph value set", PropertyGraph->GetRichTextBody().ToString(), SetInstanceText->ToString());
		InTest->TestFalse("Text graph is default", TextPropertyNode->IsValueSetToDefault());
	}
}

#define CREATE_TEXTNODE_ASSET() \
FAssetHandler StateAsset; \
if (!TestHelpers::TryCreateNewNodeAsset(this, StateAsset, USMTextGraphState::StaticClass(), true)) \
{ \
	return false; \
} \
USMNodeBlueprint* NodeBP = StateAsset.GetObjectAs<USMNodeBlueprint>(); \
FKismetEditorUtilities::CompileBlueprint(NodeBP);

/**
 * Test TextGraph Localization propagation using FText::FromString.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSMTextGraphPropertyLocalizationStringTest, "LogicDriver.TextGraphProperty.Localization.Propagation.String", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FSMTextGraphPropertyLocalizationStringTest::RunTest(const FString& Parameters)
{
	auto TestBody = [&]() -> bool
	{
		SETUP_NEW_STATE_MACHINE_FOR_TEST(1)
		CREATE_TEXTNODE_ASSET()

		UEdGraphPin* LastStatePin = nullptr;
		TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, NodeBP->GeneratedClass, USMTransitionTestInstance::StaticClass());

		const FText NewCDOText = FText::FromString("New defaults");
		// Propagation should work
		{
			const bool bShouldMatch = true;
			TestLocalizationPropagation(this, NewBP, NodeBP, NewCDOText, bShouldMatch);
		}

		// Propagation should fail
		{
			FText InstanceText = FText::FromString("New instance defaults");

			const bool bShouldMatch = false;
			TestLocalizationPropagation(this, NewBP, NodeBP, NewCDOText, bShouldMatch, &InstanceText);
		}
	
		return NewAsset.DeleteAsset(this);
	};
	
	USMProjectEditorSettings* Settings = FSMBlueprintEditorUtils::GetMutableProjectEditorSettings();
	const auto CurrentCSSetting = Settings->EditorNodeConstructionScriptSetting;
	
	Settings->EditorNodeConstructionScriptSetting = ESMEditorConstructionScriptProjectSetting::SM_Standard;
	TestBody();
	
	Settings->EditorNodeConstructionScriptSetting = ESMEditorConstructionScriptProjectSetting::SM_Legacy;
	TestBody();
	
	Settings->EditorNodeConstructionScriptSetting = CurrentCSSetting;

	return true;
}

/**
 * Test TextGraph Localization propagation when only the namespace changes.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSMTextGraphPropertyLocalizationNamespaceTest, "LogicDriver.TextGraphProperty.Localization.Propagation.Namespace", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FSMTextGraphPropertyLocalizationNamespaceTest::RunTest(const FString& Parameters)
{
	auto TestBody = [&]() -> bool
	{
		SETUP_NEW_STATE_MACHINE_FOR_TEST(1)
		CREATE_TEXTNODE_ASSET()

		UEdGraphPin* LastStatePin = nullptr;
		TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, NodeBP->GeneratedClass, USMTransitionTestInstance::StaticClass());

		TOptional<FString> Key = FTextInspector::GetKey(USMTextGraphState::DefaultText);
		check(Key.IsSet());
		const FText NewCDOText = FText::ChangeKey(TEXT("NewNamespace"), *Key, USMTextGraphState::DefaultText);
	
		// Propagation should work
		{
			const bool bShouldMatch = true;
			TestLocalizationPropagation(this, NewBP, NodeBP, NewCDOText, bShouldMatch);
		}

		// Propagation should fail
		{
			FText InstanceText = GetCurrentInstanceText(NewBP);
			InstanceText = FText::ChangeKey(TEXT("NewInstanceNamespace"), *Key, InstanceText);

			const bool bShouldMatch = false;
			TestLocalizationPropagation(this, NewBP, NodeBP, NewCDOText, bShouldMatch, &InstanceText);
		}
	
		return NewAsset.DeleteAsset(this);
	};

	USMProjectEditorSettings* Settings = FSMBlueprintEditorUtils::GetMutableProjectEditorSettings();
	const auto CurrentCSSetting = Settings->EditorNodeConstructionScriptSetting;
	
	Settings->EditorNodeConstructionScriptSetting = ESMEditorConstructionScriptProjectSetting::SM_Standard;
	TestBody();
	
	Settings->EditorNodeConstructionScriptSetting = ESMEditorConstructionScriptProjectSetting::SM_Legacy;
	TestBody();
	
	Settings->EditorNodeConstructionScriptSetting = CurrentCSSetting;

	return true;
}

/**
 * Test TextGraph Localization propagation when only the key changes.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSMTextGraphPropertyLocalizationKeyTest, "LogicDriver.TextGraphProperty.Localization.Propagation.Key", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FSMTextGraphPropertyLocalizationKeyTest::RunTest(const FString& Parameters)
{
	auto TestBody = [&]() -> bool
	{
		SETUP_NEW_STATE_MACHINE_FOR_TEST(1)
		CREATE_TEXTNODE_ASSET()

		UEdGraphPin* LastStatePin = nullptr;
		TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, NodeBP->GeneratedClass, USMTransitionTestInstance::StaticClass());

		TOptional<FString> Namespace = FTextInspector::GetNamespace(USMTextGraphState::DefaultText);
		check(Namespace.IsSet());
	
		const FText NewCDOText = FText::ChangeKey(*Namespace, TEXT("NewKey"), USMTextGraphState::DefaultText);

		// Propagation should work
		{
			const bool bShouldMatch = true;
			TestLocalizationPropagation(this, NewBP, NodeBP, NewCDOText, bShouldMatch);
		}

		// Propagation should fail
		{
			FText InstanceText = GetCurrentInstanceText(NewBP);
			InstanceText = FText::ChangeKey(*Namespace, TEXT("NewInstanceKey"), InstanceText);

			const bool bShouldMatch = false;
			TestLocalizationPropagation(this, NewBP, NodeBP, NewCDOText, bShouldMatch, &InstanceText);
		}

		StateAsset.DeleteAsset(this);
		return NewAsset.DeleteAsset(this);
	};

	USMProjectEditorSettings* Settings = FSMBlueprintEditorUtils::GetMutableProjectEditorSettings();
	const auto CurrentCSSetting = Settings->EditorNodeConstructionScriptSetting;
	
	Settings->EditorNodeConstructionScriptSetting = ESMEditorConstructionScriptProjectSetting::SM_Standard;
	TestBody();
	
	Settings->EditorNodeConstructionScriptSetting = ESMEditorConstructionScriptProjectSetting::SM_Legacy;
	TestBody();
	
	Settings->EditorNodeConstructionScriptSetting = CurrentCSSetting;

	return true;
}

/**
 * Test TextGraph Localization propagation when culture is disabled and enabled.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSMTextGraphPropertyLocalizationInvariantTest, "LogicDriver.TextGraphProperty.Localization.Propagation.Invariant", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FSMTextGraphPropertyLocalizationInvariantTest::RunTest(const FString& Parameters)
{
	auto TestBody = [&]() -> bool
	{
		SETUP_NEW_STATE_MACHINE_FOR_TEST(1)
		CREATE_TEXTNODE_ASSET()

		UEdGraphPin* LastStatePin = nullptr;
		TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, NodeBP->GeneratedClass, USMTransitionTestInstance::StaticClass());

		// Make invariant
		{
			const FText NewCDOText = FText::AsCultureInvariant(USMTextGraphState::DefaultText);
			const bool bShouldMatch = true;
			TestLocalizationPropagation(this, NewBP, NodeBP, NewCDOText, bShouldMatch);
		}

		// Add culture
		{
			const FText NewCDOText = FText::ChangeKey(TEXT("NS"), TEXT("KEY"), USMTextGraphState::DefaultText);
			const bool bShouldMatch = true;
			TestLocalizationPropagation(this, NewBP, NodeBP, NewCDOText, bShouldMatch);
		}

		// Make invariant
		{
			const FText NewCDOText = FText::AsCultureInvariant(USMTextGraphState::DefaultText);
			const bool bShouldMatch = true;
			TestLocalizationPropagation(this, NewBP, NodeBP, NewCDOText, bShouldMatch);
		}

		// Change string
		{
			const FText NewCDOText = FText::FromString("Test");
			const bool bShouldMatch = true;
			TestLocalizationPropagation(this, NewBP, NodeBP, NewCDOText, bShouldMatch);
		}

		// Propagation should fail
		{
			FText InstanceText = GetCurrentInstanceText(NewBP);
			InstanceText = FText::ChangeKey(TEXT("NS"), TEXT("KEY_INSTANCE"), InstanceText);

			const FText NewCDOText = FText::FromString("Test2");
			const bool bShouldMatch = false;
			TestLocalizationPropagation(this, NewBP, NodeBP, NewCDOText, bShouldMatch, &InstanceText);
		}
		
		StateAsset.DeleteAsset(this);
		return NewAsset.DeleteAsset(this);
	};

	USMProjectEditorSettings* Settings = FSMBlueprintEditorUtils::GetMutableProjectEditorSettings();
	const auto CurrentCSSetting = Settings->EditorNodeConstructionScriptSetting;

	Settings->EditorNodeConstructionScriptSetting = ESMEditorConstructionScriptProjectSetting::SM_Standard;
	TestBody();

	Settings->EditorNodeConstructionScriptSetting = ESMEditorConstructionScriptProjectSetting::SM_Legacy;
	TestBody();

	Settings->EditorNodeConstructionScriptSetting = CurrentCSSetting;

	return true;
}

/**
 * Test TextGraph Localization key is stable between changes and that it can change when duplicated.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSMTextGraphPropertyLocalizationStableKeyTest, "LogicDriver.TextGraphProperty.Localization.StableKey", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FSMTextGraphPropertyLocalizationStableKeyTest::RunTest(const FString& Parameters)
{
	auto TestBody = [&]() -> bool
	{
		SETUP_NEW_STATE_MACHINE_FOR_TEST(1)
		CREATE_TEXTNODE_ASSET()

		UEdGraphPin* LastStatePin = nullptr;
		TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, NodeBP->GeneratedClass, USMTransitionTestInstance::StaticClass());

		FString ExpectedNamespace;
		FString ExpectedKey;

		auto ValidateTextMatches = [&](const FText& CurrentText, const FText& NewText, bool bExpectDifferentKey = false)
		{
			TOptional<FString> CurrentNamespace = FTextInspector::GetNamespace(CurrentText);
			TOptional<FString> CurrentKey = FTextInspector::GetKey(CurrentText);

			TestEqual("Text matches", CurrentText.ToString(), NewText.ToString());
			TestEqual("Namespace set", *CurrentNamespace, ExpectedNamespace);
			if (bExpectDifferentKey)
			{
				TestNotEqual("Key set", *CurrentKey, ExpectedKey);
			}
			else
			{
				TestEqual("Key set", *CurrentKey, ExpectedKey);
			}
		};

		USMGraphK2Node_TextPropertyNode* FirstTextPropertyNode = GetCurrentInstanceNode(NewBP);
		USMTextPropertyGraph* TextGraph = GetCurrentInstanceGraph(NewBP);

#define INITIAL_NS "initial_ns"
#define INITIAL_KEY "initial_key"

		{
			const FText NewInstanceText = NSLOCTEXT(INITIAL_NS, INITIAL_KEY, "instance text");

			TextGraph->SetNewText(NewInstanceText);
			const FText FormatNodeText = TextGraph->GetFormatTextNodeText();

			// Store the initial values, they will have changed but they should persist from this point on.
			const TOptional<FString> CurrentNamespace = FTextInspector::GetNamespace(FormatNodeText);
			const TOptional<FString> CurrentKey = FTextInspector::GetKey(FormatNodeText);
			check(CurrentKey.IsSet());

			ExpectedNamespace = CurrentNamespace.Get(FString());
			ExpectedKey = *CurrentKey;

			TestNotEqual("Namespace updated from default", ExpectedNamespace, FString(INITIAL_NS));
			TestNotEqual("Key updated from default", ExpectedKey, FString(INITIAL_KEY));

			ValidateTextMatches(GetCurrentInstanceText(NewBP), NewInstanceText);
		}

		{
			const FText NewInstanceText = NSLOCTEXT("ns", "key1", "instance text 2");
			TextGraph->SetNewText(NewInstanceText);

			ValidateTextMatches(GetCurrentInstanceText(NewBP), NewInstanceText);
		}

		FKismetEditorUtilities::CompileBlueprint(NewBP);
		TextGraph = GetCurrentInstanceGraph(NewBP);
		FText SecondInstanceText;
		{
			const FText NewInstanceText = NSLOCTEXT("ns", "key2", "instance text 3abc");
			SecondInstanceText = NewInstanceText;
			TextGraph->SetNewText(NewInstanceText);
			FKismetEditorUtilities::CompileBlueprint(NewBP); // Compile needed because of previous compile and to update the format text node.
			ValidateTextMatches(GetCurrentInstanceText(NewBP), NewInstanceText);
		}

		/////////////////////////
		// Test key duplication
		/////////////////////////

		TSet<UEdGraphNode*> DuplicatedNodes = TestHelpers::DuplicateNodes(TArray<UEdGraphNode*> {FirstTextPropertyNode->GetOwningGraphNodeChecked()});
		check(DuplicatedNodes.Num() == 1)
		FKismetEditorUtilities::CompileBlueprint(NewBP); // Makes sure references are updated

		{
			const FText NewInstanceText = NSLOCTEXT("ns", "key3", "instance text after duplication");
			TextGraph->SetNewText(NewInstanceText);
			FKismetEditorUtilities::CompileBlueprint(NewBP); // Compile needed because of previous compile and to update the format text node.

			// Key should have changed
			ValidateTextMatches(GetCurrentInstanceText(NewBP), NewInstanceText, true);

			for (UEdGraphNode* DuplicatedNode : DuplicatedNodes)
			{
				// Verify duplicated node hasn't changed
				USMGraphNode_StateNode* DuplicatedState = CastChecked<USMGraphNode_StateNode>(DuplicatedNode);

				USMGraphK2Node_TextPropertyNode* DuplicatedTextNode = CastChecked<USMGraphK2Node_TextPropertyNode>(DuplicatedState->GetAllPropertyGraphNodesAsArray()[0]);
				FText FirstNodeDuplicatedText = CastChecked<USMTextPropertyGraph>(DuplicatedTextNode->GetPropertyGraph())->GetFormatTextNodeText();

				ValidateTextMatches(FirstNodeDuplicatedText, SecondInstanceText);

				// Verify changing the second node update persists the key
				const FText SecondNodeDuplicatedText = NSLOCTEXT("ns", "key4", "duplicated text change");
				USMTextPropertyGraph* DuplicatedTextGraph = CastChecked<USMTextPropertyGraph>(
					DuplicatedTextNode->GetPropertyGraph());
				DuplicatedTextGraph->SetNewText(SecondNodeDuplicatedText);
				FKismetEditorUtilities::CompileBlueprint(NewBP);
				ValidateTextMatches(DuplicatedTextGraph->GetFormatTextNodeText(), SecondNodeDuplicatedText);
			}
		}

		StateAsset.DeleteAsset(this);
		return NewAsset.DeleteAsset(this);
	};

	USMProjectEditorSettings* Settings = FSMBlueprintEditorUtils::GetMutableProjectEditorSettings();
	const auto CurrentCSSetting = Settings->EditorNodeConstructionScriptSetting;

	Settings->EditorNodeConstructionScriptSetting = ESMEditorConstructionScriptProjectSetting::SM_Standard;
	TestBody();

	Settings->EditorNodeConstructionScriptSetting = ESMEditorConstructionScriptProjectSetting::SM_Legacy;
	TestBody();

	Settings->EditorNodeConstructionScriptSetting = CurrentCSSetting;

	return true;
}

#endif

#endif //WITH_DEV_AUTOMATION_TESTS