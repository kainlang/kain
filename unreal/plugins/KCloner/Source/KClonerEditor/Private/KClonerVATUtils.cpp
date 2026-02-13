// Copyright 2026 K-Studio. All Rights Reserved.

#include "KClonerVATUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "KClonerActor.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "KClonerVATUtils"

// 16-bit pixel helper - UE needs this for high precisiin VAT textures
// don't mess with the math here or it'll explode lol
struct FPixel16 {
  uint16 R, G, B, A;

  FPixel16() : R(0), G(0), B(0), A(65535) {}
  FPixel16(float InR, float InG, float InB, float InA = 1.0f) {
    R = (uint16)FMath::Clamp(FMath::RoundToInt(InR * 65535.0f), 0, 65535);
    G = (uint16)FMath::Clamp(FMath::RoundToInt(InG * 65535.0f), 0, 65535);
    B = (uint16)FMath::Clamp(FMath::RoundToInt(InB * 65535.0f), 0, 65535);
    A = (uint16)FMath::Clamp(FMath::RoundToInt(InA * 65535.0f), 0, 65535);
  }
};

FKClonerVATResult
FKClonerVATUtils::BakeToVAT(AKClonerActor *ClonerActor,
                            const FKClonerVATOptions &Options) {
  FKClonerVATResult Result;

  // BAKE TO VAT - the big one
  // turns cloner anims into textures so we can render 10k items at 60fps
  // it's absolute magic when it works, and a pain when it doesn't
  if (!ClonerActor) {
    Result.ErrorMessage = TEXT("Invalid cloner actor");
    return Result;
  }

  if (!ClonerActor->SourceMesh) {
    Result.ErrorMessage = TEXT("Cloner has no source mesh assigned");
    FMessageDialog::Open(EAppMsgType::Ok,
                         FText::FromString(Result.ErrorMessage));
    return Result;
  }

  // Get instance count
  UHierarchicalInstancedStaticMeshComponent *ISMC = ClonerActor->InstancedMesh;
  if (!ISMC || ISMC->GetInstanceCount() == 0) {
    Result.ErrorMessage = TEXT("Cloner has no instances");
    FMessageDialog::Open(EAppMsgType::Ok,
                         FText::FromString(Result.ErrorMessage));
    return Result;
  }

  Result.InstanceCount = ISMC->GetInstanceCount();

  // figure out how many frames we need to capture
  // duration * fps = frames. easy math.
  Result.TotalFrames = FMath::CeilToInt(Options.Duration * Options.FrameRate);
  Result.FrameRate = Options.FrameRate;

  if (Result.TotalFrames < 1) {
    Result.ErrorMessage = TEXT("Duration too short, need at least 1 frame");
    return Result;
  }

  // find the smallest texture that fits all our data
  // instances * frames... gets big fast
  FIntPoint TextureSize = CalculateTextureDimensions(
      Result.InstanceCount, Result.TotalFrames, Options.MaxWidth,
      Options.MaxHeight, Result.RowsPerFrame);

  if (TextureSize.X == 0 || TextureSize.Y == 0) {
    Result.ErrorMessage = FString::Printf(
        TEXT("Cannot fit %d instances x %d frames in %dx%d texture"),
        Result.InstanceCount, Result.TotalFrames, Options.MaxWidth,
        Options.MaxHeight);
    FMessageDialog::Open(EAppMsgType::Ok,
                         FText::FromString(Result.ErrorMessage));
    return Result;
  }

  // Sample all frames
  TArray<TArray<FTransform>> FrameData;
  FrameData.SetNum(Result.TotalFrames);

  // find the bounds so we can normalize positions to 0-1
  // if we don't do this, the math in the shader gets nasty
  // also adds a bit of padding cuz floating point errors suck
  TArray<FTransform> FirstFrame = SampleInstanceTransforms(ClonerActor, 0.0f);
  Result.BoundsMin = FVector(TNumericLimits<float>::Max());
  FVector BoundsMax = FVector(TNumericLimits<float>::Min());

  for (const FTransform &T : FirstFrame) {
    Result.BoundsMin = Result.BoundsMin.ComponentMin(T.GetLocation());
    BoundsMax = BoundsMax.ComponentMax(T.GetLocation());
  }

  // Expand bounds slightly
  Result.BoundsSize = BoundsMax - Result.BoundsMin;
  Result.BoundsSize =
      Result.BoundsSize.ComponentMax(FVector(1.0f)); // Minimum size
  Result.BoundsMin -= Result.BoundsSize * 0.1f;      // 10% padding
  Result.BoundsSize *= 1.2f;

  // run the cloner frame by frame and yoink the transforms
  float TimeStep = 1.0f / Options.FrameRate;
  for (int32 Frame = 0; Frame < Result.TotalFrames; ++Frame) {
    float Time = Frame * TimeStep;
    FrameData[Frame] = SampleInstanceTransforms(ClonerActor, Time);
  }

  // Generate package path if not specified
  FString PackagePath = Options.PackagePath;
  if (PackagePath.IsEmpty()) {
    if (ClonerActor->SourceMesh) {
      PackagePath = ClonerActor->SourceMesh->GetOutermost()->GetName();
      PackagePath =
          FPaths::GetPath(PackagePath) + TEXT("/VAT_") + ClonerActor->GetName();
    } else {
      PackagePath = TEXT("/Game/VAT_") + ClonerActor->GetName();
    }
  }

  // save the textures to content browser
  // Position is RGB = XYZ, A = 1 (for now)
  Result.PositionTexture = CreatePositionTexture(
      FrameData, Result.InstanceCount, Result.TotalFrames, Result.BoundsMin,
      Result.BoundsSize, Options.Precision, TextureSize.X, TextureSize.Y,
      PackagePath);

  // Create rotation texture
  if (Options.bBakeRotation) {
    Result.RotationTexture = CreateRotationTexture(
        FrameData, Result.InstanceCount, Result.TotalFrames, Options.Precision,
        TextureSize.X, TextureSize.Y, PackagePath);
  }

  // auto-setup the material so the user doesn't have to link 50 things
  // this part is a lifesaver lol
  Result.VATMaterial = CreateAndSaveVATMaterial(
      Result.PositionTexture, Result.RotationTexture, Result.BoundsMin,
      Result.BoundsSize, Result.TotalFrames, Result.FrameRate, PackagePath,
      Result.InstanceCount);

  // Success!
  Result.bSuccess = true;

  // Show result dialog
  FMessageDialog::Open(
      EAppMsgType::Ok,
      FText::Format(
          LOCTEXT("VATBakeSuccess", "VAT Bake Complete!\n\n"
                                    "Instances: {0}\n"
                                    "Frames: {1}\n"
                                    "Texture Size: {2}x{3}\n"
                                    "Bounds Min: {4}\n"
                                    "Bounds Size: {5}\n\n"
                                    "Position Texture: {6}\n"
                                    "Rotation Texture: {7}\n"
                                    "VAT Material: {8}"),
          FText::AsNumber(Result.InstanceCount),
          FText::AsNumber(Result.TotalFrames), FText::AsNumber(TextureSize.X),
          FText::AsNumber(TextureSize.Y),
          FText::FromString(Result.BoundsMin.ToString()),
          FText::FromString(Result.BoundsSize.ToString()),
          Result.PositionTexture
              ? FText::FromString(Result.PositionTexture->GetName())
              : LOCTEXT("None", "None"),
          Result.RotationTexture
              ? FText::FromString(Result.RotationTexture->GetName())
              : LOCTEXT("None", "None"),
          Result.VATMaterial ? FText::FromString(Result.VATMaterial->GetName())
                             : LOCTEXT("None", "None")));

  return Result;
}

TArray<FTransform>
FKClonerVATUtils::SampleInstanceTransforms(AKClonerActor *ClonerActor,
                                           float Time) {
  TArray<FTransform> Transforms;

  if (!ClonerActor || !ClonerActor->InstancedMesh) {
    return Transforms;
  }

  // Set override time on the cloner
  ClonerActor->bUseOverrideTime = true;
  ClonerActor->OverrideTime = Time;

  // force a tick to update cloner logic at this specific time
  ClonerActor->Tick(0.0f);

  // Extract transforms from ISMC
  UHierarchicalInstancedStaticMeshComponent *ISMC = ClonerActor->InstancedMesh;
  int32 InstanceCount = ISMC->GetInstanceCount();
  Transforms.SetNum(InstanceCount);

  for (int32 i = 0; i < InstanceCount; ++i) {
    ISMC->GetInstanceTransform(i, Transforms[i], true); // World space
  }

  // Reset override
  ClonerActor->bUseOverrideTime = false;

  return Transforms;
}

UTexture2D *FKClonerVATUtils::CreatePositionTexture(
    const TArray<TArray<FTransform>> &FrameData, int32 InstanceCount,
    int32 TotalFrames, const FVector &BoundsMin, const FVector &BoundsSize,
    EKClonerVATPrecision Precision, int32 Width, int32 Height,
    const FString &PackagePath) {
  FString TexturePath = PackagePath + TEXT("_Position");
  UPackage *Package = CreatePackage(*TexturePath);
  if (!Package) {
    return nullptr;
  }

  UTexture2D *Texture = NewObject<UTexture2D>(
      Package, FName(*FPaths::GetBaseFilename(TexturePath)),
      RF_Public | RF_Standalone);
  if (!Texture) {
    return nullptr;
  }

  // alloc pixel buffer - 16bit is huge but we need it for smooth motion
  TArray<FPixel16> Pixels;
  Pixels.SetNum(Width * Height);

  // Fill with default (zero displacement, full alpha)
  for (FPixel16 &P : Pixels) {
    P = FPixel16(0.5f, 0.5f, 0.5f, 1.0f);
  }

  // Calculate rows per frame
  int32 RowsPerFrame = FMath::CeilToInt((float)InstanceCount / Width);

  // shove frame data into pixels
  // each instance gets one pixel per frame
  for (int32 Frame = 0; Frame < TotalFrames; ++Frame) {
    const TArray<FTransform> &Transforms = FrameData[Frame];
    int32 RowStart = Frame * RowsPerFrame;

    for (int32 i = 0; i < InstanceCount && i < Transforms.Num(); ++i) {
      int32 Row = RowStart + (i / Width);
      int32 Col = i % Width;
      int32 PixelIndex = Row * Width + Col;

      if (PixelIndex < Pixels.Num()) {
        FVector Pos = Transforms[i].GetLocation();
        FVector NormPos = NormalizePosition(Pos, BoundsMin, BoundsSize);

        Pixels[PixelIndex] = FPixel16(NormPos.X, NormPos.Y, NormPos.Z, 1.0f);
      }
    }
  }

  // Initialize texture source
  Texture->Source.Init(Width, Height, 1, 1, ETextureSourceFormat::TSF_RGBA16,
                       (const uint8 *)Pixels.GetData());

  // set texture flags - NO SRGB, NO MIPS, NO FILTERING
  // we need raw data or the math fails miserably
  Texture->SRGB = false;
  Texture->Filter =
      TextureFilter::TF_Nearest; // No interpolation for data textures
  Texture->CompressionSettings = TextureCompressionSettings::TC_HDR;
  Texture->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;

  // Update and register
  Texture->UpdateResource();
  Texture->PostEditChange();

  FAssetRegistryModule::AssetCreated(Texture);
  Package->MarkPackageDirty();

  return Texture;
}

UTexture2D *FKClonerVATUtils::CreateRotationTexture(
    const TArray<TArray<FTransform>> &FrameData, int32 InstanceCount,
    int32 TotalFrames, EKClonerVATPrecision Precision, int32 Width,
    int32 Height, const FString &PackagePath) {
  FString TexturePath = PackagePath + TEXT("_Rotation");
  UPackage *Package = CreatePackage(*TexturePath);
  if (!Package) {
    return nullptr;
  }

  UTexture2D *Texture = NewObject<UTexture2D>(
      Package, FName(*FPaths::GetBaseFilename(TexturePath)),
      RF_Public | RF_Standalone);
  if (!Texture) {
    return nullptr;
  }

  // Generate pixel data
  TArray<FPixel16> Pixels;
  Pixels.SetNum(Width * Height);

  // Fill with identity rotation (0.5 for axis, 0 for angle)
  for (FPixel16 &P : Pixels) {
    P = FPixel16(0.5f, 0.5f, 0.5f, 0.0f);
  }

  // Calculate rows per frame
  int32 RowsPerFrame = FMath::CeilToInt((float)InstanceCount / Width);

  // Write frame data
  for (int32 Frame = 0; Frame < TotalFrames; ++Frame) {
    const TArray<FTransform> &Transforms = FrameData[Frame];
    int32 RowStart = Frame * RowsPerFrame;

    for (int32 i = 0; i < InstanceCount && i < Transforms.Num(); ++i) {
      int32 Row = RowStart + (i / Width);
      int32 Col = i % Width;
      int32 PixelIndex = Row * Width + Col;

      if (PixelIndex < Pixels.Num()) {
        FVector4 EncodedRot = EncodeRotation(Transforms[i].GetRotation());
        Pixels[PixelIndex] =
            FPixel16(EncodedRot.X, EncodedRot.Y, EncodedRot.Z, EncodedRot.W);
      }
    }
  }

  // Initialize texture source
  Texture->Source.Init(Width, Height, 1, 1, ETextureSourceFormat::TSF_RGBA16,
                       (const uint8 *)Pixels.GetData());

  // Set texture properties for VAT
  Texture->SRGB = false;
  Texture->Filter = TextureFilter::TF_Nearest;
  Texture->CompressionSettings = TextureCompressionSettings::TC_HDR;
  Texture->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;

  // Update and register
  Texture->UpdateResource();
  Texture->PostEditChange();

  FAssetRegistryModule::AssetCreated(Texture);
  Package->MarkPackageDirty();

  return Texture;
}

FIntPoint FKClonerVATUtils::CalculateTextureDimensions(int32 InstanceCount,
                                                       int32 TotalFrames,
                                                       int32 MaxWidth,
                                                       int32 MaxHeight,
                                                       int32 &OutRowsPerFrame) {
  // find width/height that fits all instances across all frames
  // try to keep it under 2048 or 4096 lol
  OutRowsPerFrame = FMath::CeilToInt((float)InstanceCount / MaxWidth);

  int32 RequiredHeight = OutRowsPerFrame * TotalFrames;
  int32 RequiredWidth = FMath::Min(InstanceCount, MaxWidth);

  if (RequiredHeight > MaxHeight) {
    // Cannot fit in texture
    return FIntPoint(0, 0);
  }

  // Round to power of 2 if beneficial
  int32 Width = FMath::RoundUpToPowerOfTwo(RequiredWidth);
  int32 Height = FMath::RoundUpToPowerOfTwo(RequiredHeight);

  // Cap at max
  Width = FMath::Min(Width, MaxWidth);
  Height = FMath::Min(Height, MaxHeight);

  return FIntPoint(Width, Height);
}

FVector FKClonerVATUtils::NormalizePosition(const FVector &Position,
                                            const FVector &BoundsMin,
                                            const FVector &BoundsSize) {
  return (Position - BoundsMin) / BoundsSize;
}

FVector4 FKClonerVATUtils::EncodeRotation(const FQuat &Rotation) {
  // axis-angle encoding - a bit weird but works for shaders
  // basically xyz = axis, w = angle. 
  // hope the shader math matches this lol

  FVector Axis;
  float Angle;
  Rotation.ToAxisAndAngle(Axis, Angle);

  // Normalize axis to 0-1 range (from -1 to 1)
  FVector EncodedAxis = (Axis + FVector::OneVector) * 0.5f;

  // Normalize angle to 0-1 range (from -PI to PI)
  float EncodedAngle = (Angle / PI) * 0.5f + 0.5f;

  return FVector4(EncodedAxis.X, EncodedAxis.Y, EncodedAxis.Z, EncodedAngle);
}

EPixelFormat FKClonerVATUtils::GetPixelFormat(EKClonerVATPrecision Precision) {
  switch (Precision) {
  case EKClonerVATPrecision::Low:
    return PF_B8G8R8A8;
  case EKClonerVATPrecision::High:
    return PF_R16G16B16A16_UNORM;
  case EKClonerVATPrecision::Ultra:
    return PF_FloatRGBA;
  default:
    return PF_R16G16B16A16_UNORM;
  }
}

ETextureSourceFormat
FKClonerVATUtils::GetTextureSourceFormat(EKClonerVATPrecision Precision) {
  switch (Precision) {
  case EKClonerVATPrecision::Low:
    return TSF_BGRA8;
  case EKClonerVATPrecision::High:
    return TSF_RGBA16;
  case EKClonerVATPrecision::Ultra:
    return TSF_RGBA32F;
  default:
    return TSF_RGBA16;
  }
}

// ============================================================
// AUTO-VAT MATERIAL GENERATION
// creating a whole material graph from C++ is PAIN
// but better than manual setup every time i guess
// ============================================================

#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionFrac.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionTransform.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/SavePackage.h"

UMaterialInstanceDynamic *FKClonerVATUtils::CreateVATMaterial(
    UTexture2D *PositionTexture, UTexture2D *RotationTexture,
    const FVector &BoundsMin, const FVector &BoundsSize, int32 TotalFrames,
    float FrameRate, UObject *Outer) {
  // check if we already have the base material somewhere
  // we need the parent to create instances from it
  UMaterial *BaseMat = GetOrCreateBaseVATMaterial();
  if (!BaseMat) {
    UE_LOG(LogTemp, Error, TEXT("Failed to get/create base VAT material"));
    return nullptr;
  }

  // Create dynamic material instance
  UMaterialInstanceDynamic *MatInst =
      UMaterialInstanceDynamic::Create(BaseMat, Outer);
  if (!MatInst) {
    UE_LOG(LogTemp, Error, TEXT("Failed to create VAT material instance"));
    return nullptr;
  }

  // Set texture parameters
  if (PositionTexture) {
    MatInst->SetTextureParameterValue(FName("VATPositionTexture"),
                                      PositionTexture);
  }
  if (RotationTexture) {
    MatInst->SetTextureParameterValue(FName("VATRotationTexture"),
                                      RotationTexture);
  }
  MatInst->SetScalarParameterValue(FName("VATHasRotation"),
                                   RotationTexture ? 1.0f : 0.0f);

  // punch in all the texture and param values
  MatInst->SetVectorParameterValue(
      FName("VATBoundsMin"),
      FLinearColor(BoundsMin.X, BoundsMin.Y, BoundsMin.Z, 0.0f));
  MatInst->SetVectorParameterValue(
      FName("VATBoundsSize"),
      FLinearColor(BoundsSize.X, BoundsSize.Y, BoundsSize.Z, 0.0f));
  MatInst->SetScalarParameterValue(FName("VATFrameCount"), (float)TotalFrames);
  MatInst->SetScalarParameterValue(FName("VATFrameRate"), FrameRate);

  return MatInst;
}

UMaterialInterface *FKClonerVATUtils::CreateAndSaveVATMaterial(
    UTexture2D *PositionTexture, UTexture2D *RotationTexture,
    const FVector &BoundsMin, const FVector &BoundsSize, int32 TotalFrames,
    float FrameRate, const FString &PackagePath, int32 InstanceCount) {
  // Get or create base material
  UMaterial *BaseMat = GetOrCreateBaseVATMaterial();
  if (!BaseMat) {
    return nullptr;
  }

  // setup a permanent material asset in the project folder
  FString MatInstName =
      FString::Printf(TEXT("%s_VATMat"), *FPaths::GetBaseFilename(PackagePath));
  FString MatInstPath = FPaths::GetPath(PackagePath) / MatInstName;

  UPackage *Package = CreatePackage(*MatInstPath);

  UMaterialInstanceConstantFactoryNew *Factory =
      NewObject<UMaterialInstanceConstantFactoryNew>();
  Factory->InitialParent = BaseMat;

  UMaterialInstanceConstant *MatInst =
      Cast<UMaterialInstanceConstant>(Factory->FactoryCreateNew(
          UMaterialInstanceConstant::StaticClass(), Package,
          FName(*MatInstName), RF_Public | RF_Standalone, nullptr, GWarn));

  if (!MatInst) {
    return nullptr;
  }

  // Set parameters
  if (PositionTexture) {
    MatInst->SetTextureParameterValueEditorOnly(
        FMaterialParameterInfo(FName("VATPositionTexture")), PositionTexture);
  }
  if (RotationTexture) {
    MatInst->SetTextureParameterValueEditorOnly(
        FMaterialParameterInfo(FName("VATRotationTexture")), RotationTexture);
  }
  MatInst->SetScalarParameterValueEditorOnly(
      FMaterialParameterInfo(FName("VATHasRotation")),
      RotationTexture ? 1.0f : 0.0f);
  MatInst->SetVectorParameterValueEditorOnly(
      FMaterialParameterInfo(FName("VATBoundsMin")),
      FLinearColor(BoundsMin.X, BoundsMin.Y, BoundsMin.Z, 0.0f));
  MatInst->SetVectorParameterValueEditorOnly(
      FMaterialParameterInfo(FName("VATBoundsSize")),
      FLinearColor(BoundsSize.X, BoundsSize.Y, BoundsSize.Z, 0.0f));
  MatInst->SetScalarParameterValueEditorOnly(
      FMaterialParameterInfo(FName("VATFrameCount")), (float)TotalFrames);
  MatInst->SetScalarParameterValueEditorOnly(
      FMaterialParameterInfo(FName("VATFrameRate")), FrameRate);
  if (PositionTexture) {
    float W = (float)PositionTexture->GetSizeX();
    float H = (float)PositionTexture->GetSizeY();
    float RPF =
        InstanceCount > 0 ? FMath::CeilToFloat((float)InstanceCount / W) : 1.0f;
    MatInst->SetScalarParameterValueEditorOnly(
        FMaterialParameterInfo(FName("VATTexWidth")), W);
    MatInst->SetScalarParameterValueEditorOnly(
        FMaterialParameterInfo(FName("VATTexHeight")), H);
    MatInst->SetScalarParameterValueEditorOnly(
        FMaterialParameterInfo(FName("VATRowsPerFrame")), RPF);
  }

  // Save the asset
  FAssetRegistryModule::AssetCreated(MatInst);
  Package->MarkPackageDirty();

  FSavePackageArgs SaveArgs;
  SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
  UPackage::SavePackage(
      Package, MatInst,
      *FPackageName::LongPackageNameToFilename(
          MatInstPath, FPackageName::GetAssetPackageExtension()),
      SaveArgs);

  return MatInst;
}

// find our master VAT shader
// if it's missing, we build it from SCRATCH in code (hardcore lol)
UMaterial* FKClonerVATUtils::GetOrCreateBaseVATMaterial()
{
  // Cache the material once successfully created/loaded this session
  static TWeakObjectPtr<UMaterial> CachedBaseMat;
  if (CachedBaseMat.IsValid()) {
    return CachedBaseMat.Get();
  }

  // Try loading from plugin content first (may be shipped with plugin)
  static const TCHAR *PluginMatPath =
      TEXT("/KCloner/Materials/M_KCloner_VAT_Base");
  
  // Force synchronous load to ensure we don't get partially loaded asset
  UMaterial *BaseMat = LoadObject<UMaterial>(nullptr, PluginMatPath, nullptr, 
      LOAD_NoWarn | LOAD_Quiet);
  
  // If loaded and fully ready, use it
  if (BaseMat && !BaseMat->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad)) {
    UE_LOG(LogTemp, Log, TEXT("Using existing VAT base material from plugin: %s"), PluginMatPath);
    CachedBaseMat = BaseMat;
    return BaseMat;
  }

  // Try loading from Game content (user-created)
  static const TCHAR *GameMatPath =
      TEXT("/Game/KCloner/Materials/M_KCloner_VAT_Base");
  
  BaseMat = LoadObject<UMaterial>(nullptr, GameMatPath, nullptr, 
      LOAD_NoWarn | LOAD_Quiet);
  
  if (BaseMat && !BaseMat->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad)) {
    UE_LOG(LogTemp, Log, TEXT("Using existing VAT base material from Game: %s"), GameMatPath);
    CachedBaseMat = BaseMat;
    return BaseMat;
  }

  // Neither exists or both are partially loaded - create new in /Game folder
  UE_LOG(LogTemp, Log, TEXT("Creating new VAT base material in /Game/KCloner/Materials/"));

  FString PackagePath = TEXT("/Game/KCloner/Materials/M_KCloner_VAT_Base");
  UPackage *Package = CreatePackage(*PackagePath);
  if (!Package) {
    UE_LOG(LogTemp, Error, TEXT("Failed to create package for VAT material"));
    return nullptr;
  }

  // Make sure package is fully loaded
  Package->FullyLoad();

  UMaterialFactoryNew *Factory = NewObject<UMaterialFactoryNew>();
  BaseMat = Cast<UMaterial>(Factory->FactoryCreateNew(
      UMaterial::StaticClass(), Package, FName("M_KCloner_VAT_Base"),
      RF_Public | RF_Standalone, nullptr, GWarn));

  if (!BaseMat) {
    UE_LOG(LogTemp, Error, TEXT("Failed to create base VAT material"));
    return nullptr;
  }

  // === BUILD MASTER MATERIAL GRAPH ===
  // this is the crazy part where we manually build the shader nodes
  // seriously, don't read this unless you like pain lol

  // Texture Parameters
  UMaterialExpressionTextureSampleParameter2D *PosTexSampler =
      NewObject<UMaterialExpressionTextureSampleParameter2D>(BaseMat);
  PosTexSampler->ParameterName = FName("VATPositionTexture");
  PosTexSampler->SamplerType = SAMPLERTYPE_LinearColor;
  BaseMat->GetExpressionCollection().AddExpression(PosTexSampler);

  UMaterialExpressionTextureSampleParameter2D *RotTexSampler =
      NewObject<UMaterialExpressionTextureSampleParameter2D>(BaseMat);
  RotTexSampler->ParameterName = FName("VATRotationTexture");
  RotTexSampler->SamplerType = SAMPLERTYPE_LinearColor;
  BaseMat->GetExpressionCollection().AddExpression(RotTexSampler);

  // Scalar Parameters
  UMaterialExpressionScalarParameter *FrameCount =
      NewObject<UMaterialExpressionScalarParameter>(BaseMat);
  FrameCount->ParameterName = FName("VATFrameCount");
  FrameCount->DefaultValue = 60.0f;
  BaseMat->GetExpressionCollection().AddExpression(FrameCount);

  UMaterialExpressionScalarParameter *FrameRate =
      NewObject<UMaterialExpressionScalarParameter>(BaseMat);
  FrameRate->ParameterName = FName("VATFrameRate");
  FrameRate->DefaultValue = 30.0f;
  BaseMat->GetExpressionCollection().AddExpression(FrameRate);

  UMaterialExpressionScalarParameter *TexWidth =
      NewObject<UMaterialExpressionScalarParameter>(BaseMat);
  TexWidth->ParameterName = FName("VATTexWidth");
  TexWidth->DefaultValue = 1024.0f;
  BaseMat->GetExpressionCollection().AddExpression(TexWidth);

  UMaterialExpressionScalarParameter *TexHeight =
      NewObject<UMaterialExpressionScalarParameter>(BaseMat);
  TexHeight->ParameterName = FName("VATTexHeight");
  TexHeight->DefaultValue = 1024.0f;
  BaseMat->GetExpressionCollection().AddExpression(TexHeight);

  UMaterialExpressionScalarParameter *RowsPerFrame =
      NewObject<UMaterialExpressionScalarParameter>(BaseMat);
  RowsPerFrame->ParameterName = FName("VATRowsPerFrame");
  RowsPerFrame->DefaultValue = 1.0f;
  BaseMat->GetExpressionCollection().AddExpression(RowsPerFrame);
  
  UMaterialExpressionScalarParameter *HasRotation =
      NewObject<UMaterialExpressionScalarParameter>(BaseMat);
  HasRotation->ParameterName = FName("VATHasRotation");
  HasRotation->DefaultValue = 0.0f;
  BaseMat->GetExpressionCollection().AddExpression(HasRotation);

  // Vector Parameters
  UMaterialExpressionVectorParameter *BoundsMin =
      NewObject<UMaterialExpressionVectorParameter>(BaseMat);
  BoundsMin->ParameterName = FName("VATBoundsMin");
  BoundsMin->DefaultValue = FLinearColor::Black;
  BaseMat->GetExpressionCollection().AddExpression(BoundsMin);

  UMaterialExpressionVectorParameter *BoundsSize =
      NewObject<UMaterialExpressionVectorParameter>(BaseMat);
  BoundsSize->ParameterName = FName("VATBoundsSize");
  BoundsSize->DefaultValue = FLinearColor(100.0f, 100.0f, 100.0f, 1.0f);
  BaseMat->GetExpressionCollection().AddExpression(BoundsSize);

  // THE BRAINS - HLSL block for the actual offset math
  // grab instance data (time, index) and sample the texture pixel
  // then move the vertex. magic!
  UMaterialExpressionCustom *CustomWPO =
      NewObject<UMaterialExpressionCustom>(BaseMat);

  // Add Inputs to CustomWPO
  {
    FCustomInput &InPosTex = CustomWPO->Inputs.AddDefaulted_GetRef();
    InPosTex.InputName = TEXT("VATPositionTexture");
    InPosTex.Input.Expression = PosTexSampler;

    FCustomInput &InFC = CustomWPO->Inputs.AddDefaulted_GetRef();
    InFC.InputName = TEXT("VATFrameCount");
    InFC.Input.Expression = FrameCount;

    FCustomInput &InFR = CustomWPO->Inputs.AddDefaulted_GetRef();
    InFR.InputName = TEXT("VATFrameRate");
    InFR.Input.Expression = FrameRate;

    FCustomInput &InTW = CustomWPO->Inputs.AddDefaulted_GetRef();
    InTW.InputName = TEXT("VATTexWidth");
    InTW.Input.Expression = TexWidth;

    FCustomInput &InTH = CustomWPO->Inputs.AddDefaulted_GetRef();
    InTH.InputName = TEXT("VATTexHeight");
    InTH.Input.Expression = TexHeight;

    FCustomInput &InRPF = CustomWPO->Inputs.AddDefaulted_GetRef();
    InRPF.InputName = TEXT("VATRowsPerFrame");
    InRPF.Input.Expression = RowsPerFrame;

    FCustomInput &InBMin = CustomWPO->Inputs.AddDefaulted_GetRef();
    InBMin.InputName = TEXT("VATBoundsMin");
    InBMin.Input.Expression = BoundsMin;

    FCustomInput &InBSize = CustomWPO->Inputs.AddDefaulted_GetRef();
    InBSize.InputName = TEXT("VATBoundsSize");
    InBSize.Input.Expression = BoundsSize;
  }

  CustomWPO->Code =
      TEXT("float4 CustomData = GetPerInstanceCustomData(Parameters, 0);\n"
           "float AnimTime = CustomData.x;\n"
           "float Index = CustomData.y;\n"
           "float TotalCount = CustomData.z;\n"
           "float FrameF = frac(AnimTime * VATFrameRate / VATFrameCount) * "
           "VATFrameCount;\n"
           "float Frame = floor(FrameF);\n"
           "float W = VATTexWidth;\n"
           "float H = VATTexHeight;\n"
           "float RPF = VATRowsPerFrame;\n"
           "float RowStart = Frame * RPF;\n"
           "float Row = RowStart + floor(Index / W);\n"
           "float Col = Index - floor(Index / W) * W;\n"
           "float2 VATUV = float2((Col + 0.5) / W, (Row + 0.5) / H);\n"
           "float3 PosN = "
           "VATPositionTexture.SampleLevel(VATPositionTextureSampler, VATUV, "
           "0).rgb;\n"
           "float3 Pos = PosN * VATBoundsSize.xyz + VATBoundsMin.xyz;\n"
           "return Pos;\n");
  CustomWPO->OutputType = CMOT_Float3;
  CustomWPO->Description = TEXT("VAT Position Offset");
  BaseMat->GetExpressionCollection().AddExpression(CustomWPO);

  // Connect to World Position Offset
  BaseMat->GetEditorOnlyData()->WorldPositionOffset.Expression = CustomWPO;
  UMaterialExpressionVertexNormalWS *VN =
      NewObject<UMaterialExpressionVertexNormalWS>(BaseMat);
  BaseMat->GetExpressionCollection().AddExpression(VN);
  UMaterialExpressionCustom *CustomNormal =
      NewObject<UMaterialExpressionCustom>(BaseMat);
  CustomNormal->OutputType = CMOT_Float3;
  CustomNormal->Description = TEXT("VAT World Normal");

  // Add Inputs to CustomNormal
  {
    FCustomInput &InN = CustomNormal->Inputs.AddDefaulted_GetRef();
    InN.InputName = TEXT("N");
    InN.Input.Expression = VN;

    FCustomInput &InRotTex = CustomNormal->Inputs.AddDefaulted_GetRef();
    InRotTex.InputName = TEXT("VATRotationTexture");
    InRotTex.Input.Expression = RotTexSampler;

    FCustomInput &InFC = CustomNormal->Inputs.AddDefaulted_GetRef();
    InFC.InputName = TEXT("VATFrameCount");
    InFC.Input.Expression = FrameCount;

    FCustomInput &InFR = CustomNormal->Inputs.AddDefaulted_GetRef();
    InFR.InputName = TEXT("VATFrameRate");
    InFR.Input.Expression = FrameRate;

    FCustomInput &InTW = CustomNormal->Inputs.AddDefaulted_GetRef();
    InTW.InputName = TEXT("VATTexWidth");
    InTW.Input.Expression = TexWidth;

    FCustomInput &InTH = CustomNormal->Inputs.AddDefaulted_GetRef();
    InTH.InputName = TEXT("VATTexHeight");
    InTH.Input.Expression = TexHeight;

    FCustomInput &InRPF = CustomNormal->Inputs.AddDefaulted_GetRef();
    InRPF.InputName = TEXT("VATRowsPerFrame");
    InRPF.Input.Expression = RowsPerFrame;

    FCustomInput &InHRot = CustomNormal->Inputs.AddDefaulted_GetRef();
    InHRot.InputName = TEXT("VATHasRotation");
    InHRot.Input.Expression = HasRotation;
  }

  CustomNormal->Code = TEXT(
      "float4 CustomData = GetPerInstanceCustomData(Parameters, 0);\n"
      "float AnimTime = CustomData.x;\n"
      "float Index = CustomData.y;\n"
      "float FrameF = frac(AnimTime * VATFrameRate / VATFrameCount) * "
      "VATFrameCount;\n"
      "float Frame = floor(FrameF);\n"
      "float W = VATTexWidth;\n"
      "float H = VATTexHeight;\n"
      "float RPF = VATRowsPerFrame;\n"
      "float RowStart = Frame * RPF;\n"
      "float Row = RowStart + floor(Index / W);\n"
      "float Col = Index - floor(Index / W) * W;\n"
      "float2 VATUV = float2((Col + 0.5) / W, (Row + 0.5) / H);\n"
      "float4 RotTexel = "
      "VATRotationTexture.SampleLevel(VATRotationTextureSampler, VATUV, 0);\n"
      "float3 Axis = RotTexel.xyz * 2.0 - 1.0;\n"
      "Axis = normalize(Axis);\n"
      "float Angle = (RotTexel.w - 0.5) * 2.0 * 3.14159265;\n"
      "Angle *= VATHasRotation;\n"
      "float3 n = N;\n"
      "float c = cos(Angle);\n"
      "float s = sin(Angle);\n"
      "float3 r = n * c + cross(Axis, n) * s + Axis * dot(Axis, n) * (1.0 - "
      "c);\n"
      "return r;\n");
  BaseMat->GetExpressionCollection().AddExpression(CustomNormal);
  UMaterialExpressionTransform *TransformNormal =
      NewObject<UMaterialExpressionTransform>(BaseMat);
  TransformNormal->TransformSourceType =
      EMaterialVectorCoordTransformSource::TRANSFORMSOURCE_World;
  TransformNormal->TransformType =
      EMaterialVectorCoordTransform::TRANSFORM_Tangent;
  TransformNormal->Input.Expression = CustomNormal;
  BaseMat->GetExpressionCollection().AddExpression(TransformNormal);
  BaseMat->GetEditorOnlyData()->Normal.Expression = TransformNormal;

  // Set base color to a neutral gray
  UMaterialExpressionConstant3Vector *BaseColor =
      NewObject<UMaterialExpressionConstant3Vector>(BaseMat);
  BaseColor->Constant = FLinearColor(0.5f, 0.5f, 0.5f);
  BaseMat->GetExpressionCollection().AddExpression(BaseColor);
  BaseMat->GetEditorOnlyData()->BaseColor.Expression = BaseColor;

  // Material properties for instancing
  BaseMat->bUsedWithInstancedStaticMeshes = true;

  // Compile and save
  BaseMat->PreEditChange(nullptr);
  BaseMat->PostEditChange();

  FAssetRegistryModule::AssetCreated(BaseMat);
  Package->MarkPackageDirty();

  FSavePackageArgs SaveArgs;
  SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
  UPackage::SavePackage(
      Package, BaseMat,
      *FPackageName::LongPackageNameToFilename(
          PackagePath, FPackageName::GetAssetPackageExtension()),
      SaveArgs);

  UE_LOG(LogTemp, Log, TEXT("Created base VAT material: %s"), *PackagePath);
  CachedBaseMat = BaseMat;
  return BaseMat;
}

#undef LOCTEXT_NAMESPACE
