// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMTestHelpers.h"
#include "Helpers/SMTestBoilerplate.h"

#include "SMUtils.h"
#include "Utilities/SMBlueprintEditorUtils.h"
#include "Graph/SMGraph.h"
#include "Graph/Nodes/SMGraphK2Node_StateMachineNode.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/Nodes/SMGraphNode_StateMachineStateNode.h"
#include "Graph/Nodes/FunctionNodes/SMGraphK2Node_StateReadNodes.h"

#include "Blueprints/SMBlueprint.h"
#include "Blueprints/SMBlueprintGeneratedClass.h"

#include "EdGraph/EdGraph.h"
#include "Kismet2/KismetEditorUtilities.h"

#if WITH_DEV_AUTOMATION_TESTS

#if PLATFORM_DESKTOP

struct FLatentInitializeHelper
{
	TWeakObjectPtr<USMInstance> InitializingInstance;
	TArray<FAssetHandler> ReferencedAssets;
	FAutomationTestBase* Test;
	int32 Iterations = 0;
	bool bCallbackCompleted = false;

	~FLatentInitializeHelper()
	{
		Cleanup();
	}
	
	void Cleanup()
	{
		for (FAssetHandler& ReferencedAsset : ReferencedAssets)
		{
			ReferencedAsset.DeleteAsset(Test);
		}
	}
};

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FAsyncInitializeCommand, TSharedPtr<FLatentInitializeHelper>, Payload);

bool FAsyncInitializeCommand::Update()
{
	if (!Payload.IsValid())
	{
		return false;
	}

	if (Payload->bCallbackCompleted)
	{
		return true;
	}
	
	constexpr int32 MaxIterations = 1000;
	if (Payload->Iterations++ >= MaxIterations)
	{
		Payload->Test->TestTrue("Async initialize timed out", false);
		return true;
	}

	return false;
}


/**
 * Create a state machine instance async.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAsyncCreateInstanceTest, "LogicDriver.Async.CreateInstance", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FAsyncCreateInstanceTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST_NO_STATES()
	
	constexpr int32 TotalStatesBeforeReferences = 10;
	constexpr int32 TotalStatesAfterReferences = 10;
	constexpr int32 TotalNestedStates = 10;
	constexpr int32 TotalReferences = 2;

	TArray<FAssetHandler> ReferencedAssets;
	TArray<USMGraphNode_StateMachineStateNode*> NestedStateMachineNodes;
	
	const int32 TotalStates = TestHelpers::BuildStateMachineWithReferences(this, StateMachineGraph, TotalStatesBeforeReferences, TotalStatesAfterReferences,
		TotalReferences, TotalNestedStates, ReferencedAssets, NestedStateMachineNodes);

	check(ReferencedAssets.Num() == TotalReferences);
	check(NestedStateMachineNodes.Num() == TotalReferences);

	for (const USMGraphNode_StateMachineStateNode* ReferenceNode : NestedStateMachineNodes)
	{
		USMGraphNode_TransitionEdge* TransitionFromNestedStateMachine = CastChecked<USMGraphNode_TransitionEdge>(ReferenceNode->GetOutputPin()->LinkedTo[0]->GetOwningNode());
		UEdGraph* TransitionGraph = TransitionFromNestedStateMachine->GetBoundGraph();
		TransitionGraph->Nodes.Empty();
		TransitionGraph->GetSchema()->CreateDefaultNodesForGraph(*TransitionGraph);

		TestHelpers::AddSpecialBooleanTransitionLogic<USMGraphK2Node_StateMachineReadNode_InEndState>(this, TransitionFromNestedStateMachine);
	}

	FKismetEditorUtilities::CompileBlueprint(NewBP);
	
	const TSharedPtr<FLatentInitializeHelper> Payload = MakeShared<FLatentInitializeHelper>();

	Payload->ReferencedAssets = MoveTemp(ReferencedAssets);
	Payload->ReferencedAssets.Add(NewAsset);
	Payload->Test = this;
	
	ADD_LATENT_AUTOMATION_COMMAND(FAsyncInitializeCommand(Payload));
	
	USMTestContext* Context = NewObject<USMTestContext>();
	USMInstance* Instance = USMBlueprintUtils::CreateStateMachineInstanceAsync(
		NewBP->GetGeneratedClass(),
		Context,
		FOnStateMachineInstanceInitializedAsync::CreateLambda([&, Payload](USMInstance* CreatedInstance)
		{
			check(CreatedInstance);
			TestTrue("Instance initialized async", CreatedInstance->IsInitialized());
			TestFalse("Instance no longer initializing async", CreatedInstance->IsInitializingAsync());

			TestHelpers::RunAllStateMachinesToCompletion(this, CreatedInstance);
			Payload->bCallbackCompleted = true;
		}));
	
	TestNotNull("Instance created", Instance);
	TestFalse("Instance not initialized yet", Instance->IsInitialized());
	TestTrue("Instance is initializing async", Instance->IsInitializingAsync());
	
	return true;
}

/** Create a state machine instance async and blocking wait for it to finish. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAsyncInitializeWaitTest, "LogicDriver.Async.InitializeWait", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FAsyncInitializeWaitTest::RunTest(const FString& Parameters)
{
	SETUP_NEW_STATE_MACHINE_FOR_TEST_NO_STATES()

	constexpr int32 TotalStatesBeforeReferences = 10;
	constexpr int32 TotalStatesAfterReferences = 10;
	constexpr int32 TotalNestedStates = 10;
	constexpr int32 TotalReferences = 2;

	TArray<FAssetHandler> ReferencedAssets;
	TArray<USMGraphNode_StateMachineStateNode*> NestedStateMachineNodes;
	
	TestHelpers::BuildStateMachineWithReferences(this, StateMachineGraph, TotalStatesBeforeReferences, TotalStatesAfterReferences,
		TotalReferences, TotalNestedStates, ReferencedAssets, NestedStateMachineNodes);

	check(ReferencedAssets.Num() == TotalReferences);
	check(NestedStateMachineNodes.Num() == TotalReferences);

	for (const USMGraphNode_StateMachineStateNode* ReferenceNode : NestedStateMachineNodes)
	{
		USMGraphNode_TransitionEdge* TransitionFromNestedStateMachine = CastChecked<USMGraphNode_TransitionEdge>(ReferenceNode->GetOutputPin()->LinkedTo[0]->GetOwningNode());
		UEdGraph* TransitionGraph = TransitionFromNestedStateMachine->GetBoundGraph();
		TransitionGraph->Nodes.Empty();
		TransitionGraph->GetSchema()->CreateDefaultNodesForGraph(*TransitionGraph);

		TestHelpers::AddSpecialBooleanTransitionLogic<USMGraphK2Node_StateMachineReadNode_InEndState>(this, TransitionFromNestedStateMachine);
	}

	FKismetEditorUtilities::CompileBlueprint(NewBP);

	USMTestContext* Context = NewObject<USMTestContext>();
	USMInstance* Instance = USMBlueprintUtils::CreateStateMachineInstance(NewBP->GetGeneratedClass(), Context, false);
	Instance->InitializeAsync(Context);
	TestFalse("Instance not initialized", Instance->IsInitialized());
	TestTrue("Instance initializing async", Instance->IsInitializingAsync());

	Instance->WaitForAsyncInitializationTask(true);
	
	TestTrue("Instance is initialized", Instance->IsInitialized());
	TestFalse("Instance finished initializing async", Instance->IsInitializingAsync());

	TestHelpers::RunAllStateMachinesToCompletion(this, Instance);
	
	NewAsset.DeleteAsset(this);
	for (FAssetHandler& ReferencedAsset : ReferencedAssets)
	{
		ReferencedAsset.DeleteAsset(this);
	}
	
	return true;
}

/**
 * Verify nodes can detect if they aren't thread safe.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAsyncNodeEditorThreadSafeTest, "LogicDriver.Async.EditorThreadSafeCheck", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FAsyncNodeEditorThreadSafeTest::RunTest(const FString& Parameters)
{
	//AddExpectedError("Setting 'Is Editor Thread Safe' to false", EAutomationExpectedErrorFlags::Contains, 1);
	
	FAssetHandler StateAsset;
	if (!TestHelpers::TryCreateNewNodeAsset(this, StateAsset, USMTextGraphState::StaticClass(), true))
	{
		return false;
	}
	
	USMNodeBlueprint* NodeBlueprint = StateAsset.GetObjectAs<USMNodeBlueprint>();
	FKismetEditorUtilities::CompileBlueprint(NodeBlueprint);

	const USMNodeInstance* NodeInstance = CastChecked<USMNodeInstance>(NodeBlueprint->GeneratedClass->ClassDefaultObject);
	TestFalse("Editor thread safety is false", NodeInstance->GetIsEditorThreadSafe());
	return StateAsset.DeleteAsset(this);
}

#endif

#endif //WITH_DEV_AUTOMATION_TESTS