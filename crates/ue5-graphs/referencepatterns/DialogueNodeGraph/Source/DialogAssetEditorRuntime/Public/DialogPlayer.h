/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#pragma once

#include "CoreMinimal.h"
#include "DialogNodeInfo.h"
#include "LevelSequenceActor.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "Components/AudioComponent.h"
#include "DialogPlayer.generated.h"

struct FActorInfo;
class UDialogueMasterComponent;
struct FDialogSentence;
class UDialogNodeInfo;

class ADialogPlayer;


class TimerDialogEndCallback
{
	ADialogPlayer * _parent;
	UDialogNodeInfo * _nodeInfo;
	TArray<FDialogSentence> _answerList;
public:
	void _OnSoundPlayFinished();
	void Construct(ADialogPlayer * parent, UDialogNodeInfo * nodeInfo, TArray<FDialogSentence> answerList);
};

UENUM(BlueprintType)
enum EDialogueMasterComponentLocation
{
	GameMode				UMETA(DisplayName = "Game Mode"),
	PlayerController		UMETA(DisplayName = "Player Controller (recommended)"),
	GameState				UMETA(DisplayName = "Game State"),
	PlayerState				UMETA(DisplayName = "Player State")
};

/**
 * Plugin name : DialogueMaster
 */
UCLASS(BlueprintType, Blueprintable)
class DIALOGASSETEDITORRUNTIME_API ADialogPlayer : public AActor
{
	GENERATED_BODY()

	UPROPERTY()
	AActor * PlayerCamera = nullptr;
	bool _bFirstCameraSwitch = true;

	AActor* GetActorFromManualReferences(FActorInfo* ActorInfo, int ActorIdx);
	void PlayLine(UDialogNodeInfo * nodeInfo, TArray<FDialogSentence> answerList);
	void UpdateSwitchesAndCountersValues(UDialogueMasterComponent * DialogueMasterComponent, UDialogNodeInfo * nodeInfo);

	UAnimInstance* GetBodyPart(AActor * speaker, FString bodyPartTag);
	bool PlayMontageOnBodyPart(AActor * speaker, FString bodyPartTag, UAnimMontage * montage, bool stopOtherMontages = true);
	bool PlayMontageOnBodyPart(UAnimInstance * AnimInstance, UAnimMontage * montage, bool stopOtherMontages = true);
	bool StopMontageOnBodyPart(AActor * speaker, FString bodyPartTag);
	void NotifyDialogEnd(bool preserveUserObject = false);
	void ClearDialoguePlayer(bool bBlendCameraOut);
	void ClearDialoguePlayerCallback();

	friend class TimerDialogEndCallback;

	TArray<UChildActorComponent*> CreatedComponents;
	TArray<TimerDialogEndCallback*> TimerCallbackStack;
	TArray<ALevelSequenceActor*> CreatedLevelSequences;

	UFUNCTION()
	void consumeTask();
	
	void StartDialog();

	// Used to play the dialogue voice sound
	UPROPERTY()
	UAudioComponent* AudioPlayer = nullptr;

	UPROPERTY()
	USoundAttenuation* DefaultSoundAttenuation;
	USoundAttenuation* GetSoundAttenuation();
	void PlayDialogueLineSound(UDialogNodeInfo * dialogueLine, AActor * Speaker);
	void StopPlayingSound();
	void StopPlayingAnimationsOnActor(AActor * actorRef);
	FText GetActorTag(UDialogNodeInfo * NodeInfo);
	FText GetActorTag(FActorInfo * ActorInfo);
	
	// Used to store the timer handle of the currently playing dialogue line
	FTimerHandle TimerHandle_DialogueLineFinished;
	
	bool _bIsGamePaused = false;
	
public:
	ADialogPlayer();
	
	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;

	/**
	 * Specify the location where you added the Dialogue Master Component.
	 * It is recommended to add it to the PlayerController.
	 */
	UPROPERTY(EditAnywhere, Category="Dialogue Master - Dialog Player")
	TEnumAsByte<EDialogueMasterComponentLocation> ComponentLocation = PlayerController;

	UPROPERTY(EditAnywhere, Category="Dialogue Master - Dialog Player")
	USoundAttenuation * SoundAttenuation;
	
	UFUNCTION(BlueprintCallable, Category="Dialogue Master - Dialog Player", meta = (AutoCreateRefTerm = "UserObject,NPCManualReferences"))
	void PlayDialog(class UDialogAsset* dialogAsset, AActor * NPC, AActor * Player, UObject * UserObject, TArray<AActor*> NPCManualReferences);

	UFUNCTION(BlueprintCallable, Category="Dialogue Master - Dialog Player")
	void PlaySubDialog(class UDialogAsset * dialogAsset);

	
	UFUNCTION(BlueprintCallable, Category="Dialogue Master - Dialog Player")
	void SkipDialogLine();
	
	UFUNCTION(BlueprintCallable, Category="Dialogue Master - Dialog Player")
	void CancelDialog(bool preserveUserObject = false);
	
	UFUNCTION(BlueprintCallable, Category="Dialogue Master - Dialog Player")
	void ChooseOptionAtIndex(int index);

	
	UFUNCTION(BlueprintImplementableEvent)
	void DialogBegin(UObject * UserObject);

	UFUNCTION(BlueprintImplementableEvent)
	void DialogCancelled(UObject * UserObject);
	
	UFUNCTION(BlueprintImplementableEvent)
	void DialogLineStart(const FText& actorName, const struct FDialogReplique& replique, float duration, UObject * UserObject);

	UFUNCTION(BlueprintImplementableEvent)
	void DialogLineSkipped();
	
	UFUNCTION(BlueprintImplementableEvent)
	void DialogUpdated(const FText& actorName, const struct FDialogReplique& replique, const TArray<struct FDialogSentence>& answerList, UObject * UserObject);

	UFUNCTION(BlueprintImplementableEvent)
	void DialogEnd(UObject * UserObject);

	UFUNCTION(BlueprintCallable, Category="Dialogue Master - Dialog Player")
	void SetDisableCameraRestorationOnDialogueEnd(bool bDisabled);
private:
	void InternalCancelDialog();
	bool InternalCheckIfThereIsAtLeastOnePlayableDialogueLine();
	void InternalPlayDialog(class UDialogAsset* dialogAsset, AActor * NPC, AActor * Player, UObject * UserObject = nullptr, bool preserveCachedVariables = false, TArray<AActor*> NPCManualReferences = TArray<AActor*>());
	bool _preserveCachedVariables = false;
	bool _disableCameraRestoreAtDialogueEnd = false;
	
	UDialogueMasterComponent * FindDialogueMasterComponent();

	AActor* getActorRefFromActorInfo(FActorInfo* actorInfo, int ActorIdx);

	void NotifyDialogEndToCharacterInterface(AActor * Actor);
	
	UPROPERTY()
	class UDialogAsset* _playingAsset = nullptr;

	UPROPERTY()
	class AActor* _NPC = nullptr;

	UPROPERTY()
	class AActor* _Player = nullptr;

	UPROPERTY()
	class UObject* _UserObject = nullptr;

	UPROPERTY()
	TArray<AActor*> _NPCManualReferences;
	
	EMovementMode _PlayerInitialMovementMode;

	UPROPERTY()
	class UDialogRuntimeNode* _currentNode = nullptr;

	UPROPERTY()
	UDialogueMasterComponent * _cachedReference = nullptr;
};
