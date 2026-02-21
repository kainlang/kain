/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#pragma once

#include "CameraShot.h"

#include "ActorInfo.generated.h"


USTRUCT(BlueprintType)
struct FElevenLabsParameters
{
	GENERATED_USTRUCT_BODY()

public:

	/**
	 The ElevenLabs voice ID for this actor.
	 */
	UPROPERTY(EditAnywhere, Category="Voice parameters")
	FString SelectedVoice; 
	
	
	//UPROPERTY(EditAnywhere, Category="Voice parameters")
	//FString VoiceID;

	UPROPERTY(EditAnywhere, Category="Voice parameters")
	FString Model = "eleven_multilingual_v2";

	UPROPERTY(EditAnywhere, Category="Voice parameters", meta=(ClampMin=0.7, ClampMax=1.2, UIMin=0.7, UIMax=1.2))
	double Speed = 1;

	UPROPERTY(EditAnywhere, Category="Voice parameters", meta=(ClampMin=0, ClampMax=100, UIMin=0, UIMax=100))
	double Stability = 50;

	UPROPERTY(EditAnywhere, Category="Voice parameters", meta=(ClampMin=0, ClampMax=100, UIMin=0, UIMax=100))
	double Similarity = 75;

	UPROPERTY(EditAnywhere, Category="Voice parameters", meta=(ClampMin=0, ClampMax=100, UIMin=0, UIMax=100))
	double StyleExaggeration = 0;

	UPROPERTY(EditAnywhere, Category="Voice parameters")
	bool SpeakerBoost = true;
	
	void ResetToDefaultValues()
	{
		Speed = 1;
		Stability = 50;
		Similarity = 75;
		StyleExaggeration = 0;
		SpeakerBoost = true;
	}
};

USTRUCT(BlueprintType)
struct FElevenLabsParametersOverride
{
	GENERATED_USTRUCT_BODY()

public:
	
	UPROPERTY(EditAnywhere, Category="Voice parameters", meta=(ClampMin=0.7, ClampMax=1.2, UIMin=0.7, UIMax=1.2))
	double Speed = 1;

	UPROPERTY(EditAnywhere, Category="Voice parameters", meta=(ClampMin=0, ClampMax=100, UIMin=0, UIMax=100))
	double Stability = 50;

	UPROPERTY(EditAnywhere, Category="Voice parameters", meta=(ClampMin=0, ClampMax=100, UIMin=0, UIMax=100))
	double Similarity = 75;

	UPROPERTY(EditAnywhere, Category="Voice parameters", meta=(ClampMin=0, ClampMax=100, UIMin=0, UIMax=100))
	double StyleExaggeration = 0;

	UPROPERTY(EditAnywhere, Category="Voice parameters")
	bool SpeakerBoost = true;
	
	void ResetToDefaultValues()
	{
		Speed = 1;
		Stability = 50;
		Similarity = 75;
		StyleExaggeration = 0;
		SpeakerBoost = true;
	}
};



UENUM(BlueprintType)
enum ENPCLinkingMethod
{
	ActorTag				UMETA(DisplayName = "Link actor with tags"),
	ManualReferencing		UMETA(DisplayName = "Link actor manually (pass reference to the PlayDialogue method)")
};

USTRUCT(BlueprintType)
struct FActorInfo
{
	GENERATED_USTRUCT_BODY()

	inline static TArray<FColor> actorsColor;
	inline static bool isInitialized = false;
	inline static int counter = 0;
	static void InitializeColorList() {
		if(!isInitialized)
		{
			isInitialized = true;
			actorsColor.Add(FColor(255, 231, 0));		// Yellow
			actorsColor.Add(FColor(0, 111, 255));		// Light Blue
			actorsColor.Add(FColor(0, 255, 142));		// Light Green
			actorsColor.Add(FColor(0, 244, 255));		// Cyan
			actorsColor.Add(FColor(255, 0, 51));		// Red
			actorsColor.Add(FColor(255, 0, 226));		// Pink / purple
			actorsColor.Add(FColor(255, 154, 0));		// Orange / gold
			actorsColor.Add(FColor(255, 253, 251));	// Grey
			actorsColor.Add(FColor(1, 1, 1));			// Black
			actorsColor.Add(FColor(0, 12, 255));		// Dark blue
		}
	}

	static FColor getColor()
	{
		InitializeColorList();
		return actorsColor[FMath::RandRange(0, actorsColor.Num() - 1)];
	}

	FText getTagToSearch()
	{
		FText tagToSearch = (UniqueIdentifierOverride.IsEmpty() ?
							ActorIdentifier
							:
							UniqueIdentifierOverride);

		return tagToSearch;
	}

	/**
	 * This is the name of the actor, it will be displayed in the subtitle.
	 * If UniqueIdentifierOverride is not set, this will be used to find the
	 * actor with this tag.
	 */
	UPROPERTY(EditAnywhere, Category="Actor info", DisplayName="Actor Name")
	FText ActorIdentifier;

	/**
	 * If it is set ; this will override the ActorIdentifier and it will be used to search
	 * the actor instance with this tag instead of using the ActorIdentifier.
	 * It allows you to have multiple instances of the same NPC, with the same name, but
	 * still identify correctly the Actor to shot with the camera.
	 */
	UPROPERTY(EditAnywhere, Category="Actor info", DisplayName="Actor Unique Identifier")
	FText UniqueIdentifierOverride;

	UPROPERTY(EditAnywhere, Category="Actor info")
	FColor ActorNodeColor = FColor::Black;

	UPROPERTY(EditAnywhere, Category="Actor info")
	FCameraShot DefaultShot;

	/**
	 * The linking method. The default linking method is by tag.
	 * The other method is by getting manual reference (that you can pass
	 * to the dialog player through the PlayDialog method).
	 * If an actor is set to ManualReferencing method, you must pass it's
	 * reference to the PlayDialog method (and the order have to match
	 * with the order in the dialog asset). If you mix the two methods
	 * the actor using actor tags must not be referenced in the manual
	 * reference actor array.
	 * The manual reference method is only recommended for case where
	 * you cannot use the tag method (ex: when you are setting up
	 * environmental dialogue on random procedurally generated NPCs).
	 * If you use the manual reference on the Player actor, the Player
	 * parameter reference will be used.
	 */
	UPROPERTY(EditAnywhere, Category="Actor info")
	TEnumAsByte<ENPCLinkingMethod> NPCLinkingMethod = ActorTag;

	/**
	 The ElevenLabs parameters for this actor.
	 */
	UPROPERTY(EditAnywhere, Category="Actor info")
	FElevenLabsParameters ElevenLabsParameters;
	
	FActorInfo()
		: ActorNodeColor(FColor::Black)
	{
		if(ActorNodeColor == FColor(0, 0, 0, 0))
			ActorNodeColor = getColor();
	}
};