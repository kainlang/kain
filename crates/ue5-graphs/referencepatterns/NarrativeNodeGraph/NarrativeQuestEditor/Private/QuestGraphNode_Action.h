// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "QuestGraphNode.h"
#include "QuestGraphNode_Action.generated.h"

class UQuestBranch;

/**
 * Should be called UQuestGraphNode_Branch, but the rename was making previous releases incompatible and ClassRedirects weren't working. 
 */
UCLASS()
class UQuestGraphNode_Action : public UQuestGraphNode
{
	GENERATED_BODY()
	
public:

	virtual void AllocateDefaultPins() override;
	virtual void DestroyNode() override;

	virtual FLinearColor GetGraphNodeColor() const override;

	UPROPERTY(EditAnywhere, Category = "Node")
	UQuestBranch* Branch;

};
