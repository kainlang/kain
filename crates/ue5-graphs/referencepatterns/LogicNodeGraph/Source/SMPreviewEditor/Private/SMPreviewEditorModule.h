// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "ISMPreviewEditorModule.h"

class FMenuBuilder;
class FExtensibilityManager;

class FSMPreviewEditorModule : public ISMPreviewEditorModule
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Gets the extensibility managers for outside entities to extend this editor's menus and toolbars */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() const override { return MenuExtensibilityManager; }
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() const override { return ToolBarExtensibilityManager; }

	// ISMPreviewEditorModule
	virtual USMPreviewObject* CreatePreviewObject(UObject* Outer) override;
	virtual USMPreviewObject* RecreatePreviewObject(USMPreviewObject* OriginalPreviewObject) override;
	
	virtual void StartPreviewSimulation(USMBlueprint* StateMachineBlueprint) override;
	virtual bool CanStartPreviewSimulation(USMBlueprint* StateMachineBlueprint) override;
	virtual void StopPreviewSimulation(USMBlueprint* StateMachineBlueprint) override;
	virtual bool IsPreviewRunning(USMBlueprint* StateMachineBlueprint) override;

	virtual void DeleteSelection(TWeakPtr<FSMBlueprintEditor> InBlueprintEditor) override;
	
	virtual TSharedRef<SWidget> CreatePreviewEditorWidget(TWeakPtr<FSMBlueprintEditor> InBlueprintEditor, const FName& InTabID) override;
	virtual TSharedRef<SWidget> CreatePreviewViewportWidget(TWeakPtr<FSMBlueprintEditor> InBlueprintEditor) override;
	virtual TSharedRef<SWidget> CreateAdvancedSceneDetailsWidget(TWeakPtr<FSMBlueprintEditor> InBlueprintEditor, TSharedPtr<SWidget> InViewportWidget) override;
	// ~ISMPreviewEditorModule

private:
	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;
};
