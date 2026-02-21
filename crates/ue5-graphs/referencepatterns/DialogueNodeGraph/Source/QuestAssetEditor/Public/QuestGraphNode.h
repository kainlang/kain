/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#pragma once

#include "EdGraph/EdGraphNode.h"
#include "QuestGraphNodeBase.h"
#include "QuestNodeInfoBase.h"
#include "QuestAsset.h"
#include "QuestNodeInfo.h"
#include "QuestGraphNode.generated.h"

UCLASS()
class UQuestGraphNode : public UQuestGraphNodeBase {
    GENERATED_BODY()

public: // UEdGraphNode interface
    virtual FText GetNodeTitle(ENodeTitleType::Type titalType) const override;

    UQuestAsset * GetAsset() const
    {
        return Cast<UQuestAsset>(GetOuter()->GetOuter());
    }

    virtual FLinearColor GetNodeTitleColor() const override
    {
        UQuestNodeInfoBase* nodeInfo = Cast<UQuestNodeInfoBase>(_nodeInfo);
        return FColor::Cyan;
    }
    virtual bool CanUserDeleteNode() const override { return true; }
    virtual FString GetNodeTypeName() const;

public: // UDialogGraphNodeBase interface
    virtual FName GetSubcategory() const { return "QuestPin"; }
    virtual UEdGraphPin* CreateQuestPin(EEdGraphPinDirection direction, FName name) override;
    virtual UEdGraphPin* CreateDefaultInputPin() override;
    virtual void CreateDefaultOutputPins() override;

    virtual EQuestNodeType GetQuestNodeType() const override { return EQuestNodeType::Unknown; }

    virtual void OnPropertiesChanged()
    {
        SyncPinsWithResponses();
        UpdateIDToBeUnique();
    }

public: // Our interface
    void SyncPinsWithResponses();

    virtual void InitNodeInfo(UObject* outer) { _nodeInfo = NewObject<UQuestNodeInfo>(outer); }
    virtual void SetNodeInfo(UQuestNodeInfoBase* nodeInfo) override { _nodeInfo = Cast<UQuestNodeInfo>(nodeInfo); }
    virtual UQuestNodeInfoBase* GetNodeInfo() const override { return _nodeInfo; }
    UQuestNodeInfo* GetQuestNodeInfo() { return _nodeInfo; }

protected:
    UPROPERTY()
    UQuestNodeInfo* _nodeInfo = nullptr;
};


UCLASS()
class UQuestStepGraphNode : public UQuestGraphNode
{
    GENERATED_BODY()
    
public:
    virtual FText GetNodeTitle(ENodeTitleType::Type titalType) const override;

    virtual FName GetSubcategory() const override { return QUEST_STEP_PIN_CATEGORY; }
    
    virtual FLinearColor GetNodeTitleColor() const override
    {
        UQuestNodeInfoBase* nodeInfo = Cast<UQuestNodeInfoBase>(_nodeInfo);
        UQuestStepNodeInfo* stepInfo = Cast<UQuestStepNodeInfo>(nodeInfo);
        if(stepInfo)
        {
            if(stepInfo->StepType == QUEST_SUCCEEDED)
            {
                return FLinearColor(0.0, 1.0, 0.0);
            }
            else if(stepInfo->StepType == QUEST_FAILED)
            {
                return FLinearColor(1.0, 0.0, 0.0);
            }
        }
        return FLinearColor(1.0, 1.0, 1.0);
    }

    virtual FString GetNodeTypeName() const override
    {
        return TEXT("Quest Step Node Actions");
    }

    virtual EQuestNodeType GetQuestNodeType() const override { return EQuestNodeType::QuestStepNode; }

    virtual void InitNodeInfo(UObject* outer)
    {
        _nodeInfo = NewObject<UQuestStepNodeInfo>(outer);
        _nodeInfo->ID = "QuestStep_";
    }

    virtual void GetNodeContextMenuActions(class UToolMenu* menu, class UGraphNodeContextMenuContext* context) const override;
};


UCLASS()
class UQuestTaskListGraphNode : public UQuestGraphNode
{
    GENERATED_BODY()
    
public:
    virtual FText GetNodeTitle(ENodeTitleType::Type titalType) const override;

    virtual FName GetSubcategory() const override { return QUEST_TASK_LIST_PIN_CATEGORY; }
    
    virtual FLinearColor GetNodeTitleColor() const override
    {
        UQuestNodeInfoBase* nodeInfo = Cast<UQuestNodeInfoBase>(_nodeInfo);
        return FColor::Blue;
    }

    virtual FString GetNodeTypeName() const override
    {
        return TEXT("Quest Task List Node Actions");
    }

    virtual EQuestNodeType GetQuestNodeType() const override { return EQuestNodeType::QuestTaskListNode; }
    
    virtual void InitNodeInfo(UObject* outer)
    {
        _nodeInfo = NewObject<UQuestTaskListNodeInfo>(outer);
        _nodeInfo->ID = "QuestTasks_";
    }

    virtual void GetNodeContextMenuActions(class UToolMenu* menu, class UGraphNodeContextMenuContext* context) const override;
};