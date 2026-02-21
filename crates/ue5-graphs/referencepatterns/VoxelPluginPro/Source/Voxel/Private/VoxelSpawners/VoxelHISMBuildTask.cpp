// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelSpawners/VoxelHISMBuildTask.h"
#include "VoxelSpawners/VoxelHierarchicalInstancedStaticMeshComponent.h"
#include "VoxelSpawners/VoxelInstancedMeshManager.h"
#include "VoxelUtilities/VoxelThreadingUtilities.h"

#include "Engine/StaticMesh.h"

static TAutoConsoleVariable<int32> CVarLogHISMBuildTimes(
	TEXT("voxel.spawners.LogCullingTreeBuildTimes"),
	0,
	TEXT("If true, will log all the HISM build times"),
	ECVF_Default);

TAutoConsoleVariable<int32> CVarRandomColorPerHISM(
	TEXT("voxel.spawners.RandomColorPerHISM"),
	0,
	TEXT("If true, will set the instance random to be a hash of the HISM pointer"),
	ECVF_Default);

// NOTE: start of HISM build tree refactor:
// https://github.com/Phyronnaz/VoxelPrivate/blob/fb7cd3edc0ca01dd6b7e7ab9b6908c7c31c1f290/Source/Voxel/Private/VoxelSpawners/VoxelHierarchicalInstancedStaticMeshComponent.cpp#L41


FVoxelHISMBuildTask::FVoxelHISMBuildTask(
	UVoxelHierarchicalInstancedStaticMeshComponent* Component,
	const TArray<FVoxelSpawnerMatrix>& Matrices)
	: FVoxelAsyncWork(STATIC_FNAME("HISM Build Task"), 1e9, true)
	, UniqueId(GetUniqueId())
	, CancelCounter(MakeVoxelShared<FThreadSafeCounter>())
	, MeshBox(Component->GetStaticMesh()->GetBounds().GetBox())
	, DesiredInstancesPerLeaf(Component->DesiredInstancesPerLeaf())
	, InstancedMeshManager(Component->Voxel_InstancedMeshManager)
	, Component(Component)
	, BuiltData(MakeVoxelShared<FVoxelHISMBuiltData>())
{
	check(Matrices.Num() > 0 && Matrices.Num() < 1e8); // If you have 100M instances something went terribly wrong
	BuiltData->UniqueId = UniqueId;
	{
		VOXEL_SCOPE_COUNTER("Copy Matrices");
		BuiltData->BuiltInstancesMatrices = Matrices;
	}
}

void FVoxelHISMBuildTask::DoWork()
{
	const double StartTime = FPlatformTime::Seconds();
	const int32 NumInstances = BuiltData->BuiltInstancesMatrices.Num();
	check(NumInstances > 0);
	
	BuiltData->InstanceBuffer = MakeUnique<FStaticMeshInstanceData>(false);
	{
		VOXEL_ASYNC_SCOPE_COUNTER("AllocateInstances");
		BuiltData->InstanceBuffer->AllocateInstances(
			NumInstances,
			// TODO custom floats?
			0,
			EResizeBufferFlags::AllowSlackOnGrow | EResizeBufferFlags::AllowSlackOnReduce,
			true);
	}

	TArray<FMatrix> CleanTransforms;
	CleanTransforms.SetNumUninitialized(NumInstances);

	{
		const bool bRandomColorPerHISM = CVarRandomColorPerHISM.GetValueOnAnyThread() != 0;
		float RandomColor = 0.f;
		if (bRandomColorPerHISM)
		{
			const uint32 RandomInt = FVoxelUtilities::MurmurHash64(uint64(Component.Get()));
			RandomColor = *reinterpret_cast<const float*>(&RandomInt);
		}
		
		VOXEL_ASYNC_SCOPE_COUNTER("SetInstances");
		for (int32 InstanceIndex = 0; InstanceIndex < NumInstances; InstanceIndex++)
		{
			const FVoxelSpawnerMatrix& Matrix = BuiltData->BuiltInstancesMatrices[InstanceIndex];
			const FMatrix CleanMatrix = Matrix.GetCleanMatrix();

			const float RandomId = bRandomColorPerHISM ? RandomColor : Matrix.GetRandomInstanceId();
			
			CleanTransforms[InstanceIndex] = CleanMatrix;
			BuiltData->InstanceBuffer->SetInstance(InstanceIndex, FMatrix44f(CleanMatrix), RandomId);
		}
	}

	// Only check if we're canceled before the BuildTree, as it's the true expensive operation here
	// and if we've finished it, we might as well use the result
	if (CancelCounter->GetValue() > 0)
	{
		return;
	}

	TArray<int32> SortedInstances;
	TArray<int32> InstanceReorderTable;
	{
		VOXEL_ASYNC_SCOPE_COUNTER("BuildTreeAnyThread");
		static_assert(sizeof(FVoxelSpawnerMatrix) == sizeof(FMatrix), "");
		TArray<float> CustomDataFloats;
#if VOXEL_ENGINE_VERSION >= 504

		// do grass need this?
		float DensityScaling = 1.0f;
		int32 InstancingRandomSeed = 1;

		UHierarchicalInstancedStaticMeshComponent::FClusterBuilder Builder(CleanTransforms, CustomDataFloats, 0, MeshBox, FMath::Max(DesiredInstancesPerLeaf, 1), DensityScaling, InstancingRandomSeed, false);
		Builder.BuildTree();
		BuiltData->OcclusionLayerNum = Builder.Result->OutOcclusionLayerNum;

		BuiltData->ClusterTree = MoveTemp(Builder.Result->Nodes);
		InstanceReorderTable = MoveTemp(Builder.Result->InstanceReorderTable);
		SortedInstances = MoveTemp(Builder.Result->SortedInstances);
#else
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		UHierarchicalInstancedStaticMeshComponent::BuildTreeAnyThread(
			CleanTransforms,
			// TODO investigate what this fancy stuff does
			CustomDataFloats,
			0,
			MeshBox,
			BuiltData->ClusterTree,
			SortedInstances,
			InstanceReorderTable,
			BuiltData->OcclusionLayerNum,
			DesiredInstancesPerLeaf,
			false
		);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
	}

	{
		VOXEL_ASYNC_SCOPE_COUNTER("Build Reorder Table");
		BuiltData->InstancesToBuiltInstances = InstanceReorderTable;
		BuiltData->BuiltInstancesToInstances.Init(-1, InstanceReorderTable.Num());
		for (int32 Index = 0; Index < InstanceReorderTable.Num(); Index++)
		{
			BuiltData->BuiltInstancesToInstances[InstanceReorderTable[Index]] = Index;
		}
		for (int32 Index : BuiltData->BuiltInstancesToInstances) check(Index != -1);
	}
	
	// in-place sort the instances
	{
		VOXEL_ASYNC_SCOPE_COUNTER("Sort Instances");
		for (int32 FirstUnfixedIndex = 0; FirstUnfixedIndex < NumInstances; FirstUnfixedIndex++)
		{
			const int32 LoadFrom = SortedInstances[FirstUnfixedIndex];
			if (LoadFrom != FirstUnfixedIndex)
			{
				check(LoadFrom > FirstUnfixedIndex);
				BuiltData->InstanceBuffer->SwapInstance(FirstUnfixedIndex, LoadFrom);
				// Also keep up to date the transforms array
				BuiltData->BuiltInstancesMatrices.Swap(FirstUnfixedIndex, LoadFrom);
				const int32 SwapGoesTo = InstanceReorderTable[FirstUnfixedIndex];
				check(SwapGoesTo > FirstUnfixedIndex);
				check(SortedInstances[SwapGoesTo] == FirstUnfixedIndex);
				SortedInstances[SwapGoesTo] = LoadFrom;
				InstanceReorderTable[LoadFrom] = SwapGoesTo;
				InstanceReorderTable[FirstUnfixedIndex] = FirstUnfixedIndex;
				SortedInstances[FirstUnfixedIndex] = FirstUnfixedIndex;
			}
		}
	}

	const double EndTime = FPlatformTime::Seconds();
	if (CVarLogHISMBuildTimes.GetValueOnAnyThread() != 0)
	{
		LOG_VOXEL(
			Log,
			TEXT("Building the HISM culling tree took %2.2fms; Instances: %d"),
			(EndTime - StartTime) * 1000,
			NumInstances);
	}

	auto PinnedInstancedMeshManager = InstancedMeshManager.Pin();
	if (PinnedInstancedMeshManager.IsValid())
	{
		PinnedInstancedMeshManager->HISMBuildTaskCallback(Component, BuiltData);
		FVoxelUtilities::DeleteOnGameThread_AnyThread(PinnedInstancedMeshManager);
	}

#undef CHECK_CANCEL
}

uint32 FVoxelHISMBuildTask::GetPriority() const
{
	return 0;
}

uint64 FVoxelHISMBuildTask::GetUniqueId()
{
	return UNIQUE_ID();
}
