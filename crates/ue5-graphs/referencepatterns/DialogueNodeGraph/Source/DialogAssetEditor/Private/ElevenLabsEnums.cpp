/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#include "ElevenLabsEnums.h"

int GetSampleRate(EElevenLabsOutputFormatEnum OutputFormat)
{
	switch(OutputFormat)
	{
	case pcm_8000:
		return 8000;

	case pcm_16000:
		return 16000;

	case pcm_22050:
		return 22050;

	case pcm_24000:
		return 24000;

	case pcm_44100:
		return 44100;
	}

	return 24000;
}