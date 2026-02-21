/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#pragma once

#include "CoreMinimal.h"
#include "SGraphPanel.h"
#include "WorkflowOrientedApp/WorkflowCentricApplication.h"

class QuestAssetEditorApp : public FWorkflowCentricApplication, public FEditorUndoClient, public FNotifyHook {
public: // FWorkflowCentricApplication interface
    virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	void InitEditor(const EToolkitMode::Type mode, const TSharedPtr<class IToolkitHost>& initToolkitHost, UObject* inObject);

    class UQuestAsset* GetWorkingAsset() { return _workingAsset; }
    class UEdGraph* GetWorkingGraph() { return _workingGraph; }

    SGraphEditor* GetWorkingGraphUi() { return _workingGraphUi.Get(); }
    void SetWorkingGraphUi(TSharedPtr<SGraphEditor> workingGraphUi) { _workingGraphUi = workingGraphUi; }
    void SetSelectedNodeDetailView(TSharedPtr<class IDetailsView> detailsView);
    void OnGraphSelectionChanged(const FGraphPanelSelectionSet& selection);

    bool GetBoundsOfSelection(FSlateRect& Result, float Padding);

public: // FAssetEditorToolkit interface
    virtual FName GetToolkitFName() const override { return FName(TEXT("QuestAssetEditorApp")); }
    virtual FText GetBaseToolkitName() const override { return FText::FromString(TEXT("QuestAssetEditorApp")); }
    virtual FString GetWorldCentricTabPrefix() const override { return TEXT("QuestAssetEditorApp"); }
    virtual FLinearColor GetWorldCentricTabColorScale() const override { return FLinearColor(0.3f, 0.2f, 0.5f, 0.5f); }
	virtual FString GetDocumentationLink() const override { return TEXT("https://discord.gg/8EVbzQSFpb"); }
	virtual void OnToolkitHostingStarted(const TSharedRef<class IToolkit>& Toolkit) override { }
	virtual void OnToolkitHostingFinished(const TSharedRef<class IToolkit>& Toolkit) override { }

    virtual void OnClose() override;
    void OnNodeDetailViewPropertiesUpdated(const FPropertyChangedEvent& event);
    void OnWorkingAssetPreSave();

protected:
    void UpdateWorkingAssetFromGraph();
    void UpdateEditorGraphFromWorkingAsset();
    class UQuestGraphNodeBase* GetSelectedNode(const FGraphPanelSelectionSet& selection);
    TArray<class UQuestGraphNodeBase*> GetAllSelectedNodes(const FGraphPanelSelectionSet& selection);

private:
    UPROPERTY()
    class UQuestAsset* _workingAsset = nullptr;

    UPROPERTY() // Working graph model
    class UEdGraph* _workingGraph = nullptr;

    // Working graph UI (the slate widget that displays the graph)
    TSharedPtr<SGraphEditor> _workingGraphUi = nullptr;

    TSharedPtr<class IDetailsView> _selectedNodeDetailView = nullptr;

    UPROPERTY()
    TArray<class UQuestGraphNodeBase*> CurrentSelection;
};
