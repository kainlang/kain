/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#pragma once

#include "QuestAssetEditorApp.h"
#include "QuestNodeInfo.h"
#include "EdGraph/EdGraphNode.h"
#include "QuestNodeInfoBase.h"
#include "QuestNodeType.h"

#define QUEST_START_PIN_CATEGORY "QuestStartPin"
#define QUEST_STEP_PIN_CATEGORY "QuestStepPin"
#define QUEST_TASK_LIST_PIN_CATEGORY "QuestTaskListPin"


#include "QuestGraphNodeBase.generated.h"

UCLASS()
class UQuestGraphNodeBase : public UEdGraphNode {
    GENERATED_BODY()
    
public: // Our interface
    virtual UEdGraphPin* CreateQuestPin(EEdGraphPinDirection direction, FName name) { /* Must be overidden */ return nullptr; }
    virtual UEdGraphPin* CreateDefaultInputPin() { return nullptr; }
    virtual void CreateDefaultOutputPins() { /* Nothing to do by default */ }

    virtual void InitNodeInfo(UObject* outer) { /* None by default */ }
    virtual void SetNodeInfo(UQuestNodeInfoBase* nodeInfo) { /* None by default */ }
    virtual UQuestNodeInfoBase* GetNodeInfo() const { /* None by default */ return nullptr; }

    virtual EQuestNodeType GetQuestNodeType() const { return EQuestNodeType::Unknown; }

    virtual void OnPropertiesChanged() { /* Nothing to do by default */ }

	virtual void UpdateIDToBeUnique()
    {    	
    	TArray<FName> IDs;
    	IDs.Add(FName("QuestStep_"));
		IDs.Add(FName("QuestTasks_"));

    	UQuestNodeInfo * currentNodeInfo = Cast<UQuestNodeInfo>(GetNodeInfo());
    	
	    UObject * ownerObject = GetTypedOuter(UEdGraph::StaticClass());
    	if(IsValid(currentNodeInfo) && IsValid(ownerObject))
    	{
    		UEdGraph * ownerGraph = Cast<UEdGraph>(ownerObject);
    		if(IsValid(ownerGraph))
    		{
    			for(TObjectPtr<UEdGraphNode> node : ownerGraph->Nodes)
    			{
    				UQuestGraphNodeBase * questNode = Cast<UQuestGraphNodeBase>(node);

    				if(questNode != this)
    				{
    					UQuestNodeInfo * nodeInfo = Cast<UQuestNodeInfo>(questNode->GetNodeInfo());
    					if(IsValid(nodeInfo))
    					{
    						IDs.Add(nodeInfo->ID);
    					}
    				}
    			}

    			int IdIterator = 1;
    			FName CurrentID = currentNodeInfo->ID;
    			FName NewID = currentNodeInfo->ID;

    			while(IDs.Contains(NewID))
    			{
    				NewID = FName(*FString::Printf(TEXT("%s%d"), *CurrentID.ToString(), IdIterator++));
    			}

    			currentNodeInfo->ID = NewID;
    		}
    	}
    }
};
