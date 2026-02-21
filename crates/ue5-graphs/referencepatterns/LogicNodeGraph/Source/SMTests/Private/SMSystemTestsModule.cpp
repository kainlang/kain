// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMSystemTestsModule.h"

#include "BlueprintEditorSettings.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "SMSystemTests"

void FSMSystemTestsModule::StartupModule()
{
	FAutomationTestFramework::Get().OnTestStartEvent.AddRaw(this, &FSMSystemTestsModule::OnTestStart);
	FAutomationTestFramework::Get().OnTestEndEvent.AddRaw(this, &FSMSystemTestsModule::OnTestEnd);
}

void FSMSystemTestsModule::ShutdownModule()
{
	FAutomationTestFramework::Get().OnTestStartEvent.RemoveAll(this);
	FAutomationTestFramework::Get().OnTestEndEvent.RemoveAll(this);
}

void FSMSystemTestsModule::OnTestStart(FAutomationTestBase* Test)
{
	// bFavorPureCastNodes needs to be set for when the test creates pure cast nodes and the node is later duplicated.
	// UE 5.3 changes the pure cast to not be a UPROPERTY, which prevents it from being copied.
	UBlueprintEditorSettings* BlueprintSettings = GetMutableDefault<UBlueprintEditorSettings>();
	bFavorPureCastStoredValue = BlueprintSettings->bFavorPureCastNodes;
	BlueprintSettings->bFavorPureCastNodes = true;
}

void FSMSystemTestsModule::OnTestEnd(FAutomationTestBase* Test)
{
	UBlueprintEditorSettings* BlueprintSettings = GetMutableDefault<UBlueprintEditorSettings>();
	BlueprintSettings->bFavorPureCastNodes = bFavorPureCastStoredValue;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSMSystemTestsModule, SMSystemTests);