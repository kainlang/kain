// Copyright 2026 K-Studio. All Rights Reserved.

#include "KClonerActor.h"
#include "KClonerModifier.h"
#include "KClonerEffectorComponent.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Async/ParallelFor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Animation/AnimSequence.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "KClonerEffectorComponent.h"
#include "Rendering/PositionVertexBuffer.h"
#include "Rendering/StaticMeshVertexBuffer.h"
#include "StaticMeshResources.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Niagara/KClonerDataInterface.h"

AKClonerActor::AKClonerActor() {
  PrimaryActorTick.bCanEverTick = true;

  Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
  RootComponent = Root;

  SplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
  SplineComponent->SetupAttachment(Root);

  InstancedMesh =
      CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
          TEXT("InstancedMesh"));
  InstancedMesh->SetupAttachment(Root);
  InstancedMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

  // Second HISM for VAT-rendered clones (Auto/VATBaked modes)
  VATInstancedMesh =
      CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
          TEXT("VATInstancedMesh"));
  VATInstancedMesh->SetupAttachment(Root);
  VATInstancedMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  VATInstancedMesh->SetVisibility(false); // off by default, turns on for VAT mode

  TemplateSkeletalComponent = CreateDefaultSubobject<USkeletalMeshComponent>(
      TEXT("TemplateSkeletalComponent"));
  TemplateSkeletalComponent->SetupAttachment(Root);
  TemplateSkeletalComponent->SetCollisionEnabled(
      ECollisionEnabled::NoCollision);
  TemplateSkeletalComponent->SetVisibility(false);

  // Set a default cube mesh
  static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultMeshFinder(
      TEXT("/Engine/BasicShapes/Cube.Cube"));
  if (DefaultMeshFinder.Succeeded()) {
    SourceMesh = DefaultMeshFinder.Object;
  }
  // default output path for baked anims
  OutputFolder.Path = TEXT("/Game/Animations/Tweaked");

  // Add default distribution layer
  FKClonerDistributionLayer DefaultLayer;
  DefaultLayer.bEnabled = true;
  DefaultLayer.Mode = EKClonerMode::Grid;
  DefaultLayer.GridCount = FIntVector(3, 3, 1);
  DefaultLayer.GridSpacing = FVector(150.0f, 150.0f, 150.0f);
  DistributionLayers.Add(DefaultLayer);
}

void AKClonerActor::BeginPlay() {
  Super::BeginPlay();
  bNeedsRebuild = true;
  RebuildInstances();
}

void AKClonerActor::OnConstruction(const FTransform &Transform) {
  Super::OnConstruction(Transform);
  bNeedsRebuild = true;
  RebuildInstances();
}

#if WITH_EDITOR
void AKClonerActor::PostEditChangeProperty(
    FPropertyChangedEvent &PropertyChangedEvent) {
  Super::PostEditChangeProperty(PropertyChangedEvent);

  FName PropertyName = PropertyChangedEvent.GetPropertyName();
  if (PropertyName == GET_MEMBER_NAME_CHECKED(AKClonerActor, ClonerPreset)) {
    if (ClonerPreset) {
      ApplyPreset(ClonerPreset);
    }
  }
  // Sync proxy properties if DistributionLayers changed
  else if (PropertyName ==
               GET_MEMBER_NAME_CHECKED(AKClonerActor, DistributionLayers) ||
           PropertyName == NAME_None) {
    for (const FKClonerDistributionLayer &Layer : DistributionLayers) {
      if (Layer.Mode == EKClonerMode::Grid) {
        GridCount = FVector(Layer.GridCount);
        GridSpacing = Layer.GridSpacing;
        break;
      }
    }
  }
  // Handle VFX preset changes
  else if (PropertyName ==
               GET_MEMBER_NAME_CHECKED(AKClonerActor, NiagaraEmitter) ||
           PropertyName == GET_MEMBER_NAME_CHECKED(AKClonerActor, VFXScale)) {
    UpdateVFXComponent();
  }

  bNeedsRebuild = true;
  bModifiersDirty = true;
  RebuildInstances();
}

bool AKClonerActor::ShouldTickIfViewportsOnly() const { return true; }
#endif

void AKClonerActor::ForceRebuild() {
  bNeedsRebuild = true;
  bModifiersDirty = true;
  RebuildInstances();
}

// =========== SEQUENCER SETTERS ===========
// these are here so Sequencer can keyframe our props

void AKClonerActor::SetTimeScale(float InTimeScale) {
  TimeScale = FMath::Clamp(InTimeScale, 0.0f, 10.0f);
}

void AKClonerActor::SetEffectorRadius(float InRadius) {
  EffectorRadius = FMath::Max(0.0f, InRadius);
  bModifiersDirty = true;
}

void AKClonerActor::SetEffectorFalloff(float InFalloff) {
  EffectorFalloff = FMath::Clamp(InFalloff, 0.0f, 1.0f);
  bModifiersDirty = true;
}

void AKClonerActor::SetGridCount(FVector InCount) {
  GridCount = InCount; // Sync proxy property
  FIntVector IntCount(FMath::RoundToInt(InCount.X),
                      FMath::RoundToInt(InCount.Y),
                      FMath::RoundToInt(InCount.Z));

  // Find first grid layer or add one
  bool bFound = false;
  for (FKClonerDistributionLayer &Layer : DistributionLayers) {
    if (Layer.Mode == EKClonerMode::Grid) {
      Layer.GridCount = IntCount;
      bFound = true;
      break;
    }
  }

  if (!bFound) {
    if (DistributionLayers.Num() == 0) {
      FKClonerDistributionLayer NewLayer;
      NewLayer.Mode = EKClonerMode::Grid;
      NewLayer.GridCount = IntCount;
      DistributionLayers.Add(NewLayer);
    } else {
      FKClonerDistributionLayer NewLayer;
      NewLayer.Mode = EKClonerMode::Grid;
      NewLayer.GridCount = IntCount;
      DistributionLayers.Add(NewLayer);
    }
  }

  ForceRebuild();
}

FVector AKClonerActor::GetGridCount() const {
  // just grab the first grid layer
  for (const FKClonerDistributionLayer &Layer : DistributionLayers) {
    if (Layer.Mode == EKClonerMode::Grid) {
      return FVector(Layer.GridCount);
    }
  }
  return GridCount;
}

void AKClonerActor::SetGridSpacing(FVector InSpacing) {
  GridSpacing = InSpacing; // Sync proxy property

  bool bFound = false;
  for (FKClonerDistributionLayer &Layer : DistributionLayers) {
    if (Layer.Mode == EKClonerMode::Grid) {
      Layer.GridSpacing = InSpacing;
      bFound = true;
      break;
    }
  }

  if (!bFound) {
    if (DistributionLayers.Num() == 0) {
      FKClonerDistributionLayer NewLayer;
      NewLayer.Mode = EKClonerMode::Grid;
      NewLayer.GridSpacing = InSpacing;
      DistributionLayers.Add(NewLayer);
    } else {
      FKClonerDistributionLayer NewLayer;
      NewLayer.Mode = EKClonerMode::Grid;
      NewLayer.GridSpacing = InSpacing;
      DistributionLayers.Add(NewLayer);
    }
  }

  ForceRebuild();
}

FVector AKClonerActor::GetGridSpacing() const {
  for (const FKClonerDistributionLayer &Layer : DistributionLayers) {
    if (Layer.Mode == EKClonerMode::Grid) {
      return Layer.GridSpacing;
    }
  }
  return GridSpacing;
}

void AKClonerActor::ApplyPreset(UKClonerData *Preset) {
  if (!Preset)
    return;

  SourceMesh = Preset->SourceMesh;
  SourceSkeletalMesh = Preset->SourceSkeletalMesh;
  SourceAnimSequence = Preset->SourceAnimSequence;
  TimeScale = Preset->TimeScale;
  DistributionLayers = Preset->Layers;

  // Sync proxy properties from loaded preset
  for (const FKClonerDistributionLayer &Layer : DistributionLayers) {
    if (Layer.Mode == EKClonerMode::Grid) {
      GridCount = FVector(Layer.GridCount);
      GridSpacing = Layer.GridSpacing;
      break;
    }
  }

  Modifiers.Empty();
  for (UKClonerModifier *SrcMod : Preset->Modifiers) {
    if (SrcMod) {
      UKClonerModifier *NewMod =
          DuplicateObject<UKClonerModifier>(SrcMod, this);
      Modifiers.Add(NewMod);
    }
  }

  bNeedsRebuild = true;
  bModifiersDirty = true;
  RebuildInstances();
}

void AKClonerActor::PlayClonerMontage(UAnimMontage *Montage, float InPlayRate,
                                      FName StartSectionName) {
  if (!Montage || SourceSkeletalMesh == nullptr)
    return;

  for (USkeletalMeshComponent *Comp : SkeletalMeshPool) {
    if (Comp) {
      UAnimInstance *AnimInst = Comp->GetAnimInstance();
      if (AnimInst) {
        AnimInst->Montage_Play(Montage, InPlayRate);
        if (StartSectionName != NAME_None) {
          AnimInst->Montage_JumpToSection(StartSectionName, Montage);
        }
      }
    }
  }
}

void AKClonerActor::Tick(float DeltaTime) {
  Super::Tick(DeltaTime);
  // skip frames for perf if user wants
  if (UpdateSkipFrames > 0) {
    FrameCounter++;
    if (FrameCounter <= UpdateSkipFrames) {
      return;
    }
    FrameCounter = 0;
  }

  UpdateModifierAnimation(DeltaTime);
}

// effector influence calc - decides how much modifiers affect each clone
float AKClonerActor::GetEffectorInfluence(const FVector &WorldPosition) const {
  // auto-discover mode
  if (bAutoDiscoverEffectors) {
    if (CachedEffectors.Num() == 0) {
      return 1.0f; // no effectors = full influence everywhere
    }

    return FKClonerEffectorFinder::GetCombinedInfluence(CachedEffectors,
                                                        WorldPosition);
  }
  // old-school manual effector mode
  AActor *EffectorActor = Effector.Get();
  if (!EffectorActor) {
    return 1.0f;
  }

  FVector EffectorPos = EffectorActor->GetActorLocation();
  FVector LocalPos =
      EffectorActor->GetActorTransform().InverseTransformPosition(
          WorldPosition);

  float NormalizedDistance = 0.0f;

  switch (EffectorShape) {
  case EKClonerEffectorShape::Sphere: {
    if (EffectorRadius <= 0.0f)
      return 1.0f;
    float Distance = FVector::Dist(WorldPosition, EffectorPos);
    NormalizedDistance = Distance / EffectorRadius;
    break;
  }

  case EKClonerEffectorShape::Box: {
    FVector AbsLocal = LocalPos.GetAbs();
    FVector Extent = EffectorExtent;
    if (Extent.GetMin() <= 0.0f)
      return 1.0f;

    // chebyshev distance basically
    FVector Ratio = AbsLocal / Extent;
    NormalizedDistance = Ratio.GetMax();
    break;
  }

  case EKClonerEffectorShape::Plane: {
    // plane is infinite in XY, distance along Z
    if (EffectorRadius <= 0.0f)
      return 1.0f;
    float Distance = FMath::Abs(LocalPos.Z);
    NormalizedDistance = Distance / EffectorRadius;
    break;
  }

  case EKClonerEffectorShape::Cylinder: {
    if (EffectorRadius <= 0.0f)
      return 1.0f;
    // infinte cylinder along Z
    float Distance2D = FVector2D(LocalPos.X, LocalPos.Y).Size();
    NormalizedDistance = Distance2D / EffectorRadius;
    break;
  }

  case EKClonerEffectorShape::Torus: {
    if (EffectorRadius <= 0.0f)
      return 1.0f;
    // donut shape innit
    float Distance2D = FVector2D(LocalPos.X, LocalPos.Y).Size();
    float TorusCenter = EffectorRadius;
    FVector TorusSample(Distance2D - TorusCenter, 0.0f, LocalPos.Z);
    float DistanceToTube = TorusSample.Size();
    float TubeRadius = EffectorInnerRadius > 0.0f ? EffectorInnerRadius
                                                  : EffectorRadius * 0.25f;
    NormalizedDistance = DistanceToTube / TubeRadius;
    break;
  }

  case EKClonerEffectorShape::Unbound:
  default:
    return bInvertEffector ? 0.0f : 1.0f;
  }

  if (bInvertEffector) {
    NormalizedDistance = 1.0f - NormalizedDistance;
  }


  if (NormalizedDistance >= 1.0f) {
    return bInvertEffector ? 1.0f : 0.0f;
  }


  if (EffectorFalloff <= 0.0f) {
    return 1.0f; // Hard edge
  }

  float InnerThreshold = 1.0f - EffectorFalloff;
  if (NormalizedDistance <= InnerThreshold) {
    return 1.0f;
  }
  // smooth falloff zone
  float T = (NormalizedDistance - InnerThreshold) / EffectorFalloff;
  return FMath::SmoothStep(1.0f, 0.0f, T);
}

void AKClonerActor::EnsureCacheCapacity(int32 Count) {
  if (CachedTransforms.Num() != Count) {
    CachedTransforms.SetNumUninitialized(Count);
  }
  if (CachedInstanceData.Num() != Count) {
    CachedInstanceData.SetNum(Count);
  }
}

TArray<FTransform>
AKClonerActor::GenerateLayerTransforms(const FKClonerDistributionLayer &Layer) {
  TArray<FTransform> OutTransforms;

  switch (Layer.Mode) {
  case EKClonerMode::Grid: {
    FVector Offset =
        FVector((Layer.GridCount.X - 1) * Layer.GridSpacing.X * 0.5f,
                (Layer.GridCount.Y - 1) * Layer.GridSpacing.Y * 0.5f,
                (Layer.GridCount.Z - 1) * Layer.GridSpacing.Z * 0.5f);

    for (int32 z = 0; z < Layer.GridCount.Z; z++) {
      for (int32 y = 0; y < Layer.GridCount.Y; y++) {
        for (int32 x = 0; x < Layer.GridCount.X; x++) {
          FVector Pos =
              FVector(x * Layer.GridSpacing.X, y * Layer.GridSpacing.Y,
                      z * Layer.GridSpacing.Z) -
              Offset;
          OutTransforms.Add(
              FTransform(FRotator::ZeroRotator, Pos, FVector::OneVector));
        }
      }
    }
    break;
  }
  case EKClonerMode::Radial: {
    if (Layer.RadialCount <= 0)
      break;
    float AngleStep = 360.0f / Layer.RadialCount;

    for (int32 i = 0; i < Layer.RadialCount; i++) {
      float Angle = i * AngleStep;
      float Rad = FMath::DegreesToRadians(Angle);
      FVector Pos = FVector(FMath::Cos(Rad) * Layer.RadialRadius,
                            FMath::Sin(Rad) * Layer.RadialRadius, 0.0f);
      FRotator Rot =
          Layer.bRadialAlign ? FRotator(0, Angle, 0) : FRotator::ZeroRotator;
      OutTransforms.Add(FTransform(Rot, Pos, FVector::OneVector));
    }
    break;
  }
  case EKClonerMode::Linear: {
    if (Layer.LinearCount <= 0)
      break;
    FVector TotalOffset = Layer.LinearOffset * (Layer.LinearCount - 1);
    FVector StartPos = -TotalOffset * 0.5f;

    for (int32 i = 0; i < Layer.LinearCount; i++) {
      FVector Pos = StartPos + (Layer.LinearOffset * i);
      OutTransforms.Add(
          FTransform(FRotator::ZeroRotator, Pos, FVector::OneVector));
    }
    break;
  }
  case EKClonerMode::Spline: {
    if (Layer.SplineCount <= 0 || !SplineComponent)
      break;

    float SplineLen = SplineComponent->GetSplineLength();
    if (SplineLen <= KINDA_SMALL_NUMBER)
      break;

    bool bClosed = SplineComponent->IsClosedLoop();
    float Step = bClosed ? SplineLen / (float)Layer.SplineCount
                         : ((Layer.SplineCount > 1)
                                ? SplineLen / (float)(Layer.SplineCount - 1)
                                : 0.0f);

    for (int32 i = 0; i < Layer.SplineCount; i++) {
      float Distance = i * Step;
      FVector Pos = SplineComponent->GetLocationAtDistanceAlongSpline(
          Distance, ESplineCoordinateSpace::Local);
      FRotator Rot = FRotator::ZeroRotator;

      if (Layer.bSplineAlign) {
        Rot = SplineComponent->GetRotationAtDistanceAlongSpline(
            Distance, ESplineCoordinateSpace::Local);
      }

      OutTransforms.Add(FTransform(Rot, Pos, FVector::OneVector));
    }
    break;
  }
  case EKClonerMode::Honeycomb: {
    float Size = Layer.HoneycombSize;
    float XSpacing = Size * FMath::Sqrt(3.0f);
    float YSpacing = Size * 1.5f;

    FVector Offset =
        FVector((Layer.HoneycombCount.X - 1) * XSpacing * 0.5f,
                (Layer.HoneycombCount.Y - 1) * YSpacing * 0.5f, 0.0f);

    for (int32 y = 0; y < Layer.HoneycombCount.Y; y++) {
      for (int32 x = 0; x < Layer.HoneycombCount.X; x++) {
        float RowOffset = (y % 2 != 0) ? (XSpacing * 0.5f) : 0.0f;
        FVector Pos =
            FVector(x * XSpacing + RowOffset, y * YSpacing, 0.0f) - Offset;
        OutTransforms.Add(
            FTransform(FRotator::ZeroRotator, Pos, FVector::OneVector));
      }
    }
    break;
  }
  case EKClonerMode::Scatter: {
    if (Layer.ScatterCount <= 0)
      break;

    FRandomStream Stream(Layer.ScatterSeed);
    FVector Bounds = Layer.ScatterBounds;

    for (int32 i = 0; i < Layer.ScatterCount; i++) {
      FVector Pos = Stream.RandPointInBox(FBox(-Bounds * 0.5f, Bounds * 0.5f));
      OutTransforms.Add(
          FTransform(FRotator::ZeroRotator, Pos, FVector::OneVector));
    }
    break;
  }
  case EKClonerMode::Mesh: {
    if (!Layer.MeshAsset || Layer.MeshCount <= 0)
      break;
    OutTransforms = SampleMeshPoints(Layer.MeshAsset, Layer.MeshCount,
                                     Layer.MeshSampleMode, Layer.MeshSeed);
    break;
  }
  case EKClonerMode::Single: {
    OutTransforms.Add(FTransform::Identity);
    break;
  }
  }

  return OutTransforms;
}

TArray<FTransform> AKClonerActor::SampleMeshPoints(UStaticMesh *Mesh,
                                                   int32 Count,
                                                   EKClonerMeshSampleMode Mode,
                                                   int32 Seed) {
  TArray<FTransform> Result;
  if (!Mesh || !Mesh->GetRenderData())
    return Result;

  Result.Reserve(Count);
  FRandomStream Stream(Seed);

  // Access Render Data (LOD 0)
  const FStaticMeshLODResources &LOD = Mesh->GetRenderData()->LODResources[0];
  const FPositionVertexBuffer &PosBuffer =
      LOD.VertexBuffers.PositionVertexBuffer;
  const FStaticMeshVertexBuffer &NormBuffer =
      LOD.VertexBuffers.StaticMeshVertexBuffer;

  uint32 NumVerts = PosBuffer.GetNumVertices();
  if (NumVerts == 0)
    return Result;

  // Helper to make rotation from Normal (Up Vector) and Random alignment
  auto MakeRotFromNormal = [](const FVector &Normal) {
    return FRotationMatrix::MakeFromZ(Normal).ToQuat().Rotator();
  };

  if (Mode == EKClonerMeshSampleMode::Vertex) {
    for (int32 i = 0; i < Count; i++) {
      uint32 VertIdx = Stream.RandHelper(NumVerts);
      FVector Pos = (FVector)PosBuffer.VertexPosition(VertIdx);
      FVector Normal = (FVector)NormBuffer.VertexTangentZ(VertIdx);
      Result.Add(
          FTransform(MakeRotFromNormal(Normal), Pos, FVector::OneVector));
    }
  } else if (Mode == EKClonerMeshSampleMode::Surface) {
    const FRawStaticIndexBuffer &IndexBuffer = LOD.IndexBuffer;
    uint32 NumIndices = IndexBuffer.GetNumIndices();
    uint32 NumTriangles = NumIndices / 3;

    if (NumTriangles == 0)
      return Result;

    for (int32 i = 0; i < Count; i++) {
      // Pick random triangle
      uint32 TriIdx = Stream.RandHelper(NumTriangles);
      uint32 Index0 = IndexBuffer.GetIndex(TriIdx * 3 + 0);
      uint32 Index1 = IndexBuffer.GetIndex(TriIdx * 3 + 1);
      uint32 Index2 = IndexBuffer.GetIndex(TriIdx * 3 + 2);

      FVector P0 = (FVector)PosBuffer.VertexPosition(Index0);
      FVector P1 = (FVector)PosBuffer.VertexPosition(Index1);
      FVector P2 = (FVector)PosBuffer.VertexPosition(Index2);

      FVector N0 = (FVector)NormBuffer.VertexTangentZ(Index0);
      FVector N1 = (FVector)NormBuffer.VertexTangentZ(Index1);
      FVector N2 = (FVector)NormBuffer.VertexTangentZ(Index2);

      // Random Barycentric Coordinates (Uniform Triangle Sampling)
      float r1 = Stream.GetFraction();
      float r2 = Stream.GetFraction();
      float sqrtR1 = FMath::Sqrt(r1);
      float u = 1.0f - sqrtR1;
      float v = r2 * sqrtR1;
      float w = 1.0f - u - v;

      FVector FinalPos = u * P0 + v * P1 + w * P2;
      FVector FinalNormal = (u * N0 + v * N1 + w * N2).GetSafeNormal();

      Result.Add(FTransform(MakeRotFromNormal(FinalNormal), FinalPos,
                            FVector::OneVector));
    }
  } else // Volume (Box fallback for now)
  {
    // TODO: Implement proper volume sampling (e.g. rejection sampling)
    // For now, simple bounding box scatter
    FBox Bounds = Mesh->GetBoundingBox();
    for (int32 i = 0; i < Count; i++) {
      FVector Pos = Stream.RandPointInBox(Bounds);
      Result.Add(FTransform(FRotator::ZeroRotator, Pos, FVector::OneVector));
    }
  }

  return Result;
}

void AKClonerActor::RebuildInstances() {
  if (!InstancedMesh)
    return;

  InstancedMesh->SetStaticMesh(SourceMesh);
  InstancedMesh->ClearInstances();
  BaseTransforms.Empty();

  if (bAnimTweakMode) {
    BaseTransforms.Add(FTransform::Identity);
  } else {
    if (!SourceMesh && !SourceSkeletalMesh)
      return;

    // Combinatorial Generation
    TArray<FTransform> CurrentTransforms;
    CurrentTransforms.Add(FTransform::Identity);

    for (const FKClonerDistributionLayer &Layer : DistributionLayers) {
      if (!Layer.bEnabled)
        continue;

      TArray<FTransform> LayerTransforms = GenerateLayerTransforms(Layer);
      if (LayerTransforms.Num() == 0)
        continue;

      TArray<FTransform> CombinedTransforms;
      CombinedTransforms.Reserve(CurrentTransforms.Num() *
                                 LayerTransforms.Num());

      // Hard limit
      if ((int64)CurrentTransforms.Num() * (int64)LayerTransforms.Num() >
          100000) {
        break;
      }

      for (const FTransform &Existing : CurrentTransforms) {
        for (const FTransform &LayerT : LayerTransforms) {
          FTransform Combined = LayerT * Existing;
          CombinedTransforms.Add(Combined);
        }
      }

      CurrentTransforms = MoveTemp(CombinedTransforms);
    }

    BaseTransforms = CurrentTransforms;
  }
  EnsureCacheCapacity(BaseTransforms.Num());

  // Update rendering method
  if (SourceSkeletalMesh) {
    InstancedMesh->ClearInstances();
    InstancedMesh->SetVisibility(false);
    UpdateSkeletalMeshPool(BaseTransforms.Num());
  } else {
    UpdateSkeletalMeshPool(0);
    InstancedMesh->SetVisibility(true);
    InstancedMesh->SetStaticMesh(SourceMesh);
    InstancedMesh->NumCustomDataFloats = 3;

    for (const FTransform &Trans : BaseTransforms) {
      InstancedMesh->AddInstance(Trans);
    }
  }

  bNeedsRebuild = false;
  bModifiersDirty = true;

  // Broadcast rebuild event for Blueprint listeners
  OnClonerRebuilt.Broadcast();
}

void AKClonerActor::UpdateSkeletalMeshPool(int32 TargetCount) {
  while (SkeletalMeshPool.Num() > TargetCount) {
    USkeletalMeshComponent *Comp = SkeletalMeshPool.Pop();
    if (Comp)
      Comp->DestroyComponent();
  }

  while (SkeletalMeshPool.Num() < TargetCount) {
    FString CompName =
        FString::Printf(TEXT("SkeletalClone_%d"), SkeletalMeshPool.Num());
    USkeletalMeshComponent *NewComp =
        NewObject<USkeletalMeshComponent>(this, *CompName);
    NewComp->SetupAttachment(Root);
    NewComp->RegisterComponent();
    SkeletalMeshPool.Add(NewComp);
  }

  for (USkeletalMeshComponent *Comp : SkeletalMeshPool) {
    if (Comp) {
      if (Comp->GetSkeletalMeshAsset() != SourceSkeletalMesh) {
        Comp->SetSkeletalMeshAsset(SourceSkeletalMesh);
      }

      if (SourceAnimSequence) {
        if (Comp->GetAnimationMode() != EAnimationMode::AnimationSingleNode) {
          Comp->SetAnimationMode(EAnimationMode::AnimationSingleNode);
        }

        UAnimSingleNodeInstance *SingleNode = Comp->GetSingleNodeInstance();
        if (!SingleNode ||
            SingleNode->GetAnimationAsset() != SourceAnimSequence) {
          Comp->PlayAnimation(SourceAnimSequence, true);
        }
      }

      Comp->SetVisibility(true);
      Comp->SetHiddenInGame(false);
    }
  }
}

void AKClonerActor::UpdateModifierAnimation(float DeltaTime) {
  int32 Count = BaseTransforms.Num();
  if (!InstancedMesh || Count == 0)
    return;

  // Early out if no modifiers and optimization enabled
  if (Modifiers.Num() == 0 && bOptimizeStaticInstances && !bModifiersDirty) {
    return;
  }

  // Calculate world time
  float WorldTime = 0.0f;
  if (bUseOverrideTime) {
    WorldTime = OverrideTime;
  } else if (GetWorld()) {
    WorldTime = GetWorld()->GetTimeSeconds() * TimeScale;
  }

  // Optimization: Skip if time hasn't changed and not dirty (ONLY in Game
  // World) In Editor/Sequencer, we always update to capture property changes
  // that don't affect Time
  bool bIsGameWorld = GetWorld() && GetWorld()->IsGameWorld();
  if (bIsGameWorld && bOptimizeStaticInstances && !bModifiersDirty &&
      FMath::IsNearlyEqual(WorldTime, LastWorldTime, 0.0001f)) {
    return;
  }
  LastWorldTime = WorldTime;

  // Ensure cache is sized correctly
  EnsureCacheCapacity(Count);

  // Cache effectors once per update (using optimized KD-Tree search via
  // Subsystem)
  CachedEffectors.Reset();
  if (bAutoDiscoverEffectors) {
    CachedEffectors = FKClonerEffectorFinder::FindEffectorsNear(
        GetWorld(), GetActorLocation(), EffectorSearchRadius);
  }

  // Get actor world transform once
  FTransform ActorTransform = GetActorTransform();

  // PERF: Use ParallelFor when we have lots of instances
  // Threshold of 500 - below that the threading overhead isn't worth it
  const int32 ParallelThreshold = 500;
  const bool bUseParallel = (Count >= ParallelThreshold) && !GIsEditor;

  // Lambda that processes a single instance - used by both paths
  auto ProcessInstance = [&](int32 i) {
    FTransform Trans = BaseTransforms[i];
    float InstanceTime = WorldTime;
    FKClonerInstanceCache &Cache = CachedInstanceData[i];

    // Reset defaults
    Cache.R = 1.0f;
    Cache.G = 1.0f;
    Cache.B = 1.0f;
    Cache.bVisible = true;

    // Calculate effector influence
    FVector WorldPos = ActorTransform.TransformPosition(Trans.GetLocation());
    float EffectorInfluence = GetEffectorInfluence(WorldPos);

    // Apply modifiers (only if effector allows)
    if (EffectorInfluence > 0.0f && Modifiers.Num() > 0) {
      // Stack-allocated for thread safety (no shared state)
      float PerInstanceData[3] = { 1.0f, 1.0f, 1.0f };
      TArray<float> PerInstanceDataArray;
      PerInstanceDataArray.SetNumUninitialized(3);
      PerInstanceDataArray[0] = 1.0f;
      PerInstanceDataArray[1] = 1.0f;
      PerInstanceDataArray[2] = 1.0f;

      for (UKClonerModifier *Mod : Modifiers) {
        if (Mod) {
          Mod->ApplyModifier(Trans, i, Count, InstanceTime, PerInstanceDataArray);
        }
      }

      // Blend with effector influence
      if (EffectorInfluence < 1.0f) {
        FTransform BaseTrans = BaseTransforms[i];
        Trans.BlendWith(BaseTrans, 1.0f - EffectorInfluence);
      }

      Cache.R = PerInstanceDataArray[0];
      Cache.G = PerInstanceDataArray[1];
      Cache.B = PerInstanceDataArray[2];
    }

    CachedTransforms[i] = Trans;
    Cache.Time = InstanceTime;
  };

  if (bUseParallel) {
    // Multi-threaded path - spread across all cores
    ParallelFor(Count, ProcessInstance);
  } else {
    // Single-threaded path for editor or small counts
    for (int32 i = 0; i < Count; i++) {
      ProcessInstance(i);
    }
  }

  // Apply to rendering
  if (SourceSkeletalMesh) {
    // Get camera position for Auto mode distance check
    FVector CameraLocation = FVector::ZeroVector;
    if (SkeletalMode == EKClonerSkeletalMode::Auto && GetWorld()) {
      APlayerController *PC = GetWorld()->GetFirstPlayerController();
      if (PC && PC->PlayerCameraManager) {
        CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
      }
    }

    for (int32 i = 0; i < Count; i++) {
      if (SkeletalMeshPool.IsValidIndex(i) && SkeletalMeshPool[i]) {
        // Determine if this instance should use Physics/IK or VAT
        bool bUsePhysicsIK = true;

        if (SkeletalMode == EKClonerSkeletalMode::VATBaked) {
          bUsePhysicsIK = false;
        } else if (SkeletalMode == EKClonerSkeletalMode::Auto) {
          FVector InstanceWorldPos = ActorTransform.TransformPosition(
              CachedTransforms[i].GetLocation());
          float DistToCamera = FVector::Dist(InstanceWorldPos, CameraLocation);
          bUsePhysicsIK = (DistToCamera < SkeletalModeDistanceThreshold);
        }

        // Toggle skeletal component visibility based on mode
        SkeletalMeshPool[i]->SetVisibility(bUsePhysicsIK);

        if (bUsePhysicsIK) {
          SkeletalMeshPool[i]->SetRelativeTransform(CachedTransforms[i]);

          // Only override animation position if a modifier changed the time
          // (e.g., Delay modifier) Otherwise, let the animation play naturally
          if (SourceAnimSequence && SkeletalMeshPool[i]->GetAnimationMode() ==
                                        EAnimationMode::AnimationSingleNode) {
            float InstanceTime = CachedInstanceData[i].Time;
            float ExpectedTime = bUseOverrideTime ? OverrideTime : WorldTime;

            // Check if modifier changed the time (with small epsilon for
            // floating point)
            bool bModifierChangedTime =
                FMath::Abs(InstanceTime - ExpectedTime) > 0.001f;

            if (bModifierChangedTime) {
              // Delay/time-offset modifier is active - set explicit position
              SkeletalMeshPool[i]->SetPosition(InstanceTime);
            }
            // else: Let animation play naturally without setting explicit
            // position
          }
        }
      }
    }
  } else {
    // Batch transform update
    InstancedMesh->BatchUpdateInstancesTransforms(0, CachedTransforms, false,
                                                  true);

    // Batch custom data update
    for (int32 i = 0; i < Count; i++) {
      FKClonerInstanceCache &Cache = CachedInstanceData[i];
      InstancedMesh->SetCustomDataValue(i, 0, Cache.R, false);
      InstancedMesh->SetCustomDataValue(i, 1, Cache.G, false);
      InstancedMesh->SetCustomDataValue(i, 2, Cache.B, i == (Count - 1));
    }

    InstancedMesh->MarkRenderStateDirty();
  }

  // --- VAT HISM DUAL-RENDER (for VATBaked and Auto modes) ---
  if (SourceSkeletalMesh && VATInstancedMesh &&
      SkeletalMode != EKClonerSkeletalMode::PhysicsIK) {
    // Ensure VAT material instance exists
    if (!VATMaterialInstance && VATBaseMaterial) {
      VATMaterialInstance =
          UMaterialInstanceDynamic::Create(VATBaseMaterial, this);
      if (VATMaterialInstance) {
        if (VATPositionTexture) {
          VATMaterialInstance->SetTextureParameterValue(
              FName("VATPositionTexture"), VATPositionTexture);
        }
        if (VATRotationTexture) {
          VATMaterialInstance->SetTextureParameterValue(
              FName("VATRotationTexture"), VATRotationTexture);
        }
        VATMaterialInstance->SetScalarParameterValue(
            FName("VATHasRotation"), VATRotationTexture ? 1.0f : 0.0f);
        if (VATPositionTexture) {
          float W = (float)VATPositionTexture->GetSizeX();
          float H = (float)VATPositionTexture->GetSizeY();
          float RPF = (InstancedMesh && InstancedMesh->GetInstanceCount() > 0)
                          ? FMath::CeilToFloat(
                                (float)InstancedMesh->GetInstanceCount() / W)
                          : 1.0f;
          VATMaterialInstance->SetScalarParameterValue(FName("VATTexWidth"), W);
          VATMaterialInstance->SetScalarParameterValue(FName("VATTexHeight"),
                                                       H);
          VATMaterialInstance->SetScalarParameterValue(FName("VATRowsPerFrame"),
                                                       RPF);
        }
      }
    }

    // Get static mesh for VAT HISM (use SourceMesh or create from skeletal)
    UStaticMesh *VATMesh = SourceMesh;
    if (!VATMesh) {
      // Fallback to default cube if no static mesh available
      VATMesh = LoadObject<UStaticMesh>(nullptr,
                                        TEXT("/Engine/BasicShapes/Cube.Cube"));
    }

    // Setup VAT HISM if not already done
    if (VATInstancedMesh->GetStaticMesh() != VATMesh) {
      VATInstancedMesh->SetStaticMesh(VATMesh);
      if (VATMaterialInstance) {
        VATInstancedMesh->SetMaterial(0, VATMaterialInstance);
      }
      VATInstancedMesh->NumCustomDataFloats =
          4; // Time, Index, TotalCount, Reserved
    }

    // Collect far clone transforms
    TArray<FTransform> VATTransforms;
    TArray<int32> VATIndices;

    FVector CameraLocation = FVector::ZeroVector;
    if (GetWorld()) {
      APlayerController *PC = GetWorld()->GetFirstPlayerController();
      if (PC && PC->PlayerCameraManager) {
        CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
      }
    }

    for (int32 i = 0; i < Count; i++) {
      bool bShouldUseVAT = false;

      if (SkeletalMode == EKClonerSkeletalMode::VATBaked) {
        bShouldUseVAT = true;
      } else if (SkeletalMode == EKClonerSkeletalMode::Auto) {
        FVector InstanceWorldPos =
            ActorTransform.TransformPosition(CachedTransforms[i].GetLocation());
        float DistToCamera = FVector::Dist(InstanceWorldPos, CameraLocation);
        bShouldUseVAT = (DistToCamera >= SkeletalModeDistanceThreshold);
      }

      if (bShouldUseVAT) {
        VATTransforms.Add(CachedTransforms[i]);
        VATIndices.Add(i);
      }
    }

    // Update VAT HISM instances
    int32 VATCount = VATTransforms.Num();
    int32 CurrentVATCount = VATInstancedMesh->GetInstanceCount();

    // Resize if needed
    if (CurrentVATCount != VATCount) {
      VATInstancedMesh->ClearInstances();
      for (int32 i = 0; i < VATCount; i++) {
        VATInstancedMesh->AddInstance(VATTransforms[i], true);
      }
    } else if (VATCount > 0) {
      VATInstancedMesh->BatchUpdateInstancesTransforms(0, VATTransforms, true,
                                                       true);
    }

    // Set per-instance custom data (time for VAT shader)
    for (int32 i = 0; i < VATCount; i++) {
      int32 OriginalIndex = VATIndices[i];
      float InstanceTime = CachedInstanceData[OriginalIndex].Time;

      VATInstancedMesh->SetCustomDataValue(i, 0, InstanceTime, false); // Time
      VATInstancedMesh->SetCustomDataValue(i, 1, (float)OriginalIndex,
                                           false); // Index
      VATInstancedMesh->SetCustomDataValue(i, 2, (float)Count,
                                           false); // Total count
      VATInstancedMesh->SetCustomDataValue(i, 3, 0.0f,
                                           i == (VATCount - 1)); // Reserved
    }

    VATInstancedMesh->SetVisibility(VATCount > 0);
    if (VATCount > 0) {
      VATInstancedMesh->MarkRenderStateDirty();
    }
  }

  bModifiersDirty = false;

  // Broadcast update event for Blueprint listeners
  OnClonerUpdated.Broadcast(DeltaTime);
}

// =============================================================================
// BLUEPRINT QUERY FUNCTIONS
// =============================================================================

FTransform AKClonerActor::GetCloneTransform(int32 Index) const {
  if (CachedTransforms.IsValidIndex(Index)) {
    return GetActorTransform() * CachedTransforms[Index];
  }
  return FTransform::Identity;
}

FVector AKClonerActor::GetCloneLocation(int32 Index) const {
  if (CachedTransforms.IsValidIndex(Index)) {
    return GetActorTransform().TransformPosition(
        CachedTransforms[Index].GetLocation());
  }
  return GetActorLocation();
}

int32 AKClonerActor::GetNearestCloneIndex(FVector WorldLocation) const {
  if (CachedTransforms.Num() == 0) {
    return INDEX_NONE;
  }

  FTransform ActorTransform = GetActorTransform();
  int32 NearestIndex = 0;
  float NearestDistSq = TNumericLimits<float>::Max();

  for (int32 i = 0; i < CachedTransforms.Num(); i++) {
    FVector CloneWorld =
        ActorTransform.TransformPosition(CachedTransforms[i].GetLocation());
    float DistSq = FVector::DistSquared(CloneWorld, WorldLocation);
    if (DistSq < NearestDistSq) {
      NearestDistSq = DistSq;
      NearestIndex = i;
    }
  }

  return NearestIndex;
}

TArray<FTransform> AKClonerActor::GetAllCloneTransforms() const {
  TArray<FTransform> WorldTransforms;
  WorldTransforms.Reserve(CachedTransforms.Num());
  FTransform ActorTransform = GetActorTransform();

  for (const FTransform &LocalTrans : CachedTransforms) {
    WorldTransforms.Add(ActorTransform * LocalTrans);
  }

  return WorldTransforms;
}

bool AKClonerActor::IsAnimating() const {
  if (Modifiers.Num() == 0) {
    return false;
  }

  for (const UKClonerModifier *Mod : Modifiers) {
    if (Mod && Mod->bEnabled && Mod->Influence > 0.0f) {
      return true;
    }
  }

  return false;
}

float AKClonerActor::GetEffectorInfluenceAtLocation(
    const FVector &WorldLocation) const {
  return GetEffectorInfluence(WorldLocation);
}

// =============================================================================
// MODIFIER MANAGEMENT
// =============================================================================

UKClonerModifier *AKClonerActor::GetModifierByIndex(int32 Index) const {
  if (Modifiers.IsValidIndex(Index)) {
    return Modifiers[Index];
  }
  return nullptr;
}

UKClonerModifier *
AKClonerActor::AddModifierOfClass(TSubclassOf<UKClonerModifier> ModifierClass) {
  if (!ModifierClass) {
    return nullptr;
  }

  UKClonerModifier *NewModifier =
      NewObject<UKClonerModifier>(this, ModifierClass);
  if (NewModifier) {
    Modifiers.Add(NewModifier);
    bModifiersDirty = true;
  }

  return NewModifier;
}

bool AKClonerActor::RemoveModifier(UKClonerModifier *Modifier) {
  if (!Modifier) {
    return false;
  }

  int32 RemovedCount = Modifiers.Remove(Modifier);
  if (RemovedCount > 0) {
    bModifiersDirty = true;
    return true;
  }

  return false;
}

void AKClonerActor::ClearAllModifiers() {
  Modifiers.Empty();
  bModifiersDirty = true;
}

void AKClonerActor::SetModifierEnabled(int32 Index, bool bEnabled) {
  if (Modifiers.IsValidIndex(Index) && Modifiers[Index]) {
    Modifiers[Index]->bEnabled = bEnabled;
    bModifiersDirty = true;
  }
}

// =============================================================================
// CLONE VISIBILITY/CONTROL
// =============================================================================

void AKClonerActor::SetCloneVisible(int32 Index, bool bVisible) {
  if (CachedInstanceData.IsValidIndex(Index)) {
    CachedInstanceData[Index].bVisible = bVisible;

    // Apply immediately if using static mesh instances
    if (!SourceSkeletalMesh && InstancedMesh) {
      // HISM doesn't have direct visibility per instance, so we scale to 0
      if (!bVisible) {
        FTransform ZeroScale = CachedTransforms[Index];
        ZeroScale.SetScale3D(FVector::ZeroVector);
        InstancedMesh->UpdateInstanceTransform(Index, ZeroScale, true, true);
      } else {
        InstancedMesh->UpdateInstanceTransform(Index, CachedTransforms[Index],
                                               true, true);
      }
    }

    // For skeletal mesh, hide the component
    if (SkeletalMeshPool.IsValidIndex(Index) && SkeletalMeshPool[Index]) {
      SkeletalMeshPool[Index]->SetVisibility(bVisible);
    }
  }
}

void AKClonerActor::HideAllClones() {
  for (int32 i = 0; i < CachedInstanceData.Num(); i++) {
    SetCloneVisible(i, false);
  }
}

void AKClonerActor::ShowAllClones() {
  for (int32 i = 0; i < CachedInstanceData.Num(); i++) {
    SetCloneVisible(i, true);
  }
  bModifiersDirty = true; // Force update to restore transforms
}

// =============================================================================
// NIAGARA VFX
// =============================================================================

void AKClonerActor::UpdateVFXComponent() {
  // If no Niagara emitter assigned, destroy existing component
  if (!NiagaraEmitter) {
    if (VFXComponent) {
      VFXComponent->DeactivateImmediate();
      VFXComponent->DestroyComponent();
      VFXComponent = nullptr;
    }
    return;
  }

  // Create the Niagara component if it doesn't exist
  if (!VFXComponent) {
    VFXComponent = NewObject<UNiagaraComponent>(
        this, UNiagaraComponent::StaticClass(), TEXT("VFXComponent"));
    VFXComponent->SetupAttachment(RootComponent);
    VFXComponent->RegisterComponent();
    VFXComponent->SetAutoDestroy(false);
  }

  // Set the asset
  VFXComponent->SetAsset(NiagaraEmitter);

  // Set scale parameter if the system supports it
  VFXComponent->SetFloatParameter(FName("VFXScale"), VFXScale);

  // Try to bind the K-Cloner Data Interface if the system has one
  // The Niagara system should have a User Parameter named "KClonerSource"
  // pointing to a UKClonerDataInterface
  UKClonerDataInterface* DIObject =
      NewObject<UKClonerDataInterface>(VFXComponent);
  if (DIObject) {
    DIObject->ClonerActor = this;
    VFXComponent->SetVariableObject(FName("KClonerSource"), DIObject);
  }

  // Activate the component
  VFXComponent->Activate(true);

  UE_LOG(LogTemp, Log,
         TEXT("K-Cloner: VFX Component updated with %s, Scale: %.2f"),
         *NiagaraEmitter->GetName(), VFXScale);
}
