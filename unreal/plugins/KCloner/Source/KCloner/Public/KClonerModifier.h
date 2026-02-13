// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "KClonerEasing.h"
#include "KClonerExpressionEvaluator.h"
#include "Kismet/KismetMathLibrary.h"
#include "UObject/NoExportTypes.h"
#include "KClonerModifier.generated.h"


class UConstantQNRT;
class UCurveFloat;

UENUM(BlueprintType)
enum class EKClonerAudioMode : uint8 { Scale, Position, Rotation, CustomData };

/*
 * Base class for K-Cloner modifiers - subclass this to add new effects
 */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, BlueprintType,
       CollapseCategories)
class KCLONER_API UKClonerModifier : public UObject {
  GENERATED_BODY()

public:
  UKClonerModifier();

  // unique ID so Sequencer can track this modifier
  UPROPERTY(EditAnywhere, Category = "Modifier")
  FGuid ModifierGuid;


  UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Modifier")
  bool bEnabled = true;

  // how much this modifier affects things (0=off, 1=full)
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Modifier",
            meta = (ClampMin = "0.0", ClampMax = "1.0"))
  float Influence = 1.0f;

  // optional easing curve
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier")
  EKClonerEasing Easing = EKClonerEasing::Linear;

  // main func - applies the modifier to a clone transform
  // Time is a REF cuz delay modifier can shift it for later modifiers
  void ApplyModifier(FTransform &Transform, int32 Index, int32 Count,
                     float &Time, TArray<float> &CustomData);

protected:
  // subclasses override this - do your actual logic here
  virtual void ApplyBehavior(FTransform &Transform, int32 Index, int32 Count,
                             float &Time, TArray<float> &CustomData)
      PURE_VIRTUAL(UKClonerModifier::ApplyBehavior, );
};

// =========== THE MODIFIERS ===========

// Orbit - spins around an axis
UCLASS(DisplayName = "Orbit")
class KCLONER_API UKClonerModifier_Orbit : public UKClonerModifier {
  GENERATED_BODY()

public:
  UKClonerModifier_Orbit();

  UPROPERTY(EditAnywhere, Interp, Category = "Settings")
  float Speed = 1.0f;

  UPROPERTY(EditAnywhere, Interp, Category = "Settings")
  FVector Axis = FVector(0, 0, 1);

  UPROPERTY(EditAnywhere, Interp, Category = "Settings")
  float Step = 0.1f;

protected:
  virtual void ApplyBehavior(FTransform &Transform, int32 Index, int32 Count,
                             float &Time, TArray<float> &CustomData) override;
};

/**
 * Float Modifier: Moves instances in a sine wave pattern.
 */
UCLASS(DisplayName = "Float")
class KCLONER_API UKClonerModifier_Float : public UKClonerModifier {
  GENERATED_BODY()

public:
  UKClonerModifier_Float();

  UPROPERTY(EditAnywhere, Interp, Category = "Settings")
  float Speed = 1.0f;

  UPROPERTY(EditAnywhere, Interp, Category = "Settings")
  float Frequency = 1.0f;

  UPROPERTY(EditAnywhere, Interp, Category = "Settings")
  float Amplitude = 50.0f;

  UPROPERTY(EditAnywhere, Interp, Category = "Settings")
  FVector Direction = FVector(0, 0, 1);

protected:
  virtual void ApplyBehavior(FTransform &Transform, int32 Index, int32 Count,
                             float &Time, TArray<float> &CustomData) override;
};

// Pulse - breathing scale effect
UCLASS(DisplayName = "Pulse")
class KCLONER_API UKClonerModifier_Pulse : public UKClonerModifier {
  GENERATED_BODY()

public:
  UKClonerModifier_Pulse();

  UPROPERTY(EditAnywhere, Interp, Category = "Settings")
  float Speed = 3.0f;

  UPROPERTY(EditAnywhere, Interp, Category = "Settings")
  float Frequency = 0.5f;

  UPROPERTY(EditAnywhere, Interp, Category = "Settings")
  float ScaleAmount = 0.5f;

  UPROPERTY(EditAnywhere, Category = "Settings")
  bool bUniform = true;

protected:
  virtual void ApplyBehavior(FTransform &Transform, int32 Index, int32 Count,
                             float &Time, TArray<float> &CustomData) override;
};



// Delay - shifts time for following modifiers (PUT THIS BEFORE OTHER MODIFIERS)
UCLASS(DisplayName = "Delay")
class KCLONER_API UKClonerModifier_Delay : public UKClonerModifier {
  GENERATED_BODY()

public:
  UKClonerModifier_Delay();

  UPROPERTY(EditAnywhere, Interp, Category = "Settings")
  float DelayAmount = 0.1f;

protected:
  virtual void ApplyBehavior(FTransform &Transform, int32 Index, int32 Count,
                             float &Time, TArray<float> &CustomData) override;
};

// Random - adds noise/variation to transforms
UCLASS(DisplayName = "Random")
class KCLONER_API UKClonerModifier_Random : public UKClonerModifier {
  GENERATED_BODY()

public:
  UKClonerModifier_Random();

  UPROPERTY(EditAnywhere, Category = "Settings")
  int32 Seed = 12345;

  UPROPERTY(EditAnywhere, Category = "Settings")
  FVector Position = FVector::ZeroVector;

  UPROPERTY(EditAnywhere, Category = "Settings")
  FRotator Rotation = FRotator::ZeroRotator;

  UPROPERTY(EditAnywhere, Category = "Settings")
  FVector Scale = FVector::ZeroVector;

  UPROPERTY(EditAnywhere, Category = "Settings")
  bool bUniformScale = true;

protected:
  virtual void ApplyBehavior(FTransform &Transform, int32 Index, int32 Count,
                             float &Time, TArray<float> &CustomData) override;
};

// Shake - perlin noise jitter, better than random for organic feel
UCLASS(DisplayName = "Shake")
class KCLONER_API UKClonerModifier_Shake : public UKClonerModifier {
  GENERATED_BODY()

public:
  UKClonerModifier_Shake();

  UPROPERTY(EditAnywhere, Interp, Category = "Settings")
  float Speed = 1.0f;

  UPROPERTY(EditAnywhere, Interp, Category = "Settings")
  FVector PositionStrength = FVector(10.0f);

  UPROPERTY(EditAnywhere, Interp, Category = "Settings")
  FVector RotationStrength = FVector(10.0f);

  UPROPERTY(EditAnywhere, Interp, Category = "Settings")
  FVector ScaleStrength = FVector(0.0f);

protected:
  virtual void ApplyBehavior(FTransform &Transform, int32 Index, int32 Count,
                             float &Time, TArray<float> &CustomData) override;
};

// Wave - propagating sine wave thru the instances
UCLASS(DisplayName = "Wave")
class KCLONER_API UKClonerModifier_Wave : public UKClonerModifier {
  GENERATED_BODY()

public:
  UKClonerModifier_Wave();

  UPROPERTY(EditAnywhere, Interp, Category = "Settings")
  float Speed = 1.0f;

  UPROPERTY(EditAnywhere, Interp, Category = "Settings")
  float Frequency = 0.1f;

  UPROPERTY(EditAnywhere, Interp, Category = "Settings")
  float Amplitude = 50.0f;

  UPROPERTY(EditAnywhere, Interp, Category = "Settings")
  FVector Direction = FVector(0, 0, 1);

protected:
  virtual void ApplyBehavior(FTransform &Transform, int32 Index, int32 Count,
                             float &Time, TArray<float> &CustomData) override;
};

// Step - accumulates per clone (first +0, second +1x, third +2x, etc)
UCLASS(DisplayName = "Step")
class KCLONER_API UKClonerModifier_Step : public UKClonerModifier {
  GENERATED_BODY()

public:
  UKClonerModifier_Step();

  UPROPERTY(EditAnywhere, Interp, Category = "Settings")
  FVector PositionStep = FVector::ZeroVector;

  UPROPERTY(EditAnywhere, Interp, Category = "Settings")
  FRotator RotationStep = FRotator::ZeroRotator;

  UPROPERTY(EditAnywhere, Interp, Category = "Settings")
  FVector ScaleStep = FVector::ZeroVector;

protected:
  virtual void ApplyBehavior(FTransform &Transform, int32 Index, int32 Count,
                             float &Time, TArray<float> &CustomData) override;
};

// Elastic - bouncy springy motion
UCLASS(DisplayName = "Elastic")
class KCLONER_API UKClonerModifier_Elastic : public UKClonerModifier {
  GENERATED_BODY()

public:
  UKClonerModifier_Elastic();

  UPROPERTY(EditAnywhere, Category = "Settings")
  float Speed = 4.0f;

  UPROPERTY(EditAnywhere, Category = "Settings")
  float Amplitude = 1.0f;

  UPROPERTY(EditAnywhere, Category = "Settings")
  float Damping = 0.5f;

  UPROPERTY(EditAnywhere, Category = "Settings")
  FVector Axis = FVector(0, 0, 1);

protected:
  virtual void ApplyBehavior(FTransform &Transform, int32 Index, int32 Count,
                             float &Time, TArray<float> &CustomData) override;
};

// Tumble - constant spinning with random variation
UCLASS(DisplayName = "Tumble")
class KCLONER_API UKClonerModifier_Tumble : public UKClonerModifier {
  GENERATED_BODY()

public:
  UKClonerModifier_Tumble();

  UPROPERTY(EditAnywhere, Category = "Settings")
  FVector RotationSpeed = FVector(45.0f, 30.0f, 60.0f);

  UPROPERTY(EditAnywhere, Category = "Settings")
  float RandomOffset = 1.0f;

protected:
  virtual void ApplyBehavior(FTransform &Transform, int32 Index, int32 Count,
                             float &Time, TArray<float> &CustomData) override;
};

// Vortex - swirly black hole type thing
UCLASS(DisplayName = "Vortex")
class KCLONER_API UKClonerModifier_Vortex : public UKClonerModifier {
  GENERATED_BODY()

public:
  UKClonerModifier_Vortex();

  UPROPERTY(EditAnywhere, Interp, Category = "Vortex")
  float RotationSpeed = 2.0f;

  UPROPERTY(EditAnywhere, Interp, Category = "Vortex")
  float PullStrength = 0.5f;

  UPROPERTY(EditAnywhere, Interp, Category = "Vortex")
  float Radius = 500.0f;

  UPROPERTY(EditAnywhere, Interp, Category = "Vortex")
  FVector Axis = FVector(0, 0, 1);

protected:
  virtual void ApplyBehavior(FTransform &Transform, int32 Index, int32 Count,
                             float &Time, TArray<float> &CustomData) override;
};

// K-Script - MY BABY - custom expressions like my DCC tool had
// not a full interpreter but handles the patterns i use most
UCLASS(DisplayName = "K-Script")
class KCLONER_API UKClonerModifier_KScript : public UKClonerModifier {
  GENERATED_BODY()

public:
  UKClonerModifier_KScript();

  // write expressions like: p.y += sin(t + i * 0.1) * 10.0;
  // vars: p=position, r=rotation, s=scale, t=time, i=index, v[0..n]=sliders
  UPROPERTY(EditAnywhere, Category = "Script", meta = (MultiLine = true))
  FString Code = TEXT("p.y += sin(t + i * 0.1) * 10.0;");

  // hook up sliders to use as v[0], v[1], etc in your code
  UPROPERTY(EditAnywhere, Category = "Script")
  TArray<float> Variables;

protected:
  virtual void ApplyBehavior(FTransform &Transform, int32 Index, int32 Count,
                             float &Time, TArray<float> &CustomData) override;

#if WITH_EDITOR
  virtual void
  PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent) override;
#endif

private:
  bool bOpSinPY = false;
  bool bOpSinPZ = false;
  bool bOpPulseScale = false;
  float CachedAddPY = 0.0f;
  float CachedAddPZ = 0.0f;
  void BuildCache();
};

// Color - sets per-instance color (material needs to read CustomData or this does NOTHING)
UCLASS(DisplayName = "Color")
class KCLONER_API UKClonerModifier_Color : public UKClonerModifier {
  GENERATED_BODY()

public:
  UKClonerModifier_Color();

  UPROPERTY(EditAnywhere, Category = "Settings")
  FLinearColor Color = FLinearColor::White;

  UPROPERTY(EditAnywhere, Category = "Settings")
  bool bRandomize = false;

protected:
  virtual void ApplyBehavior(FTransform &Transform, int32 Index, int32 Count,
                             float &Time, TArray<float> &CustomData) override;
};

// ============================================================
// FORCE MODIFIERS - physics-ish effects
// ============================================================

// Noise - curl noise for fluid-like motion (NOT basic perlin)
UCLASS(DisplayName = "Noise")
class KCLONER_API UKClonerModifier_Noise : public UKClonerModifier {
  GENERATED_BODY()

public:
  UKClonerModifier_Noise();


  UPROPERTY(EditAnywhere, Interp, Category = "Noise", meta = (ClampMin = "0.0"))
  float Strength = 50.0f;

  // higher = more wiggly
  UPROPERTY(EditAnywhere, Interp, Category = "Noise",
            meta = (ClampMin = "0.01"))
  float Frequency = 0.01f;


  UPROPERTY(EditAnywhere, Interp, Category = "Noise")
  float Speed = 1.0f;


  UPROPERTY(EditAnywhere, Interp, Category = "Noise")
  FVector Offset = FVector::ZeroVector;

  /** Affect position */
  UPROPERTY(EditAnywhere, Category = "Noise")
  bool bAffectPosition = true;

  /** Affect rotation */
  UPROPERTY(EditAnywhere, Category = "Noise")
  bool bAffectRotation = false;


  UPROPERTY(EditAnywhere, Interp, Category = "Noise",
            meta = (EditCondition = "bAffectRotation"))
  float RotationStrength = 45.0f;

protected:
  virtual void ApplyBehavior(FTransform &Transform, int32 Index, int32 Count,
                             float &Time, TArray<float> &CustomData) override;
};

// Attract - pulls clones towards a point (use negative strength to repel)
UCLASS(DisplayName = "Attract")
class KCLONER_API UKClonerModifier_Attract : public UKClonerModifier {
  GENERATED_BODY()

public:
  UKClonerModifier_Attract();


  UPROPERTY(EditAnywhere, Interp, Category = "Attract")
  FVector Target = FVector::ZeroVector;

  // negative = repel
  UPROPERTY(EditAnywhere, Interp, Category = "Attract")
  float Strength = 100.0f;


  UPROPERTY(EditAnywhere, Interp, Category = "Attract",
            meta = (ClampMin = "0.0"))
  float Radius = 500.0f;

  // falloff curve: 1=linear, 2=quadratic, etc
  UPROPERTY(EditAnywhere, Interp, Category = "Attract",
            meta = (ClampMin = "0.1", ClampMax = "5.0"))
  float Falloff = 2.0f;

protected:
  virtual void ApplyBehavior(FTransform &Transform, int32 Index, int32 Count,
                             float &Time, TArray<float> &CustomData) override;
};

// Gravity - what it sounds like, s=0.5*a*t^2
UCLASS(DisplayName = "Gravity")
class KCLONER_API UKClonerModifier_Gravity : public UKClonerModifier {
  GENERATED_BODY()

public:
  UKClonerModifier_Gravity();


  UPROPERTY(EditAnywhere, Interp, Category = "Gravity")
  FVector Acceleration = FVector(0.0f, 0.0f, -980.0f);


  UPROPERTY(EditAnywhere, Interp, Category = "Gravity",
            meta = (ClampMin = "0.0"))
  float TimeMultiplier = 1.0f;

protected:
  virtual void ApplyBehavior(FTransform &Transform, int32 Index, int32 Count,
                             float &Time, TArray<float> &CustomData) override;
};

// Target - makes clones look at a point or actor
UCLASS(DisplayName = "Target")
class KCLONER_API UKClonerModifier_Target : public UKClonerModifier {
  GENERATED_BODY()

public:
  UKClonerModifier_Target();

  // fallback if no actor set
  UPROPERTY(EditAnywhere, Interp, Category = "Target")
  FVector TargetLocation = FVector(0.0f, 0.0f, 0.0f);

  // drag an actor here instead of using coords
  UPROPERTY(EditAnywhere, Category = "Target")
  AActor *TargetActor = nullptr;


  UPROPERTY(EditAnywhere, Category = "Target")
  FName TargetComponentTag = NAME_None;


  UPROPERTY(EditAnywhere, Category = "Target")
  FVector UpVector = FVector::UpVector;


  UPROPERTY(EditAnywhere, Interp, Category = "Target",
            meta = (ClampMin = "0.0", ClampMax = "1.0"))
  float BlendFactor = 1.0f;

protected:
  virtual void ApplyBehavior(FTransform &Transform, int32 Index, int32 Count,
                             float &Time, TArray<float> &CustomData) override;
};

// Push - explosion outward from a point (invert for implosion)
UCLASS(DisplayName = "Push")
class KCLONER_API UKClonerModifier_Push : public UKClonerModifier {
  GENERATED_BODY()

public:
  UKClonerModifier_Push();


  UPROPERTY(EditAnywhere, Interp, Category = "Push")
  FVector Origin = FVector::ZeroVector;


  UPROPERTY(EditAnywhere, Interp, Category = "Push")
  float Strength = 200.0f;


  UPROPERTY(EditAnywhere, Interp, Category = "Push", meta = (ClampMin = "0.0"))
  float Radius = 500.0f;

  // check this to pull instead of push
  UPROPERTY(EditAnywhere, Category = "Push")
  bool bInvert = false;

protected:
  virtual void ApplyBehavior(FTransform &Transform, int32 Index, int32 Count,
                             float &Time, TArray<float> &CustomData) override;
};

// Audio - SUPER DOPE - drives clones from audio spectrum
// needs AudioSynesthesia plugin enabled!
UCLASS(DisplayName = "Audio")
class KCLONER_API UKClonerModifier_Audio : public UKClonerModifier {
  GENERATED_BODY()

public:
  UKClonerModifier_Audio();

  UPROPERTY(EditAnywhere, Category = "Audio")
  TObjectPtr<UConstantQNRT> AudioAnalysis;


  UPROPERTY(EditAnywhere, Category = "Audio")
  EKClonerAudioMode Mode = EKClonerAudioMode::Scale;


  UPROPERTY(EditAnywhere, Category = "Audio")
  FVector Direction = FVector(0, 0, 1);


  UPROPERTY(EditAnywhere, Interp, Category = "Audio")
  float Strength = 1.0f;

  // 0=bass, 1=treble
  UPROPERTY(EditAnywhere, Interp, Category = "Audio",
            meta = (ClampMin = "0.0", ClampMax = "1.0"))
  float FrequencyMin = 0.0f;


  UPROPERTY(EditAnywhere, Interp, Category = "Audio",
            meta = (ClampMin = "0.0", ClampMax = "1.0"))
  float FrequencyMax = 0.2f;


  UPROPERTY(EditAnywhere, Category = "Audio")
  TObjectPtr<UCurveFloat> RemapCurve;


  UPROPERTY(EditAnywhere, Category = "Audio", meta = (ClampMin = "0"))
  int32 AudioChannel = 0;

protected:
  virtual void ApplyBehavior(FTransform &Transform, int32 Index, int32 Count,
                             float &Time, TArray<float> &CustomData) override;
};

// Texture - samples a texture based on position, good for displacement maps
UCLASS(DisplayName = "Texture")
class KCLONER_API UKClonerModifier_Texture : public UKClonerModifier {
  GENERATED_BODY()

public:
  UKClonerModifier_Texture();

  // use grayscale textures or it gets weird
  UPROPERTY(EditAnywhere, Category = "Texture")
  TObjectPtr<UTexture2D> SourceTexture;


  UPROPERTY(EditAnywhere, Category = "Texture")
  EKClonerAudioMode Mode = EKClonerAudioMode::Scale;


  UPROPERTY(EditAnywhere, Category = "Texture")
  FVector Direction = FVector(0, 0, 1);


  UPROPERTY(EditAnywhere, Interp, Category = "Texture")
  float Strength = 1.0f;

  // bigger numbers = more repeats
  UPROPERTY(EditAnywhere, Category = "Texture")
  FVector2D Tiling = FVector2D(0.01f, 0.01f);


  UPROPERTY(EditAnywhere, Category = "Texture")
  FVector2D Offset = FVector2D(0.0f, 0.0f);

  // which plane to project from
  UPROPERTY(EditAnywhere, Category = "Texture")
  TEnumAsByte<EAxis::Type> ProjectionAxis = EAxis::Z;

protected:
  virtual void ApplyBehavior(FTransform &Transform, int32 Index, int32 Count,
                             float &Time, TArray<float> &CustomData) override;
#if WITH_EDITOR
  virtual void
  PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent) override;
#endif

private:
  TArray<FColor> CachedPixels;
  int32 CachedW = 0;
  int32 CachedH = 0;
  int32 CachedMip = 0;
  bool bCacheValid = false;
  float SampleTexture(float U, float V);
  void UpdateTextureCache();

public:
  UPROPERTY(EditAnywhere, Category = "Texture")
  int32 MipLevel = 0;
  UPROPERTY(EditAnywhere, Category = "Texture")
  bool bBilinearFiltering = true;
};

// Inheritance - morphs between two cloner layouts
// grid->radial, logo reveals, all that good mograph stuff
UCLASS(DisplayName = "Inheritance")
class KCLONER_API UKClonerModifier_Inheritance : public UKClonerModifier {
  GENERATED_BODY()

public:
  UKClonerModifier_Inheritance();

  // point this at another K-Cloner in your scene
  UPROPERTY(EditAnywhere, Category = "Inheritance")
  TSoftObjectPtr<class AKClonerActor> SourceCloner;

  // 0=this layout, 1=other cloner layout, animate this!
  UPROPERTY(EditAnywhere, Interp, Category = "Inheritance",
            meta = (ClampMin = "0.0", ClampMax = "1.0"))
  float BlendFactor = 0.0f;

  // index matching is faster, nearest neighbor looks better sometimes
  UPROPERTY(EditAnywhere, Category = "Inheritance")
  bool bMatchByIndex = true;


  UPROPERTY(EditAnywhere, Category = "Inheritance")
  TEnumAsByte<EEasingFunc::Type> EasingFunction = EEasingFunc::EaseInOut;


  UPROPERTY(EditAnywhere, Category = "Inheritance|Components")
  bool bBlendPosition = true;


  UPROPERTY(EditAnywhere, Category = "Inheritance|Components")
  bool bBlendRotation = true;


  UPROPERTY(EditAnywhere, Category = "Inheritance|Components")
  bool bBlendScale = true;

protected:
  virtual void ApplyBehavior(FTransform &Transform, int32 Index, int32 Count,
                             float &Time, TArray<float> &CustomData) override;

private:

  TArray<FTransform> CachedSourceTransforms;


  uint32 LastCacheFrame = 0;


  void UpdateSourceCache();
};

// ============================================================
// MOTION MODIFIERS - the pretty loopy stuff
// ============================================================

// Figure 8 - infinity symbol path, iconic mograph look
UCLASS(DisplayName = "Figure 8")
class KCLONER_API UKClonerModifier_Figure8 : public UKClonerModifier {
  GENERATED_BODY()

public:
  UKClonerModifier_Figure8();


  UPROPERTY(EditAnywhere, Interp, Category = "Figure8")
  float Speed = 1.0f;


  UPROPERTY(EditAnywhere, Interp, Category = "Figure8",
            meta = (ClampMin = "0.0"))
  float Width = 100.0f;


  UPROPERTY(EditAnywhere, Interp, Category = "Figure8",
            meta = (ClampMin = "0.0"))
  float Height = 50.0f;

  // offset per clone = cascading effect, try 0.1
  UPROPERTY(EditAnywhere, Interp, Category = "Figure8")
  float Step = 0.1f;


  UPROPERTY(EditAnywhere, Category = "Figure8")
  TEnumAsByte<EAxis::Type> UpAxis = EAxis::Z;

protected:
  virtual void ApplyBehavior(FTransform &Transform, int32 Index, int32 Count,
                             float &Time, TArray<float> &CustomData) override;
};

// Lissajous - fancy math curves for that PRO mograph look
// mess with the frequency ratios - 2:3, 3:4, etc for diff patterns
UCLASS(DisplayName = "Lissajous")
class KCLONER_API UKClonerModifier_Lissajous : public UKClonerModifier {
  GENERATED_BODY()

public:
  UKClonerModifier_Lissajous();


  UPROPERTY(EditAnywhere, Interp, Category = "Lissajous")
  float Speed = 1.0f;


  UPROPERTY(EditAnywhere, Interp, Category = "Lissajous",
            meta = (ClampMin = "0.0"))
  float Size = 100.0f;

  // try primes: 2, 3, 5, 7 etc
  UPROPERTY(EditAnywhere, Interp, Category = "Lissajous",
            meta = (ClampMin = "1"))
  int32 FrequencyA = 3;

  // make this different from A for cool patterns
  UPROPERTY(EditAnywhere, Interp, Category = "Lissajous",
            meta = (ClampMin = "1"))
  int32 FrequencyB = 2;

  // 0-2PI radians, changes the shape
  UPROPERTY(EditAnywhere, Interp, Category = "Lissajous",
            meta = (ClampMin = "0.0", ClampMax = "6.28318"))
  float Phase = 0.0f;


  UPROPERTY(EditAnywhere, Interp, Category = "Lissajous")
  float Step = 0.05f;

protected:
  virtual void ApplyBehavior(FTransform &Transform, int32 Index, int32 Count,
                             float &Time, TArray<float> &CustomData) override;
};

// Bounce - bouncing ball with squash n stretch
// use for playful cartoon-y stuff
UCLASS(DisplayName = "Bounce")
class KCLONER_API UKClonerModifier_Bounce : public UKClonerModifier {
  GENERATED_BODY()

public:
  UKClonerModifier_Bounce();


  UPROPERTY(EditAnywhere, Interp, Category = "Bounce")
  float Speed = 2.0f;


  UPROPERTY(EditAnywhere, Interp, Category = "Bounce",
            meta = (ClampMin = "0.0"))
  float Height = 100.0f;

  // 1.0 = full cartoon squash lol
  UPROPERTY(EditAnywhere, Interp, Category = "Bounce",
            meta = (ClampMin = "0.0", ClampMax = "1.0"))
  float Squash = 0.3f;


  UPROPERTY(EditAnywhere, Interp, Category = "Bounce")
  float Step = 0.1f;


  UPROPERTY(EditAnywhere, Category = "Bounce")
  FVector Direction = FVector(0, 0, 1);

protected:
  virtual void ApplyBehavior(FTransform &Transform, int32 Index, int32 Count,
                             float &Time, TArray<float> &CustomData) override;
};

// Pendulum - clock pendulum swing
// good for chandeliers, signs, hangin stuff
UCLASS(DisplayName = "Pendulum")
class KCLONER_API UKClonerModifier_Pendulum : public UKClonerModifier {
  GENERATED_BODY()

public:
  UKClonerModifier_Pendulum();


  UPROPERTY(EditAnywhere, Interp, Category = "Pendulum")
  float Speed = 1.0f;


  UPROPERTY(EditAnywhere, Interp, Category = "Pendulum",
            meta = (ClampMin = "0.0", ClampMax = "180.0"))
  float Angle = 30.0f;


  UPROPERTY(EditAnywhere, Interp, Category = "Pendulum")
  float Step = 0.1f;


  UPROPERTY(EditAnywhere, Category = "Pendulum")
  TEnumAsByte<EAxis::Type> RotationAxis = EAxis::X;

protected:
  virtual void ApplyBehavior(FTransform &Transform, int32 Index, int32 Count,
                             float &Time, TArray<float> &CustomData) override;
};

// Sway - trees/plants in wind
// stacks multiple sine waves for organic feel, way better than single sine
UCLASS(DisplayName = "Sway")
class KCLONER_API UKClonerModifier_Sway : public UKClonerModifier {
  GENERATED_BODY()

public:
  UKClonerModifier_Sway();


  UPROPERTY(EditAnywhere, Interp, Category = "Sway")
  float WindSpeed = 1.0f;


  UPROPERTY(EditAnywhere, Interp, Category = "Sway",
            meta = (ClampMin = "0.0", ClampMax = "45.0"))
  float SwayAngle = 10.0f;

  // secondary waves make it look alive
  UPROPERTY(EditAnywhere, Interp, Category = "Sway",
            meta = (ClampMin = "0.0", ClampMax = "1.0"))
  float DetailIntensity = 0.3f;

  // 0=all synced, 1=each clone does its own thing
  UPROPERTY(EditAnywhere, Interp, Category = "Sway",
            meta = (ClampMin = "0.0", ClampMax = "1.0"))
  float Randomization = 0.5f;


  UPROPERTY(EditAnywhere, Category = "Sway")
  TEnumAsByte<EAxis::Type> SwayAxis = EAxis::X;

  // perpendicular sway adds 3D depth
  UPROPERTY(EditAnywhere, Category = "Sway")
  bool bCrossAxis = true;

protected:
  virtual void ApplyBehavior(FTransform &Transform, int32 Index, int32 Count,
                             float &Time, TArray<float> &CustomData) override;
};

// ============================================================
// PRESET MODIFIER - load custom effects from .json files
// ============================================================

class UKClonerModifierPreset;

// per-instance variable override - lets users tweak preset params
USTRUCT(BlueprintType)
struct KCLONER_API FKClonerVariableOverride {
  GENERATED_BODY()

  /** Variable index (0 = v0, 1 = v1, etc.) */
  UPROPERTY(VisibleAnywhere, Category = "Override")
  int32 Index = 0;

  /** Variable name (from preset) */
  UPROPERTY(VisibleAnywhere, Category = "Override")
  FString Name;

  /** Overridden value */
  UPROPERTY(EditAnywhere, Interp, Category = "Override")
  float Value = 0.0f;

  /** Whether to use override or preset default */
  UPROPERTY(EditAnywhere, Category = "Override")
  bool bOverride = false;
};

/**
 * Preset Modifier: Loads behavior from a UKClonerModifierPreset DataAsset.
 *
 * This enables:
 * - Custom modifiers without C++ coding
 * - DLC modifier packs
 * - Studio-specific modifier libraries
 * - Runtime expression evaluation
 */
UCLASS(DisplayName = "Preset")
class KCLONER_API UKClonerModifier_Preset : public UKClonerModifier {
  GENERATED_BODY()

public:
  UKClonerModifier_Preset();
  virtual ~UKClonerModifier_Preset();

  /** The preset DataAsset defining this modifier's behavior */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset")
  TObjectPtr<UKClonerModifierPreset> Preset;

  /** Variable overrides - customize preset defaults per-modifier instance */
  UPROPERTY(EditAnywhere, Category = "Preset|Variables")
  TArray<FKClonerVariableOverride> VariableOverrides;

  /** Speed multiplier (stacks with preset's SpeedMultiplier) */
  UPROPERTY(EditAnywhere, Interp, Category = "Preset|Settings")
  float Speed = 1.0f;

  /** Step/delay between instances (stacks with preset's Step) */
  UPROPERTY(EditAnywhere, Interp, Category = "Preset|Settings")
  float Step = 0.0f;

protected:
  virtual void ApplyBehavior(FTransform &Transform, int32 Index, int32 Count,
                             float &Time, TArray<float> &CustomData) override;

#if WITH_EDITOR
  virtual void
  PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent) override;
#endif

private:
  // Cached compiled expressions (hidden from header)
  TUniquePtr<FKClonerCompiledExpression> CompiledPosition;
  TUniquePtr<FKClonerCompiledExpression> CompiledRotation;
  TUniquePtr<FKClonerCompiledExpression> CompiledScale;

  // Cached preset pointer for change detection
  TWeakObjectPtr<UKClonerModifierPreset> CachedPreset;

  /** Recompile expressions from preset */
  void CompileExpressions();

  /** Sync variable overrides with preset */
  void SyncVariableOverrides();

  /** Get resolved variable values (overrides applied) */
  TArray<float> GetResolvedVariables() const;
};

