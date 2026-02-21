// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"
#include "Modules/ModuleManager.h"
#include "DialogueBlueprintCompiler.h"

DECLARE_LOG_CATEGORY_EXTERN(LogNarrativeDialogueEditor, All, All);

class FNarrativeDialogueEditorModule : public IModuleInterface,
	public IHasMenuExtensibility, public IHasToolBarExtensibility
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void RegisterSettings();
	void UnregisterSettings();

	static uint32 GameAssetCategory;

	virtual TSharedRef<class IDialogueEditor> CreateDialogueEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, class UDialogueBlueprint* DialogueAsset);

	/** Needed to allow toolbar/menu extensibility */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() override { return MenuExtensibilityManager; }
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() override { return ToolBarExtensibilityManager; }

	/** Dialogue editor app identifier string */
	static const FName DialogueEditorAppId;

private:

	TSharedPtr<class FAssetTypeActions_Base> DialogueAssetTypeActions;
	TSharedPtr<class FAssetTypeActions_Base> LegacyDialogueAssetTypeActions;

	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;

	/** Compiler customization for Widgets */
	FDialogueBlueprintCompiler DialogueBlueprintCompiler;
};
