// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "KClonerActor.h"
#include "KClonerModifier.h"
#include "KClonerBlueprintLibrary.generated.h"


// BP function library - call these from any Blueprint
// super handy for runtime cloner control
UCLASS()
class KCLONER_API UKClonerBlueprintLibrary : public UBlueprintFunctionLibrary {
  GENERATED_BODY()

public:
  // ======= FIND STUFF =======


  UFUNCTION(BlueprintCallable, Category = "K-Cloner",
            meta = (WorldContext = "WorldContextObject"))
  static TArray<AKClonerActor *>
  GetAllKCloners(const UObject *WorldContextObject);


  UFUNCTION(BlueprintCallable, Category = "K-Cloner",
            meta = (WorldContext = "WorldContextObject"))
  static AKClonerActor *GetKClonerByTag(const UObject *WorldContextObject,
                                        FName Tag);


  UFUNCTION(BlueprintCallable, Category = "K-Cloner",
            meta = (WorldContext = "WorldContextObject"))
  static AKClonerActor *GetNearestKCloner(const UObject *WorldContextObject,
                                          FVector WorldLocation);


  UFUNCTION(BlueprintPure, Category = "K-Cloner",
            meta = (WorldContext = "WorldContextObject"))
  static int32 GetTotalCloneCount(const UObject *WorldContextObject);

  // ======= BULK OPS =======


  UFUNCTION(BlueprintCallable, Category = "K-Cloner|Bulk",
            meta = (WorldContext = "WorldContextObject"))
  static void RebuildAllKCloners(const UObject *WorldContextObject);


  UFUNCTION(BlueprintCallable, Category = "K-Cloner|Bulk",
            meta = (WorldContext = "WorldContextObject"))
  static void SetAllKClonersTimeScale(const UObject *WorldContextObject,
                                      float TimeScale);


  UFUNCTION(BlueprintCallable, Category = "K-Cloner|Bulk",
            meta = (WorldContext = "WorldContextObject"))
  static void PauseAllKCloners(const UObject *WorldContextObject);


  UFUNCTION(BlueprintCallable, Category = "K-Cloner|Bulk",
            meta = (WorldContext = "WorldContextObject"))
  static void ResumeAllKCloners(const UObject *WorldContextObject);

  // ======= CLICK/RAYCAST =======

  // raycast to find which clone got hit, returns -1 if missed
  UFUNCTION(BlueprintCallable, Category = "K-Cloner|Interaction",
            meta = (WorldContext = "WorldContextObject"))
  static int32 RaycastToClone(const UObject *WorldContextObject,
                              AKClonerActor *Cloner, FVector RayStart,
                              FVector RayEnd, FVector &HitLocation);

  // click-to-select helper
  UFUNCTION(BlueprintCallable, Category = "K-Cloner|Interaction",
            meta = (WorldContext = "WorldContextObject"))
  static int32 GetCloneAtScreenPosition(const UObject *WorldContextObject,
                                        AKClonerActor *Cloner,
                                        FVector2D ScreenPosition);

  // ======= MATH (use in expressions/custom modifiers) =======


  UFUNCTION(BlueprintPure, Category = "K-Cloner|Math")
  static float Remap(float Value, float InMin, float InMax, float OutMin,
                     float OutMax);

  // oscillates 0->1->0->1... good for bobbing
  UFUNCTION(BlueprintPure, Category = "K-Cloner|Math")
  static float PingPong(float Time, float Period);

  // ease in/out
  UFUNCTION(BlueprintPure, Category = "K-Cloner|Math")
  static float SmoothStep(float Value);

  // fancy math curve, looks cool
  UFUNCTION(BlueprintPure, Category = "K-Cloner|Math")
  static FVector LissajousCurve(float Time, float FreqX, float FreqY,
                                float FreqZ, float Amplitude);

  // infinity symbol path
  UFUNCTION(BlueprintPure, Category = "K-Cloner|Math")
  static FVector Figure8Curve(float Time, float Width, float Height);
};
