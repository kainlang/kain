// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelSpawnerUtilities.h"
#include "VoxelData/VoxelDataIncludes.h"
#include "VoxelSpawners/VoxelSpawnerConfig.h"
#include "VoxelSpawners/VoxelSpawnerManager.h"
#include "VoxelSpawners/VoxelSpawnerRandomGenerator.h"

TUniquePtr<FVoxelSpawnerRandomGenerator> FVoxelSpawnerUtilities::GetRandomGenerator(const FVoxelSpawnerConfigSpawner& Spawner)
{
	if (Spawner.RandomGenerator == EVoxelSpawnerConfigElementRandomGenerator::Sobol)
	{
		return MakeUnique<FVoxelSpawnerSobolRandomGenerator>();

	}
	else
	{
		check(Spawner.RandomGenerator == EVoxelSpawnerConfigElementRandomGenerator::Halton);
		return MakeUnique<FVoxelSpawnerHaltonRandomGenerator>();
	}
}

FIntVector FVoxelSpawnerUtilities::GetClosestNotEmptyPoint(const FVoxelConstDataAccelerator& Accelerator, const FVoxelVector& Position)
{
	FIntVector ClosestPoint = FIntVector(ForceInit);
	v_flt Distance = MAX_vflt;
	for (auto& Neighbor : FVoxelUtilities::GetNeighbors(Position))
	{
		if (!Accelerator.GetValue(Neighbor, 0).IsEmpty())
		{
			const v_flt PointDistance = (FVoxelVector(Neighbor) - Position).SizeSquared();
			if (PointDistance < Distance)
			{
				Distance = PointDistance;
				ClosestPoint = Neighbor;
			}
		}
	}
	if (/*ensure*/(Distance < 100))
	{
		return ClosestPoint;
	}
	else
	{
		return FVoxelUtilities::RoundToInt(Position);
	}
}

void FVoxelSpawnerUtilities::GetSphereBasisFromBounds(
	const FVoxelIntBox& Bounds,
	FVoxelVector& OutBasisX,
	FVoxelVector& OutBasisY,
	FVoxelVector& OutBasisZ)
{
	// Find closest corner
	const FVoxelVector Direction = -Bounds.GetCenter().GetSafeNormal();

	const FVoxelVector AbsDirection = Direction.GetAbs();
	const float Max = AbsDirection.GetMax();
	const FVoxelVector Vector =
		Max == AbsDirection.X
		? FVoxelVector(0, 1, 0)
		: Max == AbsDirection.Y
		? FVoxelVector(0, 0, 1)
		: FVoxelVector(1, 0, 0);

	OutBasisX = Direction ^ Vector;
	OutBasisY = Direction ^ OutBasisX;
	OutBasisZ = Direction;

	OutBasisX.Normalize();
	OutBasisY.Normalize();

	ensure(OutBasisX.GetAbsMax() > KINDA_SMALL_NUMBER);
	ensure(OutBasisY.GetAbsMax() > KINDA_SMALL_NUMBER);
}

void FVoxelSpawnerUtilities::GetBasisFromBounds(
	const FVoxelSpawnerThreadSafeConfig& ThreadSafeConfig,
	const FVoxelIntBox& Bounds,
	FVoxelVector& OutBasisX,
	FVoxelVector& OutBasisY)
{
	if (ThreadSafeConfig.WorldType == EVoxelSpawnerConfigRayWorldType::Flat)
	{
		OutBasisX = FVoxelVector::RightVector;
		OutBasisY = FVoxelVector::ForwardVector;
	}
	else
	{
		check(ThreadSafeConfig.WorldType == EVoxelSpawnerConfigRayWorldType::Sphere);
		FVoxelVector OutBasisZ;
		GetSphereBasisFromBounds(Bounds, OutBasisX, OutBasisY, OutBasisZ);

		OutBasisX *= 1.5; // Hack to avoid holes
		OutBasisY *= 1.5;
	}
}

FVoxelVector FVoxelSpawnerUtilities::GetRayDirection(const FVoxelSpawnerThreadSafeConfig& ThreadSafeConfig, const FVoxelVector& Start, const FIntVector& ChunkPosition)
{
	if (ThreadSafeConfig.WorldType == EVoxelSpawnerConfigRayWorldType::Flat)
	{
		return -FVoxelVector::UpVector;
	}
	else
	{
		check(ThreadSafeConfig.WorldType == EVoxelSpawnerConfigRayWorldType::Sphere);
		return -(FVoxelVector(ChunkPosition) + Start).GetSafeNormal();
	}
}
