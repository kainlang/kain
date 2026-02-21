// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Toolkits/IToolkitHost.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "AssetTypeActions_Base.h"
#include "Modules/ModuleManager.h"
#include "QuestBlueprintCompiler.h"

DECLARE_LOG_CATEGORY_EXTERN(LogNarrativeQuestEditor, All, All);

class IQuestEditor;

class FNarrativeQuestEditorModule : public IModuleInterface,
	public IHasMenuExtensibility, public IHasToolBarExtensibility
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void RegisterSettings();
	void UnregisterSettings();

	static uint32 GameAssetCategory;

	virtual TSharedRef<IQuestEditor> CreateQuestEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, class UQuestBlueprint* QuestAsset);

	/** Needed to allow toolbar/menu extensibility */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() override { return MenuExtensibilityManager; }
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() override { return ToolBarExtensibilityManager; }

	/** Quest editor app identifier string */
	static const FName QuestEditorAppId;

private:

	class UQuestEditorSettings* SettingsPtr;

	TSharedPtr<class FAssetTypeActions_Base> QuestAssetTypeActions;
	TSharedPtr<class FAssetTypeActions_Base> QuestActionTypeActions;
	TSharedPtr<class FAssetTypeActions_Base> NarrativeQuestTaskTypeActions;
	TSharedPtr<class FAssetTypeActions_Base> NarrativeConditionTypeActions;
	TSharedPtr<class FAssetTypeActions_Base> NarrativeEventTypeActions;

	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;


	/** Compiler customization for Widgets */
	FQuestBlueprintCompiler QuestBlueprintCompiler;
};
