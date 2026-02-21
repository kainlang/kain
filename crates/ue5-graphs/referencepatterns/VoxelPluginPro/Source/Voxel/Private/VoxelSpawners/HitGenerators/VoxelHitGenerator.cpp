// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelSpawners/HitGenerators/VoxelHitGenerator.h"
#include "VoxelSpawners/VoxelSpawner.h"
#include "VoxelSpawners/VoxelSpawnerManager.h"
#include "VoxelSpawners/VoxelSpawnerUtilities.h"
#include "VoxelSpawners/VoxelSpawnerRandomGenerator.h"

#include "VoxelCancelCounter.h"

TMap<int32, TArray<FVoxelSpawnerHit>> FVoxelHitGenerator::Generate()
{
	VOXEL_ASYNC_FUNCTION_COUNTER();

	const FVoxelIntBox Bounds = Parameters.Bounds;
	
	ensure(Bounds.Size().X == Bounds.Size().Y && Bounds.Size().Y == Bounds.Size().Z);
	const int32 BoundsSize = Bounds.Size().X;

	const FIntVector& ChunkPosition = Bounds.Min;
	const uint32 Seed = Bounds.GetMurmurHash();

	TMap<int32, TArray<FVoxelSpawnerHit>> OutHits;
	for (int32 ElementIndex : Parameters.SpawnersToSpawn)
	{
		if (Parameters.CancelCounter.IsCanceled()) return {};
		
		const FVoxelSpawnerConfigSpawnerWithRuntimeData& Spawner = Parameters.Group.Spawners[ElementIndex];

		const uint32 ElementSeed =
			Parameters.Group.SpawnerType == EVoxelSpawnerType::Ray
			? FVoxelUtilities::MurmurHash32(Seed, Parameters.GroupSeed, ElementIndex, Spawner.Seed, /* avoid collisions with height spawners */23)
			: FVoxelUtilities::MurmurHash32(Seed, Parameters.GroupSeed, ElementIndex, Spawner.Seed);
		
		const FRandomStream RandomStream(ElementSeed);

		const TUniquePtr<FVoxelSpawnerRandomGenerator> RandomGenerator = FVoxelSpawnerUtilities::GetRandomGenerator(Spawner);
		RandomGenerator->Init(
			ElementSeed ^ FVoxelUtilities::MurmurHash32(ChunkPosition.X),
			ElementSeed ^ FVoxelUtilities::MurmurHash32(ChunkPosition.Y));

		const int32 NumRays = FMath::FloorToInt(FMath::Square(double(BoundsSize) / double(Spawner.DistanceBetweenInstancesInVoxel)));

		TArray<FVoxelSpawnerHit> Hits;
		GenerateSpawner(Hits, Spawner, RandomStream, *RandomGenerator, NumRays);

		if (Hits.Num() > 0)
		{
			OutHits.Add(ElementIndex, MoveTemp(Hits));
		}
	}
	
	return OutHits;
}
