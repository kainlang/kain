/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#pragma once

#include "CoreMinimal.h"
#include "DialogRuntimeGraph.h"
#include <functional>
#include "ActorInfo.h"
#include "DialogAsset.generated.h"

UCLASS(BlueprintType)
class DIALOGASSETEDITORRUNTIME_API UDialogAsset : public UObject {
    GENERATED_BODY()

public: // Properties
    UDialogAsset()
    {
        if(!AssetId.IsValid())
            AssetId = FGuid::NewGuid();
    }
    
    UPROPERTY(EditAnywhere, Category="Asset info", meta=(EditCondition=false))
    FGuid AssetId;
    
    //UPROPERTY(EditAnywhere)
    //FString DialogName = TEXT("Enter dialog name here");

    UPROPERTY(EditAnywhere, Category="Speakers", meta=(TitleProperty="{ActorIdentifier}"))
    TArray<FActorInfo> DialogActors;

    UPROPERTY(EditAnywhere, Category="Speakers", meta=(TitleProperty="{ActorIdentifier}"))
    FActorInfo PlayerActor;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Configuration")
    bool FreeMovement = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Configuration")
    bool SpatializedVoices = true;

    /**
     * When this property is set to true, the animations on the player
     * character are automatically cancelled when there is a choice to
     * proceed in the dialogue tree.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Configuration")
    bool StopPlayerAnimationDuringPlayerChoice = true;

    //UPROPERTY(EditAnywhere, Category="Configuration")
    //bool CanBeExited;

    //UPROPERTY(EditAnywhere, Category="Configuration")
    //bool AutoRotateSpeakers;

    //UPROPERTY(EditAnywhere, Category="Configuration")
    //bool AutoStopMovement;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera")
    bool SpawnDialogCamera = true;

    UPROPERTY(EditAnywhere, Category="Camera")
    float DialogCameraBlendTime = 0.0;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="Dialogue Master - Dialogue")
    void IsMainDialogue(bool &MainDialogue)
    {
        MainDialogue = SpawnDialogCamera || !FreeMovement;
    }
    
    FActorInfo* getActorInfoFromIndex(int actorIdx)
    {
        if(actorIdx == 0) return &PlayerActor;

        if(actorIdx >= 1 && actorIdx - 1 < DialogActors.Num())
        {
            return &DialogActors[actorIdx - 1];
        }
        
        return nullptr;
    }

    int getActorIdxFromActorName(FText name)
    {
        if(PlayerActor.ActorIdentifier.EqualToCaseIgnored(name))
            return 0;

        for(int i = 0; i < DialogActors.Num(); i++)
        {
            if(DialogActors[i].ActorIdentifier.EqualToCaseIgnored(name))
                return i + 1;
        }

        return -1;
    }

    int getNPCManualReferenceIndexForNPC(int actorIdx)
    {
        // If it is not the player :
        if(actorIdx > 0 && actorIdx - 1 < DialogActors.Num())
        {
            int manualReferenceCounter = 0;
            for(int i = 0; i < actorIdx - 1; i++)
            {
                if(DialogActors[i].NPCLinkingMethod == ManualReferencing)
                {
                    manualReferenceCounter++;
                }
            }

            return manualReferenceCounter;
        }
        
        return -1;  // Invalid index
    }
    
    UPROPERTY()
    UDialogRuntimeGraph* Graph = nullptr;

public: // Our interface
    void SetPreSaveListener(std::function<void()> onPreSaveListener) { _onPreSaveListener = onPreSaveListener; }

public: // UObject interface
    virtual void PreSave(FObjectPreSaveContext saveContext) override;
    virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
private: // Members
    std::function<void()> _onPreSaveListener = nullptr;
};
