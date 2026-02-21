// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelSpawners/VoxelSpawnerRayHandler.h"
#include "VoxelDebug/VoxelDebugManager.h"
#include "VoxelWorldInterface.h"
#include "VoxelMinimal.h"

#include "Async/Async.h"
#include "DrawDebugHelpers.h"

FVoxelSpawnerRayHandler::FVoxelSpawnerRayHandler(bool bStoreDebugRays)
	: bStoreDebugRays(bStoreDebugRays)
{

}

bool FVoxelSpawnerRayHandler::TraceRay(const FVector& Start, const FVector& Direction, FVector& HitNormal, FVector& HitPosition) const
{
	ensure(Direction.IsNormalized());

	const bool bHit = TraceRayInternal(Start, Direction, HitNormal, HitPosition);
	if (bStoreDebugRays)
	{
		FDebugRay Ray;
		Ray.Start = Start;
		Ray.Direction = Direction;
		Ray.bHit = bHit;
		Ray.HitNormal = HitNormal;
		Ray.HitPosition = HitPosition;
		DebugRays.Add(Ray);
	}
	return bHit;
}

void FVoxelSpawnerRayHandler::ShowDebug(
	TWeakObjectPtr<const AVoxelWorldInterface> VoxelWorld,
	const FIntVector& ChunkPosition,
	bool bShowDebugRays,
	bool bShowDebugHits) const
{
	AsyncTask(ENamedThreads::GameThread, [DebugRays = DebugRays, VoxelWorld, ChunkPosition, bShowDebugRays, bShowDebugHits]()
	{
		if (VoxelWorld.IsValid())
		{
			if (UWorld* World = VoxelWorld->GetWorld())
			{
				for (auto& Ray : DebugRays)
				{
					if (bShowDebugRays)
					{
						// Rays start are 4 * RENDER_CHUNK_SIZE off
						auto Start = VoxelWorld->LocalToGlobalFloat(Ray.Start + FVector(ChunkPosition) + Ray.Direction * 3.5f * RENDER_CHUNK_SIZE);
						auto End = VoxelWorld->LocalToGlobalFloat(Ray.Start + FVector(ChunkPosition) + Ray.Direction * 5.5f * RENDER_CHUNK_SIZE);
						DrawDebugDirectionalArrow(World, Start, End, 200, FColor::Red, true, 1000.f);
					}
					if (bShowDebugHits && Ray.bHit)
					{
						auto HitPosition = VoxelWorld->LocalToGlobalFloat(Ray.HitPosition + FVector(ChunkPosition));
						DrawDebugPoint(World, HitPosition, 5, FColor::Blue, true, 1000.f);
					}
				}
			}
		}
	});
}
