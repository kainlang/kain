// Copyright 2026 K-Studio. All Rights Reserved.

#include "KClonerValidation.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "KClonerActor.h"
#include "KClonerModifier.h"
#include "KClonerVATUtils.h"
#include "Materials/MaterialInstanceDynamic.h"


namespace KClonerValidation {
void ValidateVATNormals(UWorld *World) {
  if (!World)
    return;
  FActorSpawnParameters Params;
  Params.SpawnCollisionHandlingOverride =
      ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  AKClonerActor *A = World->SpawnActor<AKClonerActor>(
      AKClonerActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator,
      Params);
  if (!A)
    return;
  A->SourceMesh =
      LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
  A->DistributionLayers.Empty();
  FKClonerDistributionLayer L;
  L.Mode = EKClonerMode::Grid;
  L.GridCount = FIntVector(10, 10, 1);
  L.GridSpacing = FVector(100.0f);
  L.bEnabled = true;
  A->DistributionLayers.Add(L);
  A->ForceRebuild();
  FKClonerVATOptions Opt;
  Opt.Duration = 1.0f;
  Opt.FrameRate = 30.0f;
  Opt.bBakeRotation = true;
  FKClonerVATResult R = FKClonerVATUtils::BakeToVAT(A, Opt);
  if (!R.bSuccess || !R.PositionTexture)
    return;
  A->VATBaseMaterial = Cast<UMaterialInterface>(R.VATMaterial);
  A->VATPositionTexture = R.PositionTexture;
  A->VATRotationTexture = R.RotationTexture;
  A->SkeletalMode = EKClonerSkeletalMode::VATBaked;
  A->Tick(0.0f);
}
void ValidateTextureSampling(UWorld *World) {
  if (!World)
    return;
  FActorSpawnParameters Params;
  Params.SpawnCollisionHandlingOverride =
      ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  AKClonerActor *A = World->SpawnActor<AKClonerActor>(
      AKClonerActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator,
      Params);
  if (!A)
    return;
  A->SourceMesh =
      LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
  A->DistributionLayers.Empty();
  FKClonerDistributionLayer L;
  L.Mode = EKClonerMode::Grid;
  L.GridCount = FIntVector(50, 50, 1);
  L.GridSpacing = FVector(50.0f);
  L.bEnabled = true;
  A->DistributionLayers.Add(L);
  UKClonerModifier_Texture *M = NewObject<UKClonerModifier_Texture>(A);
  M->SourceTexture = LoadObject<UTexture2D>(
      nullptr, TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"));
  M->Mode = EKClonerAudioMode::Position;
  M->Direction = FVector(0, 0, 50);
  M->Strength = 1.0f;
  M->Tiling = FVector2D(0.02f, 0.02f);
  M->bBilinearFiltering = true;
  A->Modifiers.Add(M);
  A->ForceRebuild();
  A->Tick(0.0f);
}
} // namespace KClonerValidation
