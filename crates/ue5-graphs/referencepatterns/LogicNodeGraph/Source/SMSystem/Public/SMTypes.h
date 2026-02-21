// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

#include "SMTypes.generated.h"

/**
 * Represents an optional bool which may not be set and is supported by the editor UI.
 */
UENUM()
enum class ESMOptionalBool : uint8
{
	Unset = 0,
	IsFalse UMETA(DisplayName="False"),
	IsTrue  UMETA(DisplayName="True")
};