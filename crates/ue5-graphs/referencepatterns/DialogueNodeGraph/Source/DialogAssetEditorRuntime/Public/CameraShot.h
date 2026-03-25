/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#pragma once
#include "CineCameraActor.h"

#include "CameraShot.generated.h"


UCLASS(BlueprintType, Blueprintable)
class DIALOGASSETEDITORRUNTIME_API ADialogCameraActor : public ACineCameraActor
{
	GENERATED_BODY()
    
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Initialization")
	AActor * targetActor;

	void SetTargetActor(AActor * actor)
	{
		targetActor = actor;
		InitializePosition();
	}

	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category="Initialization")
	void InitializePosition();
};

USTRUCT(BlueprintType)
struct DIALOGASSETEDITORRUNTIME_API FCameraShot
{
	GENERATED_USTRUCT_BODY()
    
	/**
	 * Place a camera in the level to shot the speaker, if there is multiple shots in the array, it will randomly
	 * pick one.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dialogue Master - Cameras")
	TSubclassOf<ADialogCameraActor> Shot;
    
	/**
	 * Allow you to adjust the camera position if the original shot does not satisfy you. Instead of using this, you
	 * can also create your own camera shot by creating a child BP from DialogCameraActor class.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dialogue Master - Cameras")
	FTransform CameraAdditiveOffset = FTransform();
};