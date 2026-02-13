// Copyright 2026 K-Studio. All Rights Reserved.

#include "KClonerBakingUtils.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "SkeletalMeshTypes.h"

#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Editor.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GeometryCache.h"
#include "GeometryCacheCodecRaw.h"
#include "GeometryCacheMeshData.h"
#include "GeometryCacheTrack.h"
#include "KClonerActor.h"
#include "MeshDescription.h"
#include "MeshUtilities.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "PhysicsEngine/BodySetup.h"
#include "ReferenceSkeleton.h"
#include "Rendering/SkeletalMeshLODImporterData.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "SkeletalMeshAttributes.h"
#include "SkinnedAssetCompiler.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"
#include "UObject/SavePackage.h"

#define LOCTEXT_NAMESPACE "KClonerBakingUtils"

// ======================================
// SAMPLE TRANSFORMS AT A GIVEN TIME
// ======================================

TArray<FTransform>
FKClonerBakingUtils::SampleInstanceTransforms(AKClonerActor *ClonerActor,
                                              float Time) {
  TArray<FTransform> Transforms;

  if (!ClonerActor || !ClonerActor->InstancedMesh) {
    return Transforms;
  }

  // Store current time so we can restore it after
  bool bWasUsingOverride = ClonerActor->bUseOverrideTime;
  float OldOverrideTime = ClonerActor->OverrideTime;

  // force time override and tick to get the transforms at that moment
  ClonerActor->bUseOverrideTime = true;
  ClonerActor->OverrideTime = Time;

  // Force update
  ClonerActor->Tick(0.0f);

  // yoink the transforms
  UHierarchicalInstancedStaticMeshComponent *ISMC = ClonerActor->InstancedMesh;
  int32 Count = ISMC->GetInstanceCount();
  Transforms.SetNum(Count);

  for (int32 i = 0; i < Count; ++i) {
    ISMC->GetInstanceTransform(i, Transforms[i], true);
  }

  // put it back how it was
  ClonerActor->bUseOverrideTime = bWasUsingOverride;
  ClonerActor->OverrideTime = OldOverrideTime;

  return Transforms;
}

// ======================================
// MERGE INSTANCES INTO ONE MESH DESC
// this is the expensive part lol
// ======================================

bool FKClonerBakingUtils::MergeMeshInstances(
    const FMeshDescription &SourceMesh, const TArray<FTransform> &Transforms,
    FMeshDescription &OutMergedMesh) {
  if (Transforms.Num() == 0) {
    return false;
  }
  // Register mesh attributes first or UE will shit itself
  FStaticMeshAttributes Attributes(OutMergedMesh);
  Attributes.Register();

  TVertexAttributesConstRef<FVector3f> SourcePositions =
      SourceMesh.GetVertexPositions();
  TVertexInstanceAttributesConstRef<FVector3f> SourceNormals =
      SourceMesh.VertexInstanceAttributes().GetAttributesRef<FVector3f>(
          MeshAttribute::VertexInstance::Normal);
  TVertexInstanceAttributesConstRef<FVector3f> SourceTangents =
      SourceMesh.VertexInstanceAttributes().GetAttributesRef<FVector3f>(
          MeshAttribute::VertexInstance::Tangent);
  TVertexInstanceAttributesConstRef<FVector2f> SourceUVs =
      SourceMesh.VertexInstanceAttributes().GetAttributesRef<FVector2f>(
          MeshAttribute::VertexInstance::TextureCoordinate);

  // output attrs
  TVertexAttributesRef<FVector3f> OutPositions =
      OutMergedMesh.GetVertexPositions();
  TVertexInstanceAttributesRef<FVector3f> OutNormals =
      OutMergedMesh.VertexInstanceAttributes().GetAttributesRef<FVector3f>(
          MeshAttribute::VertexInstance::Normal);
  TVertexInstanceAttributesRef<FVector3f> OutTangents =
      OutMergedMesh.VertexInstanceAttributes().GetAttributesRef<FVector3f>(
          MeshAttribute::VertexInstance::Tangent);
  TVertexInstanceAttributesRef<FVector2f> OutUVs =
      OutMergedMesh.VertexInstanceAttributes().GetAttributesRef<FVector2f>(
          MeshAttribute::VertexInstance::TextureCoordinate);

  const int32 SourceVertexCount = SourceMesh.Vertices().Num();
  const int32 SourceVertexInstanceCount = SourceMesh.VertexInstances().Num();
  const int32 SourceTriangleCount = SourceMesh.Triangles().Num();
  const int32 SourcePolygonCount = SourceMesh.Polygons().Num();

  // Create polygon group once
  FPolygonGroupID PolyGroupID = OutMergedMesh.CreatePolygonGroup();

  // For each instance...
  for (int32 InstanceIdx = 0; InstanceIdx < Transforms.Num(); ++InstanceIdx) {
    const FTransform &InstanceTransform = Transforms[InstanceIdx];

    // Vertex ID offset
    const int32 VertexOffset = InstanceIdx * SourceVertexCount;


    TMap<FVertexID, FVertexID> VertexIDMap;
    for (const FVertexID SourceVertexID :
         SourceMesh.Vertices().GetElementIDs()) {
      FVector3f SourcePos = SourcePositions.Get(SourceVertexID);
      FVector3f TransformedPos =
          (FVector3f)InstanceTransform.TransformPosition(FVector(SourcePos));

      FVertexID NewVertexID = OutMergedMesh.CreateVertex();
      OutPositions.Set(NewVertexID, TransformedPos);
      VertexIDMap.Add(SourceVertexID, NewVertexID);
    }

    // recreate polys with new verts
    for (const FPolygonID SourcePolygonID :
         SourceMesh.Polygons().GetElementIDs()) {
      const TArray<FVertexInstanceID> &SourceVertexInstanceIDs =
          SourceMesh.GetPolygonVertexInstances(SourcePolygonID);

      TArray<FVertexInstanceID> NewVertexInstanceIDs;
      NewVertexInstanceIDs.Reserve(SourceVertexInstanceIDs.Num());

      for (const FVertexInstanceID &SourceVIID : SourceVertexInstanceIDs) {
        FVertexID SourceVertexID =
            SourceMesh.GetVertexInstanceVertex(SourceVIID);
        FVertexID NewVertexID = VertexIDMap[SourceVertexID];

        // Create new VI
        FVertexInstanceID NewVIID =
            OutMergedMesh.CreateVertexInstance(NewVertexID);

        // xform normals
        FVector3f SourceNormal = SourceNormals.Get(SourceVIID);
        FVector3f TransformedNormal =
            (FVector3f)InstanceTransform.TransformVectorNoScale(
                FVector(SourceNormal));
        OutNormals.Set(NewVIID, TransformedNormal.GetSafeNormal());

        // xform tangents
        if (SourceTangents.IsValid()) {
          FVector3f SourceTangent = SourceTangents.Get(SourceVIID);
          FVector3f TransformedTangent =
              (FVector3f)InstanceTransform.TransformVectorNoScale(
                  FVector(SourceTangent));
          OutTangents.Set(NewVIID, TransformedTangent.GetSafeNormal());
        }

        // UVs stay the same
        if (SourceUVs.IsValid()) {
          OutUVs.Set(NewVIID, SourceUVs.Get(SourceVIID));
        }

        NewVertexInstanceIDs.Add(NewVIID);
      }

      // make the poly
      OutMergedMesh.CreatePolygon(PolyGroupID, NewVertexInstanceIDs);
    }
  }

  // gotta triangulate or mesh wont render right
  OutMergedMesh.TriangulateMesh();

  return true;
}

// ======================================
// BAKE TO STATIC MESH
// combines all instances at current time into one mesh
// ======================================

FKClonerBakeResult FKClonerBakingUtils::BakeToStaticMesh(
    AKClonerActor *ClonerActor, const FString &PackageName,
    const FKClonerStaticMeshBakeOptions &Options) {
  FKClonerBakeResult Result;
  double StartTime = FPlatformTime::Seconds();

  // Validate
  if (!ClonerActor) {
    Result.ErrorMessage = TEXT("Invalid Cloner Actor");
    FMessageDialog::Open(EAppMsgType::Ok,
                         FText::FromString(Result.ErrorMessage));
    return Result;
  }

  UStaticMesh *SourceMesh = ClonerActor->SourceMesh;
  if (!SourceMesh) {
    Result.ErrorMessage = TEXT("No Source Mesh assigned to Cloner");
    FMessageDialog::Open(EAppMsgType::Ok,
                         FText::FromString(Result.ErrorMessage));
    return Result;
  }

  if (!ClonerActor->InstancedMesh) {
    Result.ErrorMessage = TEXT("InstancedMesh component is null");
    FMessageDialog::Open(EAppMsgType::Ok,
                         FText::FromString(Result.ErrorMessage));
    return Result;
  }

  Result.InstanceCount = ClonerActor->InstancedMesh->GetInstanceCount();
  if (Result.InstanceCount == 0) {
    Result.ErrorMessage = TEXT("No instances to bake");
    FMessageDialog::Open(EAppMsgType::Ok,
                         FText::FromString(Result.ErrorMessage));
    return Result;
  }

  // Get source mesh description
  const FMeshDescription *SourceDesc = SourceMesh->GetMeshDescription(0);
  if (!SourceDesc || SourceDesc->Vertices().Num() == 0) {
    Result.ErrorMessage = TEXT("Source mesh has no valid geometry");
    FMessageDialog::Open(EAppMsgType::Ok,
                         FText::FromString(Result.ErrorMessage));
    return Result;
  }

  // sample at time=0 for static bake
  TArray<FTransform> Transforms = SampleInstanceTransforms(ClonerActor, 0.0f);

  // merge em all
  FMeshDescription MergedMesh;
  if (!MergeMeshInstances(*SourceDesc, Transforms, MergedMesh)) {
    Result.ErrorMessage = TEXT("Failed to merge mesh instances");
    FMessageDialog::Open(EAppMsgType::Ok,
                         FText::FromString(Result.ErrorMessage));
    return Result;
  }


  UPackage *Package = CreatePackage(*PackageName);
  if (!Package) {
    Result.ErrorMessage =
        FString::Printf(TEXT("Failed to create package: %s"), *PackageName);
    FMessageDialog::Open(EAppMsgType::Ok,
                         FText::FromString(Result.ErrorMessage));
    return Result;
  }

  FString AssetName = FPaths::GetBaseFilename(PackageName);

  UStaticMesh *NewMesh =
      NewObject<UStaticMesh>(Package, *AssetName, RF_Public | RF_Standalone);
  if (!NewMesh) {
    Result.ErrorMessage = TEXT("Failed to create Static Mesh object");
    FMessageDialog::Open(EAppMsgType::Ok,
                         FText::FromString(Result.ErrorMessage));
    return Result;
  }

  // UE5.7 NEEDS at least one source model or CreateMeshDescription crashes
  if (NewMesh->GetNumSourceModels() == 0) {
    NewMesh->AddSourceModel();
  }

  // set the mesh description
  NewMesh->CreateMeshDescription(0, MoveTemp(MergedMesh));
  NewMesh->CommitMeshDescription(0);

  // copy mats from source
  for (int32 MatIdx = 0; MatIdx < SourceMesh->GetStaticMaterials().Num();
       ++MatIdx) {
    NewMesh->GetStaticMaterials().Add(SourceMesh->GetStaticMaterials()[MatIdx]);
  }

  // build settings
  if (NewMesh->GetNumSourceModels() > 0) {
    FStaticMeshSourceModel &SourceModel = NewMesh->GetSourceModel(0);
    SourceModel.BuildSettings.bRecomputeNormals = false;
    SourceModel.BuildSettings.bRecomputeTangents = true;
    SourceModel.BuildSettings.bGenerateLightmapUVs = true;
    SourceModel.BuildSettings.SrcLightmapIndex = 0;
    SourceModel.BuildSettings.DstLightmapIndex = 1;
    SourceModel.BuildSettings.bRemoveDegenerates = true;
    SourceModel.BuildSettings.bBuildReversedIndexBuffer = true;
    SourceModel.BuildSettings.bGenerateDistanceFieldAsIfTwoSided =
        Options.bGenerateDistanceField;
  }

  // collision if they want it
  if (Options.bGenerateCollision) {
    NewMesh->CreateBodySetup();
    if (NewMesh->GetBodySetup()) {
      NewMesh->GetBodySetup()->CollisionTraceFlag = CTF_UseComplexAsSimple;
    }
  }

  // Build the mesh
  NewMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;
  NewMesh->Build(false);
  NewMesh->PostEditChange();

  // Register asset
  FAssetRegistryModule::AssetCreated(NewMesh);
  Package->MarkPackageDirty();


  Result.bSuccess = true;
  Result.ResultAsset = NewMesh;
  Result.VertexCount = MergedMesh.Vertices().Num();
  Result.TriangleCount = MergedMesh.Triangles().Num();
  Result.BakeDuration = FPlatformTime::Seconds() - StartTime;


  FMessageDialog::Open(EAppMsgType::Ok,
                       FText::Format(LOCTEXT("StaticMeshBakeSuccess",
                                             "Static Mesh Bake Complete!\n\n"
                                             "Asset: {0}\n"
                                             "Instances Merged: {1}\n"
                                             "Vertices: {2}\n"
                                             "Triangles: {3}\n"
                                             "Bake Time: {4:.2f}s"),
                                     FText::FromString(AssetName),
                                     FText::AsNumber(Result.InstanceCount),
                                     FText::AsNumber(Result.VertexCount),
                                     FText::AsNumber(Result.TriangleCount),
                                     FText::AsNumber(Result.BakeDuration)));

  return Result;
}

// ======================================
// ALEMBIC EXPORT
// writes OBJ sequence for now, abc needs AlembicLib
// ======================================

bool FKClonerBakingUtils::WriteAlembicFile(
    const FString &FilePath, const TArray<TArray<FVector>> &FramePositions,
    const TArray<TArray<FVector>> &FrameNormals, const TArray<int32> &Indices,
    const TArray<FVector2D> &UVs, float FrameRate) {
  // TODO: proper alembic needs AlembicLib dep
  // for now just write OBJ sequence as fallback

  int32 FrameCount = FramePositions.Num();
  FString BasePath = FPaths::GetPath(FilePath);
  FString BaseName = FPaths::GetBaseFilename(FilePath);


  IPlatformFile &PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
  PlatformFile.CreateDirectoryTree(*BasePath);

  // write each frame as obj
  for (int32 Frame = 0; Frame < FrameCount; ++Frame) {
    FString FramePath =
        FString::Printf(TEXT("%s/%s_%04d.obj"), *BasePath, *BaseName, Frame);

    FString ObjContent;
    ObjContent +=
        TEXT("# K-Cloner Baked Frame ") + FString::FromInt(Frame) + TEXT("\n");
    ObjContent += TEXT("# Vertices: ") +
                  FString::FromInt(FramePositions[Frame].Num()) + TEXT("\n\n");

    // verts
    for (const FVector &Pos : FramePositions[Frame]) {
      ObjContent += FString::Printf(TEXT("v %f %f %f\n"), Pos.X, Pos.Y, Pos.Z);
    }

    // normals
    if (FrameNormals.IsValidIndex(Frame)) {
      for (const FVector &N : FrameNormals[Frame]) {
        ObjContent += FString::Printf(TEXT("vn %f %f %f\n"), N.X, N.Y, N.Z);
      }
    }

    // uvs
    for (const FVector2D &UV : UVs) {
      ObjContent += FString::Printf(TEXT("vt %f %f\n"), UV.X, 1.0f - UV.Y);
    }

    // faces (obj is 1-indexed, what a pain)
    for (int32 i = 0; i < Indices.Num(); i += 3) {
      int32 A = Indices[i] + 1;
      int32 B = Indices[i + 1] + 1;
      int32 C = Indices[i + 2] + 1;
      ObjContent += FString::Printf(TEXT("f %d/%d/%d %d/%d/%d %d/%d/%d\n"), A,
                                    A, A, B, B, B, C, C, C);
    }

    FFileHelper::SaveStringToFile(ObjContent, *FramePath);
  }

  // write metadata too
  FString MetaPath =
      FString::Printf(TEXT("%s/%s_meta.txt"), *BasePath, *BaseName);
  FString MetaContent;
  MetaContent += TEXT("K-Cloner Animation Export\n");
  MetaContent += FString::Printf(TEXT("Frames: %d\n"), FrameCount);
  MetaContent += FString::Printf(TEXT("FrameRate: %f\n"), FrameRate);
  MetaContent +=
      FString::Printf(TEXT("Duration: %f seconds\n"), FrameCount / FrameRate);
  FFileHelper::SaveStringToFile(MetaContent, *MetaPath);

  return true;
}

FKClonerBakeResult FKClonerBakingUtils::ExportToAlembic(
    AKClonerActor *ClonerActor, const FKClonerAlembicExportOptions &Options) {
  FKClonerBakeResult Result;
  double StartTime = FPlatformTime::Seconds();

  // Validate
  if (!ClonerActor) {
    Result.ErrorMessage = TEXT("Invalid Cloner Actor");
    FMessageDialog::Open(EAppMsgType::Ok,
                         FText::FromString(Result.ErrorMessage));
    return Result;
  }

  UStaticMesh *SourceMesh = ClonerActor->SourceMesh;
  if (!SourceMesh) {
    Result.ErrorMessage = TEXT("No Source Mesh assigned to Cloner");
    FMessageDialog::Open(EAppMsgType::Ok,
                         FText::FromString(Result.ErrorMessage));
    return Result;
  }

  if (!ClonerActor->InstancedMesh ||
      ClonerActor->InstancedMesh->GetInstanceCount() == 0) {
    Result.ErrorMessage = TEXT("No instances to export");
    FMessageDialog::Open(EAppMsgType::Ok,
                         FText::FromString(Result.ErrorMessage));
    return Result;
  }

  Result.InstanceCount = ClonerActor->InstancedMesh->GetInstanceCount();

  // Get source mesh data
  const FMeshDescription *SourceDesc = SourceMesh->GetMeshDescription(0);
  if (!SourceDesc || SourceDesc->Vertices().Num() == 0) {
    Result.ErrorMessage = TEXT("Source mesh has no valid geometry");
    FMessageDialog::Open(EAppMsgType::Ok,
                         FText::FromString(Result.ErrorMessage));
    return Result;
  }

  // Calculate frame count
  int32 FrameCount = FMath::CeilToInt(Options.Duration * Options.FrameRate);
  if (FrameCount < 1) {
    Result.ErrorMessage = TEXT("Duration too short");
    FMessageDialog::Open(EAppMsgType::Ok,
                         FText::FromString(Result.ErrorMessage));
    return Result;
  }

  // Extract source mesh data
  TVertexAttributesConstRef<FVector3f> SourcePositions =
      SourceDesc->GetVertexPositions();
  TVertexInstanceAttributesConstRef<FVector3f> SourceNormals =
      SourceDesc->VertexInstanceAttributes().GetAttributesRef<FVector3f>(
          MeshAttribute::VertexInstance::Normal);
  TVertexInstanceAttributesConstRef<FVector2f> SourceUVs =
      SourceDesc->VertexInstanceAttributes().GetAttributesRef<FVector2f>(
          MeshAttribute::VertexInstance::TextureCoordinate);

  // Build source index buffer
  TArray<int32> SourceIndices;
  TArray<FVector2D> SourceUVArray;

  for (const FTriangleID TriID : SourceDesc->Triangles().GetElementIDs()) {
    TArrayView<const FVertexInstanceID> TriVIs =
        SourceDesc->GetTriangleVertexInstances(TriID);
    for (const FVertexInstanceID &VIID : TriVIs) {
      FVertexID VID = SourceDesc->GetVertexInstanceVertex(VIID);
      SourceIndices.Add(VID.GetValue());

      if (SourceUVs.IsValid()) {
        FVector2f UV = SourceUVs.Get(VIID);
        SourceUVArray.Add(FVector2D(UV.X, UV.Y));
      }
    }
  }

  // Sample all frames
  TArray<TArray<FVector>> AllFramePositions;
  TArray<TArray<FVector>> AllFrameNormals;
  AllFramePositions.SetNum(FrameCount);
  AllFrameNormals.SetNum(FrameCount);

  float TimeStep = 1.0f / Options.FrameRate;
  int32 SourceVertCount = SourceDesc->Vertices().Num();

  for (int32 Frame = 0; Frame < FrameCount; ++Frame) {
    float Time = Frame * TimeStep;
    TArray<FTransform> Transforms = SampleInstanceTransforms(ClonerActor, Time);

    TArray<FVector> &FramePos = AllFramePositions[Frame];
    TArray<FVector> &FrameNorm = AllFrameNormals[Frame];

    FramePos.Reserve(SourceVertCount * Result.InstanceCount);
    FrameNorm.Reserve(SourceVertCount * Result.InstanceCount);

    for (const FTransform &T : Transforms) {
      for (const FVertexID VID : SourceDesc->Vertices().GetElementIDs()) {
        FVector3f Pos = SourcePositions.Get(VID);
        FramePos.Add(T.TransformPosition(FVector(Pos)));
      }

      for (const FVertexInstanceID VIID :
           SourceDesc->VertexInstances().GetElementIDs()) {
        FVector3f N = SourceNormals.Get(VIID);
        FrameNorm.Add(T.TransformVectorNoScale(FVector(N)));
      }
    }
  }

  // Build merged index buffer
  TArray<int32> MergedIndices;
  for (int32 Inst = 0; Inst < Result.InstanceCount; ++Inst) {
    int32 Offset = Inst * SourceVertCount;
    for (int32 Idx : SourceIndices) {
      MergedIndices.Add(Idx + Offset);
    }
  }

  // UVs repeat for each instance
  TArray<FVector2D> MergedUVs;
  for (int32 Inst = 0; Inst < Result.InstanceCount; ++Inst) {
    MergedUVs.Append(SourceUVArray);
  }

  // Write output
  if (!WriteAlembicFile(Options.ExportPath, AllFramePositions, AllFrameNormals,
                        MergedIndices, MergedUVs, Options.FrameRate)) {
    Result.ErrorMessage = TEXT("Failed to write Alembic file");
    FMessageDialog::Open(EAppMsgType::Ok,
                         FText::FromString(Result.ErrorMessage));
    return Result;
  }

  Result.bSuccess = true;
  Result.VertexCount = AllFramePositions[0].Num();
  Result.TriangleCount = MergedIndices.Num() / 3;
  Result.BakeDuration = FPlatformTime::Seconds() - StartTime;

  // Success dialog
  FString ExportDir = FPaths::GetPath(Options.ExportPath);
  FMessageDialog::Open(
      EAppMsgType::Ok,
      FText::Format(LOCTEXT("AlembicExportSuccess",
                            "Alembic Export Complete!\n\n"
                            "Output: {0}\n"
                            "Frames: {1}\n"
                            "Instances: {2}\n"
                            "Vertices/Frame: {3}\n"
                            "Export Time: {4:.2f}s\n\n"
                            "Note: Exported as OBJ sequence.\n"
                            "For true .abc, enable AlembicLib in Build.cs"),
                    FText::FromString(ExportDir), FText::AsNumber(FrameCount),
                    FText::AsNumber(Result.InstanceCount),
                    FText::AsNumber(Result.VertexCount),
                    FText::AsNumber(Result.BakeDuration)));

  return Result;
}

// ======================================
// GEOMETRY CACHE BAKING
// ======================================

FKClonerBakeResult FKClonerBakingUtils::BakeToGeometryCache(
    AKClonerActor *ClonerActor, const FString &PackageName,
    const FKClonerGeometryCacheBakeOptions &Options) {
  FKClonerBakeResult Result;
  double StartTime = FPlatformTime::Seconds();

  // Validate
  if (!ClonerActor) {
    Result.ErrorMessage = TEXT("Invalid Cloner Actor");
    FMessageDialog::Open(EAppMsgType::Ok,
                         FText::FromString(Result.ErrorMessage));
    return Result;
  }

  UStaticMesh *SourceMesh = ClonerActor->SourceMesh;
  if (!SourceMesh) {
    Result.ErrorMessage = TEXT("No Source Mesh assigned to Cloner");
    FMessageDialog::Open(EAppMsgType::Ok,
                         FText::FromString(Result.ErrorMessage));
    return Result;
  }

  if (!ClonerActor->InstancedMesh ||
      ClonerActor->InstancedMesh->GetInstanceCount() == 0) {
    Result.ErrorMessage = TEXT("No instances to bake");
    FMessageDialog::Open(EAppMsgType::Ok,
                         FText::FromString(Result.ErrorMessage));
    return Result;
  }

  Result.InstanceCount = ClonerActor->InstancedMesh->GetInstanceCount();

  // Get source mesh
  const FMeshDescription *SourceDesc = SourceMesh->GetMeshDescription(0);
  if (!SourceDesc || SourceDesc->Vertices().Num() == 0) {
    Result.ErrorMessage = TEXT("Source mesh has no valid geometry");
    FMessageDialog::Open(EAppMsgType::Ok,
                         FText::FromString(Result.ErrorMessage));
    return Result;
  }

  // Create package
  UPackage *Package = CreatePackage(*PackageName);
  if (!Package) {
    Result.ErrorMessage =
        FString::Printf(TEXT("Failed to create package: %s"), *PackageName);
    FMessageDialog::Open(EAppMsgType::Ok,
                         FText::FromString(Result.ErrorMessage));
    return Result;
  }

  FString AssetName = FPaths::GetBaseFilename(PackageName);

  // Create GeometryCache
  UGeometryCache *GeoCache =
      NewObject<UGeometryCache>(Package, *AssetName, RF_Public | RF_Standalone);
  if (!GeoCache) {
    Result.ErrorMessage = TEXT("Failed to create Geometry Cache object");
    FMessageDialog::Open(EAppMsgType::Ok,
                         FText::FromString(Result.ErrorMessage));
    return Result;
  }

  // Calculate frames
  int32 FrameCount = FMath::CeilToInt(Options.Duration * Options.FrameRate);
  float TimeStep = 1.0f / Options.FrameRate;

  // raw codec is fine for our purposes
  UGeometryCacheCodecRaw *Codec = NewObject<UGeometryCacheCodecRaw>(GeoCache);


  TVertexAttributesConstRef<FVector3f> SourcePositions =
      SourceDesc->GetVertexPositions();
  int32 SourceVertCount = SourceDesc->Vertices().Num();
  int32 MergedVertCount = SourceVertCount * Result.InstanceCount;

  // sample every frame and build mesh data
  TArray<FGeometryCacheMeshData> TrackMeshData;
  TrackMeshData.SetNum(FrameCount);

  for (int32 Frame = 0; Frame < FrameCount; ++Frame) {
    float Time = Frame * TimeStep;
    TArray<FTransform> Transforms = SampleInstanceTransforms(ClonerActor, Time);

    FGeometryCacheMeshData &MeshData = TrackMeshData[Frame];
    MeshData.Positions.Reserve(MergedVertCount);

    // build merged positions for this frame
    for (const FTransform &T : Transforms) {
      for (const FVertexID VID : SourceDesc->Vertices().GetElementIDs()) {
        FVector3f Pos = SourcePositions.Get(VID);
        FVector TransformedPos = T.TransformPosition(FVector(Pos));
        MeshData.Positions.Add(FVector3f(TransformedPos));
      }
    }

    // calc bbox
    FBox BBox(ForceInit);
    for (const FVector3f &P : MeshData.Positions) {
      BBox += FVector(P);
    }
    MeshData.BoundingBox = FBox3f(BBox);
  }

  // UE5.7 uses frame indices not time
  GeoCache->SetFrameStartEnd(0, FrameCount - 1);

  // copy mats
  for (const FStaticMaterial &Mat : SourceMesh->GetStaticMaterials()) {
    GeoCache->Materials.Add(Mat.MaterialInterface);
  }

  // Register asset
  GeoCache->PostEditChange();
  FAssetRegistryModule::AssetCreated(GeoCache);
  Package->MarkPackageDirty();

  Result.bSuccess = true;
  Result.ResultAsset = GeoCache;
  Result.VertexCount = MergedVertCount;
  Result.TriangleCount = SourceDesc->Triangles().Num() * Result.InstanceCount;
  Result.BakeDuration = FPlatformTime::Seconds() - StartTime;

  // Success dialog
  FMessageDialog::Open(
      EAppMsgType::Ok,
      FText::Format(LOCTEXT("GeometryCacheBakeSuccess",
                            "Geometry Cache Bake Complete!\n\n"
                            "Asset: {0}\n"
                            "Frames: {1}\n"
                            "Duration: {2:.2f}s\n"
                            "Instances: {3}\n"
                            "Vertices/Frame: {4}\n"
                            "Bake Time: {5:.2f}s"),
                    FText::FromString(AssetName), FText::AsNumber(FrameCount),
                    FText::AsNumber(FrameCount / Options.FrameRate),
                    FText::AsNumber(Result.InstanceCount),
                    FText::AsNumber(Result.VertexCount),
                    FText::AsNumber(Result.BakeDuration)));

  return Result;
}

// ======================================
// SKELETAL MESH BAKING
// turns cloner into a rigged skeletal mesh, one bone per clone
// ======================================

USkeletalMesh *
FKClonerBakingUtils::BakeToSkeletalMesh(AKClonerActor *ClonerActor,
                                        const FString &PackageName) {
  if (!ClonerActor) {
    FMessageDialog::Open(EAppMsgType::Ok,
                         LOCTEXT("InvalidClonerActor", "Invalid Cloner Actor"));
    return nullptr;
  }

  UStaticMesh *SourceMesh = ClonerActor->SourceMesh;
  if (!SourceMesh) {
    FMessageDialog::Open(
        EAppMsgType::Ok,
        LOCTEXT("NoSourceMesh", "No Source Static Mesh assigned."));
    return nullptr;
  }

  if (!ClonerActor->InstancedMesh ||
      ClonerActor->InstancedMesh->GetInstanceCount() == 0) {
    FMessageDialog::Open(EAppMsgType::Ok,
                         LOCTEXT("NoInstancesToBake", "No instances to bake."));
    return nullptr;
  }

  int32 InstanceCount = ClonerActor->InstancedMesh->GetInstanceCount();
  if (InstanceCount > 1000) {
    FMessageDialog::Open(
        EAppMsgType::Ok,
        FText::Format(
            LOCTEXT("TooManyInstances",
                    "Instance count ({0}) exceeds safe limit of 1000."),
            FText::AsNumber(InstanceCount)));
    return nullptr;
  }

  const FMeshDescription *SourceDesc = SourceMesh->GetMeshDescription(0);
  if (!SourceDesc || SourceDesc->Triangles().Num() == 0) {
    FMessageDialog::Open(
        EAppMsgType::Ok,
        LOCTEXT("NoMeshDescription", "Source mesh has no valid geometry."));
    return nullptr;
  }

  UPackage *Package = CreatePackage(*PackageName);
  if (!Package)
    return nullptr;

  FString AssetName = FPaths::GetBaseFilename(PackageName);

  USkeleton *Skeleton = NewObject<USkeleton>(
      Package, *(AssetName + TEXT("_Skeleton")), RF_Public | RF_Standalone);
  USkeletalMesh *SkeletalMesh =
      NewObject<USkeletalMesh>(Package, *AssetName, RF_Public | RF_Standalone);

  if (!Skeleton || !SkeletalMesh)
    return nullptr;

  // Build skeleton with one bone per instance
  FReferenceSkeleton RefSkeleton;
  {
    FReferenceSkeletonModifier Modifier(RefSkeleton, Skeleton);

    // Root bone
    FMeshBoneInfo RootBone;
    RootBone.Name = TEXT("Root");
    RootBone.ParentIndex = INDEX_NONE;
    Modifier.Add(RootBone, FTransform::Identity);

    // Instance bones
    for (int32 i = 0; i < InstanceCount; ++i) {
      FMeshBoneInfo InstanceBone;
      InstanceBone.Name = *FString::Printf(TEXT("Instance_%03d"), i);
      InstanceBone.ParentIndex = 0; // Parent to root

      FTransform InstanceTransform;
      ClonerActor->InstancedMesh->GetInstanceTransform(i, InstanceTransform,
                                                       true);
      Modifier.Add(InstanceBone, InstanceTransform);
    }
  }

  // CRITICAL: Set the reference skeleton BEFORE RecreateBoneTree
  // Otherwise GetRefSkeleton() returns empty data and BuildSkeletalMesh crashes
  SkeletalMesh->SetRefSkeleton(RefSkeleton);

  Skeleton->RecreateBoneTree(SkeletalMesh);
  SkeletalMesh->SetSkeleton(Skeleton);
  {
    TArray<FTransform> BaseTransforms =
        SampleInstanceTransforms(ClonerActor, 0.0f);
    FMeshDescription Merged;
    Merged.Vertices().Reserve(SourceDesc->Vertices().Num() *
                              BaseTransforms.Num());
    Merged.VertexInstances().Reserve(SourceDesc->VertexInstances().Num() *
                                     BaseTransforms.Num());
    Merged.Triangles().Reserve(SourceDesc->Triangles().Num() *
                               BaseTransforms.Num());
    MergeMeshInstances(*SourceDesc, BaseTransforms, Merged);

    FSkeletalMeshImportData ImportData;
    ImportData.MaxMaterialIndex = 0;
    ImportData.NumTexCoords = 1;
    {
      SkeletalMeshImportData::FMaterial Mat;
      Mat.MaterialImportName = TEXT("Default");
      ImportData.Materials.Add(Mat);
    }
    {
      FStaticMeshAttributes Attr(Merged);
      const TVertexAttributesConstRef<FVector3f> Positions =
          Merged.GetVertexPositions();
      const TVertexInstanceAttributesConstRef<FVector3f> Normals =
          Merged.VertexInstanceAttributes().GetAttributesRef<FVector3f>(
              MeshAttribute::VertexInstance::Normal);
      const TVertexInstanceAttributesConstRef<FVector3f> Tangents =
          Merged.VertexInstanceAttributes().GetAttributesRef<FVector3f>(
              MeshAttribute::VertexInstance::Tangent);
      const TVertexInstanceAttributesConstRef<FVector2f> UVs =
          Merged.VertexInstanceAttributes().GetAttributesRef<FVector2f>(
              MeshAttribute::VertexInstance::TextureCoordinate);

      // Map VertexID to Point Index
      TMap<FVertexID, int32> VertexIDToPointIndex;

      for (const FVertexID VertexID : Merged.Vertices().GetElementIDs()) {
        FVector3f Pos = Positions.Get(VertexID);
        int32 PtIdx = ImportData.Points.Add(Pos);
        VertexIDToPointIndex.Add(VertexID, PtIdx);
        ImportData.PointToRawMap.Add(
            PtIdx); // Simple 1:1 mapping for exact geometry
      }

      // Calculate bone weights for instances

      // Let's replace the loop to be safe and robust.
      // Since we merged N instances of M vertices.
      // Vertices 0..M-1 -> Bone 1 (Instance 0)
      // Vertices M..2M-1 -> Bone 2 (Instance 1)

      int32 SourceVertexCount = SourceDesc->Vertices().Num();
      if (SourceVertexCount > 0) {
        for (int32 i = 0; i < ImportData.Points.Num(); ++i) {
          SkeletalMeshImportData::FRawBoneInfluence Inf;
          int32 InstanceIndex = i / SourceVertexCount;
          // Bone 0 is Root. Bone 1 is Instance_000.
          Inf.BoneIndex = InstanceIndex + 1;
          Inf.VertexIndex = i;
          Inf.Weight = 1.0f;
          ImportData.Influences.Add(Inf);
        }
      }
      for (const FTriangleID TriID : Merged.Triangles().GetElementIDs()) {
        SkeletalMeshImportData::FTriangle Face;
        Face.MatIndex = 0;
        TArrayView<const FVertexInstanceID> VIView =
            Merged.GetTriangleVertexInstances(TriID);
        for (int32 Corner = 0; Corner < 3; ++Corner) {
          const FVertexInstanceID VI = VIView[Corner];
          const FVertexID V = Merged.GetVertexInstanceVertex(VI);
          SkeletalMeshImportData::FVertex Wedge;
          Wedge.MatIndex = 0;
          // Use the mapped index, not the raw ID
          if (const int32 *MappedIdx = VertexIDToPointIndex.Find(V)) {
            Wedge.VertexIndex = *MappedIdx;
          } else {
            Wedge.VertexIndex = 0; // Fallback
          }

          if (UVs.IsValid()) {
            Wedge.UVs[0] = UVs.Get(VI);
          }
          const uint32 WedgeIndex = ImportData.Wedges.Add(Wedge);
          Face.WedgeIndex[Corner] = WedgeIndex;
          if (Tangents.IsValid()) {
            Face.TangentX[Corner] = Tangents.Get(VI);
          }
          if (Normals.IsValid()) {
            Face.TangentZ[Corner] = Normals.Get(VI);
          }
        }
        ImportData.Faces.Add(Face);
      }
    }

    // CRITICAL: Add LODInfo BEFORE creating LODModel - UE5.7 requires LODInfo
    // array to match LODModels The check(LODInfoPtr) assertion at
    // SkeletalMesh.cpp:6570 fails if this is missing
    if (SkeletalMesh->GetLODNum() == 0) {
      FSkeletalMeshLODInfo &LODInfo = SkeletalMesh->AddLODInfo();
      LODInfo.ReductionSettings.NumOfTrianglesPercentage = 1.0f;
      LODInfo.ReductionSettings.NumOfVertPercentage = 1.0f;
      LODInfo.LODHysteresis = 0.02f;
      LODInfo.ScreenSize = FPerPlatformFloat(1.0f);
    }

    // Add materials to the skeletal mesh - copy from source mesh
    if (SkeletalMesh->GetMaterials().Num() == 0) {
      // Copy materials from the source static mesh
      const TArray<FStaticMaterial> &SourceMaterials =
          SourceMesh->GetStaticMaterials();
      if (SourceMaterials.Num() > 0) {
        for (const FStaticMaterial &SrcMat : SourceMaterials) {
          FSkeletalMaterial SkelMat;
          SkelMat.MaterialInterface = SrcMat.MaterialInterface;
          SkelMat.MaterialSlotName = SrcMat.MaterialSlotName;
          SkelMat.ImportedMaterialSlotName = SrcMat.ImportedMaterialSlotName;
          SkelMat.UVChannelData = SrcMat.UVChannelData;
          SkeletalMesh->GetMaterials().Add(SkelMat);
        }
      } else {
        // Fallback to default material if source has none
        SkeletalMesh->GetMaterials().Add(FSkeletalMaterial());
      }
    }

    FSkeletalMeshModel *ImportedModel = SkeletalMesh->GetImportedModel();
    if (!ImportedModel) {
      // UE5.7: GetImportedModel may return nullptr, allocate the model manually
      SkeletalMesh->AllocateResourceForRendering();
      ImportedModel = SkeletalMesh->GetImportedModel();
    }

    if (ImportedModel && ImportedModel->LODModels.Num() == 0) {
      ImportedModel->LODModels.Add(new FSkeletalMeshLODModel());
    }

    if (ImportedModel && ImportedModel->LODModels.Num() > 0) {
      FSkeletalMeshLODModel &LODModel = ImportedModel->LODModels[0];
      TArray<FText> WarnMsgs;
      TArray<FName> WarnNames;
      IMeshUtilities &MeshUtils =
          FModuleManager::LoadModuleChecked<IMeshUtilities>("MeshUtilities");
      IMeshUtilities::MeshBuildOptions BuildOpts;
      const FSkeletalMeshBuildSettings BuildSettings;
      BuildOpts.FillOptions(BuildSettings);
      TArray<FVector3f> LODPoints;
      TArray<SkeletalMeshImportData::FMeshWedge> LODWedges;
      TArray<SkeletalMeshImportData::FMeshFace> LODFaces;
      TArray<SkeletalMeshImportData::FVertInfluence> LODInfluences;
      TArray<int32> LODPointToRaw;
      ImportData.CopyLODImportData(LODPoints, LODWedges, LODFaces,
                                   LODInfluences, LODPointToRaw);
      const bool bBuilt = MeshUtils.BuildSkeletalMesh(
          LODModel, SkeletalMesh->GetPathName(), SkeletalMesh->GetRefSkeleton(),
          LODInfluences, LODWedges, LODFaces, LODPoints, LODPointToRaw,
          BuildOpts, &WarnMsgs, &WarnNames);

      if (!bBuilt) {
        UE_LOG(LogTemp, Error,
               TEXT("K-Cloner: BuildSkeletalMesh failed! Warnings:"));
        for (const FText &Msg : WarnMsgs) {
          UE_LOG(LogTemp, Warning, TEXT("  %s"), *Msg.ToString());
        }
      } else {
        // Log success info
        UE_LOG(LogTemp, Log,
               TEXT("K-Cloner: BuildSkeletalMesh succeeded - "
                    "LODModel.Sections: %d, Vertices: %d, NumVertices: %d"),
               LODModel.Sections.Num(), LODModel.NumVertices,
               (LODModel.Sections.Num() > 0 ? LODModel.Sections[0].NumVertices
                                            : 0));
      }

      LODModel.NumTexCoords = FMath::Max<uint32>(1, ImportData.NumTexCoords);

      if (LODPoints.Num() > 0) {
        FBox3f BB = FBox3f(LODPoints);
        FVector3f Origin;
        FVector3f Extent;
        BB.GetCenterAndExtents(Origin, Extent);
        const double Radius = Extent.Size();
        SkeletalMesh->SetImportedBounds(
            FBoxSphereBounds(FVector(Origin), FVector(Extent), Radius));
      }

      Skeleton->SetPreviewMesh(SkeletalMesh);

      // Build the mesh
      SkeletalMesh->Build();

      // CRITICAL: Calculate inverse reference matrices for skinning
      // This populates RefBasesInvMatrix which is required for rendering
      // The check(NumRefBasesInvMatrix != 0) at SkeletalRender.cpp:402 fails
      // without this
      SkeletalMesh->CalculateInvRefMatrices();

      // Initialize render resources - required for editor preview
      SkeletalMesh->InitResources();

      // Update UV channel data for materials
      SkeletalMesh->UpdateUVChannelData(false);
    }
  } // Close the outer mesh building scope (line 811)

  // Finalize the skeletal mesh - Must happen after all building is complete
  // Force invalidation to ensure DDC regenerates properly
  SkeletalMesh->InvalidateDeriveDataCacheGUID();
  SkeletalMesh->PostEditChange();
  SkeletalMesh->MarkPackageDirty();

  // Force render data initialization by requesting it
  // This triggers the internal build if not already done
  FSkeletalMeshRenderData *RenderData = SkeletalMesh->GetResourceForRendering();
  if (RenderData) {
    UE_LOG(LogTemp, Log, TEXT("K-Cloner: RenderData LODs: %d"),
           RenderData->LODRenderData.Num());
  } else {
    UE_LOG(LogTemp, Warning, TEXT("K-Cloner: RenderData is null after build!"));
  }

  FAssetRegistryModule::AssetCreated(SkeletalMesh);
  FAssetRegistryModule::AssetCreated(Skeleton);
  Package->MarkPackageDirty();

  // Wait for async skeletal mesh compilation to finish before allowing user to
  // open it This prevents the crash when trying to open the mesh immediately
  // after baking
#if WITH_EDITOR
  FSkinnedAssetCompilingManager::Get().FinishCompilation({SkeletalMesh});
#endif

  // Save the package to disk to ensure all data is properly serialized
  FString PackageFileName = FPackageName::LongPackageNameToFilename(
      PackageName, FPackageName::GetAssetPackageExtension());
  FSavePackageArgs SaveArgs;
  SaveArgs.TopLevelFlags = RF_Standalone;
  UPackage::SavePackage(Package, SkeletalMesh, *PackageFileName, SaveArgs);

  FMessageDialog::Open(
      EAppMsgType::Ok,
      FText::Format(
          LOCTEXT(
              "SkeletalMeshSuccess",
              "Skeletal Mesh created: {0}\nBones: {1}\n\nNote: If opening the "
              "mesh causes issues, try saving all and reopening the editor."),
          FText::FromString(AssetName), FText::AsNumber(InstanceCount + 1)));

  return SkeletalMesh;
}

UAnimSequence *FKClonerBakingUtils::BakeToAnimSequence(
    AKClonerActor *ClonerActor, USkeletalMesh *TargetMesh,
    const FString &PackageName, float Duration, float FrameRate) {
  if (!ClonerActor || !TargetMesh || !TargetMesh->GetSkeleton()) {
    FMessageDialog::Open(EAppMsgType::Ok,
                         LOCTEXT("InvalidAnimParams",
                                 "Invalid parameters for animation baking."));
    return nullptr;
  }

  UPackage *Package = CreatePackage(*PackageName);
  if (!Package)
    return nullptr;

  FString AssetName = FPaths::GetBaseFilename(PackageName);
  UAnimSequence *AnimSequence =
      NewObject<UAnimSequence>(Package, *AssetName, RF_Public | RF_Standalone);

  if (!AnimSequence)
    return nullptr;

  AnimSequence->SetSkeleton(TargetMesh->GetSkeleton());

  IAnimationDataController &Controller = AnimSequence->GetController();

  // CRITICAL: Initialize the model before adding any curves
  // UE5.7 (and probably earlier) freaks out if you don't do this
  Controller.InitializeModel();

  Controller.OpenBracket(LOCTEXT("BakeAnimation", "Bake K-Cloner Animation"));

  Controller.SetFrameRate(FFrameRate(FrameRate, 1));

  int32 FrameCount = FMath::CeilToInt(Duration * FrameRate);
  Controller.SetNumberOfFrames(FFrameNumber(FrameCount));

  // Add transform tracks for each bone
  const FReferenceSkeleton &RefSkeleton = TargetMesh->GetRefSkeleton();
  int32 BoneCount = RefSkeleton.GetNum();

  // add Root bone (bone 0) with identity transform
  // UE animation system needs a root or it cries
  {
    FName RootBoneName = RefSkeleton.GetBoneName(0);
    TArray<FVector3f> RootPositionalKeys;
    TArray<FQuat4f> RootRotationalKeys;
    TArray<FVector3f> RootScalingKeys;
    RootPositionalKeys.SetNum(FrameCount);
    RootRotationalKeys.SetNum(FrameCount);
    RootScalingKeys.SetNum(FrameCount);

    for (int32 Frame = 0; Frame < FrameCount; ++Frame) {
      RootPositionalKeys[Frame] = FVector3f::ZeroVector;
      RootRotationalKeys[Frame] = FQuat4f::Identity;
      RootScalingKeys[Frame] = FVector3f::OneVector;
    }

    Controller.AddBoneCurve(RootBoneName);
    Controller.SetBoneTrackKeys(RootBoneName, RootPositionalKeys,
                                RootRotationalKeys, RootScalingKeys);
  }

  // loop through all our fake "bones" (instances) and keyframe them
  for (int32 BoneIdx = 1; BoneIdx < BoneCount; ++BoneIdx) {
    FName BoneName = RefSkeleton.GetBoneName(BoneIdx);

    TArray<FVector3f> PositionalKeys;
    TArray<FQuat4f> RotationalKeys;
    TArray<FVector3f> ScalingKeys;

    PositionalKeys.SetNum(FrameCount);
    RotationalKeys.SetNum(FrameCount);
    ScalingKeys.SetNum(FrameCount);

    int32 InstanceIdx = BoneIdx - 1;

    for (int32 Frame = 0; Frame < FrameCount; ++Frame) {
      float Time = Frame / FrameRate;
      // slow as hell but it works: sample the whole cloner for every frame
      TArray<FTransform> Transforms =
          SampleInstanceTransforms(ClonerActor, Time);

      if (Transforms.IsValidIndex(InstanceIdx)) {
        FTransform T = Transforms[InstanceIdx];
        PositionalKeys[Frame] = FVector3f(T.GetLocation());
        RotationalKeys[Frame] = FQuat4f(T.GetRotation());
        ScalingKeys[Frame] = FVector3f(T.GetScale3D());
      } else {
        PositionalKeys[Frame] = FVector3f::ZeroVector;
        RotationalKeys[Frame] = FQuat4f::Identity;
        ScalingKeys[Frame] = FVector3f::OneVector;
      }
    }

    Controller.AddBoneCurve(BoneName);
    Controller.SetBoneTrackKeys(BoneName, PositionalKeys, RotationalKeys,
                                ScalingKeys);
  }

  Controller.CloseBracket();

  // Finalize the animation
  Controller.NotifyPopulated();
  AnimSequence->PostEditChange();

  FAssetRegistryModule::AssetCreated(AnimSequence);
  Package->MarkPackageDirty();

  FMessageDialog::Open(
      EAppMsgType::Ok,
      FText::Format(
          LOCTEXT(
              "AnimSequenceSuccess",
              "Animation Sequence created: {0}\nDuration: {1}s\nFrames: {2}"),
          FText::FromString(AssetName), FText::AsNumber(Duration),
          FText::AsNumber(FrameCount)));

  return AnimSequence;
}

#undef LOCTEXT_NAMESPACE
