// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMTestContext.h"
#include "SMTestHelpers.h"
#include "Helpers/SMTestBoilerplate.h"

#include "Blueprints/SMBlueprintFactory.h"
#include "Graph/SMGraph.h"
#include "Graph/Nodes/SMGraphK2Node_StateMachineNode.h"
#include "Graph/Nodes/SMGraphNode_AnyStateNode.h"
#include "Graph/Nodes/SMGraphNode_ConduitNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineEntryNode.h"
#include "Graph/Nodes/SMGraphNode_StateNode.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/Nodes/FunctionNodes/SMGraphK2Node_FunctionNodes_TransitionInstance.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "Blueprints/SMBlueprint.h"

#include "K2Node_CommutativeAssociativeBinaryOperator.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet2/KismetEditorUtilities.h"

#if WITH_DEV_AUTOMATION_TESTS

#if PLATFORM_DESKTOP

#define SETUP_TRANSITION_STACK_TEST_WITH_CUSTOM_LOGIC(transition_class, use_custom_transition_logic) \
FAssetHandler NewAsset;\
if (!TestHelpers::TryCreateNewStateMachineAsset(this, NewAsset, false))\
{\
return false;\
}\
USMBlueprint* NewBP = NewAsset.GetObjectAs<USMBlueprint>();\
USMGraphK2Node_StateMachineNode* RootStateMachineNode = FSMBlueprintEditorUtils::GetRootStateMachineNode(NewBP);\
USMGraph* StateMachineGraph = RootStateMachineNode->GetStateMachineGraph();\
const int32 TotalStates = 2;\
UEdGraphPin* LastStatePin = nullptr;\
TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMStateTestInstance::StaticClass(), transition_class, use_custom_transition_logic);\
USMGraphNode_StateNode* StateNode = CastChecked<USMGraphNode_StateNode>(StateMachineGraph->GetEntryNode()->GetOutputNode());\
USMGraphNode_TransitionEdge* TransitionEdge = StateNode->GetNextTransition();\
check(TransitionEdge);\
UEdGraphPin* TransitionEvalPin = TransitionEdge->GetTransitionGraph()->ResultNode->GetTransitionEvaluationPin();\
UEdGraphPin* UserData = TransitionEvalPin->LinkedTo.Num() > 0 ? TransitionEvalPin->LinkedTo[0] : nullptr;\
TestEqual("Empty transition stack", TransitionEdge->TransitionStack.Num(), 0);\

#define SETUP_TRANSITION_STACK_TEST(transition_class) \
SETUP_TRANSITION_STACK_TEST_WITH_CUSTOM_LOGIC(transition_class, false) \

#define FINISH_TRANSITION_STACK_TEST() \
auto SavedExpressions = TestExpressions; \
if (TestExpressions.Num() > 0) \
{\
	TransitionEdge->InitTransitionStack();\
	TestTrue("Transition stack initialized", TransitionEdge->HasValidTransitionStack());\
}\
TransitionEdge->FormatGraphForStackNodes();\
ValidateExpressionNodes(this, TransitionEvalPin, TestExpressions, UserData, TransitionEdge);\
check(TestExpressions.Num() == 0);\
/* Test graph regeneration. */ \
if (SavedExpressions.Num() > 0) \
{\
	auto BackupStack = TransitionEdge->TransitionStack; \
	TransitionEdge->TransitionStack.Empty(); \
	TransitionEdge->TransitionStack = BackupStack; \
	TransitionEdge->InitTransitionStack(); \
	TransitionEdge->FormatGraphForStackNodes(); \
	TestExpressions = SavedExpressions; \
	ValidateExpressionNodes(this, TransitionEvalPin, TestExpressions, UserData, TransitionEdge);\
}\
return NewAsset.DeleteAsset(this);\

static bool ValidateExpressionNodes(FAutomationTestBase* Test, const UEdGraphPin* InPin, TArray<ESMExpressionMode>& InExpectedExpressions, UEdGraphPin* InExpectedUserPin, USMGraphNode_TransitionEdge* TransitionEdge)
{
	if (TransitionEdge->GetTransitionGraph()->ResultNode == InPin->GetOwningNode())
	{
		return ValidateExpressionNodes(Test, InPin->LinkedTo[0], InExpectedExpressions, InExpectedUserPin, TransitionEdge);
	}
	
	if (UK2Node_CommutativeAssociativeBinaryOperator* BinaryNode = Cast<UK2Node_CommutativeAssociativeBinaryOperator>(InPin->GetOwningNode()))
	{
		if (BinaryNode->GetFunctionName() == GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Not_PreBool))
		{
			if (TransitionEdge->bNOTPrimaryCondition)
			{
				check(InExpectedExpressions.Num() == 0);
				return Test->TestTrue("Primary NOT node generated.",
					BinaryNode->GetFunctionName() == GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Not_PreBool));
			}
		}

		const int32 LastInputIndex = BinaryNode->NumAdditionalInputs + 1;

		// Output pin of binary node.
		if (BinaryNode->FindOutPin() == InPin)
		{
			const UEdGraphPin* LastInputPin = BinaryNode->GetInputPin(LastInputIndex);
			check(LastInputPin);
			return ValidateExpressionNodes(Test, LastInputPin, InExpectedExpressions, InExpectedUserPin, TransitionEdge);
		}

		// Find index of input pin.
		UEdGraphPin* CurrentInputPin = nullptr;
		int32 CurrentInputPinIndex = 0;
		for (int32 Idx = 0; Idx < LastInputIndex + 1; ++Idx)
		{
			CurrentInputPin = BinaryNode->GetInputPin(Idx);
			if (CurrentInputPin == InPin)
			{
				CurrentInputPinIndex = Idx;
				break;
			}
		}
		check(CurrentInputPin);

		const bool bFinishedExpressions = InExpectedExpressions.Num() == 0;
		const bool bHasExpressions = CurrentInputPinIndex > 0 && InExpectedExpressions.Num() > 0;

		const FName FunctionName = BinaryNode->GetFunctionName();
		if (bHasExpressions)
		{
			const ESMExpressionMode ExpressionToText = InExpectedExpressions.Pop();
			
			if (ExpressionToText == ESMExpressionMode::OR)
			{
				if (!Test->TestEqual("Operator function generated.", FunctionName, GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, BooleanOR)))
				{
					return false;
				}
			}
			else if (ExpressionToText == ESMExpressionMode::AND)
			{
				if (!Test->TestEqual("Operator function generated.", FunctionName, GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, BooleanAND)))
				{
					return false;
				}
			}
			else
			{
				return false;
			}
		}
		
		const bool bNodeInstanceExpected = bHasExpressions || (bFinishedExpressions && !TransitionEdge->IsUsingDefaultNodeClass());
		if (bNodeInstanceExpected)
		{
			bool bNOTExpected = false;
			
			// Validate CanEnterTransition instance nodes placed.

			const int32 CurrentStackIndex = bHasExpressions ? InExpectedExpressions.Num() : INDEX_NONE;

			FGuid GuidToTest;
			if (CurrentStackIndex == INDEX_NONE)
			{
				GuidToTest = FGuid();
			}
			else
			{
				check(CurrentStackIndex < TransitionEdge->TransitionStack.Num());
				const FTransitionStackContainer& NodeStack = TransitionEdge->TransitionStack[CurrentStackIndex];
				GuidToTest = NodeStack.TemplateGuid;
				bNOTExpected = NodeStack.bNOT;
			}

			UEdGraphNode* NodeToTest = CurrentInputPin->LinkedTo[0]->GetOwningNode();
			if (bNOTExpected)
			{
				UK2Node_CommutativeAssociativeBinaryOperator* NotOperator = Cast<UK2Node_CommutativeAssociativeBinaryOperator>(NodeToTest);
				if (!Test->TestNotNull("Not placed", NodeToTest))
				{
					return false;
				}
				NodeToTest = NotOperator->GetInputPin(0)->LinkedTo[0]->GetOwningNode();
			}

			USMGraphK2Node_TransitionInstance_CanEnterTransition* InstanceCanEnterTransitionNode =
				Cast<USMGraphK2Node_TransitionInstance_CanEnterTransition>(NodeToTest);

			if (bFinishedExpressions && InstanceCanEnterTransitionNode == nullptr)
			{
				// May be a primary NOT node.
				return ValidateExpressionNodes(Test, CurrentInputPin->LinkedTo[0], InExpectedExpressions, InExpectedUserPin, TransitionEdge);
			}
			
			Test->TestNotNull("Node instance Can Enter Transition Connected", InstanceCanEnterTransitionNode);

			if (const USMGraphK2Node_TransitionStackInstance_CanEnterTransition* StackInstanceNode =
				Cast<USMGraphK2Node_TransitionStackInstance_CanEnterTransition>(InstanceCanEnterTransitionNode))
			{
				Test->TestEqual("Correct node class placed", StackInstanceNode->GetNodeStackGuid(), GuidToTest);
			}
			else
			{
				// Default node instance.
				Test->TestEqual("Correct node class placed", FGuid(), GuidToTest);
			}
		}
		else if (bFinishedExpressions)
		{
			if (CurrentInputPin->LinkedTo.Num() == 0)
			{
				// Validate default value set.
				FString ExpectedDefaultValue;
				if (FunctionName == GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, BooleanAND))
				{
					return Test->TestEqual("AND defaults to true", CurrentInputPin->GetDefaultAsString(), TEXT("true"));
				}
				else
				{
					return Test->TestEqual("AND defaults to true", CurrentInputPin->GetDefaultAsString(), TEXT("false"));
				}
			}
			else
			{
				UEdGraphPin* PinToTest = CurrentInputPin->LinkedTo[0];
				return Test->TestEqual("Original user data found", PinToTest, InExpectedUserPin);
			}
		}

		// Move up the input pins
		if (CurrentInputPinIndex > 0)
		{
			return ValidateExpressionNodes(Test, BinaryNode->GetInputPin(CurrentInputPinIndex - 1), InExpectedExpressions, InExpectedUserPin, TransitionEdge);
		}

		// Repeat
		if (CurrentInputPin->LinkedTo.Num() > 0)
		{
			return ValidateExpressionNodes(Test, CurrentInputPin->LinkedTo[0], InExpectedExpressions, InExpectedUserPin, TransitionEdge);
		}

		// Finish
		return ValidateExpressionNodes(Test, CurrentInputPin, InExpectedExpressions, InExpectedUserPin, TransitionEdge);
	}
	UEdGraphPin* PinToTest = const_cast<UEdGraphPin*>(InPin);
	
	UClass* ExpectedNodeClass = InExpectedUserPin->GetOwningNode()->GetClass();
	if (ExpectedNodeClass == USMGraphK2Node_TransitionInstance_CanEnterTransition::StaticClass())
	{
		// For default GetNodeInstance just test the class since the nodes are recreated and originals not always destroyed.
		return Test->TestEqual("Original user data found", PinToTest->GetOwningNode()->GetClass(), ExpectedNodeClass);
	}
	
	return Test->TestEqual("Original user data found", PinToTest, InExpectedUserPin);
};

/** Single AND node with no custom user logic. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransitionStackANDDefaultTest, "LogicDriver.TransitionStack.AND_Default", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTransitionStackANDDefaultTest::RunTest(const FString& Parameters)
{
	SETUP_TRANSITION_STACK_TEST(USMTransitionInstance::StaticClass())

	// No user logic.
	TransitionEvalPin->BreakAllPinLinks();
	
	FTransitionStackContainer NewStack(USMTransitionTestInstance::StaticClass());
	NewStack.Mode = ESMExpressionMode::AND;
	TransitionEdge->TransitionStack.Add(NewStack);

	TArray<ESMExpressionMode> TestExpressions { ESMExpressionMode::AND };

	FINISH_TRANSITION_STACK_TEST()
}

/** Single OR node with no custom user logic. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransitionStackORDefaultTest, "LogicDriver.TransitionStack.OR_Default", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTransitionStackORDefaultTest::RunTest(const FString& Parameters)
{
	SETUP_TRANSITION_STACK_TEST(USMTransitionInstance::StaticClass())

	// No user logic.
	TransitionEvalPin->BreakAllPinLinks();
	
	FTransitionStackContainer NewStack(USMTransitionTestInstance::StaticClass());
	NewStack.Mode = ESMExpressionMode::OR;
	TransitionEdge->TransitionStack.Add(NewStack);

	TArray<ESMExpressionMode> TestExpressions { ESMExpressionMode::OR };
	
	FINISH_TRANSITION_STACK_TEST()
}

/** Single AND node with custom user logic specified. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransitionStackANDCustomLogicTest, "LogicDriver.TransitionStack.AND_CustomLogic", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTransitionStackANDCustomLogicTest::RunTest(const FString& Parameters)
{
	SETUP_TRANSITION_STACK_TEST(USMTransitionInstance::StaticClass())

	FTransitionStackContainer NewStack(USMTransitionTestInstance::StaticClass());
	NewStack.Mode = ESMExpressionMode::AND;
	TransitionEdge->TransitionStack.Add(NewStack);

	TArray<ESMExpressionMode> TestExpressions { ESMExpressionMode::AND };

	FINISH_TRANSITION_STACK_TEST()
}

/** Single OR node with custom user logic specified. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransitionStackORCustomLogicTest, "LogicDriver.TransitionStack.OR_CustomLogic", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTransitionStackORCustomLogicTest::RunTest(const FString& Parameters)
{
	SETUP_TRANSITION_STACK_TEST(USMTransitionInstance::StaticClass())

	FTransitionStackContainer NewStack(USMTransitionTestInstance::StaticClass());
	NewStack.Mode = ESMExpressionMode::OR;
	TransitionEdge->TransitionStack.Add(NewStack);

	TArray<ESMExpressionMode> TestExpressions { ESMExpressionMode::OR };

	FINISH_TRANSITION_STACK_TEST()
}

/** Single OR with a custom node instance set. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransitionStackORWithNodeInstanceTest, "LogicDriver.TransitionStack.OR_WithNodeInstance", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTransitionStackORWithNodeInstanceTest::RunTest(const FString& Parameters)
{
	SETUP_TRANSITION_STACK_TEST(USMTransitionTestInstance::StaticClass())

	FTransitionStackContainer NewStack(USMTransitionTestInstance::StaticClass());
	NewStack.Mode = ESMExpressionMode::OR;
	TransitionEdge->TransitionStack.Add(NewStack);

	TArray<ESMExpressionMode> TestExpressions { ESMExpressionMode::OR };

	FINISH_TRANSITION_STACK_TEST()
}

/** Single AND with a custom node instance set. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransitionStackANDWithNodeInstanceTest, "LogicDriver.TransitionStack.AND_WithNodeInstance", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTransitionStackANDWithNodeInstanceTest::RunTest(const FString& Parameters)
{
	SETUP_TRANSITION_STACK_TEST(USMTransitionTestInstance::StaticClass())

	FTransitionStackContainer NewStack(USMTransitionTestInstance::StaticClass());
	TransitionEdge->TransitionStack.Add(NewStack);

	TArray<ESMExpressionMode> TestExpressions { ESMExpressionMode::AND };

	FINISH_TRANSITION_STACK_TEST()
}

/** Node with a node instance, no stack, and custom user logic specified. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransitionStackNodeInstanceAndCustomLogicTest, "LogicDriver.TransitionStack.NodeInstanceWithCustomLogic", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTransitionStackNodeInstanceAndCustomLogicTest::RunTest(const FString& Parameters)
{
	SETUP_TRANSITION_STACK_TEST_WITH_CUSTOM_LOGIC(USMTransitionTestInstance::StaticClass(), true)

	TArray<ESMExpressionMode> TestExpressions { };

	FINISH_TRANSITION_STACK_TEST()
}

/** Single AND node with a node instance and custom user logic specified. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransitionStackANDWithNodeInstanceAndCustomLogicTest, "LogicDriver.TransitionStack.AND_NodeInstanceWithCustomLogic", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTransitionStackANDWithNodeInstanceAndCustomLogicTest::RunTest(const FString& Parameters)
{
	SETUP_TRANSITION_STACK_TEST_WITH_CUSTOM_LOGIC(USMTransitionTestInstance::StaticClass(), true)

	FTransitionStackContainer NewStack(USMTransitionTestInstance::StaticClass());
	NewStack.Mode = ESMExpressionMode::AND;
	TransitionEdge->TransitionStack.Add(NewStack);

	TArray<ESMExpressionMode> TestExpressions { ESMExpressionMode::AND };

	FINISH_TRANSITION_STACK_TEST()
}

/** Single OR node with a NOT. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransitionStackORNOTCustomLogicTest, "LogicDriver.TransitionStack.OR_NOT_CustomLogic", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTransitionStackORNOTCustomLogicTest::RunTest(const FString& Parameters)
{
	SETUP_TRANSITION_STACK_TEST(USMTransitionInstance::StaticClass())

	FTransitionStackContainer NewStack(USMTransitionTestInstance::StaticClass());
	NewStack.Mode = ESMExpressionMode::OR;
	NewStack.bNOT = true;
	TransitionEdge->TransitionStack.Add(NewStack);

	TArray<ESMExpressionMode> TestExpressions { ESMExpressionMode::OR };

	FINISH_TRANSITION_STACK_TEST()
}

/** Single AND node with a NOT. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransitionStackANDNOTCustomLogicTest, "LogicDriver.TransitionStack.AND_NOT_CustomLogic", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTransitionStackANDNOTCustomLogicTest::RunTest(const FString& Parameters)
{
	SETUP_TRANSITION_STACK_TEST(USMTransitionInstance::StaticClass())

	FTransitionStackContainer NewStack(USMTransitionTestInstance::StaticClass());
	NewStack.Mode = ESMExpressionMode::AND;
	NewStack.bNOT = true;
	TransitionEdge->TransitionStack.Add(NewStack);

	TArray<ESMExpressionMode> TestExpressions { ESMExpressionMode::AND };

	FINISH_TRANSITION_STACK_TEST()
}

/** Single NOT, no stack. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransitionStackPrimaryNOTTest, "LogicDriver.TransitionStack.PrimaryNOT", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTransitionStackPrimaryNOTTest::RunTest(const FString& Parameters)
{
	SETUP_TRANSITION_STACK_TEST(USMTransitionTestInstance::StaticClass())
	
	TransitionEdge->bNOTPrimaryCondition = true;

	TArray<ESMExpressionMode> TestExpressions { };

	FINISH_TRANSITION_STACK_TEST()
}

/** Complex expression. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransitionStackComplexExpressionTest, "LogicDriver.TransitionStack.ComplexExpression", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTransitionStackComplexExpressionTest::RunTest(const FString& Parameters)
{
	SETUP_TRANSITION_STACK_TEST(USMTransitionTestInstance::StaticClass())
	{
		FTransitionStackContainer NewStack(USMTransitionTestInstance::StaticClass());
		NewStack.Mode = ESMExpressionMode::AND;
		NewStack.bNOT = false;
		TransitionEdge->TransitionStack.Add(NewStack);
	}
	{
		FTransitionStackContainer NewStack(USMTransitionTestInstance::StaticClass());
		NewStack.Mode = ESMExpressionMode::AND;
		NewStack.bNOT = false;
		TransitionEdge->TransitionStack.Add(NewStack);
	}
	{
		FTransitionStackContainer NewStack(USMTransitionTestInstance::StaticClass());
		NewStack.Mode = ESMExpressionMode::AND;
		NewStack.bNOT = true;
		TransitionEdge->TransitionStack.Add(NewStack);
	}
	{
		FTransitionStackContainer NewStack(USMTransitionTestInstance::StaticClass());
		NewStack.Mode = ESMExpressionMode::OR;
		NewStack.bNOT = true;
		TransitionEdge->TransitionStack.Add(NewStack);
	}	
	{
		FTransitionStackContainer NewStack(USMTransitionTestInstance::StaticClass());
		NewStack.Mode = ESMExpressionMode::OR;
		NewStack.bNOT = false;
		TransitionEdge->TransitionStack.Add(NewStack);
	}	
	{
		FTransitionStackContainer NewStack(USMTransitionTestInstance::StaticClass());
		NewStack.Mode = ESMExpressionMode::AND;
		NewStack.bNOT = false;
		TransitionEdge->TransitionStack.Add(NewStack);
	}
	{
		FTransitionStackContainer NewStack(USMTransitionTestInstance::StaticClass());
		NewStack.Mode = ESMExpressionMode::OR;
		NewStack.bNOT = false;
		TransitionEdge->TransitionStack.Add(NewStack);
	}		
	TArray<ESMExpressionMode> TestExpressions { ESMExpressionMode::AND, ESMExpressionMode::AND, ESMExpressionMode::AND,
		ESMExpressionMode::OR, ESMExpressionMode::OR, ESMExpressionMode::AND, ESMExpressionMode::OR };

	FINISH_TRANSITION_STACK_TEST()
}

/** Complex expression with a primary NOT. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransitionStackComplexExpressionPrimaryNOTTest, "LogicDriver.TransitionStack.ComplexExpressionWithPrimaryNOT", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTransitionStackComplexExpressionPrimaryNOTTest::RunTest(const FString& Parameters)
{
	SETUP_TRANSITION_STACK_TEST(USMTransitionTestInstance::StaticClass())

	TransitionEdge->bNOTPrimaryCondition = true;
	
	{
		FTransitionStackContainer NewStack(USMTransitionTestInstance::StaticClass());
		NewStack.Mode = ESMExpressionMode::OR;
		NewStack.bNOT = true;
		TransitionEdge->TransitionStack.Add(NewStack);
	}
	{
		FTransitionStackContainer NewStack(USMTransitionTestInstance::StaticClass());
		NewStack.Mode = ESMExpressionMode::AND;
		NewStack.bNOT = false;
		TransitionEdge->TransitionStack.Add(NewStack);
	}
	{
		FTransitionStackContainer NewStack(USMTransitionTestInstance::StaticClass());
		NewStack.Mode = ESMExpressionMode::OR;
		NewStack.bNOT = true;
		TransitionEdge->TransitionStack.Add(NewStack);
	}
	{
		FTransitionStackContainer NewStack(USMTransitionTestInstance::StaticClass());
		NewStack.Mode = ESMExpressionMode::AND;
		NewStack.bNOT = true;
		TransitionEdge->TransitionStack.Add(NewStack);
	}	
	{
		FTransitionStackContainer NewStack(USMTransitionTestInstance::StaticClass());
		NewStack.Mode = ESMExpressionMode::AND;
		NewStack.bNOT = false;
		TransitionEdge->TransitionStack.Add(NewStack);
	}	
	{
		FTransitionStackContainer NewStack(USMTransitionTestInstance::StaticClass());
		NewStack.Mode = ESMExpressionMode::AND;
		NewStack.bNOT = false;
		TransitionEdge->TransitionStack.Add(NewStack);
	}
	{
		FTransitionStackContainer NewStack(USMTransitionTestInstance::StaticClass());
		NewStack.Mode = ESMExpressionMode::AND;
		NewStack.bNOT = false;
		TransitionEdge->TransitionStack.Add(NewStack);
	}		
	TArray<ESMExpressionMode> TestExpressions { ESMExpressionMode::OR, ESMExpressionMode::AND, ESMExpressionMode::OR,
		ESMExpressionMode::AND, ESMExpressionMode::AND, ESMExpressionMode::AND, ESMExpressionMode::AND };

	FINISH_TRANSITION_STACK_TEST()
}

/** Test transition instance methods for transition stack retrieval. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransitionStackInstanceMethodsTest, "LogicDriver.TransitionStack.InstanceMethods", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTransitionStackInstanceMethodsTest::RunTest(const FString& Parameters)
{
	SETUP_TRANSITION_STACK_TEST(USMTransitionTestInstance::StaticClass())

	{
		FTransitionStackContainer NewStack(USMTransitionTestInstance::StaticClass());
		NewStack.Mode = ESMExpressionMode::OR;
		TransitionEdge->TransitionStack.Add(NewStack);
	}
	{
		FTransitionStackContainer NewStack(USMTransitionTestInstance::StaticClass());
		NewStack.Mode = ESMExpressionMode::OR;
		TransitionEdge->TransitionStack.Add(NewStack);
	}
	{
		FTransitionStackContainer NewStack(USMTransitionStackTestInstance::StaticClass());
		NewStack.Mode = ESMExpressionMode::OR;
		TransitionEdge->TransitionStack.Add(NewStack);
	}
	
	TArray<ESMExpressionMode> TestExpressions { ESMExpressionMode::OR, ESMExpressionMode::OR, ESMExpressionMode::OR };

	TransitionEdge->InitTransitionStack();

	FKismetEditorUtilities::CompileBlueprint(NewBP);
	USMInstance* Instance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, NewObject<USMTestContext>());
	USMTransitionInstance* NodeInstance = CastChecked<USMTransitionInstance>(Instance->GetRootStateMachine().GetSingleInitialState()->GetOutgoingTransitions()[0]->GetOrCreateNodeInstance());

	{
		TArray<USMTransitionInstance*> AllTransitionInstances;
		NodeInstance->GetAllTransitionStackInstances(AllTransitionInstances);
		TestEqual("All transition instances found", AllTransitionInstances.Num(), TestExpressions.Num());

		for (int32 Idx = 0; Idx < AllTransitionInstances.Num(); ++Idx)
		{
			TestEqual("Lookup by index correct", NodeInstance->GetTransitionInStack(Idx), AllTransitionInstances[Idx]);
		}
	}

	// Test class search.
	{
		USMTransitionInstance* ClassFoundInstance = NodeInstance->GetTransitionInStackByClass(USMTransitionStackTestInstance::StaticClass());
		check(ClassFoundInstance);
		TestEqual("Stack found by class", ClassFoundInstance, NodeInstance->GetTransitionInStack(2));
	}
	{
		USMTransitionInstance* ClassFoundInstance = NodeInstance->GetTransitionInStackByClass(USMTransitionInstance::StaticClass(), true);
		check(ClassFoundInstance);
		TestNotEqual("Didn't find end instance because child found first", ClassFoundInstance, NodeInstance->GetTransitionInStack(2));
		TestEqual("Stack found by class", ClassFoundInstance, NodeInstance->GetTransitionInStack(0));
	}

	{
		TArray<USMTransitionInstance*> FoundClassInstances;
		NodeInstance->GetAllTransitionsInStackOfClass(USMTransitionStackTestInstance::StaticClass(), FoundClassInstances);
		TestEqual("1 result found", FoundClassInstances.Num(), 1);
		TestTrue("Found stack instance", FoundClassInstances.Contains(NodeInstance->GetTransitionInStack(2)));
		
		NodeInstance->GetAllTransitionsInStackOfClass(USMTransitionTestInstance::StaticClass(), FoundClassInstances, true);
		TestEqual("correct results found with children", 2, FoundClassInstances.Num());
		
		NodeInstance->GetAllTransitionsInStackOfClass(USMTransitionInstance::StaticClass(), FoundClassInstances, true);
		TestEqual("correct results found with children", 3, FoundClassInstances.Num());

		// Test index lookup.
		{
			int32 Index = NodeInstance->GetTransitionIndexInStack(FoundClassInstances[0]);
			TestEqual("Index found", Index, 0);
		}
		{
			int32 Index = NodeInstance->GetTransitionIndexInStack(FoundClassInstances[1]);
			TestEqual("Index found", Index, 1);
		}
		{
			int32 Index = NodeInstance->GetTransitionIndexInStack(NodeInstance);
			TestEqual("Index not found", Index, INDEX_NONE);
		}
		{
			int32 Index = NodeInstance->GetTransitionIndexInStack(nullptr);
			TestEqual("Index not found", Index, INDEX_NONE);
		}
	}

	TestEqual("Stack could find node instance", NodeInstance->GetTransitionInStack(0)->GetStackOwnerInstance(), Cast<USMTransitionInstance>(NodeInstance));
	TestEqual("Stack could find node instance", NodeInstance->GetTransitionInStack(1)->GetStackOwnerInstance(), Cast<USMTransitionInstance>(NodeInstance));
	TestEqual("Node instance found itself", NodeInstance->GetStackOwnerInstance(), Cast<USMTransitionInstance>(NodeInstance));
	
	FINISH_TRANSITION_STACK_TEST()
}

/** Single AND node with no custom user logic, connected from an Any State. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransitionStackANDAnyStateTest, "LogicDriver.TransitionStack.AND_AnyState", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTransitionStackANDAnyStateTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST_NO_STATES()

	// Total states to test.
	UEdGraphPin* LastStatePin = nullptr;

	// Build a state machine of only two states.
	{
		const int32 CurrentStates = 1;
		TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, CurrentStates, &LastStatePin);
	}

	USMGraphNode_StateNodeBase* LastNormalState = CastChecked<USMGraphNode_StateNodeBase>(LastStatePin->GetOwningNode());
	LastNormalState->GetNodeTemplateAs<USMStateInstance_Base>(true)->SetExcludeFromAnyState(false);

	// Add any state.
	FGraphNodeCreator<USMGraphNode_AnyStateNode> AnyStateNodeCreator(*StateMachineGraph);
	USMGraphNode_AnyStateNode* AnyState = AnyStateNodeCreator.CreateNode();
	AnyStateNodeCreator.Finalize();

	FString AnyStateInitialStateName = "AnyState_Initial";
	{
		UEdGraphPin* InputPin = AnyState->GetOutputPin();

		// Connect a state to anystate.
		TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, 1, &InputPin);

		AnyState->GetNextState()->GetBoundGraph()->Rename(*AnyStateInitialStateName, nullptr, REN_DontCreateRedirectors);
	}

	USMGraphNode_TransitionEdge* TransitionEdge = AnyState->GetNextTransition();
	check(TransitionEdge);

	TransitionEdge->GetNodeTemplateAs<USMTransitionInstance>()->SetPriorityOrder(-1);
	TestTrue("Graph Transition from Any State", TransitionEdge->IsFromAnyState());

	FTransitionStackContainer NewStack(USMTransitionTestInstance::StaticClass());
	NewStack.Mode = ESMExpressionMode::AND;
	TransitionEdge->TransitionStack.Add(NewStack);

	TransitionEdge->InitTransitionStack();
	CastChecked<USMTransitionTestInstance>(TransitionEdge->TransitionStack[0].NodeStackInstanceTemplate)->bCanTransition = true;
	TransitionEdge->FormatGraphForStackNodes();

	TestTrue("Transition stack initialized", TransitionEdge->HasValidTransitionStack());

	int32 A, B, C;
	TestHelpers::RunStateMachineToCompletion(this, NewBP, A, B, C);

	return NewAsset.DeleteAsset(this);
}

#endif

#endif //WITH_DEV_AUTOMATION_TESTS