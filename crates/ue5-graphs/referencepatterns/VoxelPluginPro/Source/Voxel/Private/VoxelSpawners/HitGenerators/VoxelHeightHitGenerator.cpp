// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelSpawners/HitGenerators/VoxelHeightHitGenerator.h"
#include "VoxelSpawners/VoxelSpawner.h"
#include "VoxelSpawners/VoxelSpawnerManager.h"
#include "VoxelSpawners/VoxelSpawnerUtilities.h"
#include "VoxelSpawners/VoxelSpawnerRandomGenerator.h"

#include "VoxelCancelCounter.h"
#include "VoxelData/VoxelDataIncludes.h"

#include "Misc/ScopeExit.h"

void FVoxelHeightHitGenerator::GenerateSpawner(
	TArray<FVoxelSpawnerHit>& OutHits,
	const FVoxelSpawnerConfigSpawnerWithRuntimeData& Spawner, 
	const FRandomStream& RandomStream, 
	const FVoxelSpawnerRandomGenerator& RandomGenerator, 
	int32 NumRays)
{
	VOXEL_ASYNC_FUNCTION_COUNTER();

	check(Spawner.SpawnerType == EVoxelSpawnerType::Height);
	ensure(Spawner.Density.Type == EVoxelSpawnerDensityType::Constant || Spawner.Density.Type == EVoxelSpawnerDensityType::GeneratorOutput);
	check(Parameters.Group.SpawnerType == EVoxelSpawnerType::Height);

	// Note: assets are ignored when querying the height and the density, as it gets way too messy
	// For flat worlds, the height and the density is queried at Z = 0 if density is computed first, or Z = Height if height first
	// For sphere worlds at a normalized (X, Y, Z) if density is computed first, or normalized (X, Y, Z) * Height if height first
	// In theory the density could be computed at the exact position if bComputeDensityFirst is false, but this makes the behavior unpredictable

	const FVoxelIntBox Bounds = Parameters.Bounds;
	auto& Accelerator = Parameters.Accelerator;
	
	const int32 ChunkSize = RENDER_CHUNK_SIZE << Parameters.Group.LOD;
	
	ensure(Bounds.Size().X == ChunkSize);
	ensure(Bounds.Size().Y == ChunkSize);
	ensure(Bounds.Size().Z == ChunkSize);

	const FVoxelIntBox BoundsLimit = Bounds.Overlap(Accelerator.Data.WorldBounds);

	const FIntVector& ChunkPosition = Bounds.Min;
	const bool bIsSphere = Parameters.Config.WorldType == EVoxelSpawnerConfigRayWorldType::Sphere;
	FVoxelGeneratorInstance& Generator = *Accelerator.Data.Generator;

	// This is the value to use if the generator doesn't have the custom output.
	constexpr v_flt DefaultHeight = 0;

	const auto GetHeight = [&](const FVoxelVector& Position)
	{
		return Generator.GetCustomOutput<v_flt>(DefaultHeight, Spawner.HeightGraphOutputName_HeightOnly, Position, 0, FVoxelItemStack::Empty);
	};

	const auto GetDensity = [&](const FVoxelVector& Position)
	{
		return
			Spawner.Density.Type == EVoxelSpawnerDensityType::Constant
			? Spawner.Density.Constant
			: Generator.GetCustomOutput<v_flt>(0.f, Spawner.Density.GeneratorOutputName, Position, 0, FVoxelItemStack::Empty);
	};

	const auto IsFloatingOrCovered = [&](const FVoxelVector& InsideSurface, const FVoxelVector& OutsideSurface)
	{
		if (Spawner.bCheckIfFloating_HeightOnly)
		{
			const v_flt InsideValue = Accelerator.GetFloatValue(InsideSurface, 0);
			if (InsideValue > 0)
			{
				return true;
			}
		}

		if (Spawner.bCheckIfCovered_HeightOnly)
		{
			const v_flt OutsideValue = Accelerator.GetFloatValue(OutsideSurface, 0);
			if (OutsideValue < 0)
			{
				return true;
			}
		}
		
		return false;
	};

	{
		const auto Range = Generator.GetCustomOutputRange<v_flt>(
			DefaultHeight,
			Spawner.HeightGraphOutputName_HeightOnly,
			Bounds,
			0,
			FVoxelItemStack::Empty);

		if (bIsSphere)
		{
			auto Corners = Bounds.GetCorners(0);
			if (!Range.Intersects(TVoxelRange<v_flt>::FromList(
				FVoxelVector(Corners[0]).Size(),
				FVoxelVector(Corners[1]).Size(),
				FVoxelVector(Corners[2]).Size(),
				FVoxelVector(Corners[3]).Size(),
				FVoxelVector(Corners[4]).Size(),
				FVoxelVector(Corners[5]).Size(),
				FVoxelVector(Corners[6]).Size(),
				FVoxelVector(Corners[7]).Size())))
			{
				return;
			}
		}
		else
		{
			if (!Range.Intersects(TVoxelRange<v_flt>(Bounds.Min.Z, Bounds.Max.Z)))
			{
				return;
			}
		}
	}
	
	for (int32 Index = 0; Index < NumRays; Index++)
	{
		if ((Index & 0xFF) == 0 && Parameters.CancelCounter.IsCanceled())
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			RandomGenerator.Next();
		};

		if (bIsSphere)
		{
			FVoxelVector BasisX, BasisY, BasisZ;
			FVoxelSpawnerUtilities::GetSphereBasisFromBounds(Bounds, BasisX, BasisY, BasisZ);

			const FVector2D RandomValue = 2 * RandomGenerator.GetValue() - 1; // Map from 0,1 to -1,1
			const auto SamplePosition = [&](float X, float Y)
			{
				// 1.5f: hack to avoid holes between chunks
				return (Bounds.GetCenter() + 1.5f * ChunkSize / 2.f * (BasisX * X + BasisY * Y)).GetSafeNormal();
			};
			const FVoxelVector Start = SamplePosition(RandomValue.X, RandomValue.Y);

			v_flt Height;
			FVoxelVector Position;
			if (Spawner.bComputeDensityFirst_HeightOnly)
			{
				const v_flt Density = GetDensity(Start);
				if (RandomStream.GetFraction() > Density)
				{
					continue;
				}
			
				Height = GetHeight(Start);
				Position = Start * Height;
				if (!BoundsLimit.ContainsFloat(Position))
				{
					continue;
				}
			}
			else
			{
				Height = GetHeight(Start);
				Position = Start * Height;
				if (!BoundsLimit.ContainsFloat(Position))
				{
					continue;
				}

				const v_flt Density = GetDensity(Position);
				if (RandomStream.GetFraction() > Density)
				{
					continue;
				}
			}
			ensure(BoundsLimit.ContainsFloat(Position));

			if (IsFloatingOrCovered(Start * (Height - 1), Start * (Height + 1)))
			{
				continue;
			}

			// Will cancel out in SamplePosition
			const v_flt Delta = 1. / ChunkSize;
			
			const FVoxelVector Left = SamplePosition(RandomValue.X - Delta, RandomValue.Y);
			const FVoxelVector Right = SamplePosition(RandomValue.X + Delta, RandomValue.Y);
			const FVoxelVector Bottom = SamplePosition(RandomValue.X, RandomValue.Y - Delta);
			const FVoxelVector Top = SamplePosition(RandomValue.X, RandomValue.Y + Delta);

			const FVoxelVector Gradient =
				BasisX * (GetHeight(Left) - GetHeight(Right)) +
				BasisY * (GetHeight(Bottom) - GetHeight(Top)) +
				BasisZ * -2.f;
			
			OutHits.Add(FVoxelSpawnerHit((Position - ChunkPosition).ToFloat(), Gradient.GetSafeNormal().ToFloat()));
		}
		else
		{
			const FVector2D LocalPosition = RandomGenerator.GetValue() * ChunkSize;
			const v_flt X = v_flt(LocalPosition.X) + Bounds.Min.X;
			const v_flt Y = v_flt(LocalPosition.Y) + Bounds.Min.Y;

			v_flt Z;
			if (Spawner.bComputeDensityFirst_HeightOnly)
			{
				const v_flt Density = GetDensity({ X, Y, 0 });
				if (RandomStream.GetFraction() > Density)
				{
					continue;
				}

				Z = GetHeight({ X, Y, 0 });
				if (BoundsLimit.Min.Z > Z || Z > BoundsLimit.Max.Z)
				{
					continue;
				}
			}
			else
			{
				Z = GetHeight({ X, Y, 0 });
				if (BoundsLimit.Min.Z > Z || Z > BoundsLimit.Max.Z)
				{
					continue;
				}
				const v_flt Density = GetDensity({ X, Y, Z });
				if (RandomStream.GetFraction() > Density)
				{
					continue;
				}
			}
			ensure(BoundsLimit.Min.Z <= Z && Z <= BoundsLimit.Max.Z);

			if (IsFloatingOrCovered({ X, Y, Z - 1 }, { X, Y, Z + 1 }))
			{
				continue;
			}

			FVoxelVector Gradient;
			Gradient.X =
				GetHeight({ X - 1, Y, 0 }) -
				GetHeight({ X + 1, Y, 0 });
			Gradient.Y =
				GetHeight({ X, Y - 1, 0 }) -
				GetHeight({ X, Y + 1, 0 });
			Gradient.Z = 2;

			OutHits.Add(FVoxelSpawnerHit((FVoxelVector(X, Y, Z) - ChunkPosition).ToFloat(), Gradient.GetSafeNormal().ToFloat()));
		}
	}
}
