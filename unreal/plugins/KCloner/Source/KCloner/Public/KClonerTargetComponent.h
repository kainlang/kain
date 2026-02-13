// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "Components/SceneComponent.h"
#include "CoreMinimal.h"
#include "KClonerTargetComponent.generated.h"


/**
 * Component that marks an actor (or a specific part of it) as a Target for
 * K-Cloner. Used by the Target Modifier to orient clones towards this location.
 */
UCLASS(ClassGroup = (KStudio),
       meta = (BlueprintSpawnableComponent, DisplayName = "KClonerTarget"))
class KCLONER_API UKClonerTargetComponent : public USceneComponent {
  GENERATED_BODY()

public:
  UKClonerTargetComponent();

  /** Strength of the attraction/look-at (0-1) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target",
            meta = (ClampMin = "0.0", ClampMax = "1.0"))
  float Strength = 1.0f;

  /** If true, this target is currently active */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target")
  bool bEnabled = true;

  /** Priority if multiple targets are active (higher number = primary target)
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target")
  int32 Priority = 0;

  // Helper to get world location
  FVector GetTargetLocation() const { return GetComponentLocation(); }
};
