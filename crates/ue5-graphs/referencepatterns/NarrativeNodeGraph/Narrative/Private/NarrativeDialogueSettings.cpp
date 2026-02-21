// Copyright Narrative Tools 2022. 

#include "NarrativeDialogueSettings.h"

UNarrativeDialogueSettings::UNarrativeDialogueSettings()
{
	LettersPerSecondLineDuration = 25.f;
	MinDialogueTextDisplayTime = 2.f;
	DialogueLineAudioSilence = 0.5f;
	bAutoSelectSingleResponse = false;
	bEnableVerticalWiring = true;

	SpeakerColors.Add(FLinearColor(0.036161, 0.115986, 0.265625, 1.000000));
	SpeakerColors.Add(FLinearColor(0.008496, 0.112847, 0.025310, 1.000000));
	SpeakerColors.Add(FLinearColor(0.194444, 0.021931, 0.075750, 1.000000));
	SpeakerColors.Add(FLinearColor(0.010000, 0.010000, 0.010000, 1.000000));
	SpeakerColors.Add(FLinearColor(0.010000, 0.010000, 0.010000, 1.000000));
	SpeakerColors.Add(FLinearColor(0.623529, 0.509607, 0.062539, 1.000000));
	SpeakerColors.Add(FLinearColor(0.100204, 0.363285, 0.581597, 1.000000));
	SpeakerColors.Add(FLinearColor(0.171875, 0.040824, 0.006940, 1.000000));
	SpeakerColors.Add(FLinearColor(0.300000, 0.300000, 0.300000, 1.000000));
	SpeakerColors.Add(FLinearColor(0.744792, 0.339469, 0.673176, 1.000000));
}
