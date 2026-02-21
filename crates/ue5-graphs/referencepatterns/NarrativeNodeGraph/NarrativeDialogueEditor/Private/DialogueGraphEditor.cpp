// Copyright Narrative Tools 2022. 

#include "DialogueGraphEditor.h"
#include "NarrativeDialogueEditorModule.h"
#include "ScopedTransaction.h"
#include "DialogueEditorTabFactories.h"
#include "GraphEditorActions.h"
#include "Framework/Commands/GenericCommands.h"
#include "DialogueGraphNode.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "EdGraphUtilities.h"
#include "DialogueGraph.h"
#include "DialogueGraphSchema.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "DialogueEditorModes.h"
#include "Widgets/Docking/SDockTab.h"
#include "DialogueEditorToolbar.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "DialogueSM.h"
#include "DialogueAsset.h"
#include "DialogueBlueprint.h"
#include "DialogueGraphNode.h"
#include "Dialogue.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h" 
#include "DialogueEditorCommands.h"
#include "DialogueEditorSettings.h"
#include <SBlueprintEditorToolbar.h>
#include <Kismet2/DebuggerCommands.h>
#include "SKismetInspector.h"
#include "EdGraphSchema_K2_Actions.h"
#include "K2Node_CustomEvent.h"
#include "DialogueGraphNode_NPC.h"
#include "DialogueGraphNode_Player.h"
#include <Engine/World.h>
#include "DialogueNodeUserWidget.h"
#include "LevelEditor.h"
#include "NarrativeDialogueSettings.h"

#define LOCTEXT_NAMESPACE "DialogueAssetEditor"

const FName FDialogueGraphEditor::DialogueEditorMode(TEXT("DialogueEditor"));

//TODO change this to the documentation page after documentation is ready for use
static const FString NarrativeHelpURL("http://www.google.com");

FDialogueGraphEditor::FDialogueGraphEditor()
{
	//UEditorEngine* Editor = (UEditorEngine*)GEngine;
	//if (Editor != NULL)
	//{
	//	Editor->RegisterForUndo(this);
	//}
}

FDialogueGraphEditor::~FDialogueGraphEditor()
{
	//UEditorEngine* Editor = (UEditorEngine*)GEngine;
	//if (Editor)
	//{
	//	Editor->UnregisterForUndo(this);
	//}
}


void FDialogueGraphEditor::OnSelectedNodesChangedImpl(const TSet<class UObject*>& NewSelection)
{
	if (NewSelection.Num() == 1)
	{
		for (auto& Obj : NewSelection)
		{
			//Want to edit the underlying dialogue object, not the graph node
			if (UDialogueGraphNode* GraphNode = Cast<UDialogueGraphNode>(Obj))
			{
				TSet<class UObject*> ModifiedSelection;
				ModifiedSelection.Add(GraphNode->DialogueNode);
				FBlueprintEditor::OnSelectedNodesChangedImpl(ModifiedSelection);
				return;
			}
		}
	}


	FBlueprintEditor::OnSelectedNodesChangedImpl(NewSelection);
}
//
//void FDialogueGraphEditor::OnSelectedNodesChanged(const TSet<class UObject*>& NewSelection)
//{
//	SelectedNodesCount = NewSelection.Num();
//
//	if (SelectedNodesCount == 0)
//	{
//		DetailsView->SetObject(DialogueBlueprint->Dialogue);
//		return;
//	}
//
//	UDialogueGraphNode* SelectedNode = nullptr;
//
//	for (UObject* Selection : NewSelection)
//	{
//		if (UDialogueGraphNode* Node = Cast<UDialogueGraphNode>(Selection))
//		{
//			SelectedNode = Node;
//			break;
//		}
//	}
//
//	if (UDialogueGraph* MyGraph = Cast<UDialogueGraph>(DialogueBlueprint->DialogueGraph))
//	{
//		if (DetailsView.IsValid())
//		{
//			if (SelectedNode)
//			{
//				//Edit the underlying graph node object 
//				if (UDialogueGraphNode* Node = Cast<UDialogueGraphNode>(SelectedNode))
//				{
//					DetailsView->SetObject(Node->DialogueNode);
//				}
//			}
//		}
//		else
//		{
//			DetailsView->SetObject(nullptr);
//		}
//	}
//}

void FDialogueGraphEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	DocumentManager->SetTabManager(InTabManager);

	FWorkflowCentricApplication::RegisterTabSpawners(InTabManager);
}

void FDialogueGraphEditor::OnWorldChange(UWorld* World, EMapChangeType MapChangeType)
{
	//Dialogue graph nodes will be referencing the UWorld, and if it changes this will breakcc
	if (World)
	{
		for (TObjectIterator<UUserWidget> Itr; Itr; ++Itr)
		{
			UUserWidget* Widget = *Itr;

			if (Widget->IsA<UDialogueNodeUserWidget>())
			{
				Widget->Rename(nullptr, GetTransientPackage());
			}
		}
	}
}

void FDialogueGraphEditor::InitDialogueEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UDialogueBlueprint* InDialogue)
{
	DialogueBlueprint = InDialogue; 

	if (!Toolbar.IsValid())
	{
		Toolbar = MakeShareable(new FBlueprintEditorToolbar(SharedThis(this)));
	}

	GetToolkitCommands()->Append(FPlayWorldCommands::GlobalPlayWorldActions.ToSharedRef());

	CreateDefaultCommands();
	//BindCommands();
	RegisterMenus();

	CreateInternalWidgets();

	TSharedPtr<FDialogueGraphEditor> ThisPtr(SharedThis(this));

	TArray<UObject*> ObjectsToEdit;
	ObjectsToEdit.Add(DialogueBlueprint);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	InitAssetEditor(Mode, InitToolkitHost, FNarrativeDialogueEditorModule::DialogueEditorAppId, FTabManager::FLayout::NullLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, ObjectsToEdit);

	TArray<UBlueprint*> EditedBlueprints;
	EditedBlueprints.Add(DialogueBlueprint);

	CommonInitialization(EditedBlueprints, false);

	AddApplicationMode(DialogueEditorMode, MakeShareable(new FDialogueEditorApplicationMode(SharedThis(this))));

	ExtendMenu();
	ExtendToolbar();
	RegenerateMenusAndToolbars();

	SetCurrentMode(DialogueEditorMode);

	PostLayoutBlueprintEditorInitialization();

	FLevelEditorModule& LevelEditor = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditor.OnMapChanged().AddRaw(this, &FDialogueGraphEditor::OnWorldChange);
}

FName FDialogueGraphEditor::GetToolkitFName() const
{
	return FName("Dialogue Editor");
}

FText FDialogueGraphEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "DialogueEditor");
}

FString FDialogueGraphEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "DialogueEditor").ToString();
}

FLinearColor FDialogueGraphEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.0f, 0.0f, 0.2f, 0.5f);
}

FText FDialogueGraphEditor::GetToolkitToolTipText() const
{
	if (DialogueBlueprint)
	{
		return FAssetEditorToolkit::GetToolTipTextForObject(DialogueBlueprint);
	}
	return FText();
}

void FDialogueGraphEditor::CreateDialogueCommandList()
{
	//if (GraphEditorCommands.IsValid())
	//{
	//	GraphEditorCommands->MapAction(FGenericCommands::Get().SelectAll,
	//		FExecuteAction::CreateRaw(this, &FDialogueGraphEditor::Dialogue_SelectAllNodes),
	//		FCanExecuteAction::CreateRaw(this, &FDialogueGraphEditor::Dialogue_CanSelectAllNodes)
	//	);

	//	GraphEditorCommands->MapAction(FGenericCommands::Get().Delete,
	//		FExecuteAction::CreateRaw(this, &FDialogueGraphEditor::Dialogue_DeleteSelectedNodes),
	//		FCanExecuteAction::CreateRaw(this, &FDialogueGraphEditor::Dialogue_CanDeleteNodes)
	//	);

	//	GraphEditorCommands->MapAction(FGenericCommands::Get().Copy,
	//		FExecuteAction::CreateRaw(this, &FDialogueGraphEditor::Dialogue_CopySelectedNodes),
	//		FCanExecuteAction::CreateRaw(this, &FDialogueGraphEditor::Dialogue_CanCopyNodes)
	//	);

	//	GraphEditorCommands->MapAction(FGenericCommands::Get().Cut,
	//		FExecuteAction::CreateRaw(this, &FDialogueGraphEditor::Dialogue_CutSelectedNodes),
	//		FCanExecuteAction::CreateRaw(this, &FDialogueGraphEditor::Dialogue_CanCutNodes)
	//	);

	//	GraphEditorCommands->MapAction(FGenericCommands::Get().Paste,
	//		FExecuteAction::CreateRaw(this, &FDialogueGraphEditor::Dialogue_PasteNodes),
	//		FCanExecuteAction::CreateRaw(this, &FDialogueGraphEditor::Dialogue_CanPasteNodes)
	//	);

	//	GraphEditorCommands->MapAction(FGenericCommands::Get().Duplicate,
	//		FExecuteAction::CreateRaw(this, &FDialogueGraphEditor::Dialogue_DuplicateNodes),
	//		FCanExecuteAction::CreateRaw(this, &FDialogueGraphEditor::Dialogue_CanDuplicateNodes)
	//	);
	//}
}


void FDialogueGraphEditor::PasteNodesHere(class UEdGraph* DestinationGraph, const FVector2D& GraphLocation)
{

	if (UDialogueGraph* DGraph = Cast<UDialogueGraph>(DestinationGraph))
	{
		Dialogue_PasteNodesHere(GraphLocation);
	}
	else
	{
		FBlueprintEditor::PasteNodesHere(DestinationGraph, GraphLocation);
	}
}

bool FDialogueGraphEditor::CanPasteNodes() const
{
	return true;
}

void FDialogueGraphEditor::Dialogue_SelectAllNodes()
{
	if (TSharedPtr<SGraphEditor> CurrentGraphEditor = UpdateGraphEdPtr.Pin())
	{
		CurrentGraphEditor->SelectAllNodes();
	}
}

bool FDialogueGraphEditor::Dialogue_CanSelectAllNodes() const
{
	return true;
}

void FDialogueGraphEditor::Dialogue_DeleteSelectedNodes()
{
	TSharedPtr<SGraphEditor> CurrentGraphEditor = UpdateGraphEdPtr.Pin();
	if (!CurrentGraphEditor.IsValid())
	{
		return;
	}

	const FScopedTransaction Transaction(FGenericCommands::Get().Delete->GetDescription());
	CurrentGraphEditor->GetCurrentGraph()->Modify();

	const FGraphPanelSelectionSet SelectedNodes = CurrentGraphEditor->GetSelectedNodes();
	CurrentGraphEditor->ClearSelectionSet();

	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(*NodeIt))
		{
			if (Node->CanUserDeleteNode())
			{
				Node->Modify();
				Node->DestroyNode();
			}
		}
	}
}

bool FDialogueGraphEditor::Dialogue_CanDeleteNodes() const
{
	// If any of the nodes can be deleted then we should allow deleting
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
		if (Node && Node->CanUserDeleteNode())
		{
			return true;
		}
	}

	return false;
}

void FDialogueGraphEditor::Dialogue_DeleteSelectedDuplicatableNodes()
{
	TSharedPtr<SGraphEditor> CurrentGraphEditor = UpdateGraphEdPtr.Pin();
	if (!CurrentGraphEditor.IsValid())
	{
		return;
	}
	const FGraphPanelSelectionSet OldSelectedNodes = CurrentGraphEditor->GetSelectedNodes();
	CurrentGraphEditor->ClearSelectionSet();

	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(OldSelectedNodes); SelectedIter; ++SelectedIter)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
		if (Node && Node->CanDuplicateNode())
		{
			CurrentGraphEditor->SetNodeSelection(Node, true);
		}
	}

	// Delete the duplicatable nodes
	DeleteSelectedNodes();

	CurrentGraphEditor->ClearSelectionSet();

	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(OldSelectedNodes); SelectedIter; ++SelectedIter)
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter))
		{
			CurrentGraphEditor->SetNodeSelection(Node, true);
		}
	}
}

void FDialogueGraphEditor::Dialogue_CutSelectedNodes()
{
	CopySelectedNodes();
	DeleteSelectedDuplicatableNodes();
}

bool FDialogueGraphEditor::Dialogue_CanCutNodes() const
{
	return CanCopyNodes() && CanDeleteNodes();
}

void FDialogueGraphEditor::Dialogue_CopySelectedNodes()
{
	// Export the selected nodes and place the text on the clipboard
	FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

	FString ExportedText;

	for (FGraphPanelSelectionSet::TIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
		UDialogueGraphNode* DialogueNode = Cast<UDialogueGraphNode>(Node);
		if (Node == nullptr)
		{
			SelectedIter.RemoveCurrent();
			continue;
		}

		Node->PrepareForCopying();
	}

	FEdGraphUtilities::ExportNodesToText(SelectedNodes, ExportedText);
	FPlatformApplicationMisc::ClipboardCopy(*ExportedText);

}

bool FDialogueGraphEditor::Dialogue_CanCopyNodes() const
{
	// If any of the nodes can be duplicated then we should allow copying
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
		if (Node && Node->CanDuplicateNode())
		{
			return true;
		}
	}

	return false;
}

void FDialogueGraphEditor::Dialogue_PasteNodes()
{

	if (TSharedPtr<SGraphEditor> CurrentGraphEditor = UpdateGraphEdPtr.Pin())
	{
		Dialogue_PasteNodesHere(CurrentGraphEditor->GetPasteLocation());
	}
}

void FDialogueGraphEditor::Dialogue_PasteNodesHere(const FVector2D& Location)
{
	//Pasting nodes is disabled as still causing issues in N3 
	// return;

	TSharedPtr<SGraphEditor> CurrentGraphEditor = FocusedGraphEdPtr.Pin();
	if (!CurrentGraphEditor.IsValid())
	{
		return;
	}

	// Undo/Redo support
	const FScopedTransaction Transaction(FGenericCommands::Get().Paste->GetDescription());
	UDialogueGraph* DialogueGraph = Cast<UDialogueGraph>(CurrentGraphEditor->GetCurrentGraph());

	DialogueGraph->Modify();

	UDialogueGraphNode* SelectedParent = NULL;
	bool bHasMultipleNodesSelected = false;

	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

	// Clear the selection set (newly pasted stuff will be selected)
	CurrentGraphEditor->ClearSelectionSet();

	// Grab the text to paste from the clipboard.
	FString TextToImport;
	FPlatformApplicationMisc::ClipboardPaste(TextToImport);

	// Disabled Node copy + paste and keeping text based until further testing can happen
	// Import the nodes
	// TSet<UEdGraphNode*> PastedNodes;
	// FEdGraphUtilities::ImportNodesFromText(DialogueGraph, TextToImport, /*out*/ PastedNodes);

	// if (PastedNodes.Num())
	// {
	// 	for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
	// 	{
	// 		UEdGraphNode* PasteNode = *It;
	// 		UDialogueGraphNode* PasteDialogueNode = Cast<UDialogueGraphNode>(PasteNode);
	//
	//
	// 		if (PasteNode && PasteDialogueNode)
	// 		{
	// 			// Select the newly pasted stuff
	// 			CurrentGraphEditor->SetNodeSelection(PasteNode, true);
	//
	// 			PasteNode->NodePosX += 400.f;
	// 			PasteNode->NodePosY += 400.f;
	//
	// 			PasteNode->SnapToGrid(16);
	//
	// 			// Give new node a different Guid from the old one
	// 			PasteNode->CreateNewGuid();
	//
	// 			//New dialogue graph node will point to old dialouenode, duplicate a new one for our new node
	// 			UDialogueNode* DupNode = Cast<UDialogueNode>(StaticDuplicateObject(PasteDialogueNode->DialogueNode, PasteDialogueNode->DialogueNode->GetOuter()));
	//
	// 			//StaticDuplicateObject won't have assigned a unique ID, grab a unique one
	// 			DupNode->EnsureUniqueID();
	//
	// 			PasteDialogueNode->DialogueNode = DupNode;
	// 		}
	// 	}
	//
	// 	//Now that everything has been pasted, iterate a second time to rebuild the new nodes connections 
	// 	for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
	// 	{
	// 		UEdGraphNode* PasteNode = *It;
	// 		UDialogueGraphNode* PasteDialogueNode = Cast<UDialogueGraphNode>(PasteNode);
	//
	// 		//Dialogue nodes connections will still be outdated, update these to the new connections
	// 		DialogueGraph->NodeAdded(PasteDialogueNode);
	// 		DialogueGraph->PinRewired(PasteDialogueNode, PasteDialogueNode->GetOutputPin());
	// 	}
	//
	// }
	// else
	// {
		/*
		We may be trying to paste from narrative dialogue markup.
		
		Format is:

		Gavin: Hey dude!
		Erno: Hey, how are you?
		Player: What are you guys doing here?

		Go through line by line and add these nodes in! 
		*/
		TArray<FString> PastedDialogueLines;
		TextToImport.ParseIntoArrayLines(PastedDialogueLines);

		DialogueGraph->Modify();

		FVector2D PasteLoc = Location;
		UDialogueGraphNode* LastNode = nullptr;

		UBlueprint* DialogueBP = GetBlueprintObj();
		UDialogue* Dialogue = nullptr;

		if (DialogueBP)
		{
			Dialogue = Cast<UDialogue>(DialogueBP->GeneratedClass->GetDefaultObject());
		}

		if (!Dialogue)
		{
			return;
		}

		if (const UDialogueGraphSchema* Schema = Cast<UDialogueGraphSchema>(DialogueGraph->GetSchema()))
		{
			for (auto& DialogueLine : PastedDialogueLines)
			{
				//on hit and run wiki any lines without colons wont be character lines
				if (!DialogueLine.Contains(":"))
				{
					continue;
				}

				int32 ColonIdx = -1;
				DialogueLine.FindChar(':', ColonIdx);

				FString SpeakerID = DialogueLine.LeftChop(DialogueLine.Len() - ColonIdx);
				SpeakerID.RemoveSpacesInline();

				FString LineText = DialogueLine.RightChop(ColonIdx);

				LineText.RemoveFromStart(":");
				
				FDialogueSchemaAction_NewNode AddNewNode;
				UDialogueGraphNode* Node;
				UClass* DialogueNodeClass = SpeakerID.Equals("Player", ESearchCase::IgnoreCase) ? UDialogueGraphNode_Player::StaticClass() : UDialogueGraphNode_NPC::StaticClass();

				//Dont do anything if we're already linked somewhere
				Node = NewObject<UDialogueGraphNode>(DialogueGraph, DialogueNodeClass);

				AddNewNode.NodeTemplate = Node;

				if (!LastNode)
				{
					AddNewNode.PerformAction(DialogueGraph, nullptr, PasteLoc);
				}
				else
				{
					AddNewNode.PerformAction(DialogueGraph, LastNode->GetOutputPin(), PasteLoc);
				}

				if (ColonIdx > -1)
				{
					if (UDialogueNode_NPC* NPCNode = Cast<UDialogueNode_NPC>(Node->DialogueNode))
					{
						if (Dialogue)
						{
							//If the speaker doesn't exist add it automatically
							bool bSpeakerFound = false;

							for (auto& Speaker : Dialogue->Speakers)
							{
								if (Speaker.SpeakerID.ToString().Equals(SpeakerID, ESearchCase::IgnoreCase))
								{
									bSpeakerFound = true;
								}
							}

							if (!bSpeakerFound)
							{
								FSpeakerInfo NewSpeaker;
								NewSpeaker.SpeakerID = FName(SpeakerID);
								Dialogue->Speakers.Add(NewSpeaker);
							}
						}

						NPCNode->SetSpeakerID(FName(SpeakerID));
					}
				}

				Node->DialogueNode->Line.Text = FText::FromString(LineText);
				Node->DialogueNode->GenerateIDFromText(); // This doesn't seem to get called automatically so call manually 

				// Update UI
				CurrentGraphEditor->NotifyGraphChanged();

				UObject* GraphOwner = DialogueGraph->GetOuter();
				if (GraphOwner)
				{
					GraphOwner->PostEditChange();
					GraphOwner->MarkPackageDirty();
				}

				if (const UNarrativeDialogueSettings* DialogueSettings = GetDefault<UNarrativeDialogueSettings>())
				{
					if (DialogueSettings->bEnableVerticalWiring)
					{
						PasteLoc += FVector2D(0.f, 250.f);
					}
					else
					{
						PasteLoc += FVector2D(550.f, 0.f);
					}
				}

				LastNode = Node;
			}
		}
	// } // else end bracket

	// Update UI
	CurrentGraphEditor->NotifyGraphChanged();

	UObject* GraphOwner = DialogueGraph->GetOuter();
	if (GraphOwner)
	{
		GraphOwner->PostEditChange();
		GraphOwner->MarkPackageDirty();
	}
}

bool FDialogueGraphEditor::Dialogue_CanPasteNodes() const
{
	TSharedPtr<SGraphEditor> CurrentGraphEditor = UpdateGraphEdPtr.Pin();
	if (!CurrentGraphEditor.IsValid())
	{
		return false;
	}

	FString ClipboardContent;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);

	return true;
	//return FEdGraphUtilities::CanImportNodesFromText(CurrentGraphEditor->GetCurrentGraph(), ClipboardContent);
}

void FDialogueGraphEditor::Dialogue_DuplicateNodes()
{
	CopySelectedNodes();
	PasteNodes();
}

bool FDialogueGraphEditor::Dialogue_CanDuplicateNodes() const
{
	//Duplicating nodes is disabled for now
	return false;

	//return CanCopyNodes();
}

bool FDialogueGraphEditor::Dialogue_GetBoundsForSelectedNodes(class FSlateRect& Rect, float Padding)
{
	const bool bResult = FBlueprintEditor::GetBoundsForSelectedNodes(Rect, Padding);

	return bResult;
}

void FDialogueGraphEditor::ShowDialogueDetails()
{
	DetailsView->SetObject(DialogueBlueprint);
}

bool FDialogueGraphEditor::CanShowDialogueDetails() const
{
	return IsValid(DialogueBlueprint);
}

void FDialogueGraphEditor::OpenNarrativeTutorialsInBrowser()
{
	FPlatformProcess::LaunchURL(*NarrativeHelpURL, NULL, NULL);
}

bool FDialogueGraphEditor::CanOpenNarrativeTutorialsInBrowser() const
{
	return true;
}

void FDialogueGraphEditor::QuickAddNode()
{
	//Disabled for now until we can find out why creating actions cause a nullptr crash
	return;

	//TSharedPtr<SGraphEditor> CurrentGraphEditor = UpdateGraphEdPtr.Pin();
	//if (!CurrentGraphEditor.IsValid())
	//{
	//	return;
	//}

	//const FScopedTransaction Transaction(FDialogueEditorCommands::Get().QuickAddNode->GetDescription());
	//UDialogueGraph* DialogueGraph = Cast<UDialogueGraph>(CurrentGraphEditor->GetCurrentGraph());

	//DialogueGraph->Modify();

	//const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

	//if (SelectedNodes.Num() == 0)
	//{
	//	return;
	//}

	//if (const UDialogueGraphSchema* Schema = Cast<UDialogueGraphSchema>(DialogueGraph->GetSchema()))
	//{
	//	for (auto& SelectedNode : SelectedNodes)
	//	{
	//		FDialogueSchemaAction_NewNode AddNewNode;
	//		UDialogueGraphNode* Node;
	//		//We we're quick adding from an action, add a new state after the action
	//		if (UDialogueGraphNode_Action* ActionNode = Cast<UDialogueGraphNode_Action>(SelectedNode))
	//		{
	//			//Dont do anything if we're already linked somewhere
	//			if (ActionNode->GetOutputPin()->LinkedTo.Num() == 0)
	//			{
	//				Node = NewObject<UDialogueGraphNode_State>(DialogueGraph, UDialogueGraphNode_State::StaticClass());
	//				AddNewNode.NodeTemplate = Node;

	//				FVector2D NewStateLocation = FVector2D(ActionNode->NodePosX, ActionNode->NodePosY);

	//				NewStateLocation.X += 300.f;

	//				AddNewNode.PerformAction(DialogueGraph, ActionNode->GetOutputPin(), NewStateLocation);


	//				// Update UI
	//				CurrentGraphEditor->NotifyGraphChanged();

	//				UObject* GraphOwner = DialogueGraph->GetOuter();
	//				if (GraphOwner)
	//				{
	//					GraphOwner->PostEditChange();
	//					GraphOwner->MarkPackageDirty();
	//				}
	//			}
	//		}
	//		else if (UDialogueGraphNode_State* StateNode = Cast<UDialogueGraphNode_State>(SelectedNode))
	//		{
	//			//Dont allow adding nodes from a failure or success node
	//			if (!(StateNode->IsA<UDialogueGraphNode_Failure>() || StateNode->IsA<UDialogueGraphNode_Success>()))
	//			{
	//				//For some reason this line crashes the editor and so quick add is disabled for now.
	//				Node = NewObject<UDialogueGraphNode_Action>(DialogueGraph, UDialogueGraphNode_Action::StaticClass());
	//				AddNewNode.NodeTemplate = Node;

	//				FVector2D NewActionLocation = FVector2D(ActionNode->NodePosX, ActionNode->NodePosY);

	//				NewActionLocation.X += 300.f;

	//				//Make sure new node doesn't overlay any others
	//				for (auto& LinkedTo : ActionNode->GetOutputPin()->LinkedTo)
	//				{
	//					if (LinkedTo->GetOwningNode()->NodePosY < NewActionLocation.Y)
	//					{
	//						NewActionLocation.Y = LinkedTo->GetOwningNode()->NodePosY - 200.f;
	//					}
	//				}

	//				AddNewNode.PerformAction(DialogueGraph, ActionNode->GetOutputPin(), NewActionLocation);


	//				// Update UI
	//				CurrentGraphEditor->NotifyGraphChanged();

	//				UObject* GraphOwner = DialogueGraph->GetOuter();
	//				if (GraphOwner)
	//				{
	//					GraphOwner->PostEditChange();
	//					GraphOwner->MarkPackageDirty();
	//				}
	//			}
	//		}
	//	}
	//}
}

bool FDialogueGraphEditor::CanQuickAddNode() const
{
	return true;
}

void FDialogueGraphEditor::PostUndo(bool bSuccess)
{
	if (bSuccess)
	{
	}

	FEditorUndoClient::PostUndo(bSuccess);

	if (DialogueBlueprint->DialogueGraph)
	{
		// Update UI
		DialogueBlueprint->DialogueGraph->NotifyGraphChanged();
		if (DialogueBlueprint)
		{
			DialogueBlueprint->PostEditChange();
			DialogueBlueprint->MarkPackageDirty();
		}
	}

}

void FDialogueGraphEditor::PostRedo(bool bSuccess)
{
	if (bSuccess)
	{
	}

	FEditorUndoClient::PostRedo(bSuccess);

	// Update UI
	if (DialogueBlueprint->DialogueGraph)
	{
		DialogueBlueprint->DialogueGraph->NotifyGraphChanged();
		if (DialogueBlueprint)
		{
			DialogueBlueprint->PostEditChange();
			DialogueBlueprint->MarkPackageDirty();
		}
	}
}


void FDialogueGraphEditor::CreateDefaultCommands()
{
	FBlueprintEditor::CreateDefaultCommands();
}

UBlueprint* FDialogueGraphEditor::GetBlueprintObj() const
{
	const TArray<UObject*>& EditingObjs = GetEditingObjects();
	for (int32 i = 0; i < EditingObjs.Num(); ++i)
	{
		if (EditingObjs[i]->IsA<UDialogueBlueprint>()) { return (UBlueprint*)EditingObjs[i]; }
	}
	return nullptr;
}

void FDialogueGraphEditor::SetupGraphEditorEvents(UEdGraph* InGraph, SGraphEditor::FGraphEditorEvents& InEvents)
{
	FBlueprintEditor::SetupGraphEditorEvents(InGraph, InEvents);

	// Custom menu for K2 schemas
	if (InGraph->Schema != nullptr && InGraph->Schema->IsChildOf(UDialogueGraphSchema::StaticClass()))
	{
		InEvents.OnNodeDoubleClicked = FSingleNodeEvent::CreateSP(this, &FDialogueGraphEditor::OnDialogueNodeDoubleClicked);
	}
}

//void FDialogueGraphEditor::StartEditingDefaults(bool bAutoFocus /*= true*/, bool bForceRefresh /*= false*/)
//{
//	if (DialogueBlueprint && DialogueBlueprint->DialogueTemplate)
//	{
//		UObject* DefaultObject = GetBlueprintObj()->GeneratedClass->GetDefaultObject();
//
//		// Update the details panel
//		FString Title;
//		DefaultObject->GetName(Title);
//		SKismetInspector::FShowDetailsOptions Options(FText::FromString(Title), bForceRefresh);
//		Options.bShowComponents = false;
//
//		Inspector->ShowDetailsForSingleObject(DialogueBlueprint->DialogueTemplate, Options);
//	}
//	else
//	{
//		FBlueprintEditor::StartEditingDefaults(bAutoFocus, bForceRefresh);
//	}
//}

void FDialogueGraphEditor::OnAddInputPin()
{
	FGraphPanelSelectionSet CurrentSelection;
	TSharedPtr<SGraphEditor> FocusedGraphEd = UpdateGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		CurrentSelection = FocusedGraphEd->GetSelectedNodes();
	}

	// Iterate over all nodes, and add the pin
	for (FGraphPanelSelectionSet::TConstIterator It(CurrentSelection); It; ++It)
	{
		//UBehaviorTreeDecoratorGraphNode_Logic* LogicNode = Cast<UBehaviorTreeDecoratorGraphNode_Logic>(*It);
		//if (LogicNode)
		//{
		//	const FScopedTransaction Transaction(LOCTEXT("AddInputPin", "Add Input Pin"));

		//	LogicNode->Modify();
		//	LogicNode->AddInputPin();

		//	const UEdGraphSchema* Schema = LogicNode->GetSchema();
		//	Schema->ReconstructNode(*LogicNode);
		//}
	}

	// Refresh the current graph, so the pins can be updated
	if (FocusedGraphEd.IsValid())
	{
		FocusedGraphEd->NotifyGraphChanged();
	}
}

bool FDialogueGraphEditor::CanAddInputPin() const
{
	return true;
}

void FDialogueGraphEditor::OnRemoveInputPin()
{

}

bool FDialogueGraphEditor::CanRemoveInputPin() const
{
	return true;
}

FText FDialogueGraphEditor::GetDialogueEditorTitle() const
{
	return FText::FromString(GetNameSafe(DialogueBlueprint));
}

FGraphAppearanceInfo FDialogueGraphEditor::GetDialogueGraphAppearance() const
{
	FGraphAppearanceInfo AppearanceInfo;
	AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText", "NARRATIVE DIALOGUE EDITOR");
	return AppearanceInfo;
}

bool FDialogueGraphEditor::CanAccessDialogueEditorMode() const
{
	return IsValid(DialogueBlueprint);
}

FText FDialogueGraphEditor::GetLocalizedMode(FName InMode)
{
	static TMap< FName, FText > LocModes;

	if (LocModes.Num() == 0)
	{
		LocModes.Add(DialogueEditorMode, LOCTEXT("DialogueEditorMode", "Dialogue Graph"));
	}

	check(InMode != NAME_None);
	const FText* OutDesc = LocModes.Find(InMode);
	check(OutDesc);
	return *OutDesc;
}


UDialogueBlueprint* FDialogueGraphEditor::GetDialogueAsset() const
{
	return DialogueBlueprint;
}

TSharedRef<SWidget> FDialogueGraphEditor::SpawnProperties()
{
	return
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.HAlign(HAlign_Fill)
		[
			DetailsView.ToSharedRef()
		];
}

void FDialogueGraphEditor::RegisterToolbarTab(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
}

void FDialogueGraphEditor::RestoreDialogueGraph()
{
	UDialogueGraph* MyGraph = Cast<UDialogueGraph>(DialogueBlueprint->DialogueGraph);
	const bool bNewGraph = MyGraph == NULL;
	if (MyGraph == NULL)
	{
		//if (DialogueBlueprint->LegacyAsset)
		//{
		//	DialogueBlueprint->LegacyAsset->DialogueGraph->Rename(nullptr, DialogueBlueprint);
		//	DialogueBlueprint->DialogueGraph = DialogueBlueprint->LegacyAsset->DialogueGraph;
		//	MyGraph = Cast<UDialogueGraph>(DialogueBlueprint->DialogueGraph);
		//}
		//else
		//{
			DialogueBlueprint->DialogueGraph = FBlueprintEditorUtils::CreateNewGraph(DialogueBlueprint, TEXT("Dialogue Graph"), UDialogueGraph::StaticClass(), UDialogueGraphSchema::StaticClass());
			MyGraph = Cast<UDialogueGraph>(DialogueBlueprint->DialogueGraph);

			FBlueprintEditorUtils::AddUbergraphPage(DialogueBlueprint, MyGraph);

			// Initialize the behavior tree graph
			const UEdGraphSchema* Schema = MyGraph->GetSchema();
			Schema->CreateDefaultNodesForGraph(*MyGraph);

			MyGraph->OnCreated();
		//}
	}
	else
	{
		MyGraph->OnLoaded();
	}

	MyGraph->Initialize();

	TSharedRef<FTabPayload_UObject> Payload = FTabPayload_UObject::Make(MyGraph);
	TSharedPtr<SDockTab> DocumentTab = DocumentManager->OpenDocument(Payload, bNewGraph ? FDocumentTracker::OpenNewDocument : FDocumentTracker::RestorePreviousDocument);

	if (DialogueBlueprint->LastEditedDocuments.Num() > 0)
	{
		TSharedRef<SGraphEditor> GraphEditor = StaticCastSharedRef<SGraphEditor>(DocumentTab->GetContent());
		GraphEditor->SetViewLocation(DialogueBlueprint->LastEditedDocuments[0].SavedViewOffset, DialogueBlueprint->LastEditedDocuments[0].SavedZoomAmount);
	}
}

void FDialogueGraphEditor::OnDialogueNodeDoubleClicked(UEdGraphNode* Node)
{
	if (UDialogueGraphNode* DNode = Cast<UDialogueGraphNode>(Node))
	{
		if (DNode->DialogueNode)
		{
			if (UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(DialogueBlueprint))
			{
				if (EventGraph->Nodes.Find(DNode->OnPlayedCustomNode) < 0)
				{
					if (UFunction* Func = DNode->FindFunction(GET_FUNCTION_NAME_CHECKED(UDialogueGraphNode, OnStartedOrFinished)))
					{
						//Try use the ID of the node if we gave it one prior to event add
						const FString NodeID = (DNode->DialogueNode->GetID() != NAME_None ? DNode->DialogueNode->GetID().ToString() : DNode->DialogueNode->GetName());
						FString OnEnteredFuncName = "OnDialogueNode Started/Finished Playing - " + NodeID;

						if (UK2Node_CustomEvent* OnPlayedEvent = UK2Node_CustomEvent::CreateFromFunction(EventGraph->GetGoodPlaceForNewNode(), EventGraph, OnEnteredFuncName, Func, false))
						{
							DNode->DialogueNode->OnPlayNodeFuncName = FName(OnEnteredFuncName);
							DNode->OnPlayedCustomNode = OnPlayedEvent;

							OnPlayedEvent->NodeComment = FString::Printf(TEXT("This event will automatically be called when this dialogue line starts/finishes. Use the bStarted param to check which occured."));
							OnPlayedEvent->SetMakeCommentBubbleVisible(true);

							OnPlayedEvent->bCanRenameNode = OnPlayedEvent->bIsEditable = false;

							//Jump to our newly created event! 
							JumpToNode(OnPlayedEvent, false);
						}
					}
				}
				else if(DNode->OnPlayedCustomNode)
				{
					JumpToNode(DNode->OnPlayedCustomNode, false);
				}
			}
		}
	}
}

void FDialogueGraphEditor::SaveEditedObjectState()
{
	DialogueBlueprint->LastEditedDocuments.Empty();
	DocumentManager->SaveAllState();
}

void FDialogueGraphEditor::OnCreateComment()
{
	TSharedPtr<SGraphEditor> GraphEditor = FocusedGraphEdPtr.Pin();
	if (GraphEditor.IsValid())
	{
		if (UEdGraph* Graph = GraphEditor->GetCurrentGraph())
		{
			if (const UEdGraphSchema* Schema = Graph->GetSchema())
			{
				if (Schema->IsA(UEdGraphSchema_K2::StaticClass()) || Schema->IsA(UDialogueGraphSchema::StaticClass()))
				{
					FEdGraphSchemaAction_K2AddComment CommentAction;
					CommentAction.PerformAction(Graph, nullptr, GraphEditor->GetPasteLocation());
				}
			}
		}
	}
}

TSharedRef<class SGraphEditor> FDialogueGraphEditor::CreateDialogueGraphEditorWidget(UEdGraph* InGraph)
{
	check(InGraph);

	CreateDialogueCommandList();

	SGraphEditor::FGraphEditorEvents InEvents;
	InEvents.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &FDialogueGraphEditor::OnSelectedNodesChanged);
	InEvents.OnNodeDoubleClicked = FSingleNodeEvent::CreateSP(this, &FDialogueGraphEditor::OnNodeDoubleClicked);
	InEvents.OnTextCommitted = FOnNodeTextCommitted::CreateSP(this, &FDialogueGraphEditor::OnNodeTitleCommitted);

	// Make title bar
	TSharedRef<SWidget> TitleBarWidget =
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush(TEXT("Graph.TitleBackground")))
		.HAlign(HAlign_Fill)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
		.HAlign(HAlign_Center)
		.FillWidth(1.f)
		[
			SNew(STextBlock)
			.Text(this, &FDialogueGraphEditor::GetDialogueEditorTitle)
			.TextStyle(FAppStyle::Get(), TEXT("GraphBreadcrumbButtonText"))
		]
		];

	// Make full graph editor
	const bool bGraphIsEditable = InGraph->bEditable;
	return SNew(SGraphEditor)
		.AdditionalCommands(GraphEditorCommands)
		.IsEditable(bGraphIsEditable)
		.Appearance(this, &FDialogueGraphEditor::GetDialogueGraphAppearance)
		.TitleBar(TitleBarWidget)
		.GraphToEdit(InGraph)
		.GraphEvents(InEvents);

}

void FDialogueGraphEditor::CreateInternalWidgets()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.bLockable = false;
	DetailsViewArgs.bAllowSearch = true;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bHideSelectionTip = false;
	DetailsViewArgs.NotifyHook = this;
	DetailsViewArgs.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Hide;
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetObject(NULL);
	DetailsView->OnFinishedChangingProperties().AddSP(this, &FDialogueGraphEditor::OnFinishedChangingProperties);
}

void FDialogueGraphEditor::ExtendMenu()
{
}


void FDialogueGraphEditor::ExtendToolbar()
{

}

//void FDialogueGraphEditor::BindCommonCommands()
//{
//	ToolkitCommands->MapAction(FDialogueEditorCommands::Get().ShowDialogueDetails,
//		FExecuteAction::CreateSP(this, &FDialogueGraphEditor::ShowDialogueDetails),
//		FCanExecuteAction::CreateSP(this, &FDialogueGraphEditor::CanShowDialogueDetails));
//
//	ToolkitCommands->MapAction(FDialogueEditorCommands::Get().ViewTutorial,
//		FExecuteAction::CreateSP(this, &FDialogueGraphEditor::OpenNarrativeTutorialsInBrowser),
//		FCanExecuteAction::CreateSP(this, &FDialogueGraphEditor::CanOpenNarrativeTutorialsInBrowser));
//
//	ToolkitCommands->MapAction(FDialogueEditorCommands::Get().QuickAddNode,
//		FExecuteAction::CreateSP(this, &FDialogueGraphEditor::QuickAddNode),
//		FCanExecuteAction::CreateSP(this, &FDialogueGraphEditor::CanQuickAddNode));
//}

#undef LOCTEXT_NAMESPACE
