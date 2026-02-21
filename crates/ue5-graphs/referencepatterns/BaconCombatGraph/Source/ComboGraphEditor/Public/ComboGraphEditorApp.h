// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorkflowOrientedApp/WorkflowCentricApplication.h" 

class FTabManager;
class IToolkitHost;
class IToolkit;
class IDetailsView;
class UComboManagerComponent;
class UComboGraphAsset;
class UComboGraph;
class UComboNodeBase;
class UComboGraphNodeData;
class SComboGraphDebugSelector;

class COMBOGRAPHEDITOR_API FComboGraphEditorApp : public FWorkflowCentricApplication, public FEditorUndoClient, public FNotifyHook
{
public:
	FComboGraphEditorApp();
	virtual ~FComboGraphEditorApp();

	// FEditorUndoClient
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
	
// FWorkflowCentricApplication interface
public:
    virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;
    void InitEditor(const EToolkitMode::Type mode, const TSharedPtr<IToolkitHost>& initToolkitHost, UObject* inObject);

    UComboGraphAsset* GetWorkingAsset() const { return WorkingAsset.Get(); }
	UComboGraph* GetWorkingGraph() const { return WorkingGraph.Get(); }

// FAssetEditorToolkit interface
public: 
    virtual FName GetToolkitFName() const override;
    virtual FText GetBaseToolkitName() const override;
    virtual FString GetWorldCentricTabPrefix() const override;
    virtual FLinearColor GetWorldCentricTabColorScale() const override;
    virtual FString GetDocumentationLink() const override;
    virtual void OnToolkitHostingStarted(const TSharedRef<IToolkit>& Toolkit) override { }
    virtual void OnToolkitHostingFinished(const TSharedRef<IToolkit>& Toolkit) override { }
    virtual void OnClose() override;

public:
    void CreateGraphEditorWidget();
    TSharedPtr<SGraphEditor> GetGraphEditorWidget() const { return GraphEditorWidget; }

// Selected node details
public:
    void SetSelectedNodeDetailView(TSharedPtr<IDetailsView> DetailsView);

protected:
    virtual void OnSelectedNodesChanged(const FGraphPanelSelectionSet& NewSelection);
    void OnNodeDetailViewPropertiesUpdated(const FPropertyChangedEvent& Event);
    UComboNodeBase* GetSelectedNode(const FGraphPanelSelectionSet& Selection);

// Tool bar
protected:
    void ExtendToolBar();
    virtual void FillToolbar(FToolBarBuilder& ToolbarBuilder, bool bEndSection = true);

	void OnSelectionChanged(TWeakObjectPtr<UComboManagerComponent> NewSelection);
    void OnComboNodeTriggered(const UComboGraphNodeData* NodeData);

// Commands
protected:
	void CreateCommandList();

	void SelectAllNodes();
	bool CanSelectAllNodes() const; 

    void DeleteSelectedNodes();
    bool CanDeleteSelectedNodes();

	void CopySelectedNodes();
	bool CanCopySelectedNodes() const;
	
	void PasteNodes();
	void PasteNodesHere(const FVector2D& Location);
	bool CanPasteNodes() const;

    void DuplicateSelectedNodes();
    bool CanDuplicateSelectedNodes() const;

// PIE
protected:
	void OnPIEStarted(bool bIsSimulating);
	void OnPIEStopped(bool bIsSimulating);
// Loading / Saving functions 
protected:
    void OnAssetPreSave(UComboGraphAsset* Asset);

	bool SaveWorkingGraphToWorkingAsset();
	bool LoadWorkingAssetToWorkingGraph();

protected:
    TWeakObjectPtr<UComboNodeBase> CurrentRunningNode;
    TWeakObjectPtr<UComboManagerComponent> DebugTarget;
    TSharedPtr<SComboGraphDebugSelector> DebugSelector;

	TSharedPtr<SGraphEditor> GraphEditorWidget = nullptr;
    TSharedPtr<FUICommandList> GraphEditorCommands = nullptr;
    TSharedPtr<IDetailsView> SelectedNodeDetailView = nullptr;
    TSharedPtr<FExtender> ToolbarExtender = nullptr;

	// Working asset and graph
    TWeakObjectPtr<UComboGraphAsset> WorkingAsset = nullptr;
	TWeakObjectPtr<UComboGraph> WorkingGraph = nullptr;
};