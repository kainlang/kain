// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelMinimal.h"
#include "VoxelSpawnerConfig.h"
#include "Containers/Queue.h"
#include "VoxelIntBox.h"
#include "VoxelTickable.h"
#include "IVoxelSpawnerManager.h"

namespace FVoxelSpawnersSaveVersion
{
	enum Type : int32;
}

class FVoxelReadScopeLock;
class FVoxelSpawnerTask;
class AVoxelWorld;
class AVoxelWorldInterface;
class FVoxelSpawnerProxy;
class FVoxelSpawnerProxyResult;
class IVoxelRenderer;
class IVoxelLODManager;
class FVoxelInstancedMeshManager;
class FVoxelEventManager;
class FVoxelDebugManager;
class FVoxelData;
class IVoxelPool;
class FVoxelConstDataAccelerator;
struct FVoxelSpawnerHit;
struct FVoxelSpawnersSaveImpl;

DECLARE_VOXEL_MEMORY_STAT(TEXT("Voxel Spawner Manager Memory"), STAT_VoxelSpawnerManagerMemory, STATGROUP_VoxelMemory, VOXEL_API);

struct FVoxelSpawnerSettings
{
	// Used for debug
	const TWeakObjectPtr<const AVoxelWorldInterface> VoxelWorldInterface;
	
	const TVoxelSharedRef<IVoxelPool> Pool;
	const TVoxelSharedRef<FVoxelDebugManager> DebugManager;
	const TVoxelSharedRef<FVoxelData> Data;
	const TVoxelSharedRef<FVoxelInstancedMeshManager> MeshManager;
	const TVoxelSharedRef<FVoxelEventManager> EventManager;
	const TVoxelSharedRef<IVoxelLODManager> LODManager;
	const TVoxelSharedRef<IVoxelRenderer> Renderer;
	const TWeakObjectPtr<UVoxelSpawnerConfig> Config;
	const float VoxelSize;
	const float PriorityDuration;
	
	FVoxelSpawnerSettings(
		const AVoxelWorld* World,
		EVoxelPlayType PlayType,
		const TVoxelSharedRef<IVoxelPool>& Pool,
		const TVoxelSharedRef<FVoxelDebugManager>& DebugManager,
		const TVoxelSharedRef<FVoxelData>& Data,
		const TVoxelSharedRef<IVoxelLODManager>& LODManager,
		const TVoxelSharedRef<IVoxelRenderer>& Renderer,
		const TVoxelSharedRef<FVoxelInstancedMeshManager>& MeshManager,
		const TVoxelSharedRef<FVoxelEventManager>& EventManager);
};

///////////////////////////////////////////////////////////////////////////////

struct FVoxelSpawnerConfigSpawnerWithRuntimeData : FVoxelSpawnerConfigSpawner
{
	float DistanceBetweenInstancesInVoxel = 0;
	FString DebugName;

	FVoxelSpawnerConfigSpawnerWithRuntimeData() = default;
	explicit FVoxelSpawnerConfigSpawnerWithRuntimeData(const FVoxelSpawnerConfigSpawner& Config)
		: FVoxelSpawnerConfigSpawner(Config)
	{
	}
};
struct FVoxelSpawnerConfigGroupBase
{
	int32 LOD = 0;
	int32 GenerationDistanceInChunks = 0;
	bool bInfiniteGenerationDistance = false;
	EVoxelSpawnerType SpawnerType = {};
};

struct FVoxelSpawnerConfigGroup : FVoxelSpawnerConfigGroupBase
{
	TArray<FVoxelSpawnerConfigSpawnerWithRuntimeData> Spawners;

	FVoxelSpawnerConfigGroup() = default;
	explicit FVoxelSpawnerConfigGroup(const FVoxelSpawnerConfigGroupBase& Base) : FVoxelSpawnerConfigGroupBase(Base) {}
};

struct FVoxelSpawnerThreadSafeConfig
{
	EVoxelSpawnerConfigRayWorldType WorldType = EVoxelSpawnerConfigRayWorldType::Flat;
	FVoxelSpawnerConfigFiveWayBlendSetup FiveWayBlendSetup;
	TArray<FVoxelSpawnerConfigGroup> Groups;
};

///////////////////////////////////////////////////////////////////////////////

class VOXEL_API FVoxelSpawnerManager : public IVoxelSpawnerManager, public FVoxelTickable, public TVoxelSharedFromThis<FVoxelSpawnerManager>
{
public:
	const FVoxelSpawnerSettings Settings;
	const FVoxelSpawnerThreadSafeConfig ThreadSafeConfig;

	static TVoxelSharedRef<FVoxelSpawnerManager> Create(const FVoxelSpawnerSettings& Settings);
	~FVoxelSpawnerManager();
	
	TVoxelSharedPtr<FVoxelSpawnerProxy> GetSpawner(UVoxelSpawner* Spawner) const;
	
	void Serialize(FArchive& Ar, FVoxelSpawnersSaveVersion::Type Version);

	//~ Begin IVoxelSpawnerManager Interface
	virtual void Destroy() override;

	virtual void Regenerate(const FVoxelIntBox& Bounds) override;
	virtual void MarkDirty(const FVoxelIntBox& Bounds) override;

	virtual void SaveTo(FVoxelSpawnersSaveImpl& Save) override;
	virtual void LoadFrom(const FVoxelSpawnersSaveImpl& Save) override;

	virtual bool GetMeshSpawnerTransforms(const FGuid& SpawnerGuid, TArray<FVoxelSpawnerTransforms>& OutTransforms) const override;
	
	virtual int32 GetTaskCount() const override;
	//~ End IVoxelSpawnerManager Interface

protected:
	//~ Begin FVoxelTickable Interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickableInEditor() const override { return true; }
	//~ End FVoxelTickable Interface

private:
	explicit FVoxelSpawnerManager(const FVoxelSpawnerSettings& Settings, const FVoxelSpawnerThreadSafeConfig& ThreadSafeConfig);

	void SpawnGroup_GameThread(FVoxelIntBox Bounds, int32 GroupIndex);
	void DestroyGroup_GameThread(FVoxelIntBox Bounds, int32 GroupIndex);

	void SpawnGroup_AnyThread(const FVoxelSpawnerTask& Task);

	void ProcessHits(
		const FVoxelSpawnerTask& Task,
		const FVoxelConstDataAccelerator& Accelerator,
		FVoxelReadScopeLock& Lock,
		const TMap<int32, TArray<FVoxelSpawnerHit>>& HitsMap);

	void UpdateTaskCount() const;

	void FlushCreateQueue_GameThread();

	template<typename T>
	void IterateResultsInBounds(const FVoxelIntBox& Bounds, T ApplyToResult_ReturnsShouldRebuild);

	bool FindSpawnerByGuid(const FGuid& Guid, int32& GroupIndex, int32& SpawnerIndex) const;

	FVoxelIntBoxWithValidity SpawnedSpawnersBounds;

	TMap<UVoxelSpawner*, TVoxelSharedPtr<FVoxelSpawnerProxy>> SpawnersMap;

	// Tricky thread safety case: Removing a chunk whose task has been started, but not finished yet
	// To handle that, we make queuing a result into the apply queues and storing a result into the chunk data atomic
	// by locking Group.Section. Using UpdateIndex we can make sure a task isn't storing old data.
	
	struct FSpawnerGroupChunkData
	{
		// Update index, used to cancel old tasks
		TVoxelSharedRef<FThreadSafeCounter64> UpdateIndex = MakeVoxelShared<FThreadSafeCounter64>();
		// The results. Same order as Group.Proxies.
		TArray<TVoxelSharedPtr<FVoxelSpawnerProxyResult>> SpawnerProxiesResults;

		void CancelTasks() const
		{
			UpdateIndex->Increment();
		}
	};
	struct FSpawnerGroupData
	{
		const uint32 ChunkSize = 0;
		// The group proxies, in the same order as the group spawners in ThreadSafeConfig
		const TArray<FVoxelSpawnerProxy*> SpawnerProxies;
		
		mutable FCriticalSection Section;
		TMap<FIntVector, FSpawnerGroupChunkData> ChunksData;
		mutable int64 AllocatedSize = 0;
		
		FSpawnerGroupData(uint32 ChunkSize, const TArray<FVoxelSpawnerProxy*>& SpawnerProxies)
			: ChunkSize(ChunkSize)
			, SpawnerProxies(SpawnerProxies)
		{
		}
		~FSpawnerGroupData()
		{
			DEC_VOXEL_MEMORY_STAT_BY(STAT_VoxelSpawnerManagerMemory, AllocatedSize);
		}
		
		FIntVector GetChunkKey(const FVoxelIntBox& Bounds) const
		{
			ensure(Bounds.Size() == FIntVector(ChunkSize));
			return Bounds.Min;
		}
		void UpdateStats() const
		{
			// Does not account for the Results array, but that should roughly be Number of Results * sizeof(ptr)
			// Would be too costly to compute exact size
			DEC_VOXEL_MEMORY_STAT_BY(STAT_VoxelSpawnerManagerMemory, AllocatedSize);
			AllocatedSize = sizeof(*this) + SpawnerProxies.GetAllocatedSize() + ChunksData.GetAllocatedSize();
			INC_VOXEL_MEMORY_STAT_BY(STAT_VoxelSpawnerManagerMemory, AllocatedSize);
		}
	};
	
	TArray<FSpawnerGroupData> GroupsData;

	// Can't use a TQueue as we need to remove elements
	FCriticalSection CreateQueueSection;
	TArray<TVoxelSharedPtr<FVoxelSpawnerProxyResult>> CreateQueue;

	// For debug
	mutable FThreadSafeCounter TaskCounter;
	
	friend FVoxelSpawnerTask;
};
