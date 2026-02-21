// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"
#include "DialogueGraphNode.generated.h"

/**
 * 
 */
UCLASS()
class UDialogueGraphNode : public UEdGraphNode
{
	GENERATED_BODY()
	
public:

	UPROPERTY(transient)
	class UDialogueGraphNode* ParentNode;

	UPROPERTY()
	TArray<class UDialogueGraphNode*> SubNodes;

	//The Dialogue node associated with this graph node
	UPROPERTY()
	class UDialogueNode* DialogueNode;

	virtual FText GetNodeTitleText() const {return FText::GetEmpty(); };
	virtual FText GetNodeText() const;

	virtual void PostPlacedNewNode() override;
	virtual void AutowireNewNode(UEdGraphPin* FromPin) override;
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual bool CanCreateUnderSpecifiedSchema(const UEdGraphSchema* DesiredSchema) const override;

	// @return the input pin for this state
	virtual UEdGraphPin* GetInputPin(int32 InputIndex = 0) const;
	// @return the output pin for this state
	virtual UEdGraphPin* GetOutputPin(int32 InputIndex = 0) const;

	virtual FLinearColor GetGraphNodeColor() const { return FLinearColor(0.15f, 0.15f, 0.15f); };


	UPROPERTY()
		class UK2Node_CustomEvent* OnPlayedCustomNode;

	UFUNCTION()
		void OnStartedOrFinished(UDialogueNode* Node, bool bStarted);

};
