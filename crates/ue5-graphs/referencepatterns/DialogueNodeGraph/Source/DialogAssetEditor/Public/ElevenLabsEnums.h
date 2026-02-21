/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#pragma once
#include "ElevenLabsEnums.generated.h"

UENUM(BlueprintType)
enum EElevenLabsOutputFormatEnum
{
	pcm_8000		UMETA(DisplayName = "PCM 8000"),
	pcm_16000		UMETA(DisplayName = "PCM 16000"),
	pcm_22050		UMETA(DisplayName = "PCM 22050"),
	pcm_24000		UMETA(DisplayName = "PCM 24000"),
	pcm_44100		UMETA(DisplayName = "PCM 44100 (require Pro tier or above submission)")
};

int GetSampleRate(EElevenLabsOutputFormatEnum OutputFormat);