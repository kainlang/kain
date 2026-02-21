// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "QuestGraphNode_State.h"
#include "QuestGraphNode_Root.generated.h"

/**
 * 
 */
UCLASS()
class UQuestGraphNode_Root : public UQuestGraphNode_State
{
	GENERATED_BODY()

public:

	virtual void AllocateDefaultPins() override;
	virtual bool CanUserDeleteNode() const override;
	virtual FLinearColor GetGraphNodeColor() const override;
	
};
