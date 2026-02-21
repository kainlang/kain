// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "QuestGraphNode.h"
#include "QuestGraphNode_PersistentTasks.generated.h"

/**
 * 
 */
UCLASS()
class UQuestGraphNode_PersistentTasks : public UQuestGraphNode
{
	GENERATED_BODY()
	
public:

	virtual void AllocateDefaultPins() override;
	virtual bool CanUserDeleteNode() const override;
	virtual FLinearColor GetGraphNodeColor() const override;

};
