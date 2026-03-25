/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#pragma once
#include "QuestState.h"
#include "GameFramework/SaveGame.h"
#include "Kismet/GameplayStatics.h"

#include "DialogueMasterSaveData.generated.h"


UCLASS()
class DIALOGASSETEDITORRUNTIME_API UDialogueMasterSaveData : public USaveGame
{
	GENERATED_BODY()

public:
	UDialogueMasterSaveData();

	UPROPERTY(VisibleAnywhere, Category="Dialogue Master - Switches and counters")
	TMap<FName, bool> SwitchesMap;

	UPROPERTY(VisibleAnywhere, Category="Dialogue Master - Switches and counters")
	TMap<FName, int> CountersMap;

	UPROPERTY(VisibleAnywhere, Category="Dialogue Master - Quests")
	TArray<class UQuestAsset*> Quests;

	UPROPERTY(VisibleAnywhere, Category="Dialogue Master - Quests")
	TMap<class UQuestAsset*, FQuestState> QuestStates;

	UPROPERTY(VisibleAnywhere, Category="Dialogue Master - Quests")
	TArray<class UQuestAsset*> TrackedQuests;
	
};


inline UDialogueMasterSaveData::UDialogueMasterSaveData()
{
	
}

