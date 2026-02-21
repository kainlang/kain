// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "QuestSM.h"
#include "QuestTask.h"
#include "Quest.generated.h"

class UQuest;
class UQuestState;
class UQuestBranch;
class UQuestBlueprint;
class UNarrativeDataTask;

//Quests
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnQuestBranchCompleted, const UQuest*, Quest, const class UQuestBranch*, Branch);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnQuestNewState, UQuest*, Quest, const UQuestState*, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FOnQuestTaskProgressChanged, const UQuest*, Quest, const UNarrativeTask*, ProgressedTask, const class UQuestBranch*, Branch, int32, OldProgress, int32, NewProgress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnQuestTaskCompleted, const UQuest*, Quest, const UNarrativeTask*, CompletedTask, const class UQuestBranch*, Branch);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnQuestSucceeded, const UQuest*, Quest, const FText&, QuestSucceededMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnQuestFailed, const UQuest*, Quest, const FText&, QuestFailedMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestStarted, const UQuest*, Quest);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestForgotten, const UQuest*, Quest);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestRestarted, const UQuest*, Quest);

// Represents the state of a particular quest
UENUM(BlueprintType)
enum class EQuestCompletion : uint8
{
	QC_NotStarted UMETA(DisplayName="Not Started"),
	QC_Started UMETA(DisplayName = "Started"),
	QC_Succeded UMETA(DisplayName = "Succeeded"),
	QC_Failed  UMETA(DisplayName = "Failed")
};

UCLASS(Blueprintable, BlueprintType)
class NARRATIVE_API UQuest : public UObject
{
	GENERATED_BODY()
	
protected:
	
	/**A lot of friends - but i'd rather couple UQuest tightly to these classes than allow anything to 
	let anything call any of the internal UQuest functions that really shouldn't ever be touched*/
	friend class UNarrativeTask;
	friend class UQuestBlueprintGeneratedClass;
	friend class UQuestState;
	friend class UQuestBranch;
	friend class UNarrativeComponent;

	UQuest();

	//Dialogue assets/nodes etc have the same name on client and server, so can be referenced over the network 
	bool IsNameStableForNetworking() const override { return true; };
	bool IsSupportedForNetworking() const override { return true; };

	//The current state the player is at in this quest
	UPROPERTY(BlueprintReadOnly, Category = "Quests")
	UQuestState* CurrentState;

	virtual UWorld* GetWorld() const override;

public:

	FORCEINLINE UQuestState* GetCurrentState() const { return CurrentState; }

	UFUNCTION(BlueprintPure, Category = "Quest")
	class UQuestState* GetState(FName ID) const;

	UFUNCTION(BlueprintPure, Category = "Quest")
	class UQuestBranch* GetBranch(FName ID) const;

	UFUNCTION(BlueprintCallable, Category = "Quests")
    FText GetQuestName() const;

	UFUNCTION(BlueprintCallable, Category = "Quests")
	FText GetQuestDescription() const;

	UFUNCTION(BlueprintCallable, Category = "Quests")
	void SetQuestName(const FText& NewName);

	UFUNCTION(BlueprintCallable, Category = "Quests")
	void SetQuestDescription(const FText& NewDescription);


protected:

	//Called before tasks are ready - a good place to set up data tasks depend on 
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName="Pre Quest Started"))
		void BPPreQuestStarted(const UQuest* Quest);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName="On Quest Started"))
		void BPOnQuestStarted(const UQuest* Quest);

	UFUNCTION()
		void FailQuest(FText QuestFailedMessage);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Quest Failed"))
		void BPOnQuestFailed(const UQuest* Quest, const FText& QuestFailedMessage);

	/**Manually set the quest as succeeded. You'll need to provide some text for the UI as theres no node  the quest, you're manually succeeding it.*/
	UFUNCTION()
		void SucceedQuest(FText QuestSucceededMessage);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Quest Succeeded"))
		void BPOnQuestSucceeded(const UQuest* Quest, const FText& QuestSucceededMessage);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Quest New State"))
		void BPOnQuestNewState(UQuest* Quest, const UQuestState* NewState);

	UFUNCTION()
		void OnQuestTaskProgressChanged(const UNarrativeTask* Task, const class UQuestBranch* Step, int32 CurrentProgress, int32 RequiredProgress);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Quest Objective Progress Made"))
		void BPOnQuestTaskProgressChanged(const UQuest* Quest, const UNarrativeTask* Task, const class UQuestBranch* Step, int32 CurrentProgress, int32 RequiredProgress);

	UFUNCTION()
		void OnQuestTaskCompleted(const UNarrativeTask* Task, const class UQuestBranch* Branch);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Quest Task Completed"))
		void BPOnQuestTaskCompleted(const UQuest* Quest, const UNarrativeTask* Task, const class UQuestBranch* Step);

	UFUNCTION()
		void OnQuestBranchCompleted(const class UQuestBranch* Branch);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Branch Taken"))
		void BPOnQuestBranchCompleted(const UQuest* Quest, const class UQuestBranch* Branch);

	//Tell the quest that it is tracked - by default this will enable the quests navigation markers. 
	UFUNCTION(BlueprintCallable, Category = "Quest")
	virtual void SetTracked(const bool bNewTracked);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Tracked Changed"))
	void BPOnTrackedChanged(const UQuest* Quest, const bool bNewTracked);

	//Explicitly tell the quest to go to the given state
	UFUNCTION(BlueprintCallable, Category = "Quest")
	virtual void EnterState(UQuestState* NewState);

	virtual void EnterState_Internal(UQuestState* NewState);
	virtual void TakeBranch(UQuestBranch* Branch);

	//Initialize this quest from its blueprint generated class. Return true if successful. 
	virtual bool Initialize(class UNarrativeComponent* InitializingComp, const FName& QuestStartID = NAME_None);
	virtual void Deinitialize();
	virtual void DuplicateAndInitializeFromQuest(UQuest* QuestTemplate);


	virtual void BeginQuest(const FName& OptionalStartFromID = NAME_None);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest Details")
	FText QuestName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest Details", meta = (MultiLine = true))
	FText QuestDescription;

	//Whether or not the quest is marked as tracked. Use this to show or hide a quest 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest Details")
	bool bTracked;

	/**Child quests don't inherit quest graph nodes, however sometimes you'd like children to inherit some states, 
	for example your parent quest could have a state in here called "RanOutOfTime", and that way any child quests
	could inherit the "RanOutOfTime" state instead of having to manually have one added to every quest. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Quests")
	TArray<UQuestState*> InheritableStates;

	/**Current quest progress*/
	UPROPERTY()
	EQuestCompletion QuestCompletion;

	//The beginning state of this quest
	UPROPERTY(BlueprintReadOnly, Category = "Quests")
	UQuestState* QuestStartState;

	//Holds all of the states in the quest
	UPROPERTY()
	TArray<UQuestState*> States;

	//Holds all of the branches in the quest
	UPROPERTY()
	TArray<UQuestBranch*> Branches;

	//Holds all the spawned quest actors
	UPROPERTY()
	TArray<class AActor*> QuestActors;

	/**All the states we've reached so far. Useful for a quest journal, where we need to show the player what they have done so far*/
	UPROPERTY(BlueprintReadOnly, Category = "Quests")
	TArray<UQuestState*> ReachedStates;

	UPROPERTY(BlueprintReadOnly, Category = "Quests")
	class UNarrativeComponent* OwningComp;

	UPROPERTY(BlueprintReadOnly, Category = "Quests")
	class APawn* OwningPawn;

	UPROPERTY(BlueprintReadOnly, Category = "Quests")
	class APlayerController* OwningController;

	/**Called when a quest objective has been completed.*/
	UPROPERTY(BlueprintAssignable, Category = "Quests")
	FOnQuestBranchCompleted QuestBranchCompleted;

	/**Called when a quest objective is updated and we've received a new objective*/
	UPROPERTY(BlueprintAssignable, Category = "Quests")
		FOnQuestNewState QuestNewState;

	/**Called when a quest task in a branch has made progress. ie 6 out of 10 coins found*/
	UPROPERTY(BlueprintAssignable, Category = "Quests")
		FOnQuestTaskProgressChanged QuestTaskProgressChanged;

	/**Called when a quest task on a branch is completed ie 10 out of 10 coins found*/
	UPROPERTY(BlueprintAssignable, Category = "Quests")
		FOnQuestTaskCompleted QuestTaskCompleted;

	/**Called when a quest is completed.*/
	UPROPERTY(BlueprintAssignable, Category = "Quests")
		FOnQuestSucceeded QuestSucceeded;

	/**Called when a quest is failed.*/
	UPROPERTY(BlueprintAssignable, Category = "Quests")
		FOnQuestFailed QuestFailed;

	/**Called when a quest is started.*/
	UPROPERTY(BlueprintAssignable, Category = "Quests")
		FOnQuestStarted QuestStarted;

	/**Called when a quest is forgotten.*/
	UPROPERTY(BlueprintAssignable, Category = "Quests")
		FOnQuestForgotten QuestForgotten;

	/**Called when a quest is restarted.*/
	UPROPERTY(BlueprintAssignable, Category = "Quests")
		FOnQuestRestarted QuestRestarted;

public:

	/*
	* Spawn a quest actor! This will ensure the actors lifetime is managed automatically by the quest for you - when the quest ends, 
	* all of the spawned quest actors will be cleaned up from the level. 
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Dialogue")
	AActor* SpawnQuestActor(TSubclassOf<class AActor> ActorClass, const FTransform& ActorTransform);
	virtual AActor* SpawnQuestActor_Implementation(TSubclassOf<class AActor> ActorClass, const FTransform& ActorTransform);

	void AddState(class UQuestState* State);
	void AddBranch(class UQuestBranch* Branch);

	void RemoveState(class UQuestState* State);
	void RemoveBranch(class UQuestBranch* Branch);

	void SetQuestStartState(class UQuestState* State);

	UFUNCTION(BlueprintPure, Category = "Quests")
	UQuestState* GetQuestStartState() const {return QuestStartState;};

	UFUNCTION(BlueprintPure, Category = "Quests")
	TArray<UQuestState*> GetStates() const {return States;};

	UFUNCTION(BlueprintPure, Category = "Quests")
	TArray<UQuestBranch*> GetBranches() const {return Branches;};

	UFUNCTION(BlueprintPure, Category = "Quests")
	TArray<UQuestNode*> GetNodes() const;

	//Grab the completion of the quest 
	UFUNCTION(BlueprintPure, Category = "Quests")
	FORCEINLINE EQuestCompletion GetQuestCompletion() const { return QuestCompletion; };

	UFUNCTION(BlueprintPure, Category = "Quests")
	FORCEINLINE class APlayerController* GetOwningController() const {return OwningController;}

	UFUNCTION(BlueprintPure, Category = "Quests")
	FORCEINLINE class APawn* GetOwningPawn() const {return OwningPawn;}

	UFUNCTION(BlueprintPure, Category = "Quests")
	FORCEINLINE class UNarrativeComponent* GetOwningComp() const {return OwningComp;};

	UFUNCTION(BlueprintPure, Category = "Quests")
	FORCEINLINE bool IsTracked() const { return bTracked;};

	/**Return all players doing this quest if shared, or the owningcontroller if solo quest*/
	UFUNCTION(BlueprintPure, Category = "Quests")
	TArray<class APlayerController*> GetGroupMembers() const; 

};
