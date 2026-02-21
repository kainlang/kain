// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelTools/Tools/VoxelMeshTool.h"

#include "VoxelTools/VoxelDataTools.inl"
#include "VoxelTools/VoxelAssetTools.inl"
#include "VoxelData/VoxelData.h"
#include "VoxelUtilities/VoxelSDFUtilities.h"
#include "VoxelUtilities/VoxelExampleUtilities.h"
#include "VoxelWorld.h"

#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"

UVoxelMeshTool::UVoxelMeshTool()
{
	ToolName = TEXT("Mesh");
	bShowPaintMaterial = false;
	
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ToolMaterialFinder(TEXT("/Voxel/ToolMaterials/ToolMeshMaterial_Mesh"));
	ToolMaterial = ToolMaterialFinder.Object;
	
	ColorsMaterial = FVoxelExampleUtilities::LoadExampleObject<UMaterialInterface>(TEXT("/Voxel/Examples/Importers/Chair/VoxelExample_M_Chair_Emissive_Color"));
	UVsMaterial = FVoxelExampleUtilities::LoadExampleObject<UMaterialInterface>(TEXT("/Voxel/Examples/Importers/Chair/VoxelExample_M_Chair_Emissive_UVs"));
	Mesh = FVoxelExampleUtilities::LoadExampleObject<UStaticMesh>(TEXT("/Voxel/Examples/Importers/Chair/VoxelExample_SM_Chair"));
}

void UVoxelMeshTool::GetToolConfig(FVoxelToolBaseConfig& OutConfig) const
{
	Super::GetToolConfig(OutConfig);

	OutConfig.MeshMaterial = ToolMaterial;
	OutConfig.Stride = Stride;
}

void UVoxelMeshTool::UpdateRender(UMaterialInstanceDynamic* OverlayMaterialInstance, UMaterialInstanceDynamic* MeshMaterialInstance)
{
	VOXEL_FUNCTION_COUNTER();
	
	if (!MeshMaterialInstance)
	{
		return;
	}
	
	auto* MeshData = GetMeshData();
	if (!MeshData)
	{
		return;
	}

	FVector MeshScale;
	FTransform TransformNoTranslation;
	FTransform TransformWithTranslation;
	GetTransform(*MeshData, MeshScale, TransformNoTranslation, TransformWithTranslation);
	
	MeshMaterialInstance->SetScalarParameterValue(STATIC_FNAME("Opacity"), SharedConfig->ToolOpacity);
	
	UpdateToolMesh(
		Mesh,
		MeshMaterialInstance,
		TransformWithTranslation);
}

FVoxelIntBoxWithValidity UVoxelMeshTool::DoEdit()
{
	VOXEL_FUNCTION_COUNTER();

	auto* MeshData = GetMeshData();
	if (!MeshData)
	{
		return {};
	}

	FVector MeshScale;
	FTransform TransformNoTranslation;
	FTransform TransformWithTranslation;
	GetTransform(*MeshData, MeshScale, TransformNoTranslation, TransformWithTranslation);

	auto& World = *GetVoxelWorld();
	const bool bAlternativeMode = GetTickData().IsAlternativeMode();

	const auto MaterialConfig = World.MaterialConfig;

	const uint8 MeshSettings_U0 = FVoxelUtilities::FloatToUINT8(UV0ToPaint.X);
	const uint8 MeshSettings_V0 = FVoxelUtilities::FloatToUINT8(UV0ToPaint.Y);
	const uint8 MeshSettings_U1 = FVoxelUtilities::FloatToUINT8(UV1ToPaint.X);
	const uint8 MeshSettings_V1 = FVoxelUtilities::FloatToUINT8(UV1ToPaint.Y);
	
	const auto GetMaterialImpl = [&](const float OldValue, const float NewValue, const FVoxelMaterial OldMaterial, bool bAllowMeshImport, const FVoxelMaterial InstanceMaterial)
	{
		if ((bAlternativeMode ? OldValue >= NewValue : OldValue <= NewValue) || NewValue > 0)
		{
			return OldMaterial;
		}

		FVoxelMaterial NewMaterial = OldMaterial;
		if (bPaintColors)
		{
			const FColor Color = bImportColorsFromMesh && bAllowMeshImport ? InstanceMaterial.GetColor() : ColorToPaint;
			if (MaterialConfig == EVoxelMaterialConfig::RGB)
			{
				NewMaterial.SetR(Color.R);
				NewMaterial.SetG(Color.G);
				NewMaterial.SetB(Color.B);
				NewMaterial.SetA(Color.A);
			}
			else if (MaterialConfig == EVoxelMaterialConfig::SingleIndex)
			{
				NewMaterial.SetR(Color.R);
				NewMaterial.SetG(Color.G);
				NewMaterial.SetB(Color.B);
			}
		}
		if (bPaintUVs && MaterialConfig != EVoxelMaterialConfig::MultiIndex)
		{
			NewMaterial.SetU0(bImportUVsFromMesh && bAllowMeshImport ? InstanceMaterial.GetU0() : MeshSettings_U0);
			NewMaterial.SetU1(bImportUVsFromMesh && bAllowMeshImport ? InstanceMaterial.GetU1() : MeshSettings_U1);
			NewMaterial.SetV0(bImportUVsFromMesh && bAllowMeshImport ? InstanceMaterial.GetV0() : MeshSettings_V0);
			NewMaterial.SetV1(bImportUVsFromMesh && bAllowMeshImport ? InstanceMaterial.GetV1() : MeshSettings_V1);
		}
		if (bPaintIndex && MaterialConfig == EVoxelMaterialConfig::SingleIndex)
		{
			NewMaterial.SetSingleIndex(IndexToPaint);
		}
		if (bPaintIndex && MaterialConfig == EVoxelMaterialConfig::MultiIndex)
		{
			NewMaterial.SetMultiIndex_Index0(IndexToPaint);
			NewMaterial.SetMultiIndex_Blend0_AsFloat(0);
			NewMaterial.SetMultiIndex_Blend1_AsFloat(0);
			NewMaterial.SetMultiIndex_Blend2_AsFloat(0);
		}

		auto Result = OldMaterial;
		Result.CopyFrom(NewMaterial, PaintMask);
		return Result;
	};

	if (bSmoothImport || bProgressiveStamp)
	{
		const float ActualSmoothness = Smoothness * MeshScale.GetMax() * MeshData->Bounds.GetSize().GetMax() / World.VoxelSize;
		const float BoxExtension = bProgressiveStamp ? 0 : (ActualSmoothness + 4);
		
		// Fixup import settings
		MeshImporterSettings.VoxelSize = World.VoxelSize;
		
		if (!DistanceFieldData.IsValid() ||
			!DistanceFieldData->Transform.Equals(TransformNoTranslation) ||
			DistanceFieldData->BoxExtension != BoxExtension ||
			DistanceFieldData->ImporterSettings != MeshImporterSettings)
		{
			auto NewDistanceFieldData = MakeUnique<FDistanceFieldData>();

			NewDistanceFieldData->Transform = TransformNoTranslation;
			NewDistanceFieldData->BoxExtension = BoxExtension;
			NewDistanceFieldData->ImporterSettings = MeshImporterSettings;
			
			int32 NumLeaks;
			UVoxelMeshImporterLibrary::ConvertMeshToDistanceField(
				MeshData->Data,
				TransformNoTranslation,
				MeshImporterSettings,
				BoxExtension,
				NewDistanceFieldData->Data,
				NewDistanceFieldData->SurfacePositions,
				NewDistanceFieldData->Size,
				NewDistanceFieldData->PositionOffset,
				NumLeaks,
				SharedConfig->bMultiThreaded);

			if (SharedConfig->bDebug)
			{
				GEngine->AddOnScreenDebugMessage(
					OBJECT_LINE_ID(),
					1.5f * GetDeltaTime(),
					FColor::Yellow,
					"Converting mesh to distance field");
			}

			DistanceFieldData = MoveTemp(NewDistanceFieldData);
		}
		check(DistanceFieldData.IsValid());

		const FIntVector VoxelPosition = World.GlobalToLocal(GetToolPosition()) + DistanceFieldData->PositionOffset;
		const FVoxelIntBox Bounds = FVoxelIntBox(VoxelPosition, VoxelPosition + DistanceFieldData->Size);
		if (!Bounds.IsValid())
		{
			return {};
		}
		const auto BoundsToCache = GetBoundsToCache(Bounds);

		auto& Data = World.GetData();
		auto DataImpl = GetDataImpl(Data);

		FVoxelWriteScopeLock Lock(Data, BoundsToCache.Extend(1) /* See MergeDistanceFieldImpl */, FUNCTION_FNAME);
		CacheData<FVoxelValue>(Data, BoundsToCache);
		
		const FIntVector Size = DistanceFieldData->Size;
		const auto& DistanceField = DistanceFieldData->Data;

		FVoxelDebug::Broadcast("MeshDistances", Bounds.Size(), DistanceField);

		const auto GetSDF = FVoxelUtilities::Create3DGetter(DistanceField, Size, Bounds.Min);

		const auto MergeSDF = [&](float A, float B) -> float
		{
			if (bProgressiveStamp)
			{
				if (bAlternativeMode)
				{
					return FMath::Lerp(A, FMath::Max(A, -B), Speed);
				}
				else
				{
					return FMath::Lerp(A, FMath::Min(A, B), Speed);
				}
			}
			else
			{
				ensureVoxelSlow(bSmoothImport);
				if (bAlternativeMode)
				{
					return FVoxelSDFUtilities::opSmoothSubtraction(B, A, ActualSmoothness);
				}
				else
				{
					return FVoxelSDFUtilities::opSmoothUnion(B, A, ActualSmoothness);
				}
			}
		};
		const auto GetMaterial = [&](float OldValue, float NewValue, FVoxelMaterial PreviousMaterial)
		{
			return GetMaterialImpl(OldValue, NewValue, PreviousMaterial, false, {});
		};
		
		UVoxelDataTools::MergeDistanceFieldImpl(Data, Bounds, GetSDF, MergeSDF, SharedConfig->bMultiThreaded, bPaint, GetMaterial);

		return Bounds;
	}
	else
	{
		FVoxelMeshImporterSettings ActualImporterSettings(MeshImporterSettings);
		ActualImporterSettings.VoxelSize = World.VoxelSize;

		ActualImporterSettings.bImportColors = bPaint && bPaintColors && ColorsMaterial && World.MaterialConfig != EVoxelMaterialConfig::MultiIndex;
		ActualImporterSettings.ColorsMaterial = ColorsMaterial;

		ActualImporterSettings.bImportUVs = bPaint && bPaintUVs && UVsMaterial && World.MaterialConfig != EVoxelMaterialConfig::MultiIndex;
		ActualImporterSettings.UVsMaterial = UVsMaterial;

		ActualImporterSettings.RenderTargetSize = RenderTargetSize;
		
		if (!AssetData.IsValid() ||
			!TransformNoTranslation.Equals(AssetData->Transform) ||
			ActualImporterSettings != AssetData_ImporterSettings)
		{
			auto NewAssetData = MakeUnique<FAssetData>();

			NewAssetData->Transform = TransformNoTranslation;
			AssetData_ImporterSettings = ActualImporterSettings;

			int32 NumLeaks;
			UVoxelMeshImporterLibrary::ConvertMeshToVoxels(
				GetVoxelWorld(),
				MeshData->Data,
				TransformNoTranslation,
				ActualImporterSettings,
				RenderTargetCache,
				NewAssetData->Data,
				NewAssetData->PositionOffset,
				NumLeaks);

			ColorsRenderTarget = RenderTargetCache.ColorsRenderTarget;
			UVsRenderTarget = RenderTargetCache.UVsRenderTarget;

			if (SharedConfig->bDebug)
			{
				GEngine->AddOnScreenDebugMessage(
					OBJECT_LINE_ID(),
					1.5f * GetDeltaTime(),
					FColor::Yellow,
					"Converting mesh to voxels");
			}

			AssetData = MoveTemp(NewAssetData);
		}
		check(AssetData.IsValid());

		const FVoxelVector VoxelPosition = World.GlobalToLocalFloat(GetToolPosition()) + FVector(AssetData->PositionOffset);
		const FVoxelIntBox Bounds = FVoxelIntBox(VoxelPosition, VoxelPosition + FVector(AssetData->Data.GetSize()));
		if (!Bounds.IsValid())
		{
			return {};
		}
		const auto BoundsToCache = GetBoundsToCache(Bounds);

		auto& Data = World.GetData();
		auto DataImpl = GetDataImpl(Data);

		FVoxelWriteScopeLock Lock(Data, BoundsToCache, FUNCTION_FNAME);
		CacheData<FVoxelValue>(Data, BoundsToCache);

		const auto GetValue = [&] (float OldValue, float InstanceValue)
		{
			return bAlternativeMode ? FMath::Max(OldValue, -InstanceValue) : FMath::Min(OldValue, InstanceValue);
		};
		const auto GetMaterial = [&](const float OldValue, const float NewValue, const FVoxelMaterial OldMaterial, const float InstanceValue, const FVoxelMaterial InstanceMaterial)
		{
			return GetMaterialImpl(OldValue, NewValue, OldMaterial, true, InstanceMaterial);
		};
		// Note: bSubtractive needs to be false here as we are manually inverting the asset above
		UVoxelAssetTools::ImportDataAssetImpl(DataImpl, VoxelPosition, AssetData->Data, false, GetValue, bPaint, GetMaterial);

		return Bounds;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

const UVoxelMeshTool::FMeshData* UVoxelMeshTool::GetMeshData()
{
	VOXEL_FUNCTION_COUNTER();
	
	if (!Mesh)
	{
		return nullptr;
	}

	if (CachedMeshData.IsValid() && CachedMeshData->StaticMesh != Mesh)
	{
		CachedMeshData.Reset();
		AssetData.Reset();
		DistanceFieldData.Reset();
	}
	
	if (!CachedMeshData.IsValid())
	{
		auto MeshData = MakeUnique<FMeshData>();
		MeshData->StaticMesh = Mesh;
		UVoxelMeshImporterLibrary::CreateMeshDataFromStaticMesh(Mesh, MeshData->Data);

		MeshData->Bounds = FBox(ForceInit);
		for (auto& Vertex : MeshData->Data.Vertices)
		{
			MeshData->Bounds += Vertex;
		}

		if (MeshData->Data.Vertices.Num() == 0)
		{
			FVoxelMessages::Error("Mesh Tool: Failed to extract mesh data!");
		}

		CachedMeshData = MoveTemp(MeshData);
	}
	check(CachedMeshData.IsValid());

	if (CachedMeshData->Data.Vertices.Num() > 0)
	{
		return CachedMeshData.Get();
	}
	else
	{
		return nullptr;
	}
}

void UVoxelMeshTool::GetTransform(
	const FMeshData& MeshData, 
	FVector& OutMeshScale, 
	FTransform& OutTransformNoTranslation, 
	FTransform& OutTransformWithTranslation) const
{
	VOXEL_FUNCTION_COUNTER();
	
	OutMeshScale =
		bAbsoluteScale
		? Scale
		: SharedConfig->BrushSize * Scale / MeshData.Bounds.GetSize().GetMax();

	const auto GetRotationMatrix = [&]()
	{
		const FVector X = bAlignToMovement ? GetToolDirection() : FVector::ForwardVector;
		const FVector Z = bAlignToNormal ? GetToolNormal() : FVector::UpVector;
		const FVector Y = (Z ^ X).GetSafeNormal();
		return FMatrix(Y ^ Z, Y, Z, FVector(0));
	};

	// Matrix and Transform multiplications are left to right!

	const FMatrix ScaleMatrix = FScaleMatrix(OutMeshScale);
	const FVector ScaledPositionOffset = PositionOffset * ScaleMatrix.TransformVector(MeshData.Bounds.GetSize());

	OutTransformNoTranslation = FTransform(
		ScaleMatrix *
		FTranslationMatrix(ScaledPositionOffset) *
		FRotationMatrix(RotationOffset) *
		GetRotationMatrix());
	
	OutTransformWithTranslation = OutTransformNoTranslation * FTransform(GetToolPreviewPosition());
}