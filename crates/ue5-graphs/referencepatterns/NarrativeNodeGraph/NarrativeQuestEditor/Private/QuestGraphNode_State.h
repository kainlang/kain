// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "QuestGraphNode.h"
#include "QuestGraphNode_State.generated.h"

class UQuestState;

/**
 * 
 */
UCLASS()
class UQuestGraphNode_State : public UQuestGraphNode
{
	GENERATED_BODY()
	
public:

	virtual void AllocateDefaultPins() override;
	virtual void DestroyNode() override;
	virtual FLinearColor GetGraphNodeColor() const override;

	UPROPERTY(EditAnywhere, Category = "Node")
	UQuestState* State;
};
