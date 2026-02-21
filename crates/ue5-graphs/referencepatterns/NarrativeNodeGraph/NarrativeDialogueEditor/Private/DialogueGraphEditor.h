// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Misc/NotifyHook.h"
#include "EditorUndoClient.h"
#include "Widgets/SWidget.h"
#include "GraphEditor.h"
#include "DialogueBlueprint.h"
#include "IDialogueEditor.h"


//class FDialogueNodeTextImporter
//{
//	
//	FDialogueNodeTextImporter(FDialogueGraphEditor* InEditor)
//	{
//		Editor = InEditor;
//	}
//
//	FDialogueGraphEditor* Editor;
//
//};

class FDialogueGraphEditor : public IDialogueEditor
{
	
public:

	FDialogueGraphEditor();
	virtual ~FDialogueGraphEditor();
	
	virtual void OnSelectedNodesChangedImpl(const TSet<class UObject*>& NewSelection) override;

	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) override;

	void InitDialogueEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, class UDialogueBlueprint* InDialogue);

	//~ Begin IToolkit Interface
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual FText GetToolkitToolTipText() const override;
	//~ End IToolkit Interface

	//~ Begin FEditorUndoClient Interface
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
	// End of FEditorUndoClient

		//~ Begin FBlueprintEditor Interface
	virtual void CreateDefaultCommands() override;
	virtual UBlueprint* GetBlueprintObj() const override;

	/** Setup all the events that the graph editor can handle - need to override this as default BP editor
double click jumps to parent node, we need custom double clicked functionality TODO: See if cleaner way to do this*/
	virtual void SetupGraphEditorEvents(UEdGraph* InGraph, SGraphEditor::FGraphEditorEvents& InEvents);

	//Need to override StartEditingDefaults to edit the Dialogue Template inside our generated class
	//virtual void StartEditingDefaults(bool bAutoFocus = true, bool bForceRefresh = false) override;

	// End of FBlueprintEditor

	void CreateDialogueCommandList();

	virtual void PasteNodesHere(class UEdGraph* DestinationGraph, const FVector2D& GraphLocation) override;
	virtual bool CanPasteNodes() const override;

	// Delegates for general graph editor commands
	void Dialogue_SelectAllNodes();
	bool Dialogue_CanSelectAllNodes() const;
	void Dialogue_DeleteSelectedNodes();
	bool Dialogue_CanDeleteNodes() const;
	void Dialogue_DeleteSelectedDuplicatableNodes();
	void Dialogue_CutSelectedNodes();
	bool Dialogue_CanCutNodes() const;
	void Dialogue_CopySelectedNodes();
	bool Dialogue_CanCopyNodes() const;
	void Dialogue_PasteNodes();
	void Dialogue_PasteNodesHere(const FVector2D& Location);
	bool Dialogue_CanPasteNodes() const;
	void Dialogue_DuplicateNodes();
	bool Dialogue_CanDuplicateNodes() const;
	void Dialogue_CreateComment();
	bool Dialogue_CanCreateComment() const;

	bool Dialogue_GetBoundsForSelectedNodes(class FSlateRect& Rect, float Padding);

	void OnWorldChange(UWorld* World, EMapChangeType MapChangeType);

	// Delegates for custom Dialogue graph editor commands
	void ShowDialogueDetails();
	bool CanShowDialogueDetails() const;
	void OpenNarrativeTutorialsInBrowser();
	bool CanOpenNarrativeTutorialsInBrowser() const;
	void QuickAddNode();
	bool CanQuickAddNode() const;

	void OnAddInputPin();
	bool CanAddInputPin() const;
	void OnRemoveInputPin();
	bool CanRemoveInputPin() const;

	FText GetDialogueEditorTitle() const;

	FGraphAppearanceInfo GetDialogueGraphAppearance() const;

	/** Check whether the behavior tree mode can be accessed (i.e whether we have a valid tree to edit) */
	bool CanAccessDialogueEditorMode() const;

	/**
	* Get the localized text to display for the specified mode
	* @param	InMode	The mode to display
	* @return the localized text representation of the mode
	*/
	static FText GetLocalizedMode(FName InMode);

	UDialogueBlueprint* GetDialogueAsset() const;

	/** Spawns the tab with the update graph inside */
	TSharedRef<SWidget> SpawnProperties();

	/** Spawn Dialogue editor tab */
	//TSharedRef<SWidget> SpawnDialogueEditor();

	// @todo This is a hack for now until we reconcile the default toolbar with application modes [duplicated from counterpart in Blueprint Editor]
	void RegisterToolbarTab(const TSharedRef<class FTabManager>& InTabManager);

	/** Restores the behavior tree graph we were editing or creates a new one if none is available */
	void RestoreDialogueGraph();

	void OnDialogueNodeDoubleClicked(UEdGraphNode* Node);

	/** Save the graph state for later editing */
	void SaveEditedObjectState();

	/** Check to see if we can create a new state node */
	bool CanCreateNewState() const;

	/** Check to see if we can create a new action node */
	bool CanCreateNewAction() const;

	/** Create the menu used to make a new state node */
	TSharedRef<SWidget> HandleCreateNewStateMenu() const;

	/** Create the menu used to make a new action node */
	TSharedRef<SWidget> HandleCreateNewActionMenu() const;

	/** Handler for when a node class is picked */
	void HandleNewNodeClassPicked(UClass* InClass) const;

	virtual void OnCreateComment() override;

protected:

	/** The command list for this editor */
	TSharedPtr<FUICommandList> GraphEditorCommands;

	/** Currently focused graph */
	TWeakPtr<SGraphEditor> UpdateGraphEdPtr;

private:

	/** Create widget for graph editing */
	TSharedRef<class SGraphEditor> CreateDialogueGraphEditorWidget(UEdGraph* InGraph);

	/** Creates all internal widgets for the tabs to point at */
	void CreateInternalWidgets();

	/** Add custom menu options */
	void ExtendMenu();
	void ExtendToolbar();


	TWeakPtr<FDocumentTabFactory> GraphEditorTabFactoryPtr;

	/**The Dialogue asset being edited*/
	UDialogueBlueprint* DialogueBlueprint;

	/** Property View */
	TSharedPtr<class IDetailsView> DetailsView;

	uint32 SelectedNodesCount;

	TSharedPtr<class FDialogueEditorToolbar> ToolbarBuilder;


	/** Handle to the registered OnPackageSave delegate */
	FDelegateHandle OnPackageSavedDelegateHandle;

public:

	static const FName DialogueEditorMode;

};
