// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelTools/Tools/VoxelSurfaceTool.h"
#include "VoxelWorld.h"
#include "VoxelMessages.h"
#include "VoxelUtilities/VoxelExampleUtilities.h"
#include "VoxelGenerators/VoxelGeneratorTools.h"
#include "VoxelTools/Impl/VoxelSurfaceEditToolsImpl.inl"
#include "VoxelTools/VoxelSurfaceTools.h"
#include "VoxelTools/VoxelSurfaceToolsImpl.h"
#include "VoxelTools/VoxelBlueprintLibrary.h"
#include "VoxelTools/VoxelHardnessHandler.h"

#include "UObject/ConstructorHelpers.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"

UVoxelSurfaceTool::UVoxelSurfaceTool()
{
	ToolName = TEXT("Surface");
	bShowPaintMaterial = true;
	
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ToolMaterialFinder(TEXT("/Voxel/ToolMaterials/ToolRenderingMaterial_Surface"));
	ToolMaterial = ToolMaterialFinder.Object;
	Mask.Texture = FVoxelExampleUtilities::LoadExampleObject<UTexture2D>(TEXT("/Voxel/Examples/VoxelDefaultBrushMask"));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void UVoxelSurfaceTool::GetToolConfig(FVoxelToolBaseConfig& OutConfig) const
{
	OutConfig.Stride = Stride;
	
	OutConfig.OverlayMaterial = ToolMaterial;

	OutConfig.bUseFixedDirection = !bAlignToMovement;
	OutConfig.FixedDirection = FixedDirection;

	OutConfig.bUseFixedNormal = b2DBrush;
	OutConfig.FixedNormal = FVector::UpVector;
}

void UVoxelSurfaceTool::Tick()
{
	Super::Tick();
	
	Falloff = GetValueAfterAxisInput(FVoxelToolAxes::Falloff, Falloff);
	if (bSculpt)
	{
		SculptStrength = GetValueAfterAxisInput(FVoxelToolAxes::Strength, SculptStrength);
	}
	else if (bPaint)
	{
		PaintStrength = GetValueAfterAxisInput(FVoxelToolAxes::Strength, PaintStrength);
	}
}

void UVoxelSurfaceTool::UpdateRender(UMaterialInstanceDynamic* OverlayMaterialInstance, UMaterialInstanceDynamic* MeshMaterialInstance)
{
	VOXEL_FUNCTION_COUNTER();

	if (!OverlayMaterialInstance)
	{
		return;
	}

	const float VoxelSize = GetVoxelWorld()->VoxelSize;

	if (ShouldUseMask())
	{
		TVoxelTexture<float> MaskTexture;
		float MaskScaleX = 1;
		float MaskScaleY = 1;
		if (GetMaskData(false, MaskTexture, MaskScaleX, MaskScaleY))
		{
			FVector MaskPlaneX;
			FVector MaskPlaneY;
			FVoxelSurfaceToolsImpl::GetStrengthMaskBasisImpl(GetToolNormal(), GetToolDirection(), MaskPlaneX, MaskPlaneY);
				
			OverlayMaterialInstance->SetVectorParameterValue(STATIC_FNAME("MaskX"), MaskPlaneX);
			OverlayMaterialInstance->SetVectorParameterValue(STATIC_FNAME("MaskY"), MaskPlaneY);
			OverlayMaterialInstance->SetVectorParameterValue(STATIC_FNAME("MaskUp"), GetToolNormal());

			{
				const FVector TextureMaskScale =
					VoxelSize *
					FVector(
						MaskScaleX * MaskTexture.GetSizeX(),
						MaskScaleY * MaskTexture.GetSizeY(),
						0);
				OverlayMaterialInstance->SetVectorParameterValue(STATIC_FNAME("MaskScale"), TextureMaskScale);
			}

			{
				FVector4 MaskChannelVector = FVector4(ForceInit);
				MaskChannelVector[int32(Mask.Type == EVoxelSurfaceToolMaskType::Texture ? Mask.Channel : EVoxelRGBA::R)] = 1;
				OverlayMaterialInstance->SetVectorParameterValue(STATIC_FNAME("MaskChannel"), FLinearColor(MaskChannelVector));
			}

			OverlayMaterialInstance->SetTextureParameterValue(STATIC_FNAME("MaskTexture"), Mask.Type == EVoxelSurfaceToolMaskType::Texture ? Mask.Texture : MaskGeneratorCache_RenderTexture);
		}
	}

	const float Radius = SharedConfig->BrushSize / 2.f;
	OverlayMaterialInstance->SetScalarParameterValue(STATIC_FNAME("Radius"), Radius);
	OverlayMaterialInstance->SetVectorParameterValue(STATIC_FNAME("Position"), GetToolPreviewPosition());
	OverlayMaterialInstance->SetScalarParameterValue(STATIC_FNAME("Falloff"), Falloff);
	OverlayMaterialInstance->SetScalarParameterValue(STATIC_FNAME("EnableFalloff"), bEnableFalloff);
	OverlayMaterialInstance->SetScalarParameterValue(STATIC_FNAME("FalloffType"), int32(FalloffType));

	float SignedSculptStrength;
	float SignedPaintStrength;
	GetStrengths(SignedSculptStrength, SignedPaintStrength);
	OverlayMaterialInstance->SetScalarParameterValue(STATIC_FNAME("SculptHeight"),  bSculpt ? SignedSculptStrength * VoxelSize : 0.f);

	OverlayMaterialInstance->SetScalarParameterValue(STATIC_FNAME("UseMask"), ShouldUseMask());
	OverlayMaterialInstance->SetScalarParameterValue(STATIC_FNAME("2DBrush"), b2DBrush);

	OverlayMaterialInstance->SetScalarParameterValue(STATIC_FNAME("Opacity"), SharedConfig->ToolOpacity);

	SetToolOverlayBounds(FBox(GetToolPreviewPosition() - Radius, GetToolPreviewPosition() + Radius));
}

FVoxelIntBoxWithValidity UVoxelSurfaceTool::DoEdit()
{
	VOXEL_FUNCTION_COUNTER();
	
	constexpr float DistanceDivisor = 4.f;
	
	float SignedSculptStrength;
	float SignedPaintStrength;
	GetStrengths(SignedSculptStrength, SignedPaintStrength);

	const float Radius = SharedConfig->BrushSize / 2.f;
	const FVoxelIntBox BoundsToDoEditsIn = UVoxelBlueprintLibrary::MakeIntBoxFromGlobalPositionAndRadius(GetVoxelWorld(), GetToolPosition(), Radius);
	const FVoxelIntBox BoundsWhereEditsHappen =
		b2DBrush
		? BoundsToDoEditsIn.Extend(FIntVector(0, 0, FMath::CeilToInt(FMath::Abs(SignedSculptStrength) + DistanceDivisor + 2)))
		: BoundsToDoEditsIn;
	
	if (!BoundsToDoEditsIn.IsValid() || !BoundsWhereEditsHappen.IsValid())
	{
		FVoxelMessages::Error("Invalid tool bounds!", this);
		return {};
	}
	
	// Don't cache the entire column
	const auto BoundsToCache = GetBoundsToCache(BoundsToDoEditsIn);

	FVoxelData& Data = GetVoxelWorld()->GetData();
	auto DataImpl = GetDataImpl(Data);

	FVoxelWriteScopeLock Lock(Data, BoundsWhereEditsHappen.Union(BoundsToCache), FUNCTION_FNAME);
	CacheData<FVoxelValue>(Data, BoundsToCache);
	
	FVoxelSurfaceEditsVoxels Voxels;
	if (b2DBrush)
	{
		Voxels = UVoxelSurfaceTools::FindSurfaceVoxels2DImpl(Data, BoundsToDoEditsIn, false);
	}
	else
	{
		if (bSculpt)
		{
			Voxels = UVoxelSurfaceTools::FindSurfaceVoxelsFromDistanceFieldImpl(Data, BoundsToDoEditsIn, SharedConfig->bMultiThreaded);
		}
		else
		{
			// No need to compute the distance field for paint
			// Only select voxels inside the surface
			Voxels = UVoxelSurfaceTools::FindSurfaceVoxelsImpl(Data, BoundsToDoEditsIn, false, true);
		}
	}

	FVoxelSurfaceEditsStack Stack;
	
	if (bEnableFalloff)
	{
		Stack.Add(UVoxelSurfaceTools::ApplyFalloff(
			GetVoxelWorld(),
			FalloffType,
			GetToolPosition(),
			Radius,
			Falloff));
	}

	if (ShouldUseMask())
	{
		TVoxelTexture<float> MaskTexture;
		float MaskScaleX = 1;
		float MaskScaleY = 1;
		if (GetMaskData(true, MaskTexture, MaskScaleX, MaskScaleY))
		{
			Stack.Add(UVoxelSurfaceTools::ApplyStrengthMask(
				GetVoxelWorld(),
				MaskTexture,
				GetToolPosition(),
				MaskScaleX,
				MaskScaleY,
				GetToolNormal(),
				GetToolDirection(),
				EVoxelSamplerMode::Tile));
		}
	}

	const FVoxelHardnessHandler HardnessHandler(*GetVoxelWorld());
	
	FVoxelSurfaceEditsProcessedVoxels ProcessedVoxels;
	if (bSculpt && bPaint)
	{
		auto SculptStack = Stack;
		SculptStack.Add(UVoxelSurfaceTools::ApplyConstantStrength(-SignedSculptStrength));
		ProcessedVoxels = SculptStack.Execute(Voxels, false);

		auto RecordingDataImpl = GetDataImpl<FModifiedVoxelValue>(Data);
		FVoxelSurfaceEditToolsImpl::EditVoxelValues(DataImpl, HardnessHandler, BoundsWhereEditsHappen, ProcessedVoxels, DistanceDivisor);
		
		TArray<FVoxelSurfaceEditsVoxel> Materials;
		Materials.Reserve(RecordingDataImpl.ModifiedValues.Num());
		for (auto& Voxel : RecordingDataImpl.ModifiedValues)
		{
			if (Voxel.OldValue > 0 && Voxel.NewValue <= 0)
			{
				FVoxelSurfaceEditsVoxel NewVoxel;
				NewVoxel.Position = Voxel.Position;
				NewVoxel.Strength = PaintStrength;
				Materials.Add(NewVoxel);
			}
		}
		FVoxelSurfaceEditToolsImpl::EditVoxelMaterials(DataImpl, BoundsWhereEditsHappen, SharedConfig->PaintMaterial, Materials);
	}
	else if (bSculpt)
	{
		auto SculptStack = Stack;
		SculptStack.Add(UVoxelSurfaceTools::ApplyConstantStrength(-SignedSculptStrength));
		ProcessedVoxels = SculptStack.Execute(Voxels, false);

		if (bPropagateMaterials && !b2DBrush)
		{
			FVoxelSurfaceEditToolsImpl::PropagateVoxelMaterials(DataImpl, ProcessedVoxels);
		}
		
		FVoxelSurfaceEditToolsImpl::EditVoxelValues(DataImpl, HardnessHandler, BoundsWhereEditsHappen, ProcessedVoxels, DistanceDivisor);
	}
	else if (bPaint)
	{
		// Note: Painting behaves the same with 2D edit on/off
		auto PaintStack = Stack;
		PaintStack.Add(UVoxelSurfaceTools::ApplyConstantStrength(SignedPaintStrength));
		ProcessedVoxels = PaintStack.Execute(Voxels, false);
		
		FVoxelSurfaceEditToolsImpl::EditVoxelMaterials(DataImpl, BoundsWhereEditsHappen, SharedConfig->PaintMaterial, *ProcessedVoxels.Voxels);
	}

	if (SharedConfig->bDebug)
	{
		UVoxelSurfaceTools::DebugSurfaceVoxels(GetVoxelWorld(), ProcessedVoxels, Stride > 0 ? 1 : 2 * GetDeltaTime());
	}

	return BoundsWhereEditsHappen;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool UVoxelSurfaceTool::GetMaskData(bool bShowNotification, TVoxelTexture<float>& OutTexture, float& OutScaleX, float& OutScaleY)
{
	if (SharedConfig->BrushSize <= 0) 
	{
		return false;
	}

	const float VoxelSize = GetVoxelWorld()->VoxelSize;

	if (Mask.Type == EVoxelSurfaceToolMaskType::Texture)
	{
		FString Error;
		if (!FVoxelTextureUtilities::CanCreateFromTexture(Mask.Texture, Error))
		{
			if (bShowNotification)
			{
				FVoxelMessages::FNotification Notification;
				Notification.UniqueId = OBJECT_LINE_ID();
				Notification.Message = Error;
				
				auto& Button = Notification.Buttons.Emplace_GetRef();
				Button.Text = "Fix Now";
				Button.Tooltip = "Fix the texture";
				Button.OnClick = FSimpleDelegate::CreateWeakLambda(Mask.Texture.Get(), [Mask = Mask.Texture]() { FVoxelTextureUtilities::FixTexture(Mask); });

				FVoxelMessages::ShowNotification(Notification);
			}
			return false;
		}
		OutTexture = FVoxelTextureUtilities::CreateFromTexture_Float(Mask.Texture, Mask.Channel);

		const float MaskWantedSize = SharedConfig->BrushSize / VoxelSize * Mask.Scale;
		OutScaleX = MaskWantedSize / OutTexture.GetSizeX();
		OutScaleY = MaskWantedSize / OutTexture.GetSizeY() * Mask.Ratio;

		return true;
	}
	else
	{
		bool bNeedToRebuild = false;
		bool bNeedToRebuildSeeds = false;
		if (LastFrameCanEdit() && LastFrameCanEdit() != CanEdit())
		{
			// Reset if we stopped clicking, or if stride edit just ended
			bNeedToRebuild = true;
			bNeedToRebuildSeeds = true;
		}
		if (!MaskGeneratorCache.CachedConfig.HasSameGeneratorSettings(Mask) ||
			(Mask.bScaleWithBrushSize && MaskGeneratorCache.BrushSize != SharedConfig->BrushSize))
		{
			bNeedToRebuild = true;
		}
		ensure(!bNeedToRebuildSeeds || bNeedToRebuild);

		if (bNeedToRebuild)
		{
			MaskGeneratorCache.CachedConfig = Mask;
			MaskGeneratorCache.BrushSize = SharedConfig->BrushSize;
			
			const int32 Size = SharedConfig->BrushSize / VoxelSize;

			if (bNeedToRebuildSeeds || MaskGeneratorCache.Seeds.Num() == 0)
			{
				MaskGeneratorCache.Seeds.Reset();
				for (auto& SeedName : Mask.SeedsToRandomize)
				{
					MaskGeneratorCache.Seeds.Add(SeedName, FMath::Rand());
				}
			}

			auto Picker = Mask.Generator;
			for (auto& It : MaskGeneratorCache.Seeds)
			{
				Picker.Parameters.Add(It.Key, LexToString(It.Value));
			}
			auto* GeneratorInstance = UVoxelGeneratorTools::MakeGeneratorInstance(Picker.GetGenerator(), GetVoxelWorld()->GetGeneratorInit());

			FVoxelFloatTexture FloatTexture;
			UVoxelGeneratorTools::CreateFloatTextureFromGenerator(
				FloatTexture,
				GeneratorInstance,
				"Value",
				Size,
				Size,
				Mask.Scale * (Mask.bScaleWithBrushSize ? 100.f / (SharedConfig->BrushSize / 2.f / VoxelSize) : 1.f),
				-Size / 2, -Size / 2);
			
			// We want the data to be between 0 and 1 for sculpt strength to be coherent
			MaskGeneratorCache.VoxelTexture = FVoxelTextureUtilities::Normalize(FloatTexture.Texture);

			const auto ColorTexture = FVoxelTextureUtilities::CreateColorTextureFromFloatTexture(MaskGeneratorCache.VoxelTexture, EVoxelRGBA::R, false);

			UTexture2D* InOutTexture = MaskGeneratorCache_RenderTexture.Get();
			FVoxelTextureUtilities::CreateOrUpdateUTexture2D(ColorTexture, InOutTexture);
			MaskGeneratorCache_RenderTexture = InOutTexture;
		}

		Mask.GeneratorDebugTexture = MaskGeneratorCache_RenderTexture;

		OutTexture = MaskGeneratorCache.VoxelTexture;
		OutScaleX = 1.f;
		OutScaleY = Mask.Ratio;
		
		return true;
	}
}

bool UVoxelSurfaceTool::ShouldUseMask() const
{
	return bUseMask && (Mask.Type == EVoxelSurfaceToolMaskType::Texture ? Mask.Texture != nullptr : Mask.Generator.IsValid());
}

void UVoxelSurfaceTool::GetStrengths(float& OutSignedSculptStrength, float& OutSignedPaintStrength) const
{
	const bool bIsStrideEnabled = Stride != 0;
	const bool bUseDeltaTimeForSculpt = bModulateStrengthByDeltaTime && !bIsStrideEnabled;
	const bool bUseDeltaTimeForPaint = bUseDeltaTimeForSculpt && PaintStrength < 1.f;

	const float MovementStrengthMultiplier = bMovementAffectsStrength ? GetMouseMovementSize() / 100 : 1;
	const float RadiusMultiplier = bIsStrideEnabled ? SharedConfig->BrushSize / 2.f / GetVoxelWorld()->VoxelSize : 1.f;

	// Default paint/sculpt strengths are too low to feel good
	const float SculptStrengthStaticMultiplier = bIsStrideEnabled ? 1.f : 50.f;
	const float PaintStrengthStaticMultiplier = 10.f;
	
	const float ActualSculptStrength = SculptStrength * MovementStrengthMultiplier * (bUseDeltaTimeForSculpt ? GetDeltaTime() : 1.f) * SculptStrengthStaticMultiplier * RadiusMultiplier;
	const float ActualPaintStrength = PaintStrength * MovementStrengthMultiplier * (bUseDeltaTimeForPaint ? GetDeltaTime() : 1.f) * PaintStrengthStaticMultiplier;

	const bool bAlternativeMode = GetTickData().IsAlternativeMode();
	OutSignedSculptStrength = ActualSculptStrength * (bAlternativeMode ? -1 : 1);
	OutSignedPaintStrength = ActualPaintStrength * (bAlternativeMode ? -1 : 1);
}