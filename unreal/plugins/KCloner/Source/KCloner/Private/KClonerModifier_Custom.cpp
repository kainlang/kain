// Copyright 2026 K-Studio. All Rights Reserved.

#include "KClonerModifier_Custom.h"

UKClonerModifier_Custom::UKClonerModifier_Custom() {
  // set up some default slots so it's not empty
  FloatParams.SetNum(4);
  VectorParams.SetNum(2);
}

void UKClonerModifier_Custom::ApplyBehavior(FTransform &Transform, int32 Index,
                                            int32 Count, float &Time,
                                            TArray<float> &CustomData) {
  // clear everything out so we don't apply garbage
  PositionOffset = FVector::ZeroVector;
  RotationOffset = FRotator::ZeroRotator;
  ScaleMultiplier = FVector::OneVector;
  CloneColor = FLinearColor::White;

  // Calculate normalized index (0.0 to 1.0)
  float NormalizedIndex =
      (Count > 1) ? (float)Index / (float)(Count - 1) : 0.0f;

  // CALL BLUEPRINT - this is where the user logic happens
  ReceiveApplyCustomEffect(Index, Count, Time, NormalizedIndex);

  // apply whatever the user set in BP
  // Position
  FVector CurrentPos = Transform.GetLocation();
  Transform.SetLocation(CurrentPos + PositionOffset);

  // Rotation
  FQuat CurrentRot = Transform.GetRotation();
  FQuat OffsetRot = RotationOffset.Quaternion();
  Transform.SetRotation(CurrentRot * OffsetRot);

  // Scale
  FVector CurrentScale = Transform.GetScale3D();
  Transform.SetScale3D(CurrentScale * ScaleMultiplier);

  // Custom data (color)
  if (CustomData.Num() >= 3) {
    CustomData[0] = CloneColor.R;
    CustomData[1] = CloneColor.G;
    CustomData[2] = CloneColor.B;
  }
}
