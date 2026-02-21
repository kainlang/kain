/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#pragma once

#include "QuestImportanceEnum.generated.h"

UENUM(BlueprintType)
enum EQuestImportance
{
	MAIN_QUEST				UMETA(DisplayName = "Main quest"),
	SIDE_QUEST		        UMETA(DisplayName = "Side quest"),
};
