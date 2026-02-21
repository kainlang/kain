/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#include "DialogAssetPrimaryTabFactory.h"
#include "DialogAssetEditorApp.h"
#include "DialogAsset.h"
#include "DialogAssetEditorSettings.h"
#include "DialogGraphNode.h"
#include "DialogStartGraphNode.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "GraphEditor.h"
#include "Framework/Commands/GenericCommands.h"
#include "Windows/WindowsPlatformApplicationMisc.h"

DialogAssetPrimaryTabFactory::DialogAssetPrimaryTabFactory(TSharedPtr<DialogAssetEditorApp> app) : FWorkflowTabFactory(FName("DialogAssetPrimaryTab"), app) {
    _app = app;

    TabLabel = FText::FromString(TEXT("Dialogue Graph"));

	ViewMenuDescription = FText::FromString(TEXT("The Dialogue Graph to edit the Dialogue."));
	ViewMenuTooltip = FText::FromString(TEXT("Show the Dialogue Graph."));

    Initialize();
}

void DialogAssetPrimaryTabFactory::Initialize()
{
    CreateEditorCommands();
}

TSharedRef<SWidget> DialogAssetPrimaryTabFactory::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const {
    TSharedPtr<DialogAssetEditorApp> app = _app.Pin();

    SGraphEditor::FGraphEditorEvents graphEvents;
    graphEvents.OnSelectionChanged.BindRaw(app.Get(), &DialogAssetEditorApp::OnGraphSelectionChanged);
    
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

FText DialogAssetPrimaryTabFactory::GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const {
    return FText::FromString(TEXT("A Dialogue Graph to edit the Dialogue."));
}

void DialogAssetPrimaryTabFactory::CreateEditorCommands()
{
    if(!_bEditorCommandsCreated)
    {
        GraphEditorCommands = MakeShareable(new FUICommandList);

        GraphEditorCommands->MapAction(
            FGenericCommands::Get().Delete,
            FExecuteAction::CreateRaw(this, &DialogAssetPrimaryTabFactory::DeleteSelectedNodes),
            FCanExecuteAction::CreateRaw(this, &DialogAssetPrimaryTabFactory::CanDeleteNodes));

        GraphEditorCommands->MapAction(
            FGenericCommands::Get().Paste,
            FExecuteAction::CreateRaw(this, &DialogAssetPrimaryTabFactory::PasteDialogueText),
            FCanExecuteAction::CreateRaw(this, &DialogAssetPrimaryTabFactory::CanPasteDialogueText));

        _bEditorCommandsCreated = true;
    }
}

void DialogAssetPrimaryTabFactory::DeleteSelectedNodes()
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
                if(Node->IsA<UDialogStartGraphNode>()) continue; // Delete the start node is not authorized !

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

bool DialogAssetPrimaryTabFactory::CanDeleteNodes()
{
    return true;
}


// If user paste text, create dialogue nodes ...
void DialogAssetPrimaryTabFactory::PasteDialogueText()
{
    SGraphEditor * GraphEditor = _app.Pin().Get()->GetWorkingGraphUi();
    UEdGraph * graph = _app.Pin().Get()->GetWorkingGraph();
    UDialogAsset * asset = _app.Pin().Get()->GetWorkingAsset();

    if(GraphEditor == nullptr || graph == nullptr || asset == nullptr) return;
    
    FString ClipboardContent;
    FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);

    TArray<FDialogueLine> lines = ParseDialogue(ClipboardContent);

    ELineDurationType durationType = ELineDurationType::DEFAULT;
    if (const UDialogAssetEditorSettings* DialogueSettings = GetDefault<UDialogAssetEditorSettings>())
    {
        durationType = DialogueSettings->DefaultDurationType;
    }

    FVector2d pos = GraphEditor->GetPasteLocation();
    int nodePosX = pos.X;
    int nodePosY = pos.Y;

    TSet<UEdGraphNode*> CreatedNodes;
    UEdGraphPin * lastOutPin = nullptr;
    for(FDialogueLine line: lines)
    {
        // Verify if actor exists in the asset:
        FText SpeakerName = FText::FromString(line.Speaker);
        int actorIdx = asset->getActorIdxFromActorName(SpeakerName);
        FActorInfo *actorInfo = asset->getActorInfoFromIndex(actorIdx);

        // Actor not found in the asset, add it automatically as a dialog actor...
        if(actorIdx == -1 || actorInfo == nullptr)
        {
            FActorInfo newActorInfo;
            newActorInfo.ActorIdentifier = SpeakerName;
            newActorInfo.UniqueIdentifierOverride = FText::AsCultureInvariant(SpeakerName);
            asset->DialogActors.Add(newActorInfo);
            actorIdx = asset->DialogActors.Num();
            actorInfo = asset->getActorInfoFromIndex(actorIdx);
        }

        
        if(actorIdx >= 0 && actorInfo != nullptr)
        {
            UDialogGraphNode* node = NewObject<UDialogGraphNode>(graph);
            node->CreateNewGuid();
            node->NodePosX = nodePosX;
            node->NodePosY = nodePosY;
            node->InitNodeInfo(node);

            UDialogNodeInfo * info = Cast<UDialogNodeInfo>(node->GetNodeInfo());
            info->Replique.SpokenText = FText::FromString(line.Text);
        
            // Initialize the actor Idx
            info->SetActorIdx(actorIdx, actorInfo->ActorIdentifier);
            info->Replique.DurationType = durationType;

            node->UpdateIDToBeUnique();
            UEdGraphPin * inPin = node->CreateDefaultInputPin();

            node->GetDialogNodeInfo()->DialogResponses.Add(FText::FromString(TEXT("")));
            node->SyncPinsWithResponses();

            UEdGraphPin * outPin = node->GetPinAt(1);   // Pin 1 is the second pin (first output).

            // If it is not the first node we create, make connection between last node and current node.
            if(lastOutPin != nullptr)
            {
                node->GetSchema()->TryCreateConnection(lastOutPin, inPin);
            }
        
            lastOutPin = outPin; 

            nodePosX += 500;

            graph->Modify();
            graph->AddNode(node, true, true);
            CreatedNodes.Add(node);
        }
    }

    //graph->SelectNodeSet(CreatedNodes);
    // Notifiez les changements pour mettre à jour le graph
    GraphEditor->GetCurrentGraph()->NotifyGraphChanged();
    GraphEditor->ClearSelectionSet();

    for(UEdGraphNode * node : CreatedNodes)
        GraphEditor->SetNodeSelection(node, true);

    GraphEditor->GetCurrentGraph()->NotifyGraphChanged();
}

bool DialogAssetPrimaryTabFactory::CanPasteDialogueText()
{
    FString ClipboardContent;
    FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);

    TArray<FDialogueLine> lines = ParseDialogue(ClipboardContent);
    
    return lines.Num() > 0;
}

TArray<FDialogueLine> DialogAssetPrimaryTabFactory::ParseDialogue(const FString& InputText)
{
    TArray<FDialogueLine> Result;
    TArray<FString> Lines;
    InputText.ParseIntoArrayLines(Lines);
    
    for (const FString& Line : Lines)
    {
        FString Speaker;
        FString Dialogue;

        if (Line.Split(TEXT(":"), &Speaker, &Dialogue))
        {
            Speaker = Speaker.TrimStartAndEnd();
            Dialogue = Dialogue.TrimStartAndEnd();

            FDialogueLine NewLine;
            NewLine.Speaker = Speaker;
            NewLine.Text = Dialogue;

            Result.Add(NewLine);
        }
    }

    return Result;
}
