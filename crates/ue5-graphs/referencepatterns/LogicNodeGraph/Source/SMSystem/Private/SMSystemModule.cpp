// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "ISMSystemModule.h"
#include "SMLogging.h"

DEFINE_LOG_CATEGORY(LogLogicDriver);

#define LOCTEXT_NAMESPACE "SMSystemModule"

class FSMSystemModule : public ISMSystemModule
{
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

void FSMSystemModule::StartupModule()
{
}

void FSMSystemModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FSMSystemModule, SMSystem)

#undef LOCTEXT_NAMESPACE
