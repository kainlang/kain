// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "QuestGraphNode_State.h"
#include "QuestGraphNode_Success.generated.h"

/**
 * 
 */
UCLASS()
class UQuestGraphNode_Success : public UQuestGraphNode_State
{
	GENERATED_BODY()
	
public:

	virtual void AllocateDefaultPins() override;
	virtual FLinearColor GetGraphNodeColor() const override;
	
};
