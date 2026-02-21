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
#include "Styling/SlateStyle.h"

class FDialogAssetEditorModule : public IModuleInterface,
	public IHasMenuExtensibility, public IHasToolBarExtensibility
{
public:
	void InitializeStyleset();
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static uint32 AssetCategory;
	
	/** IHasMenuExtensibility & IHasToolBarExtensibility implementations */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() override { return MenuExtensibilityManager; }
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() override { return ToolBarExtensibilityManager; }

private:
	TSharedPtr<FSlateStyleSet> _styleSet = nullptr;
	TSharedPtr<struct FDialogPinFactory> _pinFactory = nullptr;

	// Addition to customize node visual :
	TSharedPtr<struct FDialogNodeFactory> _nodeFactory = nullptr;

	TSharedPtr<class FAssetTypeActions_Base> DialogueAssetTypeActions;
	TSharedPtr<class FAssetTypeActions_Base> DialogueConditionTypeActions;
	TSharedPtr<class FAssetTypeActions_Base> DialogueActionTypeActions;
	TSharedPtr<class FAssetTypeActions_Base> DialogueCameraShotTypeActions;

	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;

	void RegisterDialogueEditorSettings();
	void UnregisterDialogueEditorSettings();
};
