// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FVoxelVector;
struct FVoxelIntBox;

struct FVoxelSpawnerConfigSpawner;
struct FVoxelSpawnerThreadSafeConfig;

class FVoxelConstDataAccelerator;
class FVoxelSpawnerRandomGenerator;

struct FVoxelSpawnerUtilities
{
	static TUniquePtr<FVoxelSpawnerRandomGenerator> GetRandomGenerator(const FVoxelSpawnerConfigSpawner& Spawner);
	
	static FIntVector GetClosestNotEmptyPoint(const FVoxelConstDataAccelerator& Accelerator, const FVoxelVector& Position);

	static void GetSphereBasisFromBounds(
		const FVoxelIntBox& Bounds,
		FVoxelVector& OutBasisX,
		FVoxelVector& OutBasisY,
		FVoxelVector& OutBasisZ);

	static void GetBasisFromBounds(
		const FVoxelSpawnerThreadSafeConfig& ThreadSafeConfig,
		const FVoxelIntBox& Bounds,
		FVoxelVector& OutBasisX,
		FVoxelVector& OutBasisY);

	static FVoxelVector GetRayDirection(
		const FVoxelSpawnerThreadSafeConfig& ThreadSafeConfig,
		const FVoxelVector& Start,
		const FIntVector& ChunkPosition);
};
