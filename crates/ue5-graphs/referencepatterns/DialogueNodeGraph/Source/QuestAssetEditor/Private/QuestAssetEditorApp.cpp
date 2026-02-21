/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#include "QuestAssetEditorApp.h"

#include "BlueprintEditor.h"
#include "QuestAssetAppMode.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "QuestGraphSchema.h"
#include "QuestGraphNode.h"
#include "QuestStartGraphNode.h"
#include "QuestAsset.h"

DEFINE_LOG_CATEGORY_STATIC(QuestAssetEditorAppSub, Log, All);

void QuestAssetEditorApp::RegisterTabSpawners(const TSharedRef<class FTabManager>& tabManager) {
    FWorkflowCentricApplication::RegisterTabSpawners(tabManager);
}

void QuestAssetEditorApp::InitEditor(const EToolkitMode::Type mode, const TSharedPtr<class IToolkitHost>& initToolkitHost, UObject* inObject) {
	TArray<UObject*> objectsToEdit;
    objectsToEdit.Add(inObject);
    
    _workingAsset = Cast<UQuestAsset>(inObject);
    _workingAsset->SetPreSaveListener([this] () { OnWorkingAssetPreSave(); });

    _workingGraph = FBlueprintEditorUtils::CreateNewGraph(
        _workingAsset,
        NAME_None,
        UEdGraph::StaticClass(),
        UQuestGraphSchema::StaticClass()
    );

	InitAssetEditor( 
        mode, 
        initToolkitHost, 
        TEXT("QuestAssetEditor"), 
        FTabManager::FLayout::NullLayout, 
        true, // createDefaultStandaloneMenu 
        true,  // createDefaultToolbar
        objectsToEdit);

    // Add our modes (just one for this example)
    AddApplicationMode(TEXT("QuestAssetAppMode"), MakeShareable(new QuestAssetAppMode(SharedThis(this))));

    // Set the mode
    SetCurrentMode(TEXT("QuestAssetAppMode"));

    UpdateEditorGraphFromWorkingAsset();
}

void QuestAssetEditorApp::OnClose() {
    UpdateWorkingAssetFromGraph();
    _workingAsset->SetPreSaveListener(nullptr);
    FAssetEditorToolkit::OnClose();
}

void QuestAssetEditorApp::OnNodeDetailViewPropertiesUpdated(const FPropertyChangedEvent& event) {
    if (_workingGraphUi != nullptr) {
        // Get the node being modified
        UQuestGraphNodeBase* questNode = GetSelectedNode(_workingGraphUi->GetSelectedNodes());
        if (questNode != nullptr) {
            questNode->OnPropertiesChanged();
        }
        _workingGraphUi->NotifyGraphChanged();
    }
}

void QuestAssetEditorApp::OnWorkingAssetPreSave() {
    // Update our asset from the graph just before saving it
    UpdateWorkingAssetFromGraph();
}

void QuestAssetEditorApp::UpdateWorkingAssetFromGraph() {
    if (_workingAsset == nullptr || _workingGraph == nullptr) {
        return;
    }

    UQuestRuntimeGraph* runtimeGraph = NewObject<UQuestRuntimeGraph>(_workingAsset);
    _workingAsset->Graph = runtimeGraph;

    TArray<std::pair<FGuid, FGuid>> connections;
    TMap<FGuid, UQuestRuntimePin*> idToPinMap;

    for (UEdGraphNode* uiNode : _workingGraph->Nodes) {
        if(uiNode->IsA<UQuestGraphNodeBase>())
        {
            UQuestRuntimeNode* runtimeNode = NewObject<UQuestRuntimeNode>(runtimeGraph);
            runtimeNode->Position = FVector2D(uiNode->NodePosX, uiNode->NodePosY);

            for (UEdGraphPin* uiPin : uiNode->Pins) {
                UQuestRuntimePin* runtimePin = NewObject<UQuestRuntimePin>(runtimeNode);
                runtimePin->PinName = uiPin->PinName;
                runtimePin->PinId = uiPin->PinId;
                runtimePin->Parent = runtimeNode;

                if (uiPin->HasAnyConnections() && uiPin->Direction == EEdGraphPinDirection::EGPD_Output) {
                    std::pair<FGuid, FGuid> connection = std::make_pair(uiPin->PinId, uiPin->LinkedTo[0]->PinId);
                    connections.Add(connection);
                }

                idToPinMap.Add(uiPin->PinId, runtimePin);
                if (uiPin->Direction == EEdGraphPinDirection::EGPD_Input) {
                    runtimeNode->InputPin = runtimePin;
                } else {
                    runtimeNode->OutputPins.Add(runtimePin);
                }
            }

            UQuestGraphNodeBase* uiQuestNode = Cast<UQuestGraphNodeBase>(uiNode);
            runtimeNode->NodeInfo = DuplicateObject(uiQuestNode->GetNodeInfo(), runtimeNode);
            runtimeNode->NodeType = uiQuestNode->GetQuestNodeType();

            runtimeGraph->Nodes.Add(runtimeNode);
        }
        else if(uiNode->IsA<UEdGraphNode_Comment>())
        {
            // Work on comments addition in standby...
            // TODO: Finish it...
            UQuestCommentNode* runtimeNode = NewObject<UQuestCommentNode>(runtimeGraph);
            runtimeNode->Bounds = FQuestCommentBounds(uiNode->NodePosX,
                uiNode->NodePosY,
                uiNode->NodePosX + uiNode->NodeWidth,
                uiNode->NodePosY + uiNode->NodeHeight);

            runtimeGraph->Comments.Add(runtimeNode);
            //UEdGraphNode_Comment* uiCommentNode = Cast<UEdGraphNode_Comment>(uiNode);
            //uiCommentNode->
        }
    }

    for (std::pair<FGuid, FGuid> connection : connections) {
        UQuestRuntimePin* pin1 = idToPinMap[connection.first];
        UQuestRuntimePin* pin2 = idToPinMap[connection.second];
        pin1->Connection = pin2;
    };
}

void QuestAssetEditorApp::UpdateEditorGraphFromWorkingAsset() {
    if (_workingAsset->Graph == nullptr) {
        UQuestRuntimeGraph* runtimeGraph = NewObject<UQuestRuntimeGraph>(_workingAsset);
        _workingGraph->GetSchema()->CreateDefaultNodesForGraph(*_workingGraph);
        return;
    }

    TArray<std::pair<FGuid, FGuid>> connections;
    TMap<FGuid, UEdGraphPin*> idToPinMap;

    for (UQuestRuntimeNode* runtimeNode : _workingAsset->Graph->Nodes) {
        UQuestGraphNodeBase* newNode = nullptr;
        if (runtimeNode->NodeType == EQuestNodeType::QuestStartNode) {
            newNode = NewObject<UQuestStartGraphNode>(_workingGraph);
        } else if (runtimeNode->NodeType == EQuestNodeType::QuestStepNode) {
            newNode = NewObject<UQuestStepGraphNode>(_workingGraph);
        } else if(runtimeNode->NodeType == EQuestNodeType::QuestTaskListNode) {
            newNode = NewObject<UQuestTaskListGraphNode>(_workingGraph);          
        } else {
            UE_LOG(QuestAssetEditorAppSub, Error, TEXT("QuestAssetEditorApp::UpdateEditorGraphFromWorkingAsset: Unknown node type"));
            continue;
        }
        newNode->CreateNewGuid();
        newNode->NodePosX = runtimeNode->Position.X;
        newNode->NodePosY = runtimeNode->Position.Y;
        
        if (runtimeNode->NodeInfo != nullptr) {
            newNode->SetNodeInfo(DuplicateObject(runtimeNode->NodeInfo, newNode));
        } else {
            newNode->InitNodeInfo(newNode);
        }

        if (runtimeNode->InputPin != nullptr) {
            UQuestRuntimePin* pin = runtimeNode->InputPin;
            UEdGraphPin* uiPin = newNode->CreateQuestPin(EEdGraphPinDirection::EGPD_Input, pin->PinName);
            uiPin->PinId = pin->PinId;

            if (pin->Connection != nullptr) {
                connections.Add(std::make_pair(pin->PinId, pin->Connection->PinId));
            }
            idToPinMap.Add(pin->PinId, uiPin);
        }

        for (UQuestRuntimePin* pin : runtimeNode->OutputPins) {
            UEdGraphPin* uiPin = newNode->CreateQuestPin(EEdGraphPinDirection::EGPD_Output, pin->PinName);
            uiPin->PinId = pin->PinId;

            if (pin->Connection != nullptr) {
                connections.Add(std::make_pair(pin->PinId, pin->Connection->PinId));
            }
            idToPinMap.Add(pin->PinId, uiPin);
        }

        _workingGraph->AddNode(newNode, true, true);
    }

    for(UQuestCommentNode * runtimeNode : _workingAsset->Graph->Comments)
    {
        UEdGraphNode_Comment * comment = NewObject<UEdGraphNode_Comment>(_workingGraph);
        comment->SetBounds(runtimeNode->Bounds.GetSlateBounds());
        comment->CommentColor = FLinearColor(0, 0, 0);

        _workingGraph->AddNode(comment, true, false);
    }

    for (std::pair<FGuid, FGuid> connection : connections) {
        UEdGraphPin* fromPin = idToPinMap[connection.first];
        UEdGraphPin* toPin = idToPinMap[connection.second];
        fromPin->LinkedTo.Add(toPin);
        toPin->LinkedTo.Add(fromPin);
    }
}

/**
 * Returns the first UQuestGraphNode in the given selection or nullptr if there are none.
 */
UQuestGraphNodeBase* QuestAssetEditorApp::GetSelectedNode(const FGraphPanelSelectionSet& selection) {
    for (UObject* obj : selection) {
        UQuestGraphNodeBase* node = Cast<UQuestGraphNodeBase>(obj);
        if (node != nullptr) {
            return node;
        }
    }
    
    return nullptr;
}

TArray<UQuestGraphNodeBase*> QuestAssetEditorApp::GetAllSelectedNodes(const FGraphPanelSelectionSet& selection)
{
    TArray<UQuestGraphNodeBase*> Nodes;
    for (UObject* obj : selection) {
        UQuestGraphNodeBase* node = Cast<UQuestGraphNodeBase>(obj);
        if (node != nullptr) {
            Nodes.Add(node);
        }
    }
    
    return Nodes;
}

void QuestAssetEditorApp::SetSelectedNodeDetailView(TSharedPtr<class IDetailsView> detailsView) { 
    _selectedNodeDetailView = detailsView;
    _selectedNodeDetailView->OnFinishedChangingProperties().AddRaw(this, &QuestAssetEditorApp::OnNodeDetailViewPropertiesUpdated);
}

void QuestAssetEditorApp::OnGraphSelectionChanged(const FGraphPanelSelectionSet& selection) {
    UQuestGraphNodeBase* selectedNode = GetSelectedNode(selection);
    CurrentSelection = GetAllSelectedNodes(selection);
    if (selectedNode != nullptr) {
        _selectedNodeDetailView->SetObject(selectedNode->GetNodeInfo());
    } else {
        _selectedNodeDetailView->SetObject(nullptr);
    }
}

bool QuestAssetEditorApp::GetBoundsOfSelection(FSlateRect& Result, float Padding)
{
    float minX=FLT_MAX, maxX = FLT_MIN, minY = FLT_MAX, maxY = FLT_MIN;

    bool bResult = false;
    for(UQuestGraphNodeBase* node : CurrentSelection)
    {
        if(IsValid(node))
        {
            int32 X = node->NodePosX;
            int32 Y = node->NodePosY;
            int32 R = X + node->NodeWidth;
            int32 B = Y + node->NodeHeight;
            
            if(X < minX)
            {
                minX = X;
            }

            if(Y < minY)
            {
                minY = Y;
            }

            if(R > maxX)
            {
                maxX = R;
            }

            if(B > maxY)
            {
                maxY = B;
            }

            bResult = true;
        }
    }

    Result.Left = minX - Padding;
    Result.Top = minY - Padding;
    Result.Right = maxX + Padding;
    Result.Bottom = maxY + Padding;
    
    return bResult;
}
