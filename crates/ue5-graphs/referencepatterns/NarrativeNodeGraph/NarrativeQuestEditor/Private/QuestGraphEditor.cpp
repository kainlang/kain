// Copyright Narrative Tools 2022. 

#include "QuestGraphEditor.h"
#include "NarrativeQuestEditorModule.h"
#include "ScopedTransaction.h"
#include "QuestEditorTabFactories.h"
#include "GraphEditorActions.h"
#include "Framework/Commands/GenericCommands.h"
#include "QuestGraphNode.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "EdGraphUtilities.h"
#include "QuestGraph.h"
#include "QuestGraphSchema.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "QuestEditorModes.h"
#include "Widgets/Docking/SDockTab.h"
#include "QuestEditorToolbar.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "QuestGraphNode_State.h"
#include "QuestGraphNode_Action.h"
#include "QuestGraphNode_Root.h"
#include "QuestGraphNode_PersistentTasks.h"
#include "QuestSM.h"
#include "QuestBlueprint.h"
#include "Quest.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h" 
#include "QuestEditorCommands.h"
#include "QuestGraphNode_Failure.h"
#include "QuestGraphNode_Success.h"
#include <SKismetInspector.h>
#include "Kismet2/DebuggerCommands.h"
#include "EdGraphSchema_K2_Actions.h"
#include "K2Node_CustomEvent.h"
#include <AssetRegistry/AssetRegistryModule.h>
#include "NarrativeQuestTaskBlueprint.h"
#include <Engine/World.h>
#include "QuestNodeUserWidget.h"
#include "LevelEditor.h"

#define LOCTEXT_NAMESPACE "QuestAssetEditor"

const FName FQuestGraphEditor::QuestEditorMode(TEXT("QuestEditor"));

//TODO change this to the documentation page after documentation is ready for use
static const FString NarrativeHelpURL("http://www.google.com");

FQuestGraphEditor::FQuestGraphEditor()
{
	UEditorEngine* Editor = (UEditorEngine*)GEngine;
	if (Editor != NULL)
	{
		Editor->RegisterForUndo(this);
	}
}

FQuestGraphEditor::~FQuestGraphEditor()
{
	UEditorEngine* Editor = (UEditorEngine*)GEngine;
	if (Editor)
	{
		Editor->UnregisterForUndo(this);
	}
}

void FQuestGraphEditor::OnSelectedNodesChangedImpl(const TSet<class UObject*>& NewSelection)
{
	if (NewSelection.Num() == 1)
	{
		for (auto& Obj : NewSelection)
		{
			//Want to edit the underlying quest object, not the graph node
			if (UQuestGraphNode* GraphNode = Cast<UQuestGraphNode>(Obj))
			{
				TSet<class UObject*> ModifiedSelection;
				ModifiedSelection.Add(GraphNode->QuestNode);
				FBlueprintEditor::OnSelectedNodesChangedImpl(ModifiedSelection);
				return;
			}
		}
	}


	FBlueprintEditor::OnSelectedNodesChangedImpl(NewSelection);
}

void FQuestGraphEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	DocumentManager->SetTabManager(InTabManager);

	FWorkflowCentricApplication::RegisterTabSpawners(InTabManager);
}

void FQuestGraphEditor::InitQuestEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, class UQuestBlueprint* InQuestAsset)
{

	QuestBlueprint = InQuestAsset;

	if (!Toolbar.IsValid())
	{
		Toolbar = MakeShareable(new FBlueprintEditorToolbar(SharedThis(this)));
	}

	GetToolkitCommands()->Append(FPlayWorldCommands::GlobalPlayWorldActions.ToSharedRef());


	CreateDefaultCommands();
	//BindCommands();
	RegisterMenus();

	//Needs called before InitAssetEditor() to avoid nullptr. TODO look into how anim blueprints don't use a CreateInternalWidgets() function
	CreateInternalWidgets();

	TSharedPtr<FQuestGraphEditor> ThisPtr(SharedThis(this));

	TArray<UObject*> ObjectsToEdit;
	ObjectsToEdit.Add(QuestBlueprint);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	InitAssetEditor(Mode, InitToolkitHost, FNarrativeQuestEditorModule::QuestEditorAppId, FTabManager::FLayout::NullLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, ObjectsToEdit);

	//We need to initialize the document manager
	//if (!DocumentManager.IsValid())
	//{
	//	DocumentManager = MakeShareable(new FDocumentTracker);
	//	DocumentManager->Initialize(ThisPtr);

	//	//Register our graph editor tab with the factory
	//	TSharedRef<FDocumentTabFactory> GraphEditorFactory = MakeShareable(new FQuestGraphEditorSummoner(ThisPtr,
	//		FQuestGraphEditorSummoner::FOnCreateGraphEditorWidget::CreateSP(this, &FQuestGraphEditor::CreateGraphEditorWidget)
	//	));

	//	GraphEditorTabFactoryPtr = GraphEditorFactory;
	//	DocumentManager->RegisterDocumentFactory(GraphEditorFactory);
	//}

	TArray<UBlueprint*> EditedBlueprints;
	EditedBlueprints.Add(QuestBlueprint);

	CommonInitialization(EditedBlueprints, false);

	AddApplicationMode(QuestEditorMode, MakeShareable(new FQuestEditorApplicationMode(SharedThis(this))));

	ExtendMenu();
	ExtendToolbar();
	RegenerateMenusAndToolbars();

	SetCurrentMode(QuestEditorMode);

	PostLayoutBlueprintEditorInitialization();


	//Load all Quest Tasks into memory since we need them for editing quests 
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> AssetData;

	FARFilter Filter;
	Filter.bRecursiveClasses = true;
	Filter.bRecursivePaths = true;
	Filter.bIncludeOnlyOnDiskAssets = false;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 0
	Filter.TagsAndValues.Add("ParentClass", FString("/Script/CoreUObject.Class'/Script/Narrative.NarrativeTask'"));
	Filter.ClassPaths.Add(FTopLevelAssetPath("Class'/Script/Engine.Blueprint'"));
#else
	Filter.TagsAndValues.Add("ParentClass", FString("Class'/Script/Narrative.NarrativeTask'"));
	Filter.ClassNames.Add("Blueprint");
#endif

	AssetRegistryModule.Get().GetAssets(Filter, AssetData);

	for (auto& Asset : AssetData)
	{
		if (UNarrativeTaskBlueprint* BPAsset = Cast<UNarrativeTaskBlueprint>(Asset.GetAsset()))
		{
			BPAsset->GetClass()->GetDefaultObject();
		}
	}


	FLevelEditorModule& LevelEditor = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditor.OnMapChanged().AddRaw(this, &FQuestGraphEditor::OnWorldChange);

}

FName FQuestGraphEditor::GetToolkitFName() const
{
	return FName("Quest Editor");
}

FText FQuestGraphEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "QuestEditor");
}

FString FQuestGraphEditor::GetWorldCentricTabPrefix() const
{
	return "QuestEditor";
}

FLinearColor FQuestGraphEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.0f, 0.0f, 0.2f, 0.5f);
}

FText FQuestGraphEditor::GetToolkitToolTipText() const
{
	if (QuestBlueprint)
	{
		return FAssetEditorToolkit::GetToolTipTextForObject(QuestBlueprint);
	}
	return FText();
}

void FQuestGraphEditor::OnCreateComment()
{
	TSharedPtr<SGraphEditor> GraphEditor = FocusedGraphEdPtr.Pin();
	if (GraphEditor.IsValid())
	{
		if (UEdGraph* Graph = GraphEditor->GetCurrentGraph())
		{
			if (const UEdGraphSchema* Schema = Graph->GetSchema())
			{
				if (Schema->IsA(UEdGraphSchema_K2::StaticClass()) || Schema->IsA(UQuestGraphSchema::StaticClass())) 
				{
					FEdGraphSchemaAction_K2AddComment CommentAction;
					CommentAction.PerformAction(Graph, NULL, GraphEditor->GetPasteLocation());
				}

			}
		}
	}
}

void FQuestGraphEditor::CreateQuestGraphCommandList()
{
	//if (GraphEditorCommands.IsValid())
	//{
	//	return;
	//	GraphEditorCommands->MapAction(FGenericCommands::Get().SelectAll,
	//		FExecuteAction::CreateRaw(this, &FQuestGraphEditor::Quest_SelectAllNodes),
	//		FCanExecuteAction::CreateRaw(this, &FQuestGraphEditor::Quest_CanSelectAllNodes)
	//	);

	//	GraphEditorCommands->MapAction(FGenericCommands::Get().Delete,
	//		FExecuteAction::CreateRaw(this, &FQuestGraphEditor::Quest_DeleteSelectedNodes),
	//		FCanExecuteAction::CreateRaw(this, &FQuestGraphEditor::Quest_CanDeleteNodes)
	//	);

	//	GraphEditorCommands->MapAction(FGenericCommands::Get().Copy,
	//		FExecuteAction::CreateRaw(this, &FQuestGraphEditor::Quest_CopySelectedNodes),
	//		FCanExecuteAction::CreateRaw(this, &FQuestGraphEditor::Quest_CanCopyNodes)
	//	);

	//	GraphEditorCommands->MapAction(FGenericCommands::Get().Cut,
	//		FExecuteAction::CreateRaw(this, &FQuestGraphEditor::Quest_CutSelectedNodes),
	//		FCanExecuteAction::CreateRaw(this, &FQuestGraphEditor::Quest_CanCutNodes)
	//	);

	//	GraphEditorCommands->MapAction(FGenericCommands::Get().Paste,
	//		FExecuteAction::CreateRaw(this, &FQuestGraphEditor::Quest_PasteNodes),
	//		FCanExecuteAction::CreateRaw(this, &FQuestGraphEditor::Quest_CanPasteNodes)
	//	);

	//	GraphEditorCommands->MapAction(FGenericCommands::Get().Duplicate,
	//		FExecuteAction::CreateRaw(this, &FQuestGraphEditor::Quest_DuplicateNodes),
	//		FCanExecuteAction::CreateRaw(this, &FQuestGraphEditor::Quest_CanDuplicateNodes)
	//	);

	//	GraphEditorCommands->MapAction(FGraphEditorCommands::Get().CreateComment,
	//		FExecuteAction::CreateRaw(this, &FQuestGraphEditor::Quest_CreateComment),
	//		FCanExecuteAction::CreateRaw(this, &FQuestGraphEditor::Quest_CanCreateComment)
	//	);

	//}

}

bool FQuestGraphEditor::CanCopyNodes() const
{

	// Do not allow root or PT nodes to be copied
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);

		if (Node->IsA<UQuestGraphNode_Root>() || Node->IsA<UQuestGraphNode_PersistentTasks>())
		{
			return false;
		}
	}

	return FBlueprintEditor::CanCopyNodes();
}


void FQuestGraphEditor::PasteNodesHere(class UEdGraph* DestinationGraph, const FVector2D& Location)
{
	if (UQuestGraph* DGraph = Cast<UQuestGraph>(DestinationGraph))
	{
		//Pasting quest nodes has caused more trouble than it has been worth - uncomment at your own risk! 
		//Quest_PasteNodesHere(Location);
	}
	else
	{
		FBlueprintEditor::PasteNodesHere(DestinationGraph, Location);
	}
}

void FQuestGraphEditor::Quest_SelectAllNodes()
{
	if (TSharedPtr<SGraphEditor> CurrentGraphEditor = UpdateGraphEdPtr.Pin())
	{
		CurrentGraphEditor->SelectAllNodes();
	}
}

bool FQuestGraphEditor::Quest_CanSelectAllNodes() const
{
	return true;
}

void FQuestGraphEditor::Quest_DeleteSelectedNodes()
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

bool FQuestGraphEditor::Quest_CanDeleteNodes() const
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

void FQuestGraphEditor::Quest_DeleteSelectedDuplicatableNodes()
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

void FQuestGraphEditor::Quest_CutSelectedNodes()
{
	CopySelectedNodes();
	DeleteSelectedDuplicatableNodes();
}

bool FQuestGraphEditor::Quest_CanCutNodes() const
{
	return CanCopyNodes() && CanDeleteNodes();
}

void FQuestGraphEditor::Quest_CopySelectedNodes()
{
	// Export the selected nodes and place the text on the clipboard
	FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

	FString ExportedText;

	for (FGraphPanelSelectionSet::TIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
		UQuestGraphNode* QuestNode = Cast<UQuestGraphNode>(Node);
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

bool FQuestGraphEditor::Quest_CanCopyNodes() const
{

	//Copying nodes is disabled for now
	return false;

	// If any of the nodes can be duplicated then we should allow copying
	//const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	//for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	//{
	//	UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
	//	if (Node && Node->CanDuplicateNode())
	//	{
	//		return true;
	//	}
	//}

	//return false;
}

void FQuestGraphEditor::Quest_PasteNodes()
{

	if (TSharedPtr<SGraphEditor> CurrentGraphEditor = UpdateGraphEdPtr.Pin())
	{
		Quest_PasteNodesHere(CurrentGraphEditor->GetPasteLocation());
	}
}

void FQuestGraphEditor::Quest_PasteNodesHere(const FVector2D& Location)
{
	TSharedPtr<SGraphEditor> CurrentGraphEditor = FocusedGraphEdPtr.Pin();
	if (!CurrentGraphEditor.IsValid())
	{
		return;
	}

	// Undo/Redo support
	const FScopedTransaction Transaction(FGenericCommands::Get().Paste->GetDescription());
	UQuestGraph* QuestGraph = Cast<UQuestGraph>(CurrentGraphEditor->GetCurrentGraph());

	QuestGraph->Modify();

	UQuestGraphNode* SelectedParent = NULL;
	bool bHasMultipleNodesSelected = false;

	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

	// Clear the selection set (newly pasted stuff will be selected)
	CurrentGraphEditor->ClearSelectionSet();

	// Grab the text to paste from the clipboard.
	FString TextToImport;
	FPlatformApplicationMisc::ClipboardPaste(TextToImport);

	// Import the nodes
	TSet<UEdGraphNode*> PastedNodes;
	FEdGraphUtilities::ImportNodesFromText(QuestGraph, TextToImport, /*out*/ PastedNodes);

	for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
	{
		UEdGraphNode* PasteNode = *It;
		UQuestGraphNode* PasteQuestNode = Cast<UQuestGraphNode>(PasteNode);


		if (PasteNode && PasteQuestNode)
		{
			// Select the newly pasted stuff
			CurrentGraphEditor->SetNodeSelection(PasteNode, true);

			PasteNode->NodePosX += 200.f;
			PasteNode->NodePosY += 200.f;

			PasteNode->SnapToGrid(16);

			// Give new node a different Guid from the old one
			PasteNode->CreateNewGuid();

			//New Quest graph node will point to old quest node, duplicate a new one for our new node
			UQuestNode* DupNode = Cast<UQuestNode>(StaticDuplicateObject(PasteQuestNode->QuestNode, PasteQuestNode->QuestNode->GetOuter()));
			
			//StaticDuplicateObject won't have assigned a unique ID, grab a unique one
			DupNode->EnsureUniqueID();

			PasteQuestNode->QuestNode = DupNode;
		}
	}

	//Now that everything has been pasted, iterate a second time to rebuild the new nodes connections 
	for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
	{
		UEdGraphNode* PasteNode = *It;
		UQuestGraphNode* PasteQuestNode = Cast<UQuestGraphNode>(PasteNode);

		//Quest nodes connections will still be outdated, update these to the new connections
		QuestGraph->NodeAdded(PasteQuestNode);
		QuestGraph->PinRewired(PasteQuestNode, PasteQuestNode->GetOutputPin());
	}

	// Update UI
	CurrentGraphEditor->NotifyGraphChanged();

	UObject* GraphOwner = QuestGraph->GetOuter();
	if (GraphOwner)
	{
		GraphOwner->PostEditChange();
		GraphOwner->MarkPackageDirty();
	}
}

bool FQuestGraphEditor::Quest_CanPasteNodes() const
{
	//Pasting nodes is disabled for now
	return false;

	TSharedPtr<SGraphEditor> CurrentGraphEditor = UpdateGraphEdPtr.Pin();
	if (!CurrentGraphEditor.IsValid())
	{
		return false;
	}

	FString ClipboardContent;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);

	return FEdGraphUtilities::CanImportNodesFromText(CurrentGraphEditor->GetCurrentGraph(), ClipboardContent);
}

void FQuestGraphEditor::Quest_DuplicateNodes()
{
	CopySelectedNodes();
	PasteNodes();
}

bool FQuestGraphEditor::Quest_CanDuplicateNodes() const
{
	//Duplicating nodes is disabled for now
	return false;

	//return CanCopyNodes();
}

void FQuestGraphEditor::Quest_CreateComment()
{
	TSharedPtr<SGraphEditor> GraphEditor = UpdateGraphEdPtr.Pin();
	if (GraphEditor.IsValid())
	{
		if (UEdGraph* Graph = GraphEditor->GetCurrentGraph())
		{
			if (const UEdGraphSchema* Schema = Graph->GetSchema())
			{
				FQuestSchemaAction_AddComment CommentAction;
				CommentAction.PerformAction(Graph, NULL, GraphEditor->GetPasteLocation());
			}
		}
	}
}

bool FQuestGraphEditor::Quest_CanCreateComment() const
{
	return true;
}

void FQuestGraphEditor::OnWorldChange(UWorld* World, EMapChangeType MapChangeType)
{

	//Dialogue graph nodes will be referencing the UWorld, and if it changes this will breakcc
	if (World)
	{
		for (TObjectIterator<UUserWidget> Itr; Itr; ++Itr)
		{
			UUserWidget* Widget = *Itr;

			if (Widget->IsA<UQuestNodeUserWidget>())
			{
				Widget->Rename(nullptr, GetTransientPackage());
			}
		}
	}
}

void FQuestGraphEditor::ShowQuestDetails()
{
	DetailsView->SetObject(QuestBlueprint);
}

bool FQuestGraphEditor::Quest_GetBoundsForSelectedNodes(class FSlateRect& Rect, float Padding)
{
	const bool bResult = FBlueprintEditor::GetBoundsForSelectedNodes(Rect, Padding);

	return bResult;
}

bool FQuestGraphEditor::CanShowQuestDetails() const
{
	return IsValid(QuestBlueprint);
}

void FQuestGraphEditor::OpenNarrativeTutorialsInBrowser()
{
	FPlatformProcess::LaunchURL(*NarrativeHelpURL, NULL, NULL);
}

bool FQuestGraphEditor::CanOpenNarrativeTutorialsInBrowser() const
{
	return true;
}

void FQuestGraphEditor::QuickAddNode()
{
	//Disabled for now until we can find out why creating actions cause a nullptr crash
	return;

	TSharedPtr<SGraphEditor> CurrentGraphEditor = UpdateGraphEdPtr.Pin();
	if (!CurrentGraphEditor.IsValid())
	{
		return;
	}

	const FScopedTransaction Transaction(FQuestEditorCommands::Get().QuickAddNode->GetDescription());
	UQuestGraph* QuestGraph = Cast<UQuestGraph>(CurrentGraphEditor->GetCurrentGraph());

	QuestGraph->Modify();

	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

	if (SelectedNodes.Num() == 0)
	{
		return;
	}

	if (const UQuestGraphSchema* Schema = Cast<UQuestGraphSchema>(QuestGraph->GetSchema()))
	{
		for (auto& SelectedNode : SelectedNodes)
		{
			FQuestSchemaAction_NewNode AddNewNode;
			UQuestGraphNode* Node;
			//We we're quick adding from an action, add a new state after the action
			if (UQuestGraphNode_Action* ActionNode = Cast<UQuestGraphNode_Action>(SelectedNode))
			{
				//Dont do anything if we're already linked somewhere
				if (ActionNode->GetOutputPin()->LinkedTo.Num() == 0)
				{
					Node = NewObject<UQuestGraphNode_State>(QuestGraph, UQuestGraphNode_State::StaticClass());
					AddNewNode.NodeTemplate = Node;

					FVector2D NewStateLocation = FVector2D(ActionNode->NodePosX, ActionNode->NodePosY);

					NewStateLocation.X += 300.f;

					AddNewNode.PerformAction(QuestGraph, ActionNode->GetOutputPin(), NewStateLocation);


					// Update UI
					CurrentGraphEditor->NotifyGraphChanged();

					UObject* GraphOwner = QuestGraph->GetOuter();
					if (GraphOwner)
					{
						GraphOwner->PostEditChange();
						GraphOwner->MarkPackageDirty();
					}
				}
			}
			else if (UQuestGraphNode_State* StateNode = Cast<UQuestGraphNode_State>(SelectedNode))
			{
				//Dont allow adding nodes from a failure or success node
				if (!(StateNode->IsA<UQuestGraphNode_Failure>() || StateNode->IsA<UQuestGraphNode_Success>()))
				{
					//For some reason this line crashes the editor and so quick add is disabled for now.
					Node = NewObject<UQuestGraphNode_Action>(QuestGraph, UQuestGraphNode_Action::StaticClass());
					AddNewNode.NodeTemplate = Node;

					FVector2D NewActionLocation = FVector2D(ActionNode->NodePosX, ActionNode->NodePosY);

					NewActionLocation.X += 300.f;

					//Make sure new node doesn't overlay any others
					for (auto& LinkedTo : ActionNode->GetOutputPin()->LinkedTo)
					{
						if (LinkedTo->GetOwningNode()->NodePosY < NewActionLocation.Y)
						{
							NewActionLocation.Y = LinkedTo->GetOwningNode()->NodePosY - 200.f;
						}
					}

					AddNewNode.PerformAction(QuestGraph, ActionNode->GetOutputPin(), NewActionLocation);


					// Update UI
					CurrentGraphEditor->NotifyGraphChanged();

					UObject* GraphOwner = QuestGraph->GetOuter();
					if (GraphOwner)
					{
						GraphOwner->PostEditChange();
						GraphOwner->MarkPackageDirty();
					}
				}
			}
		}
	}
}

bool FQuestGraphEditor::CanQuickAddNode() const
{
	return true;
}

void FQuestGraphEditor::PostUndo(bool bSuccess)
{
	if (bSuccess)
	{
	}

	FBlueprintEditor::PostUndo(bSuccess);

	if (QuestBlueprint->QuestGraph)
	{
		// Update UI
		QuestBlueprint->QuestGraph->NotifyGraphChanged();
		if (QuestBlueprint)
		{
			QuestBlueprint->PostEditChange();
			QuestBlueprint->MarkPackageDirty();
		}
	}

}

void FQuestGraphEditor::PostRedo(bool bSuccess)
{
	if (bSuccess)
	{
	}

	FBlueprintEditor::PostRedo(bSuccess);

	// Update UI
	if (QuestBlueprint->QuestGraph)
	{
		QuestBlueprint->QuestGraph->NotifyGraphChanged();
		if (QuestBlueprint)
		{
			QuestBlueprint->PostEditChange();
			QuestBlueprint->MarkPackageDirty();
		}
	}
}

void FQuestGraphEditor::CreateDefaultCommands()
{
	FQuestEditorCommands::Register();

	ToolkitCommands->MapAction(FQuestEditorCommands::Get().ShowQuestDetails,
		FExecuteAction::CreateSP(this, &FQuestGraphEditor::ShowQuestDetails),
		FCanExecuteAction::CreateSP(this, &FQuestGraphEditor::CanShowQuestDetails));

	ToolkitCommands->MapAction(FQuestEditorCommands::Get().ViewTutorial,
		FExecuteAction::CreateSP(this, &FQuestGraphEditor::OpenNarrativeTutorialsInBrowser),
		FCanExecuteAction::CreateSP(this, &FQuestGraphEditor::CanOpenNarrativeTutorialsInBrowser));

	ToolkitCommands->MapAction(FQuestEditorCommands::Get().QuickAddNode,
		FExecuteAction::CreateSP(this, &FQuestGraphEditor::QuickAddNode),
		FCanExecuteAction::CreateSP(this, &FQuestGraphEditor::CanQuickAddNode));

	FBlueprintEditor::CreateDefaultCommands();
}

UBlueprint* FQuestGraphEditor::GetBlueprintObj() const
{
	const TArray<UObject*>& EditingObjs = GetEditingObjects();
	for (int32 i = 0; i < EditingObjs.Num(); ++i)
	{
		if (EditingObjs[i]->IsA<UQuestBlueprint>()) { return (UBlueprint*)EditingObjs[i]; }
	}
	return nullptr;
}

void FQuestGraphEditor::SetupGraphEditorEvents(UEdGraph* InGraph, SGraphEditor::FGraphEditorEvents& InEvents)
{
	FBlueprintEditor::SetupGraphEditorEvents(InGraph, InEvents);

	// Custom menu for K2 schemas
	if (InGraph->Schema != nullptr && InGraph->Schema->IsChildOf(UQuestGraphSchema::StaticClass()))
	{
		InEvents.OnNodeDoubleClicked = FSingleNodeEvent::CreateSP(this, &FQuestGraphEditor::OnQuestNodeDoubleClicked);
	}
}

FText FQuestGraphEditor::GetQuestEditorTitle() const
{
	if (UQuest* QuestObj = Cast<UQuest>(GetBlueprintObj()->GeneratedClass->GetDefaultObject()))
	{
		return QuestObj->GetQuestName();
	}

	return FText::GetEmpty();
}

FGraphAppearanceInfo FQuestGraphEditor::GetQuestGraphAppearance() const
{
	FGraphAppearanceInfo AppearanceInfo;
	AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText", "NARRATIVE QUEST EDITOR");
	return AppearanceInfo;
}

bool FQuestGraphEditor::CanAccessQuestEditorMode() const
{
	return IsValid(QuestBlueprint);
}

FText FQuestGraphEditor::GetLocalizedMode(FName InMode)
{
	static TMap< FName, FText > LocModes;

	if (LocModes.Num() == 0)
	{
		LocModes.Add(QuestEditorMode, LOCTEXT("QuestEditorMode", "Quest Graph"));
	}

	check(InMode != NAME_None);
	const FText* OutDesc = LocModes.Find(InMode);
	check(OutDesc);
	return *OutDesc;
}

UQuestBlueprint* FQuestGraphEditor::GetQuestAsset() const
{
	return QuestBlueprint;
}

TSharedRef<SWidget> FQuestGraphEditor::SpawnProperties()
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

void FQuestGraphEditor::RestoreQuestGraph()
{
	if (!QuestBlueprint)
	{
		return;
	}
	UQuestGraph* MyGraph = Cast<UQuestGraph>(QuestBlueprint->QuestGraph);
	const bool bNewGraph = MyGraph == NULL;
	if (MyGraph == NULL)
	{
		QuestBlueprint->QuestGraph = FBlueprintEditorUtils::CreateNewGraph(QuestBlueprint, TEXT("Quest Graph"), UQuestGraph::StaticClass(), UQuestGraphSchema::StaticClass());
		MyGraph = Cast<UQuestGraph>(QuestBlueprint->QuestGraph);

		FBlueprintEditorUtils::AddUbergraphPage(QuestBlueprint, MyGraph);

		// Initialize the behavior tree graph
		const UEdGraphSchema* Schema = MyGraph->GetSchema();
		Schema->CreateDefaultNodesForGraph(*MyGraph);

		MyGraph->OnCreated();
	}
	else
	{
		MyGraph->OnLoaded();
	}

	MyGraph->Initialize();

	TSharedRef<FTabPayload_UObject> Payload = FTabPayload_UObject::Make(MyGraph);
	TSharedPtr<SDockTab> DocumentTab = DocumentManager->OpenDocument(Payload, bNewGraph ? FDocumentTracker::OpenNewDocument : FDocumentTracker::RestorePreviousDocument);

	if (QuestBlueprint->LastEditedDocuments.Num() > 0)
	{
		TSharedRef<SGraphEditor> GraphEditor = StaticCastSharedRef<SGraphEditor>(DocumentTab->GetContent());
		
		GraphEditor->SetViewLocation(QuestBlueprint->LastEditedDocuments[0].SavedViewOffset, QuestBlueprint->LastEditedDocuments[0].SavedZoomAmount);


	}
}

void FQuestGraphEditor::OnQuestNodeDoubleClicked(UEdGraphNode* Node)
{
	if (UQuestGraphNode* QuestGraphNode = Cast<UQuestGraphNode>(Node))
	{
		//Something went super wrong is state is invalid 
		if (!QuestGraphNode->QuestNode)
		{
			return;
		}

		//Create a custom event for when we reach this state!
		if (UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(QuestBlueprint))
		{
			if (EventGraph->Nodes.Find(QuestGraphNode->OnEnteredCustomNode) < 0)
			{
				if (UFunction* StateFunc = QuestGraphNode->FindFunction(GET_FUNCTION_NAME_CHECKED(UQuestGraphNode, OnEntered)))
				{
					//Try use the ID of the node if we gave it one prior to event add
					const FString NodeID = (QuestGraphNode->QuestNode->GetID() != NAME_None ? QuestGraphNode->QuestNode->GetID().ToString() : QuestGraphNode->QuestNode->GetName());
					FString OnEnteredFuncName = "On Activated/Deactivated - " + NodeID;

					OnEnteredFuncName.RemoveSpacesInline();

					if (UK2Node_CustomEvent* OnEnteredEvent = UK2Node_CustomEvent::CreateFromFunction(EventGraph->GetGoodPlaceForNewNode(), EventGraph, OnEnteredFuncName, StateFunc, false))
					{
						if (QuestGraphNode->QuestNode->IsA<UQuestState>())
						{
							OnEnteredEvent->NodeComment = FString::Printf(TEXT("This event will be called automatically when the %s state is entered/exited. Use the bActivated param to check which occured."), *NodeID);
						}
						else
						{
							OnEnteredEvent->NodeComment = FString::Printf(TEXT("This event will be called automatically when the %s branch becomes active, or was completed (and thus deactivated). Use the bActivated param to check which occured."), *NodeID);
						}

						OnEnteredEvent->SetMakeCommentBubbleVisible(true);

						QuestGraphNode->OnEnteredCustomNode = OnEnteredEvent;
						QuestGraphNode->QuestNode->OnEnteredFuncName = FName(OnEnteredFuncName);

						OnEnteredEvent->bCanRenameNode = OnEnteredEvent->bIsEditable = false;

						//Jump the new node we made! 
						if (OnEnteredEvent)
						{
							JumpToNode(OnEnteredEvent, false);
						}
					}
				}
			}
			else
			{
				//Event graph already has the node created, jump to it
				if (QuestGraphNode->OnEnteredCustomNode)
				{
					JumpToNode(QuestGraphNode->OnEnteredCustomNode, false);
				}
			}
		}
	}
}

TSharedRef<class SGraphEditor> FQuestGraphEditor::CreateQuestGraphEditorWidget(UEdGraph* InGraph)
{
	check(InGraph);

	//Register commands for the quest graph 
	CreateQuestGraphCommandList();

	SGraphEditor::FGraphEditorEvents InEvents;
	InEvents.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &FQuestGraphEditor::OnSelectedNodesChanged);
	InEvents.OnNodeDoubleClicked = FSingleNodeEvent::CreateSP(this, &FQuestGraphEditor::OnQuestNodeDoubleClicked);
	InEvents.OnTextCommitted = FOnNodeTextCommitted::CreateSP(this, &FQuestGraphEditor::OnNodeTitleCommitted);

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
			.Text(this, &FQuestGraphEditor::GetQuestEditorTitle)
			.TextStyle(FAppStyle::Get(), TEXT("GraphBreadcrumbButtonText"))
		]
		];

	// Make full graph editor
	const bool bGraphIsEditable = InGraph->bEditable;
	return SNew(SGraphEditor)
		.AdditionalCommands(GraphEditorCommands)
		.IsEditable(bGraphIsEditable)
		.Appearance(this, &FQuestGraphEditor::GetQuestGraphAppearance)
		.TitleBar(TitleBarWidget)
		.GraphToEdit(InGraph)
		.GraphEvents(InEvents);


	
}

void FQuestGraphEditor::CreateInternalWidgets()
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
	DetailsView->OnFinishedChangingProperties().AddSP(this, &FQuestGraphEditor::OnFinishedChangingProperties);
}

void FQuestGraphEditor::ExtendMenu()
{
	//TODO we're not extending toolbar for now as we need to get blueprint menu working first 
	//if (MenuExtender.IsValid())
	//{
	//	RemoveMenuExtender(MenuExtender);
	//	MenuExtender.Reset();
	//}

	//MenuExtender = MakeShareable(new FExtender);
	//AddMenuExtender(MenuExtender);

	//FNarrativeQuestEditorModule& QuestEditorModule = FModuleManager::LoadModuleChecked<FNarrativeQuestEditorModule>("NarrativeQuestEditor");
	//AddMenuExtender(QuestEditorModule.GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));
}

void FQuestGraphEditor::ExtendToolbar()
{
	//TODO we're not extending toolbar for now as we need to get blueprint toolbar working first 
	//if (ToolbarExtender.IsValid())
	//{
	//	RemoveToolbarExtender(ToolbarExtender);
	//	ToolbarExtender.Reset();
	//}

	//ToolbarExtender = MakeShareable(new FExtender);

	//AddToolbarExtender(ToolbarExtender);

	//FNarrativeQuestEditorModule& QuestEditorModule = FModuleManager::LoadModuleChecked<FNarrativeQuestEditorModule>("NarrativeQuestEditor");
	//AddMenuExtender(QuestEditorModule.GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));
}


#undef LOCTEXT_NAMESPACE