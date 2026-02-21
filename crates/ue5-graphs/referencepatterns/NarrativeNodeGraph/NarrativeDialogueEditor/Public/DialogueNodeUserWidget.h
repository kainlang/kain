//  Copyright Narrative Tools 2022.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DialogueNodeUserWidget.generated.h"

/**
 * Parent class for a custom UMG widget that narrative will add to dialogue nodes if you want to override narratives default UI
 */
UCLASS()
class NARRATIVEDIALOGUEEDITOR_API UDialogueNodeUserWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:

	void InitializeFromNode(class UDialogueNode* InNode, class UDialogue* InDialogue);

	UFUNCTION(BlueprintImplementableEvent, Category = "Dialogue Node")
	void OnNodeInitialized(class UDialogueNode* InNode, class UDialogue* InDialogue);

protected:

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue Node")
	class UDialogueNode* Node;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue Node")
	class UDialogue* Dialogue;

public:

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Dialogue Node")
	class UVerticalBox* LeftPinBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Dialogue Node")
	class UVerticalBox* RightPinBox;
};
