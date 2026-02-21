// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

#define LOGICDRIVER_EXTENDED_RUNTIME_MODULE_NAME "SMExtendedRuntime"

/**
 * The public interface to this module
 */
class ISMExtendedRuntimeModule : public IModuleInterface
{
public:
	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static ISMExtendedRuntimeModule& Get()
	{
		return FModuleManager::LoadModuleChecked<ISMExtendedRuntimeModule>(LOGICDRIVER_EXTENDED_RUNTIME_MODULE_NAME);
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded(LOGICDRIVER_EXTENDED_RUNTIME_MODULE_NAME);
	}
};

	