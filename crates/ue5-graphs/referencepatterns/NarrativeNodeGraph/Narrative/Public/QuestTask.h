//  Copyright Narrative Tools 2022.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include <Engine/EngineTypes.h>
#include "QuestTask.generated.h"

/**
 * Tasks are blueprints that manage a task for the player to complete. This is mostly used by the quest editor. 
 * This is different from Data Tasks which are just data assets: a TaskName and Argument, ie "FindItem:Sword", 
 * 
 * A good example is the GoToLocation task that comes with narrative, which checks if the player has reached a goal location. This type
 * of task wouldn't be possible with a data task, which doesn't have any logic or variables.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, AutoExpandCategories = ("Default"))
class NARRATIVE_API UNarrativeTask : public UObject
{
	GENERATED_BODY()

public:

	friend class UQuestBranch; // Needed as branches "own" tasks and need to manage them 
	friend class UNarrativeComponent; // Needed for client processing quest updates 

	UNarrativeTask();

	/**The amount of times the task needs to be completed before the quest updates */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task", meta = (ClampMin=1))
	int32 RequiredQuantity;

	/** 
	In Narrative 3, tasks now autogenerate their descriptions to save designers needing to input it! For example the GoToLocation task
	that comes with Narrative takes the location name as input and then returns "Go to {LocationName}."

	However if you want to override the description you can do so entering one here. 
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Task")
	FText DescriptionOverride;

	/** Whether or not this task is optional */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Task")
	bool bOptional;

	/** Check this box to hide this task in the narrative UI! */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Task")
	bool bHidden;

	/** Defines how often the task should tick. Set to 0 if you want to disable ticking. 
	
	To optimize performance you should try avoid having tasks that tick at all, for example if your Task was is "ObtainItem", 
	you'd bind to an OnInventoryModified delegate in BeginTask and check the progress there! That way, you're only checking 
	the task when the players inventory updates, instead of just blindly checking over and over again. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Task")
	float TickInterval;

	// Allows the Object to get a valid UWorld from it's outer.
	virtual UWorld* GetWorld() const override
	{
		if (HasAllFlags(RF_ClassDefaultObject))
		{
			// If we are a CDO, we must return nullptr instead of calling Outer->GetWorld() to fool UObject::ImplementsGetWorld.
			return nullptr;
		}

		UObject* Outer = GetOuter();

		while (Outer)
		{
			UWorld* World = Outer->GetWorld();
			if (World)
			{
				return World;
			}

			Outer = Outer->GetOuter();
		}

		return nullptr;
	}
	
protected:

	//BeginTask - lets the task bind to a delegate, or do any other init we need - even spawn some checkpoints or a bunch of enemies to take out!
	void BeginTaskInit();
	virtual void BeginTask();

	//Called when the task is began - use this to check your task right away, bind any delegates, do anything you need 
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "Begin Task", Category = "Task")
	void K2_BeginTask();

	/**A function that repeatedly gets called in order to track progress. Generally try making tasks event based if you can, however
	sometimes tasks need to tick. Note that this function doesn't get called on the client, only the server, as tasks are server auth 
	so client doesn't really have any need to check the progress of a task */
	UFUNCTION(BlueprintNativeEvent, DisplayName = "Tick Task", Category = "Task")
	void TickTask();
	virtual void TickTask_Implementation();

	virtual void EndTask();

	/**Called when the task get deactivated (when all of the tasks on its branch are completed and the player has moved on in the quest)
	
	If you're looking for a function that is called when the task is completed use OnTaskCompleted*/
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "End Task", Category = "Task")
	void K2_EndTask();

	//[server only] Set the progress of this task, which may complete it when task reaches required progress. 
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Task")
	void SetProgress(const int32 NewProgress);

	//Sets the progress to whatever value you want it to be - interval non BP exposed version
	virtual void SetProgressInternal(const int32 NewProgress, const bool bFromReplication = false);

	//Allows you to add some progress to the quantity - negative values can also be used to subtract progress! 
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Task")
	virtual void AddProgress(const int32 ProgressToAdd = 1);

	//[server only] completes the task, by setting the current progress to required amount
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Task")
	virtual void CompleteTask();

	/**Called when the task is completed. Keep in mind this function can be called multiple times as narrative supports uncompleting tasks 
	as well as completing*/
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "On Task Completed", Category = "Task")
	void K2_OnTaskCompleted();

	//The current progress this task has made
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Task")
	int32 CurrentProgress;

	UPROPERTY(BlueprintReadOnly, Category = "Task")
	class UNarrativeComponent* OwningComp;

	UPROPERTY(BlueprintReadOnly, Category = "Task")
	class APawn* OwningPawn;

	UPROPERTY(BlueprintReadOnly, Category = "Task")
	class APlayerController* OwningController;
 
	UPROPERTY(BlueprintReadOnly, Category = "Task")
	class UQuest* OwningQuest;

	bool bIsActive;

	FTimerHandle TimerHandle_TickTask;

	UFUNCTION(BlueprintPure, Category = "Task")
	class UQuestBranch* GetOwningBranch() const;

public:

	/** Implement this if you want to autogenerate task descriptions that the editor UI and narrative UI will use,
	meaning you don't have to write a description for every task in your game."*/
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Editor")
	FText GetTaskDescription() const;
	virtual FText GetTaskDescription_Implementation() const;

	/**By default quest tasks display the task, followed by progress text ie: Find 10 Logs (6/10)
	If you want to change the (6/10) to display something else, you can override this function. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Editor")
	FText GetTaskProgressText() const;
	virtual FText GetTaskProgressText_Implementation() const;

	/** Optional special version of GetTaskDescription that is used for displaying info the editor nodes,
	but won't be used for ingame descriptions. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Editor")
	FText GetTaskNodeDescription() const;
	virtual FText GetTaskNodeDescription_Implementation() const;

	//Checks whether this task is currently complete or not
	UFUNCTION(BlueprintPure, Category = "Task")
	virtual bool IsComplete() const;

};
