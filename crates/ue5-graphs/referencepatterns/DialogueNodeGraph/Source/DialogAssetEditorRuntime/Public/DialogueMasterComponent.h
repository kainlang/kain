/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#pragma once
#include "DialogueMasterSaveData.h"
#include "DialogueMasterStatistic.h"
#include "SwitchesAndCountersInterface.h"
#include "StepTypeEnum.h"

#include "DialogueMasterComponent.generated.h"

class UDialogAsset;
class UQuestAsset;

// Dialogues : 
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnDialogueBegin, class ADialogPlayer*, DialoguePlayerRef,
	class UDialogAsset*, Dialogue, UObject*, UserObject);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnDialogueCancelled, class ADialogPlayer*, DialoguePlayerRef,
	class UDialogAsset*, Dialogue, UObject*, UserObject);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnDialogueEnd, class ADialogPlayer*, DialoguePlayerRef,
	class UDialogAsset*, Dialogue, UObject*, UserObject);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_SevenParams(FOnDialogueLinePlay, class ADialogPlayer*, DialoguePlayerRef,
	class UDialogAsset*, Dialogue, const FText&, ActorName, AActor*, ActorRef,
	class UDialogNodeInfo*, Line, float, Duration, UObject*, UserObject);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnDialogueLineSkipped, class ADialogPlayer*, DialoguePlayerRef,
	class UDialogAsset*, Dialogue, UObject*, UserObject);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_SevenParams(FOnDialogueLineEnd, class ADialogPlayer*, DialoguePlayerRef,
	class UDialogAsset*, Dialogue, const FText&, ActorName, AActor*, ActorRef,
	const class UDialogNodeInfo*, Line, const TArray<struct FDialogSentence>&, answerList, UObject*, UserObject);

// Switchs & counters :
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSwitchValueChanged, FName, SwitchName, bool, NewValue);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnCounterValueChanged, FName, CounterName,
	int, PreviousValue, int, NewValue);

// Quests :
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnQuestEnteredNewStep, class UQuestAsset*, Quest, class UQuestStepNodeInfo*, NewStep);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnQuestExitStep, class UQuestAsset*, Quest, class UQuestStepNodeInfo*, ExitedStep);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FOnQuestTaskProgressUpdated, class UQuestAsset*, Quest, class UQuestStepNodeInfo*, Step,
	class UDialogueMasterTask*, Task, int, PreviousValue, int, NewValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnQuestTaskCompleted, class UQuestAsset*, Quest, class UQuestStepNodeInfo*, Step,
	class UDialogueMasterTask*, Task);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnQuestEnd, class UQuestAsset*, Quest, EStepType, EndType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestTracked, class UQuestAsset*, Quest);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnQuestUntracked, class UQuestAsset*, Quest, bool, UntrackedBecauseOfQuestEnd);

// Statistics :
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnStatisticUpdated, UDialogueMasterStatistic*, Statistic, FString, CountedEntityName, int, PreviousValue, int, NewValue);

// Save system :
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSaveBegin, const FString&, SlotName, const int32, UserIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLoadBegin, const FString&, SlotName, const int32, UserIndex);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSaveDoneDelegate, const FString&, SlotName, const int32, UserIndex, bool, Success);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FLoadDoneDelegate, const FString&, SlotName, const int32, UserIndex, USaveGame*, SaveData, bool, Success);

UCLASS(Blueprintable, BlueprintType, meta=(BlueprintSpawnableComponent))
class DIALOGASSETEDITORRUNTIME_API UDialogueMasterComponent : public UActorComponent, public ISwitchAndCounterPrerequisitesInterface
{
	GENERATED_BODY()
	
public:
	UDialogueMasterComponent();

	/*************************************************
	 * Save system
	 *************************************************/
	/**
	 *  If enabled, the save system will be disabled (no loading and no saving) of
	 *  switches and counters values.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Initialization")
	bool DebugMode = false;

private:
	//UPROPERTY()
	//UDialogueMasterSaveData *saveData;

public:
	/**
	 *  Call this function to save switches and counters values to the specified slot.
	 *  Note : If DebugMode is enabled, this function does nothing.
	 */
	UFUNCTION(BlueprintCallable, Category="Dialogue Master - Save system")
	bool DialogueMaster_SaveState(FString SlotName, int UserIndex);

	UFUNCTION(BlueprintCallable, Category="Dialogue Master - Save system")
	bool DialogueMaster_LoadState(FString SlotName, int UserIndex);

	UFUNCTION(BlueprintCallable, Category="Dialogue Master - Save system")
	bool DialogueMaster_DeleteSave(FString SlotName, int UserIndex);

	// Save events :
	// Called at the beginning of the loading process (but only if the save file exists!).
	UPROPERTY(BlueprintAssignable, Category="Dialogue Master - Save system")
	FOnLoadBegin OnLoadGameStateBegin;

	// Called at the beginning of the saving process.
	UPROPERTY(BlueprintAssignable, Category="Dialogue Master - Save system")
	FOnSaveBegin OnSaveGameStateBegin;

	// Called whatever the process succeeded (if it failed, the Success boolean will be set to false).
	UPROPERTY(BlueprintAssignable, Category="Dialogue Master - Save system")
	FLoadDoneDelegate OnGameStateLoaded;

	// Called whatever the process succeeded (if it failed, the Success boolean will be set to false).
	UPROPERTY(BlueprintAssignable, Category="Dialogue Master - Save system")
	FSaveDoneDelegate OnGameStateSaved;

	// Called when the loading process completed with success.
	UPROPERTY(BlueprintAssignable, Category="Dialogue Master - Save system")
	FOnLoadBegin OnLoadGameStateCompleted;

	// Called when the saving process completed with success.
	UPROPERTY(BlueprintAssignable, Category="Dialogue Master - Save system")
	FOnSaveBegin OnSaveGameStateCompleted;
	
	// Save event handlers :
	UFUNCTION()
	virtual void LoadGameStateBegin(const FString& SlotName, const int32 UserIndex);

	UFUNCTION()
	virtual void SaveGameStateBegin(const FString& SlotName, const int32 UserIndex);

	UFUNCTION()
	virtual void LoadGameStateCompleted(const FString& SlotName, const int32 UserIndex);

	UFUNCTION()
	virtual void SaveGameStateCompleted(const FString& SlotName, const int32 UserIndex);
	
	UFUNCTION()
	virtual void GameStateLoaded(const FString& SlotName, const int32 UserIndex, USaveGame* LoadedGameData, bool Success);

	UFUNCTION()
	virtual void GameStateSaved(const FString& SlotName, const int32 UserIndex, bool bSuccess);
	
	/*************************************************
	 * Switches & counters system
	 *************************************************/
	
	// ISwitchAndCounterPrerequisitesInterface implementation :
	virtual bool getSwitchValue_Implementation(FName switchName) override;
	virtual int getCounterValue_Implementation(FName counterName) override;
	virtual void setSwitchValue_Implementation(FName switchName, bool value) override;
	virtual void setCounterValue_Implementation(FName counterName, int value) override;
	// ------------------------------------------------------------

	// Switches & counters events :
	// Event called when a switch value change :
	UPROPERTY(BlueprintAssignable, Category="Switched & counters")
	FOnSwitchValueChanged OnSwitchValueChanged;

	// Event called when a counter value change :
	UPROPERTY(BlueprintAssignable, Category="Switched & counters")
	FOnCounterValueChanged OnCounterValueChanged;

	// Switches & counters event callbacks :
	UFUNCTION()
	virtual void SwitchValueChanged(FName SwitchName, bool NewValue);

	UFUNCTION()
	virtual void CounterValueChanged(FName CounterName, int PreviousValue, int NewValue);

	// Here if you need it, but not recommended to use !
	UFUNCTION(BlueprintCallable, Category="Switched & counters")
	virtual const TMap<FName, bool> & GetSwitchesMapRef() const;

	// Here if you need it, but not recommended to use !
	UFUNCTION(BlueprintCallable, Category="Switched & counters")
	virtual void GetSwitchesList(TArray<FName> & out) const;

	// Here if you need it, but not recommended to use !
	UFUNCTION(BlueprintCallable, Category="Switched & counters")
	virtual const TMap<FName, int> & GetCountersMapRef() const;

	// Here if you need it, but not recommended to use !
	UFUNCTION(BlueprintCallable, Category="Switched & counters")
	virtual void GetCountersList(TArray<FName> & out) const;

	/*************************************************
	 * Dialogue system
	 *************************************************/
	
	UFUNCTION(BlueprintCallable, Category="Dialogue Master Component", meta = (AutoCreateRefTerm = "UserObject,NPCManualReferences"))
	void PlayDialogue(UDialogAsset * Dialogue, AActor * NPC, AActor * Player, UObject * UserObject, TArray<AActor*> NPCManualReferences);

	UFUNCTION(BlueprintCallable, Category="Dialogue Master Component")
	void PlaySubDialogue(UDialogAsset * Dialogue);
	

	// Dialogue Events :
	UPROPERTY(BlueprintAssignable, Category="Dialogues")
	FOnDialogueBegin OnDialogueBegin;

	UPROPERTY(BlueprintAssignable, Category="Dialogues")
	FOnDialogueEnd OnDialogueEnd;

	UPROPERTY(BlueprintAssignable, Category="Dialogues")
	FOnDialogueLinePlay OnDialogueLinePlay;

	UPROPERTY(BlueprintAssignable, Category="Dialogues")
	FOnDialogueLineSkipped OnDialogueLineSkipped;
	
	UPROPERTY(BlueprintAssignable, Category="Dialogues")
	FOnDialogueLineEnd OnDialogueLineEnd;

	UPROPERTY(BlueprintAssignable, Category="Dialogues")
	FOnDialogueCancelled OnDialogueCancelled;


	// Dialogue event handlers :
	UFUNCTION()
	virtual void DialogueBegin(ADialogPlayer* DialoguePlayerRef, UDialogAsset* Dialogue, UObject* UserObject);

	UFUNCTION()
	virtual void DialogueEnd(ADialogPlayer* DialoguePlayerRef, UDialogAsset* Dialogue, UObject* UserObject);
	
	UFUNCTION()
	virtual void DialogueLinePlay(ADialogPlayer* DialoguePlayerRef, UDialogAsset* Dialogue, const FText& ActorName,
		AActor* ActorRef, UDialogNodeInfo* Line, float Duration, UObject* UserObject);

	UFUNCTION()
	virtual void DialogueLineSkipped(ADialogPlayer* DialoguePlayerRef, UDialogAsset* Dialogue, UObject* UserObject);

	UFUNCTION()
	virtual void DialogueLineEnd(ADialogPlayer* DialoguePlayerRef, UDialogAsset* Dialogue, const FText& ActorName,
		AActor* ActorRef, const UDialogNodeInfo* Line, const TArray<FDialogSentence>& AnswerList, UObject* UserObject);

	UFUNCTION()
	virtual void DialogueCancelled(ADialogPlayer* DialoguePlayerRef, UDialogAsset* Dialogue, UObject* UserObject);

private:
	UDialogueMasterStatistic * _cachedPlayDialogueNodeStatistic = nullptr;
	
protected:

	UDialogueMasterStatistic * GetPlayDialogueNodeStatisticInstance();
	
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category="Runtime (do not edit)")
	TMap<FName, bool> SwitchesMap;

	// Used to store counters and statistics.
	// On the stat side, it stores the amount of time an action have been done.
	// It is used to gray out already played DialogueResponses.
	// It can also store any other statistics (how many heal the player used, how many
	// sword swing he performed, etc...).
	UPROPERTY(EditAnywhere, Category="Runtime (do not edit)")
	TMap<FName, int> CountersMap;

	/*************************************************
	 * Quest system
	 *************************************************/
protected:	
	// This array contains all the quests the player has start or finish.
	UPROPERTY()
	TArray<UQuestAsset*> QuestsList;

	// This array contains all tracked quests.
	UPROPERTY()
	TArray<UQuestAsset*> TrackedQuests;

	UPROPERTY(EditAnywhere, Category="Quests")
	bool AutoTrackMainQuest = true;

	UPROPERTY(EditAnywhere, Category="Quests")
	bool AutoTrackSideQuest = false;

	/*
	 * Search for the given Quest in the QuestsList array.
	 * Returns the quest instance (if found) or nullptr (if not found).
	 */
	UFUNCTION(BlueprintCallable, Category="Quests")
	UQuestAsset* GetQuestInstance(UQuestAsset* Quest) const;

	/*
	 * Returns the list of all encountered quests.
	 */
	UFUNCTION(BlueprintCallable, Category="Quests")
	const TArray<UQuestAsset*>& GetQuestList() const;
	
public:
	UFUNCTION(BlueprintCallable, Category="Quests")
	virtual UQuestAsset* StartQuest(UQuestAsset* Quest);

	UFUNCTION(BlueprintCallable, Category="Quests")
	virtual void IsQuestAtStep(UQuestAsset* Quest, FName StepRequired, bool &QuestFound, bool &QuestAtRequiredStep) const;

	UFUNCTION(BlueprintCallable, Category="Quests")
	virtual void IsQuestSucceeded(UQuestAsset* Quest, bool &QuestFound, bool &Succeeded) const;

	UFUNCTION(BlueprintCallable, Category="Quests")
	virtual void IsQuestFailed(UQuestAsset* Quest, bool &QuestFound, bool &Failed) const;

	UFUNCTION(BlueprintCallable, Category="Quests")
	virtual void IsQuestFinished(UQuestAsset* Quest, bool &QuestFound, bool &Finished, bool &WithSuccess) const;

	UFUNCTION(BlueprintCallable, Category="Quests")
	virtual void TrackQuest(UQuestAsset* Quest);

	UFUNCTION(BlueprintCallable, Category="Quests")
	virtual void UntrackQuest(UQuestAsset* Quest);

protected:
	UFUNCTION()
	virtual void InternalUntrackQuest(UQuestAsset* Quest, bool becauseOfQuestEnd);
public:

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Quests")
	virtual void IsQuestTracked(UQuestAsset* Quest, bool &Tracked) const;
	
	// Quest events :
	// Redeclared here because we need a way to get the events back
	// from the quests. I really want to avoid having to dynamically
	// attach to every quest so UQuestAsset will have a ref to the
	// UDialogueMasterComponent and call back these events on it.
	UPROPERTY(BlueprintAssignable, Category="Quests")
	FOnQuestEnteredNewStep OnQuestEnteredNewStep;

	UPROPERTY(BlueprintAssignable, Category="Quests")
	FOnQuestExitStep OnQuestExitStep;

	UPROPERTY(BlueprintAssignable, Category="Quests")
	FOnQuestTaskProgressUpdated OnQuestTaskProgressUpdated;

	UPROPERTY(BlueprintAssignable, Category="Quests")
	FOnQuestTaskCompleted OnQuestTaskCompleted;

	UPROPERTY(BlueprintAssignable, Category="Quests")
	FOnQuestEnd OnQuestEnd;

	UPROPERTY(BlueprintAssignable, Category="Quests")
	FOnQuestTracked OnQuestTracked;

	UPROPERTY(BlueprintAssignable, Category="Quests")
	FOnQuestUntracked OnQuestUntracked;


	// Quest event handlers :
	UFUNCTION()
	virtual void QuestEnteredNewStep(UQuestAsset* Quest, UQuestStepNodeInfo* NewStep);

	UFUNCTION()
	virtual void QuestExitStep(UQuestAsset* Quest, UQuestStepNodeInfo* ExitedStep);
	
	UFUNCTION()
	virtual void QuestTaskProgressUpdated(UQuestAsset* Quest, UQuestStepNodeInfo* Step, UDialogueMasterTask* Task, int PreviousValue, int NewValue);

	UFUNCTION()
	virtual void QuestTaskCompleted(UQuestAsset* Quest, UQuestStepNodeInfo* Step, UDialogueMasterTask* Task);

	UFUNCTION()
	virtual void QuestEnd(UQuestAsset* Quest, EStepType EndType);

	UFUNCTION()
	virtual void QuestTracked(UQuestAsset* Quest);

	UFUNCTION()
	virtual void QuestUntracked(UQuestAsset* Quest, bool UntrackedBecauseOfQuestEnd);


	/*************************************************
	 * Statistics system
	 *************************************************/
public:
	UFUNCTION(BlueprintCallable, Category="Statistics")
	void UpdateStatistic(UDialogueMasterStatistic * StatisticAsset, const FString& CountedEntity, const int Quantity);

	UFUNCTION(BlueprintCallable, Category="Statistics")
	void SetStatisticValue(UDialogueMasterStatistic * StatisticAsset, const FString& CountedEntity, const int NewValue);

	UFUNCTION(BlueprintCallable, Category="Statistics")
	int GetStatisticValue(UDialogueMasterStatistic * StatisticAsset, const FString& CountedEntity);

	// Statistics events :
	UPROPERTY(BlueprintAssignable, Category="Statistics")
	FOnStatisticUpdated OnStatisticUpdated;

	// Statistic event handlers :
	UFUNCTION()
	virtual void StatisticUpdated(UDialogueMasterStatistic* Statistic, FString CountedEntityName, int PreviousValue, int NewValue);
};
