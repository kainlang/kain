// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelHitGenerator.h"

class FVoxelHeightHitGenerator : public FVoxelHitGenerator
{
public:
	using FVoxelHitGenerator::FVoxelHitGenerator;
	
protected:
	virtual void GenerateSpawner(
		TArray<FVoxelSpawnerHit>& OutHits,
		const FVoxelSpawnerConfigSpawnerWithRuntimeData& Spawner, 
		const FRandomStream& RandomStream, 
		const FVoxelSpawnerRandomGenerator& RandomGenerator, 
		int32 NumRays) override;
};
