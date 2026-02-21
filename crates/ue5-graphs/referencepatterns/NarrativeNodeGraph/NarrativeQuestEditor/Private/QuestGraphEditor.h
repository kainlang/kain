// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Misc/NotifyHook.h"
#include "EditorUndoClient.h"
#include "Widgets/SWidget.h"
#include "GraphEditor.h"
#include "QuestBlueprint.h"
#include "IQuestEditor.h"


class FQuestGraphEditor : public IQuestEditor
{
	
public:

	FQuestGraphEditor();
	virtual ~FQuestGraphEditor();

	virtual void OnSelectedNodesChangedImpl(const TSet<class UObject *>& NewSelection) override;

	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) override;

	void InitQuestEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UQuestBlueprint* QuestBP);

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

	//~ Begin FNotifyHook Interface
	//virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged) override;
	// End of FNotifyHook

	//~ Begin FBlueprintEditor Interface
	virtual void CreateDefaultCommands() override;
	virtual UBlueprint* GetBlueprintObj() const override;

	/** Setup all the events that the graph editor can handle - need to override this as default BP editor 
	double click jumps to parent node, we need custom double clicked functionality TODO: See if cleaner way to do this*/
	virtual void SetupGraphEditorEvents(UEdGraph* InGraph, SGraphEditor::FGraphEditorEvents& InEvents);

	// End of FBlueprintEditor

protected:

	virtual void OnCreateComment() override;

	void CreateQuestGraphCommandList();

	virtual bool CanCopyNodes() const;

	virtual void PasteNodesHere(class UEdGraph* Graph, const FVector2D& Location) override;

	//Not all of these are used yet
	void Quest_SelectAllNodes();
	bool Quest_CanSelectAllNodes() const;
	void Quest_DeleteSelectedNodes();
	bool Quest_CanDeleteNodes() const;
	void Quest_DeleteSelectedDuplicatableNodes();
	void Quest_CutSelectedNodes();
	bool Quest_CanCutNodes() const;
	void Quest_CopySelectedNodes();
	bool Quest_CanCopyNodes() const;
	void Quest_PasteNodes();
	void Quest_PasteNodesHere(const FVector2D& Location);
	bool Quest_CanPasteNodes() const;
	void Quest_DuplicateNodes();
	bool Quest_CanDuplicateNodes() const;
	void Quest_CreateComment();
	bool Quest_CanCreateComment() const;


	void OnWorldChange(UWorld* World, EMapChangeType MapChangeType);


public:

	bool Quest_GetBoundsForSelectedNodes(class FSlateRect& Rect, float Padding);

	// Delegates for custom quest graph editor commands
	void ShowQuestDetails();
	bool CanShowQuestDetails() const;
	void OpenNarrativeTutorialsInBrowser();
	bool CanOpenNarrativeTutorialsInBrowser() const;
	void QuickAddNode();
	bool CanQuickAddNode() const;

	FText GetQuestEditorTitle() const;

	FGraphAppearanceInfo GetQuestGraphAppearance() const;

	bool CanAccessQuestEditorMode() const;

	/**
	* Get the localized text to display for the specified mode
	* @param	InMode	The mode to display
	* @return the localized text representation of the mode
	*/
	static FText GetLocalizedMode(FName InMode);

	UQuestBlueprint* GetQuestAsset() const;

	/** Spawns the tab with the update graph inside */
	TSharedRef<SWidget> SpawnProperties();

	/** Restores the behavior tree graph we were editing or creates a new one if none is available */
	void RestoreQuestGraph();
	void OnQuestNodeDoubleClicked(UEdGraphNode* Node);

public:

	/** The command list for this editor */
	TSharedPtr<FUICommandList> GraphEditorCommands;

protected:

	/** Currently focused graph */
	TWeakPtr<SGraphEditor> UpdateGraphEdPtr;

	/** The extender to pass to the level editor to extend it's window menu */
	TSharedPtr<FExtender> MenuExtender;

	/** Toolbar extender */
	TSharedPtr<FExtender> ToolbarExtender;

public:

	/** Create widget for graph editing */
	TSharedRef<class SGraphEditor> CreateQuestGraphEditorWidget(UEdGraph* InGraph);

private:

	/** Creates all internal widgets for the tabs to point at */
	void CreateInternalWidgets();

	/** Add custom menu options */
	void ExtendMenu();

	void ExtendToolbar();

	/**The quest asset being edited*/
	UQuestBlueprint* QuestBlueprint;

	/** Property View */
	TSharedPtr<class IDetailsView> DetailsView;

public:

	static const FName QuestEditorMode;

};
