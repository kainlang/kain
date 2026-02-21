// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

class FAutomationTestBase;

class FSMSystemTestsModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void OnTestStart(FAutomationTestBase* Test);
	void OnTestEnd(FAutomationTestBase* Test);
	
private:
	bool bFavorPureCastStoredValue = false;
};
