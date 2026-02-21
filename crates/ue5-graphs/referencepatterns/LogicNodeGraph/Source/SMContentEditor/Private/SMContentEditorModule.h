// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "ISMContentEditorModule.h"

class FExtensibilityManager;
class FSpawnTabArgs;
class FUICommandList;
class SDockTab;

class FSMContentEditorModule : public ISMContentEditorModule
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Gets the extensibility managers for outside entities to extend this editor's menus and toolbars */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() const override { return MenuExtensibilityManager; }
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() const override { return ToolBarExtensibilityManager; }

	virtual bool IsContentCreationEnabled() const override;

protected:
	virtual UFactory* CreateFactory() const override;

private:
	void BindCommands();
	void ParseCommandLine();

	void OnFilesLoaded();
	TSharedRef<SDockTab> SpawnContentInTab(const FSpawnTabArgs& SpawnTabArgs);
	
private:
	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;

	TSharedPtr<FUICommandList> CommandList;
};
