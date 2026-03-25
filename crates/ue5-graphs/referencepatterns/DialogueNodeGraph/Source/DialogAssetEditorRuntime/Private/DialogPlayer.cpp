/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */


#include "DialogPlayer.h"

#include "CineCameraComponent.h"
#include "CineCameraSettings.h"
#include "DialogAsset.h"
#include "DialogEndNodeInfo.h"
#include "DialogueMasterComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "DialogueMasterCharacterInterface.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "TextureLayout.h"
#include "Components/AudioComponent.h"
#include "ActorInfo.h"

DEFINE_LOG_CATEGORY_STATIC(DialogPlayerSub, Log, All);

void TimerDialogEndCallback::_OnSoundPlayFinished()
{
	if(_nodeInfo != nullptr)
	{
		UDialogAsset * currentPlayingAsset = _parent->_playingAsset;
		for(UDialogueMasterAction * action : _nodeInfo->Replique.DialogActions)
		{
			if(action->TriggerMoment == End || action->TriggerMoment == Both)
				action->Trigger(_parent->FindDialogueMasterComponent(), End);
		}
		
		_parent->UpdateSwitchesAndCountersValues(_parent->FindDialogueMasterComponent(), _nodeInfo);
		
		_parent->DialogUpdated(_nodeInfo->ActorName, _nodeInfo->Replique, _answerList, _parent->_UserObject);

		FActorInfo * actorInfo = _parent->_playingAsset->getActorInfoFromIndex(_nodeInfo->ActorIdx);
		AActor * actorRef = _parent->getActorRefFromActorInfo(actorInfo, _nodeInfo->ActorIdx);
		_parent->FindDialogueMasterComponent()->OnDialogueLineEnd.Broadcast(_parent, _parent->_playingAsset,
			_nodeInfo->ActorName, actorRef, _nodeInfo, _answerList, _parent->_UserObject);

		UDialogAsset * updatedPlayingAsset = _parent->_playingAsset;

		// Stop playing animation on the player character when there is a player choice :
		if(_parent->_playingAsset->StopPlayerAnimationDuringPlayerChoice && _answerList.Num() > 1)
		{
			_parent->StopPlayingAnimationsOnActor(_parent->_Player);
		}
		
		if(_answerList.Num() == 1)	// If there is only one choice, automatically continue the dialog ...
			_parent->ChooseOptionAtIndex(_answerList[0].AnswerIndex);
		else if(_answerList.Num() == 0)
		{
			// Only notify end if the playing asset has not changed.
			// If it changed, that mean we started a subdialogue through
			// dialogue action !
			if(currentPlayingAsset == updatedPlayingAsset)	
				_parent->NotifyDialogEnd();
		}
	}

	delete this;
}

void TimerDialogEndCallback::Construct(ADialogPlayer* parent, UDialogNodeInfo* nodeInfo, TArray<FDialogSentence> answerList)
{
	this->_parent = parent;
	this->_nodeInfo = nodeInfo;
	this->_answerList = answerList;
}


AActor* ADialogPlayer::GetActorFromManualReferences(FActorInfo* ActorInfo, int ActorIdx)
{
	if(ActorIdx == 0 && IsValid(_Player))
	{
		return _Player;
	}

	int NPCManualRefIndex = _playingAsset->getNPCManualReferenceIndexForNPC(ActorIdx);

	if(NPCManualRefIndex >= 0 && NPCManualRefIndex < _NPCManualReferences.Num())
	{
		return _NPCManualReferences[NPCManualRefIndex];
	}
	
	return nullptr;
}

void ADialogPlayer::PlayLine(UDialogNodeInfo * nodeInfo, TArray<FDialogSentence> answerList)
{
	UDialogNodeInfo * _currentNodeInfo = nodeInfo;
	
	FActorInfo * actorInfo = _playingAsset->getActorInfoFromIndex(_currentNodeInfo->ActorIdx);
	AActor * actorRef = getActorRefFromActorInfo(actorInfo, _currentNodeInfo->ActorIdx);

	float duration = _currentNodeInfo->Replique.DefaultDuration;

	bool hasLevelSequenceToPlay = false;
	// The level sequence is manage even if there is no actor found (this way, it is possible to setup tutorial
	// cinematic camera to show interest point without linked actor).
	// Manage level sequence (if SpawnDialogCamera is enabled) :
	if(_playingAsset->SpawnDialogCamera && !_currentNodeInfo->Replique.NoCameraReplacement && _currentNodeInfo->Replique.LevelSequence != nullptr)
	{
		hasLevelSequenceToPlay = true;
		
		ALevelSequenceActor * SequenceActor = nullptr;
		FMovieSceneSequencePlaybackSettings settings;
		settings.bAutoPlay = true;
		settings.PlayRate = 1.0;
		settings.StartTime = 0.0;
		settings.bRandomStartTime = false;
		settings.bDisableMovementInput = false;
		settings.bDisableLookAtInput = false;
		settings.bHidePlayer = false;
		settings.bHideHud = false;
		settings.bDisableCameraCuts = false;
		settings.FinishCompletionStateOverride = EMovieSceneCompletionModeOverride::None;
		settings.bPauseAtEnd = true;
			
		ULevelSequencePlayer * SequencePlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(
			GetWorld(),
			_currentNodeInfo->Replique.LevelSequence,
			settings,
			SequenceActor
			);

		if(SequenceActor != nullptr)
			CreatedLevelSequences.Add(SequenceActor);
	}
	
	AActor * localSpeaker = actorRef;
	// If the tagged actor instance is found :
	if(localSpeaker != nullptr)
	{		
		// Manage camera shot (if SpawnDialogCamera is enabled and there is no level sequence to play (managed previously) ) :
		if(_playingAsset->SpawnDialogCamera
			&& !_currentNodeInfo->Replique.NoCameraReplacement
			&& (_currentNodeInfo->Replique.CameraShotArray.Num() > 0 || IsValid(actorInfo->DefaultShot.Shot))
			&& !hasLevelSequenceToPlay)
		{
			FCameraShot * cameraShot = nullptr;
			TSubclassOf<ACineCameraActor> cameraClass = nullptr;

			// Select a valid camera shot class :
			if(_currentNodeInfo->Replique.CameraShotArray.Num() > 0)
				cameraShot = &(_currentNodeInfo->Replique.CameraShotArray[FMath::RandRange(0, _currentNodeInfo->Replique.CameraShotArray.Num() - 1)]);
			else
				cameraShot = &(actorInfo->DefaultShot);
			

			if(cameraShot != nullptr)
				cameraClass = cameraShot->Shot;

			if(IsValid(cameraClass))
			{
				// Create the camera :
				UChildActorComponent * cameraChildActor = NewObject<UChildActorComponent>(localSpeaker,
					UChildActorComponent::StaticClass(),
					FName(TEXT("CameraChildActor")));

				if(cameraChildActor)
				{
					cameraChildActor->RegisterComponent();
					//cameraChildActor->SetupAttachment(_speaker->GetRootComponent());
					cameraChildActor->AttachToComponent(localSpeaker->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
					cameraChildActor->CreationMethod = EComponentCreationMethod::UserConstructionScript;
					cameraChildActor->SetChildActorClass(cameraClass);
					ADialogCameraActor * camera = Cast<ADialogCameraActor>(cameraChildActor->GetChildActor());

					CreatedComponents.Add(cameraChildActor);

					if(IsValid(camera))
					{
						camera->SetTargetActor(localSpeaker);
						camera->LookatTrackingSettings.ActorToTrack = localSpeaker;
						camera->GetCineCameraComponent()->FocusSettings.bSmoothFocusChanges = false;
						camera->GetCineCameraComponent()->FocusSettings.TrackingFocusSettings.ActorToTrack = localSpeaker;
						camera->GetCineCameraComponent()->FocusSettings.FocusMethod = ECameraFocusMethod::Tracking;

						// Apply additive offset :
						camera->AddActorLocalTransform(cameraShot->CameraAdditiveOffset);
				
						APlayerController * playerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
						playerController->SetViewTargetWithBlend(camera, _bFirstCameraSwitch ? _playingAsset->DialogCameraBlendTime : 0, VTBlend_EaseInOut, 1.5f, true);
					}
				}
				_bFirstCameraSwitch = false;
			}
		}

		bool hasStartMontage = false;
		UAnimInstance * BodyAnimInstance = GetBodyPart(localSpeaker, "Body");
		UAnimInstance * HeadAnimInstance = GetBodyPart(localSpeaker, "Head");
		// Manage body animation :
		if(IsValid(_currentNodeInfo->Replique.BodyAnimMontage))
		{
			PlayMontageOnBodyPart(BodyAnimInstance, _currentNodeInfo->Replique.BodyAnimMontage);
			hasStartMontage = true;
		}

		// Manage head lipsync animation :
		if(IsValid(_currentNodeInfo->Replique.FaceLipSyncAnimMontage))
		{
			// If working with a custom character where body and head animation are handled by the same anim instance,
			// we don't stop other montages if a body animation has just been started.
			// If using this kind of character, you must use layered blend in the AnimBP to be able to play montage
			// independently.
			bool stopOtherMontages = true;
			if(BodyAnimInstance == HeadAnimInstance && hasStartMontage)
				stopOtherMontages = false;
			
			PlayMontageOnBodyPart(HeadAnimInstance, _currentNodeInfo->Replique.FaceLipSyncAnimMontage, stopOtherMontages);
		}
	}

	bool hasSound = false;
	// Manage voice sound :
	if(IsValid(_currentNodeInfo->Replique.VoiceSound))
	{
		PlayDialogueLineSound(_currentNodeInfo, localSpeaker);
		hasSound = true;
		duration = _currentNodeInfo->Replique.VoiceSound->Duration;
	}
	
	// Wait until the duration is passed (sound is played or default duration if there is no sound ...)
	TimerDialogEndCallback * callback = new TimerDialogEndCallback();
	callback->Construct(this, _currentNodeInfo, answerList);
	TimerCallbackStack.Push(callback);
	DialogLineStart(actorInfo->ActorIdentifier, _currentNodeInfo->Replique, duration, _UserObject);
	FindDialogueMasterComponent()->OnDialogueLinePlay.Broadcast(this, _playingAsset, actorInfo->ActorIdentifier,
		localSpeaker, _currentNodeInfo, duration, _UserObject);

	for(UDialogueMasterAction * action : _currentNodeInfo->Replique.DialogActions)
	{
		if(action->TriggerMoment == Beginning || action->TriggerMoment == Both)
			action->Trigger(FindDialogueMasterComponent(), Beginning);
	}

	// Manage depending on duration type :
	switch(_currentNodeInfo->Replique.DurationType)
	{
	case ELineDurationType::CUSTOM_DURATION:
		GetWorldTimerManager().ClearTimer(TimerHandle_DialogueLineFinished);
		GetWorldTimerManager().SetTimer(TimerHandle_DialogueLineFinished, this, &ADialogPlayer::consumeTask, _currentNodeInfo->Replique.DefaultDuration, false);
		break;

	case ELineDurationType::NEVER:
		break;

	default:
		// Default behavior ...
		if(duration > 0)
		{
			// Not calling directly _OnSoundPlayFinished on the callback object because rumors says that SetTimer does not
			// always work with object other than the one it is called from ...

			// Using the timer if there is no sound :
			if(!hasSound)
			{
				GetWorldTimerManager().ClearTimer(TimerHandle_DialogueLineFinished);
				GetWorldTimerManager().SetTimer(TimerHandle_DialogueLineFinished, this, &ADialogPlayer::consumeTask, duration, false);
			}
			else
			{
				if(AudioPlayer)
				{
					AudioPlayer->OnAudioFinished.AddDynamic(this, &ADialogPlayer::consumeTask);
				}
			}
		}
		else
		{
			consumeTask();
		}
		break;
	}
	
}

UAnimInstance* ADialogPlayer::GetBodyPart(AActor* speaker, FString bodyPartTag)
{
	TArray<UActorComponent*> skeletalMeshes = speaker->GetComponentsByTag(USkeletalMeshComponent::StaticClass(), FName(bodyPartTag));
	// If tagged skeletal mesh is found :
	if(skeletalMeshes.Num() > 0)
	{
		USkeletalMeshComponent * mesh = Cast<USkeletalMeshComponent>(skeletalMeshes[0]);
		USkeletalMesh * skeletalMesh = mesh->GetSkeletalMeshAsset();
		if(IsValid(skeletalMesh))
		{
			UAnimInstance * AnimInstance = mesh->GetAnimInstance();
			if(AnimInstance)
			{
				return AnimInstance;
			}
		}
	}
	else
	{
		// Browse child actor components :
		TArray<UActorComponent*> childActorComponents;
		speaker->GetComponents(UChildActorComponent::StaticClass(), childActorComponents);
		for(UActorComponent* childComponent : childActorComponents)
		{
			UChildActorComponent * childActorComponent = Cast<UChildActorComponent>(childComponent);
			if(childActorComponent)
			{
				AActor * child = childActorComponent->GetChildActor();
				if(IsValid(child))
				{
					UAnimInstance * AnimInstance = GetBodyPart(child, bodyPartTag);
					if(AnimInstance != nullptr)
					{
						return AnimInstance;
					}
				}
			}
		}
	}

	return nullptr;
}

bool ADialogPlayer::PlayMontageOnBodyPart(AActor * speaker, FString bodyPartTag, UAnimMontage * montage, bool stopOtherMontages)
{
	UAnimInstance * AnimInstance = GetBodyPart(speaker, bodyPartTag);
	return PlayMontageOnBodyPart(AnimInstance, montage, stopOtherMontages);
}

bool ADialogPlayer::PlayMontageOnBodyPart(UAnimInstance* AnimInstance, UAnimMontage* montage, bool stopOtherMontages)
{
	bool result = false;
	if(AnimInstance != nullptr)
	{
		AnimInstance->Montage_Play(montage, 1, EMontagePlayReturnType::MontageLength, 0, stopOtherMontages);
		result = true;
	}
	return result;
}

bool ADialogPlayer::StopMontageOnBodyPart(AActor* speaker, FString bodyPartTag)
{
	bool result = false;

	UAnimInstance * AnimInstance = GetBodyPart(speaker, bodyPartTag);
	if(AnimInstance != nullptr)
	{
		AnimInstance->Montage_Stop(1.0f);
		result = true;
	}

	return result;
}

void ADialogPlayer::consumeTask()
{
	if(TimerCallbackStack.Num() > 0)
	{
		TimerDialogEndCallback * callback = TimerCallbackStack.Pop();
		callback->_OnSoundPlayFinished();
	}
}

bool ADialogPlayer::InternalCheckIfThereIsAtLeastOnePlayableDialogueLine()
{
	if(_playingAsset != nullptr && _currentNode != nullptr)
	{
		// _currentNode is on the start node.
		UDialogRuntimeNode * node = nullptr;	// We use node to navigate in the dialogue tree.
		UDialogueMasterComponent * DialogueMasterComponent = FindDialogueMasterComponent();
		
		if(_currentNode->OutputPins.Num() > 0)
		{
			UDialogRuntimePin* outputPin = _currentNode->OutputPins[0];
			if(outputPin->Connection != nullptr)
			{
				// node is on the first node.
				node = outputPin->Connection->Parent;
			}

			// This set avoid infinite loop (visiting again and again same nodes if the
			// user has a branching node loop in his dialogue graph).
			TSet<UDialogRuntimeNode*> VisitedNodes;
			TQueue<UDialogRuntimeNode*> NodeToCheck;
			while(node != nullptr && node->NodeType == EDialogNodeType::DialogNode && !VisitedNodes.Contains(node))
			{
				VisitedNodes.Add(node);
				UDialogNodeInfo * nodeInfo = Cast<UDialogNodeInfo>(node->NodeInfo);

				// check if first node prerequisites are verified !
				if(nodeInfo->checkPrerequisites(DialogueMasterComponent, _NPC, _Player))
				{
					if(!nodeInfo->isBranchingNode)
						return true;

					// The node is a branching node :
					for(UDialogRuntimePin * pin : node->OutputPins)
					{
						if(pin->Connection != nullptr)
						{
							UDialogRuntimeNode * sentenceNode = pin->Connection->Parent;
							if(sentenceNode != nullptr && sentenceNode->NodeType == EDialogNodeType::DialogNode && !VisitedNodes.Contains(sentenceNode))
							{
								UDialogNodeInfo * sentenceInfo = Cast<UDialogNodeInfo>(sentenceNode->NodeInfo);
								const bool bPrerequisitesVerified = sentenceInfo->checkPrerequisites(DialogueMasterComponent, _NPC, _Player);
								
								if(bPrerequisitesVerified)
								{
									NodeToCheck.Enqueue(sentenceNode);
								}
							}
						}
					}
				}

				// Navigate in the graph :
				if(NodeToCheck.IsEmpty())
					node = nullptr;	// If there is nothing more to explore, exit the loop.
				else
				{
					NodeToCheck.Dequeue(node);
				}
			}
		}
	}

	return false;
}

void ADialogPlayer::UpdateSwitchesAndCountersValues(UDialogueMasterComponent * DialogueMasterComponent, UDialogNodeInfo * nodeInfo)
{
	if(nodeInfo->PlayableOnlyOnce)
	{
		ISwitchAndCounterPrerequisitesInterface::Execute_setSwitchValue(DialogueMasterComponent, nodeInfo->GetNodeSwitchVariableName(), true);
	}
	
	for(FSwitchDialogPrerequisite modify : nodeInfo->Replique.UpdateSwitchValue)
	{
		ISwitchAndCounterPrerequisitesInterface::Execute_setSwitchValue(DialogueMasterComponent, modify.SwitchPrerequisiteName, modify.NeededValue);
	}

	for(FCounterChangeValue modify : nodeInfo->Replique.UpdateCounterValue)
	{
		int newValue = ISwitchAndCounterPrerequisitesInterface::Execute_getCounterValue(DialogueMasterComponent, modify.CounterName);

		switch(modify.ModifyValueOperator)
		{
		case Set:
			newValue = modify.NewValue;
			break;

		case Add:
			newValue += modify.NewValue;
			break;

		case Subtract:
			newValue -= modify.NewValue;
			break;
		}
				
		ISwitchAndCounterPrerequisitesInterface::Execute_setCounterValue(DialogueMasterComponent, modify.CounterName, newValue);
	}
}

void ADialogPlayer::NotifyDialogEnd(bool preserveUserObject)
{
	if(_playingAsset == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("NotifyDialogEnd called on a DialogPlayer that have NO currently playingAsset !"));
		return;
	}

	// End of dialog :
	if(!preserveUserObject)
	{
		StopPlayingSound();

		// Stop animation on NPCs :
		// + 1 because the player is not in the DialogActors list.
		for(int i = 0; i < _playingAsset->DialogActors.Num() + 1; i++)
		{
			FActorInfo * actorInfo = _playingAsset->getActorInfoFromIndex(i);
			AActor * actorRef = getActorRefFromActorInfo(actorInfo, i);
			
			StopPlayingAnimationsOnActor(actorRef);
		}
		
		if(_playingAsset->SpawnDialogCamera && _playingAsset->DialogCameraBlendTime > 0.0)
		{
			// Restore the player camera (if SpawnDialogCamera is enabled) :
			if(_playingAsset->SpawnDialogCamera && PlayerCamera && !_disableCameraRestoreAtDialogueEnd)
				UGameplayStatics::GetPlayerController(GetWorld(), 0)->SetViewTargetWithBlend(PlayerCamera, _playingAsset->DialogCameraBlendTime, VTBlend_EaseInOut, 1.5f, true);

			FTimerHandle UnusedHandle;
			GetWorldTimerManager().SetTimer(UnusedHandle, this, &ADialogPlayer::ClearDialoguePlayerCallback, _playingAsset->DialogCameraBlendTime, false);
			return;	// We returns because we don't want to clear the _playingAsset right now (we wait for the blend out to finish !).
		}
		else
		{
			ClearDialoguePlayer(true);
		}
	}

	_playingAsset = nullptr;
}

void ADialogPlayer::ClearDialoguePlayerCallback()
{
	ClearDialoguePlayer(false);
}

void ADialogPlayer::ClearDialoguePlayer(bool bBlendCameraOut)
{
	if(_playingAsset == nullptr) return;
		
	// Clear created LevelSequenceActor :
	for(ALevelSequenceActor * LSA : CreatedLevelSequences)
	{
		LSA->Destroy();
	}

	CreatedLevelSequences.Empty();

	if(_playingAsset->SpawnDialogCamera && bBlendCameraOut && PlayerCamera && !_disableCameraRestoreAtDialogueEnd)
		UGameplayStatics::GetPlayerController(GetWorld(), 0)->SetViewTargetWithBlend(PlayerCamera, _playingAsset->DialogCameraBlendTime, VTBlend_Cubic, 1, false);
	
	// Restore player movement mode (if FreeMovement is NOT enabled) :
	if(!_playingAsset->FreeMovement)
	{
		ACharacter* playerCharacter = Cast<ACharacter>(_Player);
		if(playerCharacter != nullptr)
		{
			UCharacterMovementComponent * characterMovement = playerCharacter->GetCharacterMovement();
			if(characterMovement != nullptr)
				characterMovement->SetMovementMode(_PlayerInitialMovementMode);
		}
	}
	
	_bFirstCameraSwitch = true;
	DialogEnd(_UserObject);
	FindDialogueMasterComponent()->OnDialogueEnd.Broadcast(this, _playingAsset, _UserObject);

	NotifyDialogEndToCharacterInterface(_Player);
	NotifyDialogEndToCharacterInterface(_NPC);
	
	_currentNode = nullptr;
	this->_NPC = nullptr;
	_UserObject = nullptr;

	// Clear created components :
	for(UChildActorComponent * component : CreatedComponents)
	{
		if(IsValid(component))
		{
			component->DestroyChildActor();
			component->DestroyComponent();
		}
	}

	CreatedComponents.Empty();

	// Clear callback stack (if the dialog is interrupted, there may be still object in there).
	for(TimerDialogEndCallback * callback : TimerCallbackStack)
	{
		delete callback;
	}

	TimerCallbackStack.Empty();

	_playingAsset = nullptr;
}


USoundAttenuation* ADialogPlayer::GetSoundAttenuation()
{
	if(IsValid(SoundAttenuation))
	{
		return SoundAttenuation;
	}

	return DefaultSoundAttenuation;
}

void ADialogPlayer::PlayDialogueLineSound(UDialogNodeInfo * dialogueLine, AActor* Speaker)
{
	// If there is already a sound playing (AudioPlayer != nullptr), we stop the sound
	// and destroy the audiocomponent before creating a new one (it avoid to have multiple
	// sound playing simultaneously if the player skip a dialogue line).
	StopPlayingSound();

	// If the input object is not valid, stop the execution.
	if(dialogueLine == nullptr) return;
	
	// Check the dialogueLine sound validity :
	if(IsValid(dialogueLine->Replique.VoiceSound))
	{
		// If we have a valid Speaker reference, we can spatialize the sound, so it sounds a lot more
		// natural than playing it in 2D (even more true when we have environmental dialogues!)
		if(Speaker != nullptr && _playingAsset != nullptr && _playingAsset->SpatializedVoices && dialogueLine->SpatializedVoices)
		{
			UAnimInstance * HeadAnimInstance = GetBodyPart(Speaker, "Head");
			USkeletalMeshComponent * SkeletalMesh = nullptr;
			if(HeadAnimInstance != nullptr)
			{
				SkeletalMesh = HeadAnimInstance->GetSkelMeshComponent();
			}

			// If the head is found, we attach the sound to it :
			if(SkeletalMesh != nullptr)
			{
				AudioPlayer = UGameplayStatics::SpawnSoundAttached(dialogueLine->Replique.VoiceSound, SkeletalMesh,
					NAME_None,
					FVector(ForceInit),
					FRotator::ZeroRotator,
					EAttachLocation::KeepRelativeOffset,
					false,
					1, 1, 0,
					GetSoundAttenuation());
			}
			else
			{
				// If the head is not found, we just spawn the sound at the actor location ...
				AudioPlayer = UGameplayStatics::SpawnSoundAtLocation(this,
					dialogueLine->Replique.VoiceSound,
					Speaker->GetActorLocation(),
					Speaker->GetActorForwardVector().Rotation(),
					1,
					1,
					0,
					GetSoundAttenuation());
			}
		}
		else
		{
			// If we don't have the Speaker reference, we fallback in 2D mode (it can be the case when you want
			// an external narrator or if you want the player to speak to himself).
			AudioPlayer = UGameplayStatics::SpawnSound2D(this, dialogueLine->Replique.VoiceSound);	
		}
	}
}

void ADialogPlayer::StopPlayingSound()
{
	if(AudioPlayer != nullptr)
	{
		AudioPlayer->OnAudioFinished.RemoveAll(this);
		AudioPlayer->Stop();
		AudioPlayer->DestroyComponent();
		AudioPlayer = nullptr;
	}
}

void ADialogPlayer::StopPlayingAnimationsOnActor(AActor* actorRef)
{
	if(actorRef != nullptr)
	{
		StopMontageOnBodyPart(actorRef, "Head");

		// Avoid cancelling body animation when FreeMovement is enabled.
		// Stopping animation on body can have side effect on other
		// systems like combat system, car system, etc...
		if(_playingAsset != nullptr && !_playingAsset->FreeMovement)
			StopMontageOnBodyPart(actorRef, "Body");
	}
}

FText ADialogPlayer::GetActorTag(UDialogNodeInfo * NodeInfo)
{
	FActorInfo * actorInfo = _playingAsset->getActorInfoFromIndex(NodeInfo->ActorIdx);
	return GetActorTag(actorInfo);
}

FText ADialogPlayer::GetActorTag(FActorInfo * actorInfo)
{
	FText result;
	if(actorInfo != nullptr)
	{
		result = (actorInfo->UniqueIdentifierOverride.IsEmpty() ?
								actorInfo->ActorIdentifier
								:
								actorInfo->UniqueIdentifierOverride);
	}

	return result;
}

ADialogPlayer::ADialogPlayer()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ADialogPlayer::BeginPlay()
{
	Super::BeginPlay();

	_bFirstCameraSwitch = true;
	APlayerController * PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	PlayerCamera = PC ? PC->GetViewTarget() : nullptr;

	DefaultSoundAttenuation = NewObject<USoundAttenuation>(this, USoundAttenuation::StaticClass(), "DefaultSoundAttenuation");
	if(DefaultSoundAttenuation)
	{
		DefaultSoundAttenuation->Attenuation.bAttenuate = true;
		DefaultSoundAttenuation->Attenuation.bSpatialize = true;
		DefaultSoundAttenuation->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::Linear;
		DefaultSoundAttenuation->Attenuation.FalloffDistance = 1000.0f;
	}
}

void ADialogPlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	bool currentState = UGameplayStatics::IsGamePaused(GetWorld());

	if(_bIsGamePaused != currentState)
	{
		_bIsGamePaused = currentState;
		
		if(AudioPlayer != nullptr)
		    AudioPlayer->SetPaused(currentState);	// Pause or resume the voice audio.
	}
}

void ADialogPlayer::PlayDialog(UDialogAsset* dialogAsset, AActor * NPC, AActor * Player, UObject * UserObject, TArray<AActor*> NPCManualReferences)
{
	InternalPlayDialog(dialogAsset, NPC, Player, UserObject, false, NPCManualReferences);
}

void ADialogPlayer::PlaySubDialog(UDialogAsset* dialogAsset)
{
	CancelDialog(true);
	InternalPlayDialog(dialogAsset, _NPC, _Player, _UserObject, true, _NPCManualReferences);
}

void ADialogPlayer::SkipDialogLine()
{
	if(_playingAsset == nullptr) return;
	if(_currentNode == nullptr) return;

	if(_currentNode->NodeType == EDialogNodeType::DialogNode)
	{
		UDialogNodeInfo * nodeInfo = Cast<UDialogNodeInfo>(_currentNode->NodeInfo);
		if(nodeInfo->Skippable)
		{
			GetWorldTimerManager().ClearTimer(TimerHandle_DialogueLineFinished);
			// Stop voice sound :
			StopPlayingSound();

			// Stop animations :
			FActorInfo * actorInfo = _playingAsset->getActorInfoFromIndex(nodeInfo->ActorIdx);
			AActor * actorRef = getActorRefFromActorInfo(actorInfo, nodeInfo->ActorIdx);
			StopPlayingAnimationsOnActor(actorRef);
			
			DialogLineSkipped();
			FindDialogueMasterComponent()->OnDialogueLineSkipped.Broadcast(this, _playingAsset, _UserObject);
			consumeTask();
		}
	}
}

void ADialogPlayer::SetDisableCameraRestorationOnDialogueEnd(bool bDisabled = true)
{
	this->_disableCameraRestoreAtDialogueEnd = bDisabled;
}

void ADialogPlayer::InternalCancelDialog()
{
	UDialogAsset * asset = _playingAsset;
	_playingAsset = nullptr;
	this->_NPC = nullptr;
	this->_Player = nullptr;
	this->_NPCManualReferences.Empty();
	DialogCancelled(_UserObject);
	FindDialogueMasterComponent()->OnDialogueCancelled.Broadcast(this, asset, _UserObject);
	_UserObject = nullptr;
}

void ADialogPlayer::InternalPlayDialog(UDialogAsset* dialogAsset, AActor* NPC, AActor* Player, UObject* UserObject,
                                       bool preserveCachedVariables, TArray<AActor*> NPCManualReferences)
{
	// Do not start a new dialog if the current one is not over !
	if(_playingAsset != nullptr) return;

	// Do not start a new dialog if incoming parameter is invalid !
	if(dialogAsset == nullptr) return;
	
	_playingAsset = dialogAsset;
	this->_preserveCachedVariables = preserveCachedVariables;

	// When a dialogue starts, reset this flag:
	this->_disableCameraRestoreAtDialogueEnd = false;

	this->_NPC = NPC;
	this->_Player = Player;
	this->_NPCManualReferences = NPCManualReferences;

	_UserObject = UserObject;

	// Check prerequisites on character interfaces :
	float timeToWait = 0;
	bool enterDialogue = true;
	if(_Player != nullptr && _Player->Implements<UDialogueMasterCharacterInterface>())
	{
		if(IDialogueMasterCharacterInterface::Execute_canEnterDialogue(_Player, _UserObject))
		{
			float wait = IDialogueMasterCharacterInterface::Execute_onBeforeEnterDialogue(_Player, _UserObject);
			if(wait > timeToWait)
				timeToWait = wait;
		}
		else
		{
			enterDialogue = false;
		}
	}

	if(_NPC != nullptr && _NPC->Implements<UDialogueMasterCharacterInterface>())
	{
		if(IDialogueMasterCharacterInterface::Execute_canEnterDialogue(_NPC, _UserObject))
		{
			float wait = IDialogueMasterCharacterInterface::Execute_onBeforeEnterDialogue(_NPC, _UserObject);
			if(wait > timeToWait)
				timeToWait = wait;
		}
		else
		{
			enterDialogue = false;
		}
	}

	// If prerequisites not verified : cancel the dialog activation.
	if(!enterDialogue)
	{
		InternalCancelDialog();
		return;
	}

	if(timeToWait > 0)
	{
		FTimerHandle UnusedHandle;
		GetWorldTimerManager().SetTimer(UnusedHandle, this, &ADialogPlayer::StartDialog, timeToWait, false);
	}
	else
	{
		StartDialog();
	}
}

void ADialogPlayer::CancelDialog(bool preserveUserObject)
{
	NotifyDialogEnd(preserveUserObject);
}

// Callback to start the dialog (because it can be delayed if user want to perform action before the dialogue play) :
void ADialogPlayer::StartDialog()
{
	UDialogRuntimeGraph * graph = _playingAsset->Graph;
	
	// Initialize actions & conditions :
	for(UDialogRuntimeNode* node : graph->Nodes)
	{
		if(node->NodeType == EDialogNodeType::DialogNode)
		{
			UDialogNodeInfo * info = Cast<UDialogNodeInfo>(node->NodeInfo);
			for(UDialogueMasterAction * action : info->Replique.DialogActions)
			{
				action->Initialize(_NPC, _Player, GetWorld(), this);
			}

			for(UAdvancedPrerequisiteBase * condition : info->Replique.AdvancedPrerequisites)
			{
				condition->Initialize(GetWorld());
			}
		}
	}

	// Get the start node :
	for(UDialogRuntimeNode* node : graph->Nodes)
	{
		if(node->NodeType == EDialogNodeType::StartNode)
		{
			_currentNode = node;
			break;
		}
	}

	if(_currentNode == nullptr)
	{
		InternalCancelDialog();
		UE_LOG(DialogPlayerSub, Error, TEXT("No start node in dialog."));
		return;
	}
	
	// Check if there is at least one playable dialogue line.
	if(!InternalCheckIfThereIsAtLeastOnePlayableDialogueLine())
	{
		InternalCancelDialog();
		return;
	}
	
	DialogBegin(_UserObject);
	FindDialogueMasterComponent()->OnDialogueBegin.Broadcast(this, _playingAsset, _UserObject);

	// Disable player movement if FreeMovement is NOT enabled :
	// Note that if we start a sub dialogue, we don't override the _PlayerInitialMovementMode variable
	// because if we do that we will stuck the character in no movement mode ! (because the master
	// dialogue modified the character movement mode to none (if !FreeMovement mode)).
	// That's the role of preserveCachedVariables flag.
	if(!_playingAsset->FreeMovement && !_preserveCachedVariables)
	{
		ACharacter* playerCharacter = Cast<ACharacter>(_Player);
		if(playerCharacter != nullptr)
		{
			UCharacterMovementComponent * characterMovement = playerCharacter->GetCharacterMovement();
			if(characterMovement != nullptr)
			{
				_PlayerInitialMovementMode = characterMovement->MovementMode;
				characterMovement->SetMovementMode(MOVE_None);
			}
		}
	}

	// Move to the first dialog node :
	ChooseOptionAtIndex(0);
}

void ADialogPlayer::ChooseOptionAtIndex(int index)
{
	UDialogueMasterComponent * DialogueMasterComponent = FindDialogueMasterComponent();
	
	if(_currentNode == nullptr)
	{
		NotifyDialogEnd();
		return;
	}
	
	if(index < 0 || index > _currentNode->OutputPins.Num())
	{
		UE_LOG(DialogPlayerSub, Error, TEXT("Invalid response option at index %d."), index);
		return;
	}

	UDialogRuntimePin* outputPin = _currentNode->OutputPins[index];
	if(outputPin->Connection != nullptr)
	{
		_currentNode = outputPin->Connection->Parent;
	}
	else
	{
		// No connection so we'll assume it's an end node
		NotifyDialogEnd();
	}

	if(_currentNode != nullptr && _currentNode->NodeType == EDialogNodeType::DialogNode)
	{
		UDialogNodeInfo * nodeInfo = Cast<UDialogNodeInfo>(_currentNode->NodeInfo);

		int firstSentencePrerequisitesVerified = -1;
		TArray<FDialogSentence> answerList;
		int idx = 0;
		for(UDialogRuntimePin * pin : _currentNode->OutputPins)
		{
			if(pin->Connection != nullptr)
			{
				UDialogRuntimeNode * sentenceNode = pin->Connection->Parent;
				if(sentenceNode != nullptr && sentenceNode->NodeType == EDialogNodeType::DialogNode)
				{
					UDialogNodeInfo * sentenceInfo = Cast<UDialogNodeInfo>(sentenceNode->NodeInfo);
					const bool bPrerequisitesVerified = sentenceInfo->checkPrerequisites(DialogueMasterComponent, _NPC, _Player);

					// If it is a branching node, the firstSentencePrerequisitesVerified will be used :
					if(firstSentencePrerequisitesVerified < 0 && bPrerequisitesVerified)
					{
						firstSentencePrerequisitesVerified = idx;
					}

					// Only add sentence that prerequisites are satisfied :
					if(bPrerequisitesVerified)
					{
						// Construct dialog sentences info :
						FDialogSentence sentence;
						sentence.AnswerIndex = idx;
						sentence.ChoiceText = sentenceInfo->Replique.ShortSpokenText.IsEmpty() ? sentenceInfo->Replique.SpokenText : sentenceInfo->Replique.ShortSpokenText;
						sentence.NodeUserID = sentenceInfo->UserId;
						
						// If auto selection is enabled, it will automatically choose the first
						// sentence that has its prerequisites verified and have the auto selection enabled :
						if(sentenceInfo->AutoSelect)
						{
							answerList.Empty();			// Clear any option (without auto select enabled) that could
														// have been added to the answers array.
							answerList.Add(sentence);
							break;						// Break the loop so there is only the auto selected option
														// in the array.
						}
						
						answerList.Add(sentence);		// This line is executed only if it is not an auto selected answer. 
					}
				}
			}
			idx++;
		}

		if(nodeInfo->isBranchingNode)
		{
			if(firstSentencePrerequisitesVerified >= 0)
				ChooseOptionAtIndex(firstSentencePrerequisitesVerified);
			else
			{
				// If it is a branching node, but there is no valid option,
				// the dialogue ends.
				NotifyDialogEnd();
			}
		}
		else
		{
			// Play the dialog line and update switches and counters :
			PlayLine(nodeInfo, answerList);
		}
	}
}



UDialogueMasterComponent* ADialogPlayer::FindDialogueMasterComponent()
{
	if(IsValid(_cachedReference))
	{
		return _cachedReference;
	}

	AActor * OwningActor = nullptr;
	switch(ComponentLocation)
	{
	case PlayerController:
		OwningActor = UGameplayStatics::GetPlayerController(GetWorld(), 0);
		break;

	case GameMode:
		OwningActor = UGameplayStatics::GetGameMode(GetWorld());
		break;

	case GameState:
		OwningActor = UGameplayStatics::GetGameState(GetWorld());
		break;

	case PlayerState:
		OwningActor = UGameplayStatics::GetPlayerState(GetWorld(), 0);
		break;
	}

	_cachedReference = Cast<UDialogueMasterComponent>(OwningActor->GetComponentByClass(UDialogueMasterComponent::StaticClass()));

	if(!_cachedReference)
		UE_LOG(LogTemp, Warning, TEXT("Failed to find the DialogueMasterComponent where you specified it should be ; make sure you have added it and the location specified is correct !"));
	
	return _cachedReference;
}

AActor* ADialogPlayer::getActorRefFromActorInfo(FActorInfo* actorInfo, int ActorIdx)
{
	if(actorInfo != nullptr)
	{
		FText tagToSearch = actorInfo->getTagToSearch();

		TArray<AActor*> FoundActors;
	
		// Link with actor tag :
		if(actorInfo->NPCLinkingMethod == ActorTag)
		{
			UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName(tagToSearch.ToString()), FoundActors);
		}
		// Link with manual referencing :
		else if(actorInfo->NPCLinkingMethod == ManualReferencing)
		{
			AActor * Actor = GetActorFromManualReferences(actorInfo, ActorIdx);
			if(Actor != nullptr && IsValid(Actor))
			{
				FoundActors.Add(Actor);
			}
		}

		if(FoundActors.Num() > 0)
		{
			return FoundActors[0];
		}
	}

	return nullptr;
}

void ADialogPlayer::NotifyDialogEndToCharacterInterface(AActor* Actor)
{
	if(Actor != nullptr && Actor->Implements<UDialogueMasterCharacterInterface>())
	{
		IDialogueMasterCharacterInterface::Execute_onDialogueEnd(Actor, _UserObject);
	}
}
