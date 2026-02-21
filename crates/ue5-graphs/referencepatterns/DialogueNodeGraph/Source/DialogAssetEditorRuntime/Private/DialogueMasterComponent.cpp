/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#include "DialogueMasterComponent.h"

#include "DialogPlayer.h"
#include "DialogAsset.h"
#include "DialogueMasterSaveData.h"
#include "QuestAsset.h"
#include "QuestState.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "AssetRegistry/AssetData.h"
#include "DialogueMasterStatistic.h"
#include "DialogueMasterTask.h"
#include "QuestNodeInfo.h"

static const FString PLAY_DIALOGUE_NODE_STAT_NAME("Play Dialogue Node");

UDialogueMasterComponent::UDialogueMasterComponent()
{
	// Attach dialogues event handlers :
	OnDialogueBegin.AddDynamic(this, &UDialogueMasterComponent::DialogueBegin);
	OnDialogueEnd.AddDynamic(this, &UDialogueMasterComponent::DialogueEnd);
	OnDialogueLinePlay.AddDynamic(this, &UDialogueMasterComponent::DialogueLinePlay);
	OnDialogueLineSkipped.AddDynamic(this, &UDialogueMasterComponent::DialogueLineSkipped);
	OnDialogueLineEnd.AddDynamic(this, &UDialogueMasterComponent::DialogueLineEnd);
	OnDialogueCancelled.AddDynamic(this, &UDialogueMasterComponent::DialogueCancelled);
	
	// Attach switches & counters handlers :
	OnSwitchValueChanged.AddDynamic(this, &UDialogueMasterComponent::SwitchValueChanged);
	OnCounterValueChanged.AddDynamic(this, &UDialogueMasterComponent::CounterValueChanged);
	
	// Attach quests event handlers :
	OnQuestEnteredNewStep.AddDynamic(this, &UDialogueMasterComponent::QuestEnteredNewStep);
	OnQuestExitStep.AddDynamic(this, &UDialogueMasterComponent::QuestExitStep);
	OnQuestTaskProgressUpdated.AddDynamic(this, &UDialogueMasterComponent::QuestTaskProgressUpdated);
	OnQuestTaskCompleted.AddDynamic(this, &UDialogueMasterComponent::QuestTaskCompleted);
	OnQuestEnd.AddDynamic(this, &UDialogueMasterComponent::QuestEnd);
	OnQuestTracked.AddDynamic(this, &UDialogueMasterComponent::QuestTracked);
	OnQuestUntracked.AddDynamic(this, &UDialogueMasterComponent::QuestUntracked);

	// Attach statistics event handlers :
	OnStatisticUpdated.AddDynamic(this, &UDialogueMasterComponent::StatisticUpdated);

	// Attach save system event handlers :
	OnLoadGameStateBegin.AddDynamic(this, &UDialogueMasterComponent::LoadGameStateBegin);
	OnSaveGameStateBegin.AddDynamic(this, &UDialogueMasterComponent::SaveGameStateBegin);
	
	OnGameStateLoaded.AddDynamic(this, &UDialogueMasterComponent::GameStateLoaded);
	OnGameStateSaved.AddDynamic(this, &UDialogueMasterComponent::GameStateSaved);

	OnLoadGameStateCompleted.AddDynamic(this, &UDialogueMasterComponent::LoadGameStateCompleted);
	OnSaveGameStateCompleted.AddDynamic(this, &UDialogueMasterComponent::SaveGameStateCompleted);
}

void UDialogueMasterComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UDialogueMasterComponent::SwitchValueChanged(FName SwitchName, bool NewValue)
{
}

void UDialogueMasterComponent::CounterValueChanged(FName CounterName, int PreviousValue, int NewValue)
{
}

const TMap<FName, bool>& UDialogueMasterComponent::GetSwitchesMapRef() const
{
	return SwitchesMap;
}

void UDialogueMasterComponent::GetSwitchesList(TArray<FName> & out) const
{
	SwitchesMap.GetKeys(out);
}

const TMap<FName, int>& UDialogueMasterComponent::GetCountersMapRef() const
{
	return CountersMap;
}

void UDialogueMasterComponent::GetCountersList(TArray<FName>& out) const
{
	CountersMap.GetKeys(out);
}

void UDialogueMasterComponent::PlayDialogue(UDialogAsset* Dialogue, AActor* NPC, AActor* Player, UObject* UserObject, TArray<AActor*> NPCManualReferences)
{
	if(!IsValid(Dialogue)) return;

	bool bHasFoundActor = false;
	TArray<AActor*> foundActors;
	UGameplayStatics::GetAllActorsOfClassWithTag(GetWorld(), ADialogPlayer::StaticClass(), "MainDialoguePlayer", foundActors);

	if(foundActors.Num() > 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("There is more than one DialogPlayer with tag MainDialoguePlayer found ! Check your level. There must be only one."));
	}
	
	if(foundActors.Num() > 0)
	{
		ADialogPlayer * player = Cast<ADialogPlayer>(foundActors[0]);
		if(player == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to cast the DialogPlayer actor ; make sure you have added it correctly to your level !"));
			return;
		}

		player->PlayDialog(Dialogue, NPC, Player, UserObject, NPCManualReferences);
		bHasFoundActor = true;
	}

	if(!bHasFoundActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to find the main DialogPlayer actor ; make sure you have added it to your level !"));
	}
}

void UDialogueMasterComponent::PlaySubDialogue(UDialogAsset* Dialogue)
{
	if(!IsValid(Dialogue)) return;
	
	ADialogPlayer * player = Cast<ADialogPlayer>(UGameplayStatics::GetActorOfClass(GetWorld(), ADialogPlayer::StaticClass()));
	if(player == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to find the DialogPlayer actor ; make sure you have added it to your level !"));
		return;
	}

	player->PlaySubDialog(Dialogue);
}

bool UDialogueMasterComponent::DialogueMaster_SaveState(FString SlotName, int UserIndex)
{
	bool bResult = false;
	OnSaveGameStateBegin.Broadcast(SlotName, UserIndex);
	if(!DebugMode)
	{
		UDialogueMasterSaveData * saveData = Cast<UDialogueMasterSaveData>(UGameplayStatics::CreateSaveGameObject(UDialogueMasterSaveData::StaticClass()));

		if(saveData != nullptr)
		{
			saveData->CountersMap = CountersMap;
			saveData->SwitchesMap = SwitchesMap;
			saveData->Quests = QuestsList;
			saveData->TrackedQuests = TrackedQuests;

			for(UQuestAsset * quest : QuestsList)
			{
				saveData->QuestStates.Add(quest, FQuestState(quest));
			}
		
			bResult = UGameplayStatics::SaveGameToSlot(saveData, SlotName, UserIndex);
		}
	}
	OnGameStateSaved.Broadcast(SlotName, UserIndex, bResult);
	
	return bResult;
}

bool UDialogueMasterComponent::DialogueMaster_LoadState(FString SlotName, int UserIndex)
{
	// The save file must exists before loading anything!
	if(!UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
	{
		return false;
	}

	bool bResult = false;

	// The save file exists. Try to load it:
	OnLoadGameStateBegin.Broadcast(SlotName, UserIndex);
	UDialogueMasterSaveData * saveData = nullptr;
	if(!DebugMode)
	{
		saveData = Cast<UDialogueMasterSaveData>(UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex));

		if(saveData != nullptr)
		{
			// Untrack all tracked quests ...
			TArray<UQuestAsset*> TrackedQuestsCopy = TrackedQuests;
			for(UQuestAsset * quest : TrackedQuestsCopy)
			{
				InternalUntrackQuest(quest, false);
			}

			// Disable active tasks ...
			for(UQuestAsset * quest : QuestsList)
			{
				TArray<UQuestTaskListNodeInfo*> taskLists = quest->GetStepTasksList(quest->GetCurrentStepID());
				for(UQuestTaskListNodeInfo * taskList : taskLists)
				{
					for(UDialogueMasterTask * task : taskList->Tasks)
					{
						task->EndTask();
					}
				}
			}

			// Load data...
			this->SwitchesMap = saveData->SwitchesMap;
			this->CountersMap = saveData->CountersMap;
			this->QuestsList = saveData->Quests;
			this->TrackedQuests = saveData->TrackedQuests;

			// Restore quests states :
			for(UQuestAsset * quest : QuestsList)
			{
				if(saveData->QuestStates.Contains(quest))
				{
					FQuestState state = saveData->QuestStates[quest];
					quest->RestoreFromState(&state, this, Cast<APlayerController>(GetOwner()));
				}
			}

			// Restore tracked quests :
			for(UQuestAsset * quest : TrackedQuests)
			{
				OnQuestTracked.Broadcast(quest);
			}
			bResult = true;
		}
	}
	
	OnGameStateLoaded.Broadcast(SlotName, UserIndex, saveData, bResult);
	return bResult;
}

bool UDialogueMasterComponent::DialogueMaster_DeleteSave(FString SlotName, int UserIndex)
{
	// The save file must exists before deleting anything!
	if(!UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
	{
		return false;
	}

	return UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);
}

void UDialogueMasterComponent::DialogueBegin(ADialogPlayer* DialoguePlayerRef, UDialogAsset* Dialogue, UObject* UserObject)
{
}

void UDialogueMasterComponent::DialogueEnd(ADialogPlayer* DialoguePlayerRef, UDialogAsset* Dialogue, UObject* UserObject)
{
	
}

void UDialogueMasterComponent::DialogueLinePlay(ADialogPlayer* DialoguePlayerRef, UDialogAsset* Dialogue, const FText& ActorName,
	AActor* ActorRef, UDialogNodeInfo* Line, float Duration, UObject* UserObject)
{
}

void UDialogueMasterComponent::DialogueLineSkipped(ADialogPlayer* DialoguePlayerRef, UDialogAsset* Dialogue,
	UObject* UserObject)
{
}

void UDialogueMasterComponent::DialogueLineEnd(ADialogPlayer* DialoguePlayerRef, UDialogAsset* Dialogue, const FText& ActorName,
                                               AActor* ActorRef, const UDialogNodeInfo* Line, const TArray<FDialogSentence>& AnswerList, UObject* UserObject)
{
	UDialogueMasterStatistic * playDialogueNodeStat = GetPlayDialogueNodeStatisticInstance();
	UpdateStatistic(playDialogueNodeStat, Line->UserId.ToString(), 1);
}

void UDialogueMasterComponent::DialogueCancelled(ADialogPlayer* DialoguePlayerRef, UDialogAsset* Dialogue,
	UObject* UserObject)
{
}

UDialogueMasterStatistic* UDialogueMasterComponent::GetPlayDialogueNodeStatisticInstance()
{
	if(_cachedPlayDialogueNodeStatistic != nullptr)
		return _cachedPlayDialogueNodeStatistic;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> AssetData;

	AssetRegistryModule.Get().GetAssetsByClass(UDialogueMasterStatistic::StaticClass()->GetClassPathName(), AssetData);
	
	for (FAssetData& StatisticData : AssetData)
	{
		if (UDialogueMasterStatistic* stat = Cast<UDialogueMasterStatistic>(StatisticData.GetAsset()))
		{
			if (stat->StatName.Equals(PLAY_DIALOGUE_NODE_STAT_NAME, ESearchCase::IgnoreCase))
			{
				_cachedPlayDialogueNodeStatistic = stat;
				break;
			}
		}
	}

	if(_cachedPlayDialogueNodeStatistic == nullptr)
	{
		UE_LOG(LogTemp, Fatal, TEXT("The Play Dialogue Node statistic cannot be found ! Make sure you haven't deleted it !"));
	}
	
	return _cachedPlayDialogueNodeStatistic;
}

void UDialogueMasterComponent::LoadGameStateBegin(const FString& SlotName, const int32 UserIndex)
{
}

void UDialogueMasterComponent::SaveGameStateBegin(const FString& SlotName, const int32 UserIndex)
{
}

void UDialogueMasterComponent::LoadGameStateCompleted(const FString& SlotName, const int32 UserIndex)
{
}

void UDialogueMasterComponent::SaveGameStateCompleted(const FString& SlotName, const int32 UserIndex)
{
}

void UDialogueMasterComponent::GameStateLoaded(const FString& SlotName, const int32 UserIndex,
                                               USaveGame* LoadedGameData, bool Success)
{
}

void UDialogueMasterComponent::GameStateSaved(const FString& SlotName, const int32 UserIndex, bool bSuccess)
{
}

// ISwitchAndCounterPrerequisitesInterface implementation :
bool UDialogueMasterComponent::getSwitchValue_Implementation(FName switchName)
{
	if(SwitchesMap.Contains(switchName))
		return SwitchesMap[switchName];
	
	return false;
}

int UDialogueMasterComponent::getCounterValue_Implementation(FName counterName)
{
	if(CountersMap.Contains(counterName))
		return CountersMap[counterName];
	
	return 0;
}

void UDialogueMasterComponent::setSwitchValue_Implementation(FName switchName, bool value)
{
	bool previousValue = false;
	if(SwitchesMap.Contains(switchName))
		previousValue = SwitchesMap[switchName];
	
	SwitchesMap.Add(switchName, value);

	// Switch value changed !
	if(previousValue != value)
	{
		OnSwitchValueChanged.Broadcast(switchName, value);
	}
}

void UDialogueMasterComponent::setCounterValue_Implementation(FName counterName, int value)
{
	int previousValue = 0;
	if(CountersMap.Contains(counterName))
		previousValue = CountersMap[counterName];
	
	CountersMap.Add(counterName, value);

	// Counter value changed !
	if(previousValue != value)
	{
		OnCounterValueChanged.Broadcast(counterName, previousValue, value);
	}
}
// ------------------------------------------------------------


/*************************************************
 * Quest system
 *************************************************/
UQuestAsset* UDialogueMasterComponent::GetQuestInstance(UQuestAsset* Quest) const
{
	if(IsValid(Quest))
	{
		for(UQuestAsset * questInstance : QuestsList)
		{
			if(questInstance && questInstance == Quest)
			{
				return questInstance;
			}
		}
	}

	return nullptr;
}

const TArray<UQuestAsset*>& UDialogueMasterComponent::GetQuestList() const
{
	return QuestsList;
}

UQuestAsset* UDialogueMasterComponent::StartQuest(UQuestAsset* Quest)
{
	if(IsValid(Quest))
	{
		// Verify if the quest is not already started or done :
		UQuestAsset* questInstance = GetQuestInstance(Quest);
		if(IsValid(questInstance))
			return nullptr;

		questInstance = Quest;
		if(questInstance->Initialize(this, Cast<APlayerController>(GetOwner())))
		{
			QuestsList.Add(questInstance);

			if(questInstance->QuestImportance == MAIN_QUEST && AutoTrackMainQuest)
			{
				TrackQuest(questInstance);
			}

			if(questInstance->QuestImportance == SIDE_QUEST && AutoTrackSideQuest)
			{
				TrackQuest(questInstance);
			}
			
			return questInstance;
		}
	}
	return nullptr;
}

void UDialogueMasterComponent::IsQuestAtStep(UQuestAsset* Quest, FName StepRequired, bool& QuestFound,
	bool& QuestAtRequiredStep) const
{
	QuestAtRequiredStep = false;
	QuestFound = false;
	
	UQuestAsset * FoundQuest = GetQuestInstance(Quest);
	QuestFound = IsValid(FoundQuest);
	if(QuestFound)
		QuestAtRequiredStep = FoundQuest->GetCurrentStepID() == StepRequired;
}

void UDialogueMasterComponent::IsQuestSucceeded(UQuestAsset* Quest, bool& QuestFound, bool& Succeeded) const
{
	QuestFound = false;
	Succeeded = false;

	UQuestAsset * FoundQuest = GetQuestInstance(Quest);
	QuestFound = IsValid(FoundQuest);
	if(QuestFound)
		Succeeded = FoundQuest->IsQuestSucceeded();
}

void UDialogueMasterComponent::IsQuestFailed(UQuestAsset* Quest, bool& QuestFound, bool& Failed) const
{
	QuestFound = false;
	Failed = false;

	UQuestAsset * FoundQuest = GetQuestInstance(Quest);
	QuestFound = IsValid(FoundQuest);
	if(QuestFound)
		Failed = FoundQuest->IsQuestFailed();
}

void UDialogueMasterComponent::IsQuestFinished(UQuestAsset* Quest, bool& QuestFound, bool& Finished,
	bool &WithSuccess) const
{
	QuestFound = false;
	Finished = false;
	WithSuccess = false;

	UQuestAsset * FoundQuest = GetQuestInstance(Quest);
	QuestFound = IsValid(FoundQuest);
	if(QuestFound)
	{
		EStepType currentStep = FoundQuest->GetCurrentStepType();
		Finished = currentStep != NORMAL;
		WithSuccess = currentStep == QUEST_SUCCEEDED;
	}
}

void UDialogueMasterComponent::TrackQuest(UQuestAsset* Quest)
{
	if(QuestsList.Contains(Quest))
	{
		if(!TrackedQuests.Contains(Quest))
		{
			TrackedQuests.Add(Quest);
			OnQuestTracked.Broadcast(Quest);
		}
	}
}

void UDialogueMasterComponent::UntrackQuest(UQuestAsset* Quest)
{
	InternalUntrackQuest(Quest, false);
}

void UDialogueMasterComponent::InternalUntrackQuest(UQuestAsset* Quest, bool becauseOfQuestEnd)
{
	if(TrackedQuests.Contains(Quest))
	{
		TrackedQuests.Remove(Quest);
		OnQuestUntracked.Broadcast(Quest, becauseOfQuestEnd);
	}
}

void UDialogueMasterComponent::IsQuestTracked(UQuestAsset* Quest, bool &Tracked) const
{
	Tracked = TrackedQuests.Contains(Quest);
}

void UDialogueMasterComponent::QuestEnteredNewStep(UQuestAsset* Quest, UQuestStepNodeInfo* NewStep)
{
}

void UDialogueMasterComponent::QuestExitStep(UQuestAsset* Quest, UQuestStepNodeInfo* ExitedStep)
{
	
}

void UDialogueMasterComponent::QuestTaskProgressUpdated(UQuestAsset* Quest, UQuestStepNodeInfo* Step,
                                                        UDialogueMasterTask* Task, int PreviousValue, int NewValue)
{
}

void UDialogueMasterComponent::QuestTaskCompleted(UQuestAsset* Quest, UQuestStepNodeInfo* Step,
	UDialogueMasterTask* Task)
{
	
}

void UDialogueMasterComponent::QuestEnd(UQuestAsset* Quest, EStepType EndType)
{
	InternalUntrackQuest(Quest, true);
}

void UDialogueMasterComponent::QuestTracked(UQuestAsset* Quest)
{
}

void UDialogueMasterComponent::QuestUntracked(UQuestAsset* Quest, bool UntrackedBecauseOfQuestEnd)
{
}

void UDialogueMasterComponent::UpdateStatistic(UDialogueMasterStatistic* StatisticAsset, const FString& CountedEntity,
                                               const int Quantity)
{
	if(IsValid(StatisticAsset))
	{
		FString Key = StatisticAsset->GenerateStatKey(CountedEntity);
		FName CounterName = FName(Key);
		int CurrentValue = this->getCounterValue_Implementation(CounterName);
		int NewValue = CurrentValue + Quantity;
		this->setCounterValue_Implementation(CounterName, NewValue);
		OnStatisticUpdated.Broadcast(StatisticAsset, CountedEntity, CurrentValue, NewValue);
	}
}

void UDialogueMasterComponent::SetStatisticValue(UDialogueMasterStatistic* StatisticAsset, const FString& CountedEntity,
	const int NewValue)
{
	if(IsValid(StatisticAsset))
	{
		FString Key = StatisticAsset->GenerateStatKey(CountedEntity);
		FName CounterName = FName(Key);
		int CurrentValue = this->getCounterValue_Implementation(CounterName);
		this->setCounterValue_Implementation(CounterName, NewValue);
		OnStatisticUpdated.Broadcast(StatisticAsset, CountedEntity, CurrentValue, NewValue);
	}
}

int UDialogueMasterComponent::GetStatisticValue(UDialogueMasterStatistic* StatisticAsset, const FString& CountedEntity)
{
	if(IsValid(StatisticAsset))
	{
		FString Key = StatisticAsset->GenerateStatKey(CountedEntity);
		FName CounterName = FName(Key);
		return this->getCounterValue_Implementation(CounterName);
	}

	return 0;
}

void UDialogueMasterComponent::StatisticUpdated(UDialogueMasterStatistic* Statistic, FString CountedEntityName, int PreviousValue, int NewValue)
{
	
}
