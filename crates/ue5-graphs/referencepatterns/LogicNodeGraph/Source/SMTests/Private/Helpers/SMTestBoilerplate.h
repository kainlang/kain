// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMTestHelpers.h"

#include "Graph/SMGraph.h"
#include "Graph/Nodes/SMGraphK2Node_StateMachineNode.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "Blueprints/SMBlueprint.h"

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

static bool SetupTest(FAutomationTestBase* InTest, FAssetHandler& OutNewAsset, USMBlueprint** OutNewBP,
	USMGraphK2Node_StateMachineNode** OutRootStateMachineNode, USMGraph** OutStateMachineGraph, TSubclassOf<USMInstance> StateMachineParentClass = nullptr)
{
	if (!TestHelpers::TryCreateNewStateMachineAsset(InTest, OutNewAsset, false, StateMachineParentClass))
	{
		return false;
	}
	*OutNewBP = OutNewAsset.GetObjectAs<USMBlueprint>();
	*OutRootStateMachineNode = FSMBlueprintEditorUtils::GetRootStateMachineNode(*OutNewBP);
	*OutStateMachineGraph = (*OutRootStateMachineNode)->GetStateMachineGraph();
	return true;
}

/**
 * Additional arguments for the SETUP_NEW_STATE_MACHINE macros to avoid multiple macro definitions.
 * Anything here should be reset at the end of FSMTestScopeHelper.
 */
namespace LD::Tests::Setup
{
	/** The parent class to use when creating a new state machine.  */
	inline TSubclassOf<USMInstance> ParentAssetClass = nullptr;
}

/** Run for duration of a test. */
struct FSMTestScopeHelper
{
	FSMTestScopeHelper()
	{
		FSMNode_Base::bValidateGuids = true;
	}

	~FSMTestScopeHelper()
	{
		FSMNode_Base::bValidateGuids = false;
		LD::Tests::Setup::ParentAssetClass = nullptr;
	}
};

#define SETUP_NEW_STATE_MACHINE_FOR_TEST_NO_STATES() \
FSMTestScopeHelper SMTestScopeHelper; \
FAssetHandler NewAsset; \
USMBlueprint* NewBP = nullptr; \
USMGraphK2Node_StateMachineNode* RootStateMachineNode = nullptr; \
USMGraph* StateMachineGraph = nullptr; \
if (!SetupTest(this, NewAsset, &NewBP, &RootStateMachineNode, &StateMachineGraph, LD::Tests::Setup::ParentAssetClass)) \
{ \
	return false; \
} \

#define SETUP_NEW_STATE_MACHINE_FOR_TEST(num_states) \
SETUP_NEW_STATE_MACHINE_FOR_TEST_NO_STATES() \
int32 TotalStates = num_states; \

#endif