// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h" 

class FComboGraphASC : public IModuleInterface
{
public: 
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};