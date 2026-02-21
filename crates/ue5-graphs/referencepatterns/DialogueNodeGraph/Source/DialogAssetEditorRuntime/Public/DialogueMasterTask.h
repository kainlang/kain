/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DialogueMasterTask.generated.h"

class UQuestAsset;
class UDialogueMasterComponent;
/**
 * 
 */
UCLASS(Blueprintable, EditInlineNew, Abstract, AutoExpandCategories=("Default"))
class DIALOGASSETEDITORRUNTIME_API UDialogueMasterTask : public UObject
{
	GENERATED_BODY()

public:

	UDialogueMasterTask();

	virtual UWorld* GetWorld() const override
	{
		if (HasAllFlags(RF_ClassDefaultObject))
		{
			return nullptr;
		}

		if(_World != nullptr) return _World;
		
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

	/** This is the number of completions already done by the player. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Task Info", Transient)
	int CurrentNumberOfCompletionDone;
	
	/** This is the number of completions required to update the quest task list state. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Task Info", meta = (ClampMin=1))
	int NumberOfCompletionRequired = 1;

	/** Check this box if this task is optional to complete the task list. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Task Info")
	bool bOptionalTask;

	/** Check this box if you don't want to show explicitly this task to the player in the quest description. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Task Info")
	bool bHiddenTask;

	/** Use this field to override the default generated description (to display a better description to the
	 * player).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Task Info")
	FText DescriptionOverride;

	/*
	 * This method is used to generate a task description.
	 * GetInGameTaskDescription is susceptible to use the result of this method (GetDescription)
	 * when not overridden.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Dialogue Master - Task")
	FText GetDescription();

	/*
	 * This method is used to generate a task's progress description.
	 * It's default behaviour is to return a formatted text that display the progress.
	 * Default format : (CurrentNumberOfCompletionDone / NumberOfCompletionRequired)
	 * This text is concatenated with the description to display the progress of a task.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Dialogue Master - Task")
	FText GetProgressText();

	/*
	 * This method is used to display the task description in the game user
	 * interface (displayed to the player) and on quest editor's task list nodes.
	 * If there is a DescriptionOverride set, it will display it.
	 * If there is no DescriptionOverride, it will returns the result of the
	 * GetDescription() method.
	 * If you want a custom behaviour to display the task in the user interface,
	 * you can override this method's implementation in derived C++ class or in derived blueprint.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Dialogue Master - Task")
	FText GetInGameTaskDescription();
	
	UFUNCTION(BlueprintPure, Category="Dialogue Master - Task")
	virtual bool IsCompleted();

	//------------------------------------------------
	// Runtime
	//------------------------------------------------
private:
	void UpdateNbCompletion(int newValue);
	
protected:
	UPROPERTY(BlueprintReadOnly, Category="Dialogue Master - Task")
	UQuestAsset* ParentQuest;

	UPROPERTY(BlueprintReadOnly, Category="Dialogue Master - Task")
	UDialogueMasterComponent* OwningComponent;

	UPROPERTY(BlueprintReadOnly, Category="Dialogue Master - Task")
	APlayerController* OwningPlayerController;

	UPROPERTY()
	UWorld* _World = nullptr;
	
public:
	virtual void PreInitializeTask();

	virtual void RestoreTask(UQuestAsset* ParQuest, UDialogueMasterComponent* OwningComp, APlayerController* OwningPC);
	
	virtual void BeginTask(UQuestAsset* ParQuest, UDialogueMasterComponent* OwningComp, APlayerController* OwningPC);
	
	UFUNCTION(BlueprintImplementableEvent, DisplayName="Begin Task", Category="Dialogue Master - Task")
	void BP_BeginTask();


	virtual void EndTask();

	UFUNCTION(BlueprintImplementableEvent, DisplayName="End Task", Category="Dialogue Master - Task")
	void BP_EndTask();

	UFUNCTION(BlueprintCallable, Category="Dialogue Master - Task")
	virtual void AddTaskCompletion(int NbOfCompletion = 1);

	UFUNCTION(BlueprintCallable, Category="Dialogue Master - Task")
	virtual void SetTaskCompletion(int NbOfCompletion);

	UFUNCTION(BlueprintCallable, Category="Dialogue Master - Task")
	virtual void CompleteTask();

	UFUNCTION(BlueprintCallable, Category="Dialogue Master - Task")
	virtual void ForceCompletionUpdate();

	UFUNCTION(BlueprintImplementableEvent, DisplayName="On Task Completed")
	void BP_OnTaskCompleted();
};
