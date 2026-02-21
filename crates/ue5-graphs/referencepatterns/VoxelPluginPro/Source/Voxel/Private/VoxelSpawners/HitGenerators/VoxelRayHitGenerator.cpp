// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelSpawners/HitGenerators/VoxelRayHitGenerator.h"
#include "VoxelSpawners/VoxelSpawner.h"
#include "VoxelSpawners/VoxelSpawnerManager.h"
#include "VoxelSpawners/VoxelSpawnerUtilities.h"
#include "VoxelSpawners/VoxelSpawnerRayHandler.h"
#include "VoxelSpawners/VoxelSpawnerRandomGenerator.h"

#include "VoxelCancelCounter.h"
#include "VoxelData/VoxelDataIncludes.h"
#include "VoxelUtilities/VoxelMaterialUtilities.h"

void FVoxelRayHitGenerator::GenerateSpawner(
	TArray<FVoxelSpawnerHit>& OutHits,
	const FVoxelSpawnerConfigSpawnerWithRuntimeData& Spawner, 
	const FRandomStream& RandomStream, 
	const FVoxelSpawnerRandomGenerator& RandomGenerator, 
	int32 NumRays)
{
	VOXEL_ASYNC_FUNCTION_COUNTER();

	check(Parameters.Group.SpawnerType == EVoxelSpawnerType::Ray);

	const FVoxelIntBox Bounds = Parameters.Bounds;
	auto& Accelerator = Parameters.Accelerator;
	
	check(Bounds.Size().X == Bounds.Size().Y && Bounds.Size().Y == Bounds.Size().Z);
	const int32 BoundsSize = Bounds.Size().X;
	const FIntVector& ChunkPosition = Bounds.Min;

	FVoxelVector BasisX, BasisY;
	FVoxelSpawnerUtilities::GetBasisFromBounds(Parameters.Config, Bounds, BasisX, BasisY);

	struct FVoxelRay
	{
		FVector Position;
		FVector Direction;
	};
	TArray<FVoxelSpawnerHit> Hits;
	TArray<FVoxelRay> QueuedRays;
	for (int32 Index = 0; Index < NumRays; Index++)
	{
		if ((Index & 0xFF) == 0 && Parameters.CancelCounter.IsCanceled())
		{
			return;
		}

		const FVector2D RandomValue = 2 * RandomGenerator.GetValue() - 1; // Map from 0,1 to -1,1
		const FVector Start = FVoxelVector::ToFloat(BoundsSize / 2 * (BasisX * RandomValue.X + BasisY * RandomValue.Y + 1)); // +1: we want to be in the center
		const FVector Direction = FVoxelVector::ToFloat(FVoxelSpawnerUtilities::GetRayDirection(Parameters.Config, Start, ChunkPosition));
		FVoxelSpawnerHit Hit;
		if (RayHandler.TraceRay(
			Start - Direction * 4 * BoundsSize /* Ray offset */,
			Direction,
			Hit.Normal,
			Hit.LocalPosition))
		{
			Hits.Add(Hit);
			QueuedRays.Add({ Hit.LocalPosition, Direction });
		}
		RandomGenerator.Next();
	}

	// Process consecutive hits
	while (QueuedRays.Num() > 0)
	{
		const auto Ray = QueuedRays.Pop(UE_505_SWITCH(false, EAllowShrinking::No));
		const float Offset = 1;
		FVoxelSpawnerHit Hit;
		if (RayHandler.TraceRay(
			Ray.Position + Ray.Direction * Offset,
			Ray.Direction,
			Hit.Normal,
			Hit.LocalPosition))
		{
			Hits.Add(Hit);
			QueuedRays.Add({ Hit.LocalPosition, Ray.Direction });
		}
	}
	
	for (auto& Hit : Hits)
	{
		const FVector& LocalPosition = Hit.LocalPosition;
		const FVector& Normal = Hit.Normal;
		const FVoxelVector GlobalPosition = FVoxelVector(ChunkPosition) + LocalPosition;

		if (!Accelerator.Data.IsInWorld(GlobalPosition))
		{
			continue;
		}
		
		const FIntVector VoxelPosition = FVoxelSpawnerUtilities::GetClosestNotEmptyPoint(Accelerator, GlobalPosition);

		TOptional<float> CachedGeneratorDensity;
		const auto GetGeneratorDensity = [&]()
		{
			if (!CachedGeneratorDensity.IsSet())
			{
				// Need to get the right ItemHolder
				CachedGeneratorDensity = Accelerator.GetCustomOutput<v_flt>(0.f, Spawner.Density.GeneratorOutputName, VoxelPosition.X, VoxelPosition.Y, VoxelPosition.Z, 0);
			}
			return CachedGeneratorDensity.GetValue();
		};
		
		TOptional<FVoxelMaterial> CachedMaterial;
		const auto GetMaterial = [&]()
		{
			if (!CachedMaterial.IsSet())
			{
				CachedMaterial = Accelerator.Get<FVoxelMaterial>(VoxelPosition, 0);
			}
			return CachedMaterial.GetValue();
		};

		const auto GetDensity = [&GetGeneratorDensity, &GetMaterial, this](const FVoxelSpawnerDensity& SpawnerDensity)
		{
			const auto GetDensityImpl = [&]()
			{
				switch (SpawnerDensity.Type)
				{
				default: ensure(false);
				case EVoxelSpawnerDensityType::Constant:
				{
					return SpawnerDensity.Constant;
				}
				case EVoxelSpawnerDensityType::GeneratorOutput: 
				{
					return GetGeneratorDensity();
				}
				case EVoxelSpawnerDensityType::MaterialRGBA: 
				{
					const FVoxelMaterial Material = GetMaterial();
					switch (SpawnerDensity.RGBAChannel)
					{
					default: ensure(false);
					case EVoxelRGBA::R: return Material.GetR_AsFloat();
					case EVoxelRGBA::G: return Material.GetG_AsFloat();
					case EVoxelRGBA::B: return Material.GetB_AsFloat();
					case EVoxelRGBA::A: return Material.GetA_AsFloat();
					}
				}
				case EVoxelSpawnerDensityType::MaterialUVs:
				{
					const FVoxelMaterial Material = GetMaterial();
					return SpawnerDensity.UVAxis == EVoxelSpawnerUVAxis::U
						? Material.GetU_AsFloat(SpawnerDensity.UVChannel)
						: Material.GetV_AsFloat(SpawnerDensity.UVChannel);
				}
				case EVoxelSpawnerDensityType::MaterialFiveWayBlend:
				{
					const FVoxelMaterial Material = GetMaterial();

					if (Parameters.Config.FiveWayBlendSetup.bFourWayBlend)
					{
						const TVoxelStaticArray<float, 4> Strengths = FVoxelUtilities::GetFourWayBlendStrengths(Material);
						return Strengths[FMath::Clamp(SpawnerDensity.FiveWayBlendChannel, 0, 3)];
					}
					else
					{
						const TVoxelStaticArray<float, 5> Strengths = FVoxelUtilities::GetFiveWayBlendStrengths(Material);
						return Strengths[FMath::Clamp(SpawnerDensity.FiveWayBlendChannel, 0, 4)];
					}
				}
				case EVoxelSpawnerDensityType::SingleIndex:
				{
					const FVoxelMaterial Material = GetMaterial();
					return SpawnerDensity.SingleIndexChannels.Contains(Material.GetSingleIndex()) ? 1.f : 0.f;
				}
				case EVoxelSpawnerDensityType::MultiIndex:
				{
					const FVoxelMaterial Material = GetMaterial();
					const TVoxelStaticArray<float, 4> Strengths = FVoxelUtilities::GetMultiIndexStrengths(Material);

					float Strength = 0.f;
					for (int32 Channel : SpawnerDensity.MultiIndexChannels)
					{
						const int32 Index = FVoxelUtilities::GetMultiIndexIndex(Material, Channel);
						if (Index != -1)
						{
							Strength += Strengths[Index];
						}
					}

					return Strength;
				}
				}
			};

			const float Density = GetDensityImpl();
			switch (SpawnerDensity.Transform)
			{
			default: ensure(false);
			case EVoxelSpawnerDensityTransform::Identity: return Density;
			case EVoxelSpawnerDensityTransform::OneMinus: return 1.f - Density;
			}
		};
		
		const float Density = GetDensity(Spawner.Density);
		const float DensityMultiplier = GetDensity(Spawner.DensityMultiplier_RayOnly);
		
		if (RandomStream.GetFraction() <= Density * DensityMultiplier)
		{
			OutHits.Add(FVoxelSpawnerHit(LocalPosition, Normal));
		}
	}
}
