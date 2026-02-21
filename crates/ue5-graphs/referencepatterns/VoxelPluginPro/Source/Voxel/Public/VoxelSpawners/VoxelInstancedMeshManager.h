// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelMinimal.h"
#include "VoxelEnums.h"
#include "VoxelIntBox.h"
#include "Templates/SubclassOf.h"
#include "VoxelSpawners/VoxelSpawnerMatrix.h"
#include "VoxelInstancedMeshSettings.h"
#include "VoxelTickable.h"

struct FVoxelInstancesSection;
struct FVoxelHISMBuiltData;
class AActor;
class AVoxelWorld;
class AVoxelSpawnerActor;
class UStaticMesh;
class UVoxelHierarchicalInstancedStaticMeshComponent;
class IVoxelPool;
class FVoxelData;
class FVoxelEventManager;
struct FVoxelIntBox;

struct VOXEL_API FVoxelInstancedMeshManagerSettings
{
	const TWeakObjectPtr<AActor> ComponentsOwner;
	const TVoxelSharedRef<FIntVector> WorldOffset;
	const TVoxelSharedRef<IVoxelPool> Pool;
	const TVoxelSharedRef<FVoxelEventManager> EventManager;
	const uint32 HISMChunkSize;
	const uint32 CollisionChunkSize = 32; // Also change in AVoxelWorld::PostEditChangeProperty
	const uint32 CollisionDistanceInChunks;
	const float VoxelSize;
	const int64 MaxNumberOfInstances;
	const FSimpleDelegate OnMaxInstanceReached;

	FVoxelInstancedMeshManagerSettings(
		const AVoxelWorld* World,
		const TVoxelSharedRef<IVoxelPool>& Pool,
		const TVoxelSharedRef<FVoxelEventManager>& EventManager);
};

struct FVoxelInstancedMeshInstancesRef
{
public:
	FVoxelInstancedMeshInstancesRef() = default;

	bool IsValid() const
	{
		return Section.IsValid();
	}
	
private:
	TWeakObjectPtr<UVoxelHierarchicalInstancedStaticMeshComponent> HISM;
	TVoxelWeakPtr<FVoxelInstancesSection> Section;
	int32 NumInstances = 0;

	friend class FVoxelInstancedMeshManager;
};

class VOXEL_API FVoxelInstancedMeshManager : public FVoxelTickable, public TVoxelSharedFromThis<FVoxelInstancedMeshManager>
{
public:
	const FVoxelInstancedMeshManagerSettings Settings;

	static TVoxelSharedRef<FVoxelInstancedMeshManager> Create(const FVoxelInstancedMeshManagerSettings& Settings);
	void Destroy();
	~FVoxelInstancedMeshManager();

public:
	// TransformsOffset is used to reduce precision errors
	FIntVector ComputeTransformsOffset(const FVoxelIntBox& Bounds) const
	{
		return FVoxelUtilities::DivideFloor(Bounds.Min, Settings.HISMChunkSize) * Settings.HISMChunkSize;
	}
	
	FVoxelInstancedMeshInstancesRef AddInstances(
		const FVoxelInstancedMeshAndActorSettings& MeshAndActorSettings, 
		const FVoxelSpawnerTransforms& Transforms, 
		const FVoxelIntBox& Bounds);
	void RemoveInstances(FVoxelInstancedMeshInstancesRef Ref);

	static const TSet<int32>& GetRemovedIndices(FVoxelInstancedMeshInstancesRef Ref);

	AVoxelSpawnerActor* SpawnActor(
		const FVoxelInstancedMeshAndActorSettings& MeshAndActorSettings,
		const FVoxelSpawnerTransform& Transform) const;
	void SpawnActors(
		const FVoxelInstancedMeshAndActorSettings& MeshAndActorSettings,
		const FVoxelSpawnerTransforms& Transforms,
		TArray<AVoxelSpawnerActor*>& OutActors) const;

	void SpawnActorsInArea(
		const FVoxelIntBox& Bounds,
		const FVoxelData& Data,
		EVoxelSpawnerActorSpawnType SpawnType,
		TArray<AVoxelSpawnerActor*>& OutActors) const;
	
	TMap<FVoxelInstancedMeshAndActorSettings, TArray<FVoxelSpawnerTransforms>> RemoveInstancesInArea(
		const FVoxelIntBox& Bounds,
		const FVoxelData& Data,
		EVoxelSpawnerActorSpawnType SpawnType) const;

	AVoxelSpawnerActor* SpawnActorByIndex(UVoxelHierarchicalInstancedStaticMeshComponent* Component, int32 InstanceIndex);
	
	void RecomputeMeshPositions();

protected:
	//~ Begin FVoxelTickable Interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickableInEditor() const override { return true; }
	//~ End FVoxelTickable Interface

public:
	void HISMBuildTaskCallback(TWeakObjectPtr<UVoxelHierarchicalInstancedStaticMeshComponent> Component, const TVoxelSharedRef<FVoxelHISMBuiltData>& BuiltData);

private:
	struct FQueuedBuildCallback
	{
		TWeakObjectPtr<UVoxelHierarchicalInstancedStaticMeshComponent> Component;
		TVoxelSharedPtr<FVoxelHISMBuiltData> Data;
	};
	TQueue<FQueuedBuildCallback, EQueueMode::Mpsc> HISMBuiltDataQueue;

private:
	explicit FVoxelInstancedMeshManager(const FVoxelInstancedMeshManagerSettings& Settings);

	UVoxelHierarchicalInstancedStaticMeshComponent* CreateHISM(const FVoxelInstancedMeshAndActorSettings& MeshSettings, const FIntVector& Position) const;

private:
	struct FHISMChunk
	{
		FVoxelIntBoxWithValidity Bounds;
		TWeakObjectPtr<UVoxelHierarchicalInstancedStaticMeshComponent> HISM;
	};
	struct FHISMChunks
	{
		TMap<FIntVector, FHISMChunk> Chunks;
	};
	TMap<FVoxelInstancedMeshAndActorSettings, FHISMChunks> MeshSettingsToChunks;

	TSet<TWeakObjectPtr<UVoxelHierarchicalInstancedStaticMeshComponent>> HISMs;

	int64 NumInstances = 0;
	bool bMaxNumInstancesReachedFired = false;
};
