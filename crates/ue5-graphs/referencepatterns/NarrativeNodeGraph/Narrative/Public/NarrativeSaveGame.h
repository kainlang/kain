// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "Quest.h"
#include "GameFramework/SaveGame.h"
#include "Templates/SubclassOf.h"
#include "NarrativeSaveGame.generated.h"

USTRUCT()
struct NARRATIVE_API FSavedQuestBranch
{
	GENERATED_BODY()

	FSavedQuestBranch(){};
	FSavedQuestBranch(const FName& InBranchID, const TArray<int32>& InTasksProgress) : BranchID(InBranchID), TasksProgress(InTasksProgress){};

	//The ID of the branch 
	UPROPERTY(SaveGame)
	FName BranchID;

	//All we need to save a branch is remember what progress the tasks had. Tasks always have the same order, so just save their progress 
	UPROPERTY(SaveGame)
	TArray<int32> TasksProgress;
};

USTRUCT()
struct NARRATIVE_API FNarrativeSavedQuest
{
	GENERATED_BODY()

public:

	FNarrativeSavedQuest() {};

	//The class of the quest that was active
	UPROPERTY(SaveGame)
	TSubclassOf<class UQuest> QuestClass;

	//The state the quest was at when the player saved 
	UPROPERTY(SaveGame)
	FName CurrentStateID;

	//The saved branch data
	UPROPERTY(SaveGame)
	TArray<FSavedQuestBranch> QuestBranches;

	UPROPERTY(SaveGame)
	TArray<FName> ReachedStateNames;

};

/**
 * Narrative Savegame object 
 */
UCLASS()
class NARRATIVE_API UNarrativeSaveGame : public USaveGame
{
	GENERATED_BODY()

public:

	UPROPERTY(VisibleAnywhere, Category = "Save")
	TArray<FNarrativeSavedQuest> SavedQuests;

	UPROPERTY(VisibleAnywhere, Category = "Save")
	TMap<FString, int32> MasterTaskList;

};
