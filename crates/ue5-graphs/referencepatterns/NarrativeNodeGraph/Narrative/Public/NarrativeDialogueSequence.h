// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "LevelSequencePlayer.h"
#include "DialogueSM.h"
#include <MovieSceneSequencePlayer.h>
#include <CineCameraSettings.h>
#include "NarrativeDialogueSequence.generated.h"

//If we're using an anchored shot, defines the location of the anchor
UENUM(BlueprintType)
enum class EAnchorOriginRule : uint8
{
	AOR_Disabled UMETA(DisplayName = "Disabled", ToolTip = "The shot won't be anchored/track, it is disabled."),
	AOR_ConversationCenter UMETA(DisplayName = "Conversation Center", ToolTip = "The shot will be anchored to the middle point between all the speakers"),
	AOR_Speaker UMETA(DisplayName="Speaker", ToolTip = "The shots anchor will be the person saying the line."),
	AOR_Listener UMETA(DisplayName = "Listener", ToolTip = "The shots anchor will be the person the listener the line is directed at."),
	AOR_Custom UMETA(DisplayName = "Custom", ToolTip = "The shots anchor will use the Custom Avatar ID typed in.")
};

//If we're using an anchored shot, defines the rotation of the anchor
UENUM(BlueprintType)
enum class EAnchorRotationRule : uint8
{
	ARR_AnchorActorForwardVector UMETA(DisplayName = "Anchor Avatar", ToolTip = "The anchor rotation will use the forward vector of the avatar we're using."),
	ARR_Conversation UMETA(DisplayName = "Conversation Direction", ToolTip = "Anchor rot will be the offset vector from the speaker to the listener"),
};

//Defines how the tracking should work
UENUM(BlueprintType)
enum class EShotTrackingRule : uint8
{
	STR_Disabled UMETA(DisplayName = "Disabled", ToolTip = "The shot wont track, it is disabled."),
	STR_Speaker UMETA(DisplayName="Speaker", ToolTip = "The shot will track the person saying the line."),
	STR_Listener UMETA(DisplayName = "Listener", ToolTip = "The shot will track the person the listener the line is directed at."),
	STR_Custom UMETA(DisplayName = "Custom", ToolTip = "The shot will track the Custom Avatar ID typed in.")
};

USTRUCT(BlueprintType)
struct FShotTrackingSettings
{
	GENERATED_BODY()

	public:

	FShotTrackingSettings()
	{
		AvatarToTrack = EShotTrackingRule::STR_Speaker;
		bUpdateTrackingEveryFrame = false;
		UpdateTrackingInterpSpeed = 0.5f;
		//We actually want the camera to track the face, which is approx 10 units in front of the head bone. Nudge it forward! 
		TrackBoneNudge = FVector(10.f, 0.f, 0.f);
	}

	/**What avatar should we track */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracking/Focus")
	EShotTrackingRule AvatarToTrack;

	/** If TrackedAvatar is custom, this is the ID of the avatar to use as the override */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracking/Focus", meta = (EditCondition = "AvatarToTrack == EShotTrackingRule::STR_Custom", EditConditionHides))
	FName TrackedAvatarCustomID;

	/** Offset the tracking in actors local space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracking/Focus", meta = (EditCondition = "AvatarToTrack != EShotTrackingRule::STR_Disabled", EditConditionHides))
	FVector TrackBoneNudge;

	/**If true the camera will update the tracking every frame. This is important if your character head is moving around a lot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracking/Focus", meta = (EditCondition = "AvatarToTrack != EShotTrackingRule::STR_Disabled", EditConditionHides))
	bool bUpdateTrackingEveryFrame;

	/** If Update Tracking every frame is turned on, how fast should we interp towards the head as it moves around  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracking/Focus", meta = (EditCondition = "AvatarToTrack != EShotTrackingRule::STR_Disabled", EditConditionHides))
	float UpdateTrackingInterpSpeed;
};

/**
 * Encapsulates a Level Sequence, along with all extra data needed to play that level sequence in the context of a dialogue 
 */
UCLASS(Blueprintable, EditInlineNew, AutoExpandCategories = ("Default"))
class NARRATIVE_API UNarrativeDialogueSequence : public UObject
{
	GENERATED_BODY()

	protected:

		UNarrativeDialogueSequence();
	
	
		// Allows the Object to get a valid UWorld from it's outer.
		virtual UWorld* GetWorld() const override
		{
			if (HasAllFlags(RF_ClassDefaultObject))
			{
				// If we are a CDO, we must return nullptr instead of calling Outer->GetWorld() to fool UObject::ImplementsGetWorld.
				return nullptr;
			}

			UObject* Outer = GetOuter();

			while (Outer)
			{
				UWorld* World = Outer->GetWorld();
				if (World)
				{
					return World;
				}

				Outer = Outer->GetOuter();
			}

			return nullptr;
		}


		//The sequences to use - one will be selected at random 
		UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sequence")
		FText FriendlyShotName; 
		
		//The sequences to use - one will be selected at random 
		UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sequence")
		TArray<class ULevelSequence*> SequenceAssets;

		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sequence")
		FMovieSceneSequencePlaybackSettings PlaybackSettings;

		/** Controls the crop settings. */
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Settings")
		struct FPlateCropSettings CropSettings;

		/** If narrative tries playing this sequence but it already started playing it from an earlier node, should we restart the shot or just let the existing one continue? */
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sequence")
		uint32 bShouldRestart : 1;

		/** Sequence origin will be relative to the selected item */
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchoring")
		EAnchorOriginRule AnchorOriginRule;

		/**Allows you to nudge the shot upwards, downwards, etc. Extra offset applied to shot transform. Applied in speakers transform space. */
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchoring", meta = (EditCondition = "AnchorOriginRule != EAnchorOriginRule::AOR_Disabled", EditConditionHides))
		FVector AnchorOriginNudge;

		/**Sequence rotation will be relative to the selected item*/
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchoring", meta = (EditCondition = "AnchorOriginRule != EAnchorOriginRule::AOR_Disabled", EditConditionHides))
		EAnchorRotationRule AnchorRotationRule;

		/** If AnchorAvatar is custom, this is the ID of the avatar to use as the override */
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchoring", meta = (EditCondition = "AnchorOriginRule == EAnchorOriginRule::AOR_Custom", EditConditionHides))
		FName AnchorAvatarCustomID;

		/**Force the player and all other speakers to be on opposite sides of the screen using Y-axis movement and Yaw. 
		You wouldn't want this enabled for ultra close up shots, or if you want your character to be in the middle of the screen, 
		but otherwise you should enable this as the 180 degree rule is a classic cinematography rule*/
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing")
		bool bUse180DegreeRule;
		
		/** If using 180 degree rule, how many degrees of yaw to push the shots in either direction */
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing", meta = (EditCondition = "bUse180DegreeRule", EditConditionHides))
		float UnitsY180DegreeRule;

		/** If using 180 degree rule, how many degrees of yaw to push the shots in either direction */
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing", meta = (EditCondition = "bUse180DegreeRule", EditConditionHides))
		float DegreesYaw180DegreeRule;

		/**What avatar should the camera track on if this is enabled */
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracking/Focus")
		FShotTrackingSettings LookAtTrackingSettings;

		/**What avatar should the camera track on if this is enabled */
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracking/Focus")
		FShotTrackingSettings FocusTrackingSettings;

		/**If true the camera will draw a box showing focus point */
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracking/Focus")
		bool bDrawDebugFocusPoint;

		UPROPERTY(BlueprintReadOnly, Category = "Sequence")
		TWeakObjectPtr<class AActor> Speaker;
		
		UPROPERTY(BlueprintReadOnly, Category = "Sequence")
		TWeakObjectPtr<class AActor> Listener;

		UPROPERTY(BlueprintReadOnly, Category = "Sequence")
		TWeakObjectPtr<class AActor> AnchorActor;
		
		UPROPERTY(BlueprintReadOnly, Category = "Sequence")
		TWeakObjectPtr<class AActor> LookAtActor;

		UPROPERTY(BlueprintReadOnly, Category = "Sequence")
		TWeakObjectPtr<class AActor> FocusActor;

		UPROPERTY(BlueprintReadOnly, Category = "Sequence")
		TWeakObjectPtr<class ALevelSequenceActor> SequenceActor;

		//The cinecam spawned in by the sequence 
		UPROPERTY(BlueprintReadOnly, Category = "Sequence")
		TWeakObjectPtr<class ACineCameraActor> Cinecam;

		UPROPERTY(BlueprintReadOnly, Category = "Sequence")
		TWeakObjectPtr<class UDialogue> Dialogue;
	public:

		virtual void Tick(const float DeltaTime);

		/**Play the sequence. The anchor is the actor whose head transform works as the offset the shot uses, the speaker receieves the cameras tracking/focus*/
		virtual void BeginPlaySequence(class ALevelSequenceActor* InSequenceActor, class UDialogue* InDialogue, class AActor* InSpeaker, class AActor* InListener);

		FORCEINLINE TArray<class ULevelSequence*> GetSequenceAssets() const { return SequenceAssets;}
		FORCEINLINE FMovieSceneSequencePlaybackSettings GetPlaybackSettings() const {return PlaybackSettings;}

		//Called before the shot is stopped and its sequence player is de-initialized. 
		UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "Dialogue Sequence")
		void OnStop();

		/** Plays the level sequence. Pretty rare you'd ever want to override this in BP but the option is there! */
		UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Dialogue Sequence")
		void EndSequence();
		virtual void EndSequence_Implementation();

	protected:

		/** Plays the level sequence. Pretty rare you'd ever want to override this in BP but the option is there! */
		UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Dialogue Sequence")
		void PlaySequence();
		virtual void PlaySequence_Implementation();

		/**Return the space the sequence will be shot in. Defaults to using the anchor actors head location. This is that if 
		different height characters are used, the shot will be lined up correctly regardless of height. Also applies some extra
		offsetting to adhere to the 180 degree rule if bUse180DegreeRule is checked. */
		UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Dialogue Sequence")
		FTransform GetShotAnchorTransform();
		virtual FTransform GetShotAnchorTransform_Implementation();

		/**Define the text that will show up on a node if this event is added to it */
		UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Dialogue Sequence")
		FText GetGraphDisplayText();
		virtual FText GetGraphDisplayText_Implementation();
};



