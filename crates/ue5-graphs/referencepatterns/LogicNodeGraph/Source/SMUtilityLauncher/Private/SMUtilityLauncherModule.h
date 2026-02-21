// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ISMUtilityLauncherModule.h"

class FToolBarBuilder;
class FExtender;
class FExtensibilityManager;
class FMenuBuilder;
class FUICommandList;
class SWidget;

class FSMUtilityLauncherModule : public ISMUtilityLauncherModule
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Gets the extensibility managers for outside entities to extend this editor's menus and toolbars */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() const override { return MenuExtensibilityManager; }
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() const override { return ToolBarExtensibilityManager; }

private:
	void BindCommands();
	void ExtendLevelEditorToolbar(FToolBarBuilder& ToolbarBuilder);
	TSharedRef<SWidget> GenerateMenuContent() const;

private:
	static void MakeSupportMenu(FMenuBuilder& InMenu);
	static void OpenDocs();
	static void OpenDiscord();
	static void ViewSystemInfo();
	static void CreateNewStateMachineClass();
	static void CreateNewNodeClass();
	static void MakeOpenBlueprintClassMenu(FMenuBuilder& InMenu);
	
private:
	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;
	TSharedPtr<FUICommandList> PluginCommands;
	TSharedPtr<FExtender> ToolbarExtender;
};
