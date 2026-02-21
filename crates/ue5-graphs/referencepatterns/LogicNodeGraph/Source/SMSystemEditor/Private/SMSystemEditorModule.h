// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "ISMSystemEditorModule.h"
#include "Blueprints/SMBlueprintAssetTypeActions.h"
#include "Compilers/SMKismetCompiler.h"

#include "EdGraphUtilities.h"

class FExtensibilityManager;

class FSMSystemEditorModule : public ISMSystemEditorModule
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() const override { return MenuExtensibilityManager; }
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() const override { return ToolBarExtensibilityManager; }

	virtual FExtendNodeInstanceDetails& GetExtendNodeInstanceDetails() override { return ExtendNodeInstanceDetails; }
	virtual FExtendGraphNodeContextMenu& GetExtendGraphNodeContextMenu() override { return ExtendGraphNodeContextMenu; }

	virtual TSharedPtr<FWorkspaceItem> GetToolsWorkspaceGroup() const override;

	/** If the user has pressed play in editor. */
	virtual bool IsPlayingInEditor() const override { return bPlayingInEditor; }

	virtual void RegisterBlueprintVariableCustomization() override;
	virtual void UnregisterBlueprintVariableCustomization() override;

private:
	void RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action);
	static TSharedPtr<FKismetCompilerContext> GetCompilerForStateMachineBP(UBlueprint* BP, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompileOptions);
	static TSharedPtr<FKismetCompilerContext> GetCompilerForNodeBP(UBlueprint* BP, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompileOptions);
	
	void RegisterSettings();
	void UnregisterSettings();

	void RegisterPinFactories();
	void UnregisterPinFactories();

	void BeginPIE(bool bValue);
	void EndPie(bool bValue);

	void OnAssetAdded(const FAssetData& InAssetData);

	void CheckForNewInstalledVersion();
	void DisplayUpdateNotification(const struct FPluginDescriptor& Descriptor, bool bIsUpdate);
	void OnViewNewPatchNotesClicked(FString Version);
	void OnDismissUpdateNotificationClicked();

	void HandleModuleChanged(FName ModuleName, EModuleChangeReason ChangeReason);
	
private:
	TArray<TSharedPtr<IAssetTypeActions>> CreatedAssetTypeActions;

	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;

	mutable TSharedPtr<FWorkspaceItem> LogicDriverToolsWorkspaceGroup;

	TSharedPtr<FGraphPanelNodeFactory> SMGraphPanelNodeFactory;
	
	TSharedPtr<FGraphPanelPinFactory> SMGraphPinNodeFactory;
	TSharedPtr<FGraphPanelPinFactory> SMPinNodeNameFactory;
	TSharedPtr<FGraphPanelPinFactory> SMPinSoftActorReferenceFactory;
	TSharedPtr<FGraphPanelPinFactory> SMPinObjectFactory;
	
	FSMKismetCompiler SMBlueprintCompiler;
	FSMNodeKismetCompiler SMNodeBlueprintCompiler;
	
	FDelegateHandle RefreshAllNodesDelegateHandle;
	FDelegateHandle RenameVariableDelegateHandle;
	FDelegateHandle ModuleChangedHandle;
	/** For variable customization in UE 5.1+ only. */
	FDelegateHandle BlueprintVariableCustomizationHandle;

	FDelegateHandle BeginPieHandle;
	FDelegateHandle EndPieHandle;

	FDelegateHandle AssetAddedHandle;
	FDelegateHandle FilesLoadedHandle;

	FExtendNodeInstanceDetails ExtendNodeInstanceDetails;
	FExtendGraphNodeContextMenu ExtendGraphNodeContextMenu;

	/** Notification popup that the plugin has updated. */
	TWeakPtr<SNotificationItem> NewVersionNotification;
	
	/** If the user has pressed play in editor. */
	bool bPlayingInEditor = false;
};
