//  Copyright Narrative Tools 2022.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QuestNodeUserWidget.generated.h"

/**
 * Parent class for a custom UMG widget that narrative will add to Quest nodes if you want to override narratives default UI
 */
UCLASS()
class NARRATIVEQUESTEDITOR_API UQuestNodeUserWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	void InitializeFromNode(class UQuestNode* InNode, class UQuest* InQuest);

	UFUNCTION(BlueprintImplementableEvent, Category = "Quest Node")
	void OnNodeInitialized(class UQuestNode* InNode, class UQuest* InQuest);

protected:

	UPROPERTY(BlueprintReadOnly, Category = "Quest Node")
	class UQuestNode* Node;

	UPROPERTY(BlueprintReadOnly, Category = "Quest Node")
	class UQuest* Quest;

public:

	UPROPERTY(meta = (BindWidget))
	class UVerticalBox* LeftPinBox;

	UPROPERTY(meta = (BindWidget))
	class UVerticalBox* RightPinBox;
};
