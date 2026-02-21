// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class ISMAssetManager;
class ISMGraphGeneration;
class FSMAssetExportManager;
class FSMAssetImportManager;

#define LOGICDRIVER_ASSET_TOOLS_MODULE_NAME "SMAssetTools"

/**
 * The public interface to this module
 */
class ISMAssetToolsModule : public IModuleInterface
{
public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static ISMAssetToolsModule& Get()
	{
		return FModuleManager::LoadModuleChecked<ISMAssetToolsModule>(LOGICDRIVER_ASSET_TOOLS_MODULE_NAME);
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded(LOGICDRIVER_ASSET_TOOLS_MODULE_NAME);
	}

	/**
	 * Return the asset tools interface.
	 */
	virtual TSharedPtr<ISMAssetManager> GetAssetManagerInterface() const = 0;

	/**
	 * Return the graph generation interface.
	 */
	virtual TSharedPtr<ISMGraphGeneration> GetGraphGenerationInterface() const = 0;

	/**
	 * Return the asset export manager for exporting state machine assets.
	 */
	virtual TSharedPtr<FSMAssetExportManager> GetAssetExporter() const = 0;

	/**
	 * Return the asset import manager for exporting state machine assets.
	 */
	virtual TSharedPtr<FSMAssetImportManager> GetAssetImporter() const = 0;
};

