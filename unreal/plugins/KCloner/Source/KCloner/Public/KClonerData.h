// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "KClonerTypes.h"
#include "Engine/EngineTypes.h"
#include "Engine/Texture2D.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Animation/AnimSequence.h"
#include "KClonerModifier.h"
#include "KClonerData.generated.h"

/**
 * The main data asset for K-Cloner.
 * Double-click this to open the dedicated editor window.
 */
UCLASS(BlueprintType)
class KCLONER_API UKClonerData : public UObject
{
	GENERATED_BODY()

public:
	// static mesh to clone (cubes, rocks, etc)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General")
	UStaticMesh* SourceMesh;

	// or a skeletal mesh if you're feeling fancy (and have GPU budget)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General")
	USkeletalMesh* SourceSkeletalMesh;

	// animation to play on skeletal clones
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	UAnimSequence* SourceAnimSequence;

	// speed up or slow down everything
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	float TimeScale = 1.0f;

	// where the clones go (grid, circle, spline, etc)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layers")
	TArray<FKClonerDistributionLayer> Layers;

	// THE STACK - filters and deformers
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "Modifiers")
	TArray<UKClonerModifier*> Modifiers;

	/** Output directory for baked animations */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimTweak", meta = (EditCondition = "bAnimTweakMode", ContentDir))
	FDirectoryPath OutputFolder;

	/** Name for the new animation asset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimTweak", meta = (EditCondition = "bAnimTweakMode"))
	FString OutputName = TEXT("M_TweakedAnim");

	/** Enable Anim Tweak Mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimTweak")
	bool bAnimTweakMode = false;
};
