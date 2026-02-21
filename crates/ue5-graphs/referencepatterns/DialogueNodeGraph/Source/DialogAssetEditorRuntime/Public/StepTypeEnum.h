/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#pragma once

#include "StepTypeEnum.generated.h"

UENUM(BlueprintType)
enum EStepType
{
	NORMAL				UMETA(DisplayName = "Normal step"),
	QUEST_SUCCEEDED		UMETA(DisplayName = "Quest succeeded"),
	QUEST_FAILED		UMETA(DisplayName = "Quest failed")
};
