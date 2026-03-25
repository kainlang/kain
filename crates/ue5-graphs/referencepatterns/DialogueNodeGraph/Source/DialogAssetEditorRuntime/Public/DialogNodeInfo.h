/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#pragma once

#include "CoreMinimal.h"
#include "ActorInfo.h"
#include "DialogNodeInfoBase.h"
#include "DialogueMasterAction.h"
#include "LevelSequence.h"
#include "Sound/SoundWave.h"
#include "Animation/AnimMontage.h"
#include "UObject/Object.h"
#include "CameraShot.h"
#include "LineDurationEnum.h"
#include "DialogNodeInfo.generated.h"


UINTERFACE(BlueprintType)
class UDialogActorInterface : public UInterface
{
    GENERATED_BODY()
};

class DIALOGASSETEDITORRUNTIME_API IDialogActorInterface
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Dialogue Master - Dialogue Actor Root Transform Interface")
    FTransform getDialogActorGroundRootTransform();
};


USTRUCT(BlueprintType)
struct FSwitchDialogPrerequisite
{
    GENERATED_USTRUCT_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Initialization")
    FName SwitchPrerequisiteName;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Initialization")
    bool NeededValue = false;
};

UENUM(BlueprintType)
enum EValueCheckOperator
{
    Equals              UMETA(DisplayName = "Equals"),
    NotEquals           UMETA(DisplayName = "NotEquals"),
    Greater             UMETA(DisplayName = "Greater"),
    GreaterOrEquals     UMETA(DisplayName = "GreaterOrEquals"),
    Smaller             UMETA(DisplayName = "Smaller"),
    SmallerOrEquals     UMETA(DisplayName = "SmallerOrEquals")
};

USTRUCT(BlueprintType)
struct FCounterDialogPrerequisite
{
    GENERATED_USTRUCT_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Initialization")
    FName CounterPrerequisiteName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Initialization")
    TEnumAsByte<EValueCheckOperator> ValueCheckOperator = EValueCheckOperator::Equals;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Initialization")
    int NeededValue = 0;
};

UENUM(BlueprintType)
enum EModifyValueOperator
{
    Set           UMETA(DisplayName = "Set"),
    Add           UMETA(DisplayName = "Increment"),
    Subtract      UMETA(DisplayName = "Decrement")
};

USTRUCT(BlueprintType)
struct FCounterChangeValue
{
    GENERATED_USTRUCT_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Initialization")
    FName CounterName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Initialization")
    TEnumAsByte<EModifyValueOperator> ModifyValueOperator = EModifyValueOperator::Add;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Initialization")
    int NewValue = 1;
};


USTRUCT(BlueprintType)
struct FDialogSentence
{
    GENERATED_USTRUCT_BODY()

    FDialogSentence()
    {}
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Initialization")
    int AnswerIndex = -1;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Initialization")
    FText ChoiceText;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Initialization")
    FName NodeUserID;
};

UCLASS(Abstract, DefaultToInstanced, EditInlineNew, BlueprintType, Blueprintable)
class DIALOGASSETEDITORRUNTIME_API UAdvancedPrerequisiteBase : public UObject
{
    GENERATED_BODY()
public:

    UFUNCTION()
    void Initialize(UWorld * World)
    {
        this->_World = World;
    }
    
    UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category="Dialogue Master - Advanced prerequisites")
    bool CheckPrerequisite(AActor * NPC, AActor * Player, UDialogueMasterComponent * DialogueMasterComponent);

    virtual UWorld* GetWorld() const override
    {
        if (HasAllFlags(RF_ClassDefaultObject))
        {
            return nullptr;
        }

        if(_World != nullptr) return _World;
        
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

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Dialogue Master - Advanced prerequisites")
    FString GetDescription();

protected:
    UWorld * _World = nullptr;
};

USTRUCT(BlueprintType)
struct FDialogReplique
{
    GENERATED_USTRUCT_BODY()

    /**
     * If it is set ; this text will override the SpokenText in the dialog response choice user interface
     * it should be used if the SpokenText is too long to display in response choice dialog.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dialogue Master - Text")
    FText ShortSpokenText;

    /** The text spoken by the actor. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(MultiLine=true), Category="Dialogue Master - Text")
    FText SpokenText;

    /** The duration type :
     * Default = Default behavior => if text only, take the DefaultDuration time. If a sound is set, take the sound duration.
     * Custom duration = Allows to override the duration when a sound is set.
     * Never = Wait until the player press the skip key.
     * The default value used by this field on node creation can be set in the plugin's settings (go into the project settings).
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dialogue Master - Replique")
    ELineDurationType DurationType = ELineDurationType::DEFAULT;


    UPROPERTY(meta=(HideInDetailPanel))
    bool bIsCustomDuration = false;
    
    /** This is the duration that the text will be displayed on the screen IF there is no sound cue associated with the text. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="VoiceSound == nullptr || bIsCustomDuration"), Category="Dialogue Master - Replique")
    float DefaultDuration = 1.5;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dialogue Master - Audio")
    USoundWave * VoiceSound = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dialogue Master - Animations")
    UAnimMontage * FaceLipSyncAnimMontage = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dialogue Master - Animations")
    UAnimMontage* BodyAnimMontage = nullptr;

    /**
     * If set to true, it will not change the previously set camera (you can use it when a sequence is playing and
     * you don't want it to be interrupted but also want to continue to play dialogue lines).
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dialogue Master - Cameras")
    bool NoCameraReplacement = false;

    /**
     * Place a camera in the level to shot the speaker, if there is multiple shots in the array, it will randomly
     * pick one.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dialogue Master - Cameras", meta=(EditCondition="!NoCameraReplacement && LevelSequence == nullptr"))
    TArray<FCameraShot> CameraShotArray;
    
    /**
     * It will play the specified LevelSequence. If you want to have your camera moving to focus to a point of
     * interest while your characters are speaking, create a custom LevelSequence and set it here.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dialogue Master - Cameras", meta=(EditCondition="!NoCameraReplacement && CameraShotArray.Num() == 0"))
    ULevelSequence * LevelSequence = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dialogue Master - Prerequisites", meta=(TitleProperty="{SwitchPrerequisiteName} = {NeededValue}"))
    TArray<FSwitchDialogPrerequisite> SwitchPrerequisites;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dialogue Master - Prerequisites", meta=(TitleProperty="{CounterPrerequisiteName} {ValueCheckOperator} {NeededValue}"))
    TArray<FCounterDialogPrerequisite> CounterPrerequisites;

    UPROPERTY(Instanced, EditAnywhere, BlueprintReadWrite, Category="Dialogue Master - Prerequisites", meta=(ShowInnerProperties))
    TArray<UAdvancedPrerequisiteBase*> AdvancedPrerequisites;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(TitleProperty="{SwitchPrerequisiteName} = {NeededValue}"), Category="Dialogue Master - Actions")
    TArray<FSwitchDialogPrerequisite> UpdateSwitchValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(TitleProperty="{CounterName} {ModifyValueOperator} {NewValue}"), Category="Dialogue Master - Actions")
    TArray<FCounterChangeValue> UpdateCounterValue;

    UPROPERTY(Instanced, EditAnywhere, BlueprintReadWrite, Category="Dialogue Master - Actions", meta=(ShowInnerProperties))
    TArray<UDialogueMasterAction*> DialogActions;
    
    FDialogReplique()
    {
        
    }

};


UCLASS(BlueprintType)
class DIALOGASSETEDITORRUNTIME_API UDialogNodeInfo : public UDialogNodeInfoBase {
    GENERATED_BODY()
    
public:
    UDialogNodeInfo()
    {
        if(!NodeId.IsValid())
            NodeId = FGuid::NewGuid();

        this->Skippable = true;

        Replique.bIsCustomDuration = Replique.DurationType == ELineDurationType::CUSTOM_DURATION;
    }
    
    void SetActorIdx(int idx, FText name)
    {
        this->ActorIdx = idx;
        this->ActorName = name;
        
        //PostEditChange();
    }

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override
    {
        Super::PostEditChangeProperty(PropertyChangedEvent);

        Replique.bIsCustomDuration = Replique.DurationType == ELineDurationType::CUSTOM_DURATION;
    }
#endif
    

    FName GetNodeSwitchVariableName();
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue Master - Dialogue Node Info", meta=(EditCondition=false))
    FGuid NodeId;

    /**
     *  You can use the UserId to check if a specific dialogue line is played
     *  (coupled with Quest system task).
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue Master - Dialogue Node Info")
    FName UserId;
    
    UPROPERTY(BlueprintReadOnly, Category="Dialogue Master - Dialogue Node Info")
    int ActorIdx = 0;
    
    UPROPERTY(EditAnywhere, meta=(EditCondition="false"), Category="Dialogue Master - Dialogue Node Info")
    FText ActorName;

    /**
     *  If enabled, this node act as a branching node. A branching node check prerequisites of
     *  its output dialogue options and select the first valid options (with verified prerequisites).
     *  You can use this node to build complex dialogue graph, so NPC sounds more natural.
     */
    UPROPERTY(EditAnywhere, Category="Dialogue Master - Dialogue Node Info")
    bool isBranchingNode;

    /**
     *  If enabled, this answer will be auto selected if it is the first option with
     *  verified prerequisites AND AutoSelect enabled.
     */
    UPROPERTY(EditAnywhere, meta=(EditCondition="!isBranchingNode"), Category="Dialogue Master - Dialogue Node Info")
    bool AutoSelect;

    /**
     *  If enabled, this answer will be automatically managed by Dialogue Master so it can only be played once.
     *  This way, you don't have to manage a switch variable manually.
     */
    UPROPERTY(EditAnywhere, meta=(EditCondition="!isBranchingNode"), Category="Dialogue Master - Dialogue Node Info")
    bool PlayableOnlyOnce;

    /**
     * This parameter have effect only if SpatializedVoices is also enabled in the dialogue asset configuration.
     * If enabled, the actor voice will be spatialized.
     * You can disable it if you are using the camera to show far elements but still want to ear the actor voice.
     * Default value = true.
     */
    UPROPERTY(EditAnywhere, meta=(EditCondition="!isBranchingNode"), Category="Dialogue Master - Dialogue Node Info")
    bool SpatializedVoices = true;
    
    /**
     * If enabled, the player will be able to press a key (enter key) to skip the dialogue line.
     * Default value = true.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(EditCondition="!isBranchingNode"), Category="Dialogue Master - Dialogue Node Info")
    bool Skippable;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(EditCondition="!isBranchingNode"), Category="Dialogue Master - Dialogue Node Info")
    FDialogReplique Replique;

    /**
     * If enabled, ElevenLabs voice generation will use this node parameters instead of the actor
     * configuration's parameters.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(EditCondition="!isBranchingNode"), Category="Voices parameters override")
    bool OverrideVoiceParameters = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(EditCondition="!isBranchingNode"), Category="Voices parameters override")
    FElevenLabsParametersOverride VoiceParametersOverride;
    
    UPROPERTY()
    TArray<FText> DialogResponses;

    UFUNCTION()
    bool checkPrerequisites(UDialogueMasterComponent * DialogueMasterComponent, AActor * NPC, AActor * Player);
};
