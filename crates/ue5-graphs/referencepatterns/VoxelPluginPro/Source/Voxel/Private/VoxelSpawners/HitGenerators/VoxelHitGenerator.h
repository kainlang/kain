// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelIntBox.h"

struct FVoxelSpawnerHit;
struct FVoxelSpawnerConfigGroup;
struct FVoxelSpawnerThreadSafeConfig;
struct FVoxelSpawnerConfigSpawnerWithRuntimeData;

class FVoxelCancelCounter;
class FVoxelConstDataAccelerator;
class FVoxelSpawnerRandomGenerator;

class FVoxelHitGenerator
{
public:
	struct FParameters
	{
		// Used to have different results even if the groups are identical
		int32 GroupSeed;
		FVoxelIntBox Bounds;

		const TArray<int32>& SpawnersToSpawn;
		const FVoxelSpawnerConfigGroup& Group;
		const FVoxelSpawnerThreadSafeConfig& Config;

		const FVoxelCancelCounter& CancelCounter;
		const FVoxelConstDataAccelerator& Accelerator;
	};
	const FParameters Parameters;
	
	explicit FVoxelHitGenerator(const FParameters& Parameters)
		: Parameters(Parameters)
	{
	}
	virtual ~FVoxelHitGenerator() = default;

public:
	TMap<int32, TArray<FVoxelSpawnerHit>> Generate();

protected:
	virtual void GenerateSpawner(
		TArray<FVoxelSpawnerHit>& OutHits,
		const FVoxelSpawnerConfigSpawnerWithRuntimeData& Spawner, 
		const FRandomStream& RandomStream, 
		const FVoxelSpawnerRandomGenerator& RandomGenerator, 
		int32 NumRays) = 0;
};
