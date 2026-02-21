// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelMinimal.h"
#include "VoxelAsyncWork.h"
#include "VoxelSpawners/VoxelSpawnerMatrix.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"

class FVoxelInstancedMeshManager;
class UVoxelHierarchicalInstancedStaticMeshComponent;

extern TAutoConsoleVariable<int32> CVarRandomColorPerHISM;

struct FVoxelHISMBuiltData
{
	uint64 UniqueId;
	
	TArray<FVoxelSpawnerMatrix> BuiltInstancesMatrices;
	
	TArray<int32> InstancesToBuiltInstances;
	TArray<int32> BuiltInstancesToInstances;
	
	TUniquePtr<FStaticMeshInstanceData> InstanceBuffer;
	TArray<FClusterNode> ClusterTree;
	int32 OcclusionLayerNum = 0;
};

// Will auto delete
class FVoxelHISMBuildTask : public FVoxelAsyncWork
{
public:
	const uint64 UniqueId;
	const TVoxelSharedRef<FThreadSafeCounter> CancelCounter;

	const FBox MeshBox;
	const int32 DesiredInstancesPerLeaf;
	const TVoxelWeakPtr<FVoxelInstancedMeshManager> InstancedMeshManager;
	const TWeakObjectPtr<UVoxelHierarchicalInstancedStaticMeshComponent> Component;
	
	// Output
	const TVoxelSharedRef<FVoxelHISMBuiltData> BuiltData;

	FVoxelHISMBuildTask(
		UVoxelHierarchicalInstancedStaticMeshComponent* Component,
		const TArray<FVoxelSpawnerMatrix>& Matrices);

	//~ Begin FVoxelAsyncWork Interface
	virtual void DoWork() override;
	virtual uint32 GetPriority() const override;
	//~ End FVoxelAsyncWork Interface

	static uint64 GetUniqueId();

private:
	~FVoxelHISMBuildTask() = default;

	template<typename T>
	friend struct TVoxelAsyncWorkDelete;
};
