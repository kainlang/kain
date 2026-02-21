// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMTestHelpers.h"
#include "SMTestContext.h"
#include "Helpers/SMTestBoilerplate.h"

#include "Utilities/SMPropertyUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

#if PLATFORM_DESKTOP

/**
 * Test instanced sub-objects.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInstancedSubObjectTest, "LogicDriver.SubObjects.InstancedObjects", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FInstancedSubObjectTest::RunTest(const FString& Parameters)
{
	USMTestInstancedObjectState* TestInstanceState = NewObject<USMTestInstancedObjectState>();
	TestInstanceState->InstanceObject = NewObject<UTestInstanceSubObject>(TestInstanceState);
	TestInstanceState->InstanceObjectArray.Add(NewObject<UTestInstanceSubObject>(TestInstanceState));

	const UObject* SubObject1 = TestInstanceState->InstanceObject->NestedObject;
	const UObject* SubObject2 = TestInstanceState->InstanceObjectArray[0]->NestedObject;

	TSet<UObject*> FoundSubObjects;

	const TFunction<void(UObject*)> FindSubObjectFunc = [&] (UObject* SubObject)
	{
		FoundSubObjects.Add(SubObject);
	};
	LD::PropertyUtils::ForEachInstancedSubObject(TestInstanceState, FindSubObjectFunc);

	// Just test whether the sub-objects were found. The issue is a cooked build adds the transient flag to these sub-objects
	// and we need to clear it during the compile process. Currently we don't have a good way of automating a
	// packaged build test for this so verifying we can discover the sub-objects will have to suffice.

	check(TestEqual("Objects found", FoundSubObjects.Num(), 4));
	TestTrue("Correct object found", FoundSubObjects.Contains(TestInstanceState->InstanceObject));
	TestTrue("Correct object found", FoundSubObjects.Contains(TestInstanceState->InstanceObjectArray[0]));
	TestTrue("Correct object found", FoundSubObjects.Contains(SubObject1));
	TestTrue("Correct object found", FoundSubObjects.Contains(SubObject2));

	return true;
}

#endif

#endif //WITH_DEV_AUTOMATION_TESTS