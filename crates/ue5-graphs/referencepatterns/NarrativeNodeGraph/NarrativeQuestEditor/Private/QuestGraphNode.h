// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"
#include "QuestSM.h"
#include "QuestGraphNode.generated.h"

class UNarrativeDataTask;

/**
 * 
 */
UCLASS()
class UQuestGraphNode : public UEdGraphNode
{
	GENERATED_BODY()
	
public:

	UPROPERTY(transient)
	UQuestGraphNode* ParentNode;

	UPROPERTY()
	TArray<UQuestGraphNode*> SubNodes;

	//The quest node associated with this graph node
	UPROPERTY()
	class UQuestNode* QuestNode;

	UPROPERTY()
	class UK2Node_CustomEvent* OnEnteredCustomNode;

	/*Called when the player reaches this step in the quest
	@param bActivated whether the node was activated, or deactivated. 
	*/
	UFUNCTION()
	void OnEntered(class UQuestNode* UpdatedQuestNode, bool bActivated);

	virtual void PostPlacedNewNode() override;
	virtual void AutowireNewNode(UEdGraphPin* FromPin) override;
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;

	// @return the input pin for this state
	virtual UEdGraphPin* GetInputPin(int32 InputIndex = 0) const;
	// @return the output pin for this state
	virtual UEdGraphPin* GetOutputPin(int32 InputIndex = 0) const;

	virtual FLinearColor GetGraphNodeColor() const { return FLinearColor(0.15f, 0.15f, 0.15f); };


};
