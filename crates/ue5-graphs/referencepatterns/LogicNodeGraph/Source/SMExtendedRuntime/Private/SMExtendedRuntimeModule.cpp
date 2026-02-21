// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "ISMExtendedRuntimeModule.h"

#include "SMTextGraphLogging.h"

DEFINE_LOG_CATEGORY(LogLogicDriverExtended)

class FSMExtendedRuntimeModule : public ISMExtendedRuntimeModule
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE(FSMExtendedRuntimeModule, SMExtendedRuntime)


void FSMExtendedRuntimeModule::StartupModule()
{
	// This code will execute after your module is loaded into memory (but after global variables are initialized, of course.)
}


void FSMExtendedRuntimeModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}



