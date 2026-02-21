// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#if USE_EMBREE_VOXEL

#include "CoreMinimal.h"
#include "VoxelSpawners/VoxelSpawnerRayHandler.h"

#include "embree3/rtcore.h"
#include "embree3/rtcore_ray.h"

struct FVoxelChunkMesh;

class FVoxelSpawnerEmbreeRayHandler : public FVoxelSpawnerRayHandler
{
public:
	FVoxelSpawnerEmbreeRayHandler(
		bool bStoreDebugRays,
		TArray<uint32>&& Indices,
		TArray<FVector>&& Vertices);
	~FVoxelSpawnerEmbreeRayHandler();
	
	virtual bool HasError() const override { return !EmbreeScene || !EmbreeDevice; }
	
protected:
	virtual bool TraceRayInternal(const FVector& Start, const FVector& Direction, FVector& HitNormal, FVector& HitPosition) const override;

private:
	RTCDevice EmbreeDevice = nullptr;
	RTCScene EmbreeScene = nullptr;
	RTCGeometry Geometry = nullptr;

	TArray<uint32> Indices;
	TArray<FVector3f> Vertices;
};

#endif // USE_EMBREE_VOXEL
