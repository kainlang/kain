// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelGraphPreview.h"
#include "VoxelValue.h"
#include "VoxelMaterial.h"
#include "VoxelGraphEditor.h"
#include "VoxelGraphGenerator.h"
#include "VoxelGraphPreviewSettings.h"
#include "VoxelGraphErrorReporter.h"
#include "VoxelDebug/VoxelLineBatchComponent.h"
#include "Runtime/VoxelGraph.inl"
#include "Runtime/VoxelGraphGeneratorInstance.h"
#include "Runtime/Recorders/VoxelGraphStatsRecorder.h"
#include "Runtime/Recorders/VoxelGraphRangeAnalysisRecorder.h"
#include "VoxelData/VoxelDataIncludes.h"
#include "VoxelUtilities/VoxelMaterialUtilities.h"
#include "VoxelUtilities/VoxelTextureUtilities.h"
#include "VoxelUtilities/VoxelThreadingUtilities.h"
#include "VoxelUtilities/VoxelDistanceFieldUtilities.h"
#include "VoxelPlaceableItems/VoxelPlaceableItemManager.h"
#include "VoxelGenerators/VoxelGeneratorCache.h"
#include "VoxelWorldInterface.h"

#include "SVoxelGraphPreview.h"
#include "SVoxelGraphPreviewViewport.h"

#include "Misc/MessageDialog.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/StaticMeshComponent.h"
#include "AdvancedPreviewScene.h"
#include "Engine/Texture2D.h"
#include "Kismet/KismetMathLibrary.h"

FVoxelGraphPreview::FVoxelGraphPreview(
		UVoxelGraphGenerator* Generator,
		const TSharedPtr<SVoxelGraphPreview>& Preview,
		const TSharedPtr<SVoxelGraphPreviewViewport>& PreviewViewport,
		const TSharedPtr<FAdvancedPreviewScene>& PreviewScene)
	: Generator(Generator)
	, Preview(Preview)
	, PreviewViewport(PreviewViewport)
	, PreviewScene(PreviewScene)
{
	check(Generator && Generator->PreviewSettings);
	
	PreviewScene->SetLightBrightness(0.f);
	PreviewScene->SetFloorVisibility(false, true);
	PreviewScene->SetEnvironmentVisibility(false, true);
	PreviewScene->SetSkyBrightness(0.f);
		
	PreviewSceneFloor = NewObject<UStaticMeshComponent>();
	LineBatchComponent = NewObject<UVoxelLineBatchComponent>();
	
	PreviewScene->AddComponent(PreviewSceneFloor, FTransform::Identity);
	PreviewScene->AddComponent(LineBatchComponent, FTransform::Identity);
}

void FVoxelGraphPreview::Update(EVoxelGraphPreviewFlags Flags)
{
	if (EnumHasAnyFlags(Flags, EVoxelGraphPreviewFlags::UpdatePlaceableItems))
	{
		Flags |= EVoxelGraphPreviewFlags::UpdateTextures;
	}
	if (EnumHasAnyFlags(Flags, EVoxelGraphPreviewFlags::UpdateTextures))
	{
		Flags |= EVoxelGraphPreviewFlags::UpdateMeshSettings;
	}
	
	if (EnumHasAnyFlags(Flags, EVoxelGraphPreviewFlags::UpdateTextures))
	{
		UpdateTextures(Flags);
	}
	if (EnumHasAnyFlags(Flags, EVoxelGraphPreviewFlags::UpdateMeshSettings))
	{
		UpdateMaterialParameters();
	}
	
	PreviewViewport->RefreshViewport();
}

void FVoxelGraphPreview::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(PreviewSceneFloor);
	Collector.AddReferencedObject(LineBatchComponent);
	
	Collector.AddReferencedObject(HeightmapMaterial);
	Collector.AddReferencedObject(SliceMaterial);
	
	Collector.AddReferencedObject(DensitiesTexture);
	Collector.AddReferencedObject(MaterialsTexture);
	Collector.AddReferencedObject(MaterialsTextureWithCrossAndNoAlpha);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelGraphPreview::UpdateTextures(EVoxelGraphPreviewFlags Flags)
{
	VOXEL_FUNCTION_COUNTER();
	
	FVoxelGraphErrorReporter::ClearNodesMessages(Generator);

	auto& PreviewSettings = *Generator->PreviewSettings;
	const auto Settings = FVoxelGraphPreviewSettingsWrapper(PreviewSettings);
	
	TVoxelSharedPtr<FVoxelGraphGeneratorInstance> GeneratorInstance;
	if (!Generator->GetGraphInstance(GeneratorInstance, true, !EnumHasAnyFlags(Flags, EVoxelGraphPreviewFlags::ManualPreview)))
	{
		return;
	}
		
	FVoxelGeneratorInit Init(
		PreviewSettings.VoxelSize,
		Settings.Size.GetMax(),
		PreviewSettings.RenderType,
		PreviewSettings.MaterialConfig,
		PreviewSettings.MaterialCollection,
		nullptr);
	{
		VOXEL_SCOPE_COUNTER("Init");
		GeneratorInstance->Init(Init);
	}

	if (!Data || EnumHasAnyFlags(Flags, EVoxelGraphPreviewFlags::UpdatePlaceableItems))
	{
		VOXEL_SCOPE_COUNTER("Create Data");

		Data = FVoxelData::Create(FVoxelDataSettings(32, GeneratorInstance.ToSharedRef(), false, false), 0);
		
		if (PreviewSettings.PlaceableItemManager)
		{
			PreviewSettings.PlaceableItemManager->Clear();
			PreviewSettings.PlaceableItemManager->Generate();
			PreviewSettings.PlaceableItemManager->ApplyToData(*Data);
		}
	}
	
	// Hack to reuse the same data with the items
	const_cast<TVoxelSharedRef<FVoxelGeneratorInstance>&>(Data->Generator) = GeneratorInstance.ToSharedRef();

	// Always do it as it's using Center
	{
		VOXEL_SCOPE_COUNTER("Apply Debug");
		
		Preview->SetDebugData(PreviewSettings.PlaceableItemManager);
		
		LineBatchComponent->Flush();
		if (PreviewSettings.PlaceableItemManager)
		{
			class FPreviewInterface : public IVoxelWorldInterface
			{
			public:
				const FVoxelGraphPreviewSettingsWrapper& Settings;

				explicit FPreviewInterface(const FVoxelGraphPreviewSettingsWrapper& Settings)
					: Settings(Settings)
				{
				}

				virtual FVector LocalToGlobal(const FIntVector& Position) const override { return FVector(Position - Settings.Center) / Settings.Step; }
				virtual FVector LocalToGlobalFloat(const FVoxelVector& Position) const override { return (Position - Settings.Center).ToFloat() / Settings.Step; }
			};
			PreviewSettings.PlaceableItemManager->DrawDebug(FPreviewInterface(Settings), *LineBatchComponent);
		}
	}
	
#if DO_THREADSAFE_CHECKS
	FVoxelReadScopeLock Lock(*Data, FVoxelIntBox::Infinite, FUNCTION_FNAME);
#endif
	
	const int32 RangeAnalysisChunkSize = FVoxelUtilities::DivideCeil(Settings.Resolution, PreviewSettings.NumRangeAnalysisChunksPerAxis);

	const bool bIsPreviewingPin = Generator->PreviewedPin.Get() != nullptr;
	
	const bool bShowDensities = PreviewSettings.PreviewType2D == EVoxelGraphPreviewType::Density || (PreviewSettings.PreviewType2D == EVoxelGraphPreviewType::Material && bIsPreviewingPin);
	const bool bComputeDensities = true; // Always needed for 3D preview

	const bool bShowMaterials = PreviewSettings.PreviewType2D == EVoxelGraphPreviewType::Material && !bIsPreviewingPin;
	const bool bComputeMaterials = bShowMaterials || !PreviewSettings.bHeightBasedColor;
	
	const bool bShowCosts = PreviewSettings.PreviewType2D == EVoxelGraphPreviewType::Cost;
	const bool bComputeCosts = bShowCosts;
	
	const bool bShowRangeAnalysis = PreviewSettings.PreviewType2D == EVoxelGraphPreviewType::RangeAnalysis;
	const bool bComputeRangeAnalysis = bShowRangeAnalysis;

	check(bShowDensities + bShowMaterials + bShowCosts + bShowRangeAnalysis == 1);

	TArray<v_flt> Values;
	TArray<FVoxelMaterial> Materials;
	TArray<float> Costs; // In ns
	TArray<TVoxelRange<v_flt>> Ranges;

	const int32 NumPixels = FMath::Square(Settings.Resolution);

	if (bComputeDensities) Values.SetNumUninitialized(NumPixels);
	if (bComputeMaterials) Materials.SetNumUninitialized(NumPixels);
	if (bComputeCosts) Costs.SetNumUninitialized(NumPixels);
	if (bComputeRangeAnalysis) Ranges.SetNumUninitialized(FMath::Square(PreviewSettings.NumRangeAnalysisChunksPerAxis));
	
	if (bComputeCosts)
	{
		VOXEL_SCOPE_COUNTER("Compute cost");
		
		FVoxelUtilities::ParallelFor_PerThreadData(Settings.Resolution, [&]()
		{
			return MakeUnique<FVoxelConstDataAccelerator>(*Data);
		},[&](const TUniquePtr<FVoxelConstDataAccelerator>& Accelerator, int32 X)
		{
			for (int32 Y = 0; Y < Settings.Resolution; Y++)
			{
				const int32 Index = Settings.GetDataIndex(X, Y);
				const FIntVector Position = Settings.GetWorldPosition(X, Y);
				
				const double StartTime = FPlatformTime::Seconds();
				Values[Index] = Accelerator->GetFloatValue(Position, PreviewSettings.LODToPreview);
				const double EndTime = FPlatformTime::Seconds();

				Costs[Index] = (EndTime - StartTime) * 1e9;
			}
		}, true); // not multithreaded as it messes up the results :/
	}
	else
	{
		TArray<FVoxelDataOctreeBase*> Octrees;
		FVoxelOctreeUtilities::IterateTreeInBounds(Data->GetOctree(), Settings.Bounds, [&](FVoxelDataOctreeBase& Octree)
		{
			if (Octree.IsLeafOrHasNoChildren())
			{
				Octrees.Add(&Octree);
			}
		});

		const TVoxelQueryZone<v_flt> GlobalQueryZone(Settings.Bounds, Settings.Size, Settings.LOD, Values);
		ParallelFor(Octrees.Num(), [&](int32 Index)
		{
			auto& Octree = *Octrees[Index];
			auto QueryZone = GlobalQueryZone.ShrinkTo(Octree.GetBounds());

			GeneratorInstance->GetOutput<false, v_flt, v_flt, FVoxelGraphOutputsIndices::ValueIndex>(
				FTransform(),
				1,
				QueryZone,
				PreviewSettings.LODToPreview,
				FVoxelItemStack(Octree.GetItemHolder()));
		});
	}
	
	if (bComputeMaterials)
	{
		TVoxelQueryZone<FVoxelMaterial> QueryZone(Settings.Bounds, Settings.Size, Settings.LOD, Materials);
		Data->Get<FVoxelMaterial>(QueryZone, PreviewSettings.LODToPreview);
	}
	
	if (bComputeRangeAnalysis)
	{
		for (int32 X = 0; X < PreviewSettings.NumRangeAnalysisChunksPerAxis; X++)
		{
			for (int32 Y = 0; Y < PreviewSettings.NumRangeAnalysisChunksPerAxis; Y++)
			{
				const auto Bounds = FVoxelIntBox::SafeConstruct(
					Settings.GetWorldPosition(X * RangeAnalysisChunkSize, Y * RangeAnalysisChunkSize),
					Settings.GetWorldPosition((X + 1) * RangeAnalysisChunkSize, (Y + 1) * RangeAnalysisChunkSize));

				TOptional<TVoxelRange<v_flt>> Range;
				FVoxelOctreeUtilities::IterateTreeInBounds(Data->GetOctree(), Bounds, [&](FVoxelDataOctreeBase& Octree)
				{
					if (Octree.IsLeafOrHasNoChildren())
					{
						const auto LocalRange = GeneratorInstance->GetValueRangeImpl<false>(
							{},
							Bounds.Overlap(Octree.GetBounds()),
							PreviewSettings.LODToPreview,
							FVoxelItemStack(Octree.GetItemHolder()));

						if (Range.IsSet())
						{
							Range = TVoxelRange<v_flt>::Union(Range.GetValue(), LocalRange);
						}
						else
						{
							Range = LocalRange;
						}
					}
				});

				if (ensure(Range))
				{
					Ranges[X + Y * PreviewSettings.NumRangeAnalysisChunksPerAxis] = Range.GetValue();
				}
			}
		}
	}

	AddMessages(*GeneratorInstance);
	
	TArray<FColor> NewDensities;
	TArray<FColor> NewMaterials;
	TArray<FColor> NewMaterialsWithCrossAndNoAlpha;
	NewDensities.SetNumUninitialized(NumPixels);
	NewMaterials.SetNumUninitialized(NumPixels);
	NewMaterialsWithCrossAndNoAlpha.SetNumUninitialized(NumPixels);

	v_flt MinValue;
	v_flt MaxValue;
	if (PreviewSettings.bAutoNormalize)
	{
		MinValue = Values[0];
		MaxValue = Values[0];
		for (const auto& Value : Values)
		{
			MinValue = FMath::Min(Value, MinValue);
			MaxValue = FMath::Max(Value, MaxValue);
		}
	}
	else
	{
		MinValue = PreviewSettings.NormalizeMinValue;
		MaxValue = PreviewSettings.NormalizeMaxValue;
	}
	
	double MinCost = 0;
	double MaxCost = 0;
	if (bComputeCosts)
	{
		VOXEL_SCOPE_COUNTER("Sort");
		auto SortedCopy = Costs;
		SortedCopy.Sort();
		MinCost = SortedCopy[FMath::Clamp(FMath::FloorToInt(SortedCopy.Num() * PreviewSettings.CostPercentile), 0, SortedCopy.Num() - 1)];
		MaxCost = SortedCopy[FMath::Clamp(FMath::CeilToInt(SortedCopy.Num() * (1 - PreviewSettings.CostPercentile)), 0, SortedCopy.Num() - 1)];
	}

	if (bShowDensities)
	{
		PreviewSettings.MinValue = LexToString(MinValue);
		PreviewSettings.MaxValue = LexToString(MaxValue);
	}
	else if (bShowMaterials)
	{
		PreviewSettings.MinValue = "N/A";
		PreviewSettings.MaxValue = "N/A";
	}
	else if (bShowCosts)
	{
		PreviewSettings.MinValue = LexToString(MinCost) + " ns";
		PreviewSettings.MaxValue = LexToString(MaxCost) + " ns";
	}
	else
	{
		check(bShowRangeAnalysis);
		PreviewSettings.MinValue = "N/A";
		PreviewSettings.MaxValue = "N/A";
	}

	const FIntPoint ScreenPreviewedVoxel = Settings.GetScreenPosition(PreviewSettings.PreviewedVoxel);	
	for (int32 X = 0; X < Settings.Resolution; X++)
	{
		for (int32 Y = 0; Y < Settings.Resolution; Y++)
		{
			const int32 DataIndex = Settings.GetDataIndex(X, Y);
			const int32 TextureIndex = Settings.GetTextureIndex(X, Y);
			
			const float Value = Values[DataIndex];
			const float Alpha = (Value - MinValue) / (MaxValue - MinValue);
		
			{
				uint8 IntAlpha = FVoxelUtilities::FloatToUINT8(Alpha);
				NewDensities[TextureIndex] = FColor(IntAlpha, IntAlpha, IntAlpha, 255);
			}
			
			if (bShowDensities)
			{
				if (PreviewSettings.bDrawColoredDistanceField)
				{
					const float ScaledValue = Value / (Settings.Resolution * Settings.Step) * 2;
					NewMaterials[TextureIndex] = FVoxelDistanceFieldUtilities::GetDistanceFieldColor(ScaledValue);
				}
				else
				{
					NewMaterials[TextureIndex] = NewDensities[TextureIndex];
				}
			}
			else if (bShowMaterials)
			{
				const FVoxelMaterial& Material = Materials[DataIndex];
				FColor Color = FColor::Black;

				const auto GetColor = [&](int32 Index)
				{
					if (PreviewSettings.IndexColors.IsValidIndex(Index))
					{
						return PreviewSettings.IndexColors[Index];
					}
					else
					{
						return FColor::Black;
					}
				};

				switch (PreviewSettings.MaterialPreviewType)
				{
				case EVoxelGraphMaterialPreviewType::RGB:
				{
					Color.R = Material.GetR();
					Color.G = Material.GetG();
					Color.B = Material.GetB();
					break;
				}
				case EVoxelGraphMaterialPreviewType::Alpha:
				{
					Color.R = Material.GetA();
					Color.G = Material.GetA();
					Color.B = Material.GetA();
					break;
				}
				case EVoxelGraphMaterialPreviewType::SingleIndex:
				{
					Color = GetColor(Material.GetSingleIndex());
					break;
				}
				case EVoxelGraphMaterialPreviewType::MultiIndex_Overview:
				{
					const TVoxelStaticArray<float, 4> Strengths = FVoxelUtilities::GetMultiIndexStrengths(Material);

					const FColor Color0 = GetColor(Material.GetMultiIndex_Index0());
					const FColor Color1 = GetColor(Material.GetMultiIndex_Index1());
					const FColor Color2 = GetColor(Material.GetMultiIndex_Index2());
					const FColor Color3 = GetColor(Material.GetMultiIndex_Index3());
					
					Color.R = FVoxelUtilities::ClampToUINT8(FMath::RoundToInt(Color0.R * Strengths[0] + Color1.R * Strengths[1] + Color2.R * Strengths[2] + Color3.R * Strengths[3]));
					Color.G = FVoxelUtilities::ClampToUINT8(FMath::RoundToInt(Color0.G * Strengths[0] + Color1.G * Strengths[1] + Color2.G * Strengths[2] + Color3.G * Strengths[3]));
					Color.B = FVoxelUtilities::ClampToUINT8(FMath::RoundToInt(Color0.B * Strengths[0] + Color1.B * Strengths[1] + Color2.B * Strengths[2] + Color3.B * Strengths[3]));
					
					break;
				}
				case EVoxelGraphMaterialPreviewType::MultiIndex_SingleIndexPreview:
				{
					const TVoxelStaticArray<float, 4> Strengths = FVoxelUtilities::GetMultiIndexStrengths(Material);

					float Strength = 0;
					if (PreviewSettings.MultiIndexToPreview == Material.GetMultiIndex_Index0()) Strength += Strengths[0];
					if (PreviewSettings.MultiIndexToPreview == Material.GetMultiIndex_Index1()) Strength += Strengths[1];
					if (PreviewSettings.MultiIndexToPreview == Material.GetMultiIndex_Index2()) Strength += Strengths[2];
					if (PreviewSettings.MultiIndexToPreview == Material.GetMultiIndex_Index3()) Strength += Strengths[3];

					Color.R = FVoxelUtilities::FloatToUINT8(Strength);
					Color.G = FVoxelUtilities::FloatToUINT8(Strength);
					Color.B = FVoxelUtilities::FloatToUINT8(Strength);

					break;
				}
				case EVoxelGraphMaterialPreviewType::MultiIndex_Wetness:
				{
					Color.R = Material.GetMultiIndex_Wetness();
					Color.G = Material.GetMultiIndex_Wetness();
					Color.B = Material.GetMultiIndex_Wetness();

					break;
				}
				case EVoxelGraphMaterialPreviewType::UV0:
				{
					Color.R = Material.GetU0();
					Color.G = Material.GetV0();
					break;
				}
				case EVoxelGraphMaterialPreviewType::UV1:
				{
					Color.R = Material.GetU1();
					Color.G = Material.GetV1();
					break;
				}
				case EVoxelGraphMaterialPreviewType::UV2:
				{
					Color.R = Material.GetU2();
					Color.G = Material.GetV2();
					break;
				}
				case EVoxelGraphMaterialPreviewType::UV3:
				{
					Color.R = Material.GetU3();
					Color.G = Material.GetV3();
					break;
				}
				default: ensure(false);
				}

				Color.A = 255;

				if (PreviewSettings.bHybridMaterialRendering && Value > 0)
				{
					Color = FColor::Transparent;
				}

				NewMaterials[TextureIndex] = Color;
			}
			else if (bShowCosts)
			{
				const float CostAlpha = FMath::Clamp<float>((Costs[DataIndex] - MinCost) / (MaxCost - MinCost), 0, 1);
				auto Color = FMath::Lerp(FLinearColor::Green, FLinearColor::Red, CostAlpha);
				Color = FMath::Lerp(Color, FLinearColor::Black, Value > 0 ? 0.5f : 0.f);
				NewMaterials[TextureIndex] = Color.ToFColor(false);
			}
			else
			{
				check(bShowRangeAnalysis);

				const int32 LocalX = FVoxelUtilities::DivideFloor(X, RangeAnalysisChunkSize);
				const int32 LocalY = FVoxelUtilities::DivideFloor(Y, RangeAnalysisChunkSize);
				const auto Range = Ranges[LocalX + LocalY * PreviewSettings.NumRangeAnalysisChunksPerAxis];

				auto Color = Range.Contains(0.f) ? FLinearColor::Red : FLinearColor::Green;
				Color = FMath::Lerp(Color, FLinearColor::Black, Value > 0 ? 0.5f : 0.f);
				NewMaterials[TextureIndex] = Color.ToFColor(false);
			}

			const int32 NumSameAxes = (FMath::Abs(X - ScreenPreviewedVoxel.X) < 2) + (FMath::Abs(Y - ScreenPreviewedVoxel.Y) < 2);
			if (NumSameAxes == 1 && (PreviewSettings.bShowStats || PreviewSettings.bShowValues))
			{
				NewMaterialsWithCrossAndNoAlpha[TextureIndex] = FMath::Lerp(FLinearColor(NewMaterials[TextureIndex]), FLinearColor::Black, 0.95f).ToFColor(true);
			}
			else
			{
				NewMaterialsWithCrossAndNoAlpha[TextureIndex] = NewMaterials[TextureIndex];
			}
			NewMaterialsWithCrossAndNoAlpha[TextureIndex].A = 255;
		}
	}

	FVoxelTextureUtilities::UpdateColorTexture(DensitiesTexture, FIntPoint(Settings.Resolution, Settings.Resolution), NewDensities);
	FVoxelTextureUtilities::UpdateColorTexture(MaterialsTexture, FIntPoint(Settings.Resolution, Settings.Resolution), NewMaterials);
	FVoxelTextureUtilities::UpdateColorTexture(MaterialsTextureWithCrossAndNoAlpha, FIntPoint(Settings.Resolution, Settings.Resolution), NewMaterialsWithCrossAndNoAlpha);

	Preview->SetTexture(MaterialsTextureWithCrossAndNoAlpha);
	Generator->SetPreviewTexture(NewMaterials, Settings.Resolution);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelGraphPreview::UpdateMaterialParameters()
{
	VOXEL_FUNCTION_COUNTER();
	
	const UVoxelGraphPreviewSettings& Settings = *Generator->PreviewSettings;
	const auto Wrapper = FVoxelGraphPreviewSettingsWrapper(Settings);

	if (Settings.bHeightmapMode)
	{
		HeightmapMaterial = UMaterialInstanceDynamic::Create(Settings.HeightmapMaterial, nullptr);
		if (!ensure(HeightmapMaterial))
		{
			return;
		}

		HeightmapMaterial->SetTextureParameterValue(TEXT("Color"), MaterialsTexture);
		HeightmapMaterial->SetTextureParameterValue(TEXT("Height"), DensitiesTexture);
		HeightmapMaterial->SetScalarParameterValue(TEXT("Height"), Settings.Height);
		HeightmapMaterial->SetScalarParameterValue(TEXT("StartBias"), Settings.StartBias);
		HeightmapMaterial->SetScalarParameterValue(TEXT("MaxSteps"), Settings.MaxSteps);
		HeightmapMaterial->SetScalarParameterValue(TEXT("UseHeightAsColor"), Settings.bHeightBasedColor ? 1.f : 0.f);
		HeightmapMaterial->SetScalarParameterValue(TEXT("UseWater"), Settings.bEnableWater ? 1.f : 0.f);
		HeightmapMaterial->SetVectorParameterValue(TEXT("LightDirection"), Settings.LightDirection);
		HeightmapMaterial->SetScalarParameterValue(TEXT("Brightness"), Settings.Brightness);
		HeightmapMaterial->SetScalarParameterValue(TEXT("ShadowDensity"), Settings.ShadowDensity);

		PreviewSceneFloor->SetStaticMesh(Settings.Mesh);
		PreviewSceneFloor->SetMaterial(0, HeightmapMaterial);
		PreviewSceneFloor->SetWorldScale3D(FVector(10));
		PreviewSceneFloor->SetBoundsScale(1e6f);
		PreviewSceneFloor->SetWorldRotation(FRotator::ZeroRotator);
	}
	else
	{
		SliceMaterial = UMaterialInstanceDynamic::Create(Settings.SliceMaterial, nullptr);
		if (!ensure(SliceMaterial))
		{
			return;
		}

		SliceMaterial->SetTextureParameterValue(TEXT("Color"), MaterialsTexture);

		PreviewSceneFloor->SetStaticMesh(Settings.Mesh);
		PreviewSceneFloor->SetMaterial(0, SliceMaterial);
		PreviewSceneFloor->SetWorldScale3D(FVector(Wrapper.Resolution / 200.f));

		const auto GetRotation = [&]()
		{
			const auto Make = [](const FVector& Vector, float Angle)
			{
				return FTransform(UKismetMathLibrary::RotatorFromAxisAndAngle(Vector, Angle));
			};
			const FVector X(1, 0, 0);
			const FVector Y(0, 1, 0);
			const FVector Z(0, 0, 1);
			
			switch (Settings.LeftToRight)
			{
			default: ensure(false);
			case EVoxelGraphPreviewAxes::X:
			{
				switch (Settings.BottomToTop)
				{
				default: ensure(false);
				case EVoxelGraphPreviewAxes::Y: return Make(Z, 90) * Make(Y, 180);
				case EVoxelGraphPreviewAxes::Z: return Make(X, -90) * Make(Y, -90);
				}
			}
			case EVoxelGraphPreviewAxes::Y:
			{
				switch (Settings.BottomToTop)
				{
				default: ensure(false);
				case EVoxelGraphPreviewAxes::X: return FTransform::Identity;
				case EVoxelGraphPreviewAxes::Z: return Make(Y, -90);
				}
			}
			case EVoxelGraphPreviewAxes::Z:
			{
				switch (Settings.BottomToTop)
				{
				default: ensure(false);
				case EVoxelGraphPreviewAxes::X: return Make(X, 90);
				case EVoxelGraphPreviewAxes::Y: return Make(Y, 90) * Make(X, 90);
				}
			}
			}
		};
		PreviewSceneFloor->SetWorldRotation(GetRotation().Rotator());
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

using FVoxelNodeArray = TArray<TWeakObjectPtr<const UVoxelNode>>;

uint32 GetTypeHash(const FVoxelNodeArray& Array)
{
	uint32 Hash = Array.Num();
	for (auto& It : Array)
	{
		Hash = HashCombine(Hash, GetTypeHash(It));
	}
	return Hash;
}

void FVoxelGraphPreview::AddMessages(FVoxelGraphGeneratorInstance& GraphGeneratorInstance) const
{
	VOXEL_FUNCTION_COUNTER();
	
	auto& PreviewSettings = *Generator->PreviewSettings;

	const int32 PreviewLOD = PreviewSettings.LODToPreview;
	const FIntVector PreviewedVoxel = PreviewSettings.PreviewedVoxel;

	auto& Octree = FVoxelOctreeUtilities::GetBottomNode(Data->GetOctree(), PreviewedVoxel.X, PreviewedVoxel.Y, PreviewedVoxel.Z);

	FVoxelContext Context(PreviewLOD, FVoxelItemStack(Octree.GetItemHolder()), {}, false);
	Context.LocalX = PreviewedVoxel.X;
	Context.LocalY = PreviewedVoxel.Y;
	Context.LocalZ = PreviewedVoxel.Z;
	Context.WorldX = PreviewedVoxel.X;
	Context.WorldY = PreviewedVoxel.Y;
	Context.WorldZ = PreviewedVoxel.Z;

	// If no placeable items the node will be huge
	const FVoxelIntBox RangeBounds = FVoxelIntBox(PreviewedVoxel).Extend(32);
	const FVoxelContextRange ContextRange(PreviewLOD, FVoxelItemStack(Octree.GetItemHolder()), {}, false, Octree.GetBounds().Overlap(RangeBounds));

	if (PreviewSettings.bShowStats)
	{
		VOXEL_SCOPE_COUNTER("ShowStats");
		
		TMap<TWeakObjectPtr<const UVoxelNode>, FVoxelGraphStatsRecorder::FNodeStats> NodeStats;
		{
			VOXEL_SCOPE_COUNTER("Normal");
			
			const auto Graph = GraphGeneratorInstance.GetGraph<FVoxelGraphOutputsIndices::ValueIndex>();
			check(Graph);

			FVoxelGraphVMComputeBuffers Buffers(GraphGeneratorInstance.GetVariablesBuffer(Graph));
			FVoxelGraphStatsRecorder Recorder;
			Graph->Compute(Context, Buffers, EVoxelFunctionAxisDependencies::XYZWithoutCache, Recorder);

			for (auto& It : Recorder.GetStats())
			{
				for (auto& SourceNode : It.Key->SourceNodes)
				{
					NodeStats.FindOrAdd(SourceNode) += It.Value;
				}
			}
		}

		TMap<TWeakObjectPtr<const UVoxelNode>, FVoxelGraphStatsRecorder::FNodeStats> RangeNodeStats;
		{
			VOXEL_SCOPE_COUNTER("Range");
			
			const auto Graph = GraphGeneratorInstance.GetGraph<FVoxelGraphOutputsIndices::RangeAnalysisIndex, FVoxelGraphOutputsIndices::ValueIndex>();
			check(Graph);

			FVoxelGraphVMComputeRangeBuffers Buffers(GraphGeneratorInstance.GetRangeVariablesBuffer(Graph));
			FVoxelGraphStatsRangeRecorder Recorder;
			Graph->ComputeRange(ContextRange, Buffers, Recorder);
			FVoxelRangeFailStatus::Get().Reset();

			for (auto& It : Recorder.GetStats())
			{
				for (auto& SourceNode : It.Key->SourceNodes)
				{
					RangeNodeStats.FindOrAdd(SourceNode) += It.Value;
				}
			}
		}

		TSet<TWeakObjectPtr<const UVoxelNode>> Keys;
		for (auto& It : NodeStats)
		{
			Keys.Add(It.Key);
		}
		for (auto& It : RangeNodeStats)
		{
			Keys.Add(It.Key);
		}

		FVoxelGraphErrorReporter ErrorReporter(Generator);
		for (auto& Key : Keys)
		{
			auto* Node = Key.Get();
			if (!ensure(Node))
			{
				continue;
			}

			const auto Stats = NodeStats.FindRef(Key);
			const auto RangeStats = RangeNodeStats.FindRef(Key);

			ErrorReporter.AddMessageToNode(Node,
			                               FString::Printf(TEXT("C++: %.2fns (range: %.2fns)"),
			                                               1e9 * Stats.EstimatedCppTime,
			                                               1e9 * RangeStats.EstimatedCppTime),
			                               EVoxelGraphNodeMessageType::Warning,
			                               false, false);

			ErrorReporter.AddMessageToNode(Node,
			                               FString::Printf(TEXT(" VM: %.2fns (range: %.2fns)"),
			                                               1e9 * Stats.VirtualMachineTime,
			                                               1e9 * RangeStats.VirtualMachineTime),
			                               EVoxelGraphNodeMessageType::Warning,
			                               false, false);
		}
		ErrorReporter.Apply(false);
	}

	if (PreviewSettings.bShowValues)
	{
		VOXEL_SCOPE_COUNTER("ShowValues");
		
		struct FNodeValues
		{
			EVoxelPinCategory Category;
			TOptional<FVoxelNodeType> Value;
			TOptional<FVoxelNodeRangeType> RangeValue;
			bool bIsUsed = false;
		};
		// Make sure to use the full source nodes array, else we get range error in macros due to mismatchs
		TMap<FVoxelNodeArray, TArray<FNodeValues>> NodesValues;
		TMap<FVoxelNodeArray, TArray<FString>> NodesRangeMessages;

		{
			VOXEL_SCOPE_COUNTER("Normal");
			
			const auto Graph = GraphGeneratorInstance.GetGraph<FVoxelGraphOutputsIndices::ValueIndex>();
			check(Graph);

			FVoxelGraphVMComputeBuffers Buffers(GraphGeneratorInstance.GetVariablesBuffer(Graph));
			FVoxelGraphRangeAnalysisRecorder Recorder;
			Graph->Compute(Context, Buffers, EVoxelFunctionAxisDependencies::XYZWithoutCache, Recorder);

			auto NodeOutputs = Recorder.GetNodesOutputs();

			// Copy constants
			{
				TSet<FVoxelComputeNode*> Nodes;
				Graph->GetConstantNodes(Nodes);
				for (auto* Node : Nodes)
				{
					ensure(!NodeOutputs.Contains(Node));
					auto& Outputs = NodeOutputs.Add(Node);

					Outputs.Empty(Node->OutputCount);
					Outputs.SetNumZeroed(Node->OutputCount);
					for (int32 OutputIndex = 0; OutputIndex < Node->OutputCount; OutputIndex++)
					{
						int32 OutputId = Node->GetOutputId(OutputIndex);
						if (OutputId != -1)
						{
							Outputs[OutputIndex] = Buffers.Variables[OutputId];
						}
					}
				}
			}
			
			for (auto& It : NodeOutputs)
			{
				const FVoxelComputeNode& Node = *It.Key;
				if (!ensure(Node.SourceNodes.Num() > 0))
				{
					continue;
				}

				auto& NodeValues = NodesValues.FindOrAdd(It.Key->SourceNodes);
				NodeValues.SetNum(Node.OutputCount);
				for (int32 OutputIndex = 0; OutputIndex < Node.OutputCount; OutputIndex++)
				{
					auto& NodeValue = NodeValues[OutputIndex];
					NodeValue.Category = Node.GetOutputCategory(OutputIndex);
					NodeValue.Value = It.Value[OutputIndex];
					NodeValue.bIsUsed = Node.IsOutputUsed(OutputIndex);
				}
			}
		}

		{
			VOXEL_SCOPE_COUNTER("Range");
			
			const auto Graph = GraphGeneratorInstance.GetGraph<FVoxelGraphOutputsIndices::RangeAnalysisIndex, FVoxelGraphOutputsIndices::ValueIndex>();
			check(Graph);

			FVoxelGraphVMComputeRangeBuffers Buffers(GraphGeneratorInstance.GetRangeVariablesBuffer(Graph));
			FVoxelGraphStatsRangeAnalysisRangeRecorder Recorder;
			Graph->ComputeRange(ContextRange, Buffers, Recorder);
			FVoxelRangeFailStatus::Get().Reset();

			auto NodeOutputs = Recorder.GetNodesOutputs();

			// Copy constants
			{
				TSet<FVoxelComputeNode*> Nodes;
				Graph->GetConstantNodes(Nodes);
				for (auto* Node : Nodes)
				{
					ensure(!NodeOutputs.Contains(Node));
					auto& Outputs = NodeOutputs.Add(Node);

					Outputs.Empty(Node->OutputCount);
					Outputs.SetNumZeroed(Node->OutputCount);
					for (int32 OutputIndex = 0; OutputIndex < Node->OutputCount; OutputIndex++)
					{
						int32 OutputId = Node->GetOutputId(OutputIndex);
						if (OutputId != -1)
						{
							Outputs[OutputIndex] = Buffers.Variables[OutputId];
						}
					}
				}
			}
			
			for (auto& It : NodeOutputs)
			{
				const FVoxelComputeNode& Node = *It.Key;
				if (!ensure(Node.SourceNodes.Num() > 0))
				{
					continue;
				}

				auto& NodeValues = NodesValues.FindOrAdd(It.Key->SourceNodes);
				NodeValues.SetNum(Node.OutputCount);
				for (int32 OutputIndex = 0; OutputIndex < Node.OutputCount; OutputIndex++)
				{
					auto& NodeValue = NodeValues[OutputIndex];
					NodeValue.Category = Node.GetOutputCategory(OutputIndex);
					NodeValue.RangeValue = It.Value[OutputIndex];
					NodeValue.bIsUsed = Node.IsOutputUsed(OutputIndex);
				}
			}

			for (auto& It : Recorder.GetNodesRangeMessages())
			{
				const FVoxelComputeNode& Node = *It.Key;
				for (auto& SourceNode : Node.SourceNodes)
				{
					ensure(SourceNode.IsValid());
					NodesRangeMessages.FindOrAdd(It.Key->SourceNodes).Append(It.Value);
				}
			}
		}

		FVoxelGraphErrorReporter ErrorReporter(Generator);
		for (auto& It : NodesValues)
		{
			auto* Node = It.Key[0].Get();
			if (!ensure(Node))
			{
				continue;
			}

			for (int32 OutputIndex = 0; OutputIndex < It.Value.Num(); OutputIndex++)
			{
				auto& Value = It.Value[OutputIndex];

				if (!Value.bIsUsed)
				{
					// Else we read invalid memory
					continue;
				}
				
				bool bCanShowRange = false;
				switch (Value.Category)
				{
				default: ensure(false);
				case EVoxelPinCategory::Exec:
				case EVoxelPinCategory::Seed:
				case EVoxelPinCategory::Wildcard:
				case EVoxelPinCategory::Vector:
				case EVoxelPinCategory::Material:
					continue;
				case EVoxelPinCategory::Boolean:
				case EVoxelPinCategory::Int:
				case EVoxelPinCategory::Float:
					bCanShowRange = true;
					break;
				case EVoxelPinCategory::Color:
					bCanShowRange = false;
					break;
				}

				const FName OutputName = Node->GetOutputPinName(OutputIndex);
				
				FString RangeString;
				if (Value.RangeValue.IsSet() && bCanShowRange)
				{
					RangeString = FVoxelPinCategory::ToString(Value.Category, Value.RangeValue.GetValue());
				}

				FString ValueString;
				if (Value.Value.IsSet())
				{
					ValueString = FVoxelPinCategory::ToString(Value.Category, Value.Value.GetValue());
				}

				FString Message;
				if (PreviewSettings.ShowValue == EVoxelGraphPreviewShowValue::ShowValue)
				{
					if (!ValueString.IsEmpty())
					{
						if (OutputName.IsNone())
						{
							Message = ValueString;
						}
						else
						{
							Message = OutputName.ToString() + ": " + ValueString;
						}
					}
				}
				else if (PreviewSettings.ShowValue == EVoxelGraphPreviewShowValue::ShowValueAndRange)
				{
					if (!ValueString.IsEmpty())
					{
						if (OutputName.IsNone())
						{
							Message = ValueString;
						}
						else
						{
							Message = OutputName.ToString() + ": " + ValueString;
						}

						if (!RangeString.IsEmpty())
						{
							Message += "\nRange: " + RangeString;
						}
					}
					else if (!RangeString.IsEmpty())
					{
						if (OutputName.IsNone())
						{
							Message = "Range: " + ValueString;
						}
						else
						{
							Message = OutputName.ToString() + " Range: " + ValueString;
						}
					}
				}
				else
				{
					ensure(PreviewSettings.ShowValue == EVoxelGraphPreviewShowValue::ShowRange);

					if (!RangeString.IsEmpty())
					{
						if (OutputName.IsNone())
						{
							Message = "Range: " + ValueString;
						}
						else
						{
							Message = OutputName.ToString() + " Range: " + ValueString;
						}
					}
				}

				const bool bIsInRange =
					!Value.Value.IsSet() ||
					!Value.RangeValue.IsSet() ||
					FVoxelPinCategory::IsInRange(Value.Category, Value.Value.GetValue(), Value.RangeValue.GetValue());

				if (!Message.IsEmpty() || !bIsInRange)
				{
					ErrorReporter.AddMessageToNode(
						Node,
						bIsInRange ? Message : ("RANGE ANALYSIS ERROR:\n" + Message),
						bIsInRange ? EVoxelGraphNodeMessageType::Info : EVoxelGraphNodeMessageType::Error,
						!bIsInRange,
						!bIsInRange);
				}
			}
		}
		for (auto& It : NodesRangeMessages)
		{
			auto* Node = It.Key[0].Get();
			if (!ensure(Node))
			{
				continue;
			}

			for (auto& Message : It.Value)
			{
				ErrorReporter.AddMessageToNode(
					Node,
					Message,
					EVoxelGraphNodeMessageType::Warning,
					false,
					false);
			}
		}
		ErrorReporter.Apply(false);
	}
}