// Copyright 2026 K-Studio. All Rights Reserved.

// KClonerAnimBakeUtils.h
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Animation/AnimSequence.h"
#include "KClonerAnimBakeUtils.generated.h"

class AKClonerActor;

/**
 * bake cloner motion into animations.
 * good if you want to bake a walk cycle with a figure-8 offset logic etc.
 */
UCLASS()
class KCLONEREDITOR_API UKClonerAnimBakeUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * bakes the modifier stack into a new sequence.
	 * applies everything to the ROOT BONE.
	 */
	UFUNCTION(BlueprintCallable, Category = "K-Cloner|Baking")
	static UAnimSequence* BakeAnimSequence(AKClonerActor* ClonerActor, UAnimSequence* SourceAnim, FString OutputPath, FString OutputName);

	UFUNCTION(BlueprintCallable, Category = "K-Cloner|Baking")
	static UAnimSequence* BakeAnimSequenceFromData(class UKClonerData* Data, UAnimSequence* OverrideAnim = nullptr);

private:
	static void ApplyModifiersToRootTrack(UAnimSequence* TargetAnim, AKClonerActor* ClonerActor);
};
