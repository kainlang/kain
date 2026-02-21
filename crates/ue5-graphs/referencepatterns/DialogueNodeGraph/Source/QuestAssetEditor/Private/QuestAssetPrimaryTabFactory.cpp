/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#include "QuestAssetPrimaryTabFactory.h"
#include "QuestAssetEditorApp.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "GraphEditor.h"
#include "QuestStartGraphNode.h"
#include "Editor/UnrealEd/Public/Kismet2/BlueprintEditorUtils.h"
#include "Framework/Commands/GenericCommands.h"
#include "Kismet2/KismetEditorUtilities.h"

QuestAssetPrimaryTabFactory::QuestAssetPrimaryTabFactory(TSharedPtr<QuestAssetEditorApp> app) : FWorkflowTabFactory(FName("QuestAssetPrimaryTab"), app) {
    _app = app;

    TabLabel = FText::FromString(TEXT("Quest Graph"));

	ViewMenuDescription = FText::FromString(TEXT("The Quest Graph to edit the Quest."));
	ViewMenuTooltip = FText::FromString(TEXT("Show the Quest Graph."));

    Initialize();
}

void QuestAssetPrimaryTabFactory::Initialize()
{
    CreateEditorCommands();
}

TSharedRef<SWidget> QuestAssetPrimaryTabFactory::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const {
    TSharedPtr<QuestAssetEditorApp> app = _app.Pin();

    SGraphEditor::FGraphEditorEvents graphEvents;
    graphEvents.OnSelectionChanged.BindRaw(app.Get(), &QuestAssetEditorApp::OnGraphSelectionChanged);
    
    TSharedPtr<SGraphEditor> graphEditor = 
        SNew(SGraphEditor)
            .IsEditable(true)
            .AdditionalCommands(GraphEditorCommands)
            .GraphEvents(graphEvents)
            .GraphToEdit(app->GetWorkingGraph());
    app->SetWorkingGraphUi(graphEditor);

    return SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .FillHeight(1.0f)
                .HAlign(HAlign_Fill)
                [
                    graphEditor.ToSharedRef()
                ];
}

FText QuestAssetPrimaryTabFactory::GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const {
    return FText::FromString(TEXT("A Quest Graph to edit the Quest."));
}

void QuestAssetPrimaryTabFactory::CreateEditorCommands()
{
    if(!_bEditorCommandsCreated)
    {
        GraphEditorCommands = MakeShareable(new FUICommandList);
        if(GraphEditorCommands.IsValid())
        {
            GraphEditorCommands->MapAction(
                FGenericCommands::Get().Delete,
                FExecuteAction::CreateRaw(this, &QuestAssetPrimaryTabFactory::DeleteSelectedNodes),
                FCanExecuteAction::CreateRaw(this, &QuestAssetPrimaryTabFactory::CanDeleteNodes));

            _bEditorCommandsCreated = true;
        }
    }
}

void QuestAssetPrimaryTabFactory::DeleteSelectedNodes()
{
    SGraphEditor * GraphEditor = _app.Pin().Get()->GetWorkingGraphUi();

    if(GraphEditor == nullptr) return;

    const FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();

    if (SelectedNodes.Num() > 0)
    {
        for (UObject* NodeObject : SelectedNodes)
        {
            if (UEdGraphNode* Node = Cast<UEdGraphNode>(NodeObject))
            {
                if(Node->IsA<UQuestStartGraphNode>()) continue; // Delete the start node is not authorized !
                
                // Autorise l'annulation
                Node->Modify();

                // Supprime le nœud
                Node->DestroyNode();
            }
        }

        // Notifiez les changements pour mettre à jour le graph
        GraphEditor->GetCurrentGraph()->NotifyGraphChanged();
    }
}

bool QuestAssetPrimaryTabFactory::CanDeleteNodes()
{
    return true;
}
