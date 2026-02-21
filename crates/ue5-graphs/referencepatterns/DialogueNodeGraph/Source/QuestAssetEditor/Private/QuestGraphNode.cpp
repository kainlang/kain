/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#include "QuestGraphNode.h"
#include "Framework/Commands/UIAction.h"
#include "ToolMenu.h"

FText UQuestGraphNode::GetNodeTitle(ENodeTitleType::Type titalType) const { 
    UQuestNodeInfo* nodeInfo = Cast<UQuestNodeInfo>(_nodeInfo);
    return FText::FromName(nodeInfo->ID);
}

FString UQuestGraphNode::GetNodeTypeName() const
{
    return TEXT("Quest Node Actions");
}

UEdGraphPin* UQuestGraphNode::CreateQuestPin(EEdGraphPinDirection direction, FName name) {
    FName category = (direction == EEdGraphPinDirection::EGPD_Input) ? TEXT("Inputs") : TEXT("Outputs");
    FName subcategory = GetSubcategory();

    UEdGraphPin* pin = CreatePin(
        direction,
        category,
        name
    );
    pin->PinType.PinSubCategory = subcategory;

    return pin;
}

UEdGraphPin* UQuestGraphNode::CreateDefaultInputPin() { 
    return CreateQuestPin(EEdGraphPinDirection::EGPD_Input, TEXT(""));
}

void UQuestGraphNode::CreateDefaultOutputPins() {
    FString defaultResponse = TEXT("");
    CreateQuestPin(EEdGraphPinDirection::EGPD_Output, FName(defaultResponse));
    GetQuestNodeInfo()->NodeOutputs.Add(FText::FromString(defaultResponse));
}

void UQuestGraphNode::SyncPinsWithResponses() {
    // Sync the pins on the node with the dialog responses
    // We're going to assume the first pin is always the
    // input pin
    UQuestNodeInfo* nodeInfo = GetQuestNodeInfo();
    int numGraphNodePins = Pins.Num() - 1;
    int numInfoPins = nodeInfo->NodeOutputs.Num();

    while (numGraphNodePins > numInfoPins) {
        RemovePinAt(numGraphNodePins - 1, EEdGraphPinDirection::EGPD_Output);
        numGraphNodePins--;
    }
    while (numInfoPins > numGraphNodePins) {
        CreateQuestPin(
            EEdGraphPinDirection::EGPD_Output,
            FName(nodeInfo->NodeOutputs[numGraphNodePins].ToString())
        );
        numGraphNodePins++;
    }

    int index = 1;
    for (const FText& option : nodeInfo->NodeOutputs) {
        GetPinAt(index)->PinName = FName(option.ToString());
        index++;
    }
}

FText UQuestStepGraphNode::GetNodeTitle(ENodeTitleType::Type titalType) const
{
    return FText::FromString("Step - " + Super::GetNodeTitle(titalType).ToString());
}

void UQuestStepGraphNode::GetNodeContextMenuActions(UToolMenu* menu, UGraphNodeContextMenuContext* context) const
{
    FToolMenuSection& section = menu->AddSection(TEXT("SectionName"), FText::FromString(GetNodeTypeName()));

    UQuestGraphNode* node = (UQuestGraphNode*)this;
    section.AddMenuEntry(
        TEXT("AddPinEntry"),
        FText::FromString(TEXT("Add New Quest Branch")),
        FText::FromString(TEXT("Creates a new quest branch")),
        FSlateIcon(TEXT("DialogAssetEditorStyle"), TEXT("DialogueMasterAssetEditor.NodeAddPinIcon")),
        FUIAction(FExecuteAction::CreateLambda(
            [node] () {
                node->GetQuestNodeInfo()->NodeOutputs.Add(FText::FromString(TEXT("")));
                node->SyncPinsWithResponses();
                node->GetGraph()->NotifyGraphChanged();
                node->GetGraph()->Modify();
            }
        ))
    );
    
    section.AddMenuEntry(
        TEXT("DeletePinEntry"),
        FText::FromString(TEXT("Delete Quest Branch")),
        FText::FromString(TEXT("Deletes the last quest branch")),
        FSlateIcon(TEXT("DialogAssetEditorStyle"), TEXT("DialogueMasterAssetEditor.NodeDeletePinIcon")),
        FUIAction(FExecuteAction::CreateLambda(
            [node] () {
                UEdGraphPin* pin = node->GetPinAt(node->Pins.Num() - 1);
                if (pin->Direction != EEdGraphPinDirection::EGPD_Input) {
                    UQuestNodeInfo* info = node->GetQuestNodeInfo();
                    info->NodeOutputs.RemoveAt(info->NodeOutputs.Num() - 1);
                    node->SyncPinsWithResponses();

                    node->GetGraph()->NotifyGraphChanged();
                    node->GetGraph()->Modify();
                }
            }
        ))
    );

    section.AddMenuEntry(
        TEXT("DeleteEntry"),
        FText::FromString(TEXT("Delete Node")),
        FText::FromString(TEXT("Deletes the node")),
        FSlateIcon(TEXT("DialogAssetEditorStyle"), TEXT("DialogueMasterAssetEditor.NodeDeleteNodeIcon")),
        FUIAction(FExecuteAction::CreateLambda(
            [node] () {
                node->GetGraph()->RemoveNode(node);
            }
        ))
    );
}

FText UQuestTaskListGraphNode::GetNodeTitle(ENodeTitleType::Type titalType) const
{
    return FText::FromString("Task list - " + Super::GetNodeTitle(titalType).ToString());
}

void UQuestTaskListGraphNode::GetNodeContextMenuActions(UToolMenu* menu, UGraphNodeContextMenuContext* context) const
{
    FToolMenuSection& section = menu->AddSection(TEXT("SectionName"), FText::FromString(GetNodeTypeName()));

    UQuestGraphNode* node = (UQuestGraphNode*)this;

    section.AddMenuEntry(
        TEXT("DeleteEntry"),
        FText::FromString(TEXT("Delete Node")),
        FText::FromString(TEXT("Deletes the node")),
        FSlateIcon(TEXT("DialogAssetEditorStyle"), TEXT("DialogueMasterAssetEditor.NodeDeleteNodeIcon")),
        FUIAction(FExecuteAction::CreateLambda(
            [node] () {
                node->GetGraph()->RemoveNode(node);
            }
        ))
    );
}