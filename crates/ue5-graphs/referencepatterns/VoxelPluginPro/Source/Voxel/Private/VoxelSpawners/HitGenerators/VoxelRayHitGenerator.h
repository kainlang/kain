// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelHitGenerator.h"

class FVoxelSpawnerRayHandler;

class FVoxelRayHitGenerator : public FVoxelHitGenerator
{
public:
	FVoxelSpawnerRayHandler& RayHandler;
	
	explicit FVoxelRayHitGenerator(const FParameters& Parameters, FVoxelSpawnerRayHandler& RayHandler)
		: FVoxelHitGenerator(Parameters)
		, RayHandler(RayHandler)
	{
	}
	
protected:
	virtual void GenerateSpawner(
		TArray<FVoxelSpawnerHit>& OutHits,
		const FVoxelSpawnerConfigSpawnerWithRuntimeData& Spawner, 
		const FRandomStream& RandomStream, 
		const FVoxelSpawnerRandomGenerator& RandomGenerator, 
		int32 NumRays) override;
};
