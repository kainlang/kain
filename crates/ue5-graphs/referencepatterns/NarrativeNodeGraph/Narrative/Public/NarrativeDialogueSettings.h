// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "NarrativeDialogueSettings.generated.h"

/**
 * Runtime dialogue settings for narrative 
 */
UCLASS(config = Engine, defaultconfig)
class NARRATIVE_API UNarrativeDialogueSettings : public UObject
{
	GENERATED_BODY()
	
public:

	UNarrativeDialogueSettings();

	//Optional buffer of silence added to the end of dialogue lines
	UPROPERTY(EditAnywhere, config, Category = "Dialogue Settings", meta = (ClampMin = 0.01))
	float DialogueLineAudioSilence; 

	//How long should the text be displayed for at a minimum? Since default letters per minute is 25 this prevents a reply like "no" from being played too quickly
	UPROPERTY(EditAnywhere, config, Category = "Dialogue Settings", meta = (ClampMin=0.01))
	float MinDialogueTextDisplayTime;

	//If a dialogue doesn't have audio supplied, how long should the text be displayed on the screen for? Lower letters per minute means player gets more time 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, config, Category = "Dialogue Settings", meta = (ClampMin = 1))
	float LettersPerSecondLineDuration;

	//If there is only one player response available, should we autoselect it, regardless of whether bAutoSelect is checked?
	UPROPERTY(EditAnywhere, config, Category = "Dialogue Settings")
	bool bAutoSelectSingleResponse;

	//Expiremental - won't autoarrange old dialogues, and you'll need to move your nodes into the correct position yourself. 
	//Also makes dialogue nodes sort themselves from left to right instead of top to bottom

	UPROPERTY(EditAnywhere, BlueprintReadOnly, config, Category = "Graph Options")
	bool bEnableVerticalWiring;

	//Default speaker colors
	UPROPERTY(EditAnywhere, config, Category = "Graph Style")
	TArray<FLinearColor> SpeakerColors;
};
