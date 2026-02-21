/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#pragma once

#include "EdGraph/EdGraphNode.h"
#include "DialogGraphNodeBase.h"
#include "DialogNodeInfo.h"
#include "DialogGraphNode.generated.h"

UCLASS()
class UDialogGraphNode : public UDialogGraphNodeBase {
    GENERATED_BODY()

public: // UEdGraphNode interface
    virtual FText GetNodeTitle(ENodeTitleType::Type titalType) const override;

    UDialogAsset * GetAsset() const
    {
        return Cast<UDialogAsset>(GetOuter()->GetOuter());
    }
    
    FLinearColor getActorColor() const
    {
        UDialogAsset * asset = GetAsset();
        if(_nodeInfo->ActorIdx == 0)
            return FLinearColor(asset->PlayerActor.ActorNodeColor);
        // Here we check with ActorIdx - 1 because the ActorIdx is managed like that :
        // 0 : Player actor
        // 1 or more : Dialog Actors but the index of array start at 0 so we subtract 1.
        else if(_nodeInfo->ActorIdx - 1 >= 0 && _nodeInfo->ActorIdx - 1 < asset->DialogActors.Num())
            return FLinearColor(asset->DialogActors[_nodeInfo->ActorIdx - 1].ActorNodeColor);

        // Default color is green :
        return FLinearColor(FColor::Green);
    }

    FActorInfo * getActorInfo() const
    {
        UDialogAsset * asset = GetAsset();
        if(_nodeInfo->ActorIdx == 0)
            return &asset->PlayerActor;
        // Here we check with ActorIdx - 1 because the ActorIdx is managed like that :
        // 0 : Player actor
        // 1 or more : Dialog Actors but the index of array start at 0 so we subtract 1.
        else if(_nodeInfo->ActorIdx - 1 >= 0 && _nodeInfo->ActorIdx - 1 < asset->DialogActors.Num())
            return &asset->DialogActors[_nodeInfo->ActorIdx - 1];

        // Default color is green :
        return nullptr;
    }

    virtual FLinearColor GetNodeTitleColor() const override
    {
        UDialogNodeInfo* nodeInfo = Cast<UDialogNodeInfo>(_nodeInfo);
        return (nodeInfo != nullptr && nodeInfo->isBranchingNode) ? FLinearColor(FColor::Purple) : getActorColor();
    }
    virtual bool CanUserDeleteNode() const override { return true; }
    virtual void GetNodeContextMenuActions(class UToolMenu* menu, class UGraphNodeContextMenuContext* context) const override;

public: // UDialogGraphNodeBase interface
    virtual UEdGraphPin* CreateDialogPin(EEdGraphPinDirection direction, FName name) override;
    virtual UEdGraphPin* CreateDefaultInputPin() override;
    virtual void CreateDefaultOutputPins() override;

    virtual EDialogNodeType GetDialogNodeType() const override { return EDialogNodeType::DialogNode; }

    virtual void OnPropertiesChanged()
    {
        SyncPinsWithResponses();
        UpdateIDToBeUnique();
    }

public: // Our interface
    void SyncPinsWithResponses();

    virtual void InitNodeInfo(UObject* outer) { _nodeInfo = NewObject<UDialogNodeInfo>(outer); }
    virtual void SetNodeInfo(UDialogNodeInfoBase* nodeInfo) override { _nodeInfo = Cast<UDialogNodeInfo>(nodeInfo); }
    virtual UDialogNodeInfoBase* GetNodeInfo() const override { return _nodeInfo; }
    UDialogNodeInfo* GetDialogNodeInfo() { return _nodeInfo; }

    virtual void UpdateIDToBeUnique()
    {    	
        TArray<FName> IDs;
        UDialogAsset * asset = GetAsset();
        FString displayName = asset->GetName();
        FName baseName = FName(*FString::Printf(TEXT("%s_%s"), *displayName, *FString("DialogNode_")));
        IDs.Add(baseName);

        UDialogNodeInfo * currentNodeInfo = Cast<UDialogNodeInfo>(GetNodeInfo());
    	
        UObject * ownerObject = GetTypedOuter(UEdGraph::StaticClass());
        if(IsValid(currentNodeInfo) && IsValid(ownerObject))
        {
            UEdGraph * ownerGraph = Cast<UEdGraph>(ownerObject);
            if(IsValid(ownerGraph))
            {
                for(TObjectPtr<UEdGraphNode> node : ownerGraph->Nodes)
                {
                    UDialogGraphNodeBase * questNode = Cast<UDialogGraphNodeBase>(node);

                    if(questNode != this)
                    {
                        UDialogNodeInfo * nodeInfo = Cast<UDialogNodeInfo>(questNode->GetNodeInfo());
                        if(IsValid(nodeInfo))
                        {
                            IDs.Add(nodeInfo->UserId);
                        }
                    }
                }

                int IdIterator = 1;
                FName CurrentID = currentNodeInfo->UserId;
                if(CurrentID.IsNone()) CurrentID = baseName;
                FName NewID = CurrentID;

                while(IDs.Contains(NewID))
                {
                    NewID = FName(*FString::Printf(TEXT("%s%d"), *CurrentID.ToString(), IdIterator++));
                }

                currentNodeInfo->UserId = NewID;
            }
        }
    }
protected:
    UPROPERTY()
    class UDialogNodeInfo* _nodeInfo = nullptr;

    
};
