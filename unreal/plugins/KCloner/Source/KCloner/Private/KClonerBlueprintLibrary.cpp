// Copyright 2026 K-Studio. All Rights Reserved.

#include "KClonerBlueprintLibrary.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

// ======= FIND CLONERS =======
// useful for when you need to talk to a cloner from a random BP

TArray<AKClonerActor *>
UKClonerBlueprintLibrary::GetAllKCloners(const UObject *WorldContextObject) {
  TArray<AKClonerActor *> Result;

  if (!WorldContextObject) {
    return Result;
  }

  UWorld *World = GEngine->GetWorldFromContextObject(
      WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
  if (!World) {
    return Result;
  }

  // just loop through all actors in the world and grab the cloners
  for (TActorIterator<AKClonerActor> It(World); It; ++It) {
    Result.Add(*It);
  }

  return Result;
}

AKClonerActor *
UKClonerBlueprintLibrary::GetKClonerByTag(const UObject *WorldContextObject,
                                          FName Tag) {
  TArray<AKClonerActor *> Cloners = GetAllKCloners(WorldContextObject);

  for (AKClonerActor *Cloner : Cloners) {
    if (Cloner && Cloner->ActorHasTag(Tag)) {
      return Cloner;
    }
  }

  return nullptr;
}

AKClonerActor *
UKClonerBlueprintLibrary::GetNearestKCloner(const UObject *WorldContextObject,
                                            FVector WorldLocation) {
  TArray<AKClonerActor *> Cloners = GetAllKCloners(WorldContextObject);

  AKClonerActor *Nearest = nullptr;
  float NearestDistSq = TNumericLimits<float>::Max();

  for (AKClonerActor *Cloner : Cloners) {
    if (Cloner) {
      float DistSq =
          FVector::DistSquared(Cloner->GetActorLocation(), WorldLocation);
      if (DistSq < NearestDistSq) {
        NearestDistSq = DistSq;
        Nearest = Cloner;
      }
    }
  }

  // simple dist check, find the closest one
  return Nearest;
}

int32 UKClonerBlueprintLibrary::GetTotalCloneCount(
    const UObject *WorldContextObject) {
  TArray<AKClonerActor *> Cloners = GetAllKCloners(WorldContextObject);
  int32 Total = 0;

  for (AKClonerActor *Cloner : Cloners) {
    if (Cloner) {
      Total += Cloner->GetInstanceCount();
    }
  }

  return Total;
}

// ======= BULK OPS =======
// do stuff to every cloner at once because why not lol

void UKClonerBlueprintLibrary::RebuildAllKCloners(
    const UObject *WorldContextObject) {
  TArray<AKClonerActor *> Cloners = GetAllKCloners(WorldContextObject);

  for (AKClonerActor *Cloner : Cloners) {
    if (Cloner) {
      Cloner->ForceRebuild();
    }
  }
}

void UKClonerBlueprintLibrary::SetAllKClonersTimeScale(
    const UObject *WorldContextObject, float TimeScale) {
  TArray<AKClonerActor *> Cloners = GetAllKCloners(WorldContextObject);

  for (AKClonerActor *Cloner : Cloners) {
    if (Cloner) {
      Cloner->SetTimeScale(TimeScale);
    }
  }
}

void UKClonerBlueprintLibrary::PauseAllKCloners(
    const UObject *WorldContextObject) {
  SetAllKClonersTimeScale(WorldContextObject, 0.0f);
}

void UKClonerBlueprintLibrary::ResumeAllKCloners(
    const UObject *WorldContextObject) {
  SetAllKClonersTimeScale(WorldContextObject, 1.0f);
}

// ======= CLICKING SHIT =======
// math for finding which clone you clicked on
// since they don't have individual collision components (too slow)
// we manually raycast against their cached transforms

int32 UKClonerBlueprintLibrary::RaycastToClone(
    const UObject *WorldContextObject, AKClonerActor *Cloner, FVector RayStart,
    FVector RayEnd, FVector &HitLocation) {
  HitLocation = FVector::ZeroVector;

  if (!Cloner) {
    return INDEX_NONE;
  }

  // Get all clone transforms and find the closest one to the ray
  int32 CloneCount = Cloner->GetInstanceCount();
  if (CloneCount == 0) {
    return INDEX_NONE;
  }

  FVector RayDir = (RayEnd - RayStart).GetSafeNormal();
  float RayLength = FVector::Dist(RayStart, RayEnd);

  int32 ClosestIndex = INDEX_NONE;
  float ClosestDistToRay = TNumericLimits<float>::Max();
  float CloneRadius = 50.0f; // approximate size of a clone, good enough lol

  for (int32 i = 0; i < CloneCount; i++) {
    FTransform CloneTransform = Cloner->GetCloneTransform(i);
    FVector ClonePos = CloneTransform.GetLocation();

    // find distance from the ray to the clone center
    FVector ToClone = ClonePos - RayStart;
    float ProjectedDist = FVector::DotProduct(ToClone, RayDir);

    // Skip if behind ray start or beyond ray end
    if (ProjectedDist < 0.0f || ProjectedDist > RayLength) {
      continue;
    }

    // Calculate perpendicular distance from clone to ray
    FVector ClosestPointOnRay = RayStart + (RayDir * ProjectedDist);
    float DistToRay = FVector::Dist(ClonePos, ClosestPointOnRay);

    // Check if within approximate hit radius
    FVector CloneScale = CloneTransform.GetScale3D();
    float EffectiveRadius = CloneRadius * CloneScale.GetMax();

    if (DistToRay < EffectiveRadius && DistToRay < ClosestDistToRay) {
      ClosestDistToRay = DistToRay;
      ClosestIndex = i;
      HitLocation = ClosestPointOnRay;
    }
  }

  return ClosestIndex;
}

int32 UKClonerBlueprintLibrary::GetCloneAtScreenPosition(
    const UObject *WorldContextObject, AKClonerActor *Cloner,
    FVector2D ScreenPosition) {
  if (!WorldContextObject || !Cloner) {
    return INDEX_NONE;
  }

  UWorld *World = GEngine->GetWorldFromContextObject(
      WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
  if (!World) {
    return INDEX_NONE;
  }

  APlayerController *PC = World->GetFirstPlayerController();
  if (!PC) {
    return INDEX_NONE;
  }

  // Deproject screen position to world ray
  FVector WorldLocation, WorldDirection;
  if (!UGameplayStatics::DeprojectScreenToWorld(
          PC, ScreenPosition, WorldLocation, WorldDirection)) {
    return INDEX_NONE;
  }

  FVector HitLocation;
  return RaycastToClone(WorldContextObject, Cloner, WorldLocation,
                        WorldLocation + (WorldDirection * 100000.0f),
                        HitLocation);
}

// ======= MATH HELPERS =======
// stuff that's useful for custom modifier expressions

float UKClonerBlueprintLibrary::Remap(float Value, float InMin, float InMax,
                                      float OutMin, float OutMax) {
  if (FMath::IsNearlyEqual(InMax, InMin)) {
    return OutMin;
  }
  // standard remap math, standard mograph stuff
  float T = (Value - InMin) / (InMax - InMin);
  return OutMin + T * (OutMax - OutMin);
}

float UKClonerBlueprintLibrary::PingPong(float Time, float Period) {
  if (Period <= 0.0f) {
    return 0.0f;
  }
  float T = FMath::Fmod(Time, Period * 2.0f);
  if (T > Period) {
    T = (Period * 2.0f) - T;
  }
  // loops 0 to 1 back to 0
  return T / Period;
}

float UKClonerBlueprintLibrary::SmoothStep(float Value) {
  Value = FMath::Clamp(Value, 0.0f, 1.0f);
  return Value * Value * (3.0f - 2.0f * Value);
}

FVector UKClonerBlueprintLibrary::LissajousCurve(float Time, float FreqX,
                                                 float FreqY, float FreqZ,
                                                 float Amplitude) {
  // weird math curve that looks cool in mograph
  return FVector(FMath::Sin(Time * FreqX) * Amplitude,
                 FMath::Sin(Time * FreqY) * Amplitude,
                 FMath::Sin(Time * FreqZ) * Amplitude);
}

FVector UKClonerBlueprintLibrary::Figure8Curve(float Time, float Width,
                                               float Height) {
  // figure-8 symbol - math is a bit dense but it works lol
  float Denom = 1.0f + FMath::Square(FMath::Sin(Time));
  float X = Width * FMath::Cos(Time) / Denom;
  float Y = Height * FMath::Sin(Time) * FMath::Cos(Time) / Denom;
  return FVector(X, Y, 0.0f);
}
