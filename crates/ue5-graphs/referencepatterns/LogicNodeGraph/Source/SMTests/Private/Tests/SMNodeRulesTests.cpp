// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMNodeRulesClasses.h"
#include "SMTestContext.h"
#include "SMTestHelpers.h"
#include "Helpers/SMTestBoilerplate.h"

#include "Graph/SMGraph.h"
#include "Graph/Nodes/SMGraphK2Node_StateMachineNode.h"
#include "Graph/Nodes/SMGraphNode_StateNode.h"
#include "Utilities/SMBlueprintEditorUtils.h"
#include "Utilities/SMNodeRuleUtils.h"

#include "Blueprints/SMBlueprint.h"

#if WITH_DEV_AUTOMATION_TESTS

#if PLATFORM_DESKTOP

/**
 * Verify state rules can pass their conditions.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNodeRulesStateConnectionTest, "LogicDriver.NodeInstance.Rules.StateConnections", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FNodeRulesStateConnectionTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(3)

	UEdGraphPin* LastStatePin = nullptr;

	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin,
		USMStateInstance::StaticClass(), USMTransitionInstance::StaticClass());
	USMGraphNode_StateNode* FirstStateNode = FSMBlueprintEditorUtils::GetFirstNodeOfClassNested<USMGraphNode_StateNode>(StateMachineGraph);
	check(FirstStateNode);

	USMGraphNode_StateNode* MainStateNode = CastChecked<USMGraphNode_StateNode>(FirstStateNode->GetNextState());
	USMGraphNode_StateNode* NextStateNode = CastChecked<USMGraphNode_StateNode>(MainStateNode->GetNextState());

	TestTrue("Base state class allowed", LD::NodeRuleUtils::IsNodeClassAllowed(USMStateInstance_Base::StaticClass(), MainStateNode));

	UClass* StateClassToTest = USMTestNodeRules_MainState::StaticClass();
	USMTestNodeRules_MainState* MainNodeCDO = CastChecked<USMTestNodeRules_MainState>(StateClassToTest->ClassDefaultObject);
	
	// Setting rules externally normally wouldn't work. For BP assets the rules are read from the asset registry, not CDO.
	FSMStateConnectionValidator& NodeValidator = MainNodeCDO->GetConnectionValidatorEdit();
	
	NodeValidator.AllowedInboundStates.Add(USMTestNodeRules_PreviousState::StaticClass());
	{
		// Previous state is not correct
		TestFalse("Rule fails without proper previous state", LD::NodeRuleUtils::IsNodeClassAllowed(StateClassToTest, MainStateNode));
	
		// Previous state is now correct
		TestHelpers::SetNodeClass(this, FirstStateNode, USMTestNodeRules_PreviousState::StaticClass());
		TestTrue("Rule passes with proper previous state", LD::NodeRuleUtils::IsNodeClassAllowed(StateClassToTest, MainStateNode));
	}
	
	NodeValidator.AllowedOutboundStates.Add(USMTestNodeRules_NextState::StaticClass());
	{
		// Next state is not correct
		TestFalse("Rule fails without proper next state", LD::NodeRuleUtils::IsNodeClassAllowed(StateClassToTest, MainStateNode));
	
		// Next state is now correct
		TestHelpers::SetNodeClass(this, NextStateNode, USMTestNodeRules_NextState::StaticClass());
		TestTrue("Rule passes with proper next state", LD::NodeRuleUtils::IsNodeClassAllowed(StateClassToTest, MainStateNode));
	}

	NodeValidator.AllowedInStateMachines.Add(USMTestNodeRules_StateMachineOwner::StaticClass());
	{
		TestFalse("Rule fails without proper owning state machine", LD::NodeRuleUtils::IsNodeClassAllowed(StateClassToTest, MainStateNode));

		USMInstance* CDO = CastChecked<USMInstance>(NewBP->GeneratedClass->ClassDefaultObject);
		CDO->SetRootStateMachineNodeClass(USMTestNodeRules_StateMachineOwner::StaticClass());
		
		TestHelpers::SetNodeClass(this, NextStateNode, USMTestNodeRules_NextState::StaticClass());
		TestTrue("Rule passes with proper owning state machine", LD::NodeRuleUtils::IsNodeClassAllowed(StateClassToTest, MainStateNode));
	}
	
	return NewAsset.DeleteAsset(this);
}

/**
 * Verify transitions can pass their conditions.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNodeRulesTransitionConnectionTest, "LogicDriver.NodeInstance.Rules.TransitionConnections", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FNodeRulesTransitionConnectionTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(2)

	UEdGraphPin* LastStatePin = nullptr;

	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin,
		USMStateInstance::StaticClass(), USMTransitionInstance::StaticClass());
	USMGraphNode_StateNode* FirstStateNode = FSMBlueprintEditorUtils::GetFirstNodeOfClassNested<USMGraphNode_StateNode>(StateMachineGraph);
	check(FirstStateNode);

	USMGraphNode_TransitionEdge* TransitionNode = CastChecked<USMGraphNode_TransitionEdge>(FirstStateNode->GetNextTransition());
	USMGraphNode_StateNode* NextStateNode = CastChecked<USMGraphNode_StateNode>(TransitionNode->GetToState());

	TestTrue("Base transition class allowed", LD::NodeRuleUtils::IsNodeClassAllowed(USMTransitionInstance::StaticClass(), TransitionNode));

	UClass* TransitionClassToTest = USMTestNodeRules_Transition::StaticClass();
	USMTestNodeRules_Transition* MainNodeCDO = CastChecked<USMTestNodeRules_Transition>(TransitionClassToTest->ClassDefaultObject);
	
	// Setting rules externally normally wouldn't work. For BP assets the rules are read from the asset registry, not CDO.
	FSMTransitionConnectionValidator& NodeValidator = MainNodeCDO->GetConnectionValidatorEdit();

	FSMNodeConnectionRule& ConnectionRule = NodeValidator.AllowedConnections.AddDefaulted_GetRef();
	
	ConnectionRule.FromState = USMTestNodeRules_PreviousState::StaticClass();
	{
		// Previous state is not correct
		TestFalse("Rule fails without proper previous state", LD::NodeRuleUtils::IsNodeClassAllowed(TransitionClassToTest, TransitionNode));
	
		// Previous state is now correct
		TestHelpers::SetNodeClass(this, FirstStateNode, USMTestNodeRules_PreviousState::StaticClass());
		TestTrue("Rule passes with proper previous state", LD::NodeRuleUtils::IsNodeClassAllowed(TransitionClassToTest, TransitionNode));
	}
	
	ConnectionRule.ToState = USMTestNodeRules_NextState::StaticClass();
	{
		// Next state is not correct
		TestFalse("Rule fails without proper next state", LD::NodeRuleUtils::IsNodeClassAllowed(TransitionClassToTest, TransitionNode));
	
		// Next state is now correct
		TestHelpers::SetNodeClass(this, NextStateNode, USMTestNodeRules_NextState::StaticClass());
		TestTrue("Rule passes with proper next state", LD::NodeRuleUtils::IsNodeClassAllowed(TransitionClassToTest, TransitionNode));
	}

	ConnectionRule.InStateMachine = USMTestNodeRules_StateMachineOwner::StaticClass();
	{
		TestFalse("Rule fails without proper owning state machine", LD::NodeRuleUtils::IsNodeClassAllowed(TransitionClassToTest, TransitionNode));

		USMInstance* CDO = CastChecked<USMInstance>(NewBP->GeneratedClass->ClassDefaultObject);
		CDO->SetRootStateMachineNodeClass(USMTestNodeRules_StateMachineOwner::StaticClass());
		
		TestHelpers::SetNodeClass(this, NextStateNode, USMTestNodeRules_NextState::StaticClass());
		TestTrue("Rule passes with proper owning state machine", LD::NodeRuleUtils::IsNodeClassAllowed(TransitionClassToTest, TransitionNode));
	}
	
	return NewAsset.DeleteAsset(this);
}

#endif

#endif //WITH_DEV_AUTOMATION_TESTS