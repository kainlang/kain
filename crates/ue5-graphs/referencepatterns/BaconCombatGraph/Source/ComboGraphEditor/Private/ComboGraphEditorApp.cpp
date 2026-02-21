// © 2025 mrbaconvn. All Rights Reserved.

#include "ComboGraphEditorApp.h"
#include "ComboGraphEditorAppMode.h" 
#include "ComboGraphAsset.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Framework/Commands/GenericCommands.h"
#include "Graph/ComboGraphSchema.h"
#include "Graph/ComboGraph.h"
#include "Graph/Node/ComboNodeBase.h" 
#include "GraphData/ComboGraphData.h"  
#include "ComboGraphGlobals.h"
#include "EdGraphUtilities.h"
#include "ComboManager/ComboManagerComponent.h"
#include "Settings/EditorStyleSettings.h"
#include "Widget/ComboGraphDebugSelectorWidget.h"
#include "Windows/WindowsPlatformApplicationMisc.h"

#define APP_IDENTIFIER "ComboGraphEditorApp"
#define APP_DOCUMENTATION_LINK "https://pronhub.com/";

FComboGraphEditorApp::FComboGraphEditorApp()
{
    if(GEditor)
    {
        GEditor->RegisterForUndo(this);
    }
}

FComboGraphEditorApp::~FComboGraphEditorApp()
{
    if(GEditor)
    {
        GEditor->UnregisterForUndo(this);
    }
}

void FComboGraphEditorApp::RegisterTabSpawners(const TSharedRef<class FTabManager>& tabManager) {
    FWorkflowCentricApplication::RegisterTabSpawners(tabManager);
}

FName FComboGraphEditorApp::GetToolkitFName() const
{
    return APP_IDENTIFIER;
}

FText FComboGraphEditorApp::GetBaseToolkitName() const
{
    return FText::FromString(APP_IDENTIFIER);
}

FString FComboGraphEditorApp::GetWorldCentricTabPrefix() const
{
    return APP_IDENTIFIER;
}

FLinearColor FComboGraphEditorApp::GetWorldCentricTabColorScale() const
{
    return FLinearColor(0.3f, 0.2f, 0.5f, 0.5f);
}

FString FComboGraphEditorApp::GetDocumentationLink() const
{
    return APP_DOCUMENTATION_LINK;
}

void FComboGraphEditorApp::OnClose()
{
    SaveWorkingGraphToWorkingAsset();

    if (IsValid(GetWorkingAsset()))
    {
		GetWorkingAsset()->PreSaveDelegate.RemoveAll(this);
    }

    if (DebugTarget.IsValid())
    {
        DebugTarget->OnComboNodeExecuted.RemoveAll(this);
    } 
    
    FEditorDelegates::PostPIEStarted.RemoveAll(this);
    FEditorDelegates::EndPIE.RemoveAll(this);

    FWorkflowCentricApplication::OnClose();
}

void FComboGraphEditorApp::PostUndo(bool bSuccess)
{
    FEditorUndoClient::PostUndo(bSuccess);
    // Clear selection, to avoid holding refs to nodes that go away
    if (TSharedPtr<SGraphEditor> CurrentGraphEditor = GetGraphEditorWidget())
    {
        CurrentGraphEditor->ClearSelectionSet();
        CurrentGraphEditor->NotifyGraphChanged();
    }
    FSlateApplication::Get().DismissAllMenus();
}

void FComboGraphEditorApp::PostRedo(bool bSuccess)
{
    FEditorUndoClient::PostRedo(bSuccess);
    // Clear selection, to avoid holding refs to nodes that go away
    if (TSharedPtr<SGraphEditor> CurrentGraphEditor = GetGraphEditorWidget())
    {
        CurrentGraphEditor->ClearSelectionSet();
        CurrentGraphEditor->NotifyGraphChanged();
    }
    FSlateApplication::Get().DismissAllMenus();
}

void FComboGraphEditorApp::CreateGraphEditorWidget()
{
    CreateCommandList();

    SGraphEditor::FGraphEditorEvents InEvents;
    InEvents.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &FComboGraphEditorApp::OnSelectedNodesChanged);

    const bool bGraphIsEditable = WorkingGraph->bEditable;
    GraphEditorWidget =  SNew(SGraphEditor)
        .AdditionalCommands(GraphEditorCommands)  
        .GraphToEdit(WorkingGraph.Get())
        .IsEditable(bGraphIsEditable)
        .GraphEvents(InEvents);
}

void FComboGraphEditorApp::InitEditor(const EToolkitMode::Type mode, const TSharedPtr<class IToolkitHost>& initToolkitHost, UObject* inObject) {
    TArray<UObject*> ObjectsToEdit;
    ObjectsToEdit.Add(inObject);

    WorkingAsset = Cast<UComboGraphAsset>(inObject);
    WorkingGraph = Cast<UComboGraph>(FBlueprintEditorUtils::CreateNewGraph(WorkingAsset.Get(), NAME_None, UComboGraph::StaticClass(), UComboGraphSchema::StaticClass()));

	WorkingAsset->PreSaveDelegate.AddRaw(this, &FComboGraphEditorApp::OnAssetPreSave);

    CreateGraphEditorWidget();
    
    FEditorDelegates::PostPIEStarted.AddRaw(this, &FComboGraphEditorApp::OnPIEStarted);
    FEditorDelegates::EndPIE.AddRaw(this, &FComboGraphEditorApp::OnPIEStopped);

    InitAssetEditor(
        mode,
        initToolkitHost,
        APP_IDENTIFIER,
        FTabManager::FLayout::NullLayout,
        true, // createDefaultStandaloneMenu 
        true,  // createDefaultToolbar
        ObjectsToEdit);
    ExtendToolBar();

    UComboGraphEditorAppMode* AppMode = new UComboGraphEditorAppMode(SharedThis(this));
    AddApplicationMode(AppMode->GetModeName(), MakeShareable(AppMode));

    SetCurrentMode(AppMode->GetModeName());

    LoadWorkingAssetToWorkingGraph();
}

void FComboGraphEditorApp::OnSelectedNodesChanged(const FGraphPanelSelectionSet& NewSelection)
{ 
    if (!SelectedNodeDetailView.IsValid())
    {
        return;
    }

    UComboNodeBase* SelectedNode = GetSelectedNode(NewSelection);
    if (IsValid(SelectedNode)) {
        SelectedNodeDetailView->SetObject(SelectedNode->GetNodeData());
    }
    else {
        SelectedNodeDetailView->SetObject(nullptr);
    }
}

void FComboGraphEditorApp::SetSelectedNodeDetailView(TSharedPtr<IDetailsView> DetailsView)
{
    if(SelectedNodeDetailView.IsValid())
    {
        SelectedNodeDetailView->OnFinishedChangingProperties().RemoveAll(this);
    }
    
    SelectedNodeDetailView = DetailsView;
    SelectedNodeDetailView->OnFinishedChangingProperties().AddRaw(this, &FComboGraphEditorApp::OnNodeDetailViewPropertiesUpdated);
}

void FComboGraphEditorApp::OnNodeDetailViewPropertiesUpdated(const FPropertyChangedEvent& Event)
{
    if (GetGraphEditorWidget().IsValid()) {
        UComboNodeBase* SelectedNode = GetSelectedNode(GetGraphEditorWidget()->GetSelectedNodes());
        if (SelectedNode != nullptr) {
            SelectedNode->RebuildNode();
        }

        GetGraphEditorWidget()->NotifyGraphChanged();
    }
}

UComboNodeBase* FComboGraphEditorApp::GetSelectedNode(const FGraphPanelSelectionSet& Selection) 
{
    for (UObject* Obj : Selection) {
        UComboNodeBase* Node = Cast<UComboNodeBase>(Obj);
        if (Node != nullptr) {
            return Node;
        }
    }

    return nullptr;
}

void FComboGraphEditorApp::ExtendToolBar()
{
    if (ToolbarExtender.IsValid())
    {
        RemoveToolbarExtender(ToolbarExtender);
        ToolbarExtender.Reset();
    }

    ToolbarExtender = MakeShareable(new FExtender);

    AddToolbarExtender(ToolbarExtender);

    ToolbarExtender->AddToolBarExtension(
        "Asset",
        EExtensionHook::After,
        GetToolkitCommands(),
        FToolBarExtensionDelegate::CreateSP(this, &FComboGraphEditorApp::FillToolbar, true)
    );
}

void FComboGraphEditorApp::FillToolbar(FToolBarBuilder& ToolbarBuilder, bool bEndSection)
{
	DebugSelector = SNew(SComboGraphDebugSelector)
        .DebugComboGraphChanged(this, &FComboGraphEditorApp::OnSelectionChanged);

    ToolbarBuilder.BeginSection("Toolbar");
    {
        ToolbarBuilder.AddWidget(DebugSelector.ToSharedRef());
    }

    if (bEndSection)
    {
        ToolbarBuilder.EndSection();
    }
}

void FComboGraphEditorApp::OnSelectionChanged(TWeakObjectPtr<UComboManagerComponent> NewSelection)
{
    if (DebugTarget.IsValid())
    {
		DebugTarget->OnComboNodeExecuted.RemoveAll(this);
    }
    
    DebugTarget = NewSelection;
    if (DebugTarget.IsValid())
    {
		DebugTarget->OnComboNodeExecuted.AddRaw(this, &FComboGraphEditorApp::OnComboNodeTriggered);
    }
}

void FComboGraphEditorApp::OnComboNodeTriggered(const UComboGraphNodeData* NodeData)
{
	if (IsValid(GetWorkingGraph()))
	{
        if (CurrentRunningNode.IsValid())
        {
            CurrentRunningNode->SetNodeActive(false);
        }
        CurrentRunningNode = GetWorkingGraph()->FindNodeByData(NodeData);
        if (CurrentRunningNode.IsValid())
        {
            CurrentRunningNode->SetNodeActive(true);
        }
	}
}

void FComboGraphEditorApp::CreateCommandList()
{
    GraphEditorCommands = MakeShareable(new FUICommandList);
    {
        GraphEditorCommands->MapAction(FGenericCommands::Get().SelectAll,
            FExecuteAction::CreateSP(this, &FComboGraphEditorApp::SelectAllNodes),
            FCanExecuteAction::CreateSP(this, &FComboGraphEditorApp::CanSelectAllNodes)
        );

        GraphEditorCommands->MapAction(FGenericCommands::Get().Delete,
            FExecuteAction::CreateSP(this, &FComboGraphEditorApp::DeleteSelectedNodes),
            FCanExecuteAction::CreateSP(this, &FComboGraphEditorApp::CanDeleteSelectedNodes)
        );

        GraphEditorCommands->MapAction(FGenericCommands::Get().Duplicate,
            FExecuteAction::CreateSP(this, &FComboGraphEditorApp::DuplicateSelectedNodes),
            FCanExecuteAction::CreateSP(this, &FComboGraphEditorApp::CanDuplicateSelectedNodes)
        );
        
        GraphEditorCommands->MapAction(FGenericCommands::Get().Copy,
            FExecuteAction::CreateRaw(this, &FComboGraphEditorApp::CopySelectedNodes),
            FCanExecuteAction::CreateRaw(this, &FComboGraphEditorApp::CanCopySelectedNodes)
        );
        
        GraphEditorCommands->MapAction(FGenericCommands::Get().Paste,
            FExecuteAction::CreateRaw(this, &FComboGraphEditorApp::PasteNodes),
            FCanExecuteAction::CreateRaw(this, &FComboGraphEditorApp::CanPasteNodes)
        );
    }
}

void FComboGraphEditorApp::SelectAllNodes()
{
	GraphEditorWidget->SelectAllNodes();
}


bool FComboGraphEditorApp::CanSelectAllNodes() const
{
	return GraphEditorWidget.IsValid();
}

void FComboGraphEditorApp::DeleteSelectedNodes()
{
    if(GraphEditorWidget->GetSelectedNodes().Num() == 0)
    {
        return;
    }
    
    UComboGraph* Graph = GetWorkingGraph();
    if (!IsValid(Graph))
    { 
        ERR_LOG("Failed to get working graph");
        return;
    }

    const UComboGraphSchema* Schema = Cast<UComboGraphSchema>(Graph->GetSchema());
    if (!IsValid(Schema))
    {
        ERR_LOG("Failed to get schema");
        return;
    }
    
    FScopedTransaction Transaction(FText::FromString("Delete Combo Nodes"));
    
    bool bDeleteAny = false;
    for (UObject* Object : GraphEditorWidget->GetSelectedNodes())
    {
        UComboNodeBase* GraphNode = Cast<UComboNodeBase>(Object);
        if (Schema->SafeDeleteNodeFromGraph(Graph, GraphNode))
        { 
            bDeleteAny = true;
        }
    }

    if(bDeleteAny)
    { 
        GraphEditorWidget->ClearSelectionSet();
    }
    else
    {
        Transaction.Cancel();
    }
}

bool FComboGraphEditorApp::CanDeleteSelectedNodes()
{
    // SHOULD CHECK READONLY IF NEEDED
    if (GraphEditorWidget->GetSelectedNodes().IsEmpty())
    {
        return false;
    }

    for (UObject* Object : GraphEditorWidget->GetSelectedNodes())
    {
        UEdGraphNode* GraphNode = Cast<UEdGraphNode>(Object);
        if (GraphNode && !GraphNode->CanUserDeleteNode())
        {
            return false;
        }
    }

    return true;
}

void FComboGraphEditorApp::CopySelectedNodes()
{ 
    FGraphPanelSelectionSet SelectedNodes = GetGraphEditorWidget()->GetSelectedNodes();
    TArray<UComboNodeBase*> SubNodes;

    FString ExportedText; 
    for (FGraphPanelSelectionSet::TIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
    {
        UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter); 
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

bool FComboGraphEditorApp::CanCopySelectedNodes() const
{
    return CanDuplicateSelectedNodes();
}

void FComboGraphEditorApp::PasteNodes()
{
    if(GetGraphEditorWidget().IsValid())
    {
        PasteNodesHere(GetGraphEditorWidget()->GetPasteLocation());
    }
}

void FComboGraphEditorApp::PasteNodesHere(const FVector2D& Location)
{
    UComboGraph* Graph = GetWorkingGraph();
    if (!IsValid(Graph))
    {
        return;
    }
    const FScopedTransaction Transaction(FGenericCommands::Get().Paste->GetDescription());
    GetWorkingGraph()->Modify();
 
    // Clear the selection set (newly pasted stuff will be selected)
    GetGraphEditorWidget()->ClearSelectionSet();

    // Grab the text to paste from the clipboard.
    FString TextToImport;
    FPlatformApplicationMisc::ClipboardPaste(TextToImport);
    
    // Import the nodes
    TSet<UEdGraphNode*> PastedNodes;
    FEdGraphUtilities::ImportNodesFromText(Graph, TextToImport, PastedNodes);
    
    //Average position of nodes so we can move them while still maintaining relative distances to each other
    FVector2D AvgNodePosition(0.0f, 0.0f);
	
    // Number of nodes used to calculate AvgNodePosition
    int32 AvgCount = 0;

    for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
    {
        UEdGraphNode* EdNode = *It; 
        if (EdNode && EdNode->IsA<UComboNodeBase>())
        {
            AvgNodePosition.X += EdNode->NodePosX;
            AvgNodePosition.Y += EdNode->NodePosY;
            ++AvgCount;
        }
    }

    if (AvgCount > 0)
    {
        float InvNumNodes = 1.0f / float(AvgCount);
        AvgNodePosition.X *= InvNumNodes;
        AvgNodePosition.Y *= InvNumNodes;
    }

    for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
    {
        UEdGraphNode* PasteNode = *It;

        if(!IsValid(PasteNode) || !PasteNode->IsA<UComboNodeBase>())
        {
            continue;
        }
        
        // Give new node a different Guid from the old one
        PasteNode->CreateNewGuid();

        // Generate a new pin id
        for (UEdGraphPin* Pin : PasteNode->Pins)
        {
            Pin->PinId = FGuid::NewGuid();
        }
        
        const FVector::FReal NodePosX = (PasteNode->NodePosX - AvgNodePosition.X) + Location.X;
        const FVector::FReal NodePosY = (PasteNode->NodePosY - AvgNodePosition.Y) + Location.Y;

        PasteNode->NodePosX = static_cast<int32>(NodePosX);
        PasteNode->NodePosY = static_cast<int32>(NodePosY);

        PasteNode->SnapToGrid(GetDefault<UEditorStyleSettings>()->GridSnapSize);
 
        GetGraphEditorWidget()->SetNodeSelection(PasteNode, true);
    }

    Graph->NotifyGraphChanged();
}

bool FComboGraphEditorApp::CanPasteNodes() const
{  
    FString ClipboardContent;
    FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);

    return FEdGraphUtilities::CanImportNodesFromText(GetWorkingGraph(), ClipboardContent);
}

void FComboGraphEditorApp::DuplicateSelectedNodes()
{
    if(!GetGraphEditorWidget().IsValid())
    {
        return;
    }
    
    CopySelectedNodes();  
    FVector2D AvgNodePosition(0.0f, 0.0f);

    int32 AvgCount = 0;
    FGraphPanelSelectionSet SelectionSet = GetGraphEditorWidget()->GetSelectedNodes();
    for (UObject* NodeObject : SelectionSet)
    {
        UEdGraphNode* EdNode = Cast<UEdGraphNode>(NodeObject); 
        if (EdNode && EdNode->IsA<UComboNodeBase>())
        {
            AvgNodePosition.X += EdNode->NodePosX;
            AvgNodePosition.Y += EdNode->NodePosY;
            ++AvgCount;
        }
    }

    if (AvgCount > 0)
    {
        float InvNumNodes = 1.0f / float(AvgCount);
        AvgNodePosition.X *= InvNumNodes;
        AvgNodePosition.Y *= InvNumNodes;
    }
    
    AvgNodePosition += FVector2D(50.f, -50.f);
    PasteNodesHere(AvgNodePosition);
}

bool FComboGraphEditorApp::CanDuplicateSelectedNodes() const
{
    FGraphPanelSelectionSet SelectionSet = GetGraphEditorWidget()->GetSelectedNodes();

    for (UObject* Obj : SelectionSet)
    {
        UComboNodeBase* Node = Cast<UComboNodeBase>(Obj);
        if (!IsValid(Node) || !GetWorkingGraph()->Nodes.Contains(Node) || !Node->CanDuplicateNode())
        {
            return false;
        }
    }

    return true;
}

void FComboGraphEditorApp::OnPIEStarted(bool bIsSimulating)
{
    DebugSelector->GenerateDebugObjectInstances();
}

void FComboGraphEditorApp::OnPIEStopped(bool bIsSimulating)
{
	OnComboNodeTriggered(nullptr);
}

void FComboGraphEditorApp::OnAssetPreSave(UComboGraphAsset* Asset)
{
    if (IsValid(Asset) && Asset == GetWorkingAsset())
    {
        SaveWorkingGraphToWorkingAsset();
    }
}

bool FComboGraphEditorApp::SaveWorkingGraphToWorkingAsset()
{
    if (!IsValid(GetWorkingAsset()) || !IsValid(GetWorkingGraph()))
    {
        ERR_LOG("Invalid working asset or graph.");
        return false;
    }

    // Create a new graph data object to store the serialized graph
    UComboGraphData* GraphData = NewObject<UComboGraphData>(GetWorkingAsset());
    UComboGraph* Graph = GetWorkingGraph();

    // Map to store pin connections for later linking
    TMap<FGuid, TArray<FGuid>> PinConnections;

    // Serialize nodes and pins
    for (UEdGraphNode* EdGraphNode : Graph->Nodes)
    {
        UComboNodeBase* EditorNode = Cast<UComboNodeBase>(EdGraphNode);
        if (!EditorNode)
        {
            continue;
        }

        // Duplicate node data and store it in the graph data
        UComboGraphNodeData* NodeData = DuplicateObject(EditorNode->GetNodeData(), GraphData);
        NodeData->NodePosition = FIntPoint(EditorNode->NodePosX, EditorNode->NodePosY);
        NodeData->NodeGuid = EditorNode->NodeGuid;
        NodeData->InputPins.Empty();
        NodeData->OutputPins.Empty();

        GraphData->Nodes.Add(NodeData);

        // Serialize pins and their connections
        for (UEdGraphPin* Pin : EditorNode->Pins)
        {
            if (!Pin)
            {
                continue;
            }

            UComboPinData* PinData = NewObject<UComboPinData>(NodeData);
            PinData->PinName = Pin->PinName;
            PinData->PinId = Pin->PinId;
            PinData->Parent = NodeData;

            if (Pin->Direction == EGPD_Input)
            {
                NodeData->InputPins.Add(PinData);
            }
            else
            {
                NodeData->OutputPins.Add(PinData);
            }

            // Store output pin connections for later linking
            if (Pin->Direction == EGPD_Output && Pin->HasAnyConnections())
            {
                for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
                {
                    PinConnections.FindOrAdd(Pin->PinId).Add(LinkedPin->PinId);
                }
            }
        }
    }

    // Link pins based on stored connections
    for (const auto& ConnectionPair : PinConnections)
    {
        UComboPinData* FromPin = GraphData->FindPinById(ConnectionPair.Key);
        if (!FromPin)
        {
            continue;
        }

        for (const FGuid& ToPinId : ConnectionPair.Value)
        {
            UComboPinData* ToPin = GraphData->FindPinById(ToPinId);
            if (ToPin)
            {
                FromPin->ConnectToPins.Add(ToPin);
            }
        }
    }

    // Save the graph data to the asset
    GetWorkingAsset()->GraphData = GraphData;

    return true;
}

bool FComboGraphEditorApp::LoadWorkingAssetToWorkingGraph()
{
    if (!IsValid(GetWorkingAsset()) || !IsValid(GetWorkingGraph()))
    {
        ERR_LOG("Invalid working asset or graph.");
        return false;
    }

    const UComboGraphSchema* Schema = Cast<UComboGraphSchema>(GetWorkingGraph()->GetSchema());
    if (!IsValid(Schema))
    {
        ERR_LOG("Invalid graph schema.");
        return false;
    }

    UComboGraphData* GraphData = GetWorkingAsset()->GraphData;
    if (!GraphData)
    {
        // Initialize default nodes if no graph data exists
        GetWorkingGraph()->GetSchema()->CreateDefaultNodesForGraph(*GetWorkingGraph());
        return false;
    }

    // Map to store pin connections for later linking
    TMap<FGuid, TArray<FGuid>> PinConnections;

    // Deserialize nodes and pins
    for (UComboGraphNodeData* NodeData : GraphData->Nodes)
    {
        UComboNodeBase* NewNode = Schema->CreateComboNode(NodeData->GetClass(), GetWorkingGraph());
        if (!NewNode)
        {
            ERR_LOG("Failed to create node of type %s.", *NodeData->GetClass()->GetFName().ToString());
            continue;
        }

        // Set node properties
        NewNode->NodeGuid = NodeData->NodeGuid.IsValid() ? NodeData->NodeGuid : FGuid::NewGuid();
        NewNode->NodePosX = NodeData->NodePosition.X;
        NewNode->NodePosY = NodeData->NodePosition.Y;
        NewNode->SetNodeData(DuplicateObject(NodeData, NewNode));
        NewNode->GetNodeData()->SetFlags(RF_Transactional);

        // Deserialize input pins
        for (UComboPinData* PinData : NodeData->InputPins)
        {
            UEdGraphPin* Pin = NewNode->CreateCustomPin(EGPD_Input, PinData->PinName);
            Pin->PinId = PinData->PinId;
        }

        // Deserialize output pins and store connections
        for (UComboPinData* PinData : NodeData->OutputPins)
        {
            UEdGraphPin* Pin = NewNode->CreateCustomPin(EGPD_Output, PinData->PinName);
            Pin->PinId = PinData->PinId;

            for (UComboPinData* LinkedPin : PinData->ConnectToPins)
            {
                PinConnections.FindOrAdd(Pin->PinId).Add(LinkedPin->PinId);
            }
        }

        // Validate the node
        if (!NewNode->ValidateNode())
        {
            ERR_LOG("Failed to validate node %s from %s.", *NewNode->GetName(), *GetWorkingAsset()->GetName());
        }

        GetWorkingGraph()->AddNode(NewNode);
    }

    // Link pins based on stored connections
    for (const auto& ConnectionPair : PinConnections)
    {
        UEdGraphPin* FromPin = GetWorkingGraph()->FindPinById(ConnectionPair.Key);
        if (!FromPin)
        {
            continue;
        }

        for (const FGuid& ToPinId : ConnectionPair.Value)
        {
            UEdGraphPin* ToPin = GetWorkingGraph()->FindPinById(ToPinId);
            if (ToPin)
            {
                FromPin->LinkedTo.Add(ToPin);
                ToPin->LinkedTo.Add(FromPin);
            }
        }
    }

    return true;
}