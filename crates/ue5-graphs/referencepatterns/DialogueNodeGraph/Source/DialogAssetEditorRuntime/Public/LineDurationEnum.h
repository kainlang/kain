/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
*    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#pragma once

#include "LineDurationEnum.generated.h"

UENUM(BlueprintType)
enum class ELineDurationType : uint8
{
	DEFAULT				UMETA(DisplayName = "Default behavior (if text only, take the duration. If using sound, take sound duration)"),
	CUSTOM_DURATION		UMETA(DisplayName = "Custom duration"),
	NEVER				UMETA(DisplayName = "Never (wait until player skip)")
};
