/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */
#pragma once

#include <functional>

#include "CoreMinimal.h"
#include "QuestImportanceEnum.h"
#include "QuestRuntimeGraph.h"
#include "UObject/Object.h"
#include "StepTypeEnum.h"
#include "QuestAsset.generated.h"


class UDialogueMasterTask;
/**
 * 
 */
UCLASS(BlueprintType)
class DIALOGASSETEDITORRUNTIME_API UQuestAsset : public UObject
{
	   GENERATED_BODY()

	friend struct FQuestState;
public:
    UQuestAsset()
    {
	    
    }


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quest Info")
	FText QuestName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quest Info", meta=(MultiLine = true))
	FText QuestDescription;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quest Info")
	TEnumAsByte<EQuestImportance> QuestImportance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quest Info")
	int RecommendedLevel = 1;
	
	UPROPERTY()
	UQuestRuntimeGraph* Graph = nullptr;

public: // Our interface
	void SetPreSaveListener(std::function<void()> onPreSaveListener) { _onPreSaveListener = onPreSaveListener; }

public: // UObject interface
	virtual void PreSave(FObjectPreSaveContext saveContext) override;

   private: // Members
	std::function<void()> _onPreSaveListener = nullptr;

//------------------------------------------------------------------------------	
// Runtime :
//------------------------------------------------------------------------------	
private:
	UPROPERTY()
	class UDialogueMasterComponent * _OwningComponent = nullptr;

	UPROPERTY()
	APlayerController * _OwningPlayerController = nullptr;

	UPROPERTY()
	FName _CurrentStep = NAME_None;

	bool StepExists(FName Name) const;
	UQuestRuntimeNode * GetCurrentStepNode() const;
	UQuestRuntimeNode* GetNode(FName NodeID, EQuestNodeType NodeType) const;
	UQuestRuntimeNode * GetStepNode(FName StepName) const;
	UQuestRuntimeNode * GetTaskListNode(FName TaskListName) const;
public:
	bool Initialize(class UDialogueMasterComponent * OwningComponent, APlayerController * OwningPlayerController, FName StartStep = NAME_None);
	void RestoreFromState(struct FQuestState * state, UDialogueMasterComponent * OwningComponent, APlayerController * OwningPlayerController);
	
	UFUNCTION(BlueprintCallable, Category="Quests")
	class UQuestStepNodeInfo * GetCurrentStepInfo() const;

	UFUNCTION(BlueprintCallable, Category="Quests")
	TArray<class UQuestTaskListNodeInfo*> GetCurrentStepTasksList() const;

	UFUNCTION(BlueprintCallable, Category="Quests")
	class UQuestStepNodeInfo * GetStepInfo(FName StepName) const;

	UFUNCTION(BlueprintCallable, Category="Quests")
	TArray<class UQuestTaskListNodeInfo*> GetStepTasksList(FName StepName) const;

	UFUNCTION(BlueprintCallable, Category="Quests")
	FName GetCurrentStepID() const;

	UFUNCTION(BlueprintCallable, Category="Quests")
	bool IsQuestSucceeded() const;

	UFUNCTION(BlueprintCallable, Category="Quests")
	bool IsQuestFailed() const;

	UFUNCTION(BlueprintCallable, Category="Quests")
	bool IsQuestFinished() const;

	UFUNCTION()
	EStepType GetCurrentStepType() const;

	

	/*
	UPROPERTY(BlueprintAssignable, Category="Quests")
	FOnQuestEnteredNewStep QuestNewStep;

	UPROPERTY(BlueprintAssignable, Category="Quests")
	FOnQuestTaskProgressUpdated QuestTaskProgressUpdated;

	UPROPERTY(BlueprintAssignable, Category="Quests")
	FOnQuestTaskCompleted QuestTaskCompleted;
	*/
	
	void OnTaskProgressUpdated(UDialogueMasterTask* DialogueMasterTask, int PreviousValue, int NewValue);
	void OnTaskCompleted(UDialogueMasterTask* DialogueMasterTask);
private:
	virtual void OnEnteringStep(class UQuestStepNodeInfo * step, bool bRestoreState = false);
	virtual void OnExitStep(class UQuestStepNodeInfo * exitedStep);
	virtual void CheckStepTransition();
	virtual void OnQuestEnd(EStepType EndType);

	UQuestRuntimeNode * GetStepAfterTaskList(FName taskListID);
};
