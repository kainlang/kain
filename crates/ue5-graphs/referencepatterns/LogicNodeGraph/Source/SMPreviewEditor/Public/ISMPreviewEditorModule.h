// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "Widgets/SWidget.h"

class FExtensibilityManager;
class FSMBlueprintEditor;
class SWidget;
class USMBlueprint;
class USMPreviewObject;

DECLARE_LOG_CATEGORY_EXTERN(LogLogicDriverPreviewEditor, Log, All);
#define LOGICDRIVER_PREVIEW_MODULE_NAME "SMPreviewEditor"

/**
 * The public interface to this module
 */
class ISMPreviewEditorModule : public IModuleInterface
{
public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static ISMPreviewEditorModule& Get()
	{
		return FModuleManager::LoadModuleChecked< ISMPreviewEditorModule >(LOGICDRIVER_PREVIEW_MODULE_NAME);
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded(LOGICDRIVER_PREVIEW_MODULE_NAME);
	}

	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() const = 0;
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() const = 0;

	virtual USMPreviewObject* CreatePreviewObject(UObject* Outer) = 0;
	virtual USMPreviewObject* RecreatePreviewObject(USMPreviewObject* OriginalPreviewObject) = 0;
	
	virtual void StartPreviewSimulation(USMBlueprint* StateMachineBlueprint) = 0;
	virtual bool CanStartPreviewSimulation(USMBlueprint* StateMachineBlueprint) = 0;
	virtual void StopPreviewSimulation(USMBlueprint* StateMachineBlueprint) = 0;
	virtual bool IsPreviewRunning(USMBlueprint* StateMachineBlueprint) = 0;

	/** Deletes the current selection from preview. */
	virtual void DeleteSelection(TWeakPtr<FSMBlueprintEditor> InBlueprintEditor) = 0;
	
	virtual TSharedRef<SWidget> CreatePreviewEditorWidget(TWeakPtr<FSMBlueprintEditor> InBlueprintEditor, const FName& InTabID) = 0;
	virtual TSharedRef<SWidget> CreatePreviewViewportWidget(TWeakPtr<FSMBlueprintEditor> InBlueprintEditor) = 0;
	virtual TSharedRef<SWidget> CreateAdvancedSceneDetailsWidget(TWeakPtr<FSMBlueprintEditor> InBlueprintEditor, TSharedPtr<SWidget> InViewportWidget) = 0;
};

