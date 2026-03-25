/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */
#pragma once

#include "QuestState.generated.h"

USTRUCT()
struct FQuestTaskState
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TMap<class UDialogueMasterTask*, int> TaskStatesMap;
};

USTRUCT()
struct FQuestState
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName CurrentStep;

	UPROPERTY()
	TMap<FName, FQuestTaskState> TaskStates;

	FQuestState(class UQuestAsset* quest);
	FQuestState();
};
