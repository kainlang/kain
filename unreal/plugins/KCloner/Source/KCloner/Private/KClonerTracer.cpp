// Copyright 2026 K-Studio. All Rights Reserved.

#include "KClonerTracer.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "KClonerActor.h"


AKClonerTracer::AKClonerTracer() {
  PrimaryActorTick.bCanEverTick = true;
  
  // create root component
  Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
  RootComponent = Root;
}

void AKClonerTracer::BeginPlay() { 
  Super::BeginPlay(); 
}

void AKClonerTracer::Tick(float DeltaTime) {
  Super::Tick(DeltaTime);

  // recording mode - only sample when enabled
  if (bRecording) {
    TimeSinceLastSample += DeltaTime;

    // grab points at the sample rate (e.g. 30fps)
    if (TimeSinceLastSample >= (1.0f / SampleRate)) {
      TimeSinceLastSample = 0.0f;
      SamplePositions();
    }
  }

  // always update spline visuals
  UpdateSplines();

#if WITH_EDITOR
  // Debug drawing - only in editor so we can see trails
  if (bDebugDraw) {
    for (auto &Pair : TrailMap) {
      FTrailData &Trail = Pair.Value;
      
      // line visualization so we can see the trails in editor
      if (Trail.Points.Num() < 2)
        continue;

      FColor DrawColor = DebugColor.ToFColor(true);
      for (int32 i = 0; i < Trail.Points.Num() - 1; i++) {
        DrawDebugLine(
            GetWorld(), Trail.Points[i], Trail.Points[i + 1], DrawColor, false,
            -1.0f, 0,
            1.0f + (float)i / (float)Trail.Points.Num() * 3.0f // Thicker at end
        );
      }
    }
  }
#endif
}

// ============================================================================
// SAMPLE POSITIONS - grab cloner transforms and save em for each instance
// ============================================================================
void AKClonerTracer::SamplePositions() {
  AKClonerActor *Cloner = TrackedCloner.Get();
  if (!Cloner)
    return;

  UHierarchicalInstancedStaticMeshComponent *ISMC = Cloner->InstancedMesh;
  if (!ISMC)
    return;

  int32 InstanceCount = ISMC->GetInstanceCount();
  if (InstanceCount == 0)
    return;

  // which clones are we actually tracking?
  TArray<int32> IndicesToTrack;

  if (TrackedIndices.Num() > 0) {
    // Use specified indices
    for (int32 Idx : TrackedIndices) {
      if (Idx >= 0 && Idx < InstanceCount) {
        IndicesToTrack.Add(Idx);
      }
    }
  } else {
    // too many clones will kill performance, so there's a limit
    int32 TrackCount = (MaxTrackedClones > 0)
                           ? FMath::Min(MaxTrackedClones, InstanceCount)
                           : InstanceCount;
    for (int32 i = 0; i < TrackCount; i++) {
      IndicesToTrack.Add(i);
    }
  }

  // Sample each tracked clone
  for (int32 CloneIdx : IndicesToTrack) {
    // yoink world position
    FTransform InstanceTransform;
    ISMC->GetInstanceTransform(CloneIdx, InstanceTransform, true); // World space
    FVector WorldPos = InstanceTransform.GetLocation();

    // Get or create trail data
    FTrailData *Trail = TrailMap.Find(CloneIdx);
    if (!Trail) {
      // Create new trail
      TrailMap.Add(CloneIdx, FTrailData());
      Trail = TrailMap.Find(CloneIdx);
      Trail->LastSampledPosition = WorldPos;
      Trail->Spline = CreateSplineForTrail(CloneIdx);
    }

    // don't record if it hasn't moved enough
    // avoids wasting points when stationary
    float Dist = FVector::Dist(WorldPos, Trail->LastSampledPosition);
    if (Dist < MinSampleDistance && Trail->Points.Num() > 0) {
      continue; // Skip this sample, move to next clone
    }

    Trail->LastSampledPosition = WorldPos;

    // Add point to trail
    Trail->Points.Add(WorldPos);

    // trim back of the trail if it's too long
    while (Trail->Points.Num() > TrailLength) {
      Trail->Points.RemoveAt(0);
    }
  }
}

// ============================================================================
// UPDATE SPLINES - sync spline components with sampled trail data
// ============================================================================
void AKClonerTracer::UpdateSplines() {
  for (auto &Pair : TrailMap) {
    int32 CloneIdx = Pair.Key;
    FTrailData &Trail = Pair.Value;

    if (!Trail.Spline) {
      Trail.Spline = CreateSplineForTrail(CloneIdx);
    }

    if (Trail.Points.Num() > 0) {
      // update the spline component points to match our sampled data
      // this feeds into SplineMesh system for the actual VFX mesh
      Trail.Spline->ClearSplinePoints(false);

      // Add all points
      for (int32 i = 0; i < Trail.Points.Num(); i++) {
        Trail.Spline->AddSplinePoint(Trail.Points[i],
                                     ESplineCoordinateSpace::World, false);
      }

      Trail.Spline->UpdateSpline();
    }
  }
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================
USplineComponent *AKClonerTracer::CreateSplineForTrail(int32 CloneIndex) {
  FName SplineName = *FString::Printf(TEXT("Trail_%d"), CloneIndex);
  USplineComponent *Spline = NewObject<USplineComponent>(this, SplineName);
  Spline->SetupAttachment(Root);
  Spline->RegisterComponent();
  Spline->ClearSplinePoints(false);
  Spline->SetDrawDebug(false); // We handle our own debug drawing
  return Spline;
}

void AKClonerTracer::ClearTrails() {
  for (auto &Pair : TrailMap) {
    Pair.Value.Points.Empty();
    if (Pair.Value.Spline) {
      Pair.Value.Spline->ClearSplinePoints(true);
    }
  }
}

USplineComponent *AKClonerTracer::GetTrailSpline(int32 CloneIndex) const {
  const FTrailData *Trail = TrailMap.Find(CloneIndex);
  return Trail ? Trail->Spline : nullptr;
}

