// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Construction/SMEditorConstructionConfiguration.h"

#include "SMEditorConstructionManager.h"

FSMDisableConstructionScriptsOnScope::FSMDisableConstructionScriptsOnScope():
bOriginallyEnabled(ISMEditorConstructionManager::Get().AreConstructionScriptsEnabled())
{
	ISMEditorConstructionManager::Get().SetEnableConstructionScripts(false);
}

FSMDisableConstructionScriptsOnScope::~FSMDisableConstructionScriptsOnScope()
{
	Cancel();
}

void FSMDisableConstructionScriptsOnScope::Cancel()
{
	ISMEditorConstructionManager::Get().SetEnableConstructionScripts(bOriginallyEnabled);
}
