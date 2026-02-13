// Copyright 2026 K-Studio. All Rights Reserved.

#include "KClonerModifier.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "ConstantQNRT.h"
#include "Curves/CurveFloat.h"
#include "KClonerExpressionEvaluator.h"
#include "Math/RandomStream.h"
#include "TextureResource.h"

UKClonerModifier::UKClonerModifier() {
  if (!HasAnyFlags(RF_Transactional)) {
    SetFlags(RF_Transactional);
  }
  if (!ModifierGuid.IsValid()) {
    ModifierGuid = FGuid::NewGuid();
  }
}

void UKClonerModifier::ApplyModifier(FTransform &Transform, int32 Index,
                                     int32 Count, float &Time,
                                     TArray<float> &CustomData) {
  if (!bEnabled || Influence <= 0.0f)
    return;
  ApplyBehavior(Transform, Index, Count, Time, CustomData);
}

// =========== ORBIT ===========
// classic rotation around axis, ported from my DCC setup

UKClonerModifier_Orbit::UKClonerModifier_Orbit() { Axis = FVector(0, 0, 1); }

void UKClonerModifier_Orbit::ApplyBehavior(FTransform &Transform, int32 Index,
                                           int32 Count, float &Time,
                                           TArray<float> &CustomData) {
  float Angle = (Time * Speed) + (Index * Step);
  Angle *= Influence;

  // FQuat wants radians you dingus, dont forget this
  FQuat Rotation = FQuat(
      Axis.GetSafeNormal(),
      FMath::DegreesToRadians(Angle));

  Transform.SetRotation(Transform.GetRotation() * Rotation);
}

// =========== FLOAT ===========
// makes shit bob up and down basically

UKClonerModifier_Float::UKClonerModifier_Float() {
  Direction = FVector(0, 0, 1);
}

void UKClonerModifier_Float::ApplyBehavior(FTransform &Transform, int32 Index,
                                           int32 Count, float &Time,
                                           TArray<float> &CustomData) {
  float Phase = (Time * Speed) + (Index * Frequency);
  float Offset = FMath::Sin(Phase) * Amplitude * Influence;

  Transform.AddToTranslation(Direction.GetSafeNormal() * Offset);
}

// =========== PULSE ===========

UKClonerModifier_Pulse::UKClonerModifier_Pulse() {}

void UKClonerModifier_Pulse::ApplyBehavior(FTransform &Transform, int32 Index,
                                           int32 Count, float &Time,
                                           TArray<float> &CustomData) {
  float Phase = (Time * Speed) + (Index * Frequency);
  float ScaleFactor = FMath::Sin(Phase) * ScaleAmount * Influence;
  float FinalScale = 1.0f + ScaleFactor;

  if (bUniform) {
    Transform.SetScale3D(Transform.GetScale3D() * FinalScale);
  } else {
    Transform.SetScale3D(Transform.GetScale3D() * FinalScale);
  }
}

// =========== DELAY ===========
// THIS IS IMPORTANT: shifts time for ALL modifiers after it in the stack

UKClonerModifier_Delay::UKClonerModifier_Delay() {}

void UKClonerModifier_Delay::ApplyBehavior(FTransform &Transform, int32 Index,
                                           int32 Count, float &Time,
                                           TArray<float> &CustomData) {
  Time -= Index * DelayAmount * Influence;
}

// =========== RANDOM ===========

UKClonerModifier_Random::UKClonerModifier_Random() {}

void UKClonerModifier_Random::ApplyBehavior(FTransform &Transform, int32 Index,
                                            int32 Count, float &Time,
                                            TArray<float> &CustomData) {
  FRandomStream RNG(Seed + Index); // seed + index = deterministic random per clone
  if (!Position.IsZero()) {
    FVector RandPos(RNG.FRandRange(-1, 1), RNG.FRandRange(-1, 1),
                    RNG.FRandRange(-1, 1));
    Transform.AddToTranslation(RandPos * Position * Influence);
  }


  if (!Rotation.IsZero()) {
    FRotator RandRot(RNG.FRandRange(-1, 1) * Rotation.Pitch,
                     RNG.FRandRange(-1, 1) * Rotation.Yaw,
                     RNG.FRandRange(-1, 1) * Rotation.Roll);
    FQuat AddRot(RandRot * Influence);
    Transform.SetRotation(Transform.GetRotation() * AddRot);
  }


  if (!Scale.IsZero()) {
    FVector RandScale;
    if (bUniformScale) {
      float S = RNG.FRandRange(-1, 1);
      RandScale = FVector(S, S, S);
    } else {
      RandScale = FVector(RNG.FRandRange(-1, 1), RNG.FRandRange(-1, 1),
                          RNG.FRandRange(-1, 1));
    }
    Transform.SetScale3D(Transform.GetScale3D() +
                         (RandScale * Scale * Influence));
  }
}

// =========== SHAKE ===========
// perlin noise jitter - looks way better than random

UKClonerModifier_Shake::UKClonerModifier_Shake() {}

void UKClonerModifier_Shake::ApplyBehavior(FTransform &Transform, int32 Index,
                                           int32 Count, float &Time,
                                           TArray<float> &CustomData) {
  float T = Time * Speed;
  float I = Index * 0.13f; // magic number but it works lol


  if (!PositionStrength.IsZero()) {
    float NX = FMath::PerlinNoise3D(FVector(T, I, 0.0f)) - 0.5f;
    float NY = FMath::PerlinNoise3D(FVector(T, I, 10.0f)) - 0.5f;
    float NZ = FMath::PerlinNoise3D(FVector(T, I, 20.0f)) - 0.5f;

    FVector ShakePos(NX, NY, NZ);
    // *2 cuz perlin returns -0.5 to 0.5, we want -1 to 1 ish
    Transform.AddToTranslation(ShakePos * 2.0f * PositionStrength * Influence);
  }


  if (!RotationStrength.IsZero()) {
    float RP = FMath::PerlinNoise3D(FVector(T, I, 30.0f)) - 0.5f;
    float RY = FMath::PerlinNoise3D(FVector(T, I, 40.0f)) - 0.5f;
    float RR = FMath::PerlinNoise3D(FVector(T, I, 50.0f)) - 0.5f;

    FRotator ShakeRot(RP * RotationStrength.X, RY * RotationStrength.Y,
                      RR * RotationStrength.Z);
    FQuat AddRot(ShakeRot * Influence * 2.0f);
    Transform.SetRotation(Transform.GetRotation() * AddRot);
  }
}

// =========== WAVE ===========

UKClonerModifier_Wave::UKClonerModifier_Wave() {}

void UKClonerModifier_Wave::ApplyBehavior(FTransform &Transform, int32 Index,
                                          int32 Count, float &Time,
                                          TArray<float> &CustomData) {
  float Phase = (Time * Speed) + (Index * Frequency);
  float WaveVal = FMath::Sin(Phase) * Amplitude * Influence;

  Transform.AddToTranslation(Direction.GetSafeNormal() * WaveVal);
}

// =========== STEP ===========
// dead simple accumulator - each clone stacks on prev

UKClonerModifier_Step::UKClonerModifier_Step() {}

void UKClonerModifier_Step::ApplyBehavior(FTransform &Transform, int32 Index,
                                          int32 Count, float &Time,
                                          TArray<float> &CustomData) {
  if (!PositionStep.IsZero()) {
    Transform.AddToTranslation(PositionStep * Index * Influence);
  }

  if (!RotationStep.IsZero()) {
    FQuat StepRot(RotationStep * Index * Influence);
    Transform.SetRotation(Transform.GetRotation() * StepRot);
  }

  if (!ScaleStep.IsZero()) {
    Transform.SetScale3D(Transform.GetScale3D() +
                         (ScaleStep * Index * Influence));
  }
}

// =========== ELASTIC ===========
// bouncy spring type shit, looks dope for reveals

UKClonerModifier_Elastic::UKClonerModifier_Elastic() {}

void UKClonerModifier_Elastic::ApplyBehavior(FTransform &Transform, int32 Index,
                                             int32 Count, float &Time,
                                             TArray<float> &CustomData) {
  float CycleTime = FMath::Fmod(Time * Speed + (Index * 0.2f), 2.0f * UE_PI);
  float DampeningFactor = FMath::Exp(-Damping * CycleTime);
  float Oscillation =
      FMath::Sin(CycleTime * 2.0f) * Amplitude * DampeningFactor * Influence;

  Transform.AddToTranslation(Axis.GetSafeNormal() * Oscillation);
}

// =========== TUMBLE ===========

UKClonerModifier_Tumble::UKClonerModifier_Tumble() {}

void UKClonerModifier_Tumble::ApplyBehavior(FTransform &Transform, int32 Index,
                                            int32 Count, float &Time,
                                            TArray<float> &CustomData) {
  FRandomStream RNG(Index);
  float OffsetX = RNG.FRand() * RandomOffset;
  float OffsetY = RNG.FRand() * RandomOffset;
  float OffsetZ = RNG.FRand() * RandomOffset;

  FRotator TumbleRot((Time + OffsetX) * RotationSpeed.X,
                     (Time + OffsetY) * RotationSpeed.Y,
                     (Time + OffsetZ) * RotationSpeed.Z);

  FQuat AddRot(TumbleRot * Influence);
  Transform.SetRotation(Transform.GetRotation() * AddRot);
}

// =========== VORTEX ===========
// swirly boi - great for portals and shit

UKClonerModifier_Vortex::UKClonerModifier_Vortex() {}

void UKClonerModifier_Vortex::ApplyBehavior(FTransform &Transform, int32 Index,
                                            int32 Count, float &Time,
                                            TArray<float> &CustomData) {
  FVector Pos = Transform.GetLocation();
  FVector AxisNorm = Axis.GetSafeNormal();
  FVector ProjOnAxis = FVector::DotProduct(Pos, AxisNorm) * AxisNorm;
  FVector ToAxis = ProjOnAxis - Pos;
  float Dist = ToAxis.Size();

  if (Dist > Radius || Dist < KINDA_SMALL_NUMBER)
    return;

  float Strength = (1.0f - (Dist / Radius)) * Influence;

  // swirl em around
  float SwirlAngle = Time * RotationSpeed * Strength;
  FQuat SwirlRot(AxisNorm, FMath::DegreesToRadians(SwirlAngle));

  FVector Offset = Pos - ProjOnAxis;
  FVector SwirledPos = SwirlRot.RotateVector(Offset) + ProjOnAxis;

  // suck em towards the center
  FVector PulledPos =
      SwirledPos + (ToAxis.GetSafeNormal() * PullStrength * Strength * 50.0f);

  Transform.SetLocation(PulledPos);
}

// =========== K-SCRIPT ===========
// THE GOOD SHIT - custom expressions like my DCC had
// not a full parser but handles the common patterns i use

UKClonerModifier_KScript::UKClonerModifier_KScript() {
  Variables.Add(1.0f); // Default v[0]
  Variables.Add(0.1f); // Default v[1]
  BuildCache();
}


void UKClonerModifier_KScript::ApplyBehavior(FTransform &Transform, int32 Index,
                                             int32 Count, float &Time,
                                             TArray<float> &CustomData) {
  if (Code.IsEmpty())
    return;

  FVector P = Transform.GetLocation();
  FVector S = Transform.GetScale3D();
  FRotator R = Transform.Rotator();
  float t = Time;
  float i = (float)Index;
  float c = (float)Count;

  if (bOpSinPY) {
    float v0 = Variables.IsValidIndex(0) ? Variables[0] : 1.0f;
    float v1 = Variables.IsValidIndex(1) ? Variables[1] : 0.1f;
    float v2 = Variables.IsValidIndex(2) ? Variables[2] : 10.0f;

    P.Y += FMath::Sin(t * v0 + i * v1) * v2 * Influence;
  }

  if (bOpSinPZ) {
    float v0 = Variables.IsValidIndex(0) ? Variables[0] : 1.0f;
    float v1 = Variables.IsValidIndex(1) ? Variables[1] : 0.1f;
    float v2 = Variables.IsValidIndex(2) ? Variables[2] : 10.0f;

    P.Z += FMath::Sin(t * v0 + i * v1) * v2 * Influence;
  }

  if (bOpPulseScale) {
    float v0 = Variables.IsValidIndex(0) ? Variables[0] : 1.0f;
    float pulse = (1.0f + FMath::Sin(t * v0 + i * 0.1f) * 0.2f);
    S *= pulse;
  }

  if (!FMath::IsNearlyZero(CachedAddPY)) {
    P.Y += CachedAddPY * Influence;
  }
  if (!FMath::IsNearlyZero(CachedAddPZ)) {
    P.Z += CachedAddPZ * Influence;
  }

  Transform.SetLocation(P);
  Transform.SetScale3D(S);
}

#if WITH_EDITOR
void UKClonerModifier_KScript::PostEditChangeProperty(
    FPropertyChangedEvent &PropertyChangedEvent) {
  Super::PostEditChangeProperty(PropertyChangedEvent);
  BuildCache();
}
#endif

void UKClonerModifier_KScript::BuildCache() {
  FString Src = Code;
  FString Lower = Src.TrimStartAndEnd().ToLower();
  bOpSinPY = Lower.Contains(TEXT("p.y += sin"));
  bOpSinPZ = Lower.Contains(TEXT("p.z += sin"));
  bOpPulseScale = Lower.Contains(TEXT("s *= pulse"));
  CachedAddPY = 0.0f;
  CachedAddPZ = 0.0f;
  if (Lower.Contains(TEXT("+="))) {
    TArray<FString> Lines;
    Src.ParseIntoArrayLines(Lines, true);
    for (const FString &Line : Lines) {
      FString L = Line.TrimStartAndEnd().ToLower();
      if (L.IsEmpty() || L.StartsWith(TEXT("//")))
        continue;
      if (L.Contains(TEXT("p.y +=")) && !L.Contains(TEXT("sin"))) {
        int32 OpPos = L.Find(TEXT("+="));
        if (OpPos != INDEX_NONE) {
          FString Right = L.RightChop(OpPos + 2).TrimStartAndEnd();
          CachedAddPY = FCString::Atof(*Right);
        }
      } else if (L.Contains(TEXT("p.z +=")) && !L.Contains(TEXT("sin"))) {
        int32 OpPos = L.Find(TEXT("+="));
        if (OpPos != INDEX_NONE) {
          FString Right = L.RightChop(OpPos + 2).TrimStartAndEnd();
          CachedAddPZ = FCString::Atof(*Right);
        }
      }
    }
  }
}

// =========== COLOR ===========
// sets per-instance color via custom data channels
// your material needs to READ customdata or this does jack shit

UKClonerModifier_Color::UKClonerModifier_Color() {}

void UKClonerModifier_Color::ApplyBehavior(FTransform &Transform, int32 Index,
                                           int32 Count, float &Time,
                                           TArray<float> &CustomData) {
  FLinearColor FinalColor = Color;

  if (bRandomize) {
    FRandomStream RNG(Index);
    FinalColor = FLinearColor::MakeRandomColor();
  }
  FinalColor =
      FLinearColor::LerpUsingHSV(FLinearColor::White, FinalColor, Influence);

  if (CustomData.Num() < 3) {
    CustomData.SetNum(3);
  }

  CustomData[0] = FinalColor.R;
  CustomData[1] = FinalColor.G;
  CustomData[2] = FinalColor.B;
}

// ============================================================
// FORCE MODIFIERS - the heavy hitters
// ============================================================

// =========== NOISE (Curl) ===========
// divergence-free noise so it looks like actual fluid motion
// not that shitty raw perlin everyone uses

UKClonerModifier_Noise::UKClonerModifier_Noise() {}

// ghetto noise using trig, close enough to perlin for our purposes
static FVector SimpleNoise3D(const FVector &P, float Time) {
  // bunch of primes to make it look random-ish

  float X = FMath::Sin(P.X * 1.27f + P.Y * 2.11f + P.Z * 0.87f + Time * 0.73f);
  float Y = FMath::Sin(P.X * 0.91f + P.Y * 1.53f + P.Z * 2.31f + Time * 1.17f);
  float Z = FMath::Sin(P.X * 2.03f + P.Y * 0.77f + P.Z * 1.41f + Time * 0.91f);
  return FVector(X, Y, Z);
}

// curl = cross product of gradient, makes it swirly not blobby
static FVector CurlNoise3D(const FVector &P, float Freq, float Time) {
  float Eps = 0.01f; // step size for finite diff
  FVector P0 = P * Freq;
  FVector NX1 = SimpleNoise3D(P0 + FVector(Eps, 0, 0), Time);
  FVector NX0 = SimpleNoise3D(P0 - FVector(Eps, 0, 0), Time);
  FVector NY1 = SimpleNoise3D(P0 + FVector(0, Eps, 0), Time);
  FVector NY0 = SimpleNoise3D(P0 - FVector(0, Eps, 0), Time);
  FVector NZ1 = SimpleNoise3D(P0 + FVector(0, 0, Eps), Time);
  FVector NZ0 = SimpleNoise3D(P0 - FVector(0, 0, Eps), Time);

  // curl = nabla cross F (i had to look this up lol)
  float CurlX = (NY1.Z - NY0.Z) - (NZ1.Y - NZ0.Y);
  float CurlY = (NZ1.X - NZ0.X) - (NX1.Z - NX0.Z);
  float CurlZ = (NX1.Y - NX0.Y) - (NY1.X - NY0.X);

  return FVector(CurlX, CurlY, CurlZ) / (2.0f * Eps);
}

void UKClonerModifier_Noise::ApplyBehavior(FTransform &Transform, int32 Index,
                                           int32 Count, float &Time,
                                           TArray<float> &CustomData) {
  FVector Pos = Transform.GetLocation();
  FVector SamplePos = Pos + Offset;
  float AnimTime = Time * Speed;

  FVector NoiseVec = CurlNoise3D(SamplePos, Frequency, AnimTime);

  if (bAffectPosition) {
    FVector Displacement = NoiseVec * Strength * Influence;
    Transform.AddToTranslation(Displacement);
  }

  if (bAffectRotation) {
    FRotator NoiseRot(NoiseVec.X * RotationStrength,
                      NoiseVec.Y * RotationStrength,
                      NoiseVec.Z * RotationStrength);
    NoiseRot *= Influence;
    Transform.SetRotation(Transform.GetRotation() * NoiseRot.Quaternion());
  }
}

// =========== ATTRACT ===========

UKClonerModifier_Attract::UKClonerModifier_Attract() {}

void UKClonerModifier_Attract::ApplyBehavior(FTransform &Transform, int32 Index,
                                             int32 Count, float &Time,
                                             TArray<float> &CustomData) {
  FVector Pos = Transform.GetLocation();
  FVector ToTarget = Target - Pos;
  float Distance = ToTarget.Size();

  if (Distance < KINDA_SMALL_NUMBER || (Radius > 0.0f && Distance > Radius)) {
    return;
  }

  FVector Direction = ToTarget.GetSafeNormal();

  float NormalizedDist =
      (Radius > 0.0f) ? FMath::Clamp(Distance / Radius, 0.0f, 1.0f) : 1.0f;
  float FalloffFactor = FMath::Pow(1.0f - NormalizedDist, Falloff);

  FVector Displacement = Direction * Strength * FalloffFactor * Influence;
  Transform.AddToTranslation(Displacement);
}

// =========== GRAVITY ===========
// basic physics yo

UKClonerModifier_Gravity::UKClonerModifier_Gravity() {}

void UKClonerModifier_Gravity::ApplyBehavior(FTransform &Transform, int32 Index,
                                             int32 Count, float &Time,
                                             TArray<float> &CustomData) {
  // s = 0.5*a*t^2 remember this from high school?
  float T = Time * TimeMultiplier;
  FVector Displacement = 0.5f * Acceleration * T * T * Influence;
  Transform.AddToTranslation(Displacement);
}

// =========== TARGET (Look At) ===========
// makes clones point at something, pretty useful tbh

UKClonerModifier_Target::UKClonerModifier_Target() {}

void UKClonerModifier_Target::ApplyBehavior(FTransform &Transform, int32 Index,
                                            int32 Count, float &Time,
                                            TArray<float> &CustomData) {
  FVector Pos = Transform.GetLocation();
  FVector TargetPos = TargetLocation;

  if (TargetActor) {
    if (TargetComponentTag != NAME_None) {
      TArray<UActorComponent *> Comps = TargetActor->GetComponentsByTag(
          USceneComponent::StaticClass(), TargetComponentTag);
      if (Comps.Num() > 0) {
        TargetPos = Cast<USceneComponent>(Comps[0])->GetComponentLocation();
      } else {
        TargetPos = TargetActor->GetActorLocation();
      }
    } else {
      TargetPos = TargetActor->GetActorLocation();
    }

    // gotta convert to local space or everything goes to hell
    if (AActor *Cloner = GetTypedOuter<AActor>()) {
      TargetPos =
          Cloner->GetActorTransform().InverseTransformPosition(TargetPos);
    }
  }

  FVector ToTarget = TargetPos - Pos;

  if (ToTarget.SizeSquared() < KINDA_SMALL_NUMBER) {
    return;
  }


  FQuat LookAtRot =
      FRotationMatrix::MakeFromXZ(ToTarget.GetSafeNormal(), UpVector).ToQuat();


  float Blend = BlendFactor * Influence;
  FQuat NewRot = FQuat::Slerp(Transform.GetRotation(), LookAtRot, Blend);
  Transform.SetRotation(NewRot);
}

// =========== PUSH ===========
// explosion effect basically, or implosion if inverted

UKClonerModifier_Push::UKClonerModifier_Push() {}

void UKClonerModifier_Push::ApplyBehavior(FTransform &Transform, int32 Index,
                                          int32 Count, float &Time,
                                          TArray<float> &CustomData) {
  FVector Pos = Transform.GetLocation();
  FVector FromOrigin = Pos - Origin;
  float Distance = FromOrigin.Size();

  if (Distance < KINDA_SMALL_NUMBER) {
    return;
  }

  FVector Direction = FromOrigin.GetSafeNormal();
  if (bInvert) {
    Direction = -Direction;
  }

  float FalloffFactor = 1.0f;
  if (Radius > 0.0f) {
    if (Distance > Radius) {
      return;
    }
    FalloffFactor = 1.0f - (Distance / Radius);
  }

  FVector Displacement = Direction * Strength * FalloffFactor * Influence;
  Transform.AddToTranslation(Displacement);
}

// =========== AUDIO ===========
// THE COOLEST ONE IMO - drives clones from audio spectrum
// requires AudioSynesthesia plugin or nothing works

UKClonerModifier_Audio::UKClonerModifier_Audio() {
  bEnabled = true;
  Direction = FVector(0, 0, 1);
  Strength = 1.0f;
}

void UKClonerModifier_Audio::ApplyBehavior(FTransform &Transform, int32 Index,
                                           int32 Count, float &Time,
                                           TArray<float> &CustomData) {
  if (!AudioAnalysis)
    return;

  TArray<float> Spectrum;
  AudioAnalysis->GetNormalizedChannelConstantQAtTime(Time, AudioChannel,
                                                     Spectrum);

  if (Spectrum.Num() == 0)
    return;

  int32 NumBands = Spectrum.Num();
  int32 StartIdx = FMath::Clamp(
      FMath::RoundToInt(FrequencyMin * (NumBands - 1)), 0, NumBands - 1);
  int32 EndIdx = FMath::Clamp(FMath::RoundToInt(FrequencyMax * (NumBands - 1)),
                              0, NumBands - 1);

  if (StartIdx > EndIdx) {
    int32 Temp = StartIdx;
    StartIdx = EndIdx;
    EndIdx = Temp;
  }

  float Sum = 0.0f;
  for (int32 i = StartIdx; i <= EndIdx; i++) {
    Sum += Spectrum[i];
  }
  float BandCount = (float)(EndIdx - StartIdx + 1);
  float Magnitude = Sum / BandCount;

  if (RemapCurve) {
    Magnitude = RemapCurve->GetFloatValue(Magnitude);
  }

  float FinalEffect = Magnitude * Strength * Influence;

  switch (Mode) {
  case EKClonerAudioMode::Scale: {
    FVector ScaleDelta = FVector::OneVector + (Direction * FinalEffect);
    Transform.SetScale3D(Transform.GetScale3D() * ScaleDelta);
    break;
  }
  case EKClonerAudioMode::Position: {
    Transform.AddToTranslation(Direction * FinalEffect);
    break;
  }
  case EKClonerAudioMode::Rotation: {
    FQuat Rot =
        FQuat(Direction.GetSafeNormal(), FMath::DegreesToRadians(FinalEffect));
    Transform.SetRotation(Transform.GetRotation() * Rot);
    break;
  }
  case EKClonerAudioMode::CustomData: {
    if (CustomData.Num() == 0)
      CustomData.AddZeroed(1);
    CustomData[0] += FinalEffect;
    break;
  }
  }
}

// =========== TEXTURE ===========
// samples a texture to drive transforms - good for displacment maps n stuff

UKClonerModifier_Texture::UKClonerModifier_Texture() {
  bEnabled = true;
  Direction = FVector(0, 0, 1);
  Strength = 1.0f;
}

void UKClonerModifier_Texture::ApplyBehavior(FTransform &Transform, int32 Index,
                                             int32 Count, float &Time,
                                             TArray<float> &CustomData) {
  if (!SourceTexture)
    return;
  if (!bCacheValid)
    UpdateTextureCache();
  if (CachedW <= 0 || CachedH <= 0 || CachedPixels.Num() == 0)
    return;
  FVector Pos = Transform.GetLocation();
  float U = 0.0f;
  float V = 0.0f;
  if (ProjectionAxis == EAxis::Z) {
    U = Pos.X * Tiling.X + Offset.X;
    V = Pos.Y * Tiling.Y + Offset.Y;
  } else if (ProjectionAxis == EAxis::X) {
    U = Pos.Y * Tiling.X + Offset.X;
    V = Pos.Z * Tiling.Y + Offset.Y;
  } else {
    U = Pos.X * Tiling.X + Offset.X;
    V = Pos.Z * Tiling.Y + Offset.Y;
  }
  U = U - FMath::Floor(U);
  V = V - FMath::Floor(V);
  float FinalEffect = SampleTexture(U, V) * Strength * Influence;

  switch (Mode) {
  case EKClonerAudioMode::Scale: {
    FVector ScaleDelta = FVector::OneVector + (Direction * FinalEffect);
    Transform.SetScale3D(Transform.GetScale3D() * ScaleDelta);
    break;
  }
  case EKClonerAudioMode::Position: {
    Transform.AddToTranslation(Direction * FinalEffect);
    break;
  }
  case EKClonerAudioMode::Rotation: {
    FQuat Rot =
        FQuat(Direction.GetSafeNormal(), FMath::DegreesToRadians(FinalEffect));
    Transform.SetRotation(Transform.GetRotation() * Rot);
    break;
  }
  case EKClonerAudioMode::CustomData: {
    if (CustomData.Num() == 0)
      CustomData.AddZeroed(1);
    CustomData[0] += FinalEffect;
    break;
  }
  }
}

#if WITH_EDITOR
void UKClonerModifier_Texture::PostEditChangeProperty(
    FPropertyChangedEvent &PropertyChangedEvent) {
  Super::PostEditChangeProperty(PropertyChangedEvent);
  bCacheValid = false;
}
#endif

void UKClonerModifier_Texture::UpdateTextureCache() {
  bCacheValid = false;
  CachedPixels.Reset();
  CachedW = 0;
  CachedH = 0;
  CachedMip = 0;
  if (!SourceTexture)
    return;

  // texture not streamed in yet? wait and try next frame
  // this was causing CRASHES before i added this check smh
  if (!SourceTexture->IsFullyStreamedIn()) {
    SourceTexture->SetForceMipLevelsToBeResident(30.0f);
    return;
  }

#if WITH_EDITOR
  // editor-only path for source data access
  if (SourceTexture->Source.IsValid()) {
    // Check format compatibility
    if (SourceTexture->Source.GetFormat() != TSF_BGRA8 &&
        SourceTexture->Source.GetFormat() != TSF_BGRA8) {
      // LOG IT so artists know what's wrong!
      static bool bEditorLoggedOnce = false;
      if (!bEditorLoggedOnce) {
        UE_LOG(LogTemp, Warning, 
          TEXT("K-Cloner Texture Modifier: Texture '%s' format not supported. ")
          TEXT("Set Compression to 'UserInterface2D (RGBA)' for best results."),
          *SourceTexture->GetName());
        bEditorLoggedOnce = true;
      }
    } else {
      int32 NumMips = SourceTexture->Source.GetNumMips();
      if (NumMips > 0) {
        int32 Mip = FMath::Clamp(MipLevel, 0, NumMips - 1);

        TArray64<uint8> MipData;
        if (SourceTexture->Source.GetMipData(MipData, Mip)) {
          int32 W = FMath::Max(1, SourceTexture->Source.GetSizeX() >> Mip);
          int32 H = FMath::Max(1, SourceTexture->Source.GetSizeY() >> Mip);
          int32 Count = W * H;

          if (MipData.Num() >= Count * 4) {
            CachedPixels.SetNum(Count);
            const uint8 *Src = MipData.GetData();
            for (int32 i = 0; i < Count; i++) {
              FColor C;
              const uint8 *P = Src + (i * 4);
              C.B = P[0];
              C.G = P[1];
              C.R = P[2];
              C.A = P[3];
              CachedPixels[i] = C;
            }
            CachedW = W;
            CachedH = H;
            CachedMip = Mip;
            bCacheValid = true;
            return;
          }
        }
      }
    }
  }
#endif

  // Fallback: Use platform data (runtime path)
  FTexturePlatformData *PD = SourceTexture->GetPlatformData();
  if (!PD || PD->Mips.Num() == 0)
    return;

  int32 Mip = FMath::Clamp(MipLevel, 0, PD->Mips.Num() - 1);
  FTexture2DMipMap &M = PD->Mips[Mip];

  if (SourceTexture->GetPixelFormat() != PF_B8G8R8A8) {
    // LOG WARNING so artists know to change their texture settings!
    // This was silently failing before - super confusing for users
    static bool bLoggedOnce = false;
    if (!bLoggedOnce) {
      UE_LOG(LogTemp, Warning, 
        TEXT("K-Cloner Texture Modifier: Texture '%s' has unsupported format (needs BGRA8). ")
        TEXT("Change Compression Settings to 'UserInterface2D (RGBA)' or 'VectorDisplacementmap (RGBA8)'."),
        *SourceTexture->GetName());
      bLoggedOnce = true;
    }
    return;
  }
  if (M.BulkData.GetBulkDataSize() <= 0)
    return;

  const void *Data = M.BulkData.LockReadOnly();
  if (!Data) {
    M.BulkData.Unlock();
    return;
  }

  int32 W = M.SizeX;
  int32 H = M.SizeY;
  int32 Count = W * H;
  CachedPixels.SetNum(Count);
  FMemory::Memcpy(CachedPixels.GetData(), Data,
                  static_cast<size_t>(Count) * sizeof(FColor));
  M.BulkData.Unlock();
  CachedW = W;
  CachedH = H;
  CachedMip = Mip;
  bCacheValid = true;
}

float UKClonerModifier_Texture::SampleTexture(float U, float V) {
  if (CachedW <= 0 || CachedH <= 0 || CachedPixels.Num() == 0)
    return 0.0f;
  float X = U * (float)CachedW - 0.5f;
  float Y = V * (float)CachedH - 0.5f;
  int32 X0 = FMath::FloorToInt(X);
  int32 Y0 = FMath::FloorToInt(Y);
  int32 X1 = (X0 + 1);
  int32 Y1 = (Y0 + 1);
  X0 = (X0 % CachedW + CachedW) % CachedW;
  Y0 = (Y0 % CachedH + CachedH) % CachedH;
  X1 = (X1 % CachedW + CachedW) % CachedW;
  Y1 = (Y1 % CachedH + CachedH) % CachedH;
  auto SampleAt = [&](int32 sx, int32 sy) {
    int32 idx = sy * CachedW + sx;
    const FColor &c = CachedPixels[idx];
    float r = (float)c.R / 255.0f;
    float g = (float)c.G / 255.0f;
    float b = (float)c.B / 255.0f;
    return 0.2126f * r + 0.7152f * g + 0.0722f * b;
  };
  if (!bBilinearFiltering) {
    return SampleAt(X0, Y0);
  }
  float tx = X - (float)FMath::FloorToInt(X);
  float ty = Y - (float)FMath::FloorToInt(Y);
  float c00 = SampleAt(X0, Y0);
  float c10 = SampleAt(X1, Y0);
  float c01 = SampleAt(X0, Y1);
  float c11 = SampleAt(X1, Y1);
  float cx0 = FMath::Lerp(c00, c10, tx);
  float cx1 = FMath::Lerp(c01, c11, tx);
  return FMath::Lerp(cx0, cx1, ty);
}

// =========== INHERITANCE ===========
// morphs between two cloner layouts - grid to radial, logo reveals, etc
// THIS IS SICK for motion graphics

#include "KClonerActor.h"

UKClonerModifier_Inheritance::UKClonerModifier_Inheritance() {
  bEnabled = true;
  BlendFactor = 0.0f;
}

void UKClonerModifier_Inheritance::UpdateSourceCache() {
  uint32 CurrentFrame = GFrameNumber;
  if (CurrentFrame == LastCacheFrame)
    return;
  LastCacheFrame = CurrentFrame;

  CachedSourceTransforms.Empty();

  AKClonerActor *Source = SourceCloner.Get();
  if (!Source)
    return;

  // grab transforms from the other cloner
  if (Source->InstancedMesh) {
    int32 InstanceCount = Source->InstancedMesh->GetInstanceCount();
    CachedSourceTransforms.SetNum(InstanceCount);

    for (int32 i = 0; i < InstanceCount; i++) {
      Source->InstancedMesh->GetInstanceTransform(i, CachedSourceTransforms[i]);
    }
  }
}

void UKClonerModifier_Inheritance::ApplyBehavior(FTransform &Transform,
                                                 int32 Index, int32 Count,
                                                 float &Time,
                                                 TArray<float> &CustomData) {
  if (!SourceCloner.Get())
    return;
  if (BlendFactor <= 0.0f)
    return;


  UpdateSourceCache();

  if (CachedSourceTransforms.Num() == 0)
    return;


  FTransform TargetTransform = FTransform::Identity;

  if (bMatchByIndex) {
    // wrap around if counts dont match
    int32 TargetIndex = Index % CachedSourceTransforms.Num();
    TargetTransform = CachedSourceTransforms[TargetIndex];
  } else {
    // brute force nearest neighbor... could optimize w/ kdtree but whatever
    FVector CurrentPos = Transform.GetLocation();
    float MinDistSq = FLT_MAX;

    for (int32 i = 0; i < CachedSourceTransforms.Num(); i++) {
      float DistSq = FVector::DistSquared(
          CurrentPos, CachedSourceTransforms[i].GetLocation());
      if (DistSq < MinDistSq) {
        MinDistSq = DistSq;
        TargetTransform = CachedSourceTransforms[i];
      }
    }
  }


  float EasedBlend = UKismetMathLibrary::Ease(
      0.0f, 1.0f, BlendFactor * Influence, EasingFunction);


  if (bBlendPosition) {
    FVector BlendedPos = FMath::Lerp(Transform.GetLocation(),
                                     TargetTransform.GetLocation(), EasedBlend);
    Transform.SetLocation(BlendedPos);
  }

  if (bBlendRotation) {
    FQuat BlendedRot = FQuat::Slerp(Transform.GetRotation(),
                                    TargetTransform.GetRotation(), EasedBlend);
    Transform.SetRotation(BlendedRot);
  }

  if (bBlendScale) {
    FVector BlendedScale = FMath::Lerp(
        Transform.GetScale3D(), TargetTransform.GetScale3D(), EasedBlend);
    Transform.SetScale3D(BlendedScale);
  }
}

// ============================================================
// MOTION MODIFIERS - the pretty movement stuff
// ============================================================

// =========== FIGURE 8 ===========
// infinity symbol path, looks DOPE on screen

UKClonerModifier_Figure8::UKClonerModifier_Figure8() { bEnabled = true; }

void UKClonerModifier_Figure8::ApplyBehavior(FTransform &Transform, int32 Index,
                                             int32 Count, float &Time,
                                             TArray<float> &CustomData) {
  // lemniscate of bernouli - had to google the formula ngl

  float T = (Time * Speed + Index * Step) * UE_PI * 2.0f;

  float SinT = FMath::Sin(T);
  float CosT = FMath::Cos(T);
  float Denom = 1.0f + SinT * SinT;

  float X = CosT / Denom;
  float Y = CosT * SinT / Denom;

  FVector Offset;
  switch (UpAxis) {
  case EAxis::Z:
    Offset = FVector(X * Width, 0.0f, Y * Height * 2.0f);
    break;
  case EAxis::Y:
    Offset = FVector(X * Width, Y * Height * 2.0f, 0.0f);
    break;
  case EAxis::X:
  default:
    Offset = FVector(0.0f, X * Width, Y * Height * 2.0f);
    break;
  }

  Transform.AddToTranslation(Offset * Influence);
}

// =========== LISSAJOUS ===========
// fancy harmonic curves for pro mograph vibes
// diff frequency ratios = diff patterns (try 2:3, 3:4, etc)

UKClonerModifier_Lissajous::UKClonerModifier_Lissajous() { bEnabled = true; }

void UKClonerModifier_Lissajous::ApplyBehavior(FTransform &Transform,
                                               int32 Index, int32 Count,
                                               float &Time,
                                               TArray<float> &CustomData) {
  // Lissajous curve: x = A*sin(a*t + phase), y = B*sin(b*t)
  // Different a:b ratios create different patterns (1:2, 2:3, 3:4, etc.)

  float T = Time * Speed + Index * Step;

  float X = FMath::Sin(FrequencyA * T + Phase);
  float Y = FMath::Sin(FrequencyB * T);
  float Z = FMath::Sin((FrequencyA + FrequencyB) * 0.5f * T + Phase * 0.5f);

  FVector Offset(X * Size, Y * Size, Z * Size * 0.5f);

  Transform.AddToTranslation(Offset * Influence);
}

// =========== BOUNCE ===========
// actual bouncing ball physics with squash n stretch

UKClonerModifier_Bounce::UKClonerModifier_Bounce() {
  bEnabled = true;
  Direction = FVector(0, 0, 1);
}

void UKClonerModifier_Bounce::ApplyBehavior(FTransform &Transform, int32 Index,
                                            int32 Count, float &Time,
                                            TArray<float> &CustomData) {
  float T = Time * Speed + Index * Step;

  float BouncePhase = FMath::Fmod(T, UE_PI);
  float BounceValue = FMath::Sin(BouncePhase);

  float Velocity = FMath::Cos(BouncePhase);


  FVector PositionOffset = Direction.GetSafeNormal() * BounceValue * Height;
  Transform.AddToTranslation(PositionOffset * Influence);

  // squash effect - ground=flat, peak=normal
  if (Squash > 0.0f) {


    float SquashAmount = FMath::Abs(Velocity) * Squash * Influence;


    float HeightScale = 1.0f - SquashAmount * (1.0f - BounceValue);
    float WidthScale = 1.0f + SquashAmount * (1.0f - BounceValue) * 0.5f;


    FVector Scale = Transform.GetScale3D();
    FVector DirNorm = Direction.GetSafeNormal().GetAbs();

    FVector ScaleMultiplier = FVector::OneVector;
    ScaleMultiplier += DirNorm * (HeightScale - 1.0f);
    ScaleMultiplier += (FVector::OneVector - DirNorm) * (WidthScale - 1.0f);

    Transform.SetScale3D(Scale * ScaleMultiplier);
  }
}

// =========== PENDULUM ===========
// clock pendulum swing, super satisfying

UKClonerModifier_Pendulum::UKClonerModifier_Pendulum() { bEnabled = true; }

void UKClonerModifier_Pendulum::ApplyBehavior(FTransform &Transform,
                                              int32 Index, int32 Count,
                                              float &Time,
                                              TArray<float> &CustomData) {
  float T = Time * Speed * UE_PI * 2.0f + Index * Step;
  float SwingAngle = FMath::Sin(T) * Angle * Influence;

  FQuat SwingRotation;
  switch (RotationAxis) {
  case EAxis::X:
    SwingRotation =
        FQuat(FVector::RightVector, FMath::DegreesToRadians(SwingAngle));
    break;
  case EAxis::Y:
    SwingRotation =
        FQuat(FVector::ForwardVector, FMath::DegreesToRadians(SwingAngle));
    break;
  case EAxis::Z:
  default:
    SwingRotation =
        FQuat(FVector::UpVector, FMath::DegreesToRadians(SwingAngle));
    break;
  }

  Transform.SetRotation(Transform.GetRotation() * SwingRotation);
}

// =========== SWAY ===========
// tree/plant wind movement - stacked sine waves for organic feel

UKClonerModifier_Sway::UKClonerModifier_Sway() { bEnabled = true; }

void UKClonerModifier_Sway::ApplyBehavior(FTransform &Transform, int32 Index,
                                          int32 Count, float &Time,
                                          TArray<float> &CustomData) {
  // bunch of layered sines - primary + detail + random offset
  FRandomStream RNG(Index);
  float RandomPhase = RNG.FRand() * UE_PI * 2.0f * Randomization;
  float RandomFreqMult = 1.0f + (RNG.FRand() - 0.5f) * 0.3f * Randomization;

  float T = Time * WindSpeed;

  // big slow primary wave
  float Primary = FMath::Sin((T + RandomPhase) * RandomFreqMult);

  // faster detail waves make it feel alive
  float Secondary =
      FMath::Sin((T * 2.7f + RandomPhase + 1.1f) * RandomFreqMult) *
      DetailIntensity;
  float Tertiary =
      FMath::Sin((T * 4.3f + RandomPhase + 2.3f) * RandomFreqMult) *
      DetailIntensity * 0.5f;

  float TotalSway = (Primary + Secondary + Tertiary) * SwayAngle * Influence;


  FQuat SwayRotation;
  switch (SwayAxis) {
  case EAxis::X:
    SwayRotation =
        FQuat(FVector::RightVector, FMath::DegreesToRadians(TotalSway));
    break;
  case EAxis::Y:
    SwayRotation =
        FQuat(FVector::ForwardVector, FMath::DegreesToRadians(TotalSway));
    break;
  case EAxis::Z:
  default:
    SwayRotation = FQuat(FVector::UpVector, FMath::DegreesToRadians(TotalSway));
    break;
  }

  Transform.SetRotation(Transform.GetRotation() * SwayRotation);

  // cross-axis for 3d depth
  if (bCrossAxis) {
    float CrossSway =
        FMath::Sin((T * 1.3f + RandomPhase + UE_PI * 0.5f) * RandomFreqMult) *
        SwayAngle * 0.3f * Influence;

    FQuat CrossRotation;
    switch (SwayAxis) {
    case EAxis::X:
      CrossRotation =
          FQuat(FVector::ForwardVector, FMath::DegreesToRadians(CrossSway));
      break;
    case EAxis::Y:
      CrossRotation =
          FQuat(FVector::RightVector, FMath::DegreesToRadians(CrossSway));
      break;
    case EAxis::Z:
    default:
      CrossRotation =
          FQuat(FVector::RightVector, FMath::DegreesToRadians(CrossSway));
      break;
    }

    Transform.SetRotation(Transform.GetRotation() * CrossRotation);
  }
}

// ============================================================
// PRESET MODIFIER - uses compiled expressions from .json presets
// ============================================================

#include "KClonerExpressionEvaluator.h"
#include "KClonerModifierPreset.h"

UKClonerModifier_Preset::UKClonerModifier_Preset() {
  bEnabled = true;
  CompiledPosition = MakeUnique<FKClonerCompiledExpression>();
  CompiledRotation = MakeUnique<FKClonerCompiledExpression>();
  CompiledScale = MakeUnique<FKClonerCompiledExpression>();
}

UKClonerModifier_Preset::~UKClonerModifier_Preset() {
  // TUniquePtr cleans itself up lol
}

void UKClonerModifier_Preset::CompileExpressions() {
  if (!Preset) {
    CompiledPosition = MakeUnique<FKClonerCompiledExpression>();
    CompiledRotation = MakeUnique<FKClonerCompiledExpression>();
    CompiledScale = MakeUnique<FKClonerCompiledExpression>();
    CachedPreset.Reset();
    return;
  }

  // skip if nothing changed
  if (CachedPreset.Get() == Preset) {
    return;
  }

  CachedPreset = Preset;
  int32 NumVars = Preset->Variables.Num();


  FKClonerCompiledExpression NewPosition;
  if (!FKClonerExpressionEvaluator::Compile(Preset->PositionExpression, NumVars,
                                            NewPosition)) {
    UE_LOG(
        LogTemp, Warning,
        TEXT("K-Cloner Preset '%s': Failed to compile position expression: %s"),
        *Preset->DisplayName, *NewPosition.GetError());
  }
  CompiledPosition =
      MakeUnique<FKClonerCompiledExpression>(MoveTemp(NewPosition));


  FKClonerCompiledExpression NewRotation;
  if (!FKClonerExpressionEvaluator::Compile(Preset->RotationExpression, NumVars,
                                            NewRotation)) {
    UE_LOG(
        LogTemp, Warning,
        TEXT("K-Cloner Preset '%s': Failed to compile rotation expression: %s"),
        *Preset->DisplayName, *NewRotation.GetError());
  }
  CompiledRotation =
      MakeUnique<FKClonerCompiledExpression>(MoveTemp(NewRotation));


  FKClonerCompiledExpression NewScale;
  if (!FKClonerExpressionEvaluator::Compile(Preset->ScaleExpression, NumVars,
                                            NewScale)) {
    UE_LOG(LogTemp, Warning,
           TEXT("K-Cloner Preset '%s': Failed to compile scale expression: %s"),
           *Preset->DisplayName, *NewScale.GetError());
  }
  CompiledScale = MakeUnique<FKClonerCompiledExpression>(MoveTemp(NewScale));


  SyncVariableOverrides();

  UE_LOG(LogTemp, Log, TEXT("K-Cloner: Compiled preset '%s' with %d variables"),
         *Preset->DisplayName, NumVars);
}

void UKClonerModifier_Preset::SyncVariableOverrides() {
  if (!Preset) {
    VariableOverrides.Empty();
    return;
  }


  TArray<FKClonerVariableOverride> NewOverrides;
  NewOverrides.SetNum(Preset->Variables.Num());

  for (int32 i = 0; i < Preset->Variables.Num(); ++i) {
    const FKClonerPresetVariable &PresetVar = Preset->Variables[i];


    FKClonerVariableOverride *Existing = nullptr;
    for (FKClonerVariableOverride &Override : VariableOverrides) {
      if (Override.Index == i) {
        Existing = &Override;
        break;
      }
    }

    NewOverrides[i].Index = i;
    NewOverrides[i].Name = PresetVar.Name;

    if (Existing && Existing->bOverride) {
      NewOverrides[i].Value = Existing->Value;
      NewOverrides[i].bOverride = true;
    } else {
      // Use preset default
      NewOverrides[i].Value = PresetVar.DefaultValue;
      NewOverrides[i].bOverride = false;
    }
  }

  VariableOverrides = MoveTemp(NewOverrides);
}

TArray<float> UKClonerModifier_Preset::GetResolvedVariables() const {
  TArray<float> Result;

  if (!Preset) {
    return Result;
  }

  Result.SetNum(Preset->Variables.Num());

  for (int32 i = 0; i < Preset->Variables.Num(); ++i) {
    if (i < VariableOverrides.Num() && VariableOverrides[i].bOverride) {
      Result[i] = VariableOverrides[i].Value;
    } else {
      Result[i] = Preset->Variables[i].DefaultValue;
    }
  }

  return Result;
}

void UKClonerModifier_Preset::ApplyBehavior(FTransform &Transform, int32 Index,
                                            int32 Count, float &Time,
                                            TArray<float> &CustomData) {
  if (!Preset) {
    return;
  }

  // Compile if needed
  if (CachedPreset.Get() != Preset) {
    CompileExpressions();
  }

  // Calculate effective time with step offsets
  float EffectiveTime = Time * Speed * Preset->SpeedMultiplier;
  float TotalStep = Step + Preset->Step;
  EffectiveTime += Index * TotalStep;

  // Get resolved variable values
  TArray<float> Variables = GetResolvedVariables();

  // Get current transform values
  FVector Position = Transform.GetLocation();
  FRotator Rotator = Transform.Rotator();
  FVector Rotation(Rotator.Pitch, Rotator.Yaw, Rotator.Roll);
  FVector Scale = Transform.GetScale3D();

  // Evaluate position expression
  if (CompiledPosition && CompiledPosition->IsValid()) {
    FVector TempRot = Rotation;
    FVector TempScale = Scale;
    FKClonerExpressionEvaluator::Evaluate(*CompiledPosition, EffectiveTime,
                                          Index, Count, Position, TempRot,
                                          TempScale, Variables);
  }

  // Evaluate rotation expression
  if (CompiledRotation && CompiledRotation->IsValid()) {
    FVector TempPos = Position;
    FVector TempScale = Scale;
    FKClonerExpressionEvaluator::Evaluate(*CompiledRotation, EffectiveTime,
                                          Index, Count, TempPos, Rotation,
                                          TempScale, Variables);
  }

  // Evaluate scale expression
  if (CompiledScale && CompiledScale->IsValid()) {
    FVector TempPos = Position;
    FVector TempRot = Rotation;
    FKClonerExpressionEvaluator::Evaluate(*CompiledScale, EffectiveTime, Index,
                                          Count, TempPos, TempRot, Scale,
                                          Variables);
  }

  // Apply influence
  FVector OriginalPos = Transform.GetLocation();
  FVector OriginalScale = Transform.GetScale3D();
  FRotator OriginalRot = Transform.Rotator();
  FVector OriginalRotVec(OriginalRot.Pitch, OriginalRot.Yaw, OriginalRot.Roll);

  Position = FMath::Lerp(OriginalPos, Position, Influence);
  Rotation = FMath::Lerp(OriginalRotVec, Rotation, Influence);
  Scale = FMath::Lerp(OriginalScale, Scale, Influence);

  // Apply to transform
  Transform.SetLocation(Position);
  Transform.SetRotation(
      FRotator(Rotation.X, Rotation.Y, Rotation.Z).Quaternion());
  Transform.SetScale3D(Scale);
}

#if WITH_EDITOR
void UKClonerModifier_Preset::PostEditChangeProperty(
    FPropertyChangedEvent &PropertyChangedEvent) {
  Super::PostEditChangeProperty(PropertyChangedEvent);

  FName PropertyName = PropertyChangedEvent.GetPropertyName();

  if (PropertyName ==
      GET_MEMBER_NAME_CHECKED(UKClonerModifier_Preset, Preset)) {
    // Preset changed - recompile
    CachedPreset.Reset(); // Force recompile
    CompileExpressions();
  }
}
#endif
