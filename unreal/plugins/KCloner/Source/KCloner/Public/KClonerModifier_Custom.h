// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "KClonerModifier.h"
#include "KClonerModifier_Custom.generated.h"

/**
 * Custom Blueprint Modifier: Allows creating modifiers entirely in Blueprint.
 * Override the "Apply Custom Effect" event in Blueprint to define the behavior.
 * This is the most flexible option for designers who want to create unique
 * effects without writing C++.
 *
 * Usage:
 * 1. Create a Blueprint class that inherits from this modifier
 * 2. Override the "Apply Custom Effect" event
 * 3. Modify PositionOffset, RotationOffset, ScaleMultiplier, and CloneColor
 * 4. Add your custom modifier Blueprint to a K-Cloner Actor's modifier stack
 */
UCLASS(Blueprintable, DisplayName = "Custom (Blueprint)")
class KCLONER_API UKClonerModifier_Custom : public UKClonerModifier {
  GENERATED_BODY()

public:
  UKClonerModifier_Custom();

  // --- CUSTOM DATA EXPOSED TO BLUEPRINT ---

  /** Current clone's position offset - modify this in Blueprint */
  UPROPERTY(BlueprintReadWrite, Category = "Custom|Transform")
  FVector PositionOffset = FVector::ZeroVector;

  /** Current clone's rotation offset - modify this in Blueprint */
  UPROPERTY(BlueprintReadWrite, Category = "Custom|Transform")
  FRotator RotationOffset = FRotator::ZeroRotator;

  /** Current clone's scale multiplier - modify this in Blueprint */
  UPROPERTY(BlueprintReadWrite, Category = "Custom|Transform")
  FVector ScaleMultiplier = FVector::OneVector;

  /** Current clone's color (custom data) - modify this in Blueprint */
  UPROPERTY(BlueprintReadWrite, Category = "Custom|Data")
  FLinearColor CloneColor = FLinearColor::White;

  // --- USER PARAMETERS ---

  /** Custom float parameters that can be set in the Details panel */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom|Parameters")
  TArray<float> FloatParams;

  /** Custom vector parameters */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom|Parameters")
  TArray<FVector> VectorParams;

  /** Custom name for this modifier instance (displayed in UI) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom|Settings")
  FString DisplayName = TEXT("Custom Modifier");

protected:
  /**
   * Override this event in Blueprint to create your custom effect!
   * Called once per clone, per frame. Modify PositionOffset, RotationOffset,
   * ScaleMultiplier, and CloneColor to affect the clone.
   *
   * @param Index - The index of this clone (0 to Count-1)
   * @param Count - Total number of clones
   * @param Time - Current animation time in seconds
   * @param NormalizedIndex - Index as 0.0 to 1.0 (useful for gradients)
   */
  UFUNCTION(BlueprintImplementableEvent, Category = "Custom",
            meta = (DisplayName = "Apply Custom Effect"))
  void ReceiveApplyCustomEffect(int32 Index, int32 Count, float Time,
                                float NormalizedIndex);

  /** Native implementation (calls Blueprint event) */
  virtual void ApplyBehavior(FTransform &Transform, int32 Index, int32 Count,
                             float &Time, TArray<float> &CustomData) override;
};
