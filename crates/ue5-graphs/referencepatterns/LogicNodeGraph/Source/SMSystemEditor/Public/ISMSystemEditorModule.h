// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Runtime/Launch/Resources/Version.h"
#include "Modules/ModuleManager.h"

class FExtensibilityManager;
class FKismetCompilerContext;
class ISMStateMachineBlueprintEditor;
class IDetailLayoutBuilder;
class FWorkspaceItem;
class UGraphNodeContextMenuContext;
class UToolMenu;

#define LOGICDRIVER_EDITOR_MODULE_NAME "SMSystemEditor"

// Only UE 5.1+ supports proper variable customization. Before this one plugin's customization could override another.
// This is used to help keep the codebase relatively in sync between engine versions and will likely be removed later.
#define LOGICDRIVER_HAS_PROPER_VARIABLE_CUSTOMIZATION (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1)

/**
 * The public interface to this module
 */
class ISMSystemEditorModule : public IModuleInterface
{

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FExtendNodeInstanceDetails, IDetailLayoutBuilder&);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FExtendGraphNodeContextMenu, UToolMenu*, UGraphNodeContextMenuContext*);

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static ISMSystemEditorModule& Get()
	{
		return FModuleManager::LoadModuleChecked<ISMSystemEditorModule>(LOGICDRIVER_EDITOR_MODULE_NAME);
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded(LOGICDRIVER_EDITOR_MODULE_NAME);
	}

	/** Gets the extensibility managers for outside entities to extend this editor's menus and toolbars */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() const = 0;
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() const = 0;

	/** Extension for modifying the details panel of node instances in the state machine graph. */
	virtual FExtendNodeInstanceDetails& GetExtendNodeInstanceDetails() = 0;

	/** Extend the context menu when selecting a node in the state machine graph. */
	virtual FExtendGraphNodeContextMenu& GetExtendGraphNodeContextMenu() = 0;

	/** Return the tools workspace group Logic Driver uses. */
	virtual TSharedPtr<FWorkspaceItem> GetToolsWorkspaceGroup() const = 0;

	/** If a PIE session is in progress. */
	virtual bool IsPlayingInEditor() const = 0;

	/** Register customization with the blueprint module. */
	virtual void RegisterBlueprintVariableCustomization() = 0;

	/** Unregister customization from the blueprint module. */
	virtual void UnregisterBlueprintVariableCustomization() = 0;
};

