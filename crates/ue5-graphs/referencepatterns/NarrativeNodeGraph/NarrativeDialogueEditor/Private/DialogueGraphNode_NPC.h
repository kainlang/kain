// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "DialogueGraphNode.h"
#include "DialogueGraphNode_NPC.generated.h"

/**
 * 
 */
UCLASS()
class UDialogueGraphNode_NPC : public UDialogueGraphNode
{
	GENERATED_BODY()
	
public:

	virtual void AllocateDefaultPins() override;
	virtual FLinearColor GetGraphNodeColor() const;

	virtual FText GetNodeTitleText() const;
};
