// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AVoxelWorldInterface;

class FVoxelSpawnerRayHandler
{
public:
	FVoxelSpawnerRayHandler(bool bStoreDebugRays);
	virtual ~FVoxelSpawnerRayHandler() = default;

	virtual bool HasError() const = 0;
	bool TraceRay(const FVector& Start, const FVector& Direction, FVector& HitNormal, FVector& HitPosition) const;
	
protected:
	virtual bool TraceRayInternal(const FVector& Start, const FVector& Direction, FVector& HitNormal, FVector& HitPosition) const = 0;

public:
	void ShowDebug(
		TWeakObjectPtr<const AVoxelWorldInterface> VoxelWorld, 
		const FIntVector& ChunkPosition,
		bool bShowDebugRays,
		bool bShowDebugHits) const;

private:
	struct FDebugRay
	{
		FVector Start;
		FVector Direction;

		bool bHit;
		FVector HitNormal;
		FVector HitPosition;
	};
	const bool bStoreDebugRays;
	mutable TArray<FDebugRay> DebugRays;
};
