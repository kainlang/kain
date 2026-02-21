// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "NarrativeDataTask.generated.h"

/**
 * Data Tasks are lightweight tasks that consist of a Task Name, ie "FindItem", and an argument, ie "Pickaxe".
 * 
 * You can create data tasks in the content browser and store them using the CompleteNarrativeDataTask function. These data tasks are really useful for two main reasons:
 * 1. They can be used to move the player through a quest. IE KillNPC: Boss, FindItem: Dragonstone, etc
 * 2. Narrative saves these to disk, meaning you can check if the player has ever killed a given NPC, found a given item, etc (using HasCompletedTask function)
 * Dialogues can easily check these too, which is powerful, ie If HasCompletedTask "KillNPC: King" an NPC could say "How could you have slain the king!"
 */
UCLASS(BlueprintType, Blueprintable)
class NARRATIVE_API UNarrativeDataTask : public UDataAsset
{
	GENERATED_BODY()

public:

	UNarrativeDataTask(const FObjectInitializer& ObjectInitializer);

	/**A short name describing what this Task is*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Task Details")
	FString TaskName;

	/**A description of this task. Will get used as a tooltip in the quest editor so write something useful!  */
	UPROPERTY(EditAnywhere, Category = "Task Details")
	FText TaskDescription;

	/**The name of the argument the data task takes, for example a task called FindItems argument name might be "Item Name" for example  */
	UPROPERTY(EditAnywhere, Category = "Task Details")
	FString ArgumentName;

	/**The category of this Task, used for organization in the quest tool*/
	UPROPERTY(EditAnywhere, Category = "Task Details")
	FString TaskCategory;

	/**Default argument to autofill */
	UPROPERTY(EditAnywhere, Category = "Autofill")
	FString DefaultArgument;

	/**Convert the task to a raw string that the quest state machines can use. This will just take the task name (i.e "TalkToCharacter"), and append the Argument with an underscore. (i.e "TalkToCharacter_Bob")*/
	FString MakeTaskString(const FString& Argument) const;

	FText GetReferenceDisplayText();

};
