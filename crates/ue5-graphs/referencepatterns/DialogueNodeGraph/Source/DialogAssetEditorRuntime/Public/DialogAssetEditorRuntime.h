/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FDialogAssetEditorRuntimeModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
