// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMTestHelpers.h"
#include "SMTestContext.h"
#include "Helpers/SMTestBoilerplate.h"

#include "Blueprints/SMBlueprint.h"

#include "Blueprints/SMBlueprintFactory.h"
#include "Utilities/SMBlueprintEditorUtils.h"
#include "Graph/SMGraph.h"
#include "Graph/SMStateGraph.h"
#include "Graph/Nodes/SMGraphK2Node_StateMachineNode.h"
#include "Graph/Nodes/SMGraphNode_StateNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineEntryNode.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/Nodes/SMGraphNode_StateMachineStateNode.h"
#include "Graph/Nodes/SMGraphNode_ConduitNode.h"

#include "Kismet2/KismetEditorUtilities.h"

#if WITH_DEV_AUTOMATION_TESTS

#if PLATFORM_DESKTOP

/**
 * Test state history functions.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStateHistoryTest, "LogicDriver.SMInstance.History", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FStateHistoryTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(3)

	UEdGraphPin* LastStatePin = nullptr;

	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin);
	FKismetEditorUtilities::CompileBlueprint(NewBP);
	
	USMInstance* TestInstance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, NewObject<USMTestContext>());

	TestEqual("No history", TestInstance->GetStateHistory().Num(), 0);

	TestInstance->Start();
	
	TestEqual("No history after start", TestInstance->GetStateHistory().Num(), 0);

	TestNull("PreviousEnteredState null", TestInstance->GetSingleActiveStateInstance()->GetPreviousActiveState());
	TestNull("PreviousEnteredTransition null", TestInstance->GetSingleActiveStateInstance()->GetPreviousActiveTransition());
	
	FPlatformProcess::Sleep(0.1f);
	TestInstance->Update();
	{
		TestEqual("1 state in history", TestInstance->GetStateHistory().Num(), 1);

		const FGuid StateGuid = TestInstance->GetStateHistory()[0].StateGuid;

		TestEqual("Initial state guid found", StateGuid, TestInstance->GetRootStateMachine().GetSingleInitialState()->GetGuid());
		TestNotNull("State instance found from guid", TestInstance->GetStateInstanceByGuid(StateGuid));

		TestEqual("PreviousEnteredState correct", TestInstance->GetSingleActiveStateInstance()->GetPreviousActiveState(),
			Cast<USMStateInstance_Base>(TestInstance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance()));
		TestEqual("PreviousEnteredTransition correct", TestInstance->GetSingleActiveStateInstance()->GetPreviousActiveTransition(),
			Cast<USMTransitionInstance>(TestInstance->GetRootStateMachine().GetSingleInitialState()->GetOutgoingTransitions()[0]->GetOrCreateNodeInstance()));
	}
	FPlatformProcess::Sleep(0.1f);
	TestInstance->Update();
	{
		TestEqual("2 states in history", TestInstance->GetStateHistory().Num(), 2);

		const FGuid StateGuid = TestInstance->GetStateHistory()[1].StateGuid;

		TestNotEqual("Next state guid found", StateGuid, TestInstance->GetRootStateMachine().GetSingleInitialState()->GetGuid());

		USMStateInstance_Base* PrevStateInstance = TestInstance->GetStateInstanceByGuid(StateGuid);
		TestNotNull("State instance found from guid", PrevStateInstance);

		TestNotEqual("History different", TestInstance->GetStateHistory()[0], TestInstance->GetStateHistory()[1]);
		TestTrue("Time stamp greater", TestInstance->GetStateHistory()[1].StartTime > TestInstance->GetStateHistory()[0].StartTime);

		TestEqual("PreviousEnteredState correct", TestInstance->GetSingleActiveStateInstance()->GetPreviousActiveState(), PrevStateInstance);
		
		TestInstance->SetStateHistoryMaxCount(1);
		{
			TestEqual("1 state in history", TestInstance->GetStateHistory().Num(), 1);
			TestEqual("Recent state kept", TestInstance->GetStateHistory()[0].StateGuid, StateGuid);
		}
	}
	
	return NewAsset.DeleteAsset(this);
}

/**
 * Test methods to evaluate a transition chain.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransitionChainTest, "LogicDriver.SMInstance.TransitionChain", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTransitionChainTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(4)
	UEdGraphPin* LastStatePin = nullptr;
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin);
	const USMGraphNode_StateNode* FirstStateNode = CastChecked<USMGraphNode_StateNode>(StateMachineGraph->GetEntryNode()->GetOutputNode());
	
	// Transition size 1.
	{
		USMInstance* Instance = TestHelpers::CompileAndCreateStateMachineInstanceFromBP(NewBP);

		TArray<USMStateInstance_Base*> EntryStates;
		Instance->GetRootStateMachineNodeInstance()->GetEntryStates(EntryStates);

		const USMStateInstance_Base* EntryState = EntryStates[0];

		Instance->Start();
		TestTrue("Entry state active", EntryState->IsActive());
		
		TArray<USMTransitionInstance*> OutTransitionChain;
		USMStateInstance_Base* DestinationState;
		{
			const bool bSuccess = Instance->EvaluateAndFindTransitionChain(EntryState->GetTransitionByIndex(0), OutTransitionChain, DestinationState);
			TestTrue("Chain found", bSuccess);
			TestEqual("Chain correct size", OutTransitionChain.Num(), 1);
			TestNotNull("Destination state found", DestinationState);
		}
		
		{
			const bool bSuccess = Instance->TakeTransitionChain(OutTransitionChain);
			TestTrue("Chain taken", bSuccess);
			TestFalse("Entry state not active", EntryState->IsActive());
			TestTrue("Destination state switched", DestinationState->IsActive());
		}
	}

	{
		USMGraphNode_ConduitNode* ConduitNode = FSMBlueprintEditorUtils::ConvertNodeTo<USMGraphNode_ConduitNode>(FirstStateNode->GetNextState());
		TestHelpers::SetNodeClass(this, ConduitNode, USMConduitTestInstance::StaticClass());
		ConduitNode->GetNodeTemplateAs<USMConduitTestInstance>(true)->bCanTransition = true;
		
		USMInstance* Instance = TestHelpers::CompileAndCreateStateMachineInstanceFromBP(NewBP);
		TArray<USMStateInstance_Base*> EntryStates;
		Instance->GetRootStateMachineNodeInstance()->GetEntryStates(EntryStates);
		
		const USMStateInstance_Base* EntryState = EntryStates[0];

		Instance->Start();
		TestTrue("Entry state active", EntryState->IsActive());
		
		TArray<USMTransitionInstance*> OutTransitionChain;
		USMStateInstance_Base* DestinationState;
		{
			const bool bSuccess = Instance->EvaluateAndFindTransitionChain(EntryState->GetTransitionByIndex(0), OutTransitionChain, DestinationState);
			TestTrue("Chain found", bSuccess);
			TestEqual("Chain correct size", OutTransitionChain.Num(), 2);
			TestNotNull("Destination state found", DestinationState);
		}
		
		{
			const bool bSuccess = Instance->TakeTransitionChain(OutTransitionChain);
			TestTrue("Chain taken", bSuccess);
			TestFalse("Entry state not active", EntryState->IsActive());
			TestTrue("Destination state switched", DestinationState->IsActive());
		}
	}
	
	return NewAsset.DeleteAsset(this);
}

/**
 * Test methods to retrieve or set states by qualified name.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQualifiedStateNameTest, "LogicDriver.SMInstance.QualifiedStateNames", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FQualifiedStateNameTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(2)
	
	UEdGraphPin* LastStatePin = nullptr;
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin);

	const int32 NestedStateCount = 3;
	const USMGraphNode_StateMachineStateNode* NestedFSMNode = TestHelpers::BuildNestedStateMachine(this, StateMachineGraph, NestedStateCount, &LastStatePin, nullptr);

	LastStatePin = NestedFSMNode->GetOutputPin();
	USMGraphNode_StateMachineStateNode* NestedFSMRefNode = TestHelpers::BuildNestedStateMachine(this, StateMachineGraph, NestedStateCount, &LastStatePin, nullptr);

	USMBlueprint* NewReferencedBlueprint = FSMBlueprintEditorUtils::ConvertStateMachineToReference(NestedFSMRefNode,
		false, nullptr, nullptr, USMTestNotifyInstance::StaticClass());
	FAssetHandler ReferencedAsset = TestHelpers::CreateAssetFromBlueprint(NewReferencedBlueprint);
	FKismetEditorUtilities::CompileBlueprint(NewReferencedBlueprint);
	
	USMInstance* Instance = TestHelpers::CompileAndCreateStateMachineInstanceFromBP(NewBP);
	TArray<USMInstance*> References = Instance->GetAllReferencedInstances();

	for (USMInstance* Reference : References)
	{
		CastChecked<USMTestNotifyInstance>(Reference)->bTestLastStateStarted = false;
	}
	
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
	
	for (USMStateInstance_Base* State : AllStatesToCheck)
	{
		FString NameToCheck = State->GetNodeName();
		if (State->GetOwningStateMachineNodeInstance() != Instance->GetRootStateMachineNodeInstance())
		{
			NameToCheck = State->GetOwningStateMachineNodeInstance()->GetNodeName() + TEXT(".") + NameToCheck;
		}
		USMStateInstance_Base* FoundState = Instance->GetStateInstanceByQualifiedName(NameToCheck);
		TestEqual("State found by qualified name", FoundState, State);

		// Test with root added to it.
		NameToCheck = USMInstance::GetRootNodeNameDefault() + TEXT(".") + NameToCheck;
		FoundState = Instance->GetStateInstanceByQualifiedName(NameToCheck);
		TestEqual("State found by qualified name with root attached", FoundState, State);
	}

	// Test root lookup.
	USMStateMachineInstance* RootNode = Cast<USMStateMachineInstance>(Instance->GetStateInstanceByQualifiedName(USMInstance::GetRootNodeNameDefault()));
	TestNotNull("Root instance found", RootNode);

	for (USMStateInstance_Base* State : AllStatesToCheck)
	{
		TestFalse("State not active yet", State->IsActive());
	}

	FString ReferenceNameToCheck;
	
	// Test activation.
	for (USMStateInstance_Base* State : AllStatesToCheck)
	{
		FString NameToCheck = State->GetNodeName();
		if (State->GetOwningStateMachineNodeInstance() != Instance->GetRootStateMachineNodeInstance())
		{
			NameToCheck = State->GetOwningStateMachineNodeInstance()->GetNodeName() + TEXT(".") + NameToCheck;
			ReferenceNameToCheck = NameToCheck;
		}

		TSet<FSMNode_Base*> OwningStateMachines;
		if (const FSMNode_Base* OwningState = State->GetOwningNodeAs<FSMState_Base>())
		{
			// Find all super state machines to the new state.
			while (FSMStateMachine* OwningStateMachine = static_cast<FSMStateMachine*>(OwningState->GetOwnerNode()))
			{
				OwningStateMachines.Add(OwningStateMachine);
				OwningState = OwningState->GetOwnerNode();
			}
		}

		Instance->SwitchActiveStateByQualifiedName(NameToCheck);
		TestTrue("State is active", State->IsActive());

		for (USMStateInstance_Base* OtherState : AllStatesToCheck)
		{
			if (OtherState == State)
			{
				continue;
			}

			if (OwningStateMachines.Contains(OtherState->GetOwningNode()))
			{
				TestTrue("Super state still active", OtherState->IsActive());
			}
			else if (OtherState->GetOwningStateMachineNodeInstance() == State)
			{
				TestEqual("State is active based on if it's an entry state of the current active state.", OtherState->IsActive(), OtherState->IsEntryState());
			}
			else
			{
				TestFalse("Other state deactivated", OtherState->IsActive());
			}
		}
	}

	// Test activation without deactivation.
	{
		for (USMStateInstance_Base* State : AllStatesToCheck)
		{
			FString NameToCheck = State->GetNodeName();
			if (State->GetOwningStateMachineNodeInstance() != Instance->GetRootStateMachineNodeInstance())
			{
				NameToCheck = State->GetOwningStateMachineNodeInstance()->GetNodeName() + TEXT(".") + NameToCheck;
			}

			Instance->SwitchActiveStateByQualifiedName(NameToCheck, false);
			TestTrue("State is active", State->IsActive());
		}

		for (USMStateInstance_Base* State : AllStatesToCheck)
		{
			TestTrue("All states active", State->IsActive());
		}
	}

	// Test all deactivated
	Instance->SwitchActiveState(nullptr);
	for (USMStateInstance_Base* State : AllStatesToCheck)
	{
		TestFalse("All states deactivated", State->IsActive());
	}

	// Test switching to a sub state machine in a reference properly sets variables and calls events on the reference.
	{
		Instance->Start();
		USMStateInstance_Base* InitialState = Instance->GetSingleActiveStateInstance();
		check(InitialState);
		
		USMStateInstance_Base* SubState = Instance->GetStateInstanceByQualifiedName(ReferenceNameToCheck);
		check(SubState);

		USMTestNotifyInstance* ReferencedInstance = CastChecked<USMTestNotifyInstance>(SubState->GetStateMachineInstance());
		TestFalse("SubState is in a reference", ReferencedInstance->IsPrimaryReferenceOwner());
		ReferencedInstance->TimesStateMachineStarted = 0;
		ReferencedInstance->TimesStateMachineStopped = 0;
		
		Instance->SwitchActiveState(SubState);
		
		TestTrue("Reference has started", ReferencedInstance->HasStarted());
		TestEqual("OnStateMachineStarted called", ReferencedInstance->TimesStateMachineStarted, 1);

		// Should deactivate reference.
		Instance->SwitchActiveState(InitialState);
		TestFalse("Reference has stopped", ReferencedInstance->HasStarted());
		TestEqual("OnStateMachineStopped called", ReferencedInstance->TimesStateMachineStopped, 1);
	}
	
	ReferencedAsset.DeleteAsset(this);
	return NewAsset.DeleteAsset(this);
}

/**
 * Test guid redirect.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGuidRedirectTest, "LogicDriver.SMInstance.GuidRedirect", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FGuidRedirectTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(3)

	UEdGraphPin* LastStatePin = nullptr;
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin);
	
	const int32 NestedStateCount = 3;
	const USMGraphNode_StateMachineStateNode* NestedFSMNode = TestHelpers::BuildNestedStateMachine(this, StateMachineGraph, NestedStateCount, &LastStatePin, nullptr);

	LastStatePin = NestedFSMNode->GetOutputPin();
	USMGraphNode_StateMachineStateNode* NestedFSMRefNode = TestHelpers::BuildNestedStateMachine(this, StateMachineGraph, NestedStateCount, &LastStatePin, nullptr);

	USMBlueprint* NewReferencedBlueprint = FSMBlueprintEditorUtils::ConvertStateMachineToReference(NestedFSMRefNode, false, nullptr, nullptr);
	FAssetHandler ReferencedAsset = TestHelpers::CreateAssetFromBlueprint(NewReferencedBlueprint);
	FKismetEditorUtilities::CompileBlueprint(NewReferencedBlueprint);
	
	USMInstance* Instance = TestHelpers::CompileAndCreateStateMachineInstanceFromBP(NewBP);
	check(Instance->IsInitialized());
	
	TMap<FGuid, FGuid> OldToNewGuids;

	for (const TTuple<FGuid, FSMNode_Base*>& Node : Instance->GetNodeMap())
	{
		// Just generate a guid representing an 'old' guid.
		OldToNewGuids.Add(FGuid::NewGuid(), Node.Value->GetGuid());
	}

	Instance->SetGuidRedirectMap(OldToNewGuids);

	for (const TTuple<FGuid, FGuid>& OldNewGuid : OldToNewGuids)
	{
		const FGuid& OldGuid = OldNewGuid.Key;
		USMNodeInstance* Node = Instance->GetNodeInstanceByGuid(OldGuid);
		TestNotNull("Node found by old guid", Node);
		TestNotEqual("Old guid not the same as the current node guid", OldGuid, Node->GetGuid());

		if (USMStateInstance_Base* IsState = Cast<USMStateInstance_Base>(Node))
		{
			Instance->LoadFromState(OldGuid);
		}
	}

	Instance->Start();
	TArray<USMStateInstance_Base*> AllStateInstances;
	Instance->GetAllStateInstances(AllStateInstances);
	check(AllStateInstances.Num() > 0);

	for (const USMStateInstance_Base* State : AllStateInstances)
	{
		TestTrue("State was loaded from old guid", State->IsActive());
	}

	ReferencedAsset.DeleteAsset(this);
	return NewAsset.DeleteAsset(this);
}

/**
 * Test guids are cached.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGuidCacheTest, "LogicDriver.SMInstance.GuidCache", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FGuidCacheTest::RunTest(const FString& Parameters)
{
	const int32 StateCount = 3;

	SETUP_NEW_STATE_MACHINE_FOR_TEST(StateCount)

	UEdGraphPin* LastStatePin = nullptr;
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin);

	const int32 NestedStateCount = StateCount;
	const USMGraphNode_StateMachineStateNode* NestedFSMNode = TestHelpers::BuildNestedStateMachine(this, StateMachineGraph, NestedStateCount, &LastStatePin, nullptr);

	LastStatePin = NestedFSMNode->GetOutputPin();
	USMGraphNode_StateMachineStateNode* NestedFSMRefNode = TestHelpers::BuildNestedStateMachine(this, StateMachineGraph, NestedStateCount, &LastStatePin, nullptr);

	USMBlueprint* NewReferencedBlueprint = FSMBlueprintEditorUtils::ConvertStateMachineToReference(NestedFSMRefNode, false, nullptr, nullptr);
	FAssetHandler ReferencedAsset = TestHelpers::CreateAssetFromBlueprint(NewReferencedBlueprint);
	FKismetEditorUtilities::CompileBlueprint(NewReferencedBlueprint);
	FKismetEditorUtilities::CompileBlueprint(NewBP);

	const USMInstance* CDO = CastChecked<USMInstance>(NewBP->GeneratedClass->ClassDefaultObject);

	const TMap<FGuid, FSMGuidMap>& Cache = CDO->GetRootPathGuidCache();
	TestEqual("Cache has root sms", Cache.Num(), 2);

	int32 TotalNodesCached = 2;
	for (const auto& KeyVal : Cache)
	{
		TotalNodesCached += KeyVal.Value.NodeToPathGuids.Num();
	}

	{
		USMInstance* Instance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, NewObject<USMTestContext>());
		FSMStateMachine::FGetNodeArgs Args;
		Args.bIncludeNested = true;
		TArray<FSMNode_Base*> Nodes = Instance->GetRootStateMachine().GetAllNodes(Args);
		check(Nodes.Num() > 0);

		TestEqual("Nodes initialized equal to number of nodes cached", Nodes.Num() + 1, TotalNodesCached);

		for (const FSMNode_Base* Node : Nodes)
		{
			TestTrue("Path guid set from compile", Node->GetGuid().IsValid());
		}
	}

	ReferencedAsset.DeleteAsset(this);
	return NewAsset.DeleteAsset(this);
}

/**
 * Test that it can be determined if a state machine is stopping.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIsStoppingTest, "LogicDriver.SMInstance.IsStopping", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FIsStoppingTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST(2)

	UEdGraphPin* LastStatePin = nullptr;

	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMStateIsStoppingTestInstance::StaticClass());
	FKismetEditorUtilities::CompileBlueprint(NewBP);

	USMInstance* TestInstance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, NewObject<USMTestContext>());
	TestHelpers::RunAllStateMachinesToCompletion(this, TestInstance);

	TestInstance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, NewObject<USMTestContext>());
	TArray<USMStateInstance_Base*> StateInstances;
	TestInstance->GetAllStateInstances(StateInstances);

	TArray<USMStateInstance_Base*> EntryStates;
	TestInstance->GetRootStateMachineNodeInstance()->GetEntryStates(EntryStates);
	check(EntryStates.Num() == 1);

	// Leave entry state's bEnsureIsStoppingOnStateEnd false, since it will exit via update.
	// Set last state's bEnsureIsStoppingOnStateEnd true, so we will call Stop manually.
	CastChecked<USMStateIsStoppingTestInstance>(EntryStates[0]->GetNextStateByTransitionIndex(0))->bEnsureIsStoppingOnStateEnd = true;

	TestInstance->Start();
	TestInstance->Update(0.f);
	TestInstance->Stop();

	return true;
}

#endif

#endif //WITH_DEV_AUTOMATION_TESTS